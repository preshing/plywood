{title text="TCP/IP Networking" include="ply-network.h" namespace="ply"}

Plywood provides a portable networking API for TCP/IP communication. The API supports both IPv4 and IPv6 addresses.

Before using any networking functions, you must call `Network::initialize()`. When finished, call `Network::shutdown()`.

## `IPAddress`

Represents an IP address (either IPv4 or IPv6).

{api_summary class=IPAddress}
u32 net_ordered[4]
IPVersion version() const
bool is_null() const
static constexpr IPAddress local_host(IPVersion ip_version)
static constexpr IPAddress from_ipv4(u32 net_ordered)
String to_string() const
static IPAddress from_string()
{/api_summary}

{api_descriptions class=IPAddress}
u32 net_ordered[4]
--
The raw address bytes in network byte order. For IPv4, only `net_ordered[0]` is used.

>>
IPVersion version() const
--
Returns `IPVersion::V4` or `IPVersion::V6`.

>>
bool is_null() const
--
Returns `true` if this is a null/uninitialized address.

>>
static constexpr IPAddress local_host(IPVersion ip_version)
--
Returns the localhost address (127.0.0.1 for IPv4, ::1 for IPv6).

>>
static constexpr IPAddress from_ipv4(u32 net_ordered)
--
Creates an IPv4 address from a 32-bit value in network byte order.

>>
String to_string() const
--
Returns a human-readable string representation of the address.

>>
static IPAddress from_string()
--
Parses an IP address from a string.
{/api_descriptions}

## `Network`

The `Network` class provides static methods for network initialization and connection management.

{api_summary class=Network}
static void initialize(IPVersion ip_version)
static void shutdown()
static TCPListener bind_tcp(u16 port)
static Owned<TCPConnection> connect_tcp(const IPAddress& address, u16 port)
static IPAddress resolve_host_name(StringView host_name, IPVersion ip_version)
static IPResult last_result()
{/api_summary}

{api_descriptions class=Network}
static void initialize(IPVersion ip_version)
--
Initializes the networking subsystem. Must be called before any other networking functions. Specify `IPVersion::V4` or `IPVersion::V6`.

>>
static void shutdown()
--
Shuts down the networking subsystem and releases resources.

>>
static TCPListener bind_tcp(u16 port)
--
Creates a TCP listener bound to the specified port. The listener can accept incoming connections.

>>
static Owned<TCPConnection> connect_tcp(const IPAddress& address, u16 port)
--
Establishes a TCP connection to the specified address and port. Returns null on failure.

>>
static IPAddress resolve_host_name(StringView host_name, IPVersion ip_version)
--
Resolves a hostname (e.g., "example.com") to an IP address using DNS.

>>
static IPResult last_result()
--
Returns the result code from the most recent network operation.
{/api_descriptions}

## `TCPConnection`

Represents an established TCP connection to a remote host.

{api_summary class=TCPConnection}
PipeWinsock in_pipe
PipeWinsock out_pipe
---
TCPConnection()
~TCPConnection()
const IPAddress& remote_address() const
u16 remote_port() const
SOCKET get_handle() const
Stream create_in_stream()
Stream create_out_stream()
{/api_summary}

{api_descriptions class=TCPConnection}
PipeWinsock in_pipe
PipeWinsock out_pipe
--
The underlying pipe objects for reading and writing. Typically, use `create_in_stream()` and `create_out_stream()` instead.

>>
TCPConnection()
~TCPConnection()
--
Constructor and destructor. Connections are typically created via `Network::connect_tcp()` or `TCPListener::accept()`.

>>
const IPAddress& remote_address() const
--
Returns the IP address of the remote host.

>>
u16 remote_port() const
--
Returns the port number of the remote host.

>>
SOCKET get_handle() const
--
Returns the underlying socket handle. Use with care.

>>
Stream create_in_stream()
--
Creates a buffered stream for reading data from the connection.

>>
Stream create_out_stream()
--
Creates a buffered stream for writing data to the connection.
{/api_descriptions}

## `TCPListener`

A `TCPListener` listens for incoming TCP connections on a specific port.

{api_summary class=TCPListener}
TCPListener(SOCKET listen_socket = INVALID_SOCKET)
TCPListener(TCPListener&& other)
~TCPListener()
TCPListener& operator=(TCPListener&& other)
bool is_valid()
void end_comm()
void close()
Owned<TCPConnection> accept()
{/api_summary}

{api_descriptions class=TCPListener}
TCPListener(SOCKET listen_socket = INVALID_SOCKET)
--
Constructs a listener from a socket handle. Typically created via `Network::bind_tcp()`.

>>
TCPListener(TCPListener&& other)
--
Move constructor.

>>
TCPListener& operator=(TCPListener&& other)
--
Move assignment.

>>
bool is_valid()
--
Returns `true` if the listener is bound to a valid socket.

>>
void end_comm()
--
Signals that no more connections will be accepted. Causes any blocking `accept()` call to return.

>>
void close()
--
Closes the listener socket.

>>
Owned<TCPConnection> accept()
--
Blocks until a client connects, then returns the new connection. Returns null if the listener was closed.
{/api_descriptions}

{example}
// Simple echo server
Network::initialize(IPVersion::V4);
TCPListener listener = Network::bind_tcp(8080);

while (true) {
    Owned<TCPConnection> conn = listener.accept();
    if (!conn) break;
    
    Stream in = conn->create_in_stream();
    Stream out = conn->create_out_stream();
    
    String line = read_line(in);
    out.write(line);
    out.write("\n");
    out.flush();
}

Network::shutdown();
{/example}
