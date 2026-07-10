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

// HTTPServerResponse
struct HTTPServerResponse {
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
    explicit HTTPServerResponse(Code code) : code{code} {
    }
};

// HTTPServerRequest has additional hidden members in the HTTPRequestImpl subclass.
struct HTTPServerRequest {
    IPAddress clientAddr;
    u16 clientPort = 0;
    String method;
    String uri;
    String httpVersion;
    Map<String, String> headers;
    String body;

    // Request handlers can call this to send a complete response including provided headers and body.
    // (The underlying TCP connection may be reused for other requests/responses.)
    void sendFullResponse(HTTPServerResponse&& response, StringView body = {});
    // This will send HTTP headers only. HTTPServerResponse::code must be OK.
    // After that, the request handler is expected to write "streaming" data (typically just lines of JSONL
    // over a TCP connection) to the responseStream before returning. The http server will automatically
    // close the connection when the request handler returns.
    Stream beginStreamingResponse(HTTPServerResponse&& response);
    // Send a minimal HTML error page with the given HTTP status code.
    void sendGenericResponse(HTTPServerResponse::Code responseCode);
};

// Bind to a port and run an HTTP server that dispatches to the given handler.
void runHTTPServer(u16 port, const Functor<void(HTTPServerRequest& request)>& reqHandler);

// Built-in request handler that simply echoes the client's address and request headers (for testing).
void serveEchoPage(HTTPServerRequest& request);

} // namespace ply
