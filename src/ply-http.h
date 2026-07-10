/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once

#include "ply-base.h"
#include "ply-network.h"

// Set default PLY_WITH_HTTP_CLIENT and PLY_WITH_HTTP_SERVER values.
#if !defined(PLY_WITH_HTTP_CLIENT)
#define PLY_WITH_HTTP_CLIENT 0
#endif
#if !defined(PLY_WITH_HTTP_SERVER)
#define PLY_WITH_HTTP_SERVER 1
#endif

namespace ply {

#if PLY_WITH_HTTP_CLIENT

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄       ▄▄▄▄  ▄▄▄  ▄▄                ▄▄
//  ██  ██   ██     ██   ██  ██     ██  ▀▀  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██   ██     ██   ██▀▀▀      ██      ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██   ██     ██   ██         ▀█▄▄█▀ ▄██▄ ██ ▀█▄▄▄  ██  ██  ▀█▄▄
//

//-----------------------------------------------------------
// HTTPClient is a libcurl wrapper that can be interrupted from other threads.
// It encapsulates a single HTTP request.
// The same HTTPClient object can be reused across multiple requests.
// Except for wakeUpHTTPClient, the HTTPClient API is not thread-safe.
// Most of it is designed to be used from a single thread.
//-----------------------------------------------------------
struct HTTPClientArgs {
    String url;
    Map<String, String> headers; // HTTP headers, e.g. {"Content-Type" => "application/json"}.
    String body;
    // When true, verify the peer against the cacert.pem bundle shipped next to the
    // executable (CURLOPT_CAINFO). When false, TLS verification is disabled, which is
    // only appropriate for trusted localhost endpoints.
    bool useBundledCaCert = false;
};

struct HTTPClient;

Owned<HTTPClient> createHTTPClient();
void destroy(HTTPClient* httpClient);
// Send a new request.
void sendHTTPRequest(HTTPClient* httpClient, HTTPClientArgs&& args);
// Cancel any request in progress. isHTTPRequestInProgress will return false after this.
void cancelHTTPRequest(HTTPClient* httpClient);
// Returns true as long as a request is still in progress.
bool isHTTPRequestInProgress(const HTTPClient* httpClient);
// If a request is in progress and incoming data is available, waitForHTTPResponse invokes `callback` and returns
// immediately. If no incoming data is available, waitForHTTPResponse will wait up to the maximum timeout (can be interrupted
// by wakeUpHTTPClient). If no request is in progress, waitForHTTPResponse is a no-op. `callback` is invoked with
// (chunk, false) for each chunk of response data, and one final time with (errorMessage, true) if the request fails.
bool waitForHTTPResponse(HTTPClient* httpClient, const Functor<void(StringView, bool)>& callback,
                      u32 timeOutMillis = 1000);
// wakeUpHTTPClient can be called from any thread as long as the HTTPClient exists. It there is a pending call to
// waitForHTTPResponse in another thread, it returns immediately; otherwise, the next waitForHTTPResponse call will return
// immediately.
void wakeUpHTTPClient(HTTPClient* httpClient);

#endif // PLY_WITH_HTTP_CLIENT

#if PLY_WITH_HTTP_SERVER

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄       ▄▄▄▄
//  ██  ██   ██     ██   ██  ██     ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██   ██     ██   ██▀▀▀       ▀▀▀█▄ ██▄▄██ ██  ▀▀ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀
//  ██  ██   ██     ██   ██         ▀█▄▄█▀ ▀█▄▄▄  ██       ▀█▀   ▀█▄▄▄  ██
//

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

#endif // PLY_WITH_HTTP_SERVER

} // namespace ply
