`agent-proxy`
=============

`agent-proxy` is a proxy server that handles authentication on behalf of [`agent`](/docs/apps/agent.md).
It's meant to help avoid leaking API keys while using agents to work on `agent` itself.
`agent` sends requests to `agent-proxy`; `agent-proxy` injects API keys, forwards requests to an upstream
provider and sends responses back. 

If CMake is installed, the following command builds and runs the proxy. On Windows, use `share\build-app.bat`
instead.

```
$ OPENAI_API_KEY=<key> share/build-app.sh agent-proxy --run
```

Or, if the app is already built:

```
$ OPENAI_API_KEY=<key> bin/agent-proxy
```

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-p` | `--port` | TCP port to listen on. Defaults to 8082. |
| `-h` | `--help` | Print the available options. |

The proxy loads `known-providers.json` from the executable's directory. This is the same provider list used by
[`agent`](/docs/apps/agent.md). To enable a provider, the environment variable named by `apiKeyEnv` must
be defined before running the proxy.

```
[
    {
        "provider": "openai",
        "url": "https://api.openai.com/v1/responses",
        "protocol": "responses",
        "apiKeyEnv": "OPENAI_API_KEY",
        "defaultModel": "gpt-5.6-luna"
    }
]
```
