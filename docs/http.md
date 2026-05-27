{title text="HTTP Server" include="ply-http.h" namespace="ply"}

Plywood includes a small HTTP server helper in `ply-http.h`. It is intended for simple tools, local
documentation servers and test servers that need a portable HTTP interface without pulling in a larger framework.

The webserver builds on the TCP/IP networking API, so applications must call `Network::initialize()` before starting
the server and `Network::shutdown()` after it returns.

{example}
#include <ply-http.h>

using namespace ply;

void servePage(const Request& request, Response& response) {
    *response.headers.insert("content-type").value = "text/plain";
    Stream* out = response.begin(Response::OK);
    out->format("Request URI: {}\n", request.uri);
}

int main() {
    Network::initialize(IPV4);
    runHttpServer(8080, servePage);
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

## Request body handling

`handleHttpRequest()` reads the request body before invoking the handler.

If the request contains `transfer-encoding: chunked`, the server decodes the chunks and stores the decoded bytes in
`Request::body`. Chunk extensions are ignored and trailer fields are consumed.

If the request contains `content-length`, the server reads exactly that many bytes into `Request::body`. Invalid,
unrepresentable or truncated lengths cause a `400 Bad Request` response.

If neither header is present and the method is `POST`, `PUT` or `PATCH`, the server reads from the input socket until
EOF and stores all remaining bytes in `Request::body`. This form is useful for simple clients, but it cannot be
combined with connection reuse because EOF is the only body terminator. The server closes the connection after
responding to such a request.

Requests without a body framing header and without one of those body-bearing methods have an empty `Request::body`.

## Header names

HTTP header names are case-insensitive, but Plywood stores them in maps using lower-case keys. This applies to parsed
request headers and to headers inserted by the built-in response helpers.

When writing handlers, insert response headers using lower-case names:

{example}
*response.headers.insert("content-type").value = "text/html";
*response.headers.insert("location").value = "/docs/intro";
{/example}

The webserver does not normalize response headers after the handler returns. If a handler inserts `Content-Type` and
the server later looks for `content-type`, those are different map keys. Keep header keys lower-case at insertion time.

## `Response`

`Response` is passed by mutable reference to the request handler. The handler sets response headers, calls
`Response::begin()` with a status code, then writes the body to the returned stream.

{apiSummary class=Response}
Map<String, String> headers
Stream* begin(Code responseCode)
{/apiSummary}

{context class=Response}

`Map<String, String> headers`
> Response headers to emit. Insert lower-case header names.

`Stream* begin(Code responseCode)`
> Starts the response body for the given status code and returns a stream that buffers the body bytes. The webserver
> writes the HTTP status line, headers and buffered body after the handler returns.

`Response::Code` contains the status codes currently named by the helper: `OK`, `PermanentRedirect`,
`TemporaryRedirect`, `BadRequest`, `NotFound` and `InternalError`.

Response bodies are buffered in memory. This lets the webserver add a correct `content-length` header automatically,
which is required for reliable connection reuse. If the handler does not insert `content-length`, the buffered body
size is used. If the handler does not insert `connection`, the server writes `connection: keep-alive` or
`connection: close` according to the connection state.

For `HEAD` requests, the handler can generate the body normally. The server still computes `content-length`, but it
does not write the body bytes to the socket.

## Connection reuse

`handleHttpRequest()` supports multiple HTTP requests over one TCP connection. It loops over requests on the same
connection until EOF, a malformed request, an EOF-delimited request body, or an explicit close directive stops reuse.

For HTTP/1.1 requests, the connection is kept open by default unless the request contains `connection: close`.

For HTTP/1.0 requests, the connection is closed by default unless the request contains `connection: keep-alive`.

A handler can force the connection to close by inserting:

{example}
*response.headers.insert("connection").value = "close";
{/example}

## `sendGenericResponse`

{apiSummary}
void sendGenericResponse(Response& response, Response::Code responseCode)
{/apiSummary}

Writes a minimal HTML error page for the given status code. It sets `content-type: text/html` and writes a short
response body.

## `runHttpServer`

{apiSummary}
void runHttpServer(u16 port, const RequestHandler& reqHandler)
{/apiSummary}

`runHttpServer()` binds a TCP listener to `port`, accepts connections, and starts one thread per accepted TCP
connection. Each connection thread calls the request handler once for each parsed HTTP request on that connection.

The function runs until the listener fails or is closed. If binding fails, it writes an error to standard error and
returns.
