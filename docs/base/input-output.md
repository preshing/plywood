{title text="Input and Output" include="ply-base.h" namespace="ply"}

Plywood provides a composable I/O system built around `Stream` and `Pipe` classes. Streams provide buffered, high-level read/write operations, while pipes represent the underlying I/O mechanisms (files, sockets, memory buffers).

## Standard I/O

These functions provide access to the standard input, output, and error streams.

{api_summary}
Stream get_stdin(ConsoleMode mode = TEXT)
Stream get_stdout(ConsoleMode mode = TEXT)
Stream get_stderr(ConsoleMode mode = TEXT)
---
Pipe* get_stdin_pipe()
Pipe* get_stdout_pipe()
Pipe* get_stderr_pipe()
{/api_summary}

{api_descriptions}
Stream get_stdin(ConsoleMode mode = TEXT)
--
Returns a stream for reading from standard input. Pass `BINARY` for binary mode.

>>
Stream get_stdout(ConsoleMode mode = TEXT)
--
Returns a stream for writing to standard output. Pass `BINARY` for binary mode.

>>
Stream get_stderr(ConsoleMode mode = TEXT)
--
Returns a stream for writing to standard error.

>>
Pipe* get_stdin_pipe()
Pipe* get_stdout_pipe()
Pipe* get_stderr_pipe()
--
Returns the raw pipe objects for standard I/O. These are singletons and should not be freed.
{/api_descriptions}

## `Stream`

A `Stream` wraps a `Pipe` and provides buffered I/O operations. Streams handle the details of buffering data for efficient reads and writes.

{api_summary class=Stream}
Stream()
Stream(Pipe* pipe, bool is_pipe_owner)
Stream(Stream&& other)
~Stream()
Stream& operator=(Stream&& other)
bool is_open()
explicit operator bool()
void close()
bool make_readable(u32 min_bytes = 1)
bool make_writable(u32 min_bytes = 1)
u32 num_remaining_bytes() const
StringView view_remaining_bytes() const
MutStringView view_remaining_bytes_mut()
void flush(bool to_device = false)
char read_byte()
u32 read(MutStringView dst)
u32 skip(u32 num_bytes)
bool write(char c)
u32 write(StringView bytes)
void format(StringView fmt, const Args&... args)
u64 get_seek_pos()
void seek_to(u64 seek_pos)
{/api_summary}

{api_descriptions class=Stream}
Stream()
--
Constructs an empty stream that is not connected to any pipe.

>>
Stream(Pipe* pipe, bool is_pipe_owner)
--
Constructs a stream from a pipe. If `is_pipe_owner` is true, the stream will destroy the pipe when closed.

>>
Stream(Stream&& other)
--
Move constructor.

>>
Stream& operator=(Stream&& other)
--
Move assignment.

>>
bool is_open()
--
Returns `true` if the stream is connected to a valid pipe.

>>
explicit operator bool()
--
Same as `is_open()`.

>>
void close()
--
Flushes and closes the stream. If the stream owns the pipe, the pipe is destroyed.

>>
bool make_readable(u32 min_bytes = 1)
--
Ensures at least `min_bytes` are available in the read buffer. Returns `false` if end-of-file is reached.

>>
bool make_writable(u32 min_bytes = 1)
--
Ensures at least `min_bytes` of space are available in the write buffer.

>>
u32 num_remaining_bytes() const
--
Returns the number of bytes currently available in the read buffer.

>>
StringView view_remaining_bytes() const
--
Returns a view of the bytes currently in the read buffer.

>>
MutStringView view_remaining_bytes_mut()
--
Returns a mutable view of the read buffer.

>>
void flush(bool to_device = false)
--
Writes any buffered data to the underlying pipe. If `to_device` is true, also flushes the pipe to the physical device.

>>
char read_byte()
--
Reads and returns a single byte.

>>
u32 read(MutStringView dst)
--
Reads up to `dst.num_bytes` bytes into `dst`. Returns the number of bytes actually read.

>>
u32 skip(u32 num_bytes)
--
Skips up to `num_bytes` in the input. Returns the number of bytes actually skipped.

>>
bool write(char c)
--
Writes a single byte.

>>
u32 write(StringView bytes)
--
Writes the given bytes to the stream. Returns the number of bytes written.

>>
void format(StringView fmt, const Args&... args)
--
Writes formatted text using `{}` placeholders.

>>
u64 get_seek_pos()
--
Returns the current seek position in the stream.

>>
void seek_to(u64 seek_pos)
--
Seeks to the specified position. Only works with seekable pipes.
{/api_descriptions}

Remember to write `'\n'` for newlines. There's no `endl` like C++ iostreams—use `flush` to force output.

## `MemStream`

A `MemStream` writes to an in-memory buffer that grows as needed. This is useful for building strings or serializing data.

## `ViewStream`

A `ViewStream` reads from a fixed memory buffer (a `StringView`). This is useful for parsing strings or data already in memory.

## `Pipe`

`Pipe` is the abstract base class for all I/O providers. Concrete implementations include file pipes, socket pipes, and memory pipes. You rarely need to work with pipes directly—use `Stream` instead.

{api_summary class=Pipe}
virtual ~Pipe()
virtual u32 read(MutStringView buf)
virtual bool write(StringView buf)
virtual void flush(bool to_device = false)
virtual u64 get_file_size()
virtual void seek_to(s64 offset)
u32 get_flags() const
{/api_summary}

{api_descriptions class=Pipe}
virtual ~Pipe()
--
Virtual destructor for proper cleanup of derived classes.

>>
virtual u32 read(MutStringView buf)
--
Reads up to `buf.num_bytes` bytes into `buf`. Returns the number of bytes actually read. Returns 0 at end-of-file.

>>
virtual bool write(StringView buf)
--
Writes the bytes in `buf`. Returns `true` on success.

>>
virtual void flush(bool to_device = false)
--
Flushes any buffered writes. If `to_device` is true, ensures data reaches the physical device.

>>
virtual u64 get_file_size()
--
Returns the total size of the underlying file, or 0 if not applicable.

>>
virtual void seek_to(s64 offset)
--
Seeks to the specified byte offset. Only supported by seekable pipes.

>>
u32 get_flags() const
--
Returns the pipe's capability flags (readable, writable, seekable, etc.).
{/api_descriptions}
