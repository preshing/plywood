/*───────────────────────────────────────────────────────────┐
│                                                            │
│     ____      Plywood C++ Runtime Library                  │
│    ╱   ╱╲     https://plywood.dev/                         │
│   ╱___╱╭╮╲                                                 │
│    └──┴┴┴┘    agent-proxy                                  │
│               Documentation: /docs/apps/agent-proxy.md     │
│                                                            │
└───────────────────────────────────────────────────────────*/

#include <ply-json.h>
#include <ply-network.h>
#include <ply-reflect.h>
#include <curl/curl.h>

using namespace ply;

struct CommandLineOptions {
    String settingsPath;
    bool printUsage = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

enum class Protocol {
    Completions,
    Responses,
    Anthropic,
};

struct Route {
    String path;
    String upstreamUrl;
    Protocol protocol;
    String apiKeyEnv;
};

struct Settings {
    u16 port = 8082;
    Array<Route> routes;
};

CommandLineOptions options;
Settings settings;

// Return true for headers whose framing is valid on the next HTTP connection.
static bool isEndToEndHeader(StringView name) {
    return name != "connection" && name != "content-length" && name != "host" && name != "keep-alive" &&
           name != "proxy-authenticate" && name != "proxy-authorization" && name != "te" && name != "trailer" &&
           name != "transfer-encoding" && name != "upgrade";
}

// Return true when Connection explicitly names another hop-by-hop header.
static bool isNamedByConnectionHeader(StringView name, const Map<String, String>& headers) {
    const String* connection = headers.find("connection");
    if (!connection)
        return false;
    for (StringView token : connection->split(",")) {
        if (token.trim().lower() == name)
            return true;
    }
    return false;
}

// Reject text that could inject another HTTP header line.
static bool isSafeHeaderText(StringView text) {
    return text.find('\r') < 0 && text.find('\n') < 0;
}

// Read and validate the proxy's complete route table before opening the listener.
static bool loadSettings() {
    String settingsPath = options.settingsPath ? makeAbsolutePath(options.settingsPath)
                                               : joinPath(getCurrentExecutablePath(), "../agent-proxy.json");
    if (FileSystem::exists(settingsPath) == ExistsResult::Directory) {
        settingsPath = joinPath(settingsPath, "agent-proxy.json");
    }

    // Parse the configuration root object.
    String jsonText = FileSystem::loadText(settingsPath);
    if (!jsonText) {
        getStdErr().format("Could not load configuration file: {}\n", settingsPath);
        return false;
    }
    json::Parser parser;
    json::Parser::Result result = parser.parse(settingsPath, jsonText);
    if (parser.anyError() || !result.root.isObject()) {
        getStdErr().format("Failed to parse configuration file: {}\n", settingsPath);
        return false;
    }

    // Import the optional listening port.
    if (const json::Node& jPort = result.root.get("port")) {
        double portNumber = jPort.getNumber();
        if (!jPort.isNumber() || portNumber < 1 || portNumber > 65535 || portNumber != u32(portNumber)) {
            getStdErr().format("port must be an integer from 1 to 65535 in: {}\n", settingsPath);
            return false;
        }
        settings.port = numericCast<u16>(u32(portNumber));
    }

    // Import each exact path-to-upstream mapping.
    const json::Node& jRoutes = result.root.get("routes");
    if (!jRoutes.isArray() || jRoutes.arrayView().isEmpty()) {
        getStdErr().format("routes must be a non-empty array in: {}\n", settingsPath);
        return false;
    }
    for (const json::Node& jRoute : jRoutes.arrayView()) {
        if (!jRoute.isObject()) {
            getStdErr().format("Every route must be an object in: {}\n", settingsPath);
            return false;
        }

        // Require every route field.
        const json::Node& jPath = jRoute.get("path");
        const json::Node& jUpstreamUrl = jRoute.get("upstreamUrl");
        const json::Node& jProtocol = jRoute.get("protocol");
        const json::Node& jApiKeyEnv = jRoute.get("apiKeyEnv");
        if (!jPath.isText() || !jUpstreamUrl.isText() || !jProtocol.isText() || !jApiKeyEnv.isText()) {
            getStdErr().format("Invalid or missing route property in: {}\n", settingsPath);
            return false;
        }

        // Validate the route destination and protocol.
        if (!jPath.text().startsWith('/') || jPath.text().find('?') >= 0 || !jApiKeyEnv.text() ||
            (!jUpstreamUrl.text().startsWith("https://") && !jUpstreamUrl.text().startsWith("http://"))) {
            getStdErr().format("Invalid route path, URL or API key environment variable in: {}\n", settingsPath);
            return false;
        }
        Protocol protocol;
        if (jProtocol.text() == "completions") {
            protocol = Protocol::Completions;
        } else if (jProtocol.text() == "responses") {
            protocol = Protocol::Responses;
        } else if (jProtocol.text() == "anthropic") {
            protocol = Protocol::Anthropic;
        } else {
            getStdErr().format("Unknown protocol '{}' in: {}\n", jProtocol.text(), settingsPath);
            return false;
        }
        for (const Route& route : settings.routes) {
            if (route.path == jPath.text()) {
                getStdErr().format("Duplicate route path '{}': {}\n", jPath.text(), settingsPath);
                return false;
            }
        }

        // Retain the environment variable name so the key can be resolved for each request.
        settings.routes.append({jPath.text(), jUpstreamUrl.text(), protocol, jApiKeyEnv.text()});
    }
    return true;
}

// Return a JSON error envelope understood by the agent's HTTP error handler.
static void sendProxyError(HTTPServer::Request& request, u32 statusCode, String&& message) {
    json::Node root{json::Node::Object{}};
    json::Node error{json::Node::Object{}};
    error.set("message", json::Node::Text{std::move(message)});
    root.set("error", std::move(error));
    HTTPServer::Response response{statusCode};
    *response.headers.insert("content-type").value = "application/json";
    request.sendFullResponse(std::move(response), json::toString(root, {false}));
}

// Find the fixed upstream associated with an incoming URI.
static const Route* findRoute(StringView uri) {
    for (const Route& route : settings.routes) {
        if (route.path == uri)
            return &route;
    }
    return nullptr;
}

// Forward one inference request while inserting authentication for its configured protocol.
static void proxyRequest(HTTPServer::Request& request) {
    if (request.method.lower() != "post") {
        request.sendGenericResponse(HTTPServer::Response::MethodNotAllowed);
        return;
    }
    const Route* route = findRoute(request.uri);
    if (!route) {
        request.sendGenericResponse(HTTPServer::Response::NotFound);
        return;
    }

    // Resolve only this route's credential and report configuration failures as JSON.
    String apiKey = getEnvironmentVariable(route->apiKeyEnv);
    if (!apiKey) {
        sendProxyError(request, HTTPServer::Response::ServiceUnavailable,
                       String::format("API key environment variable {} is not set in agent-proxy", route->apiKeyEnv));
        return;
    }
    if (!isSafeHeaderText(apiKey)) {
        sendProxyError(
            request, HTTPServer::Response::ServiceUnavailable,
            String::format("API key environment variable {} is not a valid HTTP header value", route->apiKeyEnv));
        return;
    }

    // Copy end-to-end request headers while replacing the protocol's credential.
    StringView apiKeyHeader = route->protocol == Protocol::Anthropic ? "x-api-key" : "authorization";
    Map<String, String> upstreamHeaders;
    for (const auto& item : request.headers.items()) {
        if (isEndToEndHeader(item.key) && !isNamedByConnectionHeader(item.key, request.headers) &&
            item.key != apiKeyHeader) {
            *upstreamHeaders.insert(item.key).value = item.value;
        }
    }
    *upstreamHeaders.insert(apiKeyHeader).value =
        route->protocol == Protocol::Anthropic ? std::move(apiKey) : String::format("Bearer {}", apiKey);

    // Relay the upstream response into a close-delimited streaming response.
    bool responseStarted = false;
    bool upstreamFailed = false;
    String upstreamError;
    Stream responseBody;
    Owned<HTTPClient> client = HTTPClient::create();
    HTTPClient::Args args;
    args.url = route->upstreamUrl;
    args.headers = std::move(upstreamHeaders);
    args.body = request.body;
    args.callback = [&](const HTTPClient::Event& event) {
        if (auto* headers = event.as<HTTPClient::Headers>()) {
            HTTPServer::Response response{headers->statusCode};
            for (const auto& item : headers->headers.items()) {
                if (isEndToEndHeader(item.key) && !isNamedByConnectionHeader(item.key, headers->headers)) {
                    *response.headers.insert(item.key).value = item.value;
                }
            }
            responseBody = request.beginStreamingResponse(std::move(response));
            responseStarted = true;
        } else if (auto* data = event.as<HTTPClient::Data>()) {
            if (responseStarted) {
                responseBody.write(data->bytes);
                responseBody.flush(true);
            }
        } else if (auto* error = event.as<HTTPClient::Error>()) {
            upstreamFailed = true;
            upstreamError = error->message;
        }
    };
    client->beginRequest(std::move(args));
    while (client->receiveResponse()) {
        // Response events are forwarded synchronously by receiveResponse().
    }

    // Return a gateway error only while it is still possible to start a response.
    if (!responseStarted) {
        request.sendGenericResponse(HTTPServer::Response::BadGateway);
    }
    if (upstreamFailed) {
        getStdErr().format("Upstream request for {} failed: {}\n", route->path, upstreamError);
    }
}

// Print the command-line syntax and registered options.
static void printUsage(Stream& out, StringView executablePath, const CommandLineParser& parser) {
    out.format("Usage: {} [-c <settings-path>]\n", executablePath);
    parser.printAvailableOptions(out);
}

int main(int argc, const char* argv[]) {
    // Parse command-line options.
    CommandLineParser parser({
        {"-c", "--config", PLY_LOOKUP_MEMBER(CommandLineOptions, settingsPath),
         "Path to JSON settings file or directory"},
        {"-h", "--help", PLY_LOOKUP_MEMBER(CommandLineOptions, printUsage), "Print this help"},
    });
    if (!parser.apply(argc, argv, &options)) {
        Stream err = getStdErr();
        err.write("\n");
        printUsage(err, argv[0], parser);
        return 1;
    }
    if (options.printUsage) {
        Stream out = getStdOut();
        printUsage(out, argv[0], parser);
        return 0;
    }
    if (!loadSettings())
        return 1;

    // Initialize networking and serve only on IPv4 loopback.
    CURLcode curlResult = curl_global_init(CURL_GLOBAL_DEFAULT);
    PLY_ASSERT(curlResult == CURLE_OK);
    PLY_UNUSED(curlResult);
    Network::initialize(IPv4);
    getStdOut().format("Forwarding {} route(s) on http://127.0.0.1:{}\n", settings.routes.numItems(), settings.port);
    HTTPServer::run(IPAddress::localHost(IPv4), settings.port, proxyRequest);
    Network::shutdown();
    curl_global_cleanup();
    return 0;
}

PLY_STRUCT_BEGIN(CommandLineOptions)
PLY_STRUCT_MEMBER(settingsPath)
PLY_STRUCT_MEMBER(printUsage)
PLY_STRUCT_END()
