{title text="TCP/IP Networking" include="ply-network.h" namespace="ply"}

Plywood provides a portable networking API for TCP/IP communication. The API supports both IPv4 and IPv6 addresses.

Before using any networking functions, you must call `Network::initialize()`. When finished, call `Network::shutdown()`.

## `IPAddress`

Represents an IP address (either IPv4 or IPv6).

{apiSummary class=IPAddress}
u32 netOrdered[4]
IPVersion version() const
bool isNull() const
static constexpr IPAddress localHost(IPVersion ipVersion)
static constexpr IPAddress from_ipv4(u32 netOrdered)
String toString() const
static IPAddress fromString()
{/apiSummary}

{context class=IPAddress}

`u32 netOrdered[4]`
> The raw address bytes in network byte order. For IPv4, only `netOrdered[0]` is used.

`IPVersion version() const`
> Returns `IPVersion::V4` or `IPVersion::V6`.

`bool isNull() const`
> Returns `true` if this is a null/uninitialized address.

`static constexpr IPAddress localHost(IPVersion ipVersion)`
> Returns the localhost address (127.0.0.1 for IPv4, ::1 for IPv6).

`static constexpr IPAddress from_ipv4(u32 netOrdered)`
> Creates an IPv4 address from a 32-bit value in network byte order.

`String toString() const`
> Returns a human-readable string representation of the address.

`static IPAddress fromString()`
> Parses an IP address from a string.

## `Network`

The `Network` class provides static methods for network initialization and connection management.

{apiSummary class=Network}
static void initialize(IPVersion ipVersion)
static void shutdown()
static TCPListener bindTcp(u16 port)
static Owned<TCPConnection> connectTcp(const IPAddress& address, u16 port)
static IPAddress resolveHostName(StringView hostName, IPVersion ipVersion)
static IPResult lastResult()
{/apiSummary}

{context class=Network}

`static void initialize(IPVersion ipVersion)`
> Initializes the networking subsystem. Must be called before any other networking functions. Specify `IPVersion::V4` or `IPVersion::V6`.

`static void shutdown()`
> Shuts down the networking subsystem and releases resources.

`static TCPListener bindTcp(u16 port)`
> Creates a TCP listener bound to the specified port. The listener can accept incoming connections.

`static Owned<TCPConnection> connectTcp(const IPAddress& address, u16 port)`
> Establishes a TCP connection to the specified address and port. Returns null on failure.

`static IPAddress resolveHostName(StringView hostName, IPVersion ipVersion)`
> Resolves a hostname (e.g., "example.com") to an IP address using DNS.

`static IPResult lastResult()`
> Returns the result code from the most recent network operation.

## `TCPConnection`

Represents an established TCP connection to a remote host.

{apiSummary class=TCPConnection}
PipeWinsock inPipe
PipeWinsock outPipe
---
TCPConnection()
~TCPConnection()
const IPAddress& remoteAddress() const
u16 remotePort() const
SOCKET getHandle() const
Stream createInStream()
Stream createOutStream()
{/apiSummary}

{context class=TCPConnection}

`PipeWinsock inPipe`
`PipeWinsock outPipe`
> The underlying pipe objects for reading and writing. Typically, use `createInStream()` and `createOutStream()` instead.

`TCPConnection()`
`~TCPConnection()`
> Constructor and destructor. Connections are typically created via `Network::connectTcp()` or `TCPListener::accept()`.

`const IPAddress& remoteAddress() const`
> Returns the IP address of the remote host.

`u16 remotePort() const`
> Returns the port number of the remote host.

`SOCKET getHandle() const`
> Returns the underlying socket handle. Use with care.

`Stream createInStream()`
> Creates a buffered stream for reading data from the connection.

`Stream createOutStream()`
> Creates a buffered stream for writing data to the connection.

## `TCPListener`

A `TCPListener` listens for incoming TCP connections on a specific port.

{apiSummary class=TCPListener}
TCPListener(SOCKET listenSocket = INVALID_SOCKET)
TCPListener(TCPListener&& other)
~TCPListener()
TCPListener& operator=(TCPListener&& other)
bool isValid()
void endComm()
void close()
Owned<TCPConnection> accept()
{/apiSummary}

{context class=TCPListener}

`TCPListener(SOCKET listenSocket = INVALID_SOCKET)`
> Constructs a listener from a socket handle. Typically created via `Network::bindTcp()`.

`TCPListener(TCPListener&& other)`
> Move constructor.

`TCPListener& operator=(TCPListener&& other)`
> Move assignment.

`bool isValid()`
> Returns `true` if the listener is bound to a valid socket.

`void endComm()`
> Signals that no more connections will be accepted. Causes any blocking `accept()` call to return.

`void close()`
> Closes the listener socket.

`Owned<TCPConnection> accept()`
> Blocks until a client connects, then returns the new connection. Returns null if the listener was closed.

{example}
// Simple echo server
Network::initialize(IPVersion::V4);
TCPListener listener = Network::bindTcp(8080);

while (true) {
    Owned<TCPConnection> conn = listener.accept();
    if (!conn) break;
    
    Stream in = conn->createInStream();
    Stream out = conn->createOutStream();
    
    String line = readLine(in);
    out.write(line);
    out.write("\n");
    out.flush();
}

Network::shutdown();
{/example}
