/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-webserver.h"

namespace ply {

// Map HTTP status code to standard reason phrase
StringView getResponseDescription(Response::Code responseCode) {
    switch (responseCode) {
        case Response::OK:
            return "OK";
        case Response::PermanentRedirect:
            return "Moved Permanently";
        case Response::TemporaryRedirect:
            return "Found";
        case Response::BadRequest:
            return "Bad Request";
        case Response::NotFound:
            return "Not Found";
        case Response::InternalError:
        default:
            return "Internal Server Error";
    }
}

// Write HTTP status line + headers, then return the stream for body content
Stream* Response::begin(Response::Code responseCode) {
    StringView message = getResponseDescription(responseCode);
    out->format("HTTP/1.1 {} {}\r\n", responseCode, message);
    for (const auto& item : headers.items()) {
        out->format("{}: {}\r\n", item.key, item.value);
    }
    out->write("\r\n");
    return out;
}

// Write a minimal HTML error page with the given status code
void sendGenericResponse(Response& response, Response::Code responseCode) {
    *response.headers.insert("Content-type").value = "text/html";
    Stream* out = response.begin(responseCode);
    StringView message = getResponseDescription(responseCode);
    out->format(R"(<html>
<head><title>{} {}</title></head>
<body>
<center><h1>{} {}</h1></center>
<hr>
</body>
</html>
)",
                responseCode, message, responseCode, message);
}

// Parse an HTTP request from a TCP connection and dispatch it to the handler
void handleHttpRequest(TCPConnection* tcpConn, const RequestHandler& reqHandler) {
    Stream in = tcpConn->createInStream();
    Stream out = tcpConn->createOutStream();

    // Create request and response objects
    Request request;
    request.clientAddr = tcpConn->remoteAddress();
    request.clientPort = tcpConn->remotePort();
    Response response;
    response.out = &out;

    // Parse HTTP request line
    String requestLine = readLine(in);
    Array<StringView> tokens = requestLine.trimRight().split(" ");
    if (tokens.numItems() != 3) {
        // Ill-formed request
        sendGenericResponse(response, Response::BadRequest);
        return;
    }
    request.method = tokens[0];
    request.uri = tokens[1];
    request.httpVersion = tokens[2];

    // Parse HTTP headers
    for (;;) {
        String line = readLine(in);
        if (line.trim().isEmpty())
            break; // Blank line
        if (isWhite(line[0]))
            continue; // FIXME: Support unfolding https://tools.ietf.org/html/rfc822#section-3.1
        s32 colonPos = line.find(':');
        if (colonPos < 0) {
            // Ill-formed request
            sendGenericResponse(response, Response::BadRequest);
            return;
        }
        *request.headers.insert(line.left(colonPos).trim()).value = line.substr(colonPos + 1).trim();
    }

    // Invoke request handler
    reqHandler(request, response);
}

// Accept connections on a port and handle each request in a new thread
void runHttpServer(u16 port, const RequestHandler& reqHandler) {
    TCPListener listener = Network::bindTcp(port);
    if (!listener.isValid()) {
        getStdErr().format("Error: Can't bind to port {}\n", port);
        return;
    }

    for (;;) {
        Owned<TCPConnection> tcpConn = listener.accept();
        if (!tcpConn)
            break;
        spawnThread([tcpConn = std::move(tcpConn), &reqHandler] { handleHttpRequest(tcpConn.get(), reqHandler); });
    }
}

} // namespace ply
