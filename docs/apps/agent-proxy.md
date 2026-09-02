`agent-proxy`
=============

`agent-proxy` is a proxy server that handles authentication on behalf of [`agent`](/docs/apps/agent.md).
It's meant to help avoid leaking API keys while using agents to work on `agent` itself.
`agent` sends requests to `agent-proxy`; `agent-proxy` injects API keys, forwards the request to an upstream
inference provider and sends the response back. 

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

## Proxy configuration

The proxy loads `known-providers.json` from the executable's directory. This is the same provider list used by
[`agent`](/docs/apps/agent.md). Each entry supplies an exact local route and its upstream authentication settings:

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
