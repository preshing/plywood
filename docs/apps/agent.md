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
$ share/build-app.sh agent -r [-c <settings-path>] [-p <provider>] [-m <model>] [-x[=<port>]] [-l] [-s] [-o] [prompt]
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
| `-x[=<port>]` | `--proxy[=<port>]` | Connect through [`agent-proxy`](/docs/apps/agent-proxy.md). Requires `-p/--provider`. |
| `-l` | `--http-log` | Write a raw HTTP log. |
| `-c` | `--config` | Path to a JSON settings file or a directory. |
| `-s[=<port>]` | `--serve[=<port>]` | Serve a web UI. Default port is 8081. |
| `-b` | `--browser` | Launch a web browser to view the web UI. |
| `-h` | `--help` | Print the available options. |

The `-p/--provider` option selects an endpoint from a list of known providers.
The known provider list is loaded from `known-providers.json`.
The choice of model can be overridden using `-m/--model`.

| Provider | URL | API key environment variable | Default model |
|---|---|---|---|
| `openai` | `https://api.openai.com/v1/responses` | `OPENAI_API_KEY` | `gpt-5.6-luna` |
| `anthropic` | `https://api.anthropic.com/v1/messages` | `ANTHROPIC_API_KEY` | `claude-haiku-4-5` |
| `ollama-cloud` | `https://ollama.com/v1/chat/completions` | `OLLAMA_API_KEY` | `deepseek-v4-flash` |

The agent reads its API key from the environment variable named by the endpoint's `apiKeyEnv` property. If this
property is `NONE`, authentication is omitted.

When `-x/--proxy` is specified, the agent connects to [`agent-proxy`](/docs/apps/agent-proxy.md) on IPv4 loopback, skips the environment variable lookup and omits authentication. The default port is 8082; pass an inline value such as `--proxy=8088` to select a different port.

## Configuration

By default, the app searches for a file named `agent.json` in the current working directory. If none is found, it checks each ancestor directory, loading the first `agent.json` file it finds.

If `-c/--config` is specified, the app loads settings from the specified path instead. If the specified path is a directory, the app tries to load `agent.json` from that directory. 

```json
{
    "systemPrompt": "You are a helpful assistant.",
    "permissions": [
        {
            "path": ".",
            "tools": ["read", "write", "edit", "shell"]
        }
    ]
}
```

The settings file must contain a single JSON object with any of the following optional properties:

- `endPoint`: A subobject with four required properties. Provider endpoints can be defined here instead of using the `-p/--provider` command-line option.
    - `url`: The URL of an inference server.
    - `protocol`: Must be one of "completions", "responses" or "anthropic".
    - `apiKeyEnv`: The name of an environment variable containing an API key, or `NONE` to omit authentication.
    - `model`: The name of the model to use.
- `systemPrompt`: A system prompt message.
- `useAgentsMD`: If `true` and the working directory contains an `AGENTS.md` file, the contents of this file are appended to the system prompt.
- `userPrompt`: The user prompt. Can be overridden by passing a prompt on the command line.
- `workingDirectory`: The working directory for this settings file. Used as the agent's working directory and as the base for relative permission paths in this settings file. Default is the directory containing the settings file itself. Relative paths are interpreted as relative to the directory containing the settings file.
- `permissions`: An array of subobjects describing the directories the agent can access. Each subobject has the following properties:
    - `path`: The path to a directory. Can be absolute or relative to `workingDirectory`.
    - `tools`: An array of strings listing the tools that the agent is allowed to use in the specified directory.
- `include`: The path to another file containing JSON settings to inherit. Can also be an array of paths for multiple includes. Relative paths are interpreted as relative to the directory containing the JSON file itself.

When the `include` property is used by file A to inherit from another file B, file B is loaded first, then file A's settings are merged in. Any `endPoint` and `userPrompt` properties are fully replaced, while the `systemPrompt` and `permissions` properties are combined additively. All `AGENTS.md` files in the include tree are appended to the system prompt, with leaf files being appended first.
