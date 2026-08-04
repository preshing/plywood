/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once

#include "ply-base.h"

#if defined(PLY_WINDOWS) // Windows
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#if !defined(PLY_WITH_HTTP_CLIENT)
#define PLY_WITH_HTTP_CLIENT 0
#endif

#if !defined(PLY_WITH_HTTP_SERVER)
#define PLY_WITH_HTTP_SERVER 0
#endif

namespace ply {

//  ▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄      ▄▄     ▄▄
//   ██  ██  ██ ██  ██  ▄▄▄██  ▄▄▄██ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ██  ██▀▀▀  ██▀▀██ ██  ██ ██  ██ ██  ▀▀ ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▄██▄ ██     ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//

enum IPVersion {
    IPV4,
    IPV6,
};

struct IPAddress {
    u32 netOrdered[4]; // big endian

    IPVersion version() const {
        return (this->netOrdered[0] == 0 && this->netOrdered[1] == 0 &&
                this->netOrdered[2] == convertBigEndian(0xffffu))
                   ? IPV4
                   : IPV6;
    }
    bool isNull() const {
        return this->netOrdered[0] == 0 && this->netOrdered[1] == 0 && this->netOrdered[2] == 0 &&
               this->netOrdered[3] == 0;
    }
    static constexpr IPAddress localHost(IPVersion ipVersion) {
        return (ipVersion == IPV4) ? IPAddress{{0, 0, convertBigEndian(0xffffu), convertBigEndian(0x7f000001u)}}
                                   : IPAddress{{0, 0, 0, convertBigEndian(1u)}};
    }
    static constexpr IPAddress from_ipv4(u32 netOrdered) {
        return {{0, 0, convertBigEndian(0xffffu), netOrdered}};
    }
    String toString() const;
    static IPAddress fromString();
};

//  ▄▄  ▄▄         ▄▄                          ▄▄
//  ███ ██  ▄▄▄▄  ▄██▄▄ ▄▄    ▄▄  ▄▄▄▄  ▄▄▄▄▄  ██  ▄▄
//  ██▀███ ██▄▄██  ██   ██ ██ ██ ██  ██ ██  ▀▀ ██▄█▀
//  ██  ██ ▀█▄▄▄   ▀█▄▄  ██▀▀██  ▀█▄▄█▀ ██     ██ ▀█▄
//

struct TCPListener;
struct TCPConnection;

#if defined(PLY_WINDOWS)

struct PipeWinsock : Pipe {
    SOCKET socket = INVALID_SOCKET;

    PipeWinsock() {
    }
    PipeWinsock(SOCKET s, u32 flags) : socket{s} {
        this->flags = flags;
    }
    virtual ~PipeWinsock();
    virtual u32 read(MutStringView buf) override;
    virtual bool write(StringView buf) override;
    virtual void flush(bool) override;
};

#endif

enum class IPResult : u8 {
    UNKNOWN = 0,
    OK,
    NO_SOCKET,
    UNREACHABLE,
    REFUSED,
    IN_USE,
};

class Network {
private:
#if defined(PLY_WINDOWS)
    using Handle = SOCKET;
    static constexpr Handle InvalidHandle = INVALID_SOCKET;
#elif defined(PLY_POSIX)
    using Handle = int;
    static constexpr Handle InvalidHandle = -1;
#endif

public:
    static bool IsInit;
    static bool HasIPv6;
    static ThreadLocal<IPResult> lastResult_;

    static void initialize(IPVersion ipVersion);
    static void shutdown();
    static TCPListener bindTcp(u16 port);
    static Owned<TCPConnection> connectTcp(const IPAddress& address, u16 port);
    static IPAddress resolveHostName(StringView hostName, IPVersion ipVersion);
    static IPResult lastResult() {
        return Network::lastResult_.load();
    }
};

//  ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄                                     ▄▄   ▄▄
//    ██   ██  ▀▀ ██  ██ ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██     ██▀▀▀  ██     ██  ██ ██  ██ ██  ██ ██▄▄██ ██     ██   ██ ██  ██ ██  ██
//    ██   ▀█▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄█▀ ██  ██ ██  ██ ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██
//

#if defined(PLY_WINDOWS)

struct TCPConnection {
    IPAddress remoteAddr_;
    u16 remotePort_ = 0;
    Owned<PipeWinsock> inPipe;
    Owned<PipeWinsock> outPipe;

    TCPConnection() {
    }
    ~TCPConnection();
    const IPAddress& remoteAddress() const {
        return this->remoteAddr_;
    }
    u16 remotePort() const {
        return this->remotePort_;
    }
    SOCKET getHandle() const {
        return inPipe->socket;
    }
    Stream createInStream() {
        return Stream{this->inPipe, false};
    }
    Stream createOutStream() {
        return Stream{this->outPipe, false};
    }
};

#elif defined(PLY_POSIX)

struct TCPConnection {
    IPAddress remoteAddr_;
    u16 remotePort_ = 0;
    Owned<Pipe_FD> inPipe;
    Owned<Pipe_FD> outPipe;

    TCPConnection() {
    }
    ~TCPConnection();
    const IPAddress& remoteAddress() const {
        return this->remoteAddr_;
    }
    u16 remotePort() const {
        return this->remotePort_;
    }
    int getSocket() const {
        return inPipe->fd;
    }
    Stream createInStream() {
        return Stream{this->inPipe, false};
    }
    Stream createOutStream() {
        return Stream{this->outPipe, false};
    }
};

#endif

//  ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄    ▄▄         ▄▄
//    ██   ██  ▀▀ ██  ██ ██    ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//    ██   ██     ██▀▀▀  ██    ██ ▀█▄▄▄   ██   ██▄▄██ ██  ██ ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██     ██▄▄▄ ██  ▄▄▄█▀  ▀█▄▄ ▀█▄▄▄  ██  ██ ▀█▄▄▄  ██
//

#if defined(PLY_WINDOWS)

struct TCPListener {
public:
    SOCKET listenSocket = INVALID_SOCKET;

    TCPListener(SOCKET listenSocket = INVALID_SOCKET) : listenSocket{listenSocket} {
    }
    TCPListener(TCPListener&& other) {
        this->listenSocket = other.listenSocket;
        other.listenSocket = INVALID_SOCKET;
    }
    ~TCPListener() {
        if (this->listenSocket >= 0) {
            closesocket(this->listenSocket);
        }
    }
    TCPListener& operator=(TCPListener&& other) {
        if (this->listenSocket >= 0) {
            closesocket(this->listenSocket);
        }
        this->listenSocket = other.listenSocket;
        other.listenSocket = INVALID_SOCKET;
        return *this;
    }
    bool isValid() {
        return this->listenSocket >= 0;
    }
    void endComm() {
        shutdown(this->listenSocket, SD_BOTH);
    }
    void close() {
        if (this->listenSocket >= 0) {
            closesocket(this->listenSocket);
            this->listenSocket = INVALID_SOCKET;
        }
    }

    Owned<TCPConnection> accept();
};

#elif defined(PLY_POSIX)

struct TCPListener {
public:
    int listenSocket = -1;

    TCPListener(int listenSocket = -1) : listenSocket{listenSocket} {
    }
    TCPListener(TCPListener&& other) {
        this->listenSocket = other.listenSocket;
        other.listenSocket = -1;
    }
    ~TCPListener() {
        if (this->listenSocket >= 0) {
            ::close(this->listenSocket);
        }
    }
    TCPListener& operator=(TCPListener&& other) {
        if (this->listenSocket >= 0) {
            ::close(this->listenSocket);
        }
        this->listenSocket = other.listenSocket;
        other.listenSocket = -1;
        return *this;
    }
    bool isValid() {
        return this->listenSocket >= 0;
    }
    void endComm() {
        shutdown(this->listenSocket, SHUT_RDWR);
    }
    void close() {
        if (this->listenSocket >= 0) {
            ::close(this->listenSocket);
            this->listenSocket = -1;
        }
    }

    Owned<TCPConnection> accept();
};

#endif

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
