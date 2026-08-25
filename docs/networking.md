`ply-network.h`: Networking
===========================

Plywood provides a portable API for TCP/IP networking supporting both IPv4 and IPv6 addresses, with optional HTTP support.

| File | Description | Size |
| --- | --- | --- |
| `ply-network.h` | Interface | ~400 lines |
| `ply-network.cpp` | Implementation | ~1,300 lines |

Before using any networking functions, call `Network::initialize()`. When finished, call `Network::shutdown()`.

## `Network`

The `Network` class provides static methods for network initialization and connection management.

{context class=Network}

`static void initialize(IPVersion ipVersion)`
> Initializes the networking subsystem. Must be called before any other networking functions. Specify `IPVersion::V4` or `IPVersion::V6`.

`static void shutdown()`
> Shuts down the networking subsystem and releases resources.

`static TCPListener bindTcp(u16 port)`
> Creates a TCP listener bound to the specified port. The listener can accept incoming connections.

`static Owned<TCPConnection> connectTcp(const IPAddress& address, u16 port)`
> Establishes a TCP connection to the specified address and port. Returns null on failure.

`static IPAddress resolveHostName(StringView hostName, IPVersion ipVersion)`
> Resolves a hostname (e.g., "example.com") to an IP address using DNS.

`static IPResult lastResult()`
> Returns the result code from the most recent network operation.

## `IPAddress`

Represents an IP address (either IPv4 or IPv6).

{context class=IPAddress}

| | |
| --- | --- |
| `u32 netOrdered[4]` | The raw address bytes in network byte order. For IPv4, only `netOrdered[0]` is used. |

`IPVersion version() const`
> Returns `IPVersion::V4` or `IPVersion::V6`.

`bool isNull() const`
> Returns `true` if this is a null/uninitialized address.

`static constexpr IPAddress localHost(IPVersion ipVersion)`
> Returns the localhost address (`127.0.0.1` for IPv4, `::1` for IPv6).

`static constexpr IPAddress fromIPv4(u32 netOrdered)`
> Creates an IPv4 address from a 32-bit value in network byte order.

`String toString() const`
> Returns a human-readable string representation of the address.

`static IPAddress fromString()`
> Parses an IP address from a string.

## `TCPConnection`

Represents an established TCP connection to a remote host. Exposes input and output pipes as public data members. Use `createInStream()` and `createOutStream()` to create `Stream` wrappers around them.

{context class=TCPConnection}

| | |
| --- | --- |
| `Owned<PipeWinsock> inPipe` | The underlying pipe object for reading on Windows. On POSIX systems, the type is `Owned<PipeFD>`. |
| `Owned<PipeWinsock> outPipe` | The underlying pipe object for writing on Windows. On POSIX systems, the type is `Owned<PipeFD>`. |

`const IPAddress& remoteAddress() const`
> Returns the IP address of the remote host.

`u16 remotePort() const`
> Returns the port number of the remote host.

`SOCKET getHandle() const`
> Returns the underlying socket handle. Use with care.

`Stream createInStream()`
> Creates a buffered stream for reading data from the connection.

`Stream createOutStream()`
> Creates a buffered stream for writing data to the connection.

## `TCPListener`

A `TCPListener` listens for incoming TCP connections on a specific port. Supports move assignment.

{context class=TCPListener}

`bool isValid()`
> Returns `true` if the listener is bound to a valid socket.

`void endComm()`
> Signals that no more connections will be accepted. Causes any blocking `accept()` call to return.

`void close()`
> Closes the listener socket.

`Owned<TCPConnection> accept()`
> Blocks until a client connects, then returns the new connection. Returns null if the listener was closed.

```
// Simple echo server
Network::initialize(IPVersion::V4);
TCPListener listener = Network::bindTcp(8080);

while (true) {
    Owned<TCPConnection> conn = listener.accept();
    if (!conn) break;

    Stream in = conn->createInStream();
    Stream out = conn->createOutStream();

    String line = readLine(in);
    out.write(line);
    out.write("\n");
    out.flush();
}

Network::shutdown();
```

## `HTTPClient`

`HTTPClient` is an optional class for making HTTP requests to remote servers.
To enable it, set the preprocessor definition `PLY_WITH_HTTP_CLIENT=1` in your project's settings.
`HTTPClient`'s member functions aren't thread-safe and are meant to be called from a single thread except for `HTTPClient::wakeUp`.
All member functions are designed to return as quickly as possible, so they can be used in an application's main loop without causing frame delays.
Requires [libcurl](https://curl.se/libcurl/) to be linked and initialized.

```
#include <ply-network.h>
#include <curl/curl.h>

using namespace ply;

int main() {
    // Initialize libcurl.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return 1;

    // Perform an HTTP request.
    Owned<HTTPClient> client = HTTPClient::create();
    HTTPClient::Args args;
    args.url = "https://plywood.dev";
    args.callback = [](const HTTPClient::Event& event) {
        if (auto* headers = event.as<HTTPClient::Headers>()) {
            getStdOut().format("HTTP status: {}\n", headers->statusCode);
        } else if (auto* data = event.as<HTTPClient::Data>()) {
            getStdOut().write(data->bytes);
        } else if (event.is<HTTPClient::End>()) {
            getStdOut().write("Request complete\n");
        } else if (auto* error = event.as<HTTPClient::Error>()) {
            getStdErr().format("Request failed: {}\n", error->message);
        }
    };
    client->beginRequest(std::move(args));
    while (client->receiveResponse()) {
        // Response data is delivered to `callback` inside receiveResponse().
    }
    client.clear();

    // Shut down libcurl.
    curl_global_cleanup();
    return 0;
}
```

`static Owned<HTTPClient> HTTPClient::create()`
> Creates a new `HTTPClient`.

`void destroy(HTTPClient* httpClient)`
> Destroys an `HTTPClient`. Any in-progress request is immediately cancelled.

`void HTTPClient::beginRequest(Args&& args)`
> Starts a new request. Must not be called while a request is already in progress.
> `HTTPClient::Args` has the following data members.
> All members are moved from when the function is called, leaving the original `args` in an empty state.
>
> | | |
> | --- | --- |
> | `String url` | The URL to request. |
> | `Map<String, String> headers` | HTTP headers to send with the request. |
> | `String body` | The request body. |
> | `Functor<void(const Event&)> callback` | Receives response events. |
> | `bool useBundledCaCert` | If `true`, verifies the peer using the bundled CA certificates. If `false`, disables TLS verification and should only be used for trusted localhost endpoints. Default is true. |

`void HTTPClient::cancelRequest()`
> Cancels any request in progress. After this returns, `isRequestInProgress()` returns `false`.
> It's safe to call `beginRequest()` on the same `HTTPClient` again after this.

`bool HTTPClient::isRequestInProgress() const`
> Returns `true` while a request is in progress.

`bool HTTPClient::receiveResponse(u32 timeOutMillis = 1000)`
> Drives the request, delivering response events to the request's callback. If an event is available, it invokes the
> callback and returns immediately. If no data is available, it waits up to `timeOutMillis` for data to arrive,
> but can be interrupted by other threads calling `wakeUp()`. When `timeOutMillis` is 0, returns as soon as all
> available data is processed. Returns `false` if no request is in progress; `true` otherwise.

`void HTTPClient::wakeUp()`
> Can be called from any thread. If `receiveResponse()` is currently blocked in another thread, it returns
> immediately; otherwise the next `receiveResponse()` call returns immediately.

### `HTTPClient::Event`

> The `HTTPClient::Event` object received by the response callback is a [variant](/docs/system/memory/variants.md) with the following subtypes:
>
> | | |
> | --- | --- |
> | `HTTPClient::Headers` | Contains the HTTP status code and response headers. Header keys are stored in lowercase. |
> | `HTTPClient::Data` | Contains the next response body chunk. Its `bytes` member is only valid for the duration of the callback. |
> | `HTTPClient::End` | Indicates that the HTTP transfer completed successfully. HTTP error status codes are still successful transfers. |
> | `HTTPClient::Error` | Contains a libcurl error message and terminates the event stream. It is never followed by `End`. |
>
> A successful request produces `Headers`, zero or more `Data` events and one `End` event. A transport failure
> produces `Error`, possibly after `Headers` and `Data` events were already delivered. `cancelRequest()` does not
> generate an event.

## `HTTPServer`

`HTTPServer` is an optional class for running a local HTTP server.
To enable it, set the preprocessor definition `PLY_WITH_HTTP_SERVER=1` in your projects's settings.
No encryption capabilities are provided.

```
#include <ply-network.h>

using namespace ply;

void serverCallback(HTTPServer::Request& request) {
    HTTPServer::Response response{HTTPServer::Response::OK};
    *response.headers.insert("content-type").value = "text/plain";
    String body = String::format("Request URI: {}\n", request.uri);
    request.sendFullResponse(std::move(response), body);
}

int main() {
    // Initialize the network.
    Network::initialize(IPv4);

    // Run a webserver.
    HTTPServer::run(8080, serverCallback);

    // Shut down the network.
    Network::shutdown();
    return 0;
}
```

`static void HTTPServer::run(u16 port, const Functor<void(Request& request)>& requestHandler)`
> When this function is called, the calling thread is blocked for as long as the server keeps running.
> `Network::initialize()` must be called first.
> `requestHandler` is a user-provided callback function that handles individual HTTP requests.

### `HTTPServer::Request`

For each incoming HTTP request, the `requestHandler` is called with an `HTTPServer::Request` object,
which exposes the following public data members and member functions:

{context class="HTTPServer::Request"}

| | |
| --- | --- |
| `IPAddress clientAddr` | The remote TCP peer address. |
| `u16 clientPort` | The remote TCP peer port. |
| `String method` | The request method, such as `GET`, `HEAD`, `POST`, `PUT` or `PATCH`. |
| `String uri` | The raw request URI. |
| `String httpVersion` | The HTTP version token, such as `HTTP/1.1`. |
| `Map<String, String> headers` | Request headers indexed by lower-case header name. For example, keys contain "content-type" rather than "Content-Type". |
| `String body` | The complete request body. This string can contain arbitrary binary data and is not guaranteed to be null-terminated. |

`void sendFullResponse(HTTPServer::Response&& response, StringView body = {})`
> Sends a complete response.
> The `response` argument is moved from, leaving the original argument in an empty state.
> The underlying connection can be reused for additional requests when HTTP rules permit it.

`Stream beginStreamingResponse(HTTPServer::Response&& response)`
> Sends response headers only and returns a TCP stream for writing the body.
> The connection is closed when the caller destroys the stream.

`void sendGenericResponse(HTTPServer::Response::Code responseCode)`
> Writes a generic HTML error page with the given status code.

### `HTTPServer::Response`

`HTTPServer::Response` is used as an argument to `HTTPServer::Request` member functions and has the following public data members:

> | | |
> | --- | --- |
> | `Code code` | The HTTP status code to emit. Default is InternalError. |
> | `Map<String, String> headers` | Response headers to emit. |

`HTTPServer::Response::Code` is an enum type with the following values:

| | |
| --- | --- |
| `Code::OK` | 200 |
| `Code::PermanentRedirect` | 301 |
| `Code::TemporaryRedirect` | 302 |
| `Code::BadRequest` | 400 |
| `Code::NotFound` | 404 |
| `Code::InternalError` | 500 |
