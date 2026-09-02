`agent`
=======

If CMake is installed, the following command will build and run the `agent` sample application. On Windows, use `share\build-app.bat` instead.

```
$ share/build-app.sh agent --run [-c <settings-path>] [-p <provider>] [-m <model>] [-l] [-s] [-o] [prompt]
```

Or, if the app is already built:

```
$ bin/agent [-c <settings-path>] [-p <provider>] [-m <model>] [-l] [-s] [-o] [prompt]
```

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-c` | `--config` | Path to a JSON settings file or a directory. |
| `-p` | `--provider` | Select a preset inference provider. |
| `-m` | `--model` | The name of the model to use. |
| `-l` | `--http-log` | Write a raw HTTP log. |
| `-s` | `--serve` | Serve a web UI on port 8081. |
| `-o` | `--open` | Open the web UI in the default browser. |
| `-h` | `--help` | Print the available options. |

The `-p/--provider` option selects an endpoint from a list of known providers.
The known provider list is loaded from `known-providers.json`.
The choice of model can be overridden using `-m/--model`.

| Provider | URL | Protocol | API key environment variable | Default model |
|---|---|---|---|---|
| `openai` | `https://api.openai.com/v1/responses` | `responses` | `OPENAI_API_KEY` | `gpt-5.6-luna` |
| `anthropic` | `https://api.anthropic.com/v1/messages` | `anthropic` | `ANTHROPIC_API_KEY` | `claude-haiku-4-5` |
| `ollama-cloud` | `https://ollama.com/v1/chat/completions` | `completions` | `OLLAMA_API_KEY` | `deepseek-v4-flash` |

The agent reads its API key from the environment variable named by the endpoint's `apiKeyEnv` property. If this
property is `NONE`, authentication is omitted.

If `-c/--config` is specified, the app loads settings directly from the specified path. If the specified path is a directory, the app tries to load `agent.json` from that directory. If `-c/--config` is not specified, the app searches for a file named `agent.json` in the current working directory; if none is found, it checks each ancestor directory, loading the first `agent.json` file it finds.

The settings file must contain a single JSON object with any of the following properties:

- `endPoint`: A subobject with four required properties:
    - `url`: The URL of an inference server.
    - `protocol`: Must be one of "completions", "responses" or "anthropic".
    - `apiKeyEnv`: The name of an environment variable containing an API key, or `NONE` to omit authentication.
    - `model`: The name of the model to use.
- `systemPrompt`: A system prompt message.
- `userPrompt`: The user prompt. Can be overridden by passing a prompt on the command line.
- `workingDirectory`: The working directory used by the agent and as the base for relative permission paths. The default is the directory containing the settings file itself. Can be absolute or relative to directory containing the settings file.
- `permissions`: An array of subobjects describing the directories the agent can access. Each subobject has the following properties:
    - `path`: The path to a directory. Can be absolute or relative to `workingDirectory`.
    - `tools`: An array of strings listing the tools the agent is allowed to use in the specified directory.
- `include`: The path to another file containing JSON settings to inherit. Can be absolute or relative to the directory containing the JSON file itself.

When the `include` property is used by file A to inherit from another file B, file B is loaded first, then file A's settings are merged in. When merging, the `endPoint` and `userPrompt` properties are fully replaced, while the `systemPrompt` and `permissions` properties are combined additively.
