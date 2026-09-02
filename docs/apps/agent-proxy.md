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
| `-c` | `--config` | Path to a JSON settings file or directory. |
| `-h` | `--help` | Print the available options. |

If `-c/--config` specifies a directory, the app loads `agent-proxy.json` from that directory. Without the option, it
loads `agent-proxy.json` from the executable's directory.

## Proxy configuration

The settings file contains a listening `port` and one or more exact `routes`:

```
{
    "port": 8082,
    "routes": [
        {
            "path": "/openai/v1/responses",
            "upstreamUrl": "https://api.openai.com/v1/responses",
            "protocol": "responses",
            "apiKeyEnv": "OPENAI_API_KEY"
        },
        {
            "path": "/anthropic/v1/messages",
            "upstreamUrl": "https://api.anthropic.com/v1/messages",
            "protocol": "anthropic",
            "apiKeyEnv": "ANTHROPIC_API_KEY"
        },
        {
            "path": "/ollama-cloud/v1/chat/completions",
            "upstreamUrl": "https://ollama.com/v1/chat/completions",
            "protocol": "completions",
            "apiKeyEnv": "OLLAMA_API_KEY"
        }
    ]
}
```

- `port`: The TCP port to listen on.
- `routes`: A required non-empty array. Each route has the following properties:
    - `path`: Exact local request URI.
    - `upstreamUrl`: Remote inference endpoint.
    - `protocol`: One of `completions`, `responses` or `anthropic`. The `completions` and `responses` protocols use
      bearer authentication; `anthropic` uses the `x-api-key` header.
    - `apiKeyEnv`: Environment variable available to the proxy process that contains the key.

For each matched `POST` request, the proxy looks up that route's environment variable and inserts API key credentials
into the request header. Otherwise, communication data is forwarded as-is.
