{title text="Input and Output" include="ply-base.h" namespace="ply"}

Plywood provides a composable I/O system built around `Stream` and `Pipe` classes. Streams provide buffered, high-level read/write operations, while pipes represent the underlying I/O mechanisms (files, sockets, memory buffers).

## Standard I/O

These functions provide access to the standard input, output, and error streams.

{apiSummary}
Stream getStdIn(ConsoleMode mode = TEXT)
Stream getStdOut(ConsoleMode mode = TEXT)
Stream getStdErr(ConsoleMode mode = TEXT)
---
Pipe* getStdInPipe()
Pipe* getStdOutPipe()
Pipe* getStdErrPipe()
{/apiSummary}

{apiDescriptions}
Stream getStdIn(ConsoleMode mode = TEXT)
--
Returns a stream for reading from standard input. Pass `BINARY` for binary mode.

>>
Stream getStdOut(ConsoleMode mode = TEXT)
--
Returns a stream for writing to standard output. Pass `BINARY` for binary mode.

>>
Stream getStdErr(ConsoleMode mode = TEXT)
--
Returns a stream for writing to standard error.

>>
Pipe* getStdInPipe()
Pipe* getStdOutPipe()
Pipe* getStdErrPipe()
--
Returns the raw pipe objects for standard I/O. These are singletons and should not be freed.
{/apiDescriptions}

## `Stream`

A `Stream` wraps a `Pipe` and provides buffered I/O operations. Streams handle the details of buffering data for efficient reads and writes.

{apiSummary class=Stream}
Stream()
Stream(Pipe* pipe, bool isPipeOwner)
Stream(Stream&& other)
~Stream()
Stream& operator=(Stream&& other)
bool isOpen()
explicit operator bool()
void close()
bool makeReadable(u32 minBytes = 1)
bool makeWritable(u32 minBytes = 1)
u32 numRemainingBytes() const
StringView viewRemainingBytes() const
MutStringView viewRemainingBytesMut()
void flush(bool toDevice = false)
char readByte()
u32 read(MutStringView dst)
u32 skip(u32 numBytes)
bool write(char c)
u32 write(StringView bytes)
void format(StringView fmt, const Args&... args)
u64 getSeekPos()
void seekTo(u64 seekPos)
{/apiSummary}

{apiDescriptions class=Stream}
Stream()
--
Constructs an empty stream that is not connected to any pipe.

>>
Stream(Pipe* pipe, bool isPipeOwner)
--
Constructs a stream from a pipe. If `isPipeOwner` is true, the stream will destroy the pipe when closed.

>>
Stream(Stream&& other)
--
Move constructor.

>>
Stream& operator=(Stream&& other)
--
Move assignment.

>>
bool isOpen()
--
Returns `true` if the stream is connected to a valid pipe.

>>
explicit operator bool()
--
Same as `isOpen()`.

>>
void close()
--
Flushes and closes the stream. If the stream owns the pipe, the pipe is destroyed.

>>
bool makeReadable(u32 minBytes = 1)
--
Ensures at least `minBytes` are available in the read buffer. Returns `false` if end-of-file is reached.

>>
bool makeWritable(u32 minBytes = 1)
--
Ensures at least `minBytes` of space are available in the write buffer.

>>
u32 numRemainingBytes() const
--
Returns the number of bytes currently available in the read buffer.

>>
StringView viewRemainingBytes() const
--
Returns a view of the bytes currently in the read buffer.

>>
MutStringView viewRemainingBytesMut()
--
Returns a mutable view of the read buffer.

>>
void flush(bool toDevice = false)
--
Writes any buffered data to the underlying pipe. If `toDevice` is true, also flushes the pipe to the physical device.

>>
char readByte()
--
Reads and returns a single byte.

>>
u32 read(MutStringView dst)
--
Reads up to `dst.numBytes` bytes into `dst`. Returns the number of bytes actually read.

>>
u32 skip(u32 numBytes)
--
Skips up to `numBytes` in the input. Returns the number of bytes actually skipped.

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
u64 getSeekPos()
--
Returns the current seek position in the stream.

>>
void seekTo(u64 seekPos)
--
Seeks to the specified position. Only works with seekable pipes.
{/apiDescriptions}

Remember to write `'\n'` for newlines. There's no `endl` like C++ iostreams—use `flush` to force output.

## `MemStream`

A `MemStream` writes to an in-memory buffer that grows as needed. This is useful for building strings or serializing data.

## `ViewStream`

A `ViewStream` reads from a fixed memory buffer (a `StringView`). This is useful for parsing strings or data already in memory.

## `Pipe`

`Pipe` is the abstract base class for all I/O providers. Concrete implementations include file pipes, socket pipes, and memory pipes. You rarely need to work with pipes directly—use `Stream` instead.

{apiSummary class=Pipe}
virtual ~Pipe()
virtual u32 read(MutStringView buf)
virtual bool write(StringView buf)
virtual void flush(bool toDevice = false)
virtual u64 getFileSize()
virtual void seekTo(s64 offset)
u32 getFlags() const
{/apiSummary}

{apiDescriptions class=Pipe}
virtual ~Pipe()
--
Virtual destructor for proper cleanup of derived classes.

>>
virtual u32 read(MutStringView buf)
--
Reads up to `buf.numBytes` bytes into `buf`. Returns the number of bytes actually read. Returns 0 at end-of-file.

>>
virtual bool write(StringView buf)
--
Writes the bytes in `buf`. Returns `true` on success.

>>
virtual void flush(bool toDevice = false)
--
Flushes any buffered writes. If `toDevice` is true, ensures data reaches the physical device.

>>
virtual u64 getFileSize()
--
Returns the total size of the underlying file, or 0 if not applicable.

>>
virtual void seekTo(s64 offset)
--
Seeks to the specified byte offset. Only supported by seekable pipes.

>>
u32 getFlags() const
--
Returns the pipe's capability flags (readable, writable, seekable, etc.).
{/apiDescriptions}
