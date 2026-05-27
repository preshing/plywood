/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-http.h"

namespace ply {

// Return true for methods that commonly carry a request body even without framing headers
bool expectsRequestBody(StringView method) {
    // Treat body-bearing methods without an explicit length as EOF-delimited.
    String lowerMethod = method.lower();
    return lowerMethod == "post" || lowerMethod == "put" || lowerMethod == "patch";
}

// Copy an exact number of bytes from one stream to another
bool copyExactBytes(Stream& in, Stream& out, u64 numBytes) {
    // Keep copying until the exact byte count has been transferred.
    while (numBytes > 0) {
        // Require readable source bytes and writable destination space.
        if (!in.makeReadable() || !out.makeWritable())
            return false;
        u32 toCopy = min<u64>(numBytes, min(in.numRemainingBytes(), out.numRemainingBytes()));
        memcpy(out.curByte, in.curByte, toCopy);
        in.curByte += toCopy;
        out.curByte += toCopy;
        numBytes -= toCopy;
    }
    return true;
}

// Read the rest of the input stream into memory until the peer closes the connection
String readBodyUntilEof(Stream& in) {
    MemStream mem;

    // Copy all available input chunks until the socket reaches EOF.
    while (in.makeReadable() && mem.makeWritable()) {
        // Copy the largest chunk that both stream buffers can handle.
        u32 toCopy = min(in.numRemainingBytes(), mem.numRemainingBytes());
        memcpy(mem.curByte, in.curByte, toCopy);
        in.curByte += toCopy;
        mem.curByte += toCopy;
    }
    return mem.moveToString();
}

// Parse a decimal Content-Length value
bool readContentLength(StringView value, u64* contentLength) {
    // Read the numeric prefix from a trimmed header value.
    ViewStream vs(value.trim());
    *contentLength = readU64FromText(vs);
    if (vs.inputError)
        return false;

    // Accept only trailing whitespace after the number.
    while (vs.makeReadable()) {
        // Reject any non-whitespace suffix after Content-Length.
        if (!isWhite(vs.readByte()))
            return false;
    }
    return true;
}

// Read an HTTP/1.1 chunked request body, ignoring chunk extensions and trailers
bool readChunkedBody(Stream& in, String* body) {
    MemStream mem;

    // Decode each chunk until the terminal zero-sized chunk is reached.
    for (;;) {
        // Read and validate the next chunk-size line.
        String chunkHeader = readLine(in);
        if (chunkHeader.isEmpty() && in.atEof)
            return false;
        StringView chunkSizeText = chunkHeader.trim();

        // Ignore any chunk extension after the hexadecimal size.
        s32 semicolonPos = chunkSizeText.find(';');
        if (semicolonPos >= 0) {
            // Keep only the hexadecimal chunk size.
            chunkSizeText = chunkSizeText.left(semicolonPos).trimRight();
        }

        // Parse the chunk size as hexadecimal text.
        ViewStream chunkSizeStream(chunkSizeText);
        u64 chunkSize = readU64FromText(chunkSizeStream, 16);
        if (chunkSizeStream.inputError)
            return false;

        // Accept only trailing whitespace after the chunk size.
        while (chunkSizeStream.makeReadable()) {
            // Reject any non-whitespace suffix after the chunk size.
            if (!isWhite(chunkSizeStream.readByte()))
                return false;
        }

        // Consume trailers after the terminal chunk.
        if (chunkSize == 0) {
            // Read trailer lines until the blank line that ends chunked framing.
            for (;;) {
                // Stop once the trailer section terminator is found.
                String trailerLine = readLine(in);
                if (trailerLine.isEmpty() && in.atEof)
                    return false;
                if (trailerLine.trim().isEmpty())
                    break;
            }
            *body = mem.moveToString();
            return true;
        }

        // Append the current chunk data and consume its terminating CRLF.
        if (!copyExactBytes(in, mem, chunkSize))
            return false;
        String terminator = readLine(in);
        if (terminator.trim() != "")
            return false;
    }
}

// Determine whether this request can keep the TCP connection open
bool shouldKeepConnectionAlive(const Request& request) {
    // Look up the canonical lower-case Connection header name.
    const String* connectionPtr = request.headers.find("connection");
    String connection = connectionPtr ? connectionPtr->trim().lower() : "";
    bool closeRequested = connection == "close";
    bool keepAliveRequested = connection == "keep-alive";
    if (closeRequested)
        return false;
    if (request.httpVersion == "HTTP/1.1")
        return true;
    return keepAliveRequested;
}

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
    // Remember the first status code chosen for this response.
    if (!this->hasBegun) {
        // Store the response status for the final write.
        this->responseCode = responseCode;
        this->hasBegun = true;
    }
    return &this->body;
}

// Write HTTP status line, headers and buffered body
void Response::finish(Stream& out, bool keepAlive, bool sendBody) {
    // Use a generic error status if the handler never started a response.
    if (!this->hasBegun) {
        // Default unfinished responses to an internal server error.
        this->responseCode = InternalError;
        this->hasBegun = true;
    }

    // Move the buffered response body into a stable string for header generation.
    String bodyBytes = this->body.moveToString();

    if (!this->headers.find("content-length")) {
        // Add the body length required for persistent connections.
        *this->headers.insert("content-length").value = String::format("{}", bodyBytes.numBytes());
    }
    if (!this->headers.find("connection")) {
        // Advertise the connection state chosen by the request loop.
        *this->headers.insert("connection").value = keepAlive ? "keep-alive" : "close";
    }

    // Emit the response head before writing the buffered body bytes.
    StringView message = getResponseDescription(this->responseCode);
    out.format("HTTP/1.1 {} {}\r\n", this->responseCode, message);
    for (const auto& item : headers.items()) {
        // Write each response header line.
        out.format("{}: {}\r\n", item.key, item.value);
    }
    out.write("\r\n");
    if (sendBody) {
        // Skip the body for HEAD responses but still report its length.
        out.write(bodyBytes);
    }

    // Flush the complete response so the client can parse the next response.
    out.flush(true);
}

// Write a minimal HTML error page with the given status code
void sendGenericResponse(Response& response, Response::Code responseCode) {
    // Store the response header name in its canonical map form.
    *response.headers.insert("content-type").value = "text/html";
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
    /*
    This function owns the HTTP request loop for one accepted TCP connection:
    - Create buffered input/output streams once so unread bytes can carry across reused requests.
    - Parse one request line and header block, storing header names as lower-case map keys.
    - Read the request body using chunked framing, Content-Length framing, or EOF for body-bearing methods.
    - Invoke the handler with a complete Request and a Response that buffers its body in memory.
    - Finish the response by adding Content-Length/Connection defaults, then decide whether to loop again.

    The common reusable path is HTTP/1.1 with Content-Length, chunked encoding, or no body. In that case,
    the response is flushed and the loop continues so the next request can be read from the same socket.

    The non-reusable body path is POST, PUT or PATCH without Content-Length or chunked encoding. The body is
    read until socket EOF, so there is no delimiter left that could separate another request; the response is
    sent with connection: close and this function returns.

    The early-exit paths are peer EOF before a new request, malformed request syntax, malformed body framing,
    request connection: close, response connection: close, and HTTP/1.0 without connection: keep-alive.
    */
    Stream in = tcpConn->createInStream();
    Stream out = tcpConn->createOutStream();

    // Reuse the connection until either peer EOF or HTTP connection rules stop the loop.
    for (;;) {
        // Create request and response objects
        Request request;
        request.clientAddr = tcpConn->remoteAddress();
        request.clientPort = tcpConn->remotePort();
        Response response;

        // Parse HTTP request line, ignoring empty lines before a request
        String requestLine;
        do {
            // Read through leading blank lines permitted before the request line.
            requestLine = readLine(in);
            if (requestLine.isEmpty() && in.atEof)
                return;
        } while (requestLine.trim().isEmpty());

        Array<StringView> tokens = requestLine.trimRight().split(" ");
        if (tokens.numItems() != 3) {
            // Ill-formed request
            sendGenericResponse(response, Response::BadRequest);
            response.finish(out, false, true);
            return;
        }
        request.method = tokens[0];
        request.uri = tokens[1];
        request.httpVersion = tokens[2];

        // Parse HTTP headers
        for (;;) {
            // Read one header line at a time until the blank separator.
            String line = readLine(in);
            if (line.isEmpty() && in.atEof) {
                // Treat EOF in the header section as a malformed request.
                sendGenericResponse(response, Response::BadRequest);
                response.finish(out, false, true);
                return;
            }
            if (line.trim().isEmpty())
                break; // Blank line
            if (isWhite(line[0]))
                continue; // FIXME: Support unfolding https://tools.ietf.org/html/rfc822#section-3.1
            s32 colonPos = line.find(':');
            if (colonPos < 0) {
                // Ill-formed request
                sendGenericResponse(response, Response::BadRequest);
                response.finish(out, false, true);
                return;
            }
            *request.headers.insert(line.left(colonPos).trim().lower()).value = line.substr(colonPos + 1).trim();
        }

        // Read request body based on HTTP framing headers
        bool closeAfterRequest = false;
        String* transferEncodingPtr = request.headers.find("transfer-encoding");
        String* contentLengthPtr = request.headers.find("content-length");
        if (transferEncodingPtr && transferEncodingPtr->trim().lower() == "chunked") {
            // Chunked transfer coding provides reusable request framing.
            if (!readChunkedBody(in, &request.body)) {
                // Reject malformed chunked bodies.
                sendGenericResponse(response, Response::BadRequest);
                response.finish(out, false, true);
                return;
            }
        } else if (contentLengthPtr) {
            // Content-Length provides reusable request framing.
            u64 contentLength = 0;
            if (!readContentLength(*contentLengthPtr, &contentLength) || contentLength > getMaxValue<u32>()) {
                // Reject invalid or unrepresentable request body lengths.
                sendGenericResponse(response, Response::BadRequest);
                response.finish(out, false, true);
                return;
            }
            if (contentLength > 0) {
                // Read the declared number of body bytes into the request.
                request.body = String::allocate(numericCast<u32>(contentLength));
                if (in.read(request.body.mutStringView()) != contentLength) {
                    // Reject truncated request bodies.
                    sendGenericResponse(response, Response::BadRequest);
                    response.finish(out, false, true);
                    return;
                }
            }
        } else if (expectsRequestBody(request.method)) {
            // EOF-delimited bodies cannot be followed by another request on the same connection.
            request.body = readBodyUntilEof(in);
            closeAfterRequest = true;
        }

        // Invoke request handler and finish the HTTP response
        bool keepAlive = shouldKeepConnectionAlive(request) && !closeAfterRequest;
        reqHandler(request, response);

        // Let the response opt out of connection reuse.
        const String* responseConnectionPtr = response.headers.find("connection");
        if (responseConnectionPtr && responseConnectionPtr->trim().lower() == "close") {
            // Honor an explicit response-level close request.
            keepAlive = false;
        }
        response.finish(out, keepAlive, request.method.lower() != "head");
        if (!keepAlive)
            return;
    }
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

        // Transfer ownership through the thread callback using a raw pointer.
        TCPConnection* tcpConnPtr = tcpConn.release();
        spawnThread([tcpConnPtr, &reqHandler] {
            // Rebuild owned connection storage inside the request thread.
            Owned<TCPConnection> tcpConn = Owned<TCPConnection>::adopt(tcpConnPtr);
            handleHttpRequest(tcpConn.get(), reqHandler);
        });
    }
}

} // namespace ply
