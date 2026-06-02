/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once

#include "ply-base.h"
#include "ply-network.h"

namespace ply {

// HTTPResponse
struct HTTPResponse {
    enum Code {
        OK = 200,
        PermanentRedirect = 301,
        TemporaryRedirect = 302,
        BadRequest = 400,
        NotFound = 404,
        InternalError = 500,
    };

    Code code = InternalError;
    // Header keys are stored in all lowercase.
    Map<String, String> headers;

    explicit HTTPResponse(Code code) : code{code} {
    }
};

// HTTPRequest
// Note: There are additional members hidden in a subclass that are used internally.
struct HTTPRequest {
    IPAddress clientAddr;
    u16 clientPort = 0;
    String method;
    String uri;
    String httpVersion;
    Map<String, String> headers;
    String body;

    // Request handlers can call this to send a complete response including provided headers and body.
    // (The underlying TCP connection may be reused for other requests/responses.)
    void sendFullResponse(HTTPResponse&& response, StringView body = {}) const;
    // This will send HTTP headers only. HTTPResponse::code must be OK.
    // After that, the request handler is expected to write "streaming" data (typically just lines of JSONL
    // over a TCP connection) to the responseStream before returning. The http server will automatically
    // close the connection when the request handler returns.
    Stream beginStreamingResponse(HTTPResponse&& response) const;
    // Send a minimal HTML error page with the given HTTP status code.
    void sendGenericResponse(HTTPResponse::Code responseCode) const;
};

// Callback invoked for each parsed HTTP request.
using HttpRequestHandler = Functor<void(const HTTPRequest& request)>;

// Bind to a port and run an HTTP server that dispatches to the given handler
void runHttpServer(u16 port, const HttpRequestHandler& reqHandler);

} // namespace ply
