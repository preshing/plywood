`agent`
=======

If CMake is installed, the following command will build and run the `agent` sample application. On Windows, use `share\build-app.bat` instead.

```
$ share/build-app.sh agent --run [-c <settings-path>] [-l] [-s] [prompt]
```

Or, if the app is already built:

```
$ bin/agent [-c <settings-path>] [-l] [-s] [prompt]
```

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-c` | `--config` | Path to a JSON settings file. |
| `-l` | `--http-log` | Write a raw HTTP log. |
| `-s` | `--serve` | Create a webserver on port 8081. |
| `-h` | `--help` | Print the available options. |

If `-c/--config` is specified, the app loads settings directly from the specified file. Otherwise, the app searches for a file named `agent.json` in the current working directory. If none is found, it checks each ancestor directory and loads the first `agent.json` file it finds.

The settings file must contain a single JSON object with any of the following properties:

- `endPoint`: A subobject with four required properties:
    - `url`: The URL of an inference server.
    - `protocol`: Either "responses" or "completions".
    - `apiKeyEnv`: The name of an environment variable containing an API key.
    - `model`: The model name.
- `systemPrompt`: A system prompt message.
- `userPrompt`: The user prompt. Can be overridden by passing a prompt on the command line.
- `workingDirectory`: The working directory used by the agent and as the base for relative permission paths. The default is the directory containing the settings file itself. Can be absolute or relative to directory containing the settings file.
- `permissions`: An array of subobjects describing the directories the agent can access. Each subobject has the following properties:
    - `path`: The path to a directory. Can be absolute or relative to `workingDirectory`.
    - `tools`: An array of strings listing the tools the agent is allowed to use in the specified directory.
- `include`: The path to another file containing JSON settings to inherit. Can be absolute or relative to the directory containing the JSON file itself.

When the `include` property is used by file A to inherit from another file B, the settings in file B are loaded first, then the settings in file A are merged according to the following rules:

- An `endPoint` object in file A fully replaces the `endPoint` from file B. None of B's `endPoint` properties are inherited.
- A `systemPrompt` property in file A is appended to the `systemPrompt` property from file B, effectively combining both system prompts.
- A `userPrompt` property in file A replaces the `userPrompt` property from file B.
- `workingDirectory` is not inherited from file B.
- `permissions` are combined, effectively giving the agent all tool permissions from every loaded settings file.
