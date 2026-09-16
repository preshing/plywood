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

// Configure PLY_AGENT_TRANSCRIPT_ONLY=1 if you just need the Transcript/Event definitions.
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

// A section of a transcript resulting from a single user prompt sent to an agent.
// Transcripts can be chained together to form a branching conversation.
struct Transcript : RefCounted<Transcript> {
    // Internal type used to store a growing text buffer as a sequence of lines.
    struct Buffer {
        static constexpr u32 TailChunkSize = 256;

        Array<String> lines;           // Completed lines. Each one ends with \n except possibly the last.
        MemStream tail{TailChunkSize}; // Accumulates the last line until a \n is received.
        PLY_DECLARE_TYPE_INFO(Buffer)

        void append(StringView text); // Appends streamed text while preserving completed lines.
        void flush();                 // Moves the unfinished tail into lines.
        String toString() const;      // Returns all lines and the unfinished tail as contiguous text.
    };

    // Each message is assigned one of the following roles.
    enum class Role {
        None,
        User,
        AgentThinking,
        Agent,
        ToolCall, // Format: `write{"path":"foo.txt","content":"Hello world!\n"}`
        Error,
    };

    // A single message in the transcript.
    struct Message {
        u64 timeStamp = 0;
        Role role = Role::None;
        Buffer content; // Tail is flushed when the message ends.
        // These members are only used by ToolCall:
        String providerToolCallID; // ID used to pair the tool call with its response at the endpoint.
        Buffer toolResponse;       // Tail is flushed when the tool response ends.
        bool toolEnded = false;

        PLY_DECLARE_TYPE_INFO(Message)
    };

    // Per-turn token usage statistics, as reported by the provider.
    struct TokenUsage {
        bool isValid = false; // False when the provider does not report statistics.
        u64 uncachedInputTokens = 0;
        u64 cachedInputTokens = 0;
        u64 outputTokens = 0;

        PLY_DECLARE_TYPE_INFO(TokenUsage)
    };

    // A chain of messages resulting from a single round-trip to the remote inference provider.
    // Turns continue as long as new tool calls keep being made.
    struct Turn {
        Array<Owned<Message>> messages;
        // Opaque provider output items used when replaying manually managed context.
        Array<String> providerOutputItems;
        // Aggregate usage reported by the provider for this inference request.
        TokenUsage tokenUsage;

        PLY_DECLARE_TYPE_INFO(Turn)
    };

    // Event represents a change to a Transcript.
    // Agents stream these events to client threads.
    struct Event {
        enum Operation {
            NoOperation,
            BeginTurn,                // Appends a turn for an inference request
            BeginMessage,             // Requires role and toolCallID (if ToolCall)
            AppendText,               // Requires text
            AppendToolResponse,       // Requires toolCallID and text
            EndToolResponse,          // Requires toolCallID
            AppendProviderOutputItem, // Requires text
            SetTokenUsage,            // Requires tokenUsage
            EndTurn,                  // Finalizes the current inference request
        };

        s64 timeStamp = 0;
        Operation operation = NoOperation;
        // role is only used by BeginMessage. If ToolCall, toolCallID is also required.
        Role role = Role::None;
        // toolCallID is only used by BeginMessage(ToolCall), AppendToolResponse and EndToolResponse.
        // The first tool call written to a turn gets toolCallID=1, the next one 2, and so on.
        u32 toolCallID = 0;
        // providerToolCallID is only used by BeginMessage(ToolCall).
        String providerToolCallID;
        // text is used by AppendText, AppendToolResponse and AppendProviderOutputItem.
        String text;
        // tokenUsage is only used by SetTokenUsage.
        TokenUsage tokenUsage;

        PLY_DECLARE_TYPE_INFO(Event)
    };

    Reference<Transcript> parent;
    Array<Turn> turns;

    PLY_DECLARE_TYPE_INFO(Transcript)
};

// Applies a streamed event directly to the supplied transcript.
void applyTranscriptEvent(Transcript* transcript, const Transcript::Event& event);

#if !PLY_AGENT_TRANSCRIPT_ONLY

struct ToolContext;

// Describes a single tool call that an agent can use.
struct ToolDefinition {
    struct Parameter {
        String name;
        String description;
        String type;
        bool required = false;
    };

    String name;
    String description;
    Array<Parameter> parameters;
    Functor<void(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments)> handler;

    StringView getLookupKey() const {
        return this->name;
    }
};

//   ▄▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██ ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██ ▀█▄▄██ ▀█▄▄▄  ██  ██  ▀█▄▄
//          ▄▄▄█▀

// Agent provides the public API for operating LLM agents.
struct Agent {
    // Determines the format of messages sent and received in the endpoint's underlying protocol.
    enum class Protocol {
        Unset,
        Completions,
        Responses,
        Anthropic,
        Interactions,
    };

    // Describes an inference provider to connect to.
    struct EndPoint {
        String provider;
        String url;
        Protocol protocol = Protocol::Unset;
        String apiKeyEnv;
        String model;
    };

    // Defines the purpose and capabilities of the agent.
    struct Capabilities {
        String systemPrompt;
        Set<Owned<ToolDefinition>> tools;
        String workingDir;
        Array<String> readableDirs;
        Array<String> writableDirs;
    };

    // Arguments passed when the Agent is created.
    struct Settings {
        const Transcript* startTranscript = nullptr;
        EndPoint endPoint;
        Capabilities capabilities;
        bool enableHttpLog = false;
    };

    const Settings* settings = nullptr; // Points to the internal copy of the settings.
    struct Impl;
    Reference<Impl> impl; // Internal details.

    // The settings are copied to the internal state.
    Agent(const Settings& settings);
    ~Agent();

    // Transcript events are internally timestamped and buffered as they are received from the LLM.
    // The client thread consumes them by calling pollForEvents, waitForEvents or waitForCompletion.
    // Only one thread can call pollForEvents, waitForEvents or waitForCompletion at a time.

    // Returns immediately and consumes any available buffered transcript events.
    Array<Transcript::Event> pollForEvents();
    // Returns as soon as there are any events available, or if the agent stopped working.
    // If maxTimeInMillis < 0, it waits for unlimited time.
    Array<Transcript::Event> waitForEvents(s32 maxTimeInMillis = -1);
    // Doesn't return until the agent stops working, or until a time limit is reached.
    // If maxTimeInMillis < 0, it waits for unlimited time.
    Array<Transcript::Event> waitForCompletion(s32 maxTimeInMillis = -1);

    // isWorking and cancel are short, thread-safe functions that don't block the calling thread.
    // Any thread can freely call them at any point during the Agent's lifetime.
    // When cancel is called:
    // - Any thread calling waitForEvents or waitForCompletion immediately returns.
    // - No new transcript events will be generated.
    // - isWorking will start returning false only after any remaining buffered transcript events are consumed.
    bool isWorking();
    void cancel();
};

//  ▄▄▄▄▄▄               ▄▄▄
//    ██    ▄▄▄▄   ▄▄▄▄   ██   ▄▄▄▄
//    ██   ██  ██ ██  ██  ██  ▀█▄▄▄
//    ██   ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄  ▄▄▄█▀
//

// Structure passed to tool handlers.
struct ToolContext {
    // The agent's working directory.
    StringView getWorkingDirectory() const;

    // Returns an absolute path if the specified access is permitted; otherwise returns an empty string.
    String checkPathPermission(StringView path, bool withWriteAccess) const;

    // Returns whether cancellation has been requested. Should be checked periodically for long-running tools.
    bool isCanceled() const;

    // Adds text to the tool response.
    void appendResponse(Transcript::Message* toolCall, StringView text);

    // If the agent has not already been canceled, registers a cancellation callback and returns true.
    // Otherwise, if the agent was already canceled, clears any existing cancellation callback and returns false.
    // The callback will be invoked from the client thread when Agent::cancel() is called.
    bool setCancelCallback(Functor<void()>&& callback);

    // Clears the cancellation callback.
    void clearCancelCallback();
};

// Individual tool registration functions.
#if !defined(PLY_IOS)
struct ShellToolSettings {
    bool unrestricted = true; // Bypasses filesystem permissions.
};
ToolDefinition* addShellTool(Agent::Capabilities* capabilities, const ShellToolSettings& settings = {});
#endif // !defined(PLY_IOS)
ToolDefinition* addReadTool(Agent::Capabilities* capabilities);
ToolDefinition* addWriteTool(Agent::Capabilities* capabilities);
ToolDefinition* addListDirTool(Agent::Capabilities* capabilities);
ToolDefinition* addFindInFilesTool(Agent::Capabilities* capabilities);
ToolDefinition* addEditTool(Agent::Capabilities* capabilities);

#endif // !PLY_AGENT_TRANSCRIPT_ONLY

} // namespace ply
