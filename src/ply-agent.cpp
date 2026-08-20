/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Agent Harness                                         │
│               Documentation: /docs/high-level/agent-harness.md      │
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

struct TranscriptUpdater {
    Transcript* transcript = nullptr;
};

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

Owned<TranscriptUpdater> createTranscriptUpdater(Transcript* transcript) {
    Owned<TranscriptUpdater> updater = Heap::create<TranscriptUpdater>();
    updater->transcript = transcript;
    return updater;
}

void destroy(TranscriptUpdater* updater) {
    Heap::destroy(updater);
}

void applyTranscriptEvent(TranscriptUpdater* updater, const TranscriptEvent& event) {
    Transcript* transcript = updater->transcript;
    switch (event.operation) {
        case TranscriptEvent::BeginMessage: {
            // Ensure there is a turn to append the message to.
            if (transcript->turns.isEmpty()) {
                transcript->turns.append();
            }
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
        case TranscriptEvent::AppendText: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            PLY_ASSERT(!turn.messages.isEmpty());
            turn.messages.back()->content.append(event.text);
            break;
        }
        case TranscriptEvent::AppendToolResponse: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            // Find the toolCallID-th ToolCall message (1-based) and append response text.
            u32 id = 0;
            for (Owned<Transcript::Message>& msg : turn.messages) {
                if (msg->role == Transcript::Role::ToolCall) {
                    id++;
                    if (id == event.toolCallID) {
                        msg->toolResponse.append(event.text);
                        break;
                    }
                }
            }
            break;
        }
        case TranscriptEvent::EndToolResponse: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            Transcript::Turn& turn = transcript->turns.back();
            u32 id = 0;
            for (Owned<Transcript::Message>& msg : turn.messages) {
                if (msg->role == Transcript::Role::ToolCall) {
                    id++;
                    if (id == event.toolCallID) {
                        msg->toolResponse.flush();
                        msg->toolEnded = true;
                        break;
                    }
                }
            }
            break;
        }
        case TranscriptEvent::AppendProviderOutputItem: {
            PLY_ASSERT(!transcript->turns.isEmpty());
            transcript->turns.back().providerOutputItems.append(event.text);
            break;
        }
        case TranscriptEvent::EndTurn: {
            if (!transcript->turns.isEmpty() && transcript->turns.back().messages) {
                transcript->turns.back().messages.back()->content.flush();
            }
            // Append a new empty turn for the next round of messages.
            transcript->turns.append();
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

PLY_STRUCT_BEGIN(Transcript::Turn)
PLY_STRUCT_MEMBER(messages)
PLY_STRUCT_MEMBER(providerOutputItems)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Transcript)
PLY_STRUCT_MEMBER(turns)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(TranscriptEvent)
PLY_STRUCT_MEMBER(timeStamp)
PLY_STRUCT_MEMBER(toolCallID)
PLY_STRUCT_MEMBER(providerToolCallID)
PLY_STRUCT_MEMBER(text)
PLY_STRUCT_END()

#if !PLY_AGENT_TRANSCRIPT_ONLY

//   ▄▄▄▄                        ▄▄         ▄▄▄▄                 ▄▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄        ██  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██   ▀▀ ▀▀  ██  ██ ██ ██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄ ▄▄ ▄▄ ▄██▄ ██ ██ ██ ██▄▄█▀ ▄██▄
//          ▄▄▄█▀                                         ██

//--------------------------------------------------------
// Agent::Impl contains information shared between the main thread and an inference thread.
// The inference thread receives response data from curl, parses each line of incoming JSONL
// and generates ResponseEvents.
//
// If tools are enabled, an additional background thread is spawned to perform the
// tool requests. Tool requests are enqueued immediately as soon as they're received.
//--------------------------------------------------------
struct Agent::Impl : RefCounted<Agent::Impl> {
    Thread inferenceThread;
    Thread toolThread;

    // Settings moved into the Agent when it's constructed.
    Agent::Settings settings;

    // internalTranscript is a copy of settings.initialTranscript, but links to the same parent.
    // Modified internally, but only when toolCtx.mutex is held.
    Reference<Transcript> internalTranscript;
    // TranscriptUpdater wraps internalTranscript and is used to apply TranscriptEvents
    // to it. Used under toolCtx.mutex.
    Owned<TranscriptUpdater> updater;
    // Tracks the role of the last message begun in the current turn, so the inference
    // thread only emits a BeginMessage event when the role changes. Reset to None at
    // the start of each turn. Protected by toolCtx.mutex.
    Transcript::Role currentRole = Transcript::Role::None;

    //----------------------------------------------
    // These members are only used by the inference thread.
    // It's a convenient place for the HTTPClient response callback to access them.
    //----------------------------------------------
    Stream httpLogFile;
    MemStream lineInProgress;
    Owned<HTTPClient> httpClient; // Only used by the inference thread.
    bool anyToolCallsThisTurn = false;

    //----------------------------------------------
    // These members are shared between all threads.
    // ToolContext is a logical grouping of the variables used by tool handlers.
    // It's mainly a way to hide the rest of the agent implementation details from tool handlers.
    // Internally, ToolContext::mutex is also used to protect access to the other members here.
    //----------------------------------------------
    // toolCtx.mutex also protects access to the remaining members below.
    ToolContext toolCtx;
    // The inference thread adds tool calls to the end of pendingToolCalls.
    // The tool thread pops each tool call from the front after it's completed.
    Array<Transcript::Message*> pendingToolCalls;
    // Events are buffered here until the client thread consumes them.
    Array<TranscriptEvent> pendingEvents;
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
static void bufferEvent(Agent::Impl* impl, TranscriptEvent&& event) {
    event.timeStamp = getUnixTimestamp();
    impl->pendingEvents.append(std::move(event));
    impl->clientCondVar.wakeAll();
}

// Must be called with toolCtx.mutex held. Applies the event to the current transcript
// section (via applyTranscriptEvent) and then buffers it for delivery to the client
// thread.
static void addEvent(Agent::Impl* impl, TranscriptEvent&& event) {
    applyTranscriptEvent(impl->updater, event);
    bufferEvent(impl, std::move(event));
}

// Emits a BeginMessage event for the given role. If the role is ToolCall, toolCallID
// identifies the tool call (1-based, sequential within the current turn).
// Must be called with toolCtx.mutex held.
static void beginMessage(Agent::Impl* impl, Transcript::Role role, u32 toolCallID = 0,
                         StringView providerToolCallID = {}) {
    TranscriptEvent event;
    event.operation = TranscriptEvent::BeginMessage;
    event.role = role;
    event.toolCallID = toolCallID;
    event.providerToolCallID = providerToolCallID;
    addEvent(impl, std::move(event));
}

// Emits an AppendText event that appends to the last message in the current turn.
// Must be called with toolCtx.mutex held.
static void appendText(Agent::Impl* impl, String&& text) {
    TranscriptEvent event;
    event.operation = TranscriptEvent::AppendText;
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
        if (msg.get() == toolCall)
            return id;
    }
    return 0;
}

//  ▄▄▄▄▄                                      ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄▄ ▄▄  ▄▄  ▄▄▄▄   ▄▄▄▄  ▄██▄▄
//  ██▀▀█▄ ██▄▄██ ██  ██ ██  ██ ██▄▄██ ▀█▄▄▄   ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ▀█▄▄▄   ▄▄▄█▀  ▀█▄▄
//                    ██

// Helper function to create the message body that goes in the POST message associated with an API call.
// This is basically a serialized JSON object containing certain properties in an expected format.

// Parses a ToolCall message's content, which encodes the tool name immediately
// followed by a JSON object of arguments (e.g. read{"path":"sample.txt"}). name is set
// to the substring before the first '{'. argsOut receives the parsed JSON object.
// Returns false if no JSON object could be parsed (name still holds the prefix).
static bool parseToolCallText(const Transcript::Buffer& content, StringView& name, json::Parser::Result& argsOut) {
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
    json::Parser parser;
    parser.setErrorCallback([](const json::ParseError&) {});
    parser.setGreedy(false);
    argsOut = parser.parse({}, text.substr(brace));
    return argsOut.root.isObject();
}

String makeRequestBody(Agent::Impl* impl) {
    json::Node root{json::Node::Object{}};

    if (impl->settings.endPoint.protocol == Protocol::Completions) {
        //-------------------------------------------
        // OpenAI Chat Completions API
        //-------------------------------------------

        // model
        root.set("model", json::Node::Text{impl->settings.endPoint.model});

        // tool definitions
        if (impl->settings.toolSet.handlers.items()) {
            json::Node jTools{json::Node::Array{}};
            for (const Owned<ToolSet::Handler>& tool : impl->settings.toolSet.handlers) {
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
                for (const ToolSet::Parameter& param : tool->parameters) {
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
        if (impl->settings.toolSet.systemPrompt) {
            json::Node jMsg{json::Node::Object{}};
            jMsg.set("role", json::Node::Text{"developer"});
            jMsg.set("content", json::Node::Text{impl->settings.toolSet.systemPrompt});
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
                        json::Parser::Result parsedArgs;
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

        // store
        root.set("store", json::Node::Bool{false});

        // reasoning_effort
        root.set("reasoning_effort", json::Node::Text{"medium"});

    } else if (impl->settings.endPoint.protocol == Protocol::Responses) {
        //-------------------------------------------
        // OpenAI Responses API
        //-------------------------------------------

        // model
        root.set("model", json::Node::Text{impl->settings.endPoint.model});

        // tool definitions
        if (impl->settings.toolSet.handlers.items()) {
            json::Node jTools{json::Node::Array{}};
            for (const Owned<ToolSet::Handler>& tool : impl->settings.toolSet.handlers) {
                json::Node& jTool = jTools.array().append(json::Node::Object{});
                jTool.set("type", json::Node::Text{"function"});
                jTool.set("name", json::Node::Text{tool->name});
                jTool.set("description", json::Node::Text{tool->description});

                // tool parameters
                json::Node jParams{json::Node::Object{}};
                jParams.set("type", json::Node::Text{"object"});
                json::Node jRequired{json::Node::Array{}};
                json::Node jProps{json::Node::Object{}};
                for (const ToolSet::Parameter& param : tool->parameters) {
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
        if (impl->settings.toolSet.systemPrompt) {
            json::Node jMsg{json::Node::Object{}};
            jMsg.set("role", json::Node::Text{"developer"});
            jMsg.set("content", json::Node::Text{impl->settings.toolSet.systemPrompt});
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
                        json::Parser::Result parsedArgs;
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
                    json::Parser parser;
                    parser.setErrorCallback([](const json::ParseError&) {});
                    json::Parser::Result item = parser.parse({}, itemText);
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

    } else {
        // Invalid protocol
        PLY_ASSERT(0);
    }

    // Convert to string
    json::WriteOptions options;
    options.includeWhitespace = false;
    return json::toString(root, options);
}

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀█▄ ██▄▄██ ▀█▄▄▄  ██  ██ ██  ██ ██  ██ ▀█▄▄▄  ██▄▄██
//  ██  ██ ▀█▄▄▄   ▄▄▄█▀ ██▄▄█▀ ▀█▄▄█▀ ██  ██  ▄▄▄█▀ ▀█▄▄▄
//                       ██

void receiveLine(Agent::Impl* impl, StringView line) {
    if (impl->settings.endPoint.protocol == Protocol::Completions) {
        //-----------------------------------------------------
        // Handle Completions API response line
        //-----------------------------------------------------

        if (line.startsWith("data: ")) {
            line = line.substr(6);
        }

        // Parse json message
        json::Parser parser;
        parser.setErrorCallback([](const json::ParseError&) {});
        parser.setGreedy(false);
        json::Parser::Result result = parser.parse({}, line);

        const json::Node& jChoices = result.root.get("choices");
        for (const json::Node& jChoice : jChoices.arrayView()) {
            const json::Node& jDelta = jChoice.get("delta");
            if (!jDelta.isValid())
                continue;
            if (jDelta.get("role").text() != "assistant")
                continue;
            const json::Node& jReasoning = jDelta.get("reasoning");
            if (jReasoning.text()) {
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.canceled)
                    return;
                emitText(impl, Transcript::Role::AgentThinking, jReasoning.text());
            }
            const json::Node& jContent = jDelta.get("content");
            if (jContent.text()) {
                LockGuard<Mutex> guard{impl->toolCtx.mutex};
                if (impl->toolCtx.canceled)
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
                if (impl->toolCtx.canceled)
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
                    json::Parser argParser;
                    argParser.setErrorCallback([](const json::ParseError&) {});
                    argParser.setGreedy(false);
                    json::Parser::Result parsedArgs = argParser.parse({}, jArgs.text());
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

    } else if (impl->settings.endPoint.protocol == Protocol::Responses) {
        // Parse Responses API data events; the event type is also present in each JSON object.
        if (line.startsWith("data: ")) {
            json::Parser parser;
            parser.setErrorCallback([](const json::ParseError&) {});
            parser.setGreedy(false);
            json::Parser::Result result = parser.parse({}, line.substr(6).trim());

            if (result.root.isObject()) {
                StringView eventType = result.root.get("type").text();
                if (eventType == "response.output_text.delta" || eventType == "response.refusal.delta" ||
                    eventType == "response.reasoning_summary_text.delta") {
                    // Stream visible output, refusals and reasoning summaries into transcript messages.
                    LockGuard<Mutex> guard{impl->toolCtx.mutex};
                    if (impl->toolCtx.canceled)
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
                    if (impl->toolCtx.canceled)
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
                    if (impl->toolCtx.canceled)
                        return;
                    emitText(impl, Transcript::Role::Error, message);
                } else if (eventType == "response.output_item.done") {
                    const json::Node& jItem = result.root.get("item");
                    if (jItem.isObject()) {
                        // Preserve the complete output item for stateless conversation replay.
                        LockGuard<Mutex> guard{impl->toolCtx.mutex};
                        if (impl->toolCtx.canceled)
                            return;
                        TranscriptEvent itemEvent;
                        itemEvent.operation = TranscriptEvent::AppendProviderOutputItem;
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
                        appendText(impl,
                                   String::format("{}{}", jItem.get("name").text(), jItem.get("arguments").text()));
                        Transcript::Message* toolCallMsg = impl->internalTranscript->turns.back().messages.back().get();
                        toolCallMsg->content.flush();
                        impl->currentRole = Transcript::Role::ToolCall;
                        impl->anyToolCallsThisTurn = true;
                        impl->pendingToolCalls.append(toolCallMsg);
                        impl->toolCondVar.wakeAll();
                    }
                }
            }
        }
    }
}

//  ▄▄▄▄          ▄▄▄                                              ▄▄▄▄▄▄ ▄▄                              ▄▄
//   ██  ▄▄▄▄▄   ██    ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄        ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//   ██  ██  ██ ▀██▀▀ ██▄▄██ ██  ▀▀ ██▄▄██ ██  ██ ██    ██▄▄██       ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//  ▄██▄ ██  ██  ██   ▀█▄▄▄  ██     ▀█▄▄▄  ██  ██ ▀█▄▄▄ ▀█▄▄▄        ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

// Helper function to sanitize a URL for use in a filename
static String sanitizeUrlForFilename(StringView url) {
    MemStream result;
    for (char c : url) {
        // Only keep alphanumeric characters, dash, underscore, and dot
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.') {
            result.write(c);
        } else if (c == '/') {
            result.write('-');
        }
    }
    return result.moveToString();
}

// Delivers an error event via the pendingEvents buffer.
// Must be called with toolCtx.mutex held. Suppresses the event if the agent was
// canceled, so bufferEvent is never called once cancellation has been requested.
void onError(Agent::Impl* impl, StringView message) {
    if (impl->toolCtx.canceled)
        return;
    beginMessage(impl, Transcript::Role::Error);
    appendText(impl, String{message});
    impl->currentRole = Transcript::Role::Error;
}

// Extracts the provider's message from a JSON error envelope and falls back to the HTTP status.
static String makeHTTPErrorMessage(u32 statusCode, StringView responseBody) {
    json::Parser parser;
    parser.setErrorCallback([](const json::ParseError&) {});
    parser.setGreedy(false);
    json::Parser::Result result = parser.parse({}, responseBody.trim());
    StringView message = result.root.get("error").get("message").text();
    if (!message) {
        message = result.root.get("message").text();
    }
    return message ? String::format("HTTP response code {} from server: {}", statusCode, message)
                   : String::format("HTTP response code {} from server", statusCode);
}

// Performs an inference request and converts the response data to a queue of ResponseEvents.
// This is the bulk of the work performed by the inference thread (Agent::Impl::inferenceThread).
// The calling thread receives response data by periodically calling receiveResponseEvents.
void performInferenceRequest(Agent::Impl* impl) {
    impl->anyToolCallsThisTurn = false;
    // Reset the per-turn role tracking so the first streamed message begins a new
    // Message block.
    impl->currentRole = Transcript::Role::None;
    impl->lineInProgress = MemStream{};

    if (impl->settings.enableHttpLog) {
        // Generate filename based on current date/time and URL
        DateTime dateTime = convertToDateTime(getUnixTimestamp());
        String timestampStr = String::fromDateTime("%Y-%m-%d_%H-%M-%S", dateTime);
        String sanitizedUrl = sanitizeUrlForFilename(impl->settings.endPoint.url);
        String logFilename = String::format("llm-log_{}_{}.txt", timestampStr, sanitizedUrl);
        impl->httpLogFile = FileSystem::openBinaryForWrite(logFilename);
    }
    PLY_ON_SCOPE_EXIT({ impl->httpLogFile.close(); });

    // Make request body
    String body = makeRequestBody(impl);

    // Apply API key. It's sent as an Authorization header alongside Content-Type.
    Map<String, String> headers;
    *headers.insert("Content-Type").value = "application/json";
    {
        String apiKey = impl->settings.endPoint.getAPIKey();
        if (!apiKey) {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            onError(impl, "Missing API key");
            return;
        }
        *headers.insert("Authorization").value = String::format("Bearer {}", apiKey);
    }

    // State accumulated across curl callbacks (the callback runs on this same inference thread,
    // invoked from within HTTPClient::receiveResponse()).
    struct RequestState {
        bool gotError = false;
        u32 statusCode = 0;
        String errorMessage;
        MemStream errorBody;
    } state;

    // The callback splits the incoming response stream into JSONL lines and dispatches each one
    // to receiveLine(). It remains owned by the HTTP request until that request completes or is cancelled.
    Functor<void(const HTTPClient::Event&)> callback = [impl, &state](const HTTPClient::Event& event) {
        if (auto* headers = event.as<HTTPClient::Headers>()) {
            // Treat non-200 responses as agent request failures while still allowing HTTPClient to deliver the body.
            state.statusCode = headers->statusCode;
            state.gotError = headers->statusCode != 200;
            if (state.gotError) {
                state.errorMessage = String::format("HTTP response code {} from server", headers->statusCode);
            }

            // Log the response status and every delivered header before the response body.
            if (impl->httpLogFile.isOpen()) {
                impl->httpLogFile.format("HTTP response status: {}\n", headers->statusCode);
                for (const auto& item : headers->headers.items()) {
                    impl->httpLogFile.format("{}: {}\n", item.key, item.value);
                }
                impl->httpLogFile.write('\n');
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
            String completedLine = impl->lineInProgress.moveToString();
            impl->lineInProgress = MemStream{};
            if (completedLine) {
                receiveLine(impl, completedLine);
            }
            return;
        }
        auto* data = event.as<HTTPClient::Data>();
        if (!data)
            return;

        // Write raw HTTP response to log file
        if (impl->httpLogFile.isOpen()) {
            impl->httpLogFile.write(data->bytes);
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
                String completedLine = impl->lineInProgress.moveToString();
                receiveLine(impl, completedLine);
                impl->lineInProgress = MemStream{};
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
            if (impl->toolCtx.canceled) {
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
        state.errorMessage = makeHTTPErrorMessage(state.statusCode, state.errorBody.moveToString());
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
            if (impl->toolCtx.canceled)
                break;
            impl->inferenceCondVar.wait(guard);
        }
    }
}

void runAgentThread(Agent::Impl* impl) {
    // Iterate making inference requests until the main thread requests exit
    // or there are no more tool responses to send back.
    for (;;) {
        // Perform one inference request.
        performInferenceRequest(impl);

        {
            // Consume tool response events and check whether the loop should continue running.
            LockGuard<Mutex> guard{impl->toolCtx.mutex};

            // Has the loop ended? Either there were no tool calls this turn, or the
            // client destroyed the Agent (setting `canceled`).
            if (!impl->anyToolCallsThisTurn || impl->toolCtx.canceled)
                break; // Yes

            // No; append a new turn for the next inference request. Emit an EndTurn
            // event so the client appends a matching turn to its own copy.
            TranscriptEvent endTurn;
            endTurn.operation = TranscriptEvent::EndTurn;
            addEvent(impl, std::move(endTurn));
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
        // Stop if the client destroyed the Agent. `canceled` is protected
        // by toolCtx.mutex.
        {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            if (impl->toolCtx.canceled) {
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
        json::Parser::Result parsedArgs;
        parseToolCallText(toolCall->content, tcName, parsedArgs);
        const json::Node& arguments = parsedArgs.root;

        // Handle this tool call (no locks held).
        // Look up the handler for this tool call by name.
        const Owned<ToolSet::Handler>* found = impl->settings.toolSet.handlers.find(tcName);
        // FIXME: Improve error handling
        PLY_ASSERT(found);
        const ToolSet::Handler* toolDef = found->get();
        {
            // Set the permissions for this tool call.
            PLY_SET_IN_SCOPE(impl->toolCtx.permissions, toolDef->permissions);
            toolDef->handler(&impl->toolCtx, toolCall, arguments);
        }

        // Finalize the internal response and notify the client that the handler returned.
        {
            LockGuard<Mutex> guard{impl->toolCtx.mutex};
            toolCall->toolResponse.flush();
            toolCall->toolEnded = true;
            if (!impl->toolCtx.canceled) {
                TranscriptEvent endResp;
                endResp.operation = TranscriptEvent::EndToolResponse;
                endResp.toolCallID = toolCallIDForMessage(impl, toolCall);
                bufferEvent(impl, std::move(endResp));
            }
        }

        popHeadItem = true;
    }

    impl->decRefCount();
}

// Main function used by tool handlers to add text to the response. It locks the mutex, appends response text
// to the internal toolCall and creates a AppendToolResponse event for the client to consume.
void ToolContext::appendResponse(Transcript::Message* toolCall, StringView text) {
    LockGuard<Mutex> guard{this->mutex};
    if (!this->canceled) {
        // Append to the internal transcript while preserving completed line boundaries.
        toolCall->toolResponse.append(text);

        TranscriptEvent appendResp;
        appendResp.operation = TranscriptEvent::AppendToolResponse;
        appendResp.toolCallID = toolCallIDForMessage(this->agentImpl, toolCall);
        appendResp.text = text;
        bufferEvent(this->agentImpl, std::move(appendResp));
    }
}

//   ▄▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄
//          ▄▄▄█▀

Agent::Agent(const Settings& settings) {
    PLY_ASSERT(settings.initialTranscript);
    PLY_ASSERT(!settings.initialTranscript->turns.isEmpty());

    // Initialize new Agent.
    this->impl = Heap::create<Agent::Impl>();
    Agent::Impl* impl = this->impl;
    impl->settings = settings;        // copies the settings
    this->settings = &impl->settings; // points to the internal copy of the settings
    impl->toolCtx.agentImpl = impl;
    impl->toolCtx.workingDirectory = impl->settings.toolSet.workingDirectory;
    // The HTTPClient lives for the whole conversation and is reused across turns.
    impl->httpClient = HTTPClient::create();

    // Make a copy of the transcript leaf, but link to the same parent.
    impl->internalTranscript = Heap::create<Transcript>(*impl->settings.initialTranscript);
    // Create the TranscriptUpdater that applies events to the internal transcript.
    impl->updater = createTranscriptUpdater(impl->internalTranscript);

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
    if (impl->toolCtx.canceled)
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
    if (!impl->toolCtx.canceled) {
        impl->toolCtx.canceled = true;
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

Array<TranscriptEvent> Agent::pollForEvents() {
    Agent::Impl* impl = this->impl;
    LockGuard<Mutex> guard{impl->toolCtx.mutex};
    return std::move(impl->pendingEvents);
}

Array<TranscriptEvent> Agent::waitForEvents(s32 maxTimeInMillis) {
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
        if (impl->toolCtx.canceled || (impl->inferenceEnded && impl->toolEnded))
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

Array<TranscriptEvent> Agent::waitForCompletion(s32 maxTimeInMillis) {
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
        if (impl->toolCtx.canceled || (impl->inferenceEnded && impl->toolEnded))
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

#endif // !PLY_AGENT_TRANSCRIPT_ONLY

} // namespace ply
