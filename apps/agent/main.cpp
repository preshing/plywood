/*───────────────────────────────────────────────────────────┐
│                                                            │
│     ____      Plywood C++ Runtime Library                  │
│    ╱   ╱╲     https://plywood.dev/                         │
│   ╱___╱╭╮╲                                                 │
│    └──┴┴┴┘    agent                                        │
│               Documentation: /docs/apps/agent.md           │
│                                                            │
└───────────────────────────────────────────────────────────*/

#include <ply-system.h>
#include <ply-json.h>
#include <ply-agent.h>
#include <ply-network.h>
#include <ply-markdown.h>
#include <curl/curl.h>

using namespace ply;

//   ▄▄▄▄  ▄▄▄         ▄▄            ▄▄▄
//  ██  ▀▀  ██   ▄▄▄▄  ██▄▄▄   ▄▄▄▄   ██   ▄▄▄▄
//  ██ ▀██  ██  ██  ██ ██  ██  ▄▄▄██  ██  ▀█▄▄▄
//  ▀█▄▄█▀ ▄██▄ ▀█▄▄█▀ ██▄▄█▀ ▀█▄▄██ ▄██▄  ▄▄▄█▀
//

struct CommandLineOptions {
    String settingsPath;
    String provider;
    String model;
    bool enableHttpLog = false;
    bool runWebServer = false;
    String userPrompt;
    bool printUsage = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

CommandLineOptions options;
Agent::Settings agentSettings;
String tempUserPrompt; // Will remove later
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
static void serveWebPage(HTTPServer::Request& request) {
    HTTPServer::Response response{HTTPServer::Response::OK};
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
.collapse-toggle { width: 17px; height: 17px; margin: 0 8px 0 0; padding: 0; border: 0; background: transparent;
    color: inherit; vertical-align: -3px; cursor: pointer; background-color: currentColor;
    -webkit-mask: var(--collapse-icon) center / contain no-repeat; mask: var(--collapse-icon) center / contain no-repeat; }
.section.collapsed .collapse-toggle { --collapse-icon: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Ccircle cx='8' cy='8' r='6' fill='none' stroke='black' stroke-width='1'/%3E%3Cpath d='M5 8h6M8 5v6' fill='none' stroke='black' stroke-linecap='round' stroke-width='1'/%3E%3C/svg%3E"); }
.section:not(.collapsed) .collapse-toggle { --collapse-icon: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Ccircle cx='8' cy='8' r='6' fill='none' stroke='black' stroke-width='1'/%3E%3Cpath d='M5 8h6' fill='none' stroke='black' stroke-linecap='round' stroke-width='1'/%3E%3C/svg%3E"); }
.collapse-toggle:focus-visible { outline: 1px solid currentColor; outline-offset: 2px; }
.section.collapsed > .message-content { display: none; }
.section.collapsed > .section-header { border-radius: 6px; }
.message-content { padding: 4px 12px; white-space: pre-wrap; }
.message-content.markdown { font: 15px/1.55 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    white-space: normal; }
.markdown > :first-child { margin-top: 0; }
.markdown p { margin: 0.65em 0; }
.markdown h1, .markdown h2, .markdown h3, .markdown h4, .markdown h5, .markdown h6 {
    margin: 1em 0 0.45em; line-height: 1.25; color: #f2f0ed; }
.markdown h1 { font-size: 1.65em; }
.markdown h2 { font-size: 1.4em; }
.markdown h3 { font-size: 1.2em; }
.markdown h4, .markdown h5, .markdown h6 { font-size: 1em; }
.markdown a { color: #75b7ff; text-decoration-thickness: 1px; text-underline-offset: 2px; }
.markdown code, .markdown pre { font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace; }
.markdown code { padding: 0.12em 0.32em; background: #151617; border-radius: 3px; font-size: 0.9em; }
.markdown pre { overflow-x: auto; margin: 0.8em 0; padding: 10px 12px; background: #151617; border-radius: 4px;
    line-height: 1.45; white-space: pre; }
.markdown pre code { padding: 0; background: transparent; border-radius: 0; font-size: inherit; }
.markdown blockquote { margin: 0.8em 0; padding: 0.05em 0 0.05em 1em; border-left: 3px solid #697178;
    color: #b9bdc1; }
.markdown ul, .markdown ol { margin: 0.65em 0; padding-left: 1.8em; }
.markdown li { margin: 0.2em 0; }
.markdown hr { height: 1px; margin: 1.2em 0; border: 0; background: #555a5e; }
.markdown table { display: block; overflow-x: auto; margin: 0.8em 0; border-collapse: collapse; }
.markdown th, .markdown td { padding: 5px 9px; border: 1px solid #555a5e; text-align: left; }
.markdown th { background: #34373a; }
.markdown img { max-width: 100%; }
.system > .section-header, .tool-definition > .section-header { background: #393c3f; }
.user > .section-header { background: #3b5d4b; }
.thinking > .section-header { background: #4b7380; }
.agent > .section-header { background: #244a7c; }
.error > .section-header { background: #813b3b; }
.tool-call > .section-header, .tool-response > .section-header { background: #393c3f; }
.timestamp { margin-right: 10px; color: #c0c3c5; font-size: 0.85em; font-weight: normal; }
.timing { font-family: system-ui, sans-serif; color: #9aa0a6; }
</style></head><body><main id="transcript"></main>
<script>
const transcript = document.getElementById('transcript');
let openMessage = null;
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();
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
    openMessage = {
        content: createSection(role[0], role[1], event.timeStamp, role[2]),
        byteChunks: [],
        numBytes: 0,
        isMarkdown: false,
    };
}

function appendText(text) {
    if (!openMessage)
        return;
    const bytes = textEncoder.encode(text);
    openMessage.byteChunks.push(bytes);
    openMessage.numBytes += bytes.length;
    openMessage.content.append(document.createTextNode(text));
}

// Reconstructs the visible message from its authoritative UTF-8 source bytes.
function renderOpenMessage() {
    const bytes = new Uint8Array(openMessage.numBytes);
    let offset = 0;
    for (const chunk of openMessage.byteChunks) {
        bytes.set(chunk, offset);
        offset += chunk.length;
    }
    const source = textDecoder.decode(bytes);
    if (openMessage.isMarkdown) {
        openMessage.content.innerHTML = source;
    } else {
        openMessage.content.textContent = source;
    }
}

function appendHtml(html) {
    if (!openMessage)
        return;
    const bytes = textEncoder.encode(html);
    openMessage.byteChunks.push(bytes);
    openMessage.numBytes += bytes.length;
    openMessage.isMarkdown = true;
    openMessage.content.classList.add('markdown');
    renderOpenMessage();
}

function eraseText(numBytes) {
    if (!openMessage || numBytes <= 0)
        return;

    // Trim whole chunks first, then shorten the final remaining chunk if needed.
    let remaining = Math.min(numBytes, openMessage.numBytes);
    openMessage.numBytes -= remaining;
    while (remaining > 0) {
        const last = openMessage.byteChunks.length - 1;
        const chunk = openMessage.byteChunks[last];
        if (remaining >= chunk.length) {
            remaining -= chunk.length;
            openMessage.byteChunks.pop();
        } else {
            openMessage.byteChunks[last] = chunk.slice(0, chunk.length - remaining);
            remaining = 0;
        }
    }

    // Reconstruct the visible content from the remaining authoritative bytes.
    renderOpenMessage();
}

function handleEvent(event) {
    switch (event.operation) {
        case 'BeginMessage':
            beginMessage(event);
            break;
        case 'AppendText':
            appendText(event.text);
            break;
        case 'AppendHtml':
            appendHtml(event.html);
            break;
        case 'EraseText':
            eraseText(event.numBytes);
            break;
        case 'EndMessage':
            if (!openMessage)
                break;
            openMessage.content.append(document.createTextNode('\n'));
            if (event.footer) {
                const footer = document.createElement('span');
                footer.className = 'timing';
                footer.textContent = event.footer;
                openMessage.content.append(footer, document.createTextNode('\n'));
            }
            openMessage = null;
            break;
        case 'AppendToolResponse':
            if (!toolResponses.has(event.toolCallID)) {
                toolResponses.set(event.toolCallID, []);
            }
            toolResponses.get(event.toolCallID).push(event.text);
            break;
        case 'EndToolResponse': {
            const content = createSection('tool-response', 'Tool Response #' + event.toolCallID,
                                          event.timeStamp, true);
            for (const line of toolResponses.get(event.toolCallID) || [])
                content.append(document.createTextNode(line));
            content.append(document.createTextNode('\n'));
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
static void serveWebEvents(HTTPServer::Request& request) {
    HTTPServer::Response response{HTTPServer::Response::OK};
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
            if (nextChunk >= webTranscript.chunks.numItems() && !webTranscript.isComplete) {
                webTranscript.changed.timedWait(lock, 15000);
            }
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
static void serveWebTranscript(HTTPServer::Request& request) {
    if (request.uri == "/events") {
        serveWebEvents(request);
    } else if (request.uri == "/" || request.uri == "/index.html") {
        serveWebPage(request);
    } else {
        request.sendGenericResponse(HTTPServer::Response::NotFound);
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
    if (name) {
        event.set("name", json::Node::Text{String{name}});
    }
    if (role == "ToolCall") {
        event.set("toolCallID", json::Node::Number{double(toolCallID)});
    }
    webTranscript.append(std::move(event));
}

// Publishes text belonging to the currently open message.
static void webAppendText(StringView text) {
    json::Node event = makeWebEvent("AppendText");
    event.set("text", json::Node::Text{String{text}});
    webTranscript.append(std::move(event));
}

// Publishes rendered HTML belonging to the currently open message.
static void webAppendHtml(StringView html) {
    json::Node event = makeWebEvent("AppendHtml");
    event.set("html", json::Node::Text{String{html}});
    webTranscript.append(std::move(event));
}

// Removes bytes from the end of the currently open message.
static void webEraseText(uptr numBytes) {
    json::Node event = makeWebEvent("EraseText");
    event.set("numBytes", json::Node::Number{double(numBytes)});
    webTranscript.append(std::move(event));
}

// Publishes the end of the current message and its optional statistics footer.
static void webEndMessage(StringView footer = {}) {
    json::Node event = makeWebEvent("EndMessage");
    if (footer) {
        event.set("footer", json::Node::Text{String{footer}});
    }
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

//   ▄▄▄▄                              ▄▄▄              ▄▄▄▄          ▄▄                  ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ██   ▄▄▄▄      ██  ██ ▄▄  ▄▄ ▄██▄▄ ▄▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██     ██  ██ ██  ██ ▀█▄▄▄  ██  ██  ██  ██▄▄██     ██  ██ ██  ██  ██   ██  ██ ██  ██  ██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██  ██  ▄▄▄█▀ ▀█▄▄█▀ ▄██▄ ▀█▄▄▄      ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ██▄▄█▀ ▀█▄▄██  ▀█▄▄
//                                                                         ██

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
        for (const auto& item : result.root.object().items) {
            out.format(" {}={}", item.key, formatJsonValue(item.value));
        }
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
    bool openIsTextMsg = false;    // true for User/AgentThinking/Agent/Error
    bool openUsesMarkdown = false; // true only for User/AgentThinking/Agent
    Transcript::Role openRole = Transcript::Role::None;
    s64 sectionStartTime = 0;
    uptr openOutputBytes = 0;
    bool openLastWasNewline = true; // ensures the timing line starts on its own line
    u32 openToolCallID = 0;
    String openToolCallText; // accumulated raw text for a ToolCall section

    // Incremental Markdown state for the currently open browser message.
    Owned<markdown::Parser> markdownParser;
    MemStream markdownLine;
    markdown::HTMLOptions markdownOptions;
    String predictedMarkdownHtml;

    // Buffered tool responses, keyed by toolCallID, awaiting flush at EndTurn.
    Map<u32, Transcript::Buffer> pendingResponses;
    Array<u32> responseOrder; // toolCallIDs in first-arrival order

    void printStartup(StringView userPrompt);
    void handleEvent(const TranscriptEvent& event);
    void finish(s64 endMicros);

    void openSection(Transcript::Role role, u32 toolCallID, s64 timeStamp);
    void closeOpen(s64 endMicros);
    void flushToolResponses(s64 timeStamp);
    void beginMarkdownMessage();
    void appendMarkdown(StringView text);
    void appendMarkdownBlockHtml(MemStream* html, Owned<markdown::Block>&& block);
    void reconcileMarkdownHtml(StringView finalizedHtml, StringView predictedHtml);
    void endMarkdownMessage(StringView footer = {});
};

// Starts incremental Markdown conversion for a browser message section.
void TranscriptPrinter::beginMarkdownMessage() {
    this->markdownParser = markdown::createParser();
    this->markdownLine = MemStream{};
    this->predictedMarkdownHtml = {};
}

// Converts a completed Markdown block to HTML and appends it to the supplied buffer.
void TranscriptPrinter::appendMarkdownBlockHtml(MemStream* html, Owned<markdown::Block>&& block) {
    markdown::convertToHtml(html, block, this->markdownOptions);
}

// Replaces only the differing suffix of the previously predicted HTML.
void TranscriptPrinter::reconcileMarkdownHtml(StringView finalizedHtml, StringView predictedHtml) {
    String replacementHtml = finalizedHtml + predictedHtml;

    // Preserve the longest byte-identical prefix already displayed by the browser.
    uptr commonBytes = 0;
    uptr maxCommonBytes = min(this->predictedMarkdownHtml.numBytes(), replacementHtml.numBytes());
    while (commonBytes < maxCommonBytes && this->predictedMarkdownHtml[commonBytes] == replacementHtml[commonBytes]) {
        commonBytes++;
    }

    uptr numBytesToErase = this->predictedMarkdownHtml.numBytes() - commonBytes;
    if (numBytesToErase > 0) {
        webEraseText(numBytesToErase);
    }
    if (commonBytes < replacementHtml.numBytes()) {
        webAppendHtml(replacementHtml.substr(commonBytes));
    }
    this->predictedMarkdownHtml = predictedHtml;
}

// Parses completed lines, then predicts the HTML for all unfinished input without advancing the real parser.
void TranscriptPrinter::appendMarkdown(StringView text) {
    MemStream finalizedHtml;
    StringView remaining = text;
    while (remaining) {
        s32 newlinePos = remaining.find('\n');
        if (newlinePos < 0) {
            this->markdownLine.write(remaining);
            break;
        }

        // Include the newline in the line passed to the Markdown parser.
        this->markdownLine.write(remaining.left(newlinePos + 1));
        String line = this->markdownLine.moveToString();
        this->markdownLine = MemStream{};
        if (Owned<markdown::Block> block = markdown::parseLine(this->markdownParser, line)) {
            this->appendMarkdownBlockHtml(&finalizedHtml, std::move(block));
        }
        remaining = remaining.substr(newlinePos + 1);
    }

    // Parse the partial line and flush a duplicate so the authoritative parser remains resumable.
    MemStream predictedHtml;
    Owned<markdown::Parser> predictionParser = this->markdownParser;
    if (this->markdownLine.getSeekPos() > 0) {
        MemStream partialLineCopy = this->markdownLine.duplicate();
        String partialLine = partialLineCopy.moveToString();
        if (Owned<markdown::Block> block = markdown::parseLine(predictionParser, partialLine)) {
            this->appendMarkdownBlockHtml(&predictedHtml, std::move(block));
        }
    }
    if (Owned<markdown::Block> block = markdown::flush(predictionParser)) {
        this->appendMarkdownBlockHtml(&predictedHtml, std::move(block));
    }
    this->reconcileMarkdownHtml(finalizedHtml.moveToString(), predictedHtml.moveToString());
}

// Parses the final partial line, flushes the parser and closes the browser section.
void TranscriptPrinter::endMarkdownMessage(StringView footer) {
    MemStream finalizedHtml;
    String line = this->markdownLine.moveToString();
    this->markdownLine = MemStream{};
    if (line) {
        if (Owned<markdown::Block> block = markdown::parseLine(this->markdownParser, line)) {
            this->appendMarkdownBlockHtml(&finalizedHtml, std::move(block));
        }
    }
    if (Owned<markdown::Block> block = markdown::flush(this->markdownParser)) {
        this->appendMarkdownBlockHtml(&finalizedHtml, std::move(block));
    }
    this->reconcileMarkdownHtml(finalizedHtml.moveToString(), {});
    this->markdownParser = {};
    webEndMessage(footer);
}

void TranscriptPrinter::printStartup(StringView userPrompt) {
    s64 now = getUnixTimestamp();

    {
        // Write system prompt to stdout.
        Stream out = getStdOut();
        out.format("{} [System Prompt]\n", formatTimeStamp(now));
        out.format("{}\n", agentSettings.toolSet.systemPrompt);
        out.format("{}\n", Separator);
    }
    if (options.runWebServer) {
        // Stream the system prompt to the web browser.
        webBeginMessage("SystemPrompt", formatTimeStamp(now));
        webAppendText(agentSettings.toolSet.systemPrompt);
        webEndMessage();
    }

    // Tool definition blocks.
    {
        Stream out = getStdOut();
        for (const Owned<ToolSet::Handler>& tool : agentSettings.toolSet.handlers) {
            // Write tool definition to stdout.
            out.format("{} [Tool Definition: {}]\n", formatTimeStamp(now), tool->name);
            for (const ToolSet::Parameter& param : tool->parameters) {
                out.format("`{}`: {}\n", param.name, param.description);
            }
            out.format("{}\n", tool->description);
            out.format("{}\n", Separator);

            if (options.runWebServer) {
                // Stream the tool definition to the web browser.
                MemStream text;
                for (const ToolSet::Parameter& param : tool->parameters) {
                    text.format("`{}`: {}\n", param.name, param.description);
                }
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
    this->openUsesMarkdown = true;
    this->sectionStartTime = now;
    this->openOutputBytes = userPrompt.numBytes();
    if (options.runWebServer) {
        // Stream the user message to the web browser.
        webBeginMessage("User", formatTimeStamp(now));
        this->beginMarkdownMessage();
        this->appendMarkdown(userPrompt);
        this->appendMarkdown("\n");
    }
}

void TranscriptPrinter::openSection(Transcript::Role role, u32 toolCallID, s64 timeStamp) {
    // Close any previously open section before starting a new one.
    if (this->hasOpen) {
        this->closeOpen(timeStamp);
    }

    this->hasOpen = true;
    this->openRole = role;
    this->openUsesMarkdown = false;
    this->sectionStartTime = timeStamp;
    this->openOutputBytes = 0;
    this->openLastWasNewline = true;
    this->openToolCallID = toolCallID;
    this->openToolCallText = {};

    Stream out = getStdOut();
    switch (role) {
        case Transcript::Role::AgentThinking:
            out.format("{} [Agent Thinking]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer) {
                webBeginMessage("AgentThinking", formatTimeStamp(timeStamp));
            }
            this->openIsTextMsg = true;
            this->openUsesMarkdown = true;
            break;
        case Transcript::Role::Agent:
            out.format("{} [Agent]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer) {
                webBeginMessage("Agent", formatTimeStamp(timeStamp));
            }
            this->openIsTextMsg = true;
            this->openUsesMarkdown = true;
            break;
        case Transcript::Role::Error:
            out.format("{} [Error]\n", formatTimeStamp(timeStamp));
            if (options.runWebServer) {
                webBeginMessage("Error", formatTimeStamp(timeStamp));
            }
            this->openIsTextMsg = true;
            break;
        case Transcript::Role::ToolCall:
            out.format("{} [Tool Call #{}]\n", formatTimeStamp(timeStamp), toolCallID);
            if (options.runWebServer) {
                webBeginMessage("ToolCall", formatTimeStamp(timeStamp), {}, toolCallID);
            }
            this->openIsTextMsg = false;
            break;
        default:
            this->openIsTextMsg = false;
            break;
    }
    if (options.runWebServer && this->openUsesMarkdown) {
        this->beginMarkdownMessage();
    }
}

void TranscriptPrinter::closeOpen(s64 endMicros) {
    Stream out = getStdOut();
    if (this->openIsTextMsg) {
        if (!this->openLastWasNewline) {
            out.format("\n");
        }

        // Error messages don't include output statistics.
        if (this->openRole == Transcript::Role::Error) {
            if (options.runWebServer) {
                webEndMessage();
            }
        } else {
            double elapsedSec = double(endMicros - this->sectionStartTime) / 1000000.0;
            if (elapsedSec < 0) {
                elapsedSec = 0;
            }
            double rate = (elapsedSec > 1e-9) ? double(this->openOutputBytes) / elapsedSec : 0;
            String footer = String::format("({:.1}s elapsed, {} output, {})", elapsedSec,
                                           formatSize(this->openOutputBytes), formatRate(rate));

            // Write message footer to stdout.
            out.format("{}\n", footer);
            if (options.runWebServer) {
                if (this->openUsesMarkdown) {
                    this->endMarkdownMessage(footer);
                } else {
                    webEndMessage(footer);
                }
            }
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
        Transcript::Buffer* response = this->pendingResponses.find(id);
        if (!response)
            continue;
        response->flush();
        // Write tool response to stdout.
        out.format("{} [Tool Response #{}]\n", formatTimeStamp(timeStamp), id);
        for (const String& line : response->lines) {
            out.write(line);
        }
        out.write('\n');
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
                    if (event.text) {
                        this->openLastWasNewline = event.text.endsWith('\n');
                    }
                    if (options.runWebServer) {
                        if (this->openUsesMarkdown) {
                            this->appendMarkdown(event.text);
                        } else {
                            webAppendText(event.text);
                        }
                    }
                } else if (this->openRole == Transcript::Role::ToolCall) {
                    // Accumulate the tool call's raw text; it is formatted on close.
                    this->openToolCallText += event.text;
                }
            }
            break;
        case TranscriptEvent::AppendToolResponse: {
            // Buffer the response; it is flushed at EndTurn.
            auto ins = this->pendingResponses.insert(event.toolCallID);
            if (!ins.wasFound) {
                this->responseOrder.append(event.toolCallID);
            }
            ins.value->append(event.text);
            if (options.runWebServer) {
                webAppendToolResponse(event.toolCallID, event.text);
            }
            break;
        }
        case TranscriptEvent::EndToolResponse:
            if (Transcript::Buffer* response = this->pendingResponses.find(event.toolCallID)) {
                response->flush();
            }
            break;
        case TranscriptEvent::EndTurn:
            if (this->hasOpen) {
                this->closeOpen(event.timeStamp);
            }
            this->flushToolResponses(event.timeStamp);
            break;
        default:
            break;
    }
}

void TranscriptPrinter::finish(s64 endMicros) {
    if (this->hasOpen) {
        this->closeOpen(endMicros);
    }
    // Flush any tool responses that never got an EndTurn (e.g. the agent was
    // canceled mid-turn).
    this->flushToolResponses(endMicros);
}

//   ▄▄▄▄          ▄▄    ▄▄   ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄ ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄  ▄▄▄▄
//   ▀▀▀█▄ ██▄▄██  ██    ██   ██ ██  ██ ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄  ▀█▄▄ ██ ██  ██ ▀█▄▄██  ▄▄▄█▀
//                                       ▄▄▄█▀

// Load a chain of JSON settings files by following includes and merge them into one.
static bool loadSettingsWithIncludes(StringView settingsPath, Array<String>& includedPaths) {
    // Reject include cycles before loading the next file.
    if (find(includedPaths, settingsPath) >= 0) {
        getStdErr().format("Configuration include cycle detected at: {}\n", settingsPath);
        return false;
    }
    includedPaths.append(settingsPath);
    PLY_ON_SCOPE_EXIT({ includedPaths.pop(); });

    // Load this settings file.
    String jsonText = FileSystem::loadText(settingsPath);
    if (!jsonText) {
        getStdErr().format("Could not load configuration file: {}\n", settingsPath);
        return false;
    }

    // Parse and validate the root node.
    json::Parser parser;
    json::Parser::Result result = parser.parse(settingsPath, jsonText);
    if (parser.anyError() || !result.root.isObject()) {
        getStdErr().format("Failed to parse configuration file: {}\n", settingsPath);
        return false;
    }
    const json::Node& root = result.root;

    // Load included settings file (if any).
    if (const json::Node& jInclude = root.get("include")) {
        if (!jInclude.isText()) {
            getStdErr().format("The 'include' property must be a string in: {}\n", settingsPath);
            return false;
        }
        String parentPath = joinPath(splitPath(settingsPath).directory, jInclude.text());
        if (!loadSettingsWithIncludes(parentPath, includedPaths))
            return false;
    }

    // Import workingDirectory.
    String workingDir = splitPath(settingsPath).directory;
    if (auto jWorkingDir = root.get("workingDirectory")) {
        if (!jWorkingDir.isText()) {
            getStdErr().format("workingDirectory must be a string in: {}\n", settingsPath);
            return false;
        }
        workingDir = joinPath(workingDir, jWorkingDir.text());
    }
    // The agent's working directory is determined by the innermost settings file.
    agentSettings.toolSet.workingDirectory = workingDir;

    // Import endPoint.
    if (const auto& jEndPoint = root.get("endPoint")) {
        if (!jEndPoint.isObject()) {
            getStdErr().format("endPoint must be an object in: {}\n", settingsPath);
            return false;
        }

        // Inherited endpoints must be fully replaced.
        agentSettings.endPoint = {};

        // Import url.
        const auto& jUrl = jEndPoint.get("url");
        if (!jUrl.isText()) {
            getStdErr().format("endPoint.url must be a string in: {}\n", settingsPath);
            return false;
        }
        agentSettings.endPoint.url = jUrl.text();

        // Import apiKeyEnv.
        const auto& jApiKeyEnv = jEndPoint.get("apiKeyEnv");
        if (!jApiKeyEnv.isText()) {
            getStdErr().format("endPoint.apiKeyEnv must be a string in: {}\n", settingsPath);
            return false;
        }
        // The API key is read from the named environment variable on demand.
        agentSettings.endPoint.apiKeyEnv = jApiKeyEnv.text();

        // Import protocol.
        const auto& jProtocol = jEndPoint.get("protocol");
        if (!jProtocol.isText()) {
            getStdErr().format("endPoint.protocol must be a string in: {}\n", settingsPath);
            return false;
        }
        StringView protocol = jProtocol.text();
        if (protocol == "completions") {
            agentSettings.endPoint.protocol = Protocol::Completions;
        } else if (protocol == "responses") {
            agentSettings.endPoint.protocol = Protocol::Responses;
        } else if (protocol == "anthropic") {
            agentSettings.endPoint.protocol = Protocol::Anthropic;
        } else {
            getStdErr().format("Unknown protocol '{}' in: {}\n", protocol, settingsPath);
            return false;
        }

        // Import model.
        const auto& jModel = jEndPoint.get("model");
        if (!jModel.isText()) {
            getStdErr().format("endPoint.model must be a string in: {}\n", settingsPath);
            return false;
        }
        agentSettings.endPoint.model = jModel.text();
    }

    // Import systemPrompt.
    if (const auto& jSystemPrompt = root.get("systemPrompt")) {
        if (!jSystemPrompt.isText()) {
            getStdErr().format("systemPrompt must be a string in: {}\n", settingsPath);
            return false;
        }
        // Append this prompt after any prompt inherited from an included settings file.
        if (agentSettings.toolSet.systemPrompt) {
            agentSettings.toolSet.systemPrompt += '\n';
        }
        agentSettings.toolSet.systemPrompt += jSystemPrompt.text();
    }

    // Import userPrompt.
    if (const auto& jUserPrompt = root.get("userPrompt")) {
        if (!jUserPrompt.isText()) {
            getStdErr().format("userPrompt must be a string in: {}\n", settingsPath);
            return false;
        }
        tempUserPrompt = jUserPrompt.text();
    }

    // Import directory permissions.
    if (const json::Node& jPerms = root.get("permissions")) {
        if (!jPerms.isArray()) {
            getStdErr().format("permissions must be an array in: {}\n", settingsPath);
            return false;
        }
        // Iterate over permissions.
        for (const json::Node& jPerm : jPerms.arrayView()) {
            if (!jPerm.isObject()) {
                getStdErr().format("Each permission entry must be an object in: {}\n", settingsPath);
                return false;
            }

            // Import path.
            const auto& jPath = jPerm.get("path");
            if (!jPath) {
                getStdErr().format("Each permission entry must have a 'path'.\n");
                return false;
            }
            if (!jPath.isText()) {
                getStdErr().format("path must be a string in: {}\n", settingsPath);
                return false;
            }
            if (!jPath.text()) {
                getStdErr().format("path must be a non-empty string in: {}\n", settingsPath);
                return false;
            }
            // paths are relative to the current settings file's working directory.
            String absPath = makeAbsolutePath(joinPath(workingDir, jPath.text()));

            // Import tools.
            const json::Node& jTools = jPerm.get("tools");
            if (!jTools) {
                getStdErr().format("Each permission entry must have 'tools'.\n");
                return false;
            }
            if (!jTools.isArray()) {
                getStdErr().format("tools must be an array in: {}\n", settingsPath);
                return false;
            }
            for (const json::Node& t : jTools.arrayView()) {
                if (!t.isText()) {
                    getStdErr().format("Each tool entry must be a string in: {}\n", settingsPath);
                    return false;
                }
                StringView toolName = t.text();

                // Register the tool on first encounter.
                Owned<ToolSet::Handler>* found = agentSettings.toolSet.handlers.find(toolName);
                if (!found) {
                    if (toolName == "read") {
                        addReadTool(&agentSettings.toolSet);
                    } else if (toolName == "write") {
                        addWriteTool(&agentSettings.toolSet);
                    } else if (toolName == "list_dir") {
                        addListDirTool(&agentSettings.toolSet);
                    } else if (toolName == "find_in_files") {
                        addFindInFilesTool(&agentSettings.toolSet);
                    } else if (toolName == "edit") {
                        addEditTool(&agentSettings.toolSet);
                    } else {
                        getStdErr().format("Unknown tool in configuration: {}\n", toolName);
                        return false;
                    }
                    found = agentSettings.toolSet.handlers.find(toolName);
                    PLY_ASSERT(found);
                }

                // Grant this directory to the tool handler.
                ToolSet::Permission& granted = found->get()->permissions.append();
                granted.absPath = absPath;
            }
        }
    }

    return true;
}

// Load settings from the appropriate JSON files and convert them to Agent::Settings.
static bool loadSettings() {
    // Set defaults.
    agentSettings.toolSet.workingDirectory = FileSystem::getWorkingDirectory();

    // Find settings file.
    String settingsPath;
    if (options.settingsPath) {
        // Use the settings file specified on the command line, if any.
        settingsPath = makeAbsolutePath(options.settingsPath);
    } else {
        // Otherwise, search the working directory and each of its ancestors up to the file system root.
        String searchDir = FileSystem::getWorkingDirectory();
        while (true) {
            settingsPath = joinPath(searchDir, "agent.json");
            if (FileSystem::exists(settingsPath) == ExistsResult::File)
                break;

            String parentDir = splitPath(searchDir).directory;
            if (parentDir == searchDir) {
                getStdErr().write("Could not find agent.json.\n");
                return false;
            }
            searchDir = std::move(parentDir);
        }
    }

    // Load and merge the complete settings chain.
    Array<String> includedPaths;
    if (!loadSettingsWithIncludes(settingsPath, includedPaths))
        return false;
    if (!options.userPrompt) {
        options.userPrompt = tempUserPrompt;
    }

    // Augment the system prompt.
    agentSettings.toolSet.systemPrompt +=
        String::format("\nThe current working directory is: {}\n", agentSettings.toolSet.workingDirectory);

    return true;
}

// Overrides the loaded endpoint settings with sensible defaults for the provider
// specified on the command line, if any.
static bool applyProviderOverride() {
    if (!options.provider)
        return true;

    // Set endpoint defaults for the selected provider.
    if (options.provider == "openai") {
        agentSettings.endPoint.url = "https://api.openai.com/v1/responses";
        agentSettings.endPoint.apiKeyEnv = "OPENAI_API_KEY";
        agentSettings.endPoint.protocol = Protocol::Responses;
        agentSettings.endPoint.model = "gpt-5.6-luna";
    } else if (options.provider == "anthropic") {
        agentSettings.endPoint.url = "https://api.anthropic.com/v1/messages";
        agentSettings.endPoint.apiKeyEnv = "ANTHROPIC_API_KEY";
        agentSettings.endPoint.protocol = Protocol::Anthropic;
        agentSettings.endPoint.model = "claude-haiku-4.5";
    } else if (options.provider == "ollama-cloud") {
        agentSettings.endPoint.url = "https://ollama.com/v1/chat/completions";
        agentSettings.endPoint.apiKeyEnv = "OLLAMA_API_KEY";
        agentSettings.endPoint.protocol = Protocol::Completions;
        agentSettings.endPoint.model = "deepseek-v4-flash";
    } else if (options.provider == "dwarfstar") {
        agentSettings.endPoint.url = "http://127.0.0.1:8000/v1/chat/completions";
        agentSettings.endPoint.apiKeyEnv = "NONE";
        agentSettings.endPoint.protocol = Protocol::Completions;
        agentSettings.endPoint.model = "deepseek-v4-flash";
    } else {
        getStdErr().format("Unknown provider: {}\n", options.provider);
        return false;
    }
    return true;
}

// Prints the command-line syntax and registered options.
static void printUsage(Stream& out, StringView executablePath, const CommandLineParser& parser) {
    out.format("Usage: {} [options] [prompt]\n", executablePath);
    parser.printAvailableOptions(out);
}

//  ▄▄   ▄▄        ▄▄
//  ███▄███  ▄▄▄▄  ▄▄ ▄▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██ ██  ██
//  ██   ██ ▀█▄▄██ ██ ██  ██
//

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    // Configure terminal window for UTF-8 output.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Parse command line options.
    CommandLineParser parser({
        {"-c", "--config", PLY_LOOKUP_MEMBER(CommandLineOptions, settingsPath), "Path to JSON settings file"},
        {"-p", "--provider", PLY_LOOKUP_MEMBER(CommandLineOptions, provider), "Select a preset inference provider"},
        {"-m", "--model", PLY_LOOKUP_MEMBER(CommandLineOptions, model), "Model name to use"},
        {"-l", "--http-log", PLY_LOOKUP_MEMBER(CommandLineOptions, enableHttpLog), "Write raw HTTP log"},
        {"-s", "--serve", PLY_LOOKUP_MEMBER(CommandLineOptions, runWebServer), "Create a webserver on port 8081"},
        {"-h", "--help", PLY_LOOKUP_MEMBER(CommandLineOptions, printUsage), "Print this help"},
    });
    u32 numUserPrompts = 0;
    parser.defaultHandler = [&](ArrayView<const char*> args, u32 index) {
        options.userPrompt = args[index];
        numUserPrompts++;
    };
    if (!parser.apply(argc, argv, &options)) {
        // Error during parsing.
        Stream err = getStdErr();
        err.write("\n");
        printUsage(err, argv[0], parser);
        return 1;
    }
    if (options.printUsage) {
        Stream out = getStdOut();
        printUsage(out, argv[0], parser);
        return 0;
    }
    if (numUserPrompts > 1) {
        getStdErr().write("Only one prompt may be specified on the command line.\n");
        return 1;
    }

    // Load agent settings.
    if (!loadSettings())
        return 1;

    // Apply command-line overrides. -m/--model takes precedence over both the
    // settings file and the model default of -p/--provider.
    if (!applyProviderOverride())
        return 1;
    if (options.model) {
        agentSettings.endPoint.model = options.model;
    }

    // Ensure a prompt was specified on the command line or in the JSON settings.
    if (!options.userPrompt) {
        getStdErr().write("A prompt must be specified.\n");
        return 1;
    }

    // Initialize curl.
    CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    PLY_ASSERT(rc == CURLE_OK);
    PLY_UNUSED(rc);

    // Start the webserver in parallel with the agent.
    Thread webServerThread;
    if (options.runWebServer) {
        Network::initialize(IPv4);
        webServerThread.run([] { HTTPServer::run(8081, serveWebTranscript); });
    }

    // Create a transcript with the user's prompt as the first turn.
    transcript = new Transcript;
    Transcript::Turn& turn = transcript->turns.append();
    {
        Owned<Transcript::Message> userMsg = Heap::create<Transcript::Message>();
        userMsg->role = Transcript::Role::User;
        userMsg->content.append(options.userPrompt);
        userMsg->content.flush();
        turn.messages.append(std::move(userMsg));
    }

    // Print the initial portion of the transcript (system prompt, tool definitions and user prompt).
    TranscriptPrinter printer;
    printer.printStartup(options.userPrompt);

    // Start the agent.
    agentSettings.initialTranscript = transcript;
    agentSettings.enableHttpLog = options.enableHttpLog;
    Owned<Agent> agent = Heap::create<Agent>(agentSettings);

    // Process streamed events until the agent stops working.
    Owned<TranscriptUpdater> updater = createTranscriptUpdater(transcript);
    while (agent->isWorking()) {
        for (const TranscriptEvent& event : agent->waitForEvents()) {
            applyTranscriptEvent(updater, event);
            printer.handleEvent(event);
        }
    }

    // Close any section that was left open (e.g. the agent's final response) and
    // flush any tool responses that never received an EndTurn.
    printer.finish(getUnixTimestamp());
    if (options.runWebServer) {
        webTranscript.complete();
    }

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
PLY_STRUCT_MEMBER(provider)
PLY_STRUCT_MEMBER(model)
PLY_STRUCT_MEMBER(enableHttpLog)
PLY_STRUCT_MEMBER(runWebServer)
PLY_STRUCT_MEMBER(printUsage)
PLY_STRUCT_END()
