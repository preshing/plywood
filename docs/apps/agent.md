`agent`
=======

`agent` is an agent harness with a command-line interface.
It provides rudimentary scaffolding around the [Agent Harness library](/docs/high-level/agent-harness.md) in the form of command-line options
and JSON settings.
Agent transcripts are streamed to standard output and can also be streamed to a web browser with Markdown formatting applied.

## Installing `libcurl`

`agent` requires [`libcurl`](https://curl.se/libcurl/) in order to build and run.

### Windows

On Windows, you can install `libcurl` using Microsoft's [`vcpkg`](https://vcpkg.io/) package manager.

For convenience, you can run `share\download-libcurl-vcpkg.bat` to clone the `vcpkg` repository, bootstrap it and use it to install `libcurl`.
This will clone `vcpkg` to the `..\vcpkg` directory relative to Plywood's root directory.

### macOS

On macOS, `libcurl` can be installed using the [Homebrew](https://brew.sh/) package manager. [More details coming soon.]

### Linux

On Linux distributions based on `apt`, such as Debian and Ubuntu, `libcurl` can be installed using the following command line.
For other distributions, follow your package manager's instructions.

```
$ sudo apt-get install libcurl4-openssl-dev libssl-dev
```

## Building and Running

If CMake and `libcurl` are both installed, the following command will build and run `agent`. On Windows, use `share\build-app.bat` instead.

```
$ share/build-app.sh agent -r [-c <settings-path>] [-p <provider>] [-m <model>] [-x[=<port>]] [-a] [-l] [-r] [-s] [-b] [prompt]
```

For example:

```
$ OPENAI_API_KEY=<key> share/build-app.sh agent -r -p openai "Hello!"
```

Or, if the app is already built:

```
$ OPENAI_API_KEY=<key> bin/agent -p openai "Hello!"
```

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-p` | `--provider` | Select from a list of known inference providers. |
| `-m` | `--model` | The name of the model to use. |
| `-x[=<port>]` | `--proxy[=<port>]` | Connect through [`agent-proxy`](/docs/apps/agent-proxy.md). |
| `-l` | `--log` | Log the transcript to `agent-log-<timestamp>.txt`. |
| `-a` | `--authorizer-log` | Log authorizer transcripts to `agent-authorization-log-<timestamp>.txt`. |
| `-r` | `--raw-log` | Write raw HTTP log to `agent-raw-log-<timestamp>.txt`. |
| `-c` | `--config` | Path to a JSON settings file or a directory. |
| `-s[=<port>]` | `--serve[=<port>]` | Serve a loopback-only web UI. Default port is 8081. |
| `-b` | `--browser` | Launch a web browser to view the web UI. |
| `-h` | `--help` | Print the available options. |

The `-p/--provider` option selects an endpoint from a list of known providers.
The known provider list is loaded from `known-providers.json`.
The choice of model can be overridden using `-m/--model`.

| Provider | URL | API key environment variable | Default model |
|---|---|---|---|
| `openai` | `https://api.openai.com/v1/responses` | `OPENAI_API_KEY` | `gpt-5.6-luna` |
| `anthropic` | `https://api.anthropic.com/v1/messages` | `ANTHROPIC_API_KEY` | `claude-haiku-4-5` |
| `google` | Gemini Interactions API | `GEMINI_API_KEY` | `gemini-3.8-flash` |
| `ollama-cloud` | `https://ollama.com/v1/chat/completions` | `OLLAMA_API_KEY` | `deepseek-v4-flash` |

The agent reads its API key from the environment variable named by the endpoint's `apiKeyEnv` property. If this
property is `NONE`, authentication is omitted.

When `-x/--proxy` is specified, a named `provider` must be selected in JSON or with `-p/--provider`. The agent connects to [`agent-proxy`](/docs/apps/agent-proxy.md) on IPv4 loopback, skips the environment variable lookup and omits authentication. The default port is 8082; pass an inline value such as `--proxy=8088` to select a different port.

## Configuration

By default, the app searches for a file named `agent.json` in the current working directory. If none is found, it checks each ancestor directory, loading the first `agent.json` file it finds.

If `-c/--config` is specified, the app loads settings from the specified path instead. If the specified path is a directory, the app tries to load `agent.json` from that directory. 

```json
{
    "provider": "openai",
    "systemPrompt": "You are a helpful assistant.",
    "readableDirs": ["../reference"],
    "writableDirs": ["."],
    "tools": [
        "read", "list_dir", "find_in_files", "write", "edit", "shell"
    ],
    "shellAuthorizer": {
        "policy": "The agent may build and run the existing sample applications.",
        "tools": ["read", "list_dir", "find_in_files"]
    }
}
```

The settings file must contain a single JSON object with these optional properties:

| Property | Description |
|---|---|
| `provider` | Selects a preset from `known-providers.json`. It must be a nonempty string and cannot be combined with `url`, `protocol`, or `apiKeyEnv`. |
| `url` | The URL of a custom inference endpoint. Custom endpoints must also specify `protocol`, `apiKeyEnv`, and `model`. |
| `protocol` | The custom endpoint protocol: `completions`, `responses`, `anthropic`, or `interactions`. |
| `apiKeyEnv` | The environment variable containing the custom endpoint's API key. Use `NONE` to omit authentication. |
| `model` | Selects the model. It is required for a custom endpoint and optionally overrides a provider's default model; a provider override must be nonempty. |
| `systemPrompt` | A system prompt message. |
| `useAgentsMD` | If `true`, appends the `AGENTS.md` in this file's working directory to the system prompt. |
| `userPrompt` | The user prompt, overridden by a prompt on the command line. |
| `workingDir` | The agent's working directory and base for this file's relative directory paths. Defaults to the settings file's directory; relative values are resolved against that directory. |
| `readableDirs` | An array of absolute paths or paths relative to this file's working directory where the agent has recursive read access. |
| `writableDirs` | An array of absolute paths or paths relative to this file's working directory where the agent has recursive write access. Write permission also grants read access. |
| `tools` | An array of tool names. Available names are `read`, `list_dir`, `find_in_files`, `write`, `edit`, and `shell` (except on iOS). |
| `shellAuthorizer` | Configures the "authorizer" agent that reviews each `shell` request. |
| `include` | A settings file path or array of paths to inherit, relative to the declaring file's directory. |

The `shellAuthorizer` subobject is used to configure a `ShellToolSettings` instance as described in the [Agent Harness library](/docs/high-level/agent-harness#tools). It accepts the following optional properties:

| Property | Description |
|---|---|
| `policy` | Additional natural-language rules describing the shell actions the main agent may take. |
| `provider` | Selects a preset from `known-providers.json`. It must be a nonempty string and cannot be combined with `url`, `protocol`, or `apiKeyEnv`. |
| `url` | The URL of a custom inference endpoint. Custom endpoints must also specify `protocol`, `apiKeyEnv`, and `model`. |
| `protocol` | The custom endpoint protocol: `completions`, `responses`, `anthropic`, or `interactions`. |
| `apiKeyEnv` | The environment variable containing the custom endpoint's API key. Use `NONE` to omit authentication. |
| `model` | Selects the model. It is required for a custom endpoint and optionally overrides a provider's default model; a provider override must be nonempty. |
| `tools` | Read-only tools available to the authorizer. |
