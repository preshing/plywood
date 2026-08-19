/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    Networking                                 │
│               Documentation: /docs/networking.md         │
│                                                          │
└─────────────────────────────────────────────────────────*/

#pragma once

#include "ply-system.h"

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

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄  ▄▄▄  ▄▄                ▄▄
//  ██  ██   ██     ██   ██  ██ ██  ▀▀  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀██   ██     ██   ██▀▀▀  ██      ██  ██ ██▄▄██ ██  ██  ██
//  ██  ██   ██     ██   ██     ▀█▄▄█▀ ▄██▄ ██ ▀█▄▄▄  ██  ██  ▀█▄▄
//

//-----------------------------------------------------------
// HTTPClient encapsulates a single HTTP request.
// The same HTTPClient object can be reused across multiple requests.
// The API is not thread-safe and is intended to be driven from a single thread except for wakeUp.
// All functions are designed to return as quickly as possible.
//-----------------------------------------------------------
struct HTTPClient {
    struct Args {
        String url;
        Map<String, String> headers; // HTTP headers, e.g. {"Content-Type" => "application/json"}.
        String body;
        Functor<void(StringView, bool)> callback; // Receives (chunk, false), or (errorMessage, true) on failure.
        // When true, verify the peer against the cacert.pem bundle shipped next to the
        // executable (CURLOPT_CAINFO). When false, TLS verification is disabled, which is
        // only appropriate for trusted localhost endpoints.
        bool useBundledCaCert = true;
    };

    static Owned<HTTPClient> create();
    void destroy();

    // Begin a new request. Must not be called while a request is already in progress.
    void beginRequest(Args&& args);
    // Cancel any request in progress. The HTTPClient can be reused after this returns.
    void cancelRequest();
    // Returns true as long as a request is still in progress.
    bool isRequestInProgress() const;
    // Drives the request, delivering incoming response data to the request's callback. If response data is available,
    // it invokes the callback and returns immediately. If no data is available, it waits up to timeOutMillis for data
    // to arrive, but can be interrupted by other threads calling wakeUp. Returns false if no request is in progress;
    // true otherwise.
    bool receiveResponse(u32 timeOutMillis = 1000);
    // Can be called from any thread. If receiveResponse is currently blocked in another thread, it returns immediately;
    // otherwise, the next receiveResponse call returns immediately.
    void wakeUp();

private:
    // Only the hidden subclass HTTPClientImpl can construct an HTTPClient.
    friend struct HTTPClientImpl;
    HTTPClient() = default;
    HTTPClient(const HTTPClient&) = delete;
    HTTPClient& operator=(const HTTPClient&) = delete;
    HTTPClient(HTTPClient&&) = delete;
    HTTPClient& operator=(HTTPClient&&) = delete;
};

#endif // PLY_WITH_HTTP_CLIENT

#if PLY_WITH_HTTP_SERVER

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██  ██   ██     ██   ██  ██ ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██   ██     ██   ██▀▀▀   ▀▀▀█▄ ██▄▄██ ██  ▀▀ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀
//  ██  ██   ██     ██   ██     ▀█▄▄█▀ ▀█▄▄▄  ██       ▀█▀   ▀█▄▄▄  ██
//

struct HTTPServer {
    // Argument type used when the callback invokes a server response function.
    struct Response {
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
        explicit Response(Code code) : code{code} {
        }
    };

    // Request has additional hidden members in the HTTPRequestImpl subclass.
    struct Request {
        IPAddress clientAddr;
        u16 clientPort = 0;
        String method;
        String uri;
        String httpVersion;
        Map<String, String> headers;
        String body;

        // Send a complete response using the provided headers and body.
        // The underlying TCP connection may be reused for other requests/responses.
        void sendFullResponse(Response&& response, StringView body = {});
        // Send response headers and returns a Stream to write the remaining body.
        // The connection will be closed when the Stream is destroyed.
        Stream beginStreamingResponse(Response&& response);
        // Send a basic HTML error page with the given HTTP status code.
        void sendGenericResponse(Response::Code responseCode);
    };

    // Bind to the given port and run an HTTP server that dispatches incoming requests to the given callback function.
    // This function never returns.
    static void run(u16 port, const Functor<void(Request& request)>& requestHandler);

    // Sample request handler for testing purposes.
    static void echoPage(Request& request);
};

#endif // PLY_WITH_HTTP_SERVER

} // namespace ply
