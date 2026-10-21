Input and Output (`ply-system.h`)
=================================

Plywood's input/output (I/O) operations are organized into three layers, with each layer building on the layers underneath it.

1. **Low-level OS support**: [`Pipe`](#pipe) encapsulates files, sockets or conversion routines.
2. **Buffered access**: [`Stream`](#stream) allows callers to perform I/O directly on an internal memory buffer, while optionally wrapping a `Pipe`.
3. **High-level serialization**: `Stream` also provides convenience functions to read and write common data formats to the internal memory buffer.

## `Pipe`

`Pipe` is a virtual base class that encapsulates a file, a network socket or a conversion routine. Applications rarely use this class directly, as [`Stream`](#stream) provides a better general-purpose interface for data serialization.

`Pipe` instances are typically allocated as opaque objects on the heap and stored in `Owned` pointers. 

```
Owned<Pipe> in = FileSystem::openPipeForRead("foo.txt");
```

Only one thread may call `read`, `tryRead`, `write`, `flush` or `seekTo` at a time on each pipe. Not every member function is supported by every pipe. The caller is responsible for calling supported functions only.

Each call to `Pipe` typically involves a system call into the underlying OS, which can be expensive. To reduce overhead, it's better to pass large chunks of data (in the ~2-64 KB range) to each `read` or `write` call at a time. `Stream` takes care of this automatically.

Any underlying file handles held by the `Pipe` are closed when the `Pipe` is destroyed.

`u32 Pipe::getCapabilities() const`
> Returns the pipe's capability flags.
>
> | Flag | Meaning |
> | ---- | ------- |
> | `Pipe::CanRead` | The pipe supports `read` and `tryRead`. |
> | `Pipe::CanWrite` | The pipe supports `write` and `flush`. |
> | `Pipe::CanSeek` | The pipe supports `seekTo` and `getFileSize`. |

`IOResult Pipe::read(MutStringView buf)`
> Reads up to `buf.numBytes` bytes into `buf`.
> The calling thread will block until at least one byte is successfully read from the underlying data source.
> `buf` must not be empty.
>
> `IOResult` has the following data members:
>
> | | |
> | ---- | ------- |
> | `Code code` | Outcome of the operation. |
> | `u32 numBytes` | Number of bytes transferred. Only non-zero when `code` is `OK`, or when a write partially completes before returning `Error` or `Disabled`. |
>
> Possible `Code` values:
> 
> | Value | Meaning |
> | ---- | ------- |
> | `OK` | The operation completed successfully. `numBytes` bytes were transferred. |
> | `EndOfFile` | The source is exhausted; no more data will arrive. |
> | `WouldBlock` | No data is available right now, but more may arrive later. (Only returned by `tryRead`.) |
> | `Error` | The operation failed due to an I/O error. |
> | `Disabled` | The pipe was disabled. |

`IOResult Pipe::tryRead(MutStringView buf)`
> Reads as much data as possible without blocking. `buf` must not be empty.

`IOResult Pipe::write(StringView buf)`
> Writes the bytes in `buf`. Returns `OK` if all bytes were written, `Disabled` if the pipe was disabled,
> or `Error` if the write failed. On a partial failure, `numBytes` gives the number of bytes actually
> written. This function may block if the pipe's destination isn't ready to receive.

`void Pipe::flush(bool toDevice = false)`
> Flushes any buffered writes. If `toDevice` is true, ensures data reaches the physical device.

`void Pipe::seekTo(s64 offset)`
> Seeks to the specified byte offset. Only supported by seekable pipes.

`u64 Pipe::getFileSize()`
> Returns the total size of the underlying file.

`void Pipe::disable()`
> Disables communication on this pipe. It's safe to call this from any thread at any time.
> If another thread is blocked inside `read` or `write`, `disable` unblocks it.
> Once a pipe has been disabled, `read` and `write` become non-blocking functions
> and the pipe makes no further calls into the underlying operating system,
> but handles remain open until the `Pipe` is destroyed.

### `MemPipe`

A `MemPipe` is a write-only `Pipe` that appends chunks of data to an arbitrary-length queue of memory. This queue can be consumed by creating one or more [`MemReader`](#memreader) instances.

`MemPipe::MemPipe(bool threadSafe = true)`
> Constructs an empty memory pipe. If `threadSafe` is `true`, it's safe to use `MemReaders` for this `MemPipe` concurrently from different threads. If `threadSafe` is `false`, callers must ensure mutual exclusion between this `MemPipe` and its `MemReaders`.

`void MemPipe::close()`
> Stops accepting writes and wakes all waiting readers. Existing data remains available to every reader; each reader reports `EndOfFile` once it catches up.

It's safe to destroy the `MemPipe` while `MemReader`s are still reading, in which case the `MemPipe` is closed and the `MemReader`s continue to consume remaining buffered data.

## `Stream`

`Stream` is a higher-level interface for data serialization. Each `Stream` instance exposes an internal memory buffer that applications can read and write directly, plus a set of convenience functions for serializing common formats into this buffer. A stream can either work as a wrapper around `Pipe` or it can store its contents entirely in memory.

`Stream` objects have a reasonably small fixed size, so they're often returned by value and stored directly in local variables.

```
Stream out = getStdOut();
out.write("Hello world!\n");
```

An underlying I/O failure sets the stream's status to `Error`. Buffered reads may remain available, but any data written to the internal buffer since the last successful write should be considered lost. While the status is `Error`, the stream is effectively disabled. Call `clearError()` to permit another attempt.

### Buffer Access

`Stream` exposes the following public data members.

| | |
| --- | --- |
| `char* curByte` | Current read/write position in the internal buffer. |
| `char* endByte` | The end of the readable/writable portion of the internal buffer. |

Callers are allowed to read or write the data at `curByte` as long as it's less than `endByte`. Callers are also allowed to increment the `curByte` pointer, but not beyond `endByte`.

`u32 Stream::getCapabilities() const`
> Returns the stream's capability flags.
>
> | Flag | Meaning |
> | ---- | ------- |
> | `Stream::CanRead` | The stream supports reading. |
> | `Stream::CanWrite` | The stream supports writing. |
> | `Stream::CanSeek` | The stream supports `seekTo`. |

`bool Stream::isOpen()`
`explicit Stream::operator bool()`
> Returns `true` if the stream is open.

`Stream::Mode Stream::getMode() const`
> Returns `Reading`, `Writing` or `Closed`.

`void Stream::close()`
> Flushes and closes the stream. If the stream owns the pipe, the pipe is destroyed.
> May block if the stream is in write mode and there is data to flush.

`IOResult::Code Stream::getStatus() const`
> Returns the current I/O status. Operations using only the current buffer leave it unchanged. `Error` persists until `clearError()` is called.

`bool Stream::atEof() const`
> Returns `true` when the internal buffer is empty and `getStatus()` is `EndOfFile`, `Error` or `Disabled`.

`bool Stream::makeReadable(u32 minBytes = 1)`
> This function blocks until `minBytes` are available to read or reading fails.
> Returns `true` if `minBytes` are available; `false` otherwise.
> Check `getStatus` to determine the cause.

`bool Stream::tryMakeReadable()`
> This function tries to make some data readable without blocking.
> Returns `true` if one or more bytes of data are available; `false` otherwise.
> Check `getStatus` to determine the cause.
>
> Non-blocking behavior is only guaranteed if the stream is in already in read mode.
> If the stream is in write mode with buffered data, this function may block as the data gets flushed.
> To ensure `tryMakeReadable` remains non-blocking, either avoid writing data or
> flush the internal buffer first by calling `flush`.

`bool Stream::makeWritable(u32 minBytes = 1)`
> Ensures at least `minBytes` of space are available in the write buffer.
> May block until the underlying destination is ready to receive.
> If the stream is a `MemStream`, `minBytes` must be 1, which is sufficient to support `format` and `write`.

`void Stream::flush(bool toDevice = false)`
> Does nothing in read mode.
> In write mode, this function may block as buffered data gets written to to the underlying pipe. If `toDevice` is true, also flushes the pipe to the physical device.

`void Stream::seekTo(u64 seekPos)`
> Seeks to the specified position. Only works with seekable streams.

`u64 Stream::getSeekPos()`
> Returns the current seek position in the stream.

`bool Stream::hasRemainingBytes() bool`
> Returns `true` if there are any bytes remaining in the read buffer.

`u32 Stream::numRemainingBytes() const`
> Returns the number of bytes currently available in the read buffer.

`StringView Stream::viewRemainingBytes() const`
> Returns a view of the bytes currently in the read buffer.

`MutStringView Stream::viewRemainingBytesMut()`
> Returns a mutable view of the read buffer.

`void Stream::disable()`
> Disables the underlying pipe, which can release operations blocked on that pipe. It's also legal to
> call this function on a non-pipe stream, in which case it has no effect. Any internally-buffered data will remain available for read after `disable()` returns, but `getStatus()` will eventually report
> `Disabled` after it runs out.

`void Stream::clearError()`
> Changes `Error` status to `OK`, allowing subsequent operations to retry. Other statuses are unchanged.

`Pipe* Stream::getPipe() const`
> Returns the underlying pipe for a pipe-backed stream, or `nullptr` for other stream types.

### High-Level Serialization

`char Stream::peekByte()`
> Returns the next byte in the input stream, or `0` if at end-of-file.

`char Stream::readByte()`
> Reads and returns a single byte.

`u32 Stream::read(MutStringView dst)`
> Reads up to `dst.numBytes` bytes into `dst`. Returns the number of bytes actually read.

`u32 Stream::tryRead(MutStringView dst)`
> Reads as much data as possible without blocking, up to `dst.numBytes` bytes. Returns the number
> of bytes actually read and leaves the rest of `dst` unchanged. A return value of 0 can mean that
> no data is immediately available, end-of-file was reached, an I/O error occurred, or the pipe was
> disabled. Inspect `getStatus()` to distinguish those cases.

`u32 Stream::skip(u32 numBytes)`
> Skips up to `numBytes` in the input. Returns the number of bytes actually skipped.

`bool Stream::write(char c)`
> Writes a single byte.

`u32 Stream::write(StringView bytes)`
> Writes the given bytes to the stream. Returns the number of bytes written.

`void Stream::format(StringView fmt, const Args&... args)`
> Writes formatted text using `{}` placeholders.

`Stream` has several subclasses, but these subclasses merely provide different ways of constructing `Stream` objects. None of these subclasses add any data members, so they can be safely assigned directly to variables of type `Stream`.

- `MemStream` is low-overhead, write-only memory stream whose contents can be converted to `String`.
- `MemFile` is a readable, writable, seekable virtual file backed by memory.
- `ViewStream` is a simple wrapper around a `StringView`.
- `MemReader` reads data written to a `MemPipe`.

### `MemStream`

A `MemStream` writes to an in-memory buffer that grows as needed, with the goal of ultimately converting to a `String`.
It's more efficient than `MemFile`, but doesn't support seeking or reads.
Copy construction and copy assignment create independent deep copies.

`String MemStream::copyToString() const`
> Copies the complete contents to a string without changing the stream.

`String MemStream::moveToString()`
> Returns the complete contents and closes the stream. Transfers its storage directly when possible.

### `MemFile`

A `MemFile` is a readable, writable, seekable file stored in memory that supports contiguous buffer requests up to `Stream::MaxConsecutiveBytes`.

Copy construction and copy assignment create independent deep copies with the same contents, mode
and seek position, including pending buffered writes. Moving transfers ownership and closes the
source file.

### `ViewStream`

A `ViewStream` reads from a fixed memory buffer (a `StringView`). This is useful for parsing strings or data already in memory.

### `MemReader`

A `MemReader` is a read-only `Stream` that consumes data written to a `MemPipe`. It's safe to create multiple `MemReader`s for a single `MemPipe`.

`MemReader::MemReader(MemPipe* memPipe)`
> Constructs a reader positioned at the beginning of `memPipe`, including data written before the reader was created. `memPipe` must not be null.

```
Owned<MemPipe> pipe = Heap::create<MemPipe>();
MemReader reader{pipe};
pipe->write("hello");
pipe.clear(); // Closes the writer; reader keeps the buffered data alive.

char bytes[5];
u32 numRead = reader.read({bytes, sizeof(bytes)}); // Reads all five bytes.
bool more = reader.makeReadable(); // false; getStatus() is EndOfFile.
```

## Standard I/O

These functions provide access to the standard input, output, and error streams.

`Stream getStdIn(ConsoleMode mode = ConsoleMode::Text)`
> Returns a stream for reading from standard input. Pass `ConsoleMode::Binary` for binary mode.

`Stream getStdOut(ConsoleMode mode = ConsoleMode::Text)`
> Returns a stream for writing to standard output. Pass `ConsoleMode::Binary` for binary mode.

`Stream getStdErr(ConsoleMode mode = ConsoleMode::Text)`
> Returns a stream for writing to standard error.

`Pipe* getStdInPipe()`
`Pipe* getStdOutPipe()`
`Pipe* getStdErrPipe()`
> Returns the raw pipe objects for standard I/O. These are singletons and should not be freed.

## Reading Text

These functions parse common text patterns from input streams.

`String readLine(Stream& in)`
`StringView readLine(ViewStream& viewIn)`
> Reads characters until a newline or end-of-file. The newline character is consumed but not included in the result.

`String readWhitespace(Stream& in)`
`StringView readWhitespace(ViewStream& in)`
> Reads and returns a sequence of whitespace characters.

`void skipWhitespace(Stream& in)`
> Consumes whitespace characters without returning them.

`String readIdentifier(Stream& in, u32 flags = 0)`
`StringView readIdentifier(ViewStream& viewIn, u32 flags = 0)`
> Reads a C-style identifier (letters, digits, underscores, starting with a non-digit).

`ReadNumber<u64> readU64FromText(Stream& in, u32 radix = 10)`
> Parses an unsigned integer from the stream. The `radix` parameter specifies the number base (e.g., 10 for decimal, 16 for hexadecimal). The result contains the parsed `value` and a `success` flag that is false when there are no digits or the value overflows.

`ReadNumber<s64> readS64FromText(Stream& in, u32 radix = 10)`
> Parses a signed integer from the stream. Handles an optional leading `-` sign and reports invalid or out-of-range values through `success`.

`ReadNumber<double> readDoubleFromText(Stream& in, u32 radix = 10)`
> Parses a floating-point number from the stream. `success` is false if no digits were found.

`String readQuotedString(Stream& in, QuotedStringType type = QuotedStringType::C, bool strict = true, Functor<void(QuotedStringError)> errorCallback = {})`
> Reads a quoted string starting at its opening quote. The `type` parameter selects the string-literal grammar (`C`, `JavaScript`, `JSON` or `Python`). In strict mode, malformed escape sequences are rejected. In permissive mode, malformed escapes are recovered in a way that tries to preserve the original text. Calls `errorCallback` if parsing fails.

## Writing Text

Most of these functions are used internally by `String::format`, but you can call them directly as well.

`void printNumber(Stream& out, s32 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, u32 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, s64 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, u64 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, double value, const NumberFormat& format = {})`
> Writes a number to the given stream. `NumberFormat` has the following members:
>
> | | |
> | --- | --- |
> | `u32 radix` | The numeric base. Default is 10. |
> | `bool capitalize` | Uses uppercase letters for digits and notation when `true`. Default is false. |
> | `u32 zeroPad` | Pads numbers shorter than this width with leading zeros. Default is 0. |
> | `SignMode signMode` | Controls whether and how space is reserved for a sign. Default is ShowMinusOnly. |
> | `FloatMode floatMode` | Controls the notation used for floating-point values. Default is Auto. |
> | `u32 fractionalPrecision` | Controls the fractional precision of floating-point values. Default is 3. |
>
> `NumberFormat::FloatMode` has the values `Regular`, `Scientific` and `Auto`. `NumberFormat::SignMode` has the values
> `ShowMinusOnly`, `ShowPlus` and `LeaveSpaceForSign`.

`void printEscapedString(Stream& out, StringView str)`
> Writes a string with C-style escape sequences for special characters (e.g., `\n`, `\t`, `\\`).

`void printXmlEscapedString(Stream& out, StringView str)`
> Writes a string with XML entity escaping (e.g., `&lt;`, `&gt;`, `&amp;`).

## Unicode Conversion

These functions convert between Unicode codepoints and various encoded representations (UTF-8, UTF-16, etc.).

`u32 encodeUnicode(FixedArray<char, 4>& buf, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams)`
> Encodes a Unicode codepoint into the specified encoding. Returns the number of bytes written to `buf`.

`DecodeResult decodeUnicode(StringView str, UnicodeType unicodeType, ExtendedTextParams* extParams = nullptr)`
> Decodes a Unicode codepoint from the beginning of `str`. Returns the codepoint and number of bytes consumed.

`bool encodeUnicode(Stream& out, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams = nullptr)`
> Encodes a Unicode codepoint and writes it to the stream.

`DecodeResult decodeUnicode(Stream& in, UnicodeType unicodeType, ExtendedTextParams* extParams = nullptr)`
> Decodes a Unicode codepoint from the stream.

## Convenience Functions

Character classification functions for common character types.

`bool isWhite(char c)`
> Returns `true` if `c` is a whitespace character (space, tab, newline, etc.).

`bool isAlpha(char c)`
> Returns `true` if `c` is an ASCII letter (a-z or A-Z).

`bool isDigit(char c)`
> Returns `true` if `c` is a decimal digit (0-9).
