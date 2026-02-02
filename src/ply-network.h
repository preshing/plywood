/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once

#include "ply-base.h"

#if defined(PLY_WINDOWS) // Windows
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

} // namespace ply
