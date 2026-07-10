{title text="HTTP" include="ply-http.h" namespace="ply"}

`ply-http.h` provides two independent helpers: an `HTTPClient` for making outgoing HTTP requests, and a small HTTP
server for simple tools, local documentation servers and test servers that need a portable HTTP interface without
pulling in a larger framework.

The HTTP client builds on libcurl, so libcurl (with OpenSSL) must be available at build time when
`PLY_WITH_HTTP_CLIENT` is enabled (the default). The HTTP server builds on the TCP/IP networking API, so applications
must call `Network::initialize()` before starting the server and `Network::shutdown()` after it returns.

## `HTTPClient`

`HTTPClient` is a libcurl wrapper that encapsulates a single HTTP request and can be interrupted from another thread.
The same `HTTPClient` object can be reused across multiple requests.

Except for `wakeUpHTTPClient`, the `HTTPClient` API is not thread-safe and is intended to be driven from a single
thread. The typical request lifecycle is:

1. Call `sendHTTPRequest()` to start a request.
2. Call `waitForHTTPResponse()` repeatedly, processing response data delivered to the callback, until it returns
   `false`.
3. The request can be aborted early with `cancelHTTPRequest()`, or interrupted from another thread with
   `wakeUpHTTPClient()` so that a blocked `waitForHTTPResponse()` returns promptly.

`HTTPClient` is an opaque type; create instances with `createHTTPClient()` and destroy them with `destroy()` (or let
the returned `Owned<HTTPClient>` go out of scope).

### `HTTPClientArgs`

`HTTPClientArgs` describes a request to send.

{apiSummary class=HTTPClientArgs}
String url
Map<String, String> headers
String body
bool useBundledCaCert = false
{/apiSummary}

{context class=HTTPClientArgs}

`String url`
> The request URL.

`Map<String, String> headers`
> HTTP headers to send with the request, e.g. `{"Content-Type" => "application/json"}`.

`String body`
> The request body. Must remain valid for the lifetime of the request because libcurl reads from it directly.

`bool useBundledCaCert = false`
> When `true`, verify the peer against the `cacert.pem` bundle shipped next to the executable. When `false`, TLS
> verification is disabled, which is only appropriate for trusted localhost endpoints.

### Client functions

{apiSummary}
Owned<HTTPClient> createHTTPClient()
void destroy(HTTPClient* httpClient)
void sendHTTPRequest(HTTPClient* httpClient, HTTPClientArgs&& args)
void cancelHTTPRequest(HTTPClient* httpClient)
bool isHTTPRequestInProgress(const HTTPClient* httpClient)
bool waitForHTTPResponse(HTTPClient* httpClient, const Functor<void(StringView, bool)>& callback, u32 timeOutMillis = 1000)
void wakeUpHTTPClient(HTTPClient* httpClient)
{/apiSummary}

`Owned<HTTPClient> createHTTPClient()`
> Creates a new `HTTPClient`.

`void destroy(HTTPClient* httpClient)`
> Destroys an `HTTPClient` created by `createHTTPClient()`. Any in-progress request is cancelled first.

`void sendHTTPRequest(HTTPClient* httpClient, HTTPClientArgs&& args)`
> Starts a new request. Must not be called while a request is already in progress.

`void cancelHTTPRequest(HTTPClient* httpClient)`
> Cancels any request in progress. After this returns, `isHTTPRequestInProgress()` returns `false`.

`bool isHTTPRequestInProgress(const HTTPClient* httpClient)`
> Returns `true` while a request is in progress.

`bool waitForHTTPResponse(HTTPClient* httpClient, const Functor<void(StringView, bool)>& callback, u32 timeOutMillis = 1000)`
> Drives the request, delivering incoming response data to `callback`. If response data is available, it invokes
> `callback` and returns immediately. If no data is available, it waits up to `timeOutMillis` (interruptible by
> `wakeUpHTTPClient()`). Returns `false` once the request completes or is cancelled; `true` if it is still in
> progress. A no-op when no request is in progress. `callback` is invoked with `(chunk, false)` for each chunk of
> response data, and one final time with `(errorMessage, true)` if the request fails.

`void wakeUpHTTPClient(HTTPClient* httpClient)`
> Can be called from any thread. If `waitForHTTPResponse()` is currently blocked in another thread, it returns
> immediately; otherwise the next `waitForHTTPResponse()` call returns immediately.

{example}
#include <ply-http.h>

using namespace ply;

void fetchPage() {
    Owned<HTTPClient> client = createHTTPClient();
    HTTPClientArgs args;
    args.url = "https://example.com";
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
}
{/example}

## HTTP Server

{example}
#include <ply-http.h>

using namespace ply;

void servePage(HTTPRequest& request) {
    HTTPResponse response{HTTPResponse::OK};
    *response.headers.insert("content-type").value = "text/plain";
    request.sendFullResponse(std::move(response), String::format("Request URI: {}\n", request.uri));
}

int main() {
    Network::initialize(IPV4);
    runHTTPServer(8080, servePage);
    Network::shutdown();
    return 0;
}
{/example}

## `HTTPRequest`

`HTTPRequest` contains the parsed HTTP request passed to the request handler.

{apiSummary class=HTTPRequest}
IPAddress clientAddr
u16 clientPort
String method
String uri
String httpVersion
Map<String, String> headers
String body
void sendFullResponse(HTTPResponse&& response, StringView body = {})
Stream beginStreamingResponse(HTTPResponse&& response)
void sendGenericResponse(HTTPResponse::Code responseCode)
{/apiSummary}

{context class=HTTPRequest}

`IPAddress clientAddr`
`u16 clientPort`
> The remote TCP peer address and port.

`String method`
> The request method from the request line, such as `GET`, `HEAD`, `POST`, `PUT` or `PATCH`.

`String uri`
> The raw request URI from the request line. It may include a query string.

`String httpVersion`
> The HTTP version token from the request line, such as `HTTP/1.1`.

`Map<String, String> headers`
> Request headers indexed by lower-case header name. For example, use `request.headers.find("content-type")` rather
> than `request.headers.find("Content-Type")`. Header values are trimmed when parsed, but are otherwise stored as
> sent by the client.

`String body`
> The request body bytes. This string can contain arbitrary binary data; it is not guaranteed to be null-terminated.

`void sendFullResponse(HTTPResponse&& response, StringView body = {})`
> Sends a complete response. The connection can be reused for additional requests when HTTP rules permit it.

`Stream beginStreamingResponse(HTTPResponse&& response)`
> Sends response headers and returns the TCP output stream for raw body bytes. Does not use chunked encoding.
> The connection is closed when the caller destructs the returned stream.

`void sendGenericResponse(HTTPResponse::Code responseCode)`
> Writes a minimal HTML error page for the given status code.

## `HTTPResponse`

`HTTPResponse` carries the response code and headers passed to `HTTPRequest::sendFullResponse()` or
`HTTPRequest::beginStreamingResponse()`.

{apiSummary class=HTTPResponse}
Code code
Map<String, String> headers
{/apiSummary}

{context class=HTTPResponse}

`Code code`
> The HTTP status code to emit.

`Map<String, String> headers`
> Response headers to emit. Insert lower-case header names.

`HTTPResponse::Code` contains the status codes currently named by the helper: `OK`, `PermanentRedirect`,
`TemporaryRedirect`, `BadRequest`, `NotFound` and `InternalError`.

`sendFullResponse()` adds a correct `content-length` header automatically when the handler does not provide one. This
is the normal path for reliable connection reuse. If the handler does not insert `connection`, the server writes
`connection: keep-alive` or `connection: close` according to the connection state.

For `HEAD` requests, the handler can pass the body normally. The server still computes `content-length`, but it does
not write the body bytes to the socket.

## `runHTTPServer`

{apiSummary}
void runHTTPServer(u16 port, const Functor<void(HTTPRequest& request)>& reqHandler)
{/apiSummary}

`runHTTPServer()` binds a TCP listener to `port`, accepts connections, and starts one thread per accepted TCP
connection. Each connection thread calls the request handler once for each parsed HTTP request on that connection.

The function runs until the listener fails or is closed. If binding fails, it writes an error to standard error and
returns.

## `serveEchoPage`

{apiSummary}
void serveEchoPage(HTTPRequest& request)
{/apiSummary}

`serveEchoPage()` is a built-in request handler for testing. It writes an HTML page that includes the remote address
and parsed request header.