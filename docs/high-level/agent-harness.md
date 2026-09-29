`ply-agent.h`: Agent Harness
============================

`ply-agent.h` defines a C++ API for interacting with AI agents. Applications create `Transcript` objects and pass them to `Agent` objects; the agent's job is to extend the transcript in a logical way. It does this by communicating with a remote inference server and running tools in the local filesystem.

<svg viewBox="0 0 482 256" style="display:block;width:482px;max-width:100%;height:auto;margin-inline:auto">
 <rect x="4" y="3" width="269" height="149" ry="16.5" fill="none" stroke="var(--border-table)" stroke-width="2"/>
 <rect x="341.5" y="67.5" width="137" height="52" ry="10.6" fill="var(--diagram-solid-fill)" stroke="var(--border-popup)" stroke-width="1"/>
 <path d="m234 211c0 3.9-10.3 7-23 7-12.7 0-23-3.1-23-7v-36h46z" fill="var(--diagram-solid-fill)" stroke="var(--border-popup)" stroke-width="1"/>
 <path d="m81.5 124.4v-48.4" fill="none" stroke="var(--text-muted)" stroke-width="1"/>
 <g fill="var(--diagram-solid-fill)" stroke="var(--border-popup)">
  <rect x="161.5" y="76.5" width="98" height="34" ry="6.5" stroke-width="1"/>
  <rect x="64.5" y="90.5" width="33" height="15" stroke-width="1"/>
  <ellipse cx="211" cy="175" rx="23" ry="7" stroke-width="1"/>
  <rect x="64.5" y="70.5" width="33" height="15" stroke-width="1"/>
  <rect x="64.5" y="110.5" width="33" height="15" stroke-width="1"/>
 </g>
 <g fill="none" stroke="var(--diagram-arrow-color)">
  <path d="m105.2 80.4-6.4-4.8 6.4-4.8" stroke-width="1"/>
  <path d="m99.3 75.4c35-1.6 26.1 18.4 61.8 18.4" stroke-width="1"/>
  <path d="m216.3 160.9-4.8 6.4-4.8-6.4" stroke-width="1"/>
  <path d="m211.5 166.8v-55.9" stroke-width="1"/>
  <path d="m334.3 83.7 6.4 4.8-6.4 4.8" stroke-width="1"/>
  <path d="m340 88.5h-80" stroke-width="1"/>
  <path d="m266.9 95.7-6.4 4.8 6.4 4.8" stroke-width="1"/>
  <path d="m261 100.5h80" stroke-width="1"/>
 </g>
 <text x="210.4" y="96.9" fill="var(--type-color)" font-family="jetbrains_mono,monospace" font-size="14px" font-weight="bold" stroke-width="1" text-anchor="middle">ply::Agent</text>
 <text x="80.9" y="63.9" fill="var(--type-color)" font-family="jetbrains_mono,monospace" font-size="14px" font-weight="bold" stroke-width="1" text-anchor="middle">ply::Transcript</text>
 <g fill="var(--text-secondary)" font-family="source_sans_3,sans-serif" font-size="16px" stroke-width="1" text-anchor="middle">
  <text x="139.1" y="19.6">Application</text>
  <text x="212.1" y="234.6">Local<tspan x="212.1" y="250.6">Filesystem</tspan></text>
  <text x="409.3" y="89.6">Remote<tspan x="409.3" y="105.6">Inference Server</tspan></text>
 </g>
 <rect x="168.5" y="120.5" width="84" height="20" ry="3.8" fill="var(--diagram-solid-fill)" stroke="var(--border-popup)" stroke-width="1"/>
 <text x="212.1" y="134.6" fill="var(--text-secondary)" font-family="source_sans_3,sans-serif" font-size="14px" stroke-width="1" text-anchor="middle">Authorizer</text>
</svg>

The steps for creating and running a Plywood agent are as follows:

1. Create a new `Transcript` object containing the user's prompt.
2. Create a new `Agent` object, passing in the `Transcript`, the desired inference provider and a set of tools for the agent to use.
   The agent runs in a background thread.
3. Receive `Transcript::Event` objects back from the `Agent`.
4. As each event comes in, call `applyTranscriptEvent` and perform any application-specific handling.
5. Once `Agent::isWorking()` returns `false`, no further events will be received and the `Agent` can be safely destroyed.

To send a followup prompt, create a new `Transcript` object, assign the previous `Transcript` as its parent, then create another `Agent` object.

Using `ply-agent.h` requires linking your project with [`libcurl`](https://curl.se/libcurl/) for HTTPS support. Instructions for installing `libcurl` can be found in the documentation for [building and running the `agent` sample](/docs/apps/agent.md#installing-libcurl).

## `Agent`

The `Agent` class represents a live conversation with an agent running in a background thread. The background thread executes tool calls automatically and buffers `Transcript::Event` objects for the application to consume.

A working agent can be cancelled by any thread at any time by calling `cancel()`.
`Agent` objects can be destroyed at any time as long as there are no racing member function calls from other threads.

`Agent::Agent(const Agent::Settings& settings)`
> Constructor. The agent starts running in a background thread. `Agent::Settings` has the following data members:
>
> | | |
> | --- | --- |
> | `const Transcript* startTranscript` | The transcript used to start the agent. |
> | `Agent::EndPoint endPoint` | Identifies the inference provider, protocol and model. |
> | `Agent::Capabilities capabilities` | Specifies the system prompt, working directory and available tools. |
> | `bool enableRawLog` | Enables raw HTTP-level logging (for debug purposes). Default is `false`. |
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
> Returns all currently buffered events without waiting, or an empty array if no events are buffered. Only one thread is allowed to call `pollForEvents`, `waitForEvents` or `waitForCompletion` at a time.

`Array<Transcript::Event> Agent::waitForEvents(s32 maxTimeInMillis = -1)`
> Waits until at least one event is available, then returns all buffered events. A negative argument waits indefinitely. Only one thread is allowed to call `pollForEvents`, `waitForEvents` or `waitForCompletion` at a time.

`Array<Transcript::Event> Agent::waitForCompletion(s32 maxTimeInMillis = -1)`
> Waits until the agent stops working or the time limit is reached, then returns all buffered events. A negative argument waits indefinitely. Only one thread is allowed to call `pollForEvents`, `waitForEvents` or `waitForCompletion` at a time.

`bool Agent::isWorking()`
> If `true`, the agent can still return more events. `false` means the agent has finished running, the buffer is empty and no more events will arrive.
> This function can be called by any thread at any time.

`void Agent::cancel()`
> Stops running the agent. No new events will be generated after this function returns, but any events already buffered remain available for consumption.
> This function can be called by any thread at any time.
> If another thread is waiting inside `waitForEvents` or `waitForCompletion`, that thread will immediately return.
>
> If `cancel` is called while a tool call is running in the background, the tool call might not stop immediately. Tool calls can continue running briefly after `cancel` returns, but they'll be stopped as soon as possible and won't generate any further `Transcript::Event`s.

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

To give an agent access to tools in the local filesystem, fill in the `capabilities.tools` member of `Agent::Settings` before creating the agent. Several built-in tools are available and custom tools can be easily created.

### Built-In Tools

Built-in tools can be created by calling any of the following functions. Each function returns an `Owned<ToolDefinition>` that can be added to the `capabilities.tools` member of `Agent::Settings`.

| Function name | Tool name | Description |
| --- | --- | --- |
| `createShellTool` | `shell` | Runs a command using the system shell after authorization. Not available on iOS. |
| `createReadTool` | `read` | Reads part or all of a file. |
| `createWriteTool` | `write` | Creates or overwrites a file. |
| `createListDirTool` | `list_dir` | Lists the contents of a directory. |
| `createFindInFilesTool` | `find_in_files` | Searches for text in a directory tree. |
| `createEditTool` | `edit` | Edits a file using exact text replacements. |

All built-in tools except `shell` are designed to respect the directory permissions outlined in `Agent::Capabilities`. Custom tools must implement their own permission checks. `shell` tool permissions are implemented by passing a callback to `createShellTool`, as described in the [Tool Monitoring](#tool-monitoring) section.

### Creating Custom Tools

In addition to the built-in tools, applications are free to create their own tools that integrate more closely with agents. To do so, simply fill in the public data members of `ToolDefinition` as appropriate.

| | |
| --- | --- |
| `String name` | The name of the tool, as presented to the agent. |
| `String description` | A description that tells the agent when and how to use the tool. |
| `Array<Parameter> parameters` | Describes the JSON parameters accepted by the tool. |
| `Functor<...> handler` | The internal callback invoked when the agent uses the tool. |
| `bool readOnly` | Indicates that the tool does not modify any data. |

### `ToolContext`

Each time a tool handler is invoked, it receives a `ToolContext` object. `ToolContext` provides the following member functions:

`StringView ToolContext::getWorkingDirectory() const`
> Returns the agent's working directory.

`String ToolContext::checkPathPermission(StringView path, bool withWriteAccess) const`
> Converts `path` to an absolute path if read access is permitted, or write access if `withWriteAccess` is true. Otherwise returns an empty string.

`void ToolContext::appendResponse(Transcript::Message* toolCall, StringView text)`
> Adds text to the tool response in a thread-safe manner. Can be called more than once to stream a response. Each call to `appendResponse` creates a new `Transcript::Event` and buffers it in the `Agent` so that the application receives it as soon as possible. The complete tool response won't be sent to the remote inference server until the next turn.

`bool ToolContext::isCanceled() const`
> Returns whether the agent has been canceled. Long-running tools should call this periodically and return promptly when it becomes true.

`bool ToolContext::setCancelCallback(Functor<void()>&& callback)`
> If the agent hasn't already been canceled, registers a cancellation callback and returns `true`.
> Otherwise, if the agent was already canceled, clears any existing cancellation callback and returns `false`.
> A tool that performs an interruptible blocking operation can use `setCancelCallback()` to register a callback that unblocks it. The callback will be invoked from the client thread when `Agent::cancel()` is called.

`void ToolContext::clearCancelCallback()`
> Clears the registered cancellation callback. The callback must be cleared before the tool handler returns.

For example, a tool to count the number of bytes in a string can be implemented as follows.

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

## Tool Monitoring

Every `shell` tool request made by an agent is passed through an application-defined callback. This allows the application to monitor tool requests and automatically approve, reject and/or bring requests to the user's attention according to application-defined policies.

`Owned<ToolDefinition> createShellTool(Functor<bool(ToolContext* toolCtx, StringView shellCommand)>&& authorizer);`
> Creates a new `shell` tool instance. The `authorizer` callback is moved to the `ToolDefinition` and will be invoked for every `shell` tool request made by the agent. If `authorizer` returns `true`, the shell command is permitted; if it returns `false`, permission is denied and an error is reported back to the agent.

One way to monitor tool requests is to use a second "authorizer" agent to review the requests made by the first agent. For convenience, Plywood provides a `ShellAuthorizationPolicy` class to support this case. `ShellAuthorizationPolicy` has the following data members:

| | |
| --- | --- |
| `String policy` | A natural-language description of shell commands the agent is allowed to run. |
| `Agent::EndPoint authorizerEndPoint` | The authorizer's provider, protocol and model. |
| `Set<Owned<ToolDefinition>> authorizerTools` | The tools available to the authorizer. |
| `Functor<void(Agent*)> loggingHook` | Optional hook for logging the authorizer's transcript. Must consume events from the provided agent until the agent finishes. |
| `bool unrestricted` | If `true`, every command is allowed without consulting the authorizer. |

To invoke the "authorizer" agent, initialize a `ShellAuthorizationPolicy` object and call `authorizeCommand()`. See the [`agent`](/docs/apps/agent.md) sample for an example of how it's used in practice.

`bool ShellAuthorizationPolicy::authorizeCommand(ToolContext* toolCtx, StringView shellCommand) const`
> Returns `true` if the given `shellCommand` was determined to be allowed by the stated policy.

Monitoring `shell` tool requests is only a first line of defense against agents performing unwanted actions. For simple workflows, where the agent is only allowed to run a limited set of shell commands, this is likely sufficient. In environments where stronger security guarantees are needed, the application should be sandboxed and monitored at the operating system level as well.
