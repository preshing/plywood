`ply-agent.h`: Agent Harness
============================

`ply-agent.h` defines a C++ API for interacting with AI agents. Applications create `Transcript` objects and pass them to `Agent` objects; the agent's job is to extend the transcript in a logical way. It does this by communicating with a remote inference server and running local tools.

<svg viewBox="0 0 497 243" style="display:block;width:497px;max-width:100%;height:auto;margin-inline:auto">
 <rect x="4" y="3" width="269" height="134" ry="14.8" fill="none" stroke="var(--border-table)" stroke-width="2"/>
 <path d="M81.5 124V76" fill="none" stroke="var(--text-muted)" stroke-width="1"/>
 <g fill="var(--diagram-solid-fill)" stroke="var(--border-popup)" stroke-width="1">
  <rect x="357.5" y="67.5" width="137" height="52" ry="10.6"/>
  <rect x="161.5" y="76.5" width="98" height="34" ry="6.49"/>
  <rect x="64.5" y="70.5" width="33" height="15"/>
  <rect x="64.5" y="90.5" width="33" height="15"/>
  <rect x="64.5" y="110.5" width="33" height="15"/>
  <path d="M234 197c0 3.87-10.3 7-23 7s-23-3.13-23-7v-36h46z"/>
  <ellipse cx="211" cy="161" rx="23" ry="7"/>
 </g>
 <g fill="none" stroke="var(--diagram-arrow-color)" stroke-width="1">
  <path d="m105 80.4-6.43-4.81 6.43-4.81M99.3 75.4c35-1.6 26.1 18.4 61.8 18.4"/>
  <path d="M216.3 146.9 211.5 153.3 206.7 146.9M211.5 152.7V111"/>
  <path d="m350 93.8 6.43 4.81-6.43 4.81M356 98.6h-95.7"/>
  <path d="m267 83.8-6.43 4.81 6.43 4.81M262 88.6h95.7"/>
 </g>
 <g fill="var(--type-color)" font-family="jetbrains_mono,monospace" font-size="14px" text-anchor="middle">
  <text x="210" y="96.9">ply::Agent</text>
  <text x="80.9" y="63.9">ply::Transcript</text>
 </g>
 <g fill="var(--text-secondary)" font-family="source_sans_3,sans-serif" font-size="16px" text-anchor="middle">
  <text x="139" y="19.6">Application</text>
  <text x="212" y="221">Local<tspan x="212" y="237">Filesystem</tspan></text>
  <text x="425" y="89.6">Remote<tspan x="425" y="106">Inference Server</tspan></text>
 </g>
</svg>

The steps for interacting with agents are as follows:

1. Create a new `Transcript` object containing the user's prompt.
2. Create a new `Agent` object, passing in the `Transcript`, the desired inference provider and a set of tools for the agent to use.
   The agent runs in a background thread.
3. Receive `Transcript::Event` objects back from the `Agent`.
4. As each event comes in, call `applyTranscriptEvent` and perform any application-specific handling.
5. Once `Agent::isWorking()` returns `false`, no further events will be received and the `Agent` can be safely destroyed.

To send a followup prompt, create a new `Transcript` object, assign the previous `Transcript` as its parent, then create another `Agent` object.

Using `ply-agent.h` requires linking your project with [`libcurl`](https://curl.se/libcurl/) for HTTPS support. Instructions for installing `libcurl` can be found in the documentation for [building and running the `agent` sample](/docs/apps/agent.md#installing-libcurl).

## `Transcript`

A transcript consists of a sequence of turns, with each turn consisting of a sequence of messages. These concepts are represented by the `Transcript`, `Transcript::Turn` and `Transcript::Message` classes.

Each message in the transcript is associated with a `Transcript::Role`, which can take any of the following values:

| |
| --- |
| `User` |
| `AgentThinking` |
| `Agent` |
| `ToolCall` |
| `Error` |

Every `Transcript` object holds a reference to a parent `Transcript` object, allowing you to link them together into a graph. Each node in this graph corresponds to a followup prompt sent by the user.

## `Agent`

The `Agent` class represents an agent running in a background thread. As the agent runs, it generates `Transcript::Event`s, which are buffered internally until the application calls `pollForEvents`, `waitForEvents` or `waitForCompletion`. Only one thread is allowed to call `pollForEvents`, `waitForEvents` or `waitForCompletion` at a time.

You can destroy an `Agent` at any time as long as there are no racing member function calls from other threads. If the agent is still running at destruction time, it's immediately canceled.

`Agent::Agent(const Agent::Settings& settings)`
> Constructor. The agent starts running in a background thread. `Agent::Settings` has the following data members:
>
> | | |
> | --- | --- |
> | `const Transcript* startTranscript` | The transcript used to start the agent. |
> | `Agent::EndPoint endPoint` | Identifies the inference provider, protocol and model. |
> | `Agent::Capabilities capabilities` | Specifies the system prompt, working directory and available tools. |
> | `bool enableHttpLog` | Enables HTTP-level logging (for debugging purposes). Default is `false`. |
>
> `Agent::Capabilities` has the following data members:
>
> | | |
> | --- | --- |
> | `String systemPrompt` | The system prompt passed to the agent. |
> | `Set<Owned<ToolDefinition>> tools` | The tools available to the agent. |
> | `String workingDir` | The agent's working directory. |
> | `Array<String> readableDirs` | Absolute directory paths granting recursive read access. |
> | `Array<String> writableDirs` | Absolute directory paths granting recursive write access. These paths should also be present in `readableDirs`. |

`Array<Transcript::Event> Agent::pollForEvents()`
> Returns all currently buffered events without waiting, or an empty array if no events are buffered.

`Array<Transcript::Event> Agent::waitForEvents(s32 maxTimeInMillis = -1)`
> Waits until at least one event is available, then returns all buffered events. A negative argument waits indefinitely.

`Array<Transcript::Event> Agent::waitForCompletion(s32 maxTimeInMillis = -1)`
> Waits until the agent stops working or the time limit is reached, then returns all buffered events. A negative argument waits indefinitely.

`bool Agent::isWorking()`
> If `true`, the agent can still return more events. `false` means the agent has finished running, the buffer is empty and no more events will arrive.
> This function can be called by any thread at any time.

`void Agent::cancel()`
> Stops running the agent. No new events will be generated after this function returns, but any events already buffered remain available for consumption.
> This function can be called by any thread at any time.
> If another thread is waiting inside `waitForEvents` or `waitForCompletion`, that thread will immediately return.
>
> If `cancel` is called while a tool call is running in the background, the tool call might not stop immediately. Tool calls can continue running briefly after `cancel` returns, but they'll be stopped as soon as possible and won't generate any further `Transcript::Event`s.

### `Transcript::Event`

Transcript changes are received as a stream of `Transcript::Event` objects. The agent never modifies the original `Transcript` object directly; instead, the application must call `applyTranscriptEvent` for each event it receives.

`void applyTranscriptEvent(Transcript* transcript, const Transcript::Event& event)`
> Modifies `transcript` by applying the given `event`.

Applications are free to perform additional application-specific handling in response to each event. To facilitate this, `Transcript::Event` exposes the following data members:

| | |
| --- | --- |
| `s64 timeStamp` | The time when the event was created, expressed as a Unix timestamp in microseconds. |
| `Operation operation` | The kind of change represented by the event. |
| `Transcript::Role role` | The role of the message started by `BeginMessage`. Unused by other operations. |
| `u32 toolCallID` | The index of a tool call within the current transcript. |
| `String providerToolCallID` | The inference provider's identifier for a tool call. Used internally. |
| `String text` | The content carried by `AppendText`, `AppendToolResponse` or `AppendProviderOutputItem` events. |

`Transcript::Event::Operation` can have any of the following values:

| | |
| --- | --- |
| `BeginTurn` | Appends a new turn for an inference request. |
| `BeginMessage` | Starts a message with the specified `role`, finalizing the preceding message if necessary. |
| `AppendText` | Appends `text` to the current message. |
| `AppendToolResponse` | Appends `text` to the response for the tool call identified by `toolCallID`. |
| `EndToolResponse` | Finalizes the response for the tool call identified by `toolCallID`. |
| `AppendProviderOutputItem` | Preserves an opaque provider output item for use when replaying the transcript as context. |
| `SetTokenUsage` | Stores aggregate token usage on the current turn. |
| `EndTurn` | Finalizes the current message after an inference request completes. |

For every inference request that completes or reports an error, the agent emits `EndTurn`. If tool calls require another inference request, the agent then emits `BeginTurn` before any events belonging to that request. A canceled inference is incomplete and does not receive `EndTurn`.

## Tools

The tools available to an agent are defined by filling in `Agent::Capabilities::tools`.

Several built-in tools are available. To add them to `Agent::Capabilities`, call any of the following functions. Each function returns an `Owned<ToolDefinition>` that can be inserted into `tools`.

| Function name | Tool name | Description |
| --- | --- | --- |
| `createShellTool` | `shell` | Runs a command using the system shell after authorization. Not available on iOS. |
| `createReadTool` | `read` | Reads part or all of a file. |
| `createWriteTool` | `write` | Creates or overwrites a file. |
| `createListDirTool` | `list_dir` | Lists the contents of a directory. |
| `createFindInFilesTool` | `find_in_files` | Searches for text in a directory tree. |
| `createEditTool` | `edit` | Edits a file using exact text replacements. |

`ToolDefinition` has the following data members:

| | |
| --- | --- |
| `String name` | Tool name as presented to the agent. |
| `String description` | A description that tells the agent when and how to use the tool. |
| `Array<Parameter> parameters` | Describes the JSON parameters accepted by the tool. |
| `Functor<...> handler` | The internal callback invoked when the agent uses the tool. |
| `bool readOnly` | Indicates that the tool does not modify any data. |

### `shell` Tool Permissions

There are two ways for agents to use the `shell` tool:

- With full unrestricted access to run any command on behalf of the user.
- In a restricted mode, where a second "authorizer" agent reviews each shell command to make sure it's permitted by a given policy. The "authorizer" has read-only access to the full set of directories available to the model.

To configure a `shell` tool, pass a `ShellToolSettings` instance to `createShellTool`. `ShellToolSettings` has the following data members:

| | |
| --- | --- |
| `String policy` | A natural-language description of shell commands the agent is allowed to run. |
| `Agent::EndPoint authorizerEndPoint` | The authorizer's provider, protocol and model. |
| `Set<Owned<ToolDefinition>> authorizerTools` | The tools available to the authorizer. |
| `bool unrestricted` | If `true`, all permission checks are bypassed completely. |

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

void addByteCountTool(Agent::Capabilities* capabilities) {
    // Describe the tool and its arguments.
    Owned<ToolDefinition> tool = Heap::create<ToolDefinition>();
    tool->name = "byte_count";
    tool->description = "Return the length of a string in bytes.";
    ToolDefinition::Parameter& textParam = tool->parameters.append();
    textParam.name = "text";
    textParam.description = "Text to measure";
    textParam.type = "string";
    textParam.required = true;
    tool->handler = byteCountToolHandler;
    capabilities->tools.insertItem(std::move(tool));
}
```

### `ToolContext`

Each time a tool is invoked, it receives a `ToolContext` object. `ToolContext` provides the following member functions:

`StringView ToolContext::getWorkingDirectory() const`
> Returns the agent's working directory.

`String ToolContext::checkPathPermission(StringView path, bool withWriteAccess) const`
> Converts `path` to an absolute path if read access is permitted, or write access if `withWriteAccess` is true. Otherwise returns an empty string.

`void ToolContext::appendResponse(Transcript::Message* toolCall, StringView text)`
> Adds text to the tool response in a thread-safe manner. Can be called more than once to stream a response. Each call to `appendResponse` creates a new `Transcript::Event` and buffers it in the `Agent` so that the application receives it as soon as possible. The complete tool response won't be sent to the remote inference server until the next turn.

`bool ToolContext::isCanceled() const`
> Returns whether the agent has been canceled.

`bool ToolContext::setCancelCallback(Functor<void()>&& callback)`
> If the agent hasn't already been canceled, registers a cancellation callback and returns `true`.
> Otherwise, if the agent was already canceled, clears any existing cancellation callback and returns `false`.
> The callback will be invoked from the client thread when `Agent::cancel()` is called.

`void ToolContext::clearCancelCallback()`
> Clears the registered cancellation callback.

Long-running tools should call `isCanceled()` periodically and return promptly when it becomes true. A tool blocked in an interruptible operation can use `setCancelCallback()` to register a callback that unblocks it. The callback must be cleared before the tool handler returns.
