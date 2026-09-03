`ply-agent.h`: Agent Harness
============================

`ply-agent.h` defines a C++ API for interacting with AI agents. Applications can create `Agent` objects that communicate with remote inference providers and have access to tools running on the local filesystem.

Using `ply-agent.h` requires linking your project with [`libcurl`](https://curl.se/libcurl/). Instructions for installing `libcurl` can be found in the documentation for [building and running the `agent` sample](/docs/apps/agent.md#installing-libcurl).

The steps for interacting with agents are as follows:

1. Create a new `Transcript` object containing the user's prompt.
2. Create a new `Agent` object, passing in the `Transcript`, the desired inference provider and a set of tools for the agent to use.
   The agent starts running in a background thread.
3. Receive `TranscriptEvent` objects back from the `Agent`.
4. As each event comes in, call `applyTranscriptEvent` and perform any application-specific handling.
5. When `Agent::isWorking()` returns `false`, the conversation has ended and the `Agent` can be destroyed without missing any events.

To send a followup prompt, create a new `Transcript` object, assign the previous `Transcript` as its parent, then create another `Agent` object.

## `Transcript`

A transcript consists of a sequence of turns, and each turn consists of a sequence of messages. These concepts are represented by the `Transcript`, `Transcript::Turn` and `Transcript::Message` classes.

Each message in the transcript is associated with a `Transcript::Role`, which can have any of the following values:

| |
| --- |
| `User` |
| `AgentThinking` |
| `Agent` |
| `ToolCall` |
| `Error` |

Every `Transcript` object holds a reference to a parent `Transcript` object, allowing you to link them together into a graph. Each node in this graph corresponds to a followup prompt sent by the user.

## `Agent`

The `Agent` class represents an agent running in a background thread. As the agent runs, it generates `TranscriptEvent`s, which are buffered internally until the application calls `pollForEvents`, `waitForEvents` or `waitForCompletion`. Only one thread is allowed to call `pollForEvents`, `waitForEvents` or `waitForCompletion` at a time on a given `Agent`.

You can destroy an `Agent` at any time so long as no other threads are using it. A running agent is immediately canceled.

`Agent::Agent(const Agent::Settings& settings)`
> Constructor. The agent starts running in a background thread. `Agent::Settings` has the following data members:
>
> | | |
> | --- | --- |
> | `const Transcript* startTranscript` | The transcript used to start the agent. |
> | `EndPoint endPoint` | Identifies the inference provider, protocol and model. |
> | `ToolSet toolSet` | Specifies the system prompt, working directory and available tools. |
> | `bool enableHttpLog` | Enables HTTP-level logging (for debugging purposes). Default is `false`. |
>
> `ToolSet` has the following data members:
>
> | | |
> | --- | --- |
> | `String systemPrompt` | The system prompt passed to the agent. |
> | `Set<Owned<Handler>> handlers` | The tool handlers available to the agent. |
> | `String workingDirectory` | The agent's working directory. |

`Array<TranscriptEvent> Agent::pollForEvents()`
> Returns all currently buffered events without waiting, or an empty array if no events are buffered.

`Array<TranscriptEvent> Agent::waitForEvents(s32 maxTimeInMillis = -1)`
> Waits until at least one event is available, then returns all buffered events. A negative argument waits indefinitely.

`Array<TranscriptEvent> Agent::waitForCompletion(s32 maxTimeInMillis = -1)`
> Waits until the agent stops working or the time limit is reached, then returns all buffered events. A negative argument waits indefinitely.

`bool Agent::isWorking()`
> If `true`, the agent can still return more events. `false` means the agent has finished running, the buffer is empty and no more events will arrive.
> This function can be called by any thread at any time.

`void Agent::cancel()`
> Stops running the agent. No new events will be generated after this function returns, but any events already buffered remain available for consumption.
> This function can be called by any thread at any time.
> If another thread is waiting inside `waitForEvents` or `waitForCompletion`, that thread will immediately return.
>
> If `cancel` is called while a tool call is running in the background, the tool call might not stop immediately. Tool calls can continue running briefly after `cancel` returns, but they'll be stopped as soon as possible and won't generate any further `TranscriptEvent`s.

### `TranscriptEvent`

Transcript changes are received as a stream of `TranscriptEvent` objects. The agent never modifies the original `Transcript` object directly; instead, the application must call `applyTranscriptEvent` for each event it receives.

`void applyTranscriptEvent(Transcript* transcript, const TranscriptEvent& event)`
> Modifies `transcript` by applying the given `event`.

Applications are free to perform additional application-specific handling in response to each event. To facilitate this, `TranscriptEvent` exposes the following data members:

| | |
| --- | --- |
| `s64 timeStamp` | The time when the event was created, expressed as a Unix timestamp in microseconds. |
| `Operation operation` | The kind of change represented by the event. |
| `Transcript::Role role` | The role of the message started by `BeginMessage`. Unused by other operations. |
| `u32 toolCallID` | The index of a tool call within the current transcript. |
| `String providerToolCallID` | The inference provider's identifier for a tool call. Used internally. |
| `String text` | The content carried by `AppendText`, `AppendToolResponse` or `AppendProviderOutputItem` events. |

`TranscriptEvent::Operation` can have any of the following values:

| | |
| --- | --- |
| `NoOperation` | Makes no change. |
| `BeginMessage` | Starts a message with the specified `role`, finalizing the preceding message if necessary. |
| `AppendText` | Appends `text` to the current message. |
| `AppendToolResponse` | Appends `text` to the response for the tool call identified by `toolCallID`. |
| `EndToolResponse` | Finalizes the response for the tool call identified by `toolCallID`. |
| `AppendProviderOutputItem` | Preserves an opaque provider output item for use when replaying the transcript as context. |
| `EndTurn` | Finalizes the current message and appends an empty turn for subsequent messages. |

## Adding Tools

The tools available to an agent are defined by filling in `ToolSet::handlers`.

Several built-in tools are available. To add them to the `ToolSet`, call any of the following functions. Each function adds a new `ToolSet::Handler` and returns a pointer to it.

| Function name | Tool name | Description |
| --- | --- | --- |
| `addShellTool` | `shell` | Runs a command using the system shell. Not available on iOS. |
| `addReadTool` | `read` | Reads part or all of a file. |
| `addWriteTool` | `write` | Creates or overwrites a file. |
| `addListDirTool` | `list_dir` | Lists the contents of a directory. |
| `addFindInFilesTool` | `find_in_files` | Searches for text in a directory tree. |
| `addEditTool` | `edit` | Edits a file using exact text replacements. |

`ToolSet::Handler` has the following data members:

| | |
| --- | --- |
| `String name` | Tool name as presented to the agent. |
| `String description` | A description that tells the agent when and how to use the tool. |
| `Array<Parameter> parameters` | Describes the JSON parameters accepted by the tool. |
| `Functor<void(ToolContext*, Transcript::Message*, const json::Node&)> handler` | The internal callback invoked when the agent requests the tool. |
| `Array<String> permittedDirectories` | Directories that the tool is permitted to access.  |

When a tool handler is added, its `permittedDirectories` is initially empty. The application can add directories before creating the agent. All built-in tools currently enforce these permissions except the `shell` tool, which should be used with caution; ideally in a sandboxed environment. (Note: An auto-approve mode for the `shell` tool is planned.)

### Defining Custom Tools

In addition to the built-in tools, applications are free to create their own tools to integrate more closely with agents. For example, a tool to count the number of bytes in a string can be implemented as follows.

```
void byteCountToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall,
                          const json::Node& arguments) {
    // Validate the argument.
    const json::Node& textArg = arguments.get("text");
    if (!textArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'text' argument is required.");
        return;
    }

    // Add response text to the transcript.
    toolCtx->appendResponse(toolCall, String::format("{} bytes", textArg.text().numBytes()));
}

void addByteCountTool(ToolSet* toolSet) {
    // Describe the tool and its arguments.
    Owned<ToolSet::Handler> tool = Heap::create<ToolSet::Handler>();
    tool->name = "byte_count";
    tool->description = "Return the length of a string in bytes.";
    ToolSet::Parameter& textParam = tool->parameters.append();
    textParam.name = "text";
    textParam.description = "Text to measure";
    textParam.type = "string";
    textParam.required = true;
    tool->handler = byteCountToolHandler;
    toolSet->handlers.insertItem(std::move(tool));
}
```

Each time a tool is invoked, it receives a `ToolContext` object. `ToolContext` has the following data members:

| | |
| --- | --- |
| `bool canceled` | Set to `true` when the agent is destroyed or `cancel` is called. |
| `ArrayView<const String> permittedDirectories` | The permitted directories for this tool. |
| `StringView workingDirectory` | The agent's working directory. |

Long-running tools should check the `canceled` flag periodically and stop running if set to `true`.

To add text to the response, tools should call `ToolContext::appendResponse`.

`void ToolContext::appendResponse(Transcript::Message* toolCall, StringView text)`
> Add text to the tool response in a thread-safe manner. Can be called more than once to stream a response. Each call to `appendResponse` creates a new `TranscriptEvent` and buffers it in the `Agent` so that the application receives it immediately. The complete response won't be sent to the remote provider until the next turn.
