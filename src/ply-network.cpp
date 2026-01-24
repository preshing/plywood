/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "ply-network.h"

#if PLY_MACOS
#include <arpa/inet.h>
#define PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS 0
#endif

namespace ply {

//  ▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄      ▄▄     ▄▄
//   ██  ██  ██ ██  ██  ▄▄▄██  ▄▄▄██ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ██  ██▀▀▀  ██▀▀██ ██  ██ ██  ██ ██  ▀▀ ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▄██▄ ██     ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//

String IPAddress::to_string() const {
    char buf[INET6_ADDRSTRLEN] = {0};
    if (this->version() == IPV4) {
        // FIXME: Rewrite without using CRT
        const char* r = inet_ntop(AF_INET, &this->net_ordered[3], buf, INET6_ADDRSTRLEN);
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
ThreadLocal<IPResult> Network::last_result_;

#if defined(PLY_WINDOWS)

PipeWinsock::~PipeWinsock() {
    if (this->socket != INVALID_SOCKET) {
        ::closesocket(this->socket);
        this->socket = INVALID_SOCKET;
    }
}

u32 PipeWinsock::read(MutStringView buf) {
    int rc = recv(this->socket, (char*) buf.bytes, int(buf.num_bytes), 0);
    if (rc == 0 || rc == SOCKET_ERROR)
        return 0;
    PLY_ASSERT(rc > 0);
    return rc;
}

bool PipeWinsock::write(StringView buf) {
    while (buf.num_bytes() > 0) {
        int rc = send(this->socket, (const char*) buf.bytes(), (DWORD) buf.num_bytes(), 0);
        if (rc == SOCKET_ERROR) // FIXME: Test to make sure that disconnected sockets return
                                // SOCKET_ERROR and not 0
            return false;
        PLY_ASSERT(rc >= 0 && u32(rc) <= buf.num_bytes());
        buf = buf.substr(rc);
    }
    return true;
}

void PipeWinsock::flush(bool) {
}

void Network::initialize(IPVersion ip_version) {
    PLY_ASSERT(!IsInit);
    // Initialize Winsock
    WSADATA wsa_data;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    PLY_ASSERT(LOBYTE(wsa_data.wVersion) == 2 && HIBYTE(wsa_data.wVersion) == 2);
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
    this->out_pipe->socket = INVALID_SOCKET;
}

Owned<TCPConnection> TCPListener::accept() {
    if (this->listen_socket == INVALID_SOCKET) {
        Network::last_result_.store(IPResult::NO_SOCKET);
        return nullptr;
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remote_addr;
    socklen_t remote_addr_len = sizeof(sockaddr_in);
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        remote_addr_len = sizeof(sockaddr_in6);
    }
    socklen_t passed_addr_len = remote_addr_len;
    SOCKET host_socket = ::accept(this->listen_socket, (struct sockaddr*) &remote_addr, &remote_addr_len);

    if (host_socket == INVALID_SOCKET) {
        // FIXME: Check WSAGetLastError
        PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
        Network::last_result_.store(IPResult::UNKNOWN);
        return nullptr;
    }

    PLY_ASSERT(passed_addr_len >= remote_addr_len);
    TCPConnection* tcp_conn = Heap::create<TCPConnection>();
#if PLY_WITH_IPV6
    if (Network::HasIPv6 && remote_addr_len == sizeof(sockaddr_in6)) {
        PLY_ASSERT(remote_addr.sin6_family == AF_INET6);
        memcpy(&tcp_conn->remote_addr_, &remote_addr.sin6_addr, 16);
    } else
#endif
    {
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remote_addr;
        PLY_ASSERT(remoteAddrV4->sin_family == AF_INET);
        tcp_conn->remote_addr_ = IPAddress::from_ipv4(remoteAddrV4->sin_addr.s_addr);
    }
    tcp_conn->remote_port_ = convert_big_endian(remote_addr.sin6_port);
    tcp_conn->in_pipe = Heap::create<PipeWinsock>(host_socket, Pipe::HAS_READ_PERMISSION);
    tcp_conn->out_pipe = Heap::create<PipeWinsock>(host_socket, Pipe::HAS_WRITE_PERMISSION);
    Network::last_result_.store(IPResult::OK);
    return tcp_conn;
}

SOCKET create_socket(int type) {
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
                Network::last_result_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }
    return s;
}

TCPListener Network::bind_tcp(u16 port) {
    SOCKET listen_socket = create_socket(SOCK_STREAM);
    if (listen_socket == INVALID_SOCKET) { // last_result_ is already set
        return {};
    }

    BOOL reuse_addr = TRUE;
    int rc = setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuse_addr, sizeof(reuse_addr));
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) server_addr;
    socklen_t server_addr_len = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        server_addr_len = sizeof(sockaddr_in6);
        memset(&server_addr, 0, server_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin6_len = server_addr_len;
#endif
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_addr = IN6ADDR_ANY_INIT;
        server_addr.sin6_port = convert_big_endian(port);
    } else
#endif
    {
        struct sockaddr_in* serverAddrV4 = (struct sockaddr_in*) &server_addr;
        memset(serverAddrV4, 0, server_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin_len = server_addr_len;
#endif
        serverAddrV4->sin_family = AF_INET;
        serverAddrV4->sin_addr.s_addr = INADDR_ANY;
        serverAddrV4->sin_port = convert_big_endian(port);
    }

    rc = bind(listen_socket, (struct sockaddr*) &server_addr, server_addr_len);
    if (rc == 0) {
        rc = listen(listen_socket, 1);
        if (rc == 0) {
            Network::last_result_.store(IPResult::OK);
            return TCPListener{listen_socket};
        } else {
            int err = WSAGetLastError();
            switch (err) {
                case 0: // Dummy case to prevent compiler warnings
                default: {
                    // FIXME: Recognize this error code
                    PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
                    Network::last_result_.store(IPResult::UNKNOWN);
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
                Network::last_result_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }

    // Failed
    rc = ::closesocket(listen_socket);
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return {};
}

Owned<TCPConnection> Network::connect_tcp(const IPAddress& address, u16 port) {
    SOCKET connect_socket = create_socket(SOCK_STREAM);
    if (connect_socket == INVALID_SOCKET) { // last_result_ is already set
        return {};
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remote_addr;
    socklen_t remote_addr_len = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        remote_addr_len = sizeof(sockaddr_in6);
        memset(&remote_addr, 0, remote_addr_len);
#if PLY_KERNEL_FREEBSD
        remote_addr.sin6_len = remote_addr_len;
#endif
        remote_addr.sin6_family = AF_INET6;
        memcpy(&remote_addr.sin6_addr, &address, 16);
        remote_addr.sin6_port = convert_big_endian(port);
    } else
#endif
    {
        PLY_ASSERT(address.version() == IPV4);
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remote_addr;
        memset(remoteAddrV4, 0, remote_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin_len = server_addr_len;
#endif
        remoteAddrV4->sin_family = AF_INET;
        remoteAddrV4->sin_addr.s_addr = address.net_ordered[3];
        remoteAddrV4->sin_port = convert_big_endian(port);
    }

    int rc = ::connect(connect_socket, (sockaddr*) &remote_addr, remote_addr_len);
    if (rc == 0) {
        TCPConnection* tcp_conn = Heap::create<TCPConnection>();
        tcp_conn->remote_addr_ = address;
        tcp_conn->remote_port_ = port;
        tcp_conn->in_pipe = Heap::create<PipeWinsock>(connect_socket, Pipe::HAS_READ_PERMISSION);
        tcp_conn->out_pipe = Heap::create<PipeWinsock>(connect_socket, Pipe::HAS_WRITE_PERMISSION);
        Network::last_result_.store(IPResult::OK);
        return tcp_conn;
    }

    int err = WSAGetLastError();
    switch (err) {
        case WSAECONNREFUSED: {
            Network::last_result_.store(IPResult::REFUSED);
            break;
        }
        default: {
            PLY_ASSERT(PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this error ode
            Network::last_result_.store(IPResult::UNKNOWN);
            break;
        }
    }
    rc = ::closesocket(connect_socket);
    PLY_ASSERT(rc == 0 || PLY_IPWINSOCK_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return nullptr;
}

IPAddress Network::resolve_host_name(StringView host_name, IPVersion ip_version) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
#if PLY_WITH_IPV6
    if (ip_version == IPV6) {
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG; // Fallback to V4 if no V6
    }
#endif
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo((host_name + '\0').bytes(), nullptr, &hints, &res);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    struct addrinfo* best = nullptr;
    for (struct addrinfo* cur = res; cur; cur = cur->ai_next) {
#if PLY_WITH_IPV6
        if (cur->ai_family == AF_INET6 && ip_version == IPV6) {
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

    IPAddress ip_addr;
    if (best) {
#if PLY_WITH_IPV6
        if (best->ai_family == AF_INET6) {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in6));
            struct sockaddr_in6* resolved_addr = (struct sockaddr_in6*) best->ai_addr;
            memcpy(&ip_addr, &resolved_addr->sin6_addr, 16);
        } else
#endif
        {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in));
            struct sockaddr_in* resolved_addr = (struct sockaddr_in*) best->ai_addr;
            ip_addr = IPAddress::from_ipv4(resolved_addr->sin_addr.s_addr);
        }
    }
    freeaddrinfo(res);
    return ip_addr;
}

#elif defined(PLY_POSIX)

void Network::initialize(IPVersion ip_version) {
    // FIXME: Move this to some kind of generic Plywood initialization function, since this disables
    // SIGPIPE for all file descriptors, not just sockets, and we probably always want that. In
    // particular, we want that when communicating with a subprocess:
    signal(SIGPIPE, SIG_IGN);

    IsInit = true;

#if PLY_WITH_IPV6
    if (ip_version == IPV6) {
        // FIXME: Is there a better way to test for IPv6 support?
        int test_socket = socket(AF_INET6, SOCK_STREAM, 0);
        if (test_socket >= 0) {
            Network::HasIPv6 = true;
            int rc = ::close(test_socket);
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
    this->out_pipe->fd = -1;
}

Owned<TCPConnection> TCPListener::accept() {
    if (this->listen_socket < 0) {
        Network::last_result_.store(IPResult::NO_SOCKET);
        return nullptr;
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remote_addr;
    socklen_t remote_addr_len = sizeof(sockaddr_in);
    if (PLY_IF_IPV6(Network::HasIPv6, false)) {
        remote_addr_len = sizeof(sockaddr_in6);
    }
    socklen_t passed_addr_len = remote_addr_len;
    int host_socket = ::accept(this->listen_socket, (struct sockaddr*) &remote_addr, &remote_addr_len);

    if (host_socket <= 0) {
        // FIXME: Check errno
        PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
        Network::last_result_.store(IPResult::UNKNOWN);
        return nullptr;
    }

    PLY_ASSERT(passed_addr_len >= remote_addr_len);
    PLY_UNUSED(passed_addr_len);
    TCPConnection* tcp_conn = Heap::create<TCPConnection>();
#if PLY_WITH_IPV6
    if (Network::HasIPv6 && remote_addr_len == sizeof(sockaddr_in6)) {
        PLY_ASSERT(remote_addr.sin6_family == AF_INET6);
        memcpy(&tcp_conn->remote_addr_, &remote_addr.sin6_addr, 16);
    } else
#endif
    {
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remote_addr;
        PLY_ASSERT(remoteAddrV4->sin_family == AF_INET);
        tcp_conn->remote_addr_ = IPAddress::from_ipv4(remoteAddrV4->sin_addr.s_addr);
    }
    tcp_conn->remote_port_ = convert_big_endian(remote_addr.sin6_port);
    tcp_conn->in_pipe->fd = host_socket;
    tcp_conn->out_pipe->fd = host_socket;
    Network::last_result_.store(IPResult::OK);
    return tcp_conn;
}

int create_socket(int type) {
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
                Network::last_result_.store(IPResult::NO_SOCKET);
                break;
            }
            case EAFNOSUPPORT:
            case EINVAL:
            case EPROTONOSUPPORT:
                // Maybe fall back to IPv4 if this happens for IPv6?
            default: {
                PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this code
                Network::last_result_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }
    return s;
}

TCPListener Network::bind_tcp(u16 port) {
    int listen_socket = create_socket(SOCK_STREAM);
    if (listen_socket < 0) { // last_result_ is already set
        return {};
    }

    int reuse_addr = 1;
    int rc = setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) server_addr;
    socklen_t server_addr_len = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        server_addr_len = sizeof(sockaddr_in6);
        memset(&server_addr, 0, server_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin6_len = server_addr_len;
#endif
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_addr = IN6ADDR_ANY_INIT;
        server_addr.sin6_port = convert_big_endian(port);
    } else
#endif
    {
        struct sockaddr_in* serverAddrV4 = (struct sockaddr_in*) &server_addr;
        memset(serverAddrV4, 0, server_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin_len = server_addr_len;
#endif
        serverAddrV4->sin_family = AF_INET;
        serverAddrV4->sin_addr.s_addr = INADDR_ANY;
        serverAddrV4->sin_port = convert_big_endian(port);
    }

    rc = bind(listen_socket, (struct sockaddr*) &server_addr, server_addr_len);
    if (rc == 0) {
        rc = listen(listen_socket, 1);
        if (rc == 0) {
            Network::last_result_.store(IPResult::OK);
            return TCPListener{listen_socket};
        } else {
            switch (errno) {
                case EADDRINUSE: {
                    Network::last_result_.store(IPResult::IN_USE);
                    break;
                }
                default: {
                    // FIXME: Recognize this errno
                    PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
                    Network::last_result_.store(IPResult::UNKNOWN);
                    break;
                }
            }
        }
    } else {
        switch (errno) {
            case EADDRINUSE: {
                Network::last_result_.store(IPResult::IN_USE);
                break;
            }
            default: {
                // FIXME: Recognize this errno
                PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
                Network::last_result_.store(IPResult::UNKNOWN);
                break;
            }
        }
    }

    // Failed
    rc = ::close(listen_socket);
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return {};
}

Owned<TCPConnection> Network::connect_tcp(const IPAddress& address, u16 port) {
    int connect_socket = create_socket(SOCK_STREAM);
    if (connect_socket < 0) { // last_result_ is already set
        return {};
    }

    struct PLY_IF_IPV6(sockaddr_in6, sockaddr_in) remote_addr;
    socklen_t remote_addr_len = sizeof(sockaddr_in);
#if PLY_WITH_IPV6
    if (Network::HasIPv6) {
        remote_addr_len = sizeof(sockaddr_in6);
        memset(&remote_addr, 0, remote_addr_len);
#if PLY_KERNEL_FREEBSD
        remote_addr.sin6_len = remote_addr_len;
#endif
        remote_addr.sin6_family = AF_INET6;
        memcpy(&remote_addr.sin6_addr, &address, 16);
        remote_addr.sin6_port = convert_big_endian(port);
    } else
#endif
    {
        PLY_ASSERT(address.version() == IPV4);
        struct sockaddr_in* remoteAddrV4 = (struct sockaddr_in*) &remote_addr;
        memset(remoteAddrV4, 0, remote_addr_len);
#if PLY_KERNEL_FREEBSD
        server_addr.sin_len = server_addr_len;
#endif
        remoteAddrV4->sin_family = AF_INET;
        remoteAddrV4->sin_addr.s_addr = address.net_ordered[3];
        remoteAddrV4->sin_port = convert_big_endian(port);
    }

    int rc = ::connect(connect_socket, (sockaddr*) &remote_addr, remote_addr_len);
    if (rc == 0) {
        TCPConnection* tcp_conn = Heap::create<TCPConnection>();
        tcp_conn->remote_addr_ = address;
        tcp_conn->remote_port_ = port;
        tcp_conn->in_pipe->fd = connect_socket;
        tcp_conn->out_pipe->fd = connect_socket;
        Network::last_result_.store(IPResult::OK);
        return tcp_conn;
    }

    switch (errno) {
        case ECONNREFUSED: {
            Network::last_result_.store(IPResult::REFUSED);
            break;
        }
        case ENETUNREACH: {
            Network::last_result_.store(IPResult::UNREACHABLE);
            break;
        }
        default: {
            PLY_ASSERT(PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS); // FIXME: Recognize this code
            Network::last_result_.store(IPResult::UNKNOWN);
            break;
        }
    }
    rc = ::close(connect_socket);
    PLY_ASSERT(rc == 0 || PLY_IPPOSIX_ALLOW_UNKNOWN_ERRORS);
    PLY_UNUSED(rc);
    return nullptr;
}

IPAddress Network::resolve_host_name(StringView host_name, IPVersion ip_version) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
#if PLY_WITH_IPV6
    if (ip_version == IPV6) {
        hints.ai_family = AF_INET6;
        hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG; // Fallback to V4 if no V6
    }
#endif
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo((host_name + '\0').bytes(), nullptr, &hints, &res);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    struct addrinfo* best = nullptr;
    for (struct addrinfo* cur = res; cur; cur = cur->ai_next) {
#if PLY_WITH_IPV6
        if (cur->ai_family == AF_INET6 && ip_version == IPV6) {
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

    IPAddress ip_addr;
    if (best) {
#if PLY_WITH_IPV6
        if (best->ai_family == AF_INET6) {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in6));
            struct sockaddr_in6* resolved_addr = (struct sockaddr_in6*) best->ai_addr;
            memcpy(&ip_addr, &resolved_addr->sin6_addr, 16);
        } else
#endif
        {
            PLY_ASSERT(best->ai_addrlen >= sizeof(sockaddr_in));
            struct sockaddr_in* resolved_addr = (struct sockaddr_in*) best->ai_addr;
            ip_addr = IPAddress::from_ipv4(resolved_addr->sin_addr.s_addr);
        }
    }
    freeaddrinfo(res);
    return ip_addr;
}

#endif // PLY_POSIX

} // namespace ply
