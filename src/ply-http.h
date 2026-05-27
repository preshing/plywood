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

// Parsed HTTP request
struct Request {
    IPAddress clientAddr;
    u16 clientPort = 0;
    String method;
    String uri;
    String httpVersion;
    Map<String, String> headers;
    String body;
};

// Forward declaration for friend declaration below
class Response;
// Callback invoked for each parsed HTTP request
using RequestHandler = Functor<void(const Request& request, Response& response)>;

// HTTP response builder
class Response {
public:
    enum Code {
        OK = 200,
        PermanentRedirect = 301,
        TemporaryRedirect = 302,
        BadRequest = 400,
        NotFound = 404,
        InternalError = 500,
    };

private:
    MemStream body;
    Code responseCode = InternalError;
    bool hasBegun = false;
    friend void handleHttpRequest(TCPConnection* tcpConn, const RequestHandler& reqHandler);

    void finish(Stream& out, bool keepAlive, bool sendBody);

public:
    Map<String, String> headers;

    Stream* begin(Code responseCode);
};

// Send a minimal HTML error page for the given status code
void sendGenericResponse(Response& response, Response::Code responseCode);
// Bind to a port and run an HTTP server that dispatches to the given handler
void runHttpServer(u16 port, const RequestHandler& reqHandler);

} // namespace ply
