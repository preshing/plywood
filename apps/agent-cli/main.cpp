/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-base.h>
#include <ply-json.h>
#include <ply-agent.h>
#include <ply-http.h>
#include <curl/curl.h>

using namespace ply;

//---------------------------------------------------
// Global variables and options.
//---------------------------------------------------
struct CommandLineOptions {
    String settingsPath;
    bool enableHttpLog = false;
    bool runWebServer = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

CommandLineOptions options;
EndPoint endPoint;
ToolSet toolSet;
Reference<Transcript> transcript;

//---------------------------------------------------
// Helpers for formatting the transcript output.
//---------------------------------------------------
static const StringView Separator = "-------------------------------";

static String formatTimeStamp(s64 micros) {
    DateTime dt = convertToDateTime(micros);
    u32 h12 = dt.hour % 12;
    if (h12 == 0)
        h12 = 12;
    const char* ap = dt.hour < 12 ? "am" : "pm";
    return String::format("{}:{:02}{}", h12, dt.minute, ap);
}

static String formatSize(uptr bytes) {
    if (bytes < 1024)
        return String::format("{} bytes", bytes);
    double kb = double(bytes) / 1024.0;
    if (kb < 1024.0)
        return String::format("{:.1}KB", kb);
    return String::format("{:.1}MB", kb / 1024.0);
}

static String formatRate(double bytesPerSec) {
    if (bytesPerSec < 1024.0)
        return String::format("{:.1} bytes/s", bytesPerSec);
    double kb = bytesPerSec / 1024.0;
    if (kb < 1024.0)
        return String::format("{:.1}KB/s", kb);
    return String::format("{:.1}MB/s", kb / 1024.0);
}

static String formatJsonValue(const json::Node& node) {
    if (node.isText())
        return node.text();
    if (node.isNumber()) {
        double v = node.getNumber();
        if (v == (s64) v)
            return String::format("{:d}", (s64) v);
        return String::format("{}", v);
    }
    if (node.isBool())
        return String{node.getBool() ? "true" : "false"};
    return json::toString(node);
}

// Formats a raw tool call of the form `name{"key":"value",...}` as the more
// readable `name key=value key=value`.
static String formatToolCall(StringView raw) {
    s32 brace = raw.find('{');
    if (brace < 0)
        return raw;
    StringView name = raw.left(brace);
    json::Parser parser;
    parser.setErrorCallback([](const json::ParseError&) {});
    parser.setGreedy(false);
    json::Parser::Result result = parser.parse({}, raw.substr(brace));
    MemStream out;
    out.format("{}", name);
    if (result.root.isObject()) {
        for (const auto& item : result.root.object().items)
            out.format(" {}={}", item.key, formatJsonValue(item.value));
    }
    return out.moveToString();
}

//---------------------------------------------------
// TranscriptPrinter writes the agent's transcript to stdout as a sequence of
// timestamped message blocks. Tool responses are received asynchronously
// from the rest of the transcript, so they are buffered here and only flushed
// to stdout once an EndTurn event arrives (or the agent stops). This keeps
// them from appearing interleaved with the other messages of the turn.
//---------------------------------------------------
struct TranscriptPrinter {
    // The currently open section: its header (and, for streamed text messages,
    // its content) has already been written to stdout, but the closing timing
    // line and separator are deferred until the next section opens or the turn
    // ends. This is what lets us compute the message's elapsed time and output
    // size.
    bool hasOpen = false;
    bool openIsTextMsg = false; // true for User/AgentThinking/Agent/Error
    Transcript::Role openRole = Transcript::Role::None;
    s64 sectionStartTime = 0;
    uptr openOutputBytes = 0;
    bool openLastWasNewline = true; // ensures the timing line starts on its own line
    String openToolCallText; // accumulated raw text for a ToolCall section

    // Buffered tool responses, keyed by toolCallID, awaiting flush at EndTurn.
    Map<u32, String> pendingResponses;
    Array<u32> responseOrder; // toolCallIDs in first-arrival order

    void printStartup(StringView userPrompt);
    void handleEvent(const TranscriptEvent& event);
    void finish(s64 endMicros);

    void openSection(Transcript::Role role, u32 toolCallID, s64 timeStamp);
    void closeOpen(s64 endMicros);
    void flushToolResponses(s64 timeStamp);
};

void TranscriptPrinter::printStartup(StringView userPrompt) {
    s64 now = getUnixTimestamp();

    // System prompt block.
    {
        Stream out = getStdOut();
        out.format("{} [System Prompt]\n", formatTimeStamp(now));
        out.format("{}\n", toolSet.systemMessage);
        out.format("{}\n", Separator);
    }

    // Tool definition blocks.
    {
        Stream out = getStdOut();
        for (const Owned<ToolSet::Handler>& tool : toolSet.handlers) {
            out.format("{} [Tool Definition: {}]\n", formatTimeStamp(now), tool->name);
            for (const ToolSet::Parameter& param : tool->parameters)
                out.format("`{}`: {}\n", param.name, param.description);
            out.format("{}\n", tool->description);
            out.format("{}\n", Separator);
        }
    }

    // User block. Its header and content are emitted now, but it is left open so
    // that its elapsed time reflects the wait for the agent's first response.
    {
        Stream out = getStdOut();
        out.format("{} [User]\n", formatTimeStamp(now));
        out.format("{}\n", userPrompt);
    }
    this->hasOpen = true;
    this->openIsTextMsg = true;
    this->openRole = Transcript::Role::User;
    this->sectionStartTime = now;
    this->openOutputBytes = userPrompt.numBytes();
}

void TranscriptPrinter::openSection(Transcript::Role role, u32 toolCallID, s64 timeStamp) {
    // Close any previously open section before starting a new one.
    if (this->hasOpen)
        this->closeOpen(timeStamp);

    this->hasOpen = true;
    this->openRole = role;
    this->sectionStartTime = timeStamp;
    this->openOutputBytes = 0;
    this->openLastWasNewline = true;
    this->openToolCallText = {};

    Stream out = getStdOut();
    switch (role) {
        case Transcript::Role::AgentThinking:
            out.format("{} [Agent Thinking]\n", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::Agent:
            out.format("{} [Agent]\n", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::Error:
            out.format("{} [Error]\n", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::ToolCall:
            out.format("{} [Tool Call #{}]\n", formatTimeStamp(timeStamp), toolCallID);
            this->openIsTextMsg = false;
            break;
        default:
            this->openIsTextMsg = false;
            break;
    }
}

void TranscriptPrinter::closeOpen(s64 endMicros) {
    Stream out = getStdOut();
    if (this->openIsTextMsg) {
        double elapsedSec = double(endMicros - this->sectionStartTime) / 1000000.0;
        if (elapsedSec < 0)
            elapsedSec = 0;
        double rate = (elapsedSec > 1e-9) ? double(this->openOutputBytes) / elapsedSec : 0;
        if (!this->openLastWasNewline)
            out.format("\n");
        out.format("({:.1}s elapsed, {} output, {})\n", elapsedSec, formatSize(this->openOutputBytes),
                   formatRate(rate));
    } else if (this->openRole == Transcript::Role::ToolCall) {
        out.format("{}\n", formatToolCall(this->openToolCallText));
    }
    out.format("{}\n", Separator);
    this->hasOpen = false;
}

void TranscriptPrinter::flushToolResponses(s64 timeStamp) {
    // Print responses in toolCallID order (responses can arrive out of order since
    // they are produced asynchronously by the tool thread).
    Array<u32> ids = this->responseOrder;
    sort(ids);
    Stream out = getStdOut();
    for (u32 id : ids) {
        String* text = this->pendingResponses.find(id);
        if (!text)
            continue;
        out.format("{} [Tool Response #{}]\n", formatTimeStamp(timeStamp), id);
        out.format("{}\n", *text);
        out.format("{}\n", Separator);
    }
    this->pendingResponses.clear();
    this->responseOrder.clear();
}

void TranscriptPrinter::handleEvent(const TranscriptEvent& event) {
    switch (event.operation) {
        case TranscriptEvent::BeginMessage:
            this->openSection(event.role, event.toolCallID, event.timeStamp);
            break;
        case TranscriptEvent::AppendText:
            if (this->hasOpen) {
                if (this->openIsTextMsg) {
                    // Stream text messages to stdout as they arrive.
                    getStdOut().format("{}", event.text);
                    this->openOutputBytes += event.text.numBytes();
                    if (event.text)
                        this->openLastWasNewline = event.text.endsWith('\n');
                } else if (this->openRole == Transcript::Role::ToolCall) {
                    // Accumulate the tool call's raw text; it is formatted on close.
                    this->openToolCallText += event.text;
                }
            }
            break;
        case TranscriptEvent::AppendToolResponse: {
            // Buffer the response; it is flushed at EndTurn.
            auto ins = this->pendingResponses.insert(event.toolCallID);
            if (!ins.wasFound)
                this->responseOrder.append(event.toolCallID);
            *ins.value += event.text;
            break;
        }
        case TranscriptEvent::EndToolResponse:
            // Nothing to do here; responses are flushed at EndTurn.
            break;
        case TranscriptEvent::EndTurn:
            if (this->hasOpen)
                this->closeOpen(event.timeStamp);
            this->flushToolResponses(event.timeStamp);
            break;
        default:
            break;
    }
}

void TranscriptPrinter::finish(s64 endMicros) {
    if (this->hasOpen)
        this->closeOpen(endMicros);
    // Flush any tool responses that never got an EndTurn (e.g. the agent was
    // canceled mid-turn).
    this->flushToolResponses(endMicros);
}

//---------------------------------------------------
// Load the agent configuration settings from agent.json.
//---------------------------------------------------
static bool loadSettings() {
    // Make full settings path.
    String configPath;
    if (options.settingsPath) {
        configPath = joinPath(options.settingsPath, "agent.json");
    } else {
        // First try the current working directory.
        String cwdPath = joinPath(Filesystem::getWorkingDirectory(), "agent.json");
        if (Filesystem::exists(cwdPath) == ER_FILE) {
            configPath = std::move(cwdPath);
        } else {
            // Fall back to the executable's directory.
            String exePath = getCurrentExecutablePath();
            configPath = joinPath(splitPath(exePath).directory, "agent.json");
        }
    }

    // Load JSON settings file.
    String jsonText = Filesystem::loadText(configPath);
    if (!jsonText) {
        getStdErr().format("Could not load configuration file: {}\n", configPath);
        return false;
    }

    // Parse the JSON structure.
    json::Parser parser;
    json::Parser::Result result = parser.parse(configPath, jsonText);
    if (parser.anyError()) {
        getStdErr().format("Failed to parse configuration file: {}\n", configPath);
        return false;
    }
    const json::Node& root = result.root;

    // Import endPoint settings.
    const json::Node& ep = root.get("endPoint");
    endPoint.url = ep.get("url").text();
    StringView protocol = ep.get("protocol").text();
    if (protocol.endsWith("-responses")) {
        endPoint.protocol = Protocol::Responses;
    } else {
        endPoint.protocol = Protocol::Completions;
    }
    StringView modelView = ep.get("model").text();
    if (modelView)
        endPoint.model = modelView;

    // The API key is read from the named environment variable on demand. Plywood
    // strings aren't null-terminated, so append an explicit NUL byte for getenv.
    String apiKeyEnvZ = String{ep.get("apiKeyEnv").text()} + '\0';
    endPoint.getAPIKey = [apiKeyEnvZ]() -> String {
        const char* envVar = getenv(apiKeyEnvZ.bytes());
        PLY_ASSERT(envVar);
        return envVar;
    };

    // Import system prompt.
    toolSet.systemMessage = root.get("systemPrompt").text();

    // Import current directory.
    StringView rawCwd = root.get("currentDirectory").text();
    StringView configDir = splitPath(configPath).directory;
    toolSet.currentDirectory = rawCwd ? makeAbsolutePath(joinPath(configDir, rawCwd)) : makeAbsolutePath(configDir);

    // Import directory permissions.
    Set<String> registeredTools; // union of all tools across trees (registered once each)
    const json::Node& perms = root.get("permissions");
    for (const json::Node& perm : perms.arrayView()) {
        StringView rawRoot = perm.get("path").text();
        if (!rawRoot) {
            getStdErr().format("Each permission entry must have a 'path'.\n");
            return false;
        }

        String absPath = makeAbsolutePath(joinPath(toolSet.currentDirectory, rawRoot));

        const json::Node& toolsNode = perm.get("tools");
        for (const json::Node& t : toolsNode.arrayView()) {
            StringView toolName = t.text();

            // Register the tool on first encounter.
            Owned<ToolSet::Handler>* found = toolSet.handlers.find(toolName);
            if (!found) {
                if (toolName == "read") {
                    addReadTool(&toolSet);
                } else if (toolName == "write") {
                    addWriteTool(&toolSet);
                } else if (toolName == "list_dir") {
                    addListDirTool(&toolSet);
                } else if (toolName == "find_in_files") {
                    addFindInFilesTool(&toolSet);
                } else if (toolName == "edit") {
                    addEditTool(&toolSet);
                } else {
                    getStdErr().format("Unknown tool in configuration: {}\n", toolName);
                    return false;
                }
                found = toolSet.handlers.find(toolName);
                PLY_ASSERT(found);
            }

            // Grant this directory to the tool handler.
            ToolSet::Permission& granted = found->get()->permissions.append();
            granted.absPath = absPath;
        }
    }

    // Augment the system prompt.
    toolSet.systemMessage += String::format("\nThe current working directory is: {}\n", toolSet.currentDirectory);

    return true;
}

//---------------------------------------------------
// Main entry point.
//---------------------------------------------------
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    // Configure terminal window for UTF8 output.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Parse command line options.
    CommandLineParser parser({
        {"-s", PLY_LOOKUP_MEMBER(CommandLineOptions, settingsPath), "Directory containing agent.json"},
        {"-h", PLY_LOOKUP_MEMBER(CommandLineOptions, enableHttpLog), "Enable HTTP logging"},
        {"-w", PLY_LOOKUP_MEMBER(CommandLineOptions, runWebServer), "Run echo webserver on port 8081"},
    });
    if (!parser.apply(argc, argv, &options))
        return 1; // parsing error

    // Initialize curl.
    CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    PLY_ASSERT(rc == CURLE_OK);
    PLY_UNUSED(rc);

    // Load agent settings.
    if (!loadSettings())
        return 1;

    // Start the echo webserver in parallel with the agent. Joining this thread below keeps the app running after
    // the agent finishes, until the process is interrupted with Ctrl+C.
    Thread webServerThread;
    if (options.runWebServer) {
        Network::initialize(IPV4);
        webServerThread.run([] {
            runHTTPServer(8081, serveEchoPage);
        });
    }

    // Create a transcript with the user's prompt as the first turn.
    String userPrompt = "Please summarize the contents of the file sample.txt";
    transcript = new Transcript;
    Transcript::Turn& turn = transcript->turns.append();
    {
        Owned<Transcript::Message> userMsg = Heap::create<Transcript::Message>();
        userMsg->role = Transcript::Role::User;
        userMsg->text = userPrompt;
        turn.messages.append(std::move(userMsg));
    }

    // Initialize Agent::Properties.
    Agent::Properties props;
    props.endPoint = &endPoint;
    props.toolSet = &toolSet;
    props.enableHttpLog = options.enableHttpLog;

    // Start the agent.
    props.originalTranscript = transcript;
    Owned<Agent> agent = Heap::create<Agent>(std::move(props));

    // Print the static portion of the transcript (system prompt, tool definitions
    // and the user prompt), then stream the agent's events as they arrive.
    TranscriptPrinter printer;
    printer.printStartup(userPrompt);

    // Process streamed events until the agent stops working.
    Owned<TranscriptUpdater> updater = createTranscriptUpdater(transcript);
    while (agent->isWorking()) {
        for (const TranscriptEvent& event : agent->waitForEvents()) {
            applyTranscriptEvent(updater, &event);
            printer.handleEvent(event);
        }
    }

    // Close any section that was left open (e.g. the agent's final response) and
    // flush any tool responses that never received an EndTurn.
    printer.finish(getUnixTimestamp());

    // Keep serving HTTP requests after the agent finishes.
    if (webServerThread.isValid()) {
        webServerThread.join();
        Network::shutdown();
    }

    // Cleanup.
    curl_global_cleanup();

    return 0;
}

PLY_STRUCT_BEGIN(CommandLineOptions)
PLY_STRUCT_MEMBER(settingsPath)
PLY_STRUCT_MEMBER(enableHttpLog)
PLY_STRUCT_MEMBER(runWebServer)
PLY_STRUCT_END()
