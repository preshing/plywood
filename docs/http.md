{title text="HTTP Server" include="ply-http.h" namespace="ply::http"}

Plywood includes a small HTTP server helper in `ply-http.h`. It is intended for simple tools, local
documentation servers and test servers that need a portable HTTP interface without pulling in a larger framework.

The HTTP server builds on the TCP/IP networking API, so applications must call `Network::initialize()` before starting
the server and `Network::shutdown()` after it returns.

{example}
#include <ply-http.h>

using namespace ply;

void servePage(http::Request& request) {
    http::Response response{http::Response::OK};
    *response.headers.insert("content-type").value = "text/plain";
    request.sendFullResponse(std::move(response), String::format("Request URI: {}\n", request.uri));
}

int main() {
    Network::initialize(IPV4);
    http::runServer(8080, servePage);
    Network::shutdown();
    return 0;
}
{/example}

## `Request`

`Request` contains the parsed HTTP request passed to the request handler.

{apiSummary class=Request}
IPAddress clientAddr
u16 clientPort
String method
String uri
String httpVersion
Map<String, String> headers
String body
void sendFullResponse(Response&& response, StringView body = {})
Stream beginStreamingResponse(Response&& response)
void sendGenericResponse(Response::Code responseCode)
{/apiSummary}

{context class=Request}

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

`void sendFullResponse(Response&& response, StringView body = {})`
> Sends a complete response. The connection can be reused for additional requests when HTTP rules permit it.

`Stream beginStreamingResponse(Response&& response)`
> Sends response headers and returns the TCP output stream for raw body bytes. Does not use chunked encoding.
> The connection is closed when the caller destructs the returned stream.

`void sendGenericResponse(Response::Code responseCode)`
> Writes a minimal HTML error page for the given status code.

## `Response`

`Response` carries the response code and headers passed to `Request::sendFullResponse()` or
`Request::beginStreamingResponse()`.

{apiSummary class=Response}
Code code
Map<String, String> headers
{/apiSummary}

{context class=Response}

`Code code`
> The HTTP status code to emit.

`Map<String, String> headers`
> Response headers to emit. Insert lower-case header names.

`Response::Code` contains the status codes currently named by the helper: `OK`, `PermanentRedirect`,
`TemporaryRedirect`, `BadRequest`, `NotFound` and `InternalError`.

`sendFullResponse()` adds a correct `content-length` header automatically when the handler does not provide one. This
is the normal path for reliable connection reuse. If the handler does not insert `connection`, the server writes
`connection: keep-alive` or `connection: close` according to the connection state.

For `HEAD` requests, the handler can pass the body normally. The server still computes `content-length`, but it does
not write the body bytes to the socket.

## `runServer`

{apiSummary}
void runServer(u16 port, const Functor<void(Request& request)>& reqHandler)
{/apiSummary}

`runServer()` binds a TCP listener to `port`, accepts connections, and starts one thread per accepted TCP
connection. Each connection thread calls the request handler once for each parsed HTTP request on that connection.

The function runs until the listener fails or is closed. If binding fails, it writes an error to standard error and
returns.

## `serveEchoPage`

{apiSummary}
void serveEchoPage(Request& request)
{/apiSummary}

`serveEchoPage()` is a built-in request handler for testing. It writes an HTML page that includes the remote address
and parsed request header.
