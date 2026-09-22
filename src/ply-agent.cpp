/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Agent Harness                                         │
│               Documentation: docs/high-level/agent-harness.md       │
│                                                                     │
└────────────────────────────────────────────────────────────────────*/

#include "ply-agent.h"
#include "ply-network.h"
#include "ply-json.h"

namespace ply {

//  ▄▄▄▄▄▄                                          ▄▄         ▄▄
//    ██   ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄  ▄██▄▄
//    ██   ██  ▀▀  ▄▄▄██ ██  ██ ▀█▄▄▄  ██    ██  ▀▀ ██ ██  ██  ██
//    ██   ██     ▀█▄▄██ ██  ██  ▄▄▄█▀ ▀█▄▄▄ ██     ██ ██▄▄█▀  ▀█▄▄
//                                                     ██

// Appends a streamed chunk while retaining each completed line as a separate string.
void Transcript::Buffer::append(StringView text) {
    while (text) {
        s32 newLinePos = text.find('\n');
        if (newLinePos < 0) {
            this->tail.write(text);
            break;
        }

        // Complete the current line, retaining the newline in its own line string.
        this->tail.write(text.left(newLinePos + 1));
        this->lines.append(this->tail.moveToString());
        this->tail = MemStream{TailChunkSize};
        text = text.substr(newLinePos + 1);
    }
}

// Finalizes the incomplete last line without combining it with any completed lines.
void Transcript::Buffer::flush() {
    if (this->tail.getSeekPos() > 0) {
        this->lines.append(this->tail.moveToString());
        this->tail = MemStream{TailChunkSize};
    }
}

// Returns the complete Buffer contents as a String.
String Transcript::Buffer::toString() const {
    // Copy the tail (often empty), then allocate the exact combined size.
    String tailText = this->tail.copyToString();
    u64 numBytes = tailText.numBytes();
    for (const String& line : this->lines) {
        numBytes += line.numBytes();
    }
    String result = String::allocate(numericCast<u32>(numBytes));

    // Copy each stored fragment directly into its final location.
    char* dst = result.bytes();
    for (const String& line : this->lines) {
        memcpy(dst, line.bytes(), line.numBytes());
        dst += line.numBytes();
    }
    if (tailText) {
        memcpy(dst, tailText.bytes(), tailText.numBytes());
    }
    return result;
}

// Returns the indexed tool call from a turn and asserts if the event stream is invalid.
static Transcript::Message* getToolCall(Transcript::Turn& turn, u32 toolCallID) {
    PLY_ASSERT(toolCallID > 0);
    u32 id = 0;
    for (Owned<Transcript::Message>& msg : turn.messages) {
        if (msg->role == Transcript::Role::ToolCall && ++id == toolCallID)
            return msg;
    }
    PLY_ASSERT(0);
    return nullptr;
}

// Applies a streamed event directly to the supplied transcript.
void applyTranscriptEvent(Transcript* transcript, const Transcript::Event& event) {
    switch (event.operation) {
        case Transcript::Event::BeginTurn: {
            transcript->turns.append();
            break;
        }
        case Transcript::Event::BeginMessage: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            if (turn.messages) {
                turn.messages.back()->content.flush();
            }
            Owned<Transcript::Message> msg = Heap::create<Transcript::Message>();
            msg->timeStamp = (u64) getUnixTimestamp();
            msg->role = event.role;
            msg->providerToolCallID = event.providerToolCallID;
            turn.messages.append(std::move(msg));
            break;
        }
        case Transcript::Event::AppendText: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            PLY_ASSERT(!turn.messages.isEmpty());
            turn.messages.back()->content.append(event.text);
            break;
        }
        case Transcript::Event::AppendToolResponse: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            getToolCall(turn, event.toolCallID)->toolResponse.append(event.text);
            break;
        }
        case Transcript::Event::EndToolResponse: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            Transcript::Message* toolCall = getToolCall(turn, event.toolCallID);
            toolCall->toolResponse.flush();
            toolCall->toolEnded = true;
            break;
        }
        case Transcript::Event::AppendProviderOutputItem: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            transcript->turns.back().providerOutputItems.append(event.text);
            break;
        }
        case Transcript::Event::SetTokenUsage: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            transcript->turns.back().tokenUsage = event.tokenUsage;
            break;
        }
        case Transcript::Event::EndTurn: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            if (turn.messages) {
                turn.messages.back()->content.flush();
            }
            break;
        }
        default:
            break;
    }
}

PLY_STRUCT_BEGIN(Transcript::Buffer)
PLY_STRUCT_MEMBER(lines)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript::Message)
PLY_STRUCT_MEMBER(timeStamp)
PLY_STRUCT_MEMBER(content)
PLY_STRUCT_MEMBER(providerToolCallID)
PLY_STRUCT_MEMBER(toolResponse)
PLY_STRUCT_MEMBER(toolEnded)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript::TokenUsage)
PLY_STRUCT_MEMBER(isValid)
PLY_STRUCT_MEMBER(uncachedInputTokens)
PLY_STRUCT_MEMBER(cachedInputTokens)
PLY_STRUCT_MEMBER(outputTokens)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript::Turn)
PLY_STRUCT_MEMBER(messages)
PLY_STRUCT_MEMBER(providerOutputItems)
PLY_STRUCT_MEMBER(tokenUsage)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript)
PLY_STRUCT_MEMBER(turns)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript::Event)
PLY_STRUCT_MEMBER(timeStamp)
PLY_STRUCT_MEMBER(toolCallID)
PLY_STRUCT_MEMBER(providerToolCallID)
PLY_STRUCT_MEMBER(text)
PLY_STRUCT_MEMBER(tokenUsage)
PLY_STRUCT_END()

#if !PLY_AGENT_TRANSCRIPT_ONLY

// ProtocolHandler owns protocol-specific request construction and stream parsing state.
struct ProtocolHandler {
    Agent::Impl* const impl;

    explicit ProtocolHandler(Agent::Impl* impl) : impl{impl} {
    }
    virtual ~ProtocolHandler() = default;
    virtual String makeRequestBody() = 0;
    virtual void receiveLine(StringView line) = 0;
};

//   ▄▄▄▄                        ▄▄         ▄▄▄▄                 ▄▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄        ██  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██   ▀▀ ▀▀  ██  ██ ██ ██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄ ▄▄ ▄▄ ▄██▄ ██ ██ ██ ██▄▄█▀ ▄██▄
//          ▄▄▄█▀                                         ██

struct ToolContextImpl : ToolContext {
    // The mutex serializes cancellation callbacks, transcript changes and event buffering across the agent threads.
    Mutex mutex;
    Atomic<bool> canceled = false;
    Functor<void()> cancelCallback;
    Agent::Impl* agentImpl = nullptr;
    StringView workingDir;
};

//--------------------------------------------------------
// Agent::Impl contains information shared between the main thread and an inference thread.
// The inference thread receives response data from curl, parses each line of incoming JSONL
// and generates transcript events.
//
// If tools are enabled, an additional background thread is spawned to perform the
// tool requests. Tool requests are enqueued immediately as soon as they're received.
//--------------------------------------------------------
struct Agent::Impl : RefCounted<Agent::Impl> {
    Thread inferenceThread;
    Thread toolThread;

    // Settings moved into the Agent when it's constructed.
    Agent::Settings settings;

    // internalTranscript is a copy of settings.startTranscript, but links to the same parent.
    // Modified internally, but only when toolCtx.mutex is held.
    Reference<Transcript> internalTranscript;
    // Tracks the role of the last message begun in the current turn, so the inference
    // thread only emits a BeginMessage event when the role changes. Reset to None at
    // the start of each turn. Protected by toolCtx.mutex.
    Transcript::Role currentRole = Transcript::Role::None;

    //----------------------------------------------
    // These members are only used by the inference thread.
    // It's a convenient place for the HTTPClient response callback to access them.
    //----------------------------------------------
    Stream rawLogFile;
    MemStream lineInProgress;
    Owned<HTTPClient> httpClient; // Only used by the inference thread.
    Owned<ProtocolHandler> protocolHandler;
    bool anyToolCallsThisTurn = false;

    //----------------------------------------------
    // These members are shared between all threads.
    // ToolContext is a logical grouping of the variables used by tool handlers.
    // It's mainly a way to hide the rest of the agent implementation details from tool handlers.
    // Internally, toolCtx.mutex is also used to protect access to the other members here.
    //----------------------------------------------
    // toolCtx.mutex also protects access to the remaining members below.
    ToolContextImpl toolCtx;
    // The inference thread adds tool calls to the end of pendingToolCalls.
    // The tool thread pops each tool call from the front after it's completed.
    Array<Transcript::Message*> pendingToolCalls;
    // Events are buffered here until the client thread consumes them.
    Array<Transcript::Event> pendingEvents;
    // The inference thread only sets inferenceEnded to true when the LLM completes a turn
    // without issuing any new tool requests.
    bool inferenceEnded = false;
    bool toolEnded = false;
    // Condition variables used to wake each thread after various events.
    ConditionVariable inferenceCondVar;
    ConditionVariable toolCondVar;
    ConditionVariable clientCondVar;
    ConditionVariable completionCondVar;
};

// Must be called with toolCtx.mutex held. Timestamps the event, appends it to the
// pendingEvents buffer and wakes any client thread blocked in waitForEvents. It does
// NOT wake waitForCompletion, which is only released by when the agent stops.
static void bufferEvent(Agent::Impl* impl, Transcript::Event&& event) {
    event.timeStamp = getUnixTimestamp();
    impl->pendingEvents.append(std::move(event));
    impl->clientCondVar.wakeAll();
}

// Must be called with toolCtx.mutex held. Applies the event to the current transcript
// section (via applyTranscriptEvent) and then buffers it for delivery to the client
// thread.
static void addEvent(Agent::Impl* impl, Transcript::Event&& event) {
    applyTranscriptEvent(impl->internalTranscript, event);
    bufferEvent(impl, std::move(event));
}

// Starts a new turn for an inference request.
// Must be called with toolCtx.mutex held.
static void beginTurn(Agent::Impl* impl) {
    Transcript::Event event;
    event.operation = Transcript::Event::BeginTurn;
    addEvent(impl, std::move(event));
}

// Finalizes the current turn after an inference request completes.
// Must be called with toolCtx.mutex held.
static void endTurn(Agent::Impl* impl) {
    Transcript::Event event;
    event.operation = Transcript::Event::EndTurn;
    addEvent(impl, std::move(event));
}

// Emits a BeginMessage event for the given role. If the role is ToolCall, toolCallID
// identifies the tool call (1-based, sequential within the current turn).
// Must be called with toolCtx.mutex held.
static void beginMessage(Agent::Impl* impl, Transcript::Role role, u32 toolCallID = 0,
                         StringView providerToolCallID = {}) {
    Transcript::Event event;
    event.operation = Transcript::Event::BeginMessage;
    event.role = role;
    event.toolCallID = toolCallID;
    event.providerToolCallID = providerToolCallID;
    addEvent(impl, std::move(event));
}

// Emits an AppendText event that appends to the last message in the current turn.
// Must be called with toolCtx.mutex held.
static void appendText(Agent::Impl* impl, String&& text) {
    Transcript::Event event;
    event.operation = Transcript::Event::AppendText;
    event.text = std::move(text);
    addEvent(impl, std::move(event));
}

// Emits a BeginMessage (if the role changed since the last message begun in this turn)
// followed by an AppendText. Used by the inference thread to stream reasoning/content.
// Must be called with toolCtx.mutex held.
static void emitText(Agent::Impl* impl, Transcript::Role role, StringView text) {
    if (impl->currentRole != role) {
        beginMessage(impl, role);
        impl->currentRole = role;
    }
    appendText(impl, String{text});
}

// Updates the aggregate token usage for the current inference request.
// Must be called with toolCtx.mutex held.
static void setTokenUsage(Agent::Impl* impl, const Transcript::TokenUsage& tokenUsage) {
    Transcript::Event event;
    event.operation = Transcript::Event::SetTokenUsage;
    event.tokenUsage = tokenUsage;
    addEvent(impl, std::move(event));
}

// Copies a token field when the provider included it in a usage object.
static void copyTokenCount(u64& dst, const json::Node& src) {
    if (src.isNumber()) {
        dst = numericCast<u64>(src.getNumber());
    }
}

// Copies uncached input tokens, which the provider reports either as total input minus cached or directly.
static void copyUncachedInputCount(u64& dst, const json::Node& totalSrc, u64 cachedInputTokens) {
    u64 totalInputTokens = 0;
    copyTokenCount(totalInputTokens, totalSrc);
    dst = totalInputTokens > cachedInputTokens ? totalInputTokens - cachedInputTokens : 0;
}

// Counts the ToolCall-role messages in the current turn, up to and including the one
// pointed to by toolCall (1-based). Returns 0 if toolCall is not found.
// Must be called with toolCtx.mutex held.
static u32 toolCallIDForMessage(Agent::Impl* impl, Transcript::Message* toolCall) {
    PLY_ASSERT(!impl->internalTranscript->turns.isEmpty());
    const Transcript::Turn& turn = impl->internalTranscript->turns.back();
    u32 id = 0;
    for (const Owned<Transcript::Message>& msg : turn.messages) {
        if (msg->role == Transcript::Role::ToolCall)
            id++;
        if (msg.get() == toolCall) {
            PLY_ASSERT(msg->role == Transcript::Role::ToolCall);
            return id;
        }
    }
    PLY_ASSERT(0);
    return 0;
}

// Parses a ToolCall message's content, which encodes the tool name immediately
// followed by a JSON object of arguments (e.g. read{"path":"sample.txt"}). name is set
// to the substring before the first '{'. argsOut receives the parsed JSON object.
// Returns false if no JSON object could be parsed (name still holds the prefix).
static bool parseToolCallText(const Transcript::Buffer& content, StringView& name, json::ParseResult& argsOut) {
    PLY_ASSERT(content.lines.numItems() <= 1);
    StringView text;
    if (content.lines) {
        text = content.lines[0];
    }
    s32 brace = text.find('{');
    if (brace < 0) {
        name = text;
        return false;
    }
    name = text.left(brace);
    Owned<json::Parser> parser = json::Parser::create();
    parser->setErrorCallback([](const json::ParseError&) {});
    parser->setGreedy(false);
    argsOut = parser->parse({}, text.substr(brace));
    return argsOut.root.isObject();
}

//   ▄▄▄▄                         ▄▄▄          ▄▄   ▄▄                           ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██   ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄      ██  ██ ██  ██  ██
//  ██     ██  ██ ██ ██ ██ ██  ██  ██  ██▄▄██  ██   ██ ██  ██ ██  ██ ▀█▄▄▄      ██▀▀██ ██▀▀▀   ██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██ ██ ██ ██▄▄█▀ ▄██▄ ▀█▄▄▄   ▀█▄▄ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀     ██  ██ ██     ▄██▄
//                         ██

struct CompletionsProtocolHandler : ProtocolHandler {
    using ProtocolHandler::ProtocolHandler;
    virtual String makeRequestBody() override;
    virtual void receiveLine(StringView line) override;
};

String CompletionsProtocolHandler::makeRequestBody() {
    Agent::Impl* impl = this->impl;
    json::Node root{json::Node::Object{}};

    // model
    root.set("model", json::Node::Text{impl->settings.endPoint.model});

    // tool definitions
    if (impl->settings.capabilities.tools.items()) {
        json::Node jTools{json::Node::Array{}};
        for (const Owned<ToolDefinition>& tool : impl->settings.capabilities.tools) {
            json::Node& jTool = jTools.array().append(json::Node::Object{});
            jTool.set("type", json::Node::Text{"function"});
            json::Node jFunc{json::Node::Object{}};
            jFunc.set("name", json::Node::Text{tool->name});
            jFunc.set("description", json::Node::Text{tool->description});

            // tool parameters
            json::Node jParams{json::Node::Object{}};
            jParams.set("type", json::Node::Text{"object"});
            json::Node jRequired{json::Node::Array{}};
            json::Node jProps{json::Node::Object{}};
            for (const ToolDefinition::Parameter& param : tool->parameters) {
                json::Node jParam{json::Node::Object{}};
                jParam.set("description", json::Node::Text{param.description});
                jParam.set("type", json::Node::Text{param.type});
                jProps.set(param.name, std::move(jParam));
                if (param.required) {
                    jRequired.array().append(json::Node::Text{param.name});
                }
            }
            jParams.set("required", std::move(jRequired));
            jParams.set("properties", std::move(jProps));
            jFunc.set("parameters", std::move(jParams));
            jFunc.set("strict", json::Node::Bool{false});
            jTool.set("function", std::move(jFunc));
        }
        root.set("tools", std::move(jTools));
    }

    // messages
    json::Node jMessages{json::Node::Array{}};
    if (impl->settings.capabilities.systemPrompt) {
        json::Node jMsg{json::Node::Object{}};
        jMsg.set("role", json::Node::Text{"developer"});
        jMsg.set("content", json::Node::Text{impl->settings.capabilities.systemPrompt});
        jMessages.array().append(jMsg);
    }

    // Flatten chain of parents into an array.
    Array<const Transcript*> flattened;
    for (const Transcript* transcript = impl->internalTranscript; transcript; transcript = transcript->parent) {
        flattened.append(transcript);
    }

    // Iterate over flattened array from root to leaf.
    for (s32 i = flattened.numItems() - 1; i >= 0; i--) {
        for (const Transcript::Turn& turn : flattened[i]->turns) {
            // Conversational messages and tool requests.
            const Transcript::Message* prevMsg = nullptr;
            u32 toolCallID = 0; // 1-based, sequential within this turn
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role == Transcript::Role::User) {
                    // User message
                    json::Node jMsg{json::Node::Object{}};
                    jMsg.set("role", json::Node::Text{"user"});
                    json::Node jContent{json::Node::Object{}};
                    jContent.set("type", json::Node::Text{"text"});
                    jContent.set("text", json::Node::Text{msg->content.toString()});
                    json::Node jArray{json::Node::Array{}};
                    jArray.array().append(std::move(jContent));
                    jMsg.set("content", std::move(jArray));
                    jMessages.array().append(std::move(jMsg));
                    // A user message never participates in assistant-side grouping.
                    prevMsg = nullptr;
                } else if (msg->role == Transcript::Role::AgentThinking) {
                    // Reasoning message
                    json::Node jMsg{json::Node::Object{}};
                    jMsg.set("role", json::Node::Text{"assistant"});
                    jMsg.set("reasoning", json::Node::Text{msg->content.toString()});
                    jMessages.array().append(std::move(jMsg));
                    prevMsg = msg;
                } else if (msg->role == Transcript::Role::Agent) {
                    // Content message
                    if (prevMsg && prevMsg->role == Transcript::Role::AgentThinking) {
                        // Merge with previous reasoning message
                        jMessages.array().back().set("content", json::Node::Text{msg->content.toString()});
                    } else {
                        json::Node jMsg{json::Node::Object{}};
                        jMsg.set("role", json::Node::Text{"assistant"});
                        jMsg.set("content", json::Node::Text{msg->content.toString()});
                        jMessages.array().append(std::move(jMsg));
                    }
                    prevMsg = msg;
                } else if (msg->role == Transcript::Role::ToolCall) {
                    // Tool call. Parse the name and arguments out of the message text.
                    toolCallID++;
                    StringView tcName;
                    json::ParseResult parsedArgs;
                    parseToolCallText(msg->content, tcName, parsedArgs);

                    // Ensure we have a "tool_calls" JSON array in which to put the object.
                    if (!prevMsg) {
                        json::Node jMsg{json::Node::Object{}};
                        jMsg.set("role", json::Node::Text{"assistant"});
                        jMsg.set("tool_calls", json::Node::Array{});
                        jMessages.array().append(std::move(jMsg));
                    } else if (prevMsg->role != Transcript::Role::ToolCall) {
                        jMessages.array().back().set("tool_calls", json::Node::Array{});
                    }
                    json::Node& jToolCalls = jMessages.array().back().get("tool_calls");
                    PLY_ASSERT(jToolCalls.isArray());

                    // Build the tool call JSON object.
                    json::Node jToolCall{json::Node::Object{}};
                    jToolCall.set("id", json::Node::Text{String::format("{}", toolCallID)});
                    jToolCall.set("type", json::Node::Text{"function"});
                    json::Node jFunction{json::Node::Object{}};
                    jFunction.set("name", json::Node::Text{tcName});

                    // Stringify the arguments for Ollama Cloud).
                    json::Node jArgs{json::Node::Object{}};
                    if (parsedArgs.root.isObject()) {
                        for (const auto& item : parsedArgs.root.object().items) {
                            jArgs.set(item.key, json::Node{item.value});
                        }
                    }
                    json::WriteOptions options;
                    options.includeWhitespace = false;
                    String stringified = json::toString(jArgs, options);
                    jFunction.set("arguments", json::Node::Text{stringified});

                    // Add tool call to JSON array.
                    jToolCall.set("function", std::move(jFunction));
                    jToolCalls.array().append(std::move(jToolCall));
                    prevMsg = msg;
                }
            }

            // Tool responses
            u32 responseToolCallID = 0; // 1-based, sequential within this turn
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role == Transcript::Role::ToolCall) {
                    responseToolCallID++;
                    json::Node jMsg{json::Node::Object{}};
                    jMsg.set("role", json::Node::Text{"tool"});
                    jMsg.set("content", json::Node::Text{msg->toolResponse.toString()});
                    jMsg.set("tool_call_id", json::Node::Text{String::format("{}", responseToolCallID)});
                    jMessages.array().append(std::move(jMsg));
                }
            }
        }
    }
    root.set("messages", std::move(jMessages));

    // stream
    root.set("stream", json::Node::Bool{true});

    // Request the final streaming chunk that contains aggregate token usage.
    {
        json::Node jStreamOptions{json::Node::Object{}};
        jStreamOptions.set("include_usage", json::Node::Bool{true});
        root.set("stream_options", std::move(jStreamOptions));
    }

    // store
    root.set("store", json::Node::Bool{false});

    // reasoning_effort
    root.set("reasoning_effort", json::Node::Text{"medium"});

    // Convert to string
    json::WriteOptions options;
    options.includeWhitespace = false;
    return json::toString(root, options);
}

// Handle Completions API response line.
void CompletionsProtocolHandler::receiveLine(StringView line) {
    Agent::Impl* impl = this->impl;
    if (line.startsWith("data: ")) {
        line = line.substr(6);
    }

    // Parse json message
    Owned<json::Parser> parser = json::Parser::create();
    parser->setErrorCallback([](const json::ParseError&) {});
    parser->setGreedy(false);
    json::ParseResult result = parser->parse({}, line);

    const json::Node& jChoices = result.root.get("choices");
    for (const json::Node& jChoice : jChoices.arrayView()) {
        const json::Node& jDelta = jChoice.get("delta");
        if (!jDelta.isValid())
            continue;
        // Some providers identify the role on the first delta only.
        StringView role = jDelta.get("role").text();
        if (role && role != "assistant")
            continue;
        const json::Node& jReasoning = jDelta.get("reasoning");
        if (jReasoning.text()) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            emitText(impl, Transcript::Role::AgentThinking, jReasoning.text());
        }
        const json::Node& jContent = jDelta.get("content");
        if (jContent.text()) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            emitText(impl, Transcript::Role::Agent, jContent.text());
        }
        // "tool_calls":[{"id":"call_ta2rz3e6","index":0,"type":"function",
        // "function":{"name":"read","arguments":"{\"path\":\"sample.txt\"}"}}]},"finish_reason":null}]}
        const json::Node& jToolCalls = jDelta.get("tool_calls");
        for (const json::Node& jCall : jToolCalls.arrayView()) {
            // Hold the transcript mutex for the whole mutation + event emission
            // so it serializes with the tool thread's event buffering calls, and so we
            // don't touch the transcript after the client destroyed the Agent.
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;

            const json::Node& jFunc = jCall.get("function");
            String tcName = jFunc.get("name").text();

            // Handle arguments.
            // When using the OpenAI Chat Completions API, it's a JSON string.
            // When using Ollama's /api/chat endpoint, it's a JSON object.
            const json::Node& jArgs = jFunc.get("arguments");
            json::Node jArgsObj{json::Node::Object{}};
            if (jArgs.isObject()) {
                for (const auto& item : jArgs.object().items) {
                    jArgsObj.set(item.key, json::Node{item.value});
                }
            } else if (jArgs.isText()) {
                Owned<json::Parser> argParser = json::Parser::create();
                argParser->setErrorCallback([](const json::ParseError&) {});
                argParser->setGreedy(false);
                json::ParseResult parsedArgs = argParser->parse({}, jArgs.text());
                if (parsedArgs.root.isObject()) {
                    for (const auto& item : parsedArgs.root.object().items) {
                        jArgsObj.set(item.key, json::Node{item.value});
                    }
                }
            }

            // Stringify the arguments so the tool call's text encodes
            // `<name><json-object>` (e.g. read{"path":"sample.txt"}).
            json::WriteOptions writeOpts;
            writeOpts.includeWhitespace = false;
            String stringified = json::toString(jArgsObj, writeOpts);

            // Assign a sequential 1-based toolCallID within the current turn.
            u32 toolCallID = 0;
            for (const Owned<Transcript::Message>& msg : impl->internalTranscript->turns.back().messages) {
                if (msg->role == Transcript::Role::ToolCall)
                    toolCallID++;
            }
            toolCallID++;

            // Emit BeginMessage + AppendText so the tool call (name + arguments)
            // is recorded in both the internal transcript and the client's copy.
            beginMessage(impl, Transcript::Role::ToolCall, toolCallID);
            appendText(impl, String::format("{}{}", tcName, stringified));

            // Grab a pointer to the newly created tool call message so the tool
            // thread can mutate its toolResponse/toolEnded directly.
            Transcript::Message* toolCallMsg = impl->internalTranscript->turns.back().messages.back().get();
            toolCallMsg->content.flush();
            impl->currentRole = Transcript::Role::ToolCall;

            // Hand this tool call off to the tool thread.
            impl->anyToolCallsThisTurn = true;
            impl->pendingToolCalls.append(toolCallMsg);
            impl->toolCondVar.wakeAll();
        }
    }

    // The final empty-choice chunk reports aggregate usage for this request.
    const json::Node& jUsage = result.root.get("usage");
    if (jUsage.isObject()) {
        Transcript::TokenUsage tokenUsage;
        tokenUsage.isValid = true;
        copyTokenCount(tokenUsage.cachedInputTokens, jUsage.get("prompt_tokens_details").get("cached_tokens"));
        copyUncachedInputCount(tokenUsage.uncachedInputTokens, jUsage.get("prompt_tokens"),
                               tokenUsage.cachedInputTokens);
        copyTokenCount(tokenUsage.outputTokens, jUsage.get("completion_tokens"));

        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        setTokenUsage(impl, tokenUsage);
    }
}

//  ▄▄▄▄▄                                                               ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄      ██  ██ ██  ██  ██
//  ██▀▀█▄ ██▄▄██ ▀█▄▄▄  ██  ██ ██  ██ ██  ██ ▀█▄▄▄  ██▄▄██ ▀█▄▄▄      ██▀▀██ ██▀▀▀   ██
//  ██  ██ ▀█▄▄▄   ▄▄▄█▀ ██▄▄█▀ ▀█▄▄█▀ ██  ██  ▄▄▄█▀ ▀█▄▄▄   ▄▄▄█▀     ██  ██ ██     ▄██▄
//                       ██

struct ResponsesProtocolHandler : ProtocolHandler {
    using ProtocolHandler::ProtocolHandler;
    virtual String makeRequestBody() override;
    virtual void receiveLine(StringView line) override;
};

String ResponsesProtocolHandler::makeRequestBody() {
    Agent::Impl* impl = this->impl;
    json::Node root{json::Node::Object{}};

    // model
    root.set("model", json::Node::Text{impl->settings.endPoint.model});

    // tool definitions
    if (impl->settings.capabilities.tools.items()) {
        json::Node jTools{json::Node::Array{}};
        for (const Owned<ToolDefinition>& tool : impl->settings.capabilities.tools) {
            json::Node& jTool = jTools.array().append(json::Node::Object{});
            jTool.set("type", json::Node::Text{"function"});
            jTool.set("name", json::Node::Text{tool->name});
            jTool.set("description", json::Node::Text{tool->description});

            // tool parameters
            json::Node jParams{json::Node::Object{}};
            jParams.set("type", json::Node::Text{"object"});
            json::Node jRequired{json::Node::Array{}};
            json::Node jProps{json::Node::Object{}};
            for (const ToolDefinition::Parameter& param : tool->parameters) {
                json::Node jParam{json::Node::Object{}};
                jParam.set("description", json::Node::Text{param.description});
                jParam.set("type", json::Node::Text{param.type});
                jProps.set(param.name, std::move(jParam));
                if (param.required) {
                    jRequired.array().append(json::Node::Text{param.name});
                }
            }
            jParams.set("required", std::move(jRequired));
            jParams.set("properties", std::move(jProps));
            jTool.set("parameters", std::move(jParams));
            jTool.set("strict", json::Node::Bool{false});
        }
        root.set("tools", std::move(jTools));
    }

    // messages
    json::Node jInput{json::Node::Array{}};
    if (impl->settings.capabilities.systemPrompt) {
        json::Node jMsg{json::Node::Object{}};
        jMsg.set("role", json::Node::Text{"developer"});
        jMsg.set("content", json::Node::Text{impl->settings.capabilities.systemPrompt});
        jInput.array().append(jMsg);
    }

    // Flatten the transcript chain so input items are emitted from root to leaf.
    Array<const Transcript*> flattened;
    for (const Transcript* transcript = impl->internalTranscript; transcript; transcript = transcript->parent) {
        flattened.append(transcript);
    }
    for (s32 i = flattened.numItems() - 1; i >= 0; i--) {
        for (u32 turnIndex = 0; turnIndex < flattened[i]->turns.numItems(); turnIndex++) {
            const Transcript::Turn& turn = flattened[i]->turns[turnIndex];
            u32 fallbackToolCallID = 0;
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role == Transcript::Role::User ||
                    (msg->role == Transcript::Role::Agent && turn.providerOutputItems.isEmpty())) {
                    // Convert conversational messages to Responses API input message items.
                    json::Node jMsg{json::Node::Object{}};
                    bool isUser = msg->role == Transcript::Role::User;
                    jMsg.set("type", json::Node::Text{"message"});
                    jMsg.set("role", json::Node::Text{isUser ? "user" : "assistant"});
                    jMsg.set("content", json::Node::Text{msg->content.toString()});
                    jInput.array().append(std::move(jMsg));
                } else if (msg->role == Transcript::Role::ToolCall && turn.providerOutputItems.isEmpty()) {
                    // Recreate the function call item and pair its completed output by call_id.
                    fallbackToolCallID++;
                    StringView tcName;
                    json::ParseResult parsedArgs;
                    parseToolCallText(msg->content, tcName, parsedArgs);
                    String arguments = parsedArgs.root.isObject() ? json::toString(parsedArgs.root, {false}) : "{}";
                    String callID = msg->providerToolCallID
                                        ? msg->providerToolCallID
                                        : String::format("call_{}_{}_{}", i, turnIndex, fallbackToolCallID);

                    json::Node jCall{json::Node::Object{}};
                    jCall.set("type", json::Node::Text{"function_call"});
                    jCall.set("call_id", json::Node::Text{callID});
                    jCall.set("name", json::Node::Text{tcName});
                    jCall.set("arguments", json::Node::Text{std::move(arguments)});
                    jInput.array().append(std::move(jCall));
                }
            }

            // Replay the endpoint's original output items, including encrypted reasoning context.
            for (StringView itemText : turn.providerOutputItems) {
                Owned<json::Parser> parser = json::Parser::create();
                parser->setErrorCallback([](const json::ParseError&) {});
                json::ParseResult item = parser->parse({}, itemText);
                if (item.root.isObject()) {
                    jInput.array().append(std::move(item.root));
                }
            }

            // Append tool outputs after all function calls from the turn.
            fallbackToolCallID = 0;
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role != Transcript::Role::ToolCall)
                    continue;
                fallbackToolCallID++;
                if (!msg->toolEnded)
                    continue;
                String callID = msg->providerToolCallID
                                    ? msg->providerToolCallID
                                    : String::format("call_{}_{}_{}", i, turnIndex, fallbackToolCallID);
                json::Node jOutput{json::Node::Object{}};
                jOutput.set("type", json::Node::Text{"function_call_output"});
                jOutput.set("call_id", json::Node::Text{std::move(callID)});
                jOutput.set("output", json::Node::Text{msg->toolResponse.toString()});
                jInput.array().append(std::move(jOutput));
            }
        }
    }
    root.set("input", std::move(jInput));

    // stream
    root.set("stream", json::Node::Bool{true});

    // store
    root.set("store", json::Node::Bool{false});

    // Request replayable reasoning data for manually managed conversation history.
    json::Node jInclude{json::Node::Array{}};
    jInclude.array().append(json::Node::Text{"reasoning.encrypted_content"});
    root.set("include", std::move(jInclude));

    // reasoning
    {
        json::Node jReasoning{json::Node::Object{}};
        jReasoning.set("effort", json::Node::Text{"medium"});
        jReasoning.set("summary", json::Node::Text{"auto"});
        root.set("reasoning", std::move(jReasoning));
    }

    // Convert to string
    json::WriteOptions options;
    options.includeWhitespace = false;
    return json::toString(root, options);
}

void ResponsesProtocolHandler::receiveLine(StringView line) {
    Agent::Impl* impl = this->impl;
    // Parse Responses API data events; the event type is also present in each JSON object.
    if (line.startsWith("data: ")) {
        Owned<json::Parser> parser = json::Parser::create();
        parser->setErrorCallback([](const json::ParseError&) {});
        parser->setGreedy(false);
        json::ParseResult result = parser->parse({}, line.substr(6).trim());

        if (result.root.isObject()) {
            StringView eventType = result.root.get("type").text();
            if (eventType == "response.output_text.delta" || eventType == "response.refusal.delta" ||
                eventType == "response.reasoning_summary_text.delta") {
                // Stream visible output, refusals and reasoning summaries into transcript messages.
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.isCanceled())
                    return;
                Transcript::Role role = eventType == "response.reasoning_summary_text.delta"
                                            ? Transcript::Role::AgentThinking
                                            : Transcript::Role::Agent;
                emitText(impl, role, result.root.get("delta").text());
            } else if (eventType == "error" || eventType == "response.failed") {
                // Surface both transport-level stream errors and failed response objects.
                const json::Node& jError =
                    eventType == "error" ? result.root : result.root.get("response").get("error");
                StringView message = jError.get("message").text();
                if (!message) {
                    message = "Responses API request failed";
                }
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.isCanceled())
                    return;
                emitText(impl, Transcript::Role::Error, message);
            } else if (eventType == "response.incomplete") {
                // Surface the reason that the endpoint stopped before completing its response.
                StringView reason = result.root.get("response").get("incomplete_details").get("reason").text();
                String message = "Responses API request incomplete";
                if (reason) {
                    message += String::format(": {}", reason);
                }
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.isCanceled())
                    return;
                emitText(impl, Transcript::Role::Error, message);
            } else if (eventType == "response.output_item.done") {
                const json::Node& jItem = result.root.get("item");
                if (jItem.isObject()) {
                    // Preserve the complete output item for stateless conversation replay.
                    LockGuard<Mutex> guard{impl->toolCtx.mutex};
                    if (impl->toolCtx.isCanceled())
                        return;
                    Transcript::Event itemEvent;
                    itemEvent.operation = Transcript::Event::AppendProviderOutputItem;
                    itemEvent.text = json::toString(jItem, {false});
                    addEvent(impl, std::move(itemEvent));

                    if (jItem.get("type").text() != "function_call")
                        return;

                    // Queue the completed function call for the tool thread.
                    u32 toolCallID = 1;
                    for (const Owned<Transcript::Message>& msg : impl->internalTranscript->turns.back().messages) {
                        if (msg->role == Transcript::Role::ToolCall) {
                            toolCallID++;
                        }
                    }
                    beginMessage(impl, Transcript::Role::ToolCall, toolCallID, jItem.get("call_id").text());
                    appendText(impl, String::format("{}{}", jItem.get("name").text(), jItem.get("arguments").text()));
                    Transcript::Message* toolCallMsg = impl->internalTranscript->turns.back().messages.back().get();
                    toolCallMsg->content.flush();
                    impl->currentRole = Transcript::Role::ToolCall;
                    impl->anyToolCallsThisTurn = true;
                    impl->pendingToolCalls.append(toolCallMsg);
                    impl->toolCondVar.wakeAll();
                }
            } else if (eventType == "response.completed") {
                // The completed response contains aggregate usage for this request.
                const json::Node& jUsage = result.root.get("response").get("usage");
                if (!jUsage.isObject())
                    return;

                Transcript::TokenUsage tokenUsage;
                tokenUsage.isValid = true;
                copyTokenCount(tokenUsage.cachedInputTokens, jUsage.get("input_tokens_details").get("cached_tokens"));
                copyUncachedInputCount(tokenUsage.uncachedInputTokens, jUsage.get("input_tokens"),
                                       tokenUsage.cachedInputTokens);
                copyTokenCount(tokenUsage.outputTokens, jUsage.get("output_tokens"));

                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.isCanceled())
                    return;
                setTokenUsage(impl, tokenUsage);
            }
        }
    }
}

//   ▄▄▄▄          ▄▄   ▄▄                          ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄▄▄▄  ▄██▄▄ ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀██ ██  ██  ██   ██  ██ ██  ▀▀ ██  ██ ██  ██ ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██  ██ ██  ██  ▀█▄▄ ██  ██ ██     ▀█▄▄█▀ ██▄▄█▀ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//                                           ██

struct AnthropicProtocolHandler : ProtocolHandler {
    // Accumulate the active SSE content block until it is complete and replayable.
    s32 contentBlockIndex = -1;
    json::Node contentBlock;
    Transcript::Message* toolCall = nullptr;
    Transcript::TokenUsage tokenUsage;

    using ProtocolHandler::ProtocolHandler;
    virtual String makeRequestBody() override;
    virtual void receiveLine(StringView line) override;
};

String AnthropicProtocolHandler::makeRequestBody() {
    Agent::Impl* impl = this->impl;

    // Discard any incomplete block left by the preceding request.
    this->contentBlockIndex = -1;
    this->contentBlock = {};
    this->toolCall = nullptr;
    this->tokenUsage = {};

    json::Node root{json::Node::Object{}};
    root.set("model", json::Node::Text{impl->settings.endPoint.model});
    root.set("max_tokens", json::Node::Number{16384});
    root.set("stream", json::Node::Bool{true});
    if (impl->settings.capabilities.systemPrompt) {
        root.set("system", json::Node::Text{impl->settings.capabilities.systemPrompt});
    }

    // Describe tools using the Messages API's input_schema format.
    if (impl->settings.capabilities.tools.items()) {
        json::Node jTools{json::Node::Array{}};
        for (const Owned<ToolDefinition>& tool : impl->settings.capabilities.tools) {
            json::Node& jTool = jTools.array().append(json::Node::Object{});
            jTool.set("name", json::Node::Text{tool->name});
            jTool.set("description", json::Node::Text{tool->description});
            json::Node jSchema{json::Node::Object{}};
            jSchema.set("type", json::Node::Text{"object"});
            json::Node jRequired{json::Node::Array{}};
            json::Node jProperties{json::Node::Object{}};
            for (const ToolDefinition::Parameter& param : tool->parameters) {
                json::Node jParam{json::Node::Object{}};
                jParam.set("type", json::Node::Text{param.type});
                jParam.set("description", json::Node::Text{param.description});
                jProperties.set(param.name, std::move(jParam));
                if (param.required) {
                    jRequired.array().append(json::Node::Text{param.name});
                }
            }
            jSchema.set("properties", std::move(jProperties));
            jSchema.set("required", std::move(jRequired));
            jTool.set("input_schema", std::move(jSchema));
        }
        root.set("tools", std::move(jTools));
    }

    // Flatten the transcript and translate each turn into alternating Messages API messages.
    Array<const Transcript*> flattened;
    for (const Transcript* transcript = impl->internalTranscript; transcript; transcript = transcript->parent) {
        flattened.append(transcript);
    }
    json::Node jMessages{json::Node::Array{}};
    for (s32 i = flattened.numItems() - 1; i >= 0; i--) {
        for (u32 turnIndex = 0; turnIndex < flattened[i]->turns.numItems(); turnIndex++) {
            const Transcript::Turn& turn = flattened[i]->turns[turnIndex];
            json::Node jAssistantContent{json::Node::Array{}};
            json::Node jToolResults{json::Node::Array{}};
            u32 fallbackToolCallID = 0;
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role == Transcript::Role::User) {
                    json::Node& jMessage = jMessages.array().append(json::Node::Object{});
                    jMessage.set("role", json::Node::Text{"user"});
                    jMessage.set("content", json::Node::Text{msg->content.toString()});
                } else if (msg->role == Transcript::Role::Agent && turn.providerOutputItems.isEmpty()) {
                    json::Node& jText = jAssistantContent.array().append(json::Node::Object{});
                    jText.set("type", json::Node::Text{"text"});
                    jText.set("text", json::Node::Text{msg->content.toString()});
                } else if (msg->role == Transcript::Role::ToolCall) {
                    // Recreate tool blocks when opaque provider output isn't available.
                    fallbackToolCallID++;
                    StringView name;
                    json::ParseResult parsedArgs;
                    parseToolCallText(msg->content, name, parsedArgs);
                    String callID = msg->providerToolCallID
                                        ? msg->providerToolCallID
                                        : String::format("toolu_{}_{}_{}", i, turnIndex, fallbackToolCallID);
                    if (turn.providerOutputItems.isEmpty()) {
                        json::Node& jCall = jAssistantContent.array().append(json::Node::Object{});
                        jCall.set("type", json::Node::Text{"tool_use"});
                        jCall.set("id", json::Node::Text{callID});
                        jCall.set("name", json::Node::Text{name});
                        jCall.set("input", parsedArgs.root.isObject() ? std::move(parsedArgs.root)
                                                                      : json::Node{json::Node::Object{}});
                    }
                    if (msg->toolEnded) {
                        json::Node& jResult = jToolResults.array().append(json::Node::Object{});
                        jResult.set("type", json::Node::Text{"tool_result"});
                        jResult.set("tool_use_id", json::Node::Text{std::move(callID)});
                        jResult.set("content", json::Node::Text{msg->toolResponse.toString()});
                    }
                }
            }

            // Replay original content blocks to preserve signed thinking context.
            for (StringView itemText : turn.providerOutputItems) {
                Owned<json::Parser> parser = json::Parser::create();
                parser->setErrorCallback([](const json::ParseError&) {});
                json::ParseResult item = parser->parse({}, itemText);
                if (item.root.isObject()) {
                    jAssistantContent.array().append(std::move(item.root));
                }
            }

            // Anthropic requires tool results in the user message immediately after tool uses.
            if (jAssistantContent.array().items()) {
                json::Node& jMessage = jMessages.array().append(json::Node::Object{});
                jMessage.set("role", json::Node::Text{"assistant"});
                jMessage.set("content", std::move(jAssistantContent));
            }
            if (jToolResults.array().items()) {
                json::Node& jMessage = jMessages.array().append(json::Node::Object{});
                jMessage.set("role", json::Node::Text{"user"});
                jMessage.set("content", std::move(jToolResults));
            }
        }
    }
    root.set("messages", std::move(jMessages));
    return json::toString(root, {false});
}

void AnthropicProtocolHandler::receiveLine(StringView line) {
    Agent::Impl* impl = this->impl;
    if (!line.startsWith("data: "))
        return;

    // Parse the JSON payload; the SSE event name is duplicated in its type property.
    Owned<json::Parser> parser = json::Parser::create();
    parser->setErrorCallback([](const json::ParseError&) {});
    parser->setGreedy(false);
    json::ParseResult result = parser->parse({}, line.substr(6).trim());
    if (!result.root.isObject())
        return;
    StringView eventType = result.root.get("type").text();
    const json::Node& jDelta = result.root.get("delta");

    // Message streams expose input counts at the start and cumulative output at the end.
    if (eventType == "message_start" || eventType == "message_delta") {
        const json::Node& jUsage =
            eventType == "message_start" ? result.root.get("message").get("usage") : result.root.get("usage");
        if (jUsage.isObject()) {
            this->tokenUsage.isValid = true;
            // Anthropic reports uncached input and cache writes separately from cache reads.
            u64 inputTokens = 0;
            u64 cacheCreationInputTokens = 0;
            copyTokenCount(inputTokens, jUsage.get("input_tokens"));
            copyTokenCount(cacheCreationInputTokens, jUsage.get("cache_creation_input_tokens"));
            this->tokenUsage.uncachedInputTokens = inputTokens + cacheCreationInputTokens;
            copyTokenCount(this->tokenUsage.cachedInputTokens, jUsage.get("cache_read_input_tokens"));
            copyTokenCount(this->tokenUsage.outputTokens, jUsage.get("output_tokens"));
        }
        if (eventType == "message_delta" && jDelta.get("stop_reason").text() && this->tokenUsage.isValid) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            setTokenUsage(impl, this->tokenUsage);
        }
    }

    if (eventType == "content_block_start") {
        const json::Node& jBlock = result.root.get("content_block");
        this->contentBlock = json::Node{jBlock};
        this->contentBlockIndex = (s32) result.root.get("index").getNumber();
        if (jBlock.get("type").text() != "tool_use")
            return;
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;

        // Start buffering the tool name followed by streamed JSON arguments.
        u32 toolCallID = 1;
        for (const Owned<Transcript::Message>& msg : impl->internalTranscript->turns.back().messages) {
            if (msg->role == Transcript::Role::ToolCall) {
                toolCallID++;
            }
        }
        beginMessage(impl, Transcript::Role::ToolCall, toolCallID, jBlock.get("id").text());
        appendText(impl, String{jBlock.get("name").text()});
        this->toolCall = impl->internalTranscript->turns.back().messages.back().get();
        impl->currentRole = Transcript::Role::ToolCall;
    } else if (eventType == "content_block_delta") {
        StringView deltaType = jDelta.get("type").text();
        StringView field;
        StringView deltaText;
        if (deltaType == "text_delta") {
            field = "text";
            deltaText = jDelta.get("text").text();
        } else if (deltaType == "thinking_delta") {
            field = "thinking";
            deltaText = jDelta.get("thinking").text();
        } else if (deltaType == "signature_delta") {
            field = "signature";
            deltaText = jDelta.get("signature").text();
        }
        if (field && this->contentBlock.isObject() &&
            this->contentBlockIndex == (s32) result.root.get("index").getNumber()) {
            String text = this->contentBlock.get(field).text();
            text += deltaText;
            this->contentBlock.set(field, json::Node::Text{std::move(text)});
        }

        if (deltaType == "text_delta" || deltaType == "thinking_delta") {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            Transcript::Role role =
                deltaType == "thinking_delta" ? Transcript::Role::AgentThinking : Transcript::Role::Agent;
            StringView text = deltaType == "thinking_delta" ? jDelta.get("thinking").text() : jDelta.get("text").text();
            emitText(impl, role, text);
        } else if (deltaType == "input_json_delta") {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            if (this->toolCall && this->contentBlockIndex == (s32) result.root.get("index").getNumber()) {
                appendText(impl, String{jDelta.get("partial_json").text()});
            }
        }
    } else if (eventType == "content_block_stop") {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        if (this->contentBlockIndex != (s32) result.root.get("index").getNumber())
            return;
        if (this->toolCall) {
            // Queue the tool only after its complete input JSON has arrived.
            if (this->toolCall->content.toString().find('{') < 0) {
                appendText(impl, String{"{}"});
            }
            this->toolCall->content.flush();
            StringView name;
            json::ParseResult parsedArgs;
            parseToolCallText(this->toolCall->content, name, parsedArgs);
            this->contentBlock.get("input") =
                parsedArgs.root.isObject() ? std::move(parsedArgs.root) : json::Node{json::Node::Object{}};
            impl->anyToolCallsThisTurn = true;
            impl->pendingToolCalls.append(this->toolCall);
            impl->toolCondVar.wakeAll();
            this->toolCall = nullptr;
        }

        // Preserve the completed block for exact stateless replay on the next request.
        Transcript::Event itemEvent;
        itemEvent.operation = Transcript::Event::AppendProviderOutputItem;
        itemEvent.text = json::toString(this->contentBlock, {false});
        addEvent(impl, std::move(itemEvent));
        this->contentBlock = {};
        this->contentBlockIndex = -1;
    } else if (eventType == "error") {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        StringView message = result.root.get("error").get("message").text();
        emitText(impl, Transcript::Role::Error, message ? message : "Anthropic API request failed");
    } else if (eventType == "message_delta" && jDelta.get("stop_reason").text() == "max_tokens") {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        emitText(impl, Transcript::Role::Error, "Anthropic API request reached max_tokens");
    }
}

//  ▄▄▄▄         ▄▄                               ▄▄   ▄▄                           ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//   ██  ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄      ██  ██ ██  ██  ██
//   ██  ██  ██  ██   ██▄▄██ ██  ▀▀  ▄▄▄██ ██     ██   ██ ██  ██ ██  ██ ▀█▄▄▄      ██▀▀██ ██▀▀▀   ██
//  ▄██▄ ██  ██  ▀█▄▄ ▀█▄▄▄  ██     ▀█▄▄██ ▀█▄▄▄  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀     ██  ██ ██     ▄██▄
//

struct InteractionsProtocolHandler : ProtocolHandler {
    // Accumulate the active streamed step until it is complete and replayable.
    s32 stepIndex = -1;
    json::Node step;
    Transcript::Message* toolCall = nullptr;

    using ProtocolHandler::ProtocolHandler;
    virtual String makeRequestBody() override;
    virtual void receiveLine(StringView line) override;
};

// Append text to the final text block in an Interactions step.
static void appendInteractionsText(json::Node& step, StringView field, StringView text) {
    json::Node& jItems = step.get(field);
    if (!jItems.isArray()) {
        step.set(field, json::Node::Array{});
    }
    Array<json::Node>& items = step.get(field).array();
    if (items.items() && items.back().get("type").text() == "text") {
        String combined = items.back().get("text").text();
        combined += text;
        items.back().set("text", json::Node::Text{std::move(combined)});
    } else {
        json::Node& item = items.append(json::Node::Object{});
        item.set("type", json::Node::Text{"text"});
        item.set("text", json::Node::Text{String{text}});
    }
}

String InteractionsProtocolHandler::makeRequestBody() {
    Agent::Impl* impl = this->impl;

    // Discard any incomplete step left by the preceding request.
    this->stepIndex = -1;
    this->step = {};
    this->toolCall = nullptr;

    json::Node root{json::Node::Object{}};
    root.set("model", json::Node::Text{impl->settings.endPoint.model});
    root.set("store", json::Node::Bool{false});
    root.set("stream", json::Node::Bool{true});

    // Send the system prompt as interaction-scoped configuration.
    if (impl->settings.capabilities.systemPrompt) {
        root.set("system_instruction", json::Node::Text{impl->settings.capabilities.systemPrompt});
    }

    // Describe client-side tools using Interactions function declarations.
    if (impl->settings.capabilities.tools.items()) {
        json::Node tools{json::Node::Array{}};
        for (const Owned<ToolDefinition>& tool : impl->settings.capabilities.tools) {
            json::Node& jTool = tools.array().append(json::Node::Object{});
            jTool.set("type", json::Node::Text{"function"});
            jTool.set("name", json::Node::Text{tool->name});
            jTool.set("description", json::Node::Text{tool->description});
            json::Node schema{json::Node::Object{}};
            schema.set("type", json::Node::Text{"object"});
            json::Node required{json::Node::Array{}};
            json::Node properties{json::Node::Object{}};
            for (const ToolDefinition::Parameter& param : tool->parameters) {
                json::Node property{json::Node::Object{}};
                property.set("type", json::Node::Text{param.type});
                property.set("description", json::Node::Text{param.description});
                if (param.type == "array") {
                    json::Node items{json::Node::Object{}};
                    items.set("type", json::Node::Text{"object"});
                    property.set("items", std::move(items));
                }
                properties.set(param.name, std::move(property));
                if (param.required) {
                    required.array().append(json::Node::Text{param.name});
                }
            }
            schema.set("properties", std::move(properties));
            schema.set("required", std::move(required));
            jTool.set("parameters", std::move(schema));
        }
        root.set("tools", std::move(tools));
    }

    // Flatten the transcript into the complete stateless interaction history.
    Array<const Transcript*> flattened;
    for (const Transcript* transcript = impl->internalTranscript; transcript; transcript = transcript->parent) {
        flattened.append(transcript);
    }
    json::Node input{json::Node::Array{}};
    for (s32 i = flattened.numItems() - 1; i >= 0; i--) {
        for (u32 turnIndex = 0; turnIndex < flattened[i]->turns.numItems(); turnIndex++) {
            const Transcript::Turn& turn = flattened[i]->turns[turnIndex];
            json::Node fallbackSteps{json::Node::Array{}};
            json::Node functionResults{json::Node::Array{}};
            u32 fallbackToolCallID = 0;
            for (const Transcript::Message* msg : turn.messages) {
                if (msg->role == Transcript::Role::User) {
                    json::Node& userInput = input.array().append(json::Node::Object{});
                    userInput.set("type", json::Node::Text{"user_input"});
                    appendInteractionsText(userInput, "content", msg->content.toString());
                } else if (msg->role == Transcript::Role::Agent && turn.providerOutputItems.isEmpty()) {
                    json::Node& modelOutput = fallbackSteps.array().append(json::Node::Object{});
                    modelOutput.set("type", json::Node::Text{"model_output"});
                    appendInteractionsText(modelOutput, "content", msg->content.toString());
                } else if (msg->role == Transcript::Role::ToolCall) {
                    fallbackToolCallID++;
                    StringView name;
                    json::ParseResult parsedArgs;
                    parseToolCallText(msg->content, name, parsedArgs);
                    String callID = msg->providerToolCallID
                                        ? msg->providerToolCallID
                                        : String::format("call_{}_{}_{}", i, turnIndex, fallbackToolCallID);
                    if (turn.providerOutputItems.isEmpty()) {
                        json::Node& functionCall = fallbackSteps.array().append(json::Node::Object{});
                        functionCall.set("type", json::Node::Text{"function_call"});
                        functionCall.set("id", json::Node::Text{callID});
                        functionCall.set("name", json::Node::Text{name});
                        functionCall.set("arguments", parsedArgs.root.isObject() ? std::move(parsedArgs.root)
                                                                                 : json::Node{json::Node::Object{}});
                    }
                    if (msg->toolEnded) {
                        json::Node& functionResult = functionResults.array().append(json::Node::Object{});
                        functionResult.set("type", json::Node::Text{"function_result"});
                        functionResult.set("name", json::Node::Text{name});
                        functionResult.set("call_id", json::Node::Text{std::move(callID)});
                        appendInteractionsText(functionResult, "result", msg->toolResponse.toString());
                    }
                }
            }

            // Replay every signed model step exactly as it was assembled from the stream.
            for (StringView itemText : turn.providerOutputItems) {
                Owned<json::Parser> parser = json::Parser::create();
                parser->setErrorCallback([](const json::ParseError&) {});
                json::ParseResult item = parser->parse({}, itemText);
                if (item.root.isObject()) {
                    input.array().append(std::move(item.root));
                }
            }
            if (turn.providerOutputItems.isEmpty()) {
                for (json::Node& step : fallbackSteps.array()) {
                    input.array().append(std::move(step));
                }
            }
            for (json::Node& result : functionResults.array()) {
                input.array().append(std::move(result));
            }
        }
    }
    root.set("input", std::move(input));

    // Request thought summaries while allowing the selected model to choose its default effort.
    json::Node generationConfig{json::Node::Object{}};
    generationConfig.set("thinking_summaries", json::Node::Text{"auto"});
    root.set("generation_config", std::move(generationConfig));
    return json::toString(root, {false});
}

void InteractionsProtocolHandler::receiveLine(StringView line) {
    Agent::Impl* impl = this->impl;
    if (!line.startsWith("data:"))
        return;

    // Parse one typed event from the Interactions SSE stream.
    Owned<json::Parser> parser = json::Parser::create();
    parser->setErrorCallback([](const json::ParseError&) {});
    parser->setGreedy(false);
    json::ParseResult result = parser->parse({}, line.substr(5).trim());
    if (!result.root.isObject())
        return;

    StringView eventType = result.root.get("event_type").text();
    if (!eventType) {
        eventType = result.root.get("type").text();
    }

    // Start a replayable step and prepare any client-side function call.
    if (eventType == "step.start") {
        const json::Node& jStep = result.root.get("step");
        if (!jStep.isObject())
            return;
        this->step = json::Node{jStep};
        this->stepIndex = (s32) result.root.get("index").getNumber();
        if (jStep.get("type").text() != "function_call")
            return;
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;

        // Buffer the function name followed by its streamed JSON arguments.
        u32 toolCallID = 1;
        for (const Owned<Transcript::Message>& msg : impl->internalTranscript->turns.back().messages) {
            if (msg->role == Transcript::Role::ToolCall) {
                toolCallID++;
            }
        }
        beginMessage(impl, Transcript::Role::ToolCall, toolCallID, jStep.get("id").text());
        appendText(impl, String{jStep.get("name").text()});
        this->toolCall = impl->internalTranscript->turns.back().messages.back().get();
        impl->currentRole = Transcript::Role::ToolCall;
    } else if (eventType == "step.delta") {
        if (this->stepIndex != (s32) result.root.get("index").getNumber())
            return;
        const json::Node& delta = result.root.get("delta");
        StringView deltaType = delta.get("type").text();
        StringView visibleText;
        Transcript::Role role = Transcript::Role::None;
        if (deltaType == "text") {
            visibleText = delta.get("text").text();
            if (visibleText) {
                appendInteractionsText(this->step, "content", visibleText);
            }
            role = Transcript::Role::Agent;
        } else if (deltaType == "thought_summary" || deltaType == "thought") {
            visibleText = delta.get("content").get("text").text();
            if (!visibleText) {
                visibleText = delta.get("text").text();
            }
            if (visibleText) {
                appendInteractionsText(this->step, "summary", visibleText);
            }
            role = Transcript::Role::AgentThinking;
        } else if (deltaType == "thought_signature") {
            this->step.set("signature", json::Node::Text{String{delta.get("signature").text()}});
        } else if (deltaType == "arguments_delta" || deltaType == "arguments") {
            StringView arguments = delta.get("arguments").text();
            if (!arguments) {
                arguments = delta.get("partial_arguments").text();
            }
            if (this->toolCall) {
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.isCanceled())
                    return;
                appendText(impl, String{arguments});
            }
        }
        if (role != Transcript::Role::None && visibleText) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled())
                return;
            emitText(impl, role, visibleText);
        }
    } else if (eventType == "step.stop") {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        if (this->stepIndex != (s32) result.root.get("index").getNumber() || !this->step.isObject())
            return;
        if (this->toolCall) {
            // Queue the function only after its complete argument JSON has arrived.
            if (this->toolCall->content.toString().find('{') < 0) {
                appendText(impl, String{"{}"});
            }
            this->toolCall->content.flush();
            StringView name;
            json::ParseResult parsedArgs;
            parseToolCallText(this->toolCall->content, name, parsedArgs);
            this->step.get("arguments") =
                parsedArgs.root.isObject() ? std::move(parsedArgs.root) : json::Node{json::Node::Object{}};
            impl->anyToolCallsThisTurn = true;
            impl->pendingToolCalls.append(this->toolCall);
            impl->toolCondVar.wakeAll();
            this->toolCall = nullptr;
        }

        // Preserve the completed step for exact stateless replay on the next request.
        Transcript::Event itemEvent;
        itemEvent.operation = Transcript::Event::AppendProviderOutputItem;
        itemEvent.text = json::toString(this->step, {false});
        addEvent(impl, std::move(itemEvent));
        this->step = {};
        this->stepIndex = -1;
    } else if (eventType == "interaction.completed") {
        const json::Node& interaction = result.root.get("interaction");
        const json::Node& usage = interaction.get("usage");
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        if (usage.isObject()) {
            Transcript::TokenUsage tokenUsage;
            tokenUsage.isValid = true;
            copyTokenCount(tokenUsage.cachedInputTokens, usage.get("total_cached_tokens"));
            copyUncachedInputCount(tokenUsage.uncachedInputTokens, usage.get("total_input_tokens"),
                                   tokenUsage.cachedInputTokens);
            u64 outputTokens = 0;
            u64 thoughtTokens = 0;
            copyTokenCount(outputTokens, usage.get("total_output_tokens"));
            copyTokenCount(thoughtTokens, usage.get("total_thought_tokens"));
            tokenUsage.outputTokens = outputTokens + thoughtTokens;
            setTokenUsage(impl, tokenUsage);
        }
        StringView status = interaction.get("status").text();
        if (status && status != "completed" && status != "requires_action") {
            emitText(impl, Transcript::Role::Error,
                     String::format("Interactions API completed with status {}", status));
        }
    } else if (eventType == "error" || eventType == "interaction.failed") {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->toolCtx.isCanceled())
            return;
        StringView message = result.root.get("error").get("message").text();
        emitText(impl, Transcript::Role::Error, message ? message : "Interactions API request failed");
    }
}

//  ▄▄▄▄          ▄▄▄                                              ▄▄▄▄▄▄ ▄▄                              ▄▄
//   ██  ▄▄▄▄▄   ██    ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄        ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//   ██  ██  ██ ▀██▀▀ ██▄▄██ ██  ▀▀ ██▄▄██ ██  ██ ██    ██▄▄██       ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//  ▄██▄ ██  ██  ██   ▀█▄▄▄  ██     ▀█▄▄▄  ██  ██ ▀█▄▄▄ ▀█▄▄▄        ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

// Delivers an error event via the pendingEvents buffer.
// Must be called with toolCtx.mutex held. Suppresses the event if the agent was
// canceled, so bufferEvent is never called once cancellation has been requested.
void onError(Agent::Impl* impl, StringView message) {
    if (impl->toolCtx.isCanceled())
        return;
    beginMessage(impl, Transcript::Role::Error);
    appendText(impl, String{message});
    impl->currentRole = Transcript::Role::Error;
}

// Extracts the provider's message from a JSON error envelope and falls back to the HTTP status.
// When authentication is rejected, identify the environment variable that supplied the key.
static String makeHTTPErrorMessage(u32 statusCode, StringView responseBody, StringView apiKeyEnv) {
    MemStream httpMessage;
    if ((statusCode == 401) && apiKeyEnv && (apiKeyEnv != "NONE")) {
        httpMessage.format("The API key stored in {} was rejected by the server.\n", apiKeyEnv);
    }
    httpMessage.format("HTTP response code {}", statusCode);
    Owned<json::Parser> parser = json::Parser::create();
    parser->setErrorCallback([](const json::ParseError&) {});
    parser->setGreedy(false);
    json::ParseResult result = parser->parse({}, responseBody.trim());
    StringView message = result.root.get("error").get("message").text();
    if (!message) {
        message = result.root.get("message").text();
    }
    if (message) {
        httpMessage.format(": {}", message);
    }
    return httpMessage.moveToString();
}

// Dispatches the accumulated line to the selected protocol handler.
void receiveLineInProgress(Agent::Impl* impl) {
    String line = impl->lineInProgress.moveToString();
    impl->lineInProgress = MemStream{};
    if (!line)
        return;
    impl->protocolHandler->receiveLine(line);
}

// Convert the Agent::Protocol enum to a string.
static StringView getProtocolName(Agent::Protocol protocol) {
    switch (protocol) {
        case Agent::Protocol::Unset:
            break;
        case Agent::Protocol::Completions:
            return "completions";
        case Agent::Protocol::Responses:
            return "responses";
        case Agent::Protocol::Anthropic:
            return "anthropic";
        case Agent::Protocol::Interactions:
            return "interactions";
    }
    PLY_ASSERT(0);
    return {};
}

// Performs an inference request and converts the response data to a queue of transcript events.
// This is the bulk of the work performed by the inference thread (Agent::Impl::inferenceThread).
// The calling thread receives events by periodically calling pollForEvents or one of the wait functions.
void performInferenceRequest(Agent::Impl* impl, u32 turnNumber) {
    impl->anyToolCallsThisTurn = false;
    // Reset the per-turn role tracking so the first streamed message begins a new
    // Message block.
    impl->currentRole = Transcript::Role::None;
    impl->lineInProgress = MemStream{};

    if (impl->settings.enableRawLog && !impl->rawLogFile.isOpen()) {
        // Create one log file for the agent's complete inference session.
        DateTime dateTime = convertToDateTime(getUnixTimestamp());
        String timestampStr = String::fromDateTime("%Y%m%d-%H%M%S", dateTime);
        String logFilename = String::format("agent-raw-{}.log", timestampStr);
        impl->rawLogFile = FileSystem::openBinaryForWrite(logFilename);
        if (impl->rawLogFile.isOpen()) {
            // Write immutable session details before logging the first provider response.
            impl->rawLogFile.format("========================================\n"
                                    "AGENT RAW LOG\n"
                                    "========================================\n"
                                    "Start time: {}\n",
                                    String::fromDateTime("%Y-%m-%d %H:%M:%S", dateTime));
            impl->rawLogFile.format("Destination URL: {}\nProtocol: {}\nModel: {}\n\n", impl->settings.endPoint.url,
                                    getProtocolName(impl->settings.endPoint.protocol), impl->settings.endPoint.model);
        }
    }
    if (impl->rawLogFile.isOpen()) {
        // Separate each provider request so tool-driven follow-up turns remain readable.
        if (turnNumber != 1) {
            impl->rawLogFile.write('\n');
        }
        impl->rawLogFile.format(
            "----------------------------------------\nTURN #{}\n----------------------------------------\n\n",
            turnNumber);
    }

    // Let the selected protocol translate the current transcript into a request body.
    String body = impl->protocolHandler->makeRequestBody();

    // Apply the protocol-specific headers alongside Content-Type.
    Map<String, String> headers;
    *headers.insert("Content-Type").value = "application/json";
    if (impl->settings.endPoint.protocol == Agent::Protocol::Anthropic) {
        *headers.insert("anthropic-version").value = "2023-06-01";
    }
    StringView apiKeyEnv = impl->settings.endPoint.apiKeyEnv;
    if (apiKeyEnv != "NONE") {
        String apiKey = getEnvironmentVariable(apiKeyEnv);
        if (!apiKey) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            onError(impl, String::format("Missing API key: environment variable {} is not set", apiKeyEnv));
            return;
        }
        if (impl->settings.endPoint.protocol == Agent::Protocol::Anthropic) {
            *headers.insert("x-api-key").value = std::move(apiKey);
        } else if (impl->settings.endPoint.protocol == Agent::Protocol::Interactions) {
            *headers.insert("x-goog-api-key").value = std::move(apiKey);
        } else {
            *headers.insert("Authorization").value = String::format("Bearer {}", apiKey);
        }
    }

    // State accumulated across curl callbacks (the callback runs on this same inference thread,
    // invoked from within HTTPClient::receiveResponse()).
    struct RequestState {
        bool gotError = false;
        u32 statusCode = 0;
        String errorMessage;
        MemStream errorBody;
    } state;

    // The callback splits the incoming response stream into JSONL lines and dispatches each one.
    // It remains owned by the HTTP request until that request completes or is canceled.
    Functor<void(const HTTPClient::Event&)> callback = [impl, &state](const HTTPClient::Event& event) {
        if (auto* headers = event.as<HTTPClient::Headers>()) {
            // Treat non-200 responses as agent request failures while still allowing HTTPClient to deliver the body.
            state.statusCode = headers->statusCode;
            state.gotError = headers->statusCode != 200;
            if (state.gotError) {
                state.errorMessage = String::format("HTTP response code {} from server", headers->statusCode);
            }

            // Log the response status and every delivered header before the response body.
            if (impl->rawLogFile.isOpen()) {
                impl->rawLogFile.format("HTTP response status: {}\n", headers->statusCode);
                for (const auto& item : headers->headers.items()) {
                    impl->rawLogFile.format("{}: {}\n", item.key, item.value);
                }
                impl->rawLogFile.write('\n');
            }
            return;
        }
        if (auto* error = event.as<HTTPClient::Error>()) {
            // HTTPClient reports libcurl errors using a terminal Error event.
            state.gotError = true;
            state.errorMessage = error->message;
            return;
        }
        if (event.is<HTTPClient::End>()) {
            // Parse a final unterminated streaming line before completing the request.
            receiveLineInProgress(impl);
            return;
        }
        auto* data = event.as<HTTPClient::Data>();
        if (!data)
            return;

        // Write raw HTTP response to log file
        if (impl->rawLogFile.isOpen()) {
            impl->rawLogFile.write(data->bytes);
        }
        if (state.statusCode != 200) {
            state.errorBody.write(data->bytes);
        }

        // Split incoming data into lines.
        StringView remaining = data->bytes;
        while (remaining) {
            s32 newLinePos = remaining.find('\n');
            if (newLinePos >= 0) {
                // Reached the end of a line.
                impl->lineInProgress.write(remaining.left(newLinePos + 1));
                receiveLineInProgress(impl);
                remaining = remaining.substr(newLinePos + 1);
            } else {
                // Add incomplete line to lineInProgress.
                impl->lineInProgress.write(remaining);
                break;
            }
        }
    };

    // Send the request via HTTPClient (libcurl multi interface).
    {
        HTTPClient::Args args;
        args.url = impl->settings.endPoint.url;
        args.headers = std::move(headers);
        args.body = std::move(body);
        args.callback = std::move(callback);
        args.useBundledCaCert = true; // Verify TLS against the shipped cacert.pem.
        impl->httpClient->beginRequest(std::move(args));
    }

    // Drive the multi handle until the request completes or the client cancels.
    // HTTPClient::receiveResponse() performs curl_multi_perform/poll and returns false once the
    // request has finished.
    for (;;) {
        // Check for cancellation between iterations. The client thread signals cancel
        // by setting `canceled` and calling HTTPClient::wakeUp(), which unblocks the
        // curl_multi_poll inside receiveResponse() so this loop observes the change promptly.
        {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled()) {
                impl->httpClient->cancelRequest();
                break;
            }
        }
        if (!impl->httpClient->isRequestInProgress())
            break;
        if (!impl->httpClient->receiveResponse())
            break;
    }

    // Replace the generic HTTP failure with the provider's JSON error message when available.
    if (state.statusCode != 0 && state.statusCode != 200) {
        state.errorMessage =
            makeHTTPErrorMessage(state.statusCode, state.errorBody.moveToString(), impl->settings.endPoint.apiKeyEnv);
    }

    // Report any error that occurred during the request.
    if (state.gotError) {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        onError(impl, state.errorMessage);
    }

    // Wait for the tool thread to finish all currently-queued tools. Also stop
    // waiting if the client canceled; the runAgentThread loop will observe
    // `canceled` afterwards and exit without appending another turn.
    {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        while (!impl->pendingToolCalls.isEmpty()) {
            if (impl->toolCtx.isCanceled())
                break;
            impl->inferenceCondVar.wait(guard);
        }
    }
}

void runAgentThread(Agent::Impl* impl) {
    // Supply the initial turn when the caller passed an empty transcript.
    {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        if (impl->internalTranscript->turns.isEmpty() && !impl->toolCtx.isCanceled()) {
            beginTurn(impl);
        }
    }

    // Iterate making inference requests until the main thread requests exit
    // or there are no more tool responses to send back.
    for (u32 turnNumber = 1;; turnNumber++) {
        // Perform one inference request.
        performInferenceRequest(impl, turnNumber);

        {
            // Consume tool response events and check whether the loop should continue running.
            LockGuard<Mutex> guard{impl->toolCtx.mutex};

            // A canceled inference is incomplete and therefore has no EndTurn event.
            if (impl->toolCtx.isCanceled())
                break;

            // Finalize every completed inference, including the last one.
            endTurn(impl);
            if (!impl->anyToolCallsThisTurn)
                break;

            // Create the destination turn before starting the next inference.
            beginTurn(impl);
        }
    }

    // The inference thread is exiting: set `inferenceEnded` (protected by toolCtx.mutex)
    // and wake the tool thread so it observes it, drains any remaining tool queue, sets
    // toolEnded and wakes the client condvars (including the completion condvar). We do
    // NOT signal the client condvars here: inferenceEnded alone does not make the agent
    // "stopped" (toolEnded is still false), so waking waitForEvents/waitForCompletion now
    // would only be a spurious wakeup. They are released by the tool thread instead.
    {
        LockGuard<Mutex> guard{impl->toolCtx.mutex};
        impl->inferenceEnded = true;
        impl->toolCondVar.wakeAll();
    }

    // The inference thread's Reference<Agent::Impl> (captured by the thread functor)
    // is released when this function returns and the functor is destroyed.
    impl->decRefCount();
}

//  ▄▄▄▄▄▄               ▄▄▄      ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██    ▄▄▄▄   ▄▄▄▄   ██        ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ██  ██        ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄       ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

void runToolThread(Agent::Impl* impl) {
    bool popHeadItem = false;

    for (;;) {
        // Stop if the client destroyed the Agent. Hold the mutex while publishing the stopped state.
        {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.isCanceled()) {
                impl->toolEnded = true;
                // Release any client thread waiting for the agent to stop.
                impl->clientCondVar.wakeAll();
                impl->completionCondVar.wakeAll();
                break;
            }
        }

        Transcript::Message* toolCall = nullptr;
        {
            // Critical section: fetch the next tool call to run.
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (popHeadItem) {
                // Pop the previously completed tool call from the queue.
                impl->pendingToolCalls.erase(0);
                popHeadItem = false;
            }
            if (impl->pendingToolCalls.isEmpty()) {
                if (impl->inferenceEnded) {
                    // No more work will ever arrive.
                    impl->toolEnded = true;
                    // Release any client thread waiting for the agent to stop.
                    impl->clientCondVar.wakeAll();
                    impl->completionCondVar.wakeAll();
                    break;
                }
                // No requests available. Notify the inference thread (it may be
                // waiting for the queue to drain) and wait for work, cancellation,
                // or for inference to end. After waking we loop back to re-check
                // `canceled` at the top of the outer loop.
                impl->inferenceCondVar.wakeAll();
                impl->toolCondVar.wait(guard);
                continue;
            }
            toolCall = impl->pendingToolCalls[0];
        }

        // Re-parse the tool call's content to recover the tool name and its
        // JSON arguments, then look up the handler by name.
        StringView tcName;
        json::ParseResult parsedArgs;
        parseToolCallText(toolCall->content, tcName, parsedArgs);
        const json::Node& arguments = parsedArgs.root;

        // Handle this tool call (no locks held).
        // Look up the handler for this tool call by name.
        const Owned<ToolDefinition>* found = impl->settings.capabilities.tools.find(tcName);
        // FIXME: Improve error handling
        PLY_ASSERT(found);
        const ToolDefinition* toolDef = found->get();
        {
            toolDef->handler(&impl->toolCtx, toolCall, arguments);
            // The handler must not leave a cancel callback set.
            PLY_ASSERT(!impl->toolCtx.cancelCallback);
        }

        // Finalize the internal response and notify the client that the handler returned.
        {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            toolCall->toolResponse.flush();
            toolCall->toolEnded = true;
            if (!impl->toolCtx.isCanceled()) {
                Transcript::Event endResp;
                endResp.operation = Transcript::Event::EndToolResponse;
                endResp.toolCallID = toolCallIDForMessage(impl, toolCall);
                bufferEvent(impl, std::move(endResp));
            }
        }

        popHeadItem = true;
    }

    impl->decRefCount();
}

bool ToolContext::isCanceled() const {
    return static_cast<const ToolContextImpl*>(this)->canceled.load(MemoryOrder::Acquire);
}

bool ToolContext::setCancelCallback(Functor<void()>&& callback) {
    ToolContextImpl* impl = static_cast<ToolContextImpl*>(this);
    LockGuard<Mutex> guard{impl->mutex};
    PLY_ASSERT(callback);
    if (this->isCanceled()) {
        impl->cancelCallback = {};
        return false;
    }
    impl->cancelCallback = std::move(callback);
    return true;
}

void ToolContext::clearCancelCallback() {
    ToolContextImpl* impl = static_cast<ToolContextImpl*>(this);
    LockGuard<Mutex> guard{impl->mutex};
    impl->cancelCallback = {};
}

// Main function used by tool handlers to add text to the response. It locks the mutex, appends response text
// to the internal toolCall and creates a AppendToolResponse event for the client to consume.
void ToolContext::appendResponse(Transcript::Message* toolCall, StringView text) {
    ToolContextImpl* impl = static_cast<ToolContextImpl*>(this);
    LockGuard<Mutex> guard{impl->mutex};
    if (!this->isCanceled()) {
        // Append to the internal transcript while preserving completed line boundaries.
        toolCall->toolResponse.append(text);

        Transcript::Event appendResp;
        appendResp.operation = Transcript::Event::AppendToolResponse;
        appendResp.toolCallID = toolCallIDForMessage(impl->agentImpl, toolCall);
        appendResp.text = text;
        bufferEvent(impl->agentImpl, std::move(appendResp));
    }
}

StringView ToolContext::getWorkingDirectory() const {
    return static_cast<const ToolContextImpl*>(this)->workingDir;
}

//   ▄▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄
//          ▄▄▄█▀

Agent::Agent(const Settings& settings) {
    PLY_ASSERT(settings.startTranscript);

    // Initialize new Agent.
    this->impl = Heap::create<Agent::Impl>();
    Agent::Impl* impl = this->impl;
    impl->settings = settings;        // copies the settings
    this->settings = &impl->settings; // points to the internal copy of the settings
    impl->toolCtx.agentImpl = impl;
    impl->toolCtx.workingDir = impl->settings.capabilities.workingDir;

    // Create the handler that owns this endpoint's protocol-specific behavior and state.
    if (impl->settings.endPoint.protocol == Agent::Protocol::Completions) {
        impl->protocolHandler = Heap::create<CompletionsProtocolHandler>(impl);
    } else if (impl->settings.endPoint.protocol == Agent::Protocol::Responses) {
        impl->protocolHandler = Heap::create<ResponsesProtocolHandler>(impl);
    } else if (impl->settings.endPoint.protocol == Agent::Protocol::Anthropic) {
        impl->protocolHandler = Heap::create<AnthropicProtocolHandler>(impl);
    } else if (impl->settings.endPoint.protocol == Agent::Protocol::Interactions) {
        impl->protocolHandler = Heap::create<InteractionsProtocolHandler>(impl);
    } else {
        PLY_ASSERT(0);
    }

    // The HTTPClient lives for the whole conversation and is reused across turns.
    impl->httpClient = HTTPClient::create();

    // Make a copy of the transcript leaf, but link to the same parent.
    impl->internalTranscript = Heap::create<Transcript>(*impl->settings.startTranscript);

    // Spawn threads. Each thread captures a raw Agent::Impl* and is responsible for
    // dropping its own reference once it exits, so Agent::Impl stays alive until both
    // threads have finished.
    impl->incRefCount();
    impl->incRefCount();
    impl->toolThread.run([impl]() { runToolThread(impl); });
    impl->inferenceThread.run([impl]() { runAgentThread(impl); });
}

Agent::~Agent() {
    // Request cancellation, then return without joining the background threads. They
    // hold their own references to Agent::Impl, so it stays alive until both exit.

    this->cancel();
}

bool Agent::isWorking() {
    Agent::Impl* impl = this->impl;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    // The agent is still "working" while there are unconsumed buffered events, or
    // while the background threads are still active and haven't been canceled.
    if (!impl->pendingEvents.isEmpty())
        return true;
    if (impl->toolCtx.isCanceled())
        return false;
    if (impl->inferenceEnded && impl->toolEnded)
        return false;
    return true;
}

void Agent::cancel() {
    Agent::Impl* impl = this->impl;
    if (!impl)
        return;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    if (!impl->toolCtx.canceled.exchange(true, MemoryOrder::Release)) {
        // Interrupt any blocking operation owned by the active tool handler before waking its thread.
        if (impl->toolCtx.cancelCallback) {
            impl->toolCtx.cancelCallback();
        }
        // Wake both background threads so they observe `canceled` promptly. The tool
        // thread may be idle on toolCondVar; the inference thread may be blocked in its
        // "wait for tools" loop on inferenceCondVar, or blocked inside
        // curl_multi_poll while driving a request. wakeUp() unblocks the latter so
        // the inference loop observes `canceled` and tears down the request.
        impl->toolCondVar.wakeAll();
        impl->inferenceCondVar.wakeAll();
        // Release any client thread waiting for the agent to stop.
        impl->clientCondVar.wakeAll();
        impl->completionCondVar.wakeAll();
        impl->httpClient->wakeUp();
    }
}

Array<Transcript::Event> Agent::pollForEvents() {
    Agent::Impl* impl = this->impl;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    return std::move(impl->pendingEvents);
}

Array<Transcript::Event> Agent::waitForEvents(s32 maxTimeInMillis) {
    Agent::Impl* impl = this->impl;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    if (maxTimeInMillis == 0) {
        // Non-blocking: return whatever is buffered right now without waiting.
        return std::move(impl->pendingEvents);
    }
    u64 startTicks = 0;
    double ticksPerMs = 0.0;
    if (maxTimeInMillis > 0) {
        startTicks = getCpuTicks();
        ticksPerMs = getCpuTicksPerSecond() / 1000.0;
    }
    for (;;) {
        // Return as soon as there are any events available.
        if (!impl->pendingEvents.isEmpty())
            return std::move(impl->pendingEvents);
        // Return (with an empty array) once the agent has stopped working: no more
        // events will ever be produced.
        if (impl->toolCtx.isCanceled() || (impl->inferenceEnded && impl->toolEnded))
            return std::move(impl->pendingEvents);
        if (maxTimeInMillis < 0) {
            impl->clientCondVar.wait(guard);
        } else {
            u64 elapsed = (getCpuTicks() - startTicks) / ticksPerMs;
            if (elapsed >= numericCast<u64>(maxTimeInMillis))
                return std::move(impl->pendingEvents);
            impl->clientCondVar.timedWait(guard, numericCast<u32>(maxTimeInMillis - elapsed));
        }
    }
}

Array<Transcript::Event> Agent::waitForCompletion(s32 maxTimeInMillis) {
    Agent::Impl* impl = this->impl;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    if (maxTimeInMillis == 0) {
        // Non-blocking: return whatever is buffered right now without waiting.
        return std::move(impl->pendingEvents);
    }
    u64 startTicks = 0;
    double ticksPerMs = 0.0;
    if (maxTimeInMillis > 0) {
        startTicks = getCpuTicks();
        ticksPerMs = getCpuTicksPerSecond() / 1000.0;
    }
    for (;;) {
        // Stop waiting once the agent has stopped working (cancel, or both threads
        // exited) and drain all remaining buffered events.
        if (impl->toolCtx.isCanceled() || (impl->inferenceEnded && impl->toolEnded))
            return std::move(impl->pendingEvents);
        if (maxTimeInMillis < 0) {
            impl->completionCondVar.wait(guard);
        } else {
            u64 elapsed = (getCpuTicks() - startTicks) / ticksPerMs;
            if (elapsed >= numericCast<u64>(maxTimeInMillis))
                return std::move(impl->pendingEvents);
            impl->completionCondVar.timedWait(guard, numericCast<u32>(maxTimeInMillis - elapsed));
        }
    }
}

//--------------------------------------------------
// Tool permission helpers
//--------------------------------------------------
String ToolContext::checkPathPermission(StringView path, bool withWriteAccess) const {
    String absPath = makeAbsolutePath(joinPath(this->getWorkingDirectory(), path));
    const Agent::Capabilities& capabilities =
        static_cast<const ToolContextImpl*>(this)->agentImpl->settings.capabilities;
    const Array<String>& directories = withWriteAccess ? capabilities.writableDirs : capabilities.readableDirs;
    for (const String& dir : directories) {
        if (dir == absPath || (dir && absPath.numBytes() > dir.numBytes() && absPath.startsWith(dir) &&
                               (isPathSeparator(dir.back()) || isPathSeparator(absPath[dir.numBytes()]))))
            return absPath;
    }
    return {};
}

//         ▄▄            ▄▄▄  ▄▄▄
//   ▄▄▄▄  ██▄▄▄   ▄▄▄▄   ██   ██
//  ▀█▄▄▄  ██  ██ ██▄▄██  ██   ██
//   ▄▄▄█▀ ██  ██ ▀█▄▄▄  ▄██▄ ▄██▄
//

#if PLY_WITH_SUBPROCESS

// Runs a short-lived agent that decides whether a complete shell expression is permitted.
static bool authorizeShellCommand(ToolContext* toolCtx, StringView command, const ShellToolSettings& settings) {
    if (!settings.authorizerEndPoint.url || settings.authorizerEndPoint.protocol == Agent::Protocol::Unset ||
        !settings.authorizerEndPoint.model) {
        return false;
    }

    // Give the authorizer read access to every directory the main agent can read or write.
    const Agent::Capabilities& mainCapabilities =
        static_cast<const ToolContextImpl*>(toolCtx)->agentImpl->settings.capabilities;
    Array<String> readableDirs = mainCapabilities.readableDirs;
    for (const String& dir : mainCapabilities.writableDirs) {
        if (find(readableDirs, dir) < 0) {
            readableDirs.append(dir);
        }
    }

    // Give the authorizer a narrow, fail-closed role whose only trusted policy comes from this prompt.
    Agent::Capabilities capabilities;
    capabilities.workingDir = toolCtx->getWorkingDirectory();
    capabilities.readableDirs = readableDirs;
    capabilities.tools = settings.authorizerTools;
    MemStream systemPrompt;
    systemPrompt.write(
        "Your job is to approve or reject shell commands by deciding whether they're allowed by policy details given "
        "below. Decide whether the proposed command is allowed in its entirety. Reply with exactly ALLOW to approve "
        "it, or exactly DENY to reject it. Do not include any other text.\n\n"
        "By default, allow only familiar, read-only inspection utilities such as ls, dir and pwd. Reject commands "
        "that can modify state, access ungranted paths or communicate over a network such as shells, interpreters, "
        "compilers, debuggers, package managers and arbitrary process launchers, unless the policy details explicitly "
        "allow it. Check every command in pipelines, substitutions and compound expressions, as well as redirects and "
        "other shell side effects. If any part is unclear, reject the command.\n\n"
        "The shell command is only allowed to access files and directories inside the following directory trees:");
    for (const String& dir : readableDirs) {
        bool writable = find(mainCapabilities.writableDirs, dir) >= 0;
        systemPrompt.format("\n- {} ({})", dir, writable ? "read and write access" : "read only access");
    }
    systemPrompt.format("\n\nAdditional policy details:\n{}", settings.policy ? settings.policy : "(none)");
    capabilities.systemPrompt = systemPrompt.moveToString();

    // Present the command as user data in a fresh transcript.
    Transcript startTranscript;
    Transcript::Turn& turn = startTranscript.turns.append();
    Owned<Transcript::Message> userMsg = Heap::create<Transcript::Message>();
    userMsg->role = Transcript::Role::User;
    userMsg->content.append(String::format("Working directory: {}\nProposed command:\n<command>\n{}\n</command>",
                                           toolCtx->getWorkingDirectory(), command));
    userMsg->content.flush();
    turn.messages.append(std::move(userMsg));

    // Run the authorizer while forwarding cancellation from the main agent.
    Agent::Settings agentSettings;
    agentSettings.startTranscript = &startTranscript;
    agentSettings.endPoint = settings.authorizerEndPoint;
    agentSettings.capabilities = std::move(capabilities);
    Owned<Agent> authorizer = Heap::create<Agent>(agentSettings);
    if (!toolCtx->setCancelCallback([authorizer = authorizer.get()]() { authorizer->cancel(); })) {
        authorizer->cancel();
    }
    if (settings.authorizerHook) {
        settings.authorizerHook(authorizer);
    } else {
        while (authorizer->isWorking()) {
            authorizer->waitForEvents();
        }
    }
    toolCtx->clearCancelCallback();
    if (toolCtx->isCanceled())
        return false;

    // Accept only the exact affirmative sentinel from the last agent message.
    const Transcript& completedTranscript = *authorizer->impl->internalTranscript;
    for (s32 turnIndex = numericCast<s32>(completedTranscript.turns.numItems()) - 1; turnIndex >= 0; turnIndex--) {
        const Transcript::Turn& transcriptTurn = completedTranscript.turns[numericCast<u32>(turnIndex)];
        for (s32 msgIndex = numericCast<s32>(transcriptTurn.messages.numItems()) - 1; msgIndex >= 0; msgIndex--) {
            const Transcript::Message& msg = *transcriptTurn.messages[numericCast<u32>(msgIndex)];
            if (msg.role == Transcript::Role::Agent) {
                return msg.content.toString().trim() == "ALLOW";
            }
        }
    }
    return false;
}

// Executes an already-authorized command through the default system shell.
static void executeShellCommand(ToolContext* toolCtx, Transcript::Message* toolCall, StringView command) {
    // Run the command through the default shell with closed stdin and merged stdout/stderr.
    Subprocess::Options processOptions;
    processOptions.terminateProcessTree = true;
    Owned<Subprocess> process =
        Subprocess::execShellCommand(command, toolCtx->getWorkingDirectory(), Subprocess::Output::openMerged(),
                                     Subprocess::Input::ignore(), processOptions);
    if (!process) {
        toolCtx->appendResponse(toolCall, "Error: Could not start shell command.");
        return;
    }

    // Make cancellation terminate every process that can keep the merged output pipe open.
    if (!toolCtx->setCancelCallback([process = process.get()]() { process->terminate(); })) {
        process->terminate();
    }

    // Stream up to 5 KB of output while continuing to drain the pipe after the limit.
    constexpr u32 OutputLimit = 5000;
    char buffer[4096];
    u32 outputBytes = 0;
    char lastOutputByte = 0;
    bool outputTruncated = false;
    while (u32 numBytes = process->getStdOutReader()->read({buffer, sizeof(buffer)})) {
        u32 numBytesToAppend = min(numBytes, OutputLimit - outputBytes);
        if (numBytesToAppend > 0) {
            toolCtx->appendResponse(toolCall, StringView{buffer, numBytesToAppend});
            outputBytes += numBytesToAppend;
            lastOutputByte = buffer[numBytesToAppend - 1];
        }
        outputTruncated |= numBytesToAppend < numBytes;
    }
    // Keep the cancellation callback registered while join blocks; terminate() safely satisfies that wait.
    s32 exitCode = process->join();
    toolCtx->clearCancelCallback();

    // Report truncation and command status after all output has been consumed.
    MemStream response;
    if (outputBytes > 0 && lastOutputByte != '\n') {
        response.write('\n');
    }
    if (outputTruncated) {
        response.write("[Output truncated at 5000 bytes.]\n");
    }
    if (exitCode >= 0) {
        response.format("Process exited with code {}.", exitCode);
    } else {
        response.write("Error: Could not obtain shell command exit status.");
    }
    toolCtx->appendResponse(toolCall, response.moveToString());
}

Owned<ToolDefinition> createShellTool(const ShellToolSettings& settings) {
    // Create a handler that owns an immutable copy of the final authorization settings.
    Owned<ToolDefinition> shellTool = Heap::create<ToolDefinition>();
    shellTool->name = "shell";
    shellTool->readOnly = false;
    shellTool->description = "Request authorization to execute a command using the system shell in the current "
                             "working directory. Returns merged stdout/stderr, truncated to 5KB, followed by the "
                             "exit code.";
    shellTool->parameters.append();
    shellTool->parameters.back().name = "command";
    shellTool->parameters.back().description = "Shell command to execute";
    shellTool->parameters.back().type = "string";
    shellTool->parameters.back().required = true;
    shellTool->handler = [settings](ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
        // Validate the command before consulting the authorizer.
        const json::Node& commandArg = arguments.get("command");
        if (!commandArg.isText()) {
            toolCtx->appendResponse(toolCall, "Error: 'command' argument is required.");
            return;
        }

        // Fail closed unless unrestricted execution was explicitly requested.
        if (!settings.unrestricted && !authorizeShellCommand(toolCtx, commandArg.text(), settings)) {
            if (!toolCtx->isCanceled()) {
                toolCtx->appendResponse(toolCall, "Error: Shell command denied by authorizer.");
            }
            return;
        }
        executeShellCommand(toolCtx, toolCall, commandArg.text());
    };
    return shellTool;
}

#endif // PLY_WITH_SUBPROCESS

//                           ▄▄
//  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//  ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//  ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

void readToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    String absPath = toolCtx->checkPathPermission(path, false);
    if (!absPath) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Open file.
    Stream in = FileSystem::openTextForReadAutodetect(absPath);
    if (FileSystem::lastResult() != FSResult::OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not read file '{}'.", path));
        return;
    }

    // Set line and size limits.
    u32 lineOffset = 1;
    u32 lineLimit = 2000;
    u32 sizeLimit = 50000;
    const json::Node& offsetArg = arguments.get("offset");
    if (offsetArg.isValid()) {
        lineOffset = (u32) offsetArg.getNumber();
    }
    const json::Node& limitArg = arguments.get("limit");
    if (limitArg.isValid()) {
        lineLimit = (u32) limitArg.getNumber();
    }

    // Collect the desired file range into a single response to minimize mutex overhead.
    MemStream response;
    u32 lineNum = 0;
    u32 linesOutput = 0;
    while (StringView line = readLine(in)) {
        lineNum++;
        if (lineNum < lineOffset)
            continue;
        response.write(line.left(sizeLimit));
        linesOutput++;
        if (linesOutput >= lineLimit)
            break;
        if (line.numBytes() >= sizeLimit)
            break;
        sizeLimit -= line.numBytes();
    }
    String responseText = response.moveToString();
    if (responseText) {
        toolCtx->appendResponse(toolCall, responseText);
    }
}

Owned<ToolDefinition> createReadTool() {
    Owned<ToolDefinition> readTool = Heap::create<ToolDefinition>();
    readTool->name = "read";
    readTool->readOnly = true;
    readTool->description =
        "Read the contents of a file. For text files, output is truncated to 2000 lines or 50KB (whichever is hit "
        "first). Use offset/limit for large files. When you need the full file, continue with offset until "
        "complete.";
    readTool->parameters.append();
    readTool->parameters.back().name = "path";
    readTool->parameters.back().description = "Path to the file to read (relative or absolute)";
    readTool->parameters.back().type = "string";
    readTool->parameters.back().required = true;
    readTool->parameters.append();
    readTool->parameters.back().name = "offset";
    readTool->parameters.back().description = "Line number to start reading from (1-indexed)";
    readTool->parameters.back().type = "number";
    readTool->parameters.append();
    readTool->parameters.back().name = "limit";
    readTool->parameters.back().description = "Maximum number of lines to read";
    readTool->parameters.back().type = "number";
    readTool->handler = readToolHandler;
    return readTool;
}

//                  ▄▄  ▄▄
//  ▄▄    ▄▄ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ██ ██ ██ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

void writeToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Validate content argument.
    const json::Node& contentArg = arguments.get("content");
    if (!contentArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'content' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    String absPath = toolCtx->checkPathPermission(path, true);
    if (!absPath) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Create missing parent directories only when each new directory is writable.
    String parent = splitPath(absPath).directory;
    for (String missing = parent; FileSystem::exists(missing) == ExistsResult::NotFound;) {
        if (!toolCtx->checkPathPermission(missing, true)) {
            toolCtx->appendResponse(toolCall, "Error: Permission denied for a missing parent directory.");
            return;
        }
        String next = splitPath(missing).directory;
        if (next == missing)
            break;
        missing = std::move(next);
    }
    FSResult parentResult = FileSystem::makeDirs(parent);
    if (parentResult != FSResult::OK && parentResult != FSResult::AlreadyExists) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not create parent directory for '{}'.", path));
        return;
    }

    // Save file.
    StringView content = contentArg.text();
    FSResult fsResult = FileSystem::saveText(absPath, content);
    if (fsResult == FSResult::OK) {
        toolCtx->appendResponse(toolCall,
                                String::format("Successfully wrote {} bytes to '{}'.", content.numBytes(), path));
    } else {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not write to '{}'.", path));
    }
}

Owned<ToolDefinition> createWriteTool() {
    Owned<ToolDefinition> writeTool = Heap::create<ToolDefinition>();
    writeTool->name = "write";
    writeTool->readOnly = false;
    writeTool->description = "Write content to a file. Creates the file if it doesn't exist, overwrites if it "
                             "does. Automatically creates parent directories.";
    writeTool->parameters.append();
    writeTool->parameters.back().name = "path";
    writeTool->parameters.back().description = "Path to the file to write (relative or absolute)";
    writeTool->parameters.back().type = "string";
    writeTool->parameters.back().required = true;
    writeTool->parameters.append();
    writeTool->parameters.back().name = "content";
    writeTool->parameters.back().description = "Content to write to the file";
    writeTool->parameters.back().type = "string";
    writeTool->parameters.back().required = true;
    writeTool->handler = writeToolHandler;
    return writeTool;
}

//  ▄▄▄  ▄▄         ▄▄             ▄▄ ▄▄
//   ██  ▄▄  ▄▄▄▄  ▄██▄▄        ▄▄▄██ ▄▄ ▄▄▄▄▄
//   ██  ██ ▀█▄▄▄   ██         ██  ██ ██ ██  ▀▀
//  ▄██▄ ██  ▄▄▄█▀  ▀█▄▄ ▄▄▄▄▄ ▀█▄▄██ ██ ██
//

void listDirToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    String absPath = toolCtx->checkPathPermission(path, false);
    if (!absPath) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // List directory.
    Array<DirectoryEntry> entries = FileSystem::listDir(absPath);
    if (FileSystem::lastResult() != FSResult::OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not list '{}'.", path));
        return;
    }

    // Sort alphabetically.
    sort(entries, [](const DirectoryEntry& a, const DirectoryEntry& b) {
        if (a.isDir != b.isDir) {
            return a.isDir > b.isDir; // directories first
        }
        return a.name < b.name;
    });

    // Collect all directory entries into a single response to minimize mutex overhead.
    MemStream response;
    for (const DirectoryEntry& entry : entries) {
        if (entry.isDir) {
            response.format("{}\n", entry.name);
        } else {
            response.format("{} ({} bytes)\n", entry.name, entry.fileSize);
        }
    }
    String responseText = response.moveToString();
    if (responseText) {
        toolCtx->appendResponse(toolCall, responseText);
    }
}

Owned<ToolDefinition> createListDirTool() {
    Owned<ToolDefinition> listDirTool = Heap::create<ToolDefinition>();
    listDirTool->name = "list_dir";
    listDirTool->readOnly = true;
    listDirTool->description = "List the contents of a directory. Shows files with their size in bytes and "
                               "subdirectories with a trailing '/'.";
    listDirTool->parameters.append();
    listDirTool->parameters.back().name = "path";
    listDirTool->parameters.back().description =
        "Relative or absolute path to the directory to list, inside one of the allowed directory roots";
    listDirTool->parameters.back().type = "string";
    listDirTool->parameters.back().required = true;
    listDirTool->handler = listDirToolHandler;
    return listDirTool;
}

//    ▄▄▄ ▄▄            ▄▄       ▄▄                ▄▄▄ ▄▄ ▄▄▄
//   ██   ▄▄ ▄▄▄▄▄   ▄▄▄██       ▄▄ ▄▄▄▄▄         ██   ▄▄  ██   ▄▄▄▄   ▄▄▄▄
//  ▀██▀▀ ██ ██  ██ ██  ██       ██ ██  ██       ▀██▀▀ ██  ██  ██▄▄██ ▀█▄▄▄
//   ██   ██ ██  ██ ▀█▄▄██ ▄▄▄▄▄ ██ ██  ██ ▄▄▄▄▄  ██   ██ ▄██▄ ▀█▄▄▄   ▄▄▄█▀
//

// Simple glob matching: supports * wildcard matching any substring
static bool globMatches(StringView pattern, StringView name) {
    s32 wildCardPos = pattern.find("*");
    if (wildCardPos < 0) {
        // No wildcards. Name must match exactly.
        return (pattern == name);
    } else if (wildCardPos > 0) {
        // There are wildcards, but not at the beginning.
        // Make sure the prefixes match.
        if (!name.startsWith(pattern.left(wildCardPos)))
            return false;
        name = name.substr(wildCardPos);
        // Advanced to the part after the wildcard.
    }

    // We've found the first * in the input pattern and trimmed the prefix from the input name.
    // Loop over the rest of the pattern.
    for (;;) {
        PLY_ASSERT(pattern[wildCardPos] == '*');
        // Advance to the next non-wildcard character in the input pattern.
        do {
            wildCardPos++;
        } while ((numericCast<u32>(wildCardPos) < pattern.numBytes()) && (pattern[wildCardPos] == '*'));
        // Trim the prefix from the input pattern.
        pattern = pattern.substr(wildCardPos);
        // If the pattern is now empty, that means the pattern ended with *,
        // which means that the rest of the input name always matches.
        if (pattern.isEmpty())
            return true;

        // Find next wildcard character.
        wildCardPos = pattern.find("*");
        if (wildCardPos < 0) {
            // No more wildcard characters. Make sure the input name ends with the remainder of the pattern.
            return name.endsWith(pattern);
        } else if (wildCardPos > 0) {
            // Wildcard found. Find the intermediate segment in the input name.
            s32 index = name.find(pattern.left(wildCardPos));
            if (index < 0)
                return false; // Not found

            // Found. Trim the input name to the part after the intermediate segment.
            name = name.substr(index + wildCardPos);
        }
    }
}

struct GitIgnoreContents {
    struct Item {
        bool exclude = true;
        String pattern;
    };

    String absRoot;
    Array<Item> items;
};

// Loads the .gitignore file for the specified directory.
// Returns an empty object if no .gitignore file found.
GitIgnoreContents loadGitIgnoreForDirectory(ToolContext* toolCtx, StringView absDirPath) {
    PLY_ASSERT(isAbsolutePath(absDirPath));

    String gitIgnorePath = toolCtx->checkPathPermission(joinPath(absDirPath, ".gitignore"), false);
    if (!gitIgnorePath)
        return {};
    String text = FileSystem::loadTextAutodetect(gitIgnorePath);
    if (!text)
        return {};

    // Load file contents
    GitIgnoreContents contents;
    contents.absRoot = absDirPath;

    ViewStream stream{StringView{text}};
    for (;;) {
        StringView trimmed = readLine(stream).trim();
        if (stream.atEof)
            break;

        // If line is empty or a comment, continue.
        if (trimmed.isEmpty())
            continue;
        if (trimmed.startsWith("#"))
            continue;

        // Add pattern.
        GitIgnoreContents::Item item;
        if (trimmed.startsWith("!")) {
            item.exclude = false;
            item.pattern = trimmed.substr(1);
        } else {
            item.exclude = true;
            item.pattern = trimmed;
        }
        if (item.pattern) {
            contents.items.append(std::move(item));
        }
    }

    return contents;
}

// Returns an array of .gitignore file contents from all ancestor directories.
Array<GitIgnoreContents> loadAllAncestorGitIgnoreFiles(ToolContext* toolCtx, StringView absDirPath) {
    PLY_ASSERT(isAbsolutePath(absDirPath));
    Array<GitIgnoreContents> result;

    String currentDir = absDirPath;
    for (;;) {
        // Walk up to the parent directory.
        SplitPath sp = splitPath(currentDir);
        if (sp.directory.isEmpty() || sp.directory == currentDir)
            break; // Reached the file system root.
        currentDir = sp.directory;

        GitIgnoreContents contents = loadGitIgnoreForDirectory(toolCtx, currentDir);
        if (contents.items) {
            result.append(std::move(contents));
        }
    }

    return result;
}

bool isIgnored(const GitIgnoreContents& gitIgnore, StringView absPath, bool isDir) {
    String relPath = makeRelativePath(gitIgnore.absRoot, absPath);
    bool ignore = false;
    for (const GitIgnoreContents::Item& item : gitIgnore.items) {
        if (matchGitIgnorePattern(relPath, isDir, item.pattern)) {
            if (item.exclude) {
                ignore = true;
            } else {
                ignore = false;
            }
        }
    }
    return ignore;
}

bool isIgnored(ArrayView<const GitIgnoreContents> ignoreLists, StringView absPath, bool isDir) {
    for (const GitIgnoreContents& gitIgnore : ignoreLists) {
        if (isIgnored(gitIgnore, absPath, isDir))
            return true;
    }
    return false;
}

struct FindInFiles {
    ToolContext* toolCtx = nullptr;
    Transcript::Message* toolCall = nullptr;
    Array<GitIgnoreContents> ignoreLists;
    StringView glob;
    StringView text;
    String root;
};

void findInFiles(FindInFiles& findInfo, StringView absPath, bool isDir) {
    // Stop promptly when the caller cancels a recursive search.
    if (findInfo.toolCtx->isCanceled())
        return;
    if (isIgnored(findInfo.ignoreLists, absPath, isDir))
        return;

    if (isDir) {
        // Load .gitignore file for this directory.
        bool pushedGitIgnore = false;
        GitIgnoreContents contents = loadGitIgnoreForDirectory(findInfo.toolCtx, absPath);
        if (!contents.items.isEmpty()) {
            findInfo.ignoreLists.append(std::move(contents));
            pushedGitIgnore = true;
        }

        // Iterate over all directory entries.
        for (const DirectoryEntry& entry : FileSystem::listDir(absPath)) {
            findInFiles(findInfo, joinPath(absPath, entry.name), entry.isDir);
        }

        if (pushedGitIgnore) {
            findInfo.ignoreLists.pop();
        }
    } else {
        if (!globMatches(findInfo.glob, splitPath(absPath).filename))
            return;

        // Check file contents.
        String content = FileSystem::loadTextAutodetect(absPath);
        ViewStream stream{StringView{content}};
        u32 lineNum = 0;
        String relPath = makeRelativePath(findInfo.root, absPath);
        while (true) {
            StringView line = readLine(stream);
            if (line.isEmpty())
                break;
            lineNum++;
            if (line.find(findInfo.text) >= 0) {
                findInfo.toolCtx->appendResponse(findInfo.toolCall,
                                                 String::format("{}({}):{}\n", relPath, lineNum, line.trimRight()));
            }
        }
    }
}

void findInFilesToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate arguments.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }
    const json::Node& globArg = arguments.get("glob");
    if (!globArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'glob' argument is required.");
        return;
    }
    const json::Node& textArg = arguments.get("text");
    if (!textArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'text' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    String absPath = toolCtx->checkPathPermission(path, false);
    if (!absPath) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Check that the search path exists.
    if (FileSystem::exists(absPath) == ExistsResult::NotFound) {
        toolCtx->appendResponse(toolCall, String::format("Error: Path '{}' does not exist.", path));
        return;
    }

    // Initialize FindInFiles struct.
    FindInFiles findInfo;
    findInfo.toolCtx = toolCtx;
    findInfo.toolCall = toolCall;
    findInfo.ignoreLists = loadAllAncestorGitIgnoreFiles(toolCtx, absPath);
    findInfo.glob = globArg.text();
    findInfo.text = textArg.text();
    findInfo.root = absPath;

    findInFiles(findInfo, absPath, FileSystem::isDir(absPath));
}

Owned<ToolDefinition> createFindInFilesTool() {
    Owned<ToolDefinition> findInFilesTool = Heap::create<ToolDefinition>();
    findInFilesTool->name = "find_in_files";
    findInFilesTool->readOnly = true;
    findInFilesTool->description = "Search for text inside files matching a glob pattern in a directory tree. "
                                   "Returns matching lines in 'path(line):content' format. The glob pattern "
                                   "supports '*' as a wildcard matching any substring (case sensitive).";
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "path";
    findInFilesTool->parameters.back().description =
        "Starting directory for the search (relative or absolute path inside one of the allowed directory roots)";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "glob";
    findInFilesTool->parameters.back().description =
        "Wildcard pattern for filenames. Supports '*' to match any substring (case sensitive)";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "text";
    findInFilesTool->parameters.back().description = "The exact text to search for inside each file";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->handler = findInFilesToolHandler;
    return findInFilesTool;
}

//             ▄▄ ▄▄  ▄▄
//   ▄▄▄▄   ▄▄▄██ ▄▄ ▄██▄▄
//  ██▄▄██ ██  ██ ██  ██
//  ▀█▄▄▄  ▀█▄▄██ ██  ▀█▄▄
//

void editToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Validate edits argument.
    const json::Node& editsArg = arguments.get("edits");
    if (!editsArg.isArray()) {
        toolCtx->appendResponse(toolCall, "Error: 'edits' argument is required and must be an array.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    String absPath = toolCtx->checkPathPermission(path, true);
    if (!absPath) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Load file contents.
    String text = FileSystem::loadTextAutodetect(absPath);
    if (FileSystem::lastResult() != FSResult::OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not read file '{}'.", path));
        return;
    }

    // Collect all edit positions against the original text.
    struct EditPos {
        s32 start;
        s32 end;
        String newText;
    };
    Array<EditPos> editPositions;

    for (const json::Node& jEdit : editsArg.arrayView()) {
        if (!jEdit.isObject()) {
            toolCtx->appendResponse(toolCall, "Error: Each edit must be an object with 'oldText' and 'newText'.");
            return;
        }
        const json::Node& jOldText = jEdit.get("oldText");
        const json::Node& jNewText = jEdit.get("newText");
        if (!jOldText.isText() || !jNewText.isText()) {
            toolCtx->appendResponse(toolCall, "Error: Each edit must have 'oldText' (string) and 'newText' (string).");
            return;
        }

        StringView oldText = jOldText.text();
        StringView newText = jNewText.text();

        // Find position in original text.
        s32 pos = text.find(oldText);
        if (pos < 0) {
            toolCtx->appendResponse(toolCall, String::format("Error: Could not find '{}' in '{}'.", oldText, path));
            return;
        }

        // Check uniqueness.
        s32 secondPos = text.find(oldText, pos + oldText.numBytes());
        if (secondPos >= 0) {
            toolCtx->appendResponse(
                toolCall, String::format("Error: '{}' appears multiple times in '{}'. Use a more unique oldText.",
                                         oldText, path));
            return;
        }

        // Check for overlap with already-scheduled edits.
        for (const EditPos& ep : editPositions) {
            if (pos < ep.end && pos + (s32) oldText.numBytes() > ep.start) {
                toolCtx->appendResponse(toolCall,
                                        String::format("Error: Edit for '{}' overlaps with another edit.", oldText));
                return;
            }
        }

        editPositions.append({pos, pos + (s32) oldText.numBytes(), String{newText}});
    }

    // Sort edits by position descending so replacements don't invalidate earlier positions.
    sort(editPositions, [](const EditPos& a, const EditPos& b) { return a.start > b.start; });

    // Apply edits.
    String mutableText = std::move(text);
    for (const EditPos& ep : editPositions) {
        mutableText = mutableText.left(ep.start) + ep.newText + mutableText.substr(ep.end);
    }

    // Save file.
    FSResult fsResult = FileSystem::saveText(absPath, mutableText);
    if (fsResult == FSResult::OK) {
        toolCtx->appendResponse(toolCall, String::format("Successfully edited '{}' with {} replacement(s).", path,
                                                         editPositions.numItems()));
    } else {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not write to '{}'.", path));
    }
}

Owned<ToolDefinition> createEditTool() {
    Owned<ToolDefinition> editTool = Heap::create<ToolDefinition>();
    editTool->name = "edit";
    editTool->readOnly = false;
    editTool->description = "Edit a single file using exact text replacement. Every edits[].oldText must match a "
                            "unique, non-overlapping region of the original file. If two changes affect the same "
                            "block or nearby lines, merge them into one edit instead of emitting overlapping "
                            "edits. Do not include large unchanged regions just to connect distant changes.";
    editTool->parameters.append();
    editTool->parameters.back().name = "path";
    editTool->parameters.back().description = "Path to the file to edit (relative or absolute)";
    editTool->parameters.back().type = "string";
    editTool->parameters.back().required = true;
    editTool->parameters.append();
    editTool->parameters.back().name = "edits";
    editTool->parameters.back().description =
        "One or more targeted replacements. Each edit is matched against the original file, not incrementally. "
        "Do not include overlapping or nested edits. If two changes touch the same block or nearby lines, merge "
        "them into one edit instead.";
    editTool->parameters.back().type = "array";
    editTool->parameters.back().required = true;
    editTool->handler = editToolHandler;
    return editTool;
}

#endif // !PLY_AGENT_TRANSCRIPT_ONLY

} // namespace ply
