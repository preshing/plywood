/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-network.h"

#if defined(PLY_POSIX)
#include <arpa/inet.h>
#include <signal.h>
#define PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS 0
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

} // namespace ply
