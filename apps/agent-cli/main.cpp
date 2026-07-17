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
    return String::fromDateTime("%l:%M%P", convertToDateTime(micros));
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

//  ▄▄    ▄▄        ▄▄
//  ██ ▄▄ ██  ▄▄▄▄  ██▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ▀█▄██▄█▀ ██▄▄██ ██  ██ ▀█▄▄▄  ██▄▄██ ██  ▀▀ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀
//   ██▀▀██  ▀█▄▄▄  ██▄▄█▀  ▄▄▄█▀ ▀█▄▄▄  ██       ▀█▀   ▀█▄▄▄  ██
//

//---------------------------------------------------
// WebTranscript stores JSONL events as an append-only stream. Each browser
// replays the stream from the beginning, then waits for new events.
//---------------------------------------------------
struct WebTranscript {
    Mutex mutex;
    ConditionVariable changed;
    Array<String> chunks;
    bool isComplete = false;

    // Publishes another JSON event to all connected browsers.
    void append(json::Node&& event) {
        String line = json::toString(event, {false});
        line += '\n';
        LockGuard<Mutex> lock{this->mutex};
        this->chunks.append(std::move(line));
        this->changed.wakeAll();
    }

    // Signals that no more JSON events will be published.
    void complete() {
        LockGuard<Mutex> lock{this->mutex};
        this->isComplete = true;
        this->changed.wakeAll();
    }
};

WebTranscript webTranscript;

// Sends the complete page that reconstructs the transcript from JSONL events.
static void serveWebPage(HTTPServerRequest& request) {
    HTTPServerResponse response{HTTPServerResponse::OK};
    *response.headers.insert("content-type").value = "text/html; charset=utf-8";
    *response.headers.insert("cache-control").value = "no-store";
    request.sendFullResponse(std::move(response), R"(<!doctype html>
<html><head><meta charset="utf-8"><title>Agent Transcript</title>
<style>
body { margin: 0; background: #101010; color: #e8e6e3; font: 14px/1.5 monospace; }
#transcript { max-width: 960px; margin: auto; padding: 16px; }
.section { margin: 0 0 12px; background: #282828; border-radius: 6px; }
.section-header { padding: 2px 12px; background: #303438; border-radius: 6px 6px 0 0;
    font-family: system-ui, sans-serif; color: #d0d0d0; font-weight: 600; }
.tool-call > .section-header, .tool-response > .section-header { color: #202426; }
.collapse-toggle { width: 17px; height: 17px; margin: 0 8px 0 0; padding: 0; border: 0; background: transparent;
    color: inherit; vertical-align: -3px; cursor: pointer; background-color: currentColor;
    -webkit-mask: var(--collapse-icon) center / contain no-repeat; mask: var(--collapse-icon) center / contain no-repeat; }
.section.collapsed .collapse-toggle { --collapse-icon: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Ccircle cx='8' cy='8' r='6' fill='none' stroke='black' stroke-width='1'/%3E%3Cpath d='M5 8h6M8 5v6' fill='none' stroke='black' stroke-linecap='round' stroke-width='1'/%3E%3C/svg%3E"); }
.section:not(.collapsed) .collapse-toggle { --collapse-icon: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Ccircle cx='8' cy='8' r='6' fill='none' stroke='black' stroke-width='1'/%3E%3Cpath d='M5 8h6' fill='none' stroke='black' stroke-linecap='round' stroke-width='1'/%3E%3C/svg%3E"); }
.collapse-toggle:focus-visible { outline: 1px solid currentColor; outline-offset: 2px; }
.section.collapsed > .message-content { display: none; }
.section.collapsed > .section-header { border-radius: 6px; }
.message-content { padding: 4px 12px; white-space: pre-wrap; }
.system > .section-header, .tool-definition > .section-header { background: #393c3f; }
.user > .section-header { background: #3b5d4b; }
.thinking > .section-header { background: #4b7380; }
.agent > .section-header { background: #244a7c; }
.error > .section-header { background: #813b3b; }
.tool-call > .section-header, .tool-response > .section-header { background: #8d9398; }
.tool-call > .section-header .timestamp, .tool-response > .section-header .timestamp { color: #4d555a; }
.timestamp { margin-right: 10px; color: #c0c3c5; font-size: 0.85em; font-weight: normal; }
.timing { font-family: system-ui, sans-serif; color: #9aa0a6; }
</style></head><body><main id="transcript"></main>
<script>
const transcript = document.getElementById('transcript');
let openMessage = null;
const toolResponses = new Map();
let pinned = true;
function atBottom() { return innerHeight + scrollY >= document.documentElement.scrollHeight - 24; }
addEventListener('scroll', () => pinned = atBottom(), {passive: true});
new MutationObserver(() => { if (pinned) requestAnimationFrame(() => scrollTo(0, document.body.scrollHeight)); })
    .observe(transcript, {childList: true, subtree: true, characterData: true});
transcript.addEventListener('click', event => {
    const button = event.target.closest('.collapse-toggle');
    if (!button)
        return;
    const section = button.closest('.section');
    section.classList.toggle('collapsed');
});

function createSection(className, caption, timeStamp, collapsed) {
    const section = document.createElement('div');
    section.className = 'section ' + className + (collapsed ? ' collapsed' : '');
    section.innerHTML = '<div class="section-header"><button class="collapse-toggle"></button>' +
                        '<span class="timestamp"></span><span class="caption"></span></div>' +
                        '<div class="message-content"></div>';
    section.querySelector('.timestamp').textContent = timeStamp;
    section.querySelector('.caption').textContent = caption;
    transcript.append(section);
    return section.querySelector('.message-content');
}

function beginMessage(event) {
    const roles = {
        SystemPrompt: ['system', 'System Prompt', true],
        ToolDefinition: ['tool-definition', 'Tool Definition: ' + event.name, true],
        User: ['user', 'User', false],
        AgentThinking: ['thinking', 'Agent Thinking', false],
        Agent: ['agent', 'Agent', false],
        Error: ['error', 'Error', false],
        ToolCall: ['tool-call', 'Tool Call #' + event.toolCallID, false],
    };
    const role = roles[event.role];
    if (!role)
        return;
    openMessage = createSection(role[0], role[1], event.timeStamp, role[2]);
}

function handleEvent(event) {
    switch (event.operation) {
        case 'BeginMessage':
            beginMessage(event);
            break;
        case 'AppendText':
            if (openMessage)
                openMessage.append(document.createTextNode(event.text));
            break;
        case 'EndMessage':
            if (!openMessage)
                break;
            openMessage.append(document.createTextNode('\n'));
            if (event.footer) {
                const footer = document.createElement('span');
                footer.className = 'timing';
                footer.textContent = event.footer;
                openMessage.append(footer, document.createTextNode('\n'));
            }
            openMessage = null;
            break;
        case 'AppendToolResponse':
            toolResponses.set(event.toolCallID, (toolResponses.get(event.toolCallID) || '') + event.text);
            break;
        case 'EndToolResponse': {
            const content = createSection('tool-response', 'Tool Response #' + event.toolCallID,
                                          event.timeStamp, true);
            content.textContent = (toolResponses.get(event.toolCallID) || '') + '\n';
            toolResponses.delete(event.toolCallID);
            break;
        }
    }
}

async function streamEvents() {
    const response = await fetch('/events');
    if (!response.ok || !response.body)
        throw new Error('Unable to stream transcript events');
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let pending = '';
    for (;;) {
        const result = await reader.read();
        pending += decoder.decode(result.value || new Uint8Array(), {stream: !result.done});
        const lines = pending.split('\n');
        pending = lines.pop();
        for (const line of lines) {
            if (line)
                handleEvent(JSON.parse(line));
        }
        if (result.done) {
            if (pending)
                handleEvent(JSON.parse(pending));
            return;
        }
    }
}

streamEvents().catch(error => console.error(error));
</script></body></html>
)");
}

// Streams the transcript accumulated so far, followed by new JSONL events.
static void serveWebEvents(HTTPServerRequest& request) {
    HTTPServerResponse response{HTTPServerResponse::OK};
    *response.headers.insert("content-type").value = "application/x-ndjson; charset=utf-8";
    *response.headers.insert("cache-control").value = "no-store";
    Stream out = request.beginStreamingResponse(std::move(response));

    // Replay all existing events, then block until more are published. Once the
    // transcript is complete, drain the remaining events and close the stream.
    u32 nextChunk = 0;
    for (;;) {
        String chunk;
        bool isComplete = false;
        {
            LockGuard<Mutex> lock{webTranscript.mutex};
            if (nextChunk >= webTranscript.chunks.numItems() && !webTranscript.isComplete)
                webTranscript.changed.timedWait(lock, 15000);
            if (nextChunk < webTranscript.chunks.numItems()) {
                chunk = webTranscript.chunks[nextChunk++];
            } else {
                isComplete = webTranscript.isComplete;
            }
        }
        if (isComplete) {
            out.flush(true);
            return;
        }
        // A blank JSONL line keeps an idle connection alive and is ignored by the client.
        StringView bytes = chunk ? StringView{chunk} : StringView{"\n"};
        if (out.write(bytes) != bytes.numBytes())
            return;
        out.flush(true);
    }
}

// Routes browser page and event stream requests.
static void serveWebTranscript(HTTPServerRequest& request) {
    if (request.uri == "/events") {
        serveWebEvents(request);
    } else if (request.uri == "/" || request.uri == "/index.html") {
        serveWebPage(request);
    } else {
        request.sendGenericResponse(HTTPServerResponse::NotFound);
    }
}

// Creates a JSON object with the operation shared by every web event.
static json::Node makeWebEvent(StringView operation) {
    json::Node event{json::Node::Object{}};
    event.set("operation", json::Node::Text{String{operation}});
    return event;
}

// Publishes the header of a message section.
static void webBeginMessage(StringView role, StringView timeStamp, StringView name = {}, u32 toolCallID = 0) {
    json::Node event = makeWebEvent("BeginMessage");
    event.set("timeStamp", json::Node::Text{String{timeStamp}});
    event.set("role", json::Node::Text{String{role}});
    if (name)
        event.set("name", json::Node::Text{String{name}});
    if (role == "ToolCall")
        event.set("toolCallID", json::Node::Number{double(toolCallID)});
    webTranscript.append(std::move(event));
}

// Publishes text belonging to the currently open message.
static void webAppendText(StringView text) {
    json::Node event = makeWebEvent("AppendText");
    event.set("text", json::Node::Text{String{text}});
    webTranscript.append(std::move(event));
}

// Publishes the end of the current message and its optional statistics footer.
static void webEndMessage(StringView footer = {}) {
    json::Node event = makeWebEvent("EndMessage");
    if (footer)
        event.set("footer", json::Node::Text{String{footer}});
    webTranscript.append(std::move(event));
}

// Publishes a chunk belonging to a buffered tool response.
static void webAppendToolResponse(u32 toolCallID, StringView text) {
    json::Node event = makeWebEvent("AppendToolResponse");
    event.set("toolCallID", json::Node::Number{double(toolCallID)});
    event.set("text", json::Node::Text{String{text}});
    webTranscript.append(std::move(event));
}

// Publishes the end of a tool response with the timestamp used by its section.
static void webEndToolResponse(u32 toolCallID, StringView timeStamp) {
    json::Node event = makeWebEvent("EndToolResponse");
    event.set("toolCallID", json::Node::Number{double(toolCallID)});
    event.set("timeStamp", json::Node::Text{String{timeStamp}});
    webTranscript.append(std::move(event));
}

//  ▄▄▄▄▄▄                        ▄▄               ▄▄▄       ▄▄▄▄          ▄▄                  ▄▄
//    ██    ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄   ██      ██  ██ ▄▄  ▄▄ ▄██▄▄ ▄▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//    ██   ██▄▄██ ██  ▀▀ ██ ██ ██ ██ ██  ██  ▄▄▄██  ██      ██  ██ ██  ██  ██   ██  ██ ██  ██  ██
//    ██   ▀█▄▄▄  ██     ██ ██ ██ ██ ██  ██ ▀█▄▄██ ▄██▄     ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ██▄▄█▀ ▀█▄▄██  ▀█▄▄
//                                                                              ██

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
    u32 openToolCallID = 0;
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

    {
        // Write system prompt to stdout.
        Stream out = getStdOut();
        out.format("{} [System Prompt]\n", formatTimeStamp(now));
        out.format("{}\n", toolSet.systemMessage);
        out.format("{}\n", Separator);
    }
    if (options.runWebServer) {
        // Stream the system prompt to the web browser.
        webBeginMessage("SystemPrompt", formatTimeStamp(now));
        webAppendText(toolSet.systemMessage);
        webEndMessage();
    }

    // Tool definition blocks.
    {
        Stream out = getStdOut();
        for (const Owned<ToolSet::Handler>& tool : toolSet.handlers) {
            // Write tool definition to stdout.
            out.format("{} [Tool Definition: {}]\n", formatTimeStamp(now), tool->name);
            for (const ToolSet::Parameter& param : tool->parameters)
                out.format("`{}`: {}\n", param.name, param.description);
            out.format("{}\n", tool->description);
            out.format("{}\n", Separator);

            if (options.runWebServer) {
                // Stream the tool definition to the web browser.
                MemStream text;
                for (const ToolSet::Parameter& param : tool->parameters)
                    text.format("`{}`: {}\n", param.name, param.description);
                text.format("{}", tool->description);
                webBeginMessage("ToolDefinition", formatTimeStamp(now), tool->name);
                webAppendText(text.moveToString());
                webEndMessage();
            }
        }
    }

    // User block. Its header and content are emitted now, but it is left open so
    // that its elapsed time reflects the wait for the agent's first response.
    {
        // Write user header to stdout.
        Stream out = getStdOut();
        out.format("{} [User]\n", formatTimeStamp(now));
        out.format("{}\n", userPrompt);
    }
    this->hasOpen = true;
    this->openIsTextMsg = true;
    this->openRole = Transcript::Role::User;
    this->sectionStartTime = now;
    this->openOutputBytes = userPrompt.numBytes();
    if (options.runWebServer) {
        // Stream the user message to the web browser.
        webBeginMessage("User", formatTimeStamp(now));
        webAppendText(userPrompt);
        webAppendText("\n");
    }
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
    this->openToolCallID = toolCallID;
    this->openToolCallText = {};

    Stream out = getStdOut();
    switch (role) {
        case Transcript::Role::AgentThinking:
            out.format("{} [Agent Thinking]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer)
                webBeginMessage("AgentThinking", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::Agent:
            out.format("{} [Agent]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer)
                webBeginMessage("Agent", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::Error:
            out.format("{} [Error]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer)
                webBeginMessage("Error", formatTimeStamp(timeStamp));
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::ToolCall:
            out.format("{} [Tool Call #{}]\n", formatTimeStamp(timeStamp), toolCallID);
            if (options.runWebServer)
                webBeginMessage("ToolCall", formatTimeStamp(timeStamp), {}, toolCallID);
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
        if (!this->openLastWasNewline)
            out.format("\n");

        // Error messages don't include output statistics.
        if (this->openRole == Transcript::Role::Error) {
            if (options.runWebServer)
                webEndMessage();
        } else {
            double elapsedSec = double(endMicros - this->sectionStartTime) / 1000000.0;
            if (elapsedSec < 0)
                elapsedSec = 0;
            double rate = (elapsedSec > 1e-9) ? double(this->openOutputBytes) / elapsedSec : 0;
            String footer = String::format("({:.1}s elapsed, {} output, {})", elapsedSec,
                                           formatSize(this->openOutputBytes), formatRate(rate));

            // Write message footer to stdout.
            out.format("{}\n", footer);
            if (options.runWebServer)
                webEndMessage(footer);
        }
    } else if (this->openRole == Transcript::Role::ToolCall) {
        // Flush tool call to stdout.
        String formatted = formatToolCall(this->openToolCallText);
        out.format("{}\n", formatted);
        if (options.runWebServer) {
            // Publish the formatted tool call as a complete message.
            webAppendText(formatted);
            webEndMessage();
        }
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
        // Write tool response to stdout.
        out.format("{} [Tool Response #{}]\n", formatTimeStamp(timeStamp), id);
        out.format("{}\n", *text);
        out.format("{}\n", Separator);
        if (options.runWebServer) {
            // Display the response after all of its streamed chunks have arrived.
            webEndToolResponse(id, formatTimeStamp(timeStamp));
        }
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
                    if (options.runWebServer)
                        webAppendText(event.text);
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
            if (options.runWebServer)
                webAppendToolResponse(event.toolCallID, event.text);
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

//  ▄▄   ▄▄        ▄▄
//  ███▄███  ▄▄▄▄  ▄▄ ▄▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██ ██  ██
//  ██   ██ ▀█▄▄██ ██ ██  ██
//

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
        if (!envVar)
            return {};
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
        {"-j", PLY_LOOKUP_MEMBER(CommandLineOptions, settingsPath), "Path to JSON settings file"},
        {"-l", PLY_LOOKUP_MEMBER(CommandLineOptions, enableHttpLog), "Write raw HTTP log"},
        {"-s", PLY_LOOKUP_MEMBER(CommandLineOptions, runWebServer), "Serve transcript on port 8081"},
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
        webServerThread.run([] { runHTTPServer(8081, serveWebTranscript); });
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
    if (options.runWebServer)
        webTranscript.complete();

    // Keep serving HTTP requests after the agent finishes.
    if (webServerThread.isValid()) {
        getStdOut().write("Waiting for HTTP connections...");
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
