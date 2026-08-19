`ply-network.h`: Networking
===========================

Plywood provides a portable networking API for TCP/IP communication. The API supports both IPv4 and IPv6 addresses.

Before using any networking functions, you must call `Network::initialize()`. When finished, call `Network::shutdown()`.

## `IPAddress`

Represents an IP address (either IPv4 or IPv6).

{context class=IPAddress}

`u32 netOrdered[4]`
> The raw address bytes in network byte order. For IPv4, only `netOrdered[0]` is used.

`IPVersion version() const`
> Returns `IPVersion::V4` or `IPVersion::V6`.

`bool isNull() const`
> Returns `true` if this is a null/uninitialized address.

`static constexpr IPAddress localHost(IPVersion ipVersion)`
> Returns the localhost address (127.0.0.1 for IPv4, ::1 for IPv6).

`static constexpr IPAddress from_ipv4(u32 netOrdered)`
> Creates an IPv4 address from a 32-bit value in network byte order.

`String toString() const`
> Returns a human-readable string representation of the address.

`static IPAddress fromString()`
> Parses an IP address from a string.

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

## `TCPConnection`

Represents an established TCP connection to a remote host.

{context class=TCPConnection}

`PipeWinsock inPipe`
`PipeWinsock outPipe`
> The underlying pipe objects for reading and writing. Typically, use `createInStream()` and `createOutStream()` instead.

`TCPConnection()`
`~TCPConnection()`
> Constructor and destructor. Connections are typically created via `Network::connectTcp()` or `TCPListener::accept()`.

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

A `TCPListener` listens for incoming TCP connections on a specific port.

{context class=TCPListener}

`TCPListener(SOCKET listenSocket = INVALID_SOCKET)`
> Constructs a listener from a socket handle. Typically created via `Network::bindTcp()`.

`TCPListener(TCPListener&& other)`
> Move constructor.

`TCPListener& operator=(TCPListener&& other)`
> Move assignment.

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

Setting `PLY_WITH_HTTP_CLIENT=1` enables `HTTPClient`, which encapsulates HTTP requests. The `HTTPClient` API is not thread-safe and is intended to be driven from a single
thread except for `wakeUpHTTPClient`. All functions except `waitForHTTPResponse` are designed to return as quickly as possible, so they can be used in an application's main loop without causing frame spikes. Requires [libcurl](https://curl.se/libcurl/) to be linked and initialized using [`curl_global_init`](https://curl.se/libcurl/c/curl_global_init.html).

```
#include <ply-network.h>
#include <curl/curl.h>

using namespace ply;

int main() {
    // Initialize libcurl.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return 1;

    // Perform an HTTP request.
    Owned<HTTPClient> client = createHTTPClient();
    HTTPClientArgs args;
    args.url = "https://plywood.dev";
    sendHTTPRequest(client, std::move(args));
    Functor<void(StringView, bool)> callback = [](StringView data, bool isEnd) {
        if (isEnd) {
            getStdErr().format("Request finished: {}\n", data);
        } else {
            getStdOut().write(data);
        }
    };
    while (waitForHTTPResponse(client, callback)) {
        // Response data is delivered to `callback` inside waitForHTTPResponse().
    }
    client.clear();

    // Shut down libcurl.
    curl_global_cleanup();
    return 0;
}
```

`Owned<HTTPClient> createHTTPClient()`
> Creates a new `HTTPClient`.

`void destroy(HTTPClient* httpClient)`
> Destroys an `HTTPClient` created by `createHTTPClient()`. Any in-progress request is cancelled first.

`void sendHTTPRequest(HTTPClient* httpClient, HTTPClientArgs&& args)`
> Starts a new request. Must not be called while a request is already in progress.
> `HTTPClientArgs` is defined as follows:
> ```
> struct HTTPClientArgs {
>     String url;
>     Map<String, String> headers;
>     String body;
>     bool useBundledCaCert = true;
> };
> ```

`void cancelHTTPRequest(HTTPClient* httpClient)`
> Cancels any request in progress. After this returns, `isHTTPRequestInProgress()` returns `false`.
> It's OK to call `sendHTTPRequest` on the same `HTTPClient` again after this.

`bool isHTTPRequestInProgress(const HTTPClient* httpClient)`
> Returns `true` while a request is in progress.

`bool waitForHTTPResponse(HTTPClient* httpClient, const Functor<void(StringView, bool)>& callback, u32 timeOutMillis = 1000)`
> Drives the request, delivering incoming response data to `callback`. If response data is available, it invokes
> `callback` and returns immediately. If no data is available, it waits up to `timeOutMillis` (interruptible by
> `wakeUpHTTPClient()`). Returns `false` once the request completes or is cancelled; `true` if it is still in
> progress. `callback` is invoked with `(chunk, false)` for each chunk of
> response data, and one final time with `(errorMessage, true)` if the request fails.

`void wakeUpHTTPClient(HTTPClient* httpClient)`
> Can be called from any thread. If `waitForHTTPResponse()` is currently blocked in another thread, it returns
> immediately; otherwise the next `waitForHTTPResponse()` call returns immediately.

## HTTP Server

Setting `PLY_WITH_HTTP_SERVER=1` enables the `runHTTPServer` function, which runs an HTTP server.

```
#include <ply-network.h>

using namespace ply;

void serverCallback(HTTPServerRequest& request) {
    HTTPServerResponse response{HTTPServerResponse::OK};
    *response.headers.insert("content-type").value = "text/plain";
    request.sendFullResponse(std::move(response), String::format("Request URI: {}\n", request.uri));
}

int main() {
    // Initialize the network.
    Network::initialize(IPV4);

    // Run a webserver.
    runHTTPServer(8080, serverCallback);

    // Shut down the network.
    Network::shutdown();
    return 0;
}
```

`void runHTTPServer(u16 port, const Functor<void(HTTPServerRequest& request)>& requestHandler)`
> When this function is called, the calling thread is blocked for as long as the server keeps running.
> `Network::initialize()` must be called first.
> `requestHandler` is a user-provided callback function that handles each HTTP request.

### `HTTPServerRequest`

`HTTPServerRequest` contains all the information the callback function needs to handle the request
and exposes member functions for responding to the request.

{context class=HTTPServerRequest}

`IPAddress clientAddr`
`u16 clientPort`
> The remote TCP peer address and port.

`String method`
> The request method, such as `GET`, `HEAD`, `POST`, `PUT` or `PATCH`.

`String uri`
> The raw request URI.

`String httpVersion`
> The HTTP version token, such as `HTTP/1.1`.

`Map<String, String> headers`
> Request headers indexed by lower-case header name. For example, use `request.headers.find("content-type")` rather
> than `request.headers.find("Content-Type")`.

`String body`
> The request body bytes. This string can contain arbitrary binary data; it is not guaranteed to be null-terminated.

`void sendFullResponse(HTTPServerResponse&& response, StringView body = {})`
> Sends a complete response. The connection can be reused for additional requests when HTTP rules permit it.

`Stream beginStreamingResponse(HTTPServerResponse&& response)`
> Sends response headers and returns the TCP output stream for raw body bytes.
> The connection is closed when the caller destructs the returned stream.

`void sendGenericResponse(HTTPServerResponse::Code responseCode)`
> Writes a minimal HTML error page for the given status code.

### `HTTPServerResponse`

`HTTPServerResponse` is an argument passed to several `HTTPServerRequest` member functions. It has the following members:

{context class=HTTPServerResponse}

`Code code`
> The HTTP status code to emit. Must be one of the following enumerator values:
>
> | | |
> | --- | --- |
> | `Code::OK` | 200 |
> | `Code::PermanentRedirect` | 301 |
> | `Code::TemporaryRedirect` | 302 |
> | `Code::BadRequest` | 400 |
> | `Code::NotFound` | 404 |
> | `Code::InternalError` | 500 |

`Map<String, String> headers`
> Response headers to emit.
