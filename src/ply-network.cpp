/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    Networking                                 │
│               Documentation: /docs/networking.md         │
│                                                          │
└─────────────────────────────────────────────────────────*/

#include "ply-network.h"

#if defined(PLY_POSIX)
#include <arpa/inet.h>
#include <signal.h>
#define PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS 0
#endif

#if PLY_WITH_HTTP_CLIENT
// HTTPClient requires curl.
#include <curl/curl.h>
#include <openssl/x509_vfy.h>
#endif

namespace ply {

//  ▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄      ▄▄     ▄▄
//   ██  ██  ██ ██  ██  ▄▄▄██  ▄▄▄██ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ██  ██▀▀▀  ██▀▀██ ██  ██ ██  ██ ██  ▀▀ ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▄██▄ ██     ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//

String IPAddress::toString() const {
    char buf[INET6_ADDRSTRLEN] = {0};
    if (this->version() == IPV4) {
        // FIXME: Rewrite without using CRT
        const char* r = inet_ntop(AF_INET, &this->netOrdered[3], buf, INET6_ADDRSTRLEN);
        PLY_ASSERT(r == buf);
        PLY_UNUSED(r);
    } else {
        const char* r = inet_ntop(AF_INET6, this, buf, INET6_ADDRSTRLEN);
        PLY_ASSERT(r == buf);
        PLY_UNUSED(r);
    }
    return buf;
}

//  ▄▄  ▄▄         ▄▄                          ▄▄
//  ███ ██  ▄▄▄▄  ▄██▄▄ ▄▄    ▄▄  ▄▄▄▄  ▄▄▄▄▄  ██  ▄▄
//  ██▀███ ██▄▄██  ██   ██ ██ ██ ██  ██ ██  ▀▀ ██▄█▀
//  ██  ██ ▀█▄▄▄   ▀█▄▄  ██▀▀██  ▀█▄▄█▀ ██     ██ ▀█▄
//

#define PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS 0
#define PLY_WITH_IPV6 1

#if PLY_WITH_IPV6
PLY_STATIC_ASSERT(sizeof(struct sockaddr_in) <= sizeof(struct sockaddr_in6));
#define PLY_IF_IPV6(v6expr, v4expr) v6expr
#else
#define PLY_IF_IPV4(v6expr, v4expr) v4expr
#endif

bool Network::IsInit = false;
bool Network::HasIPv6 = false;
ThreadLocal<IPResult> Network::lastResult_;

#if defined(PLY_WINDOWS)

PipeWinsock::~PipeWinsock() {
    if (this->socket != INVALID_SOCKET) {
        ::closesocket(this->socket);
        this->socket = INVALID_SOCKET;
    }
}

u32 PipeWinsock::read(MutStringView buf) {
    int rc = recv(this->socket, (char*) buf.bytes, int(buf.numBytes), 0);
    if (rc == 0 || rc == SOCKET_ERROR)
        return 0;
    PLY_ASSERT(rc > 0);
    return rc;
}

bool PipeWinsock::write(StringView buf) {
    while (buf.numBytes() > 0) {
        int rc = send(this->socket, (const char*) buf.bytes(), (DWORD) buf.numBytes(), 0);
        if (rc == SOCKET_ERROR) // FIXME: Test to make sure that disconnected sockets return
                                // SOCKET_ERROR and not 0
            return false;
        PLY_ASSERT(rc >= 0 && u32(rc) <= buf.numBytes());
        buf = buf.substr(rc);
    }
    return true;
}

void PipeWinsock::flush(bool) {
}

void Network::initialize(IPVersion ipVersion) {
    PLY_ASSERT(!IsInit);
    // Initialize Winsock
    WSADATA wsaData;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    PLY_ASSERT(LOBYTE(wsaData.wVersion) == 2 && HIBYTE(wsaData.wVersion) == 2);
    IsInit = true;
}

void Network::shutdown() {
    PLY_ASSERT(IsInit);
    int rc = WSACleanup();
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    IsInit = false;
}

TCPConnection::~TCPConnection() {
    // Prevent double-deletion of socket handle
    this->outPipe->socket = INVALID_SOCKET;
}

Owned<TCPConnection> TCPListener::accept() {
    if (this->listenSocket == INVALID_SOCKET) {
        Network::lastResult_.store(IPResult::NO_SOCKET);
        return nullptr;
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remoteAddr;
    socklen_t remoteAddrLen = sizeof(sockaddr_in);
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        remoteAddrLen = sizeof(sockaddr_in6);
    }
    socklen_t passedAddrLen = remoteAddrLen;
    SOCKET hostSocket = ::accept(this->listenSocket, (struct sockaddr*) &remoteAddr, &remoteAddrLen);

    if (hostSocket == INVALID_SOCKET) {
        // FIXME: Check WSAGetLastError
        PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
        Network::lastResult_.store(IPResult::UNKNOWN);
        return nullptr;
    }

    PLY_ASSERT(passedAddrLen >= remoteAddrLen);
    TCPConnection* tcpConn = Heap::create<TCPConnection>();
#if PLY_WITH_IPV6
    if (Network::HasIPv6 && remoteAddrLen == sizeof(sockaddr_in6)) {
        PLY_ASSERT(remoteAddr.sin6_family == AF_INET6);
        memcpy(&tcpConn->remoteAddr_, &remoteAddr.sin6_addr, 16);
    } else
#endif
    {
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remoteAddr;
        PLY_ASSERT(remoteAddrV4->sin_family == AF_INET);
        tcpConn->remoteAddr_ = IPAddress::from_ipv4(remoteAddrV4->sin_addr.s_addr);
    }
    tcpConn->remotePort_ = convertBigEndian(remoteAddr.sin6_port);
    tcpConn->inPipe = Heap::create<PipeWinsock>(hostSocket, Pipe::HAS_READ_PERMISSION);
    tcpConn->outPipe = Heap::create<PipeWinsock>(hostSocket, Pipe::HAS_WRITE_PERMISSION);
    Network::lastResult_.store(IPResult::OK);
    return tcpConn;
}

SOCKET createSocket(int type) {
    int family = AF_INET;
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        family = AF_INET6;
    }
    SOCKET s = socket(family, type, 0);
    if (s == INVALID_SOCKET) {
        int err = WSAGetLastError();
        switch (err) {
            case 0: // Dummy case to prevent compiler warnings
            default: {
                PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this code
                Network::lastResult_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }
    return s;
}

TCPListener Network::bindTcp(u16 port) {
    SOCKET listenSocket = createSocket(SOCK_STREAM);
    if (listenSocket == INVALID_SOCKET) { // lastResult_ is already set
        return {};
    }

    BOOL reuseAddr = TRUE;
    int rc = setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuseAddr, sizeof(reuseAddr));
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) serverAddr;
    socklen_t serverAddrLen = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        serverAddrLen = sizeof(sockaddr_in6);
        memset(&serverAddr, 0, serverAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin6_len = serverAddrLen;
#endif
        serverAddr.sin6_family = AF_INET6;
        serverAddr.sin6_addr = IN6ADDR_ANY_INIT;
        serverAddr.sin6_port = convertBigEndian(port);
    } else
#endif
    {
        struct sockaddr_in* serverAddrV4 = (struct sockaddr_in*) &serverAddr;
        memset(serverAddrV4, 0, serverAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin_len = serverAddrLen;
#endif
        serverAddrV4->sin_family = AF_INET;
        serverAddrV4->sin_addr.s_addr = INADDR_ANY;
        serverAddrV4->sin_port = convertBigEndian(port);
    }

    rc = bind(listenSocket, (struct sockaddr*) &serverAddr, serverAddrLen);
    if (rc == 0) {
        rc = listen(listenSocket, 1);
        if (rc == 0) {
            Network::lastResult_.store(IPResult::OK);
            return TCPListener{listenSocket};
        } else {
            int err = WSAGetLastError();
            switch (err) {
                case 0: // Dummy case to prevent compiler warnings
                default: {
                    // FIXME: Recognize this error code
                    PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
                    Network::lastResult_.store(IPResult::UNKNOWN);
                    break;
                }
            }
        }
    } else {
        int err = WSAGetLastError();
        switch (err) {
            case 0: // Dummy case to prevent compiler warnings
            default: {
                // FIXME: Recognize this error code
                PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
                Network::lastResult_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }

    // Failed
    rc = ::closesocket(listenSocket);
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return {};
}

Owned<TCPConnection> Network::connectTcp(const IPAddress& address, u16 port) {
    SOCKET connectSocket = createSocket(SOCK_STREAM);
    if (connectSocket == INVALID_SOCKET) { // lastResult_ is already set
        return {};
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remoteAddr;
    socklen_t remoteAddrLen = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        remoteAddrLen = sizeof(sockaddr_in6);
        memset(&remoteAddr, 0, remoteAddrLen);
#if PLY_KERNEL_FREEBSD
        remoteAddr.sin6_len = remoteAddrLen;
#endif
        remoteAddr.sin6_family = AF_INET6;
        memcpy(&remoteAddr.sin6_addr, &address, 16);
        remoteAddr.sin6_port = convertBigEndian(port);
    } else
#endif
    {
        PLY_ASSERT(address.version() == IPV4);
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remoteAddr;
        memset(remoteAddrV4, 0, remoteAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin_len = serverAddrLen;
#endif
        remoteAddrV4->sin_family = AF_INET;
        remoteAddrV4->sin_addr.s_addr = address.netOrdered[3];
        remoteAddrV4->sin_port = convertBigEndian(port);
    }

    int rc = ::connect(connectSocket, (sockaddr*) &remoteAddr, remoteAddrLen);
    if (rc == 0) {
        TCPConnection* tcpConn = Heap::create<TCPConnection>();
        tcpConn->remoteAddr_ = address;
        tcpConn->remotePort_ = port;
        tcpConn->inPipe = Heap::create<PipeWinsock>(connectSocket, Pipe::HAS_READ_PERMISSION);
        tcpConn->outPipe = Heap::create<PipeWinsock>(connectSocket, Pipe::HAS_WRITE_PERMISSION);
        Network::lastResult_.store(IPResult::OK);
        return tcpConn;
    }

    int err = WSAGetLastError();
    switch (err) {
        case WSAECONNREFUSED: {
            Network::lastResult_.store(IPResult::REFUSED);
            break;
        }
        default: {
            PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this error ode
            Network::lastResult_.store(IPResult::UNKNOWN);
            break;
        }
    }
    rc = ::closesocket(connectSocket);
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return nullptr;
}

IPAddress Network::resolveHostName(StringView hostName, IPVersion ipVersion) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
#if PLY_WITH_IPV6
    if (ipVersion == IPV6) {
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG; // Fallback to V4 if no V6
    }
#endif
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo((hostName + '\0').bytes(), nullptr, &hints, &res);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    struct addrinfo* best = nullptr;
    for (struct addrinfo* cur = res; cur; cur = cur->ai_next) {
#if PLY_WITH_IPV6
        if (cur->ai_family == AF_INET6 && ipVersion == IPV6) {
            if (!best || best->ai_family != AF_INET6) {
                best = cur;
            }
        }
#endif
        if (cur->ai_family == AF_INET) {
            if (!best) {
                best = cur;
            }
        }
    }

    IPAddress ipAddr;
    if (best) {
#if PLY_WITH_IPV6
        if (best->ai_family == AF_INET6) {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in6));
            struct sockaddr_in6* resolvedAddr = (struct sockaddr_in6*) best->ai_addr;
            memcpy(&ipAddr, &resolvedAddr->sin6_addr, 16);
        } else
#endif
        {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in));
            struct sockaddr_in* resolvedAddr = (struct sockaddr_in*) best->ai_addr;
            ipAddr = IPAddress::from_ipv4(resolvedAddr->sin_addr.s_addr);
        }
    }
    freeaddrinfo(res);
    return ipAddr;
}

#elif defined(PLY_POSIX)

void Network::initialize(IPVersion ipVersion) {
    // FIXME: Move this to some kind of generic Plywood initialization function, since this disables
    // SIGPIPE for all file descriptors, not just sockets, and we probably always want that. In
    // particular, we want that when communicating with a subprocess:
    signal(SIGPIPE, SIG_IGN);

    IsInit = true;

#if PLY_WITH_IPV6
    if (ipVersion == IPV6) {
        // FIXME: Is there a better way to test for IPv6 support?
        int testSocket = socket(AF_INET6, SOCK_STREAM, 0);
        if (testSocket >= 0) {
            Network::HasIPv6 = true;
            int rc = ::close(testSocket);
            PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
            PLY_UNUSED(rc);
        }
    }
#endif
}

void Network::shutdown() {
    PLY_ASSERT(IsInit);
    IsInit = false;
}

TCPConnection::~TCPConnection() {
    // Prevent double-deletion of file descriptor
    this->outPipe->fd = -1;
}

Owned<TCPConnection> TCPListener::accept() {
    if (this->listenSocket < 0) {
        Network::lastResult_.store(IPResult::NO_SOCKET);
        return nullptr;
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remoteAddr;
    socklen_t remoteAddrLen = sizeof(sockaddr_in);
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        remoteAddrLen = sizeof(sockaddr_in6);
    }
    socklen_t passedAddrLen = remoteAddrLen;
    int hostSocket = ::accept(this->listenSocket, (struct sockaddr*) &remoteAddr, &remoteAddrLen);

    // `accept` returns -1 on failure; descriptor 0 is a valid accepted socket.
    if (hostSocket < 0) {
        // FIXME: Check errno
        PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
        Network::lastResult_.store(IPResult::UNKNOWN);
        return nullptr;
    }

    PLY_ASSERT(passedAddrLen >= remoteAddrLen);
    PLY_UNUSED(passedAddrLen);
    TCPConnection* tcpConn = Heap::create<TCPConnection>();
#if PLY_WITH_IPV6
    if (Network::HasIPv6 && remoteAddrLen == sizeof(sockaddr_in6)) {
        PLY_ASSERT(remoteAddr.sin6_family == AF_INET6);
        memcpy(&tcpConn->remoteAddr_, &remoteAddr.sin6_addr, 16);
    } else
#endif
    {
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remoteAddr;
        PLY_ASSERT(remoteAddrV4->sin_family == AF_INET);
        tcpConn->remoteAddr_ = IPAddress::from_ipv4(remoteAddrV4->sin_addr.s_addr);
    }
    tcpConn->remotePort_ = convertBigEndian(remoteAddr.sin6_port);
    tcpConn->inPipe = Heap::create<Pipe_FD>(hostSocket, Pipe::HAS_READ_PERMISSION);
    tcpConn->outPipe = Heap::create<Pipe_FD>(hostSocket, Pipe::HAS_WRITE_PERMISSION);
    Network::lastResult_.store(IPResult::OK);
    return tcpConn;
}

int createSocket(int type) {
    int family = AF_INET;
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        family = AF_INET6;
    }
    int s = socket(family, type, 0);
    if (s < 0) {
        switch (errno) {
            case ENOBUFS:
            case ENOMEM:
            case ENFILE:
            case EMFILE: {
                Network::lastResult_.store(IPResult::NO_SOCKET);
                break;
            }
            case EAFNOSUPPORT:
            case EINVAL:
            case EPROTONOSUPPORT:
                // Maybe fall back to IPv4 if this happens for IPv6?
            default: {
                PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this code
                Network::lastResult_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }
    return s;
}

TCPListener Network::bindTcp(u16 port) {
    int listenSocket = createSocket(SOCK_STREAM);
    if (listenSocket < 0) { // lastResult_ is already set
        return {};
    }

    int reuseAddr = 1;
    int rc = setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) serverAddr;
    socklen_t serverAddrLen = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        serverAddrLen = sizeof(sockaddr_in6);
        memset(&serverAddr, 0, serverAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin6_len = serverAddrLen;
#endif
        serverAddr.sin6_family = AF_INET6;
        serverAddr.sin6_addr = IN6ADDR_ANY_INIT;
        serverAddr.sin6_port = convertBigEndian(port);
    } else
#endif
    {
        struct sockaddr_in* serverAddrV4 = (struct sockaddr_in*) &serverAddr;
        memset(serverAddrV4, 0, serverAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin_len = serverAddrLen;
#endif
        serverAddrV4->sin_family = AF_INET;
        serverAddrV4->sin_addr.s_addr = INADDR_ANY;
        serverAddrV4->sin_port = convertBigEndian(port);
    }

    rc = bind(listenSocket, (struct sockaddr*) &serverAddr, serverAddrLen);
    if (rc == 0) {
        rc = listen(listenSocket, 1);
        if (rc == 0) {
            Network::lastResult_.store(IPResult::OK);
            return TCPListener{listenSocket};
        } else {
            switch (errno) {
                case EADDRINUSE: {
                    Network::lastResult_.store(IPResult::IN_USE);
                    break;
                }
                default: {
                    // FIXME: Recognize this errno
                    PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
                    Network::lastResult_.store(IPResult::UNKNOWN);
                    break;
                }
            }
        }
    } else {
        switch (errno) {
            case EADDRINUSE: {
                Network::lastResult_.store(IPResult::IN_USE);
                break;
            }
            default: {
                // FIXME: Recognize this errno
                PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
                Network::lastResult_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }

    // Failed
    rc = ::close(listenSocket);
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return {};
}

Owned<TCPConnection> Network::connectTcp(const IPAddress& address, u16 port) {
    int connectSocket = createSocket(SOCK_STREAM);
    if (connectSocket < 0) { // lastResult_ is already set
        return {};
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remoteAddr;
    socklen_t remoteAddrLen = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        remoteAddrLen = sizeof(sockaddr_in6);
        memset(&remoteAddr, 0, remoteAddrLen);
#if PLY_KERNEL_FREEBSD
        remoteAddr.sin6_len = remoteAddrLen;
#endif
        remoteAddr.sin6_family = AF_INET6;
        memcpy(&remoteAddr.sin6_addr, &address, 16);
        remoteAddr.sin6_port = convertBigEndian(port);
    } else
#endif
    {
        PLY_ASSERT(address.version() == IPV4);
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remoteAddr;
        memset(remoteAddrV4, 0, remoteAddrLen);
#if PLY_KERNEL_FREEBSD
        serverAddr.sin_len = serverAddrLen;
#endif
        remoteAddrV4->sin_family = AF_INET;
        remoteAddrV4->sin_addr.s_addr = address.netOrdered[3];
        remoteAddrV4->sin_port = convertBigEndian(port);
    }

    int rc = ::connect(connectSocket, (sockaddr*) &remoteAddr, remoteAddrLen);
    if (rc == 0) {
        TCPConnection* tcpConn = Heap::create<TCPConnection>();
        tcpConn->remoteAddr_ = address;
        tcpConn->remotePort_ = port;
        tcpConn->inPipe = Heap::create<Pipe_FD>(connectSocket, Pipe::HAS_READ_PERMISSION);
        tcpConn->outPipe = Heap::create<Pipe_FD>(connectSocket, Pipe::HAS_WRITE_PERMISSION);
        Network::lastResult_.store(IPResult::OK);
        return tcpConn;
    }

    switch (errno) {
        case ECONNREFUSED: {
            Network::lastResult_.store(IPResult::REFUSED);
            break;
        }
        case ENETUNREACH: {
            Network::lastResult_.store(IPResult::UNREACHABLE);
            break;
        }
        default: {
            PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this code
            Network::lastResult_.store(IPResult::UNKNOWN);
            break;
        }
    }
    rc = ::close(connectSocket);
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return nullptr;
}

IPAddress Network::resolveHostName(StringView hostName, IPVersion ipVersion) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
#if PLY_WITH_IPV6
    if (ipVersion == IPV6) {
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG; // Fallback to V4 if no V6
    }
#endif
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo((hostName + '\0').bytes(), nullptr, &hints, &res);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    struct addrinfo* best = nullptr;
    for (struct addrinfo* cur = res; cur; cur = cur->ai_next) {
#if PLY_WITH_IPV6
        if (cur->ai_family == AF_INET6 && ipVersion == IPV6) {
            if (!best || best->ai_family != AF_INET6) {
                best = cur;
            }
        }
#endif
        if (cur->ai_family == AF_INET) {
            if (!best) {
                best = cur;
            }
        }
    }

    IPAddress ipAddr;
    if (best) {
#if PLY_WITH_IPV6
        if (best->ai_family == AF_INET6) {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in6));
            struct sockaddr_in6* resolvedAddr = (struct sockaddr_in6*) best->ai_addr;
            memcpy(&ipAddr, &resolvedAddr->sin6_addr, 16);
        } else
#endif
        {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in));
            struct sockaddr_in* resolvedAddr = (struct sockaddr_in*) best->ai_addr;
            ipAddr = IPAddress::from_ipv4(resolvedAddr->sin_addr.s_addr);
        }
    }
    freeaddrinfo(res);
    return ipAddr;
}

#endif // PLY_POSIX

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
