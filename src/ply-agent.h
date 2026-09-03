/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Agent Harness                                         │
│               Documentation: docs/high-level/agent-harness.md       │
│                                                                     │
└────────────────────────────────────────────────────────────────────*/

#pragma once
#include "ply-reflect.h"

// Configure PLY_AGENT_TRANSCRIPT_ONLY=1 if you just need the Transcript/TranscriptEvent definitions.
// This disables all code for running a local agent.
#if !defined(PLY_AGENT_TRANSCRIPT_ONLY)
#define PLY_AGENT_TRANSCRIPT_ONLY 0
#endif

namespace ply {

//  ▄▄▄▄▄▄                                          ▄▄         ▄▄
//    ██   ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄  ▄██▄▄
//    ██   ██  ▀▀  ▄▄▄██ ██  ██ ▀█▄▄▄  ██    ██  ▀▀ ██ ██  ██  ██
//    ██   ██     ▀█▄▄██ ██  ██  ▄▄▄█▀ ▀█▄▄▄ ██     ██ ██▄▄█▀  ▀█▄▄
//                                                     ██

struct Transcript : RefCounted<Transcript> {
    enum class Role {
        None,
        User,
        AgentThinking,
        Agent,
        ToolCall, // Format: `write{"path":"foo.txt","content":"Hello world!\n"}`
        Error,
    };

    // Internal type used to store a growing text buffer as a sequence of lines.
    struct Buffer {
        static constexpr u32 TailChunkSize = 256;

        Array<String> lines; // Completed lines. Each one ends with \n except possibly the last.
        MemStream tail{TailChunkSize}; // Accumulates the last line until a \n is received.
        PLY_DECLARE_TYPE_INFO(Transcript::Buffer)

        void append(StringView text); // Appends streamed text while preserving completed lines.
        void flush(); // Moves the unfinished tail into lines.
        String toString() const; // Returns all lines and the unfinished tail as contiguous text.
    };

    struct Message {
        u64 timeStamp = 0;
        Role role = Role::None;
        Buffer content; // Tail is flushed when the message ends.
        // These members are only used by ToolCall:
        String providerToolCallID; // ID used to pair the tool call with its response at the endpoint.
        Buffer toolResponse;       // Tail is flushed when the tool response ends.
        bool toolEnded = false;

        PLY_DECLARE_TYPE_INFO(Transcript::Message)
    };

    struct Turn {
        Array<Owned<Message>> messages;
        // Opaque provider output items used when replaying manually managed context.
        Array<String> providerOutputItems;

        PLY_DECLARE_TYPE_INFO(Transcript::Turn)
    };

    // Transcript data members.
    Reference<Transcript> parent;
    Array<Turn> turns;

    PLY_DECLARE_TYPE_INFO(Transcript)
};

//-------------------------------------------
// TranscriptEvent represents a change to a Transcript.
// These events are streamed to the client thread via Agent::Impl::pendingEvents.
//-------------------------------------------
struct TranscriptEvent {
    enum Operation {
        NoOperation,
        BeginMessage,             // Requires role and toolCallID (if ToolCall)
        AppendText,               // Requires text
        AppendToolResponse,       // Requires toolCallID and text
        EndToolResponse,          // Requires toolCallID
        AppendProviderOutputItem, // Requires text
        EndTurn,
    };

    s64 timeStamp = 0;
    Operation operation = NoOperation;
    // role is only used by BeginMessage. If ToolCall, toolCallID is also required.
    Transcript::Role role = Transcript::Role::None;
    // toolCallID is only used by BeginMessage(ToolCall), AppendToolResponse and EndToolResponse.
    // The first tool call written to a turn gets toolCallID=1, the next one 2, and so on.
    u32 toolCallID = 0;
    // providerToolCallID is only used by BeginMessage(ToolCall).
    String providerToolCallID;
    // text is used by AppendText, AppendToolResponse and AppendProviderOutputItem.
    String text;

    PLY_DECLARE_TYPE_INFO(TranscriptEvent)
};

// Applies a streamed event directly to the supplied transcript.
void applyTranscriptEvent(Transcript* transcript, const TranscriptEvent& event);

#if !PLY_AGENT_TRANSCRIPT_ONLY

//   ▄▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄
//          ▄▄▄█▀

// Determines the format of messages sent and received in the endpoint's underlying protocol.
enum class Protocol {
    Completions,
    Responses,
    Anthropic,
};

// Describes an inference provider to connect to.
struct EndPoint {
    String url;
    String apiKeyEnv;
    Protocol protocol = Protocol::Completions;
    String model;
};

// Forward declaration.
struct ToolContext;

// ToolSet defines the agent's available tools and its system prompt.
struct ToolSet {
    struct Parameter {
        String name;
        String description;
        String type;
        bool required = false;
    };

    struct Handler {
        String name;
        String description;
        Array<Parameter> parameters;
        Functor<void(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments)> handler;
        Array<String> permittedDirectories;

        StringView getLookupKey() const {
            return this->name;
        }
    };

    String systemPrompt;
    Set<Owned<Handler>> handlers;
    String workingDirectory;
};

// Agent provides the public API for operating LLM agents.
struct Agent {
    struct Impl;

    struct Settings {
        // The agent doesn't modify startTranscript; only uses it to initialize the HTTP request.
        const Transcript* startTranscript = nullptr;
        EndPoint endPoint;
        ToolSet toolSet;
        bool enableHttpLog = false;
    };

    const Settings* settings = nullptr; // Points to the internal copy of the settings.
    Reference<Impl> impl;               // Internal details.

    // The settings are copied to the internal state.
    Agent(const Settings& settings);
    ~Agent();

    // TranscriptEvents are internally timestamped and buffered as they are received from the LLM.
    // The client thread consumes them by calling pollForEvents, waitForEvents or waitForCompletion.
    // Only one thread can call pollForEvents, waitForEvents or waitForCompletion at a time.

    // Returns immediately and consumes any available buffered TranscriptEvents.
    Array<TranscriptEvent> pollForEvents();
    // Returns as soon as there are any events available, or if the agent stopped working.
    // If maxTimeInMillis < 0, it waits for unlimited time.
    Array<TranscriptEvent> waitForEvents(s32 maxTimeInMillis = -1);
    // Doesn't return until the agent stops working, or until a time limit is reached.
    // If maxTimeInMillis < 0, it waits for unlimited time.
    Array<TranscriptEvent> waitForCompletion(s32 maxTimeInMillis = -1);

    // isWorking and cancel are short, thread-safe functions that don't block the calling thread.
    // Any thread can freely call them at any point during the Agent's lifetime.
    // When cancel is called:
    // - Any thread calling waitForEvents or waitForCompletion immediately returns.
    // - No new TranscriptEvents will be generated.
    // - isWorking will start returning false only after any remaining buffered TranscriptEvents are consumed.
    bool isWorking();
    void cancel();
};

//  ▄▄▄▄▄▄               ▄▄▄
//    ██    ▄▄▄▄   ▄▄▄▄   ██   ▄▄▄▄
//    ██   ██  ██ ██  ██  ██  ▀█▄▄▄
//    ██   ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄  ▄▄▄█▀
//

// ToolContext is used to implement tool handlers.
struct ToolContext {
    // Protects `canceled` and serializes changes to the agent's transcript
    // and serializes event buffering across the inference and tool threads.
    Mutex mutex;
    bool canceled = false; // Set by the client thread (via cancel or destruction) to request cancellation.
    Agent::Impl* agentImpl = nullptr; // Used internally.
    ArrayView<const String> permittedDirectories;
    StringView workingDirectory;

    // Main function used by tool handlers to add text to the response. It locks the mutex, appends response text
    // to the internal toolCall and creates a AppendToolResponse event for the client to consume.
    void appendResponse(Transcript::Message* toolCall, StringView text);
};

// Individual tool registration functions. Each adds a single tool and returns its handler.
#if !defined(PLY_IOS)
ToolSet::Handler* addShellTool(ToolSet* toolSet);
#endif // !defined(PLY_IOS)
ToolSet::Handler* addReadTool(ToolSet* toolSet);
ToolSet::Handler* addWriteTool(ToolSet* toolSet);
ToolSet::Handler* addListDirTool(ToolSet* toolSet);
ToolSet::Handler* addFindInFilesTool(ToolSet* toolSet);
ToolSet::Handler* addEditTool(ToolSet* toolSet);

#endif // !PLY_AGENT_TRANSCRIPT_ONLY

} // namespace ply
