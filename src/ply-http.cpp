/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-http.h"

#if PLY_WITH_HTTP_CLIENT
// HTTPClient requires curl.
#include <curl/curl.h>
#include <openssl/x509_vfy.h>
#endif

namespace ply {

#if PLY_WITH_HTTP_CLIENT

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄       ▄▄▄▄  ▄▄▄  ▄▄                ▄▄
//  ██  ██   ██     ██   ██  ██     ██  ▀▀  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██   ██     ██   ██▀▀▀      ██      ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██   ██     ██   ██         ▀█▄▄█▀ ▄██▄ ██ ▀█▄▄▄  ██  ██  ▀█▄▄
//

// HTTPClient uses libcurl's multi interface because it's the only supported way to immediately cancel a
// partially completed HTTP request.
struct HTTPClient {
    CURLM* multiHandle = nullptr;
    CURL* requestHandle = nullptr;
    struct curl_slist* requestHeaders = NULL;
    HTTPClientArgs args;
    const Functor<void(StringView, bool)>* callback = nullptr;
    // When true, enables CURLOPT_VERBOSE/CURLOPT_CERTINFO on outgoing requests and dumps
    // the SSL verification result + certificate chain to stderr after each request completes.
    bool debug = false;

    HTTPClient() {
        this->multiHandle = curl_multi_init();
    }
    ~HTTPClient() {
        cancelHTTPRequest(this);
        curl_multi_cleanup(this->multiHandle);
    }
};

Owned<HTTPClient> createHTTPClient() {
    return Owned<HTTPClient>::adopt(Heap::create<HTTPClient>());
}

void destroy(HTTPClient* httpClient) {
    Heap::destroy(httpClient);
}

static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* httpClient = static_cast<const HTTPClient*>(userdata);
    StringView data = {ptr, numericCast<u32>(size * nmemb)};
    (*httpClient->callback)(data, false);
    return data.numBytes();
}

// Dumps the SSL verification result and the peer certificate chain for a completed
// request.
static void dumpCurlDebugInfo(CURL* requestHandle) {
    long verifyResult = 0;
    curl_easy_getinfo(requestHandle, CURLINFO_SSL_VERIFYRESULT, &verifyResult);
    getStdErr().format("SSL verify result: {} ({})\n", X509_verify_cert_error_string(verifyResult),
                       numericCast<s64>(verifyResult));

    struct curl_certinfo* ci = NULL;
    if (curl_easy_getinfo(requestHandle, CURLINFO_CERTINFO, &ci) == CURLE_OK && ci) {
        for (int i = 0; i < ci->num_of_certs; i++) {
            getStdErr().format("Cert {}:\n", i);
            struct curl_slist* slist = ci->certinfo[i];
            for (; slist; slist = slist->next) {
                getStdErr().format("  {}\n", slist->data);
            }
        }
    }
}

void sendHTTPRequest(HTTPClient* httpClient, HTTPClientArgs&& args) {
    PLY_ASSERT(!httpClient->requestHandle);
    httpClient->args = std::move(args);

    // Create new requestHandle, configure it and add it to the multiHandle.
    httpClient->requestHandle = curl_easy_init();
    for (const auto& item : httpClient->args.headers.items()) {
        String header = String::format("{}: {}", item.key, item.value);
        httpClient->requestHeaders = curl_slist_append(httpClient->requestHeaders, (header + '\0').bytes());
    }
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_URL, (httpClient->args.url + '\0').bytes());
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_HTTPHEADER, httpClient->requestHeaders);
    // NOTE: body must remain valid for the lifetime of requestHandle because
    // CURLOPT_POSTFIELDS points into its internal buffer.
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_POSTFIELDS, httpClient->args.body.bytes());
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_POSTFIELDSIZE, (long) httpClient->args.body.numBytes());
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_CONNECTTIMEOUT, 15L);
    // Debug: enable verbose output and capture the peer certificate chain for dumping
    // after the request completes (see waitForHTTPResponse()).
    if (httpClient->debug) {
        curl_easy_setopt(httpClient->requestHandle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(httpClient->requestHandle, CURLOPT_CERTINFO, 1L);
    }
    // TLS verification: either verify against the bundled cacert.pem, or disable
    // verification (localhost only).
    if (httpClient->args.useBundledCaCert) {
        String cacertPath = joinPath(getCurrentExecutablePath(), "../cacert.pem");
        curl_easy_setopt(httpClient->requestHandle, CURLOPT_CAINFO, (cacertPath + '\0').bytes());
    } else {
        curl_easy_setopt(httpClient->requestHandle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(httpClient->requestHandle, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(httpClient->requestHandle, CURLOPT_WRITEDATA, httpClient);
    curl_multi_add_handle(httpClient->multiHandle, httpClient->requestHandle);
}

void cancelHTTPRequest(HTTPClient* httpClient) {
    if (httpClient->requestHandle != nullptr) {
        curl_multi_remove_handle(httpClient->multiHandle, httpClient->requestHandle);
        curl_easy_cleanup(httpClient->requestHandle);
        curl_slist_free_all(httpClient->requestHeaders);
        httpClient->requestHandle = NULL;
        httpClient->requestHeaders = NULL;
    }
}

bool isHTTPRequestInProgress(const HTTPClient* httpClient) {
    return httpClient->requestHandle != nullptr;
}

bool waitForHTTPResponse(HTTPClient* httpClient, const Functor<void(StringView, bool)>& callback, u32 timeOutMillis) {
    PLY_ASSERT(httpClient->requestHandle);
    PLY_ASSERT(!httpClient->callback);
    PLY_SET_IN_SCOPE(httpClient->callback, &callback);

    // Handle incoming response data.
    int stillRunning = 0;
    CURLMcode mc = curl_multi_perform(httpClient->multiHandle, &stillRunning);
    PLY_ASSERT(mc == CURLM_OK);

    // Handle completed requests and HTTP errors by iterating over available libcurl messages.
    int msgsInQueue = 0;
    CURLMsg* msg = curl_multi_info_read(httpClient->multiHandle, &msgsInQueue);
    while (msg) {
        PLY_ASSERT(msg->easy_handle == httpClient->requestHandle);
        if (msg->msg == CURLMSG_DONE) {
            // Request has completed.
            if (msg->data.result != CURLE_OK) {
                // Internal libcurl error.
                callback(String::format("libcurl error: {}", curl_easy_strerror(msg->data.result)), true);
            } else {
                long responseCode = 0;
                curl_easy_getinfo(httpClient->requestHandle, CURLINFO_RESPONSE_CODE, &responseCode);
                if (responseCode != 200) {
                    // HTTP error sent from the server.
                    callback(String::format("Error: HTTP response code {} from server", numericCast<s64>(responseCode)),
                             true);
                }
            }
            // Debug: dump the SSL verification result and certificate chain before
            // the easy handle is destroyed.
            if (httpClient->debug)
                dumpCurlDebugInfo(httpClient->requestHandle);
            // Destroy the request.
            cancelHTTPRequest(httpClient);
            return false;
        } else {
            // CURLMSG_DONE is currently the only message type defined by curl.
            PLY_ASSERT(0);
        }
        // Iterate to the next available message.
        msg = curl_multi_info_read(httpClient->multiHandle, &msgsInQueue);
    }

    // Wait for more response data if needed.
    if (httpClient->requestHandle && (stillRunning > 0)) {
        mc = curl_multi_poll(httpClient->multiHandle, NULL, 0, timeOutMillis, NULL);
        PLY_ASSERT(mc == CURLM_OK);
    }

    return true;
}

void wakeUpHTTPClient(HTTPClient* httpClient) {
    curl_multi_wakeup(httpClient->multiHandle);
}

#endif // PLY_WITH_HTTP_CLIENT

#if PLY_WITH_HTTP_SERVER

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄       ▄▄▄▄
//  ██  ██   ██     ██   ██  ██     ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██   ██     ██   ██▀▀▀       ▀▀▀█▄ ██▄▄██ ██  ▀▀ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀
//  ██  ██   ██     ██   ██         ▀█▄▄█▀ ▀█▄▄▄  ██       ▀█▀   ▀█▄▄▄  ██
//

enum class HTTPServerResponseType { None, Full, Streaming };

struct HTTPServerRequestImpl : HTTPServerRequest {
    // Used internally.
    Stream* responseStream = nullptr;
    HTTPServerResponseType responseType = HTTPServerResponseType::None;
    bool canReuseConnection = false;
};

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

// Map HTTP status code to standard reason phrase
StringView getResponseDescription(HTTPServerResponse::Code responseCode) {
    switch (responseCode) {
        case HTTPServerResponse::OK:
            return "OK";
        case HTTPServerResponse::PermanentRedirect:
            return "Moved Permanently";
        case HTTPServerResponse::TemporaryRedirect:
            return "Found";
        case HTTPServerResponse::BadRequest:
            return "Bad Request";
        case HTTPServerResponse::NotFound:
            return "Not Found";
        case HTTPServerResponse::InternalError:
        default:
            return "Internal Server Error";
    }
}

// Write an HTTP status line and header block
void writeResponseHeaders(Stream& out, const HTTPServerResponse& response) {
    // Emit the response head before writing any body bytes.
    StringView message = getResponseDescription(response.code);
    out.format("HTTP/1.1 {} {}\r\n", response.code, message);
    for (const auto& item : response.headers.items()) {
        // Write each response header line.
        out.format("{}: {}\r\n", item.key, item.value);
    }
    out.write("\r\n");
}

// Send a complete HTTP response whose body framing allows connection reuse
void HTTPServerRequest::sendFullResponse(HTTPServerResponse&& response, StringView body) {
    HTTPServerRequestImpl* req = static_cast<HTTPServerRequestImpl*>(this);
    PLY_ASSERT(req->responseType == HTTPServerResponseType::None);

    // Add content-length. Always set from the body so the peer can delimit the response.
    *response.headers.insert("content-length").value = String::format("{}", body.numBytes());

    // Add connection header only if the handler hasn't already set one.
    auto connectionResult = response.headers.insert("connection");
    if (!connectionResult.wasFound) {
        *connectionResult.value = req->canReuseConnection ? "keep-alive" : "close";
    } else if (connectionResult.value->lower() == "close") {
        // The handler explicitly asked to close; honour the request.
        req->canReuseConnection = false;
    }

    // Send response.
    req->responseType = HTTPServerResponseType::Full;
    writeResponseHeaders(*req->responseStream, response);
    bool sendBody = req->method.lower() != "head";
    if (sendBody) {
        req->responseStream->write(body);
    }
    req->responseStream->flush(true);
}

// Send headers for a raw streaming response and transfer the output stream to the handler
Stream HTTPServerRequest::beginStreamingResponse(HTTPServerResponse&& response) {
    HTTPServerRequestImpl* req = static_cast<HTTPServerRequestImpl*>(this);
    PLY_ASSERT(req->responseType == HTTPServerResponseType::None);

    // Add headers.
    *response.headers.insert("connection").value = "close";

    // Begin response and send back the Stream
    req->responseType = HTTPServerResponseType::Streaming;
    req->canReuseConnection = false;
    writeResponseHeaders(*req->responseStream, response);
    req->responseStream->flush(true);
    return std::move(*req->responseStream);
}

// Write a minimal HTML error page with the given status code
void HTTPServerRequest::sendGenericResponse(HTTPServerResponse::Code responseCode) {
    HTTPServerRequestImpl* req = static_cast<HTTPServerRequestImpl*>(this);
    HTTPServerResponse response{responseCode};
    *response.headers.insert("content-type").value = "text/html";
    StringView message = getResponseDescription(responseCode);
    req->sendFullResponse(std::move(response), String::format(R"(<html>
<head><title>{} {}</title></head>
<body>
<center><h1>{} {}</h1></center>
<hr>
</body>
</html>
)",
                                                              responseCode, message, responseCode, message));
}

// Send an HTML page that echoes request details for testing
void serveEchoPage(HTTPServerRequest& request) {
    HTTPServerResponse response{HTTPServerResponse::OK};
    *response.headers.insert("content-type").value = "text/html";
    MemStream out;
    out.write(R"(<html>
<head><title>Echo</title></head>
<body>
<center><h1>Echo</h1></center>
)");

    // Write client IP.
    out.format("<p>Connection from: <code>{:&}:{}</code></p>", request.clientAddr.toString(), request.clientPort);

    // Write request header.
    out.write("<p>Request header:</p>\n");
    out.write("<pre>\n");
    out.format("{:&} {:&} {:&}\n", request.method, request.uri, request.httpVersion);
    for (const auto& item : request.headers.items()) {
        out.format("{:&}: {:&}\n", item.key, item.value);
    }
    out.write("</pre>\n");
    out.write(R"(</body>
</html>
)");
    request.sendFullResponse(std::move(response), out.moveToString());
}

// Parse an HTTP request from a TCP connection and dispatch it to the handler
void handleRequest(TCPConnection* tcpConn, const Functor<void(HTTPServerRequest& request)>& reqHandler) {
    Stream in = tcpConn->createInStream();
    Stream out = tcpConn->createOutStream();

    // Reuse the connection as much as possible.
    for (;;) {
        // Create request object.
        HTTPServerRequestImpl request;
        request.clientAddr = tcpConn->remoteAddress();
        request.clientPort = tcpConn->remotePort();
        request.responseStream = &out;

        // Read HTTP request line.
        String requestLine;
        do {
            requestLine = readLine(in);
            if (requestLine.isEmpty() && in.atEof) {
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
        } while (requestLine.trim().isEmpty());
        Array<StringView> tokens = requestLine.trim().split(" ");
        if (tokens.numItems() != 3) {
            request.sendGenericResponse(HTTPServerResponse::BadRequest);
            return;
        }
        request.method = tokens[0];
        request.uri = tokens[1];
        request.httpVersion = tokens[2];

        // Read HTTP headers.
        for (;;) {
            String line = readLine(in);
            if (line.isEmpty() && in.atEof) {
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
            if (line.trim().isEmpty())
                break; // Blank line.
            if (isWhite(line[0])) {
                // FIXME: Support unfolding https://tools.ietf.org/html/rfc822#section-3.1
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
            s32 colonPos = line.find(':');
            if (colonPos < 0) {
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
            *request.headers.insert(line.left(colonPos).trim().lower()).value = line.substr(colonPos + 1).trim();
        }

        // Determine whether the connection can be reused by default.
        const String* connectionPtr = request.headers.find("connection");
        String connection = connectionPtr ? connectionPtr->lower() : "";
        if (connection == "close") {
            request.canReuseConnection = false;
        } else if (request.httpVersion == "HTTP/1.1") {
            request.canReuseConnection = true;
        } else {
            request.canReuseConnection = (connection == "keep-alive");
        }

        // There are three ways to read the request body (if any):
        // 1. Chunked transfer encoding - allows connection reuse.
        // 2. Explicit content length - allows connection reuse.
        // 3. Read until EOF - doesn't allow connection reuse.
        String* transferEncodingPtr = request.headers.find("transfer-encoding");
        String* contentLengthPtr = request.headers.find("content-length");
        if (transferEncodingPtr && transferEncodingPtr->lower() == "chunked") {
            // Chunked transfer encoding.
            if (!readChunkedBody(in, &request.body)) {
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
        } else if (contentLengthPtr) {
            // Explicit content length.
            u64 contentLength = 0;
            if (!contentLengthPtr->match("%d", &contentLength) || (contentLength > getMaxValue<u32>())) {
                request.sendGenericResponse(HTTPServerResponse::BadRequest);
                return;
            }
            if (contentLength > 0) {
                request.body = String::allocate(numericCast<u32>(contentLength));
                if (in.read(request.body.mutStringView()) != contentLength) {
                    request.sendGenericResponse(HTTPServerResponse::BadRequest);
                    return;
                }
            }
        } else {
            String lowerMethod = request.method.lower();
            if (lowerMethod == "post" || lowerMethod == "put" || lowerMethod == "patch") {
                // Read request body until EOF.
                MemStream mem;
                while (in.makeReadable() && mem.makeWritable()) {
                    u32 toCopy = min(in.numRemainingBytes(), mem.numRemainingBytes());
                    memcpy(mem.curByte, in.curByte, toCopy);
                    in.curByte += toCopy;
                    mem.curByte += toCopy;
                }
                request.body = mem.moveToString();
                request.canReuseConnection = false;
            }
        }

        // Invoke request handler and require it to send exactly one response.
        reqHandler(request);
        if (request.responseType == HTTPServerResponseType::None) {
            // No response was sent.
            request.sendGenericResponse(HTTPServerResponse::InternalError);
            return;
        }
        if (!request.canReuseConnection)
            return;
    }
}

// Accept connections on a port and handle each request in a new thread
void runHTTPServer(u16 port, const Functor<void(HTTPServerRequest& request)>& reqHandler) {
    TCPListener listener = Network::bindTcp(port);
    if (!listener.isValid()) {
        getStdErr().format("Error: Can't bind to port {}\n", port);
        return;
    }

    // Accepting incoming TCP connections in a loop.
    for (;;) {
        Owned<TCPConnection> tcpConn = listener.accept();
        if (!tcpConn)
            break;

        // Transfer ownership through the thread callback using a raw pointer.
        TCPConnection* tcpConnPtr = tcpConn.release();
        spawnThread([tcpConnPtr, &reqHandler] {
            Owned<TCPConnection> tcpConn = Owned<TCPConnection>::adopt(tcpConnPtr);
            handleRequest(tcpConn.get(), reqHandler);
        });
    }
}

#endif // PLY_WITH_HTTP_SERVER

} // namespace ply
