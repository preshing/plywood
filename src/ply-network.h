/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#pragma once

#include "ply-base.h"

#if defined(_WIN32)  // Windows
#include <winsock2.h>
#include <ws2tcpip.h>
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
    u32 net_ordered[4]; // big endian

    IPVersion version() const {
        return (this->net_ordered[0] == 0 && this->net_ordered[1] == 0 &&
                this->net_ordered[2] == convert_big_endian(0xffffu))
                   ? IPV4
                   : IPV6;
    }
    bool is_null() const {
        return this->net_ordered[0] == 0 && this->net_ordered[1] == 0 && this->net_ordered[2] == 0 &&
               this->net_ordered[3] == 0;
    }
    static constexpr IPAddress local_host(IPVersion ip_version) {
        return (ip_version == IPV4) ? IPAddress{{0, 0, convert_big_endian(0xffffu), convert_big_endian(0x7f000001u)}}
                                    : IPAddress{{0, 0, 0, convert_big_endian(1u)}};
    }
    static constexpr IPAddress from_ipv4(u32 net_ordered) {
        return {{0, 0, convert_big_endian(0xffffu), net_ordered}};
    }
    String to_string() const;
    static IPAddress from_string();
};

//  ▄▄  ▄▄         ▄▄                          ▄▄
//  ███ ██  ▄▄▄▄  ▄██▄▄ ▄▄    ▄▄  ▄▄▄▄  ▄▄▄▄▄  ██  ▄▄
//  ██▀███ ██▄▄██  ██   ██ ██ ██ ██  ██ ██  ▀▀ ██▄█▀
//  ██  ██ ▀█▄▄▄   ▀█▄▄  ██▀▀██  ▀█▄▄█▀ ██     ██ ▀█▄
//

struct TCPListener;
struct TCPConnection;

#if defined(_WIN32)

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
#if defined(_WIN32)
    using Handle = SOCKET;
    static constexpr Handle InvalidHandle = INVALID_SOCKET;
#elif defined(PLY_POSIX)
    using Handle = int;
    static constexpr Handle InvalidHandle = -1;
#endif

public:
    static bool IsInit;
    static bool HasIPv6;
    static ThreadLocal<IPResult> last_result_;

    static void initialize(IPVersion ip_version);
    static void shutdown();
    static TCPListener bind_tcp(u16 port);
    static Owned<TCPConnection> connect_tcp(const IPAddress& address, u16 port);
    static IPAddress resolve_host_name(StringView host_name, IPVersion ip_version);
    static IPResult last_result() {
        return Network::last_result_.load();
    }
};

//  ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄                                     ▄▄   ▄▄
//    ██   ██  ▀▀ ██  ██ ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██     ██▀▀▀  ██     ██  ██ ██  ██ ██  ██ ██▄▄██ ██     ██   ██ ██  ██ ██  ██
//    ██   ▀█▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄█▀ ██  ██ ██  ██ ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██
//

#if defined(_WIN32)

struct TCPConnection {
    IPAddress remote_addr_;
    u16 remote_port_ = 0;
    Owned<PipeWinsock> in_pipe;
    Owned<PipeWinsock> out_pipe;

    TCPConnection() {
    }
    ~TCPConnection();
    const IPAddress& remote_address() const {
        return this->remote_addr_;
    }
    u16 remote_port() const {
        return this->remote_port_;
    }
    SOCKET get_handle() const {
        return in_pipe->socket;
    }
    Stream create_in_stream() {
        return Stream{this->in_pipe, false};
    }
    Stream create_out_stream() {
        return Stream{this->out_pipe, false};
    }
};

#elif defined(PLY_POSIX)

struct TCPConnection {
    IPAddress remote_addr_;
    u16 remote_port_ = 0;
    Owned<Pipe_FD> in_pipe;
    Owned<Pipe_FD> out_pipe;

    TCPConnection() {
    }
    ~TCPConnection();
    const IPAddress& remote_address() const {
        return this->remote_addr_;
    }
    u16 remote_port() const {
        return this->remote_port_;
    }
    int get_socket() const {
        return in_pipe.fd;
    }
    Stream create_in_stream() {
        return Stream{&this->in_pipe, false};
    }
    Stream create_out_stream() {
        return Stream{&this->out_pipe, false};
    }
};

#endif

//  ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄    ▄▄         ▄▄
//    ██   ██  ▀▀ ██  ██ ██    ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//    ██   ██     ██▀▀▀  ██    ██ ▀█▄▄▄   ██   ██▄▄██ ██  ██ ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██     ██▄▄▄ ██  ▄▄▄█▀  ▀█▄▄ ▀█▄▄▄  ██  ██ ▀█▄▄▄  ██
//

#if defined(_WIN32)

struct TCPListener {
public:
    SOCKET listen_socket = INVALID_SOCKET;

    TCPListener(SOCKET listen_socket = INVALID_SOCKET) : listen_socket{listen_socket} {
    }
    TCPListener(TCPListener&& other) {
        this->listen_socket = other.listen_socket;
        other.listen_socket = INVALID_SOCKET;
    }
    ~TCPListener() {
        if (this->listen_socket >= 0) {
            closesocket(this->listen_socket);
        }
    }
    TCPListener& operator=(TCPListener&& other) {
        if (this->listen_socket >= 0) {
            closesocket(this->listen_socket);
        }
        this->listen_socket = other.listen_socket;
        other.listen_socket = INVALID_SOCKET;
        return *this;
    }
    bool is_valid() {
        return this->listen_socket >= 0;
    }
    void end_comm() {
        shutdown(this->listen_socket, SD_BOTH);
    }
    void close() {
        if (this->listen_socket >= 0) {
            closesocket(this->listen_socket);
            this->listen_socket = INVALID_SOCKET;
        }
    }

    Owned<TCPConnection> accept();
};

#elif defined(PLY_POSIX)

struct TCPListener {
public:
    int listen_socket = -1;

    TCPListener(int listen_socket = -1) : listen_socket{listen_socket} {
    }
    TCPListener(TCPListener&& other) {
        this->listen_socket = other.listen_socket;
        other.listen_socket = -1;
    }
    ~TCPListener() {
        if (this->listen_socket >= 0) {
            ::close(this->listen_socket);
        }
    }
    TCPListener& operator=(TCPListener&& other) {
        if (this->listen_socket >= 0) {
            ::close(this->listen_socket);
        }
        this->listen_socket = other.listen_socket;
        other.listen_socket = -1;
        return *this;
    }
    bool is_valid() {
        return this->listen_socket >= 0;
    }
    void end_comm() {
        shutdown(this->listen_socket, SHUT_RDWR);
    }
    void close() {
        if (this->listen_socket >= 0) {
            ::close(this->listen_socket);
            this->listen_socket = -1;
        }
    }

    Owned<TCPConnection> accept();
};

#endif

} // namespace ply
