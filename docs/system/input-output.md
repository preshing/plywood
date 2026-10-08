Input and Output (`ply-system.h`)
=================================

Plywood exposes a three-layer API for input/output (I/O) operations:

1. **Low-level OS support**: [`Pipe`](#pipe) encapsulates files, sockets or conversion routines.
2. **Buffered access**: [`Stream`](#stream) is a wrapper around `Pipe` that exposes access to an internal memory buffer.
3. **High-level data serialization**: `Stream` also provides convenience functions to read and write common data formats to the internal memory buffer.

<svg viewBox="0 0 220 181" style="display:block;width:220px;max-width:100%;height:auto;margin-inline:auto">
 <g fill="none" stroke="var(--diagram-arrow-color)" stroke-width="1">
  <path d="m117.3 114.8-4.8 6.4-4.8-6.4"/>
  <path d="M112.5 121.9V101"/>
  <path d="m117.3 60.8-4.8 6.4-4.8-6.4"/>
  <path d="M112.5 67.9V51"/>
 </g>
 <g fill="none" stroke="var(--border-table)" stroke-width="2">
  <rect x="4" y="3" width="214" height="98" ry="15"/>
  <rect x="24" y="123" width="174" height="56" ry="8.6"/>
 </g>
 <g fill="var(--diagram-solid-fill)" stroke="var(--border-popup)" stroke-width="1">
  <rect x="9.5" y="26.5" width="203" height="24" ry="5.9"/>
  <rect x="53.5" y="68.5" width="115" height="24" ry="5.9"/>
  <rect x="32.5" y="146.5" width="157" height="24" ry="5.9"/>
 </g>
 <g fill="var(--text-code)" font-family="jetbrains_mono,monospace" font-size="14px" text-anchor="middle">
  <text x="111" y="18.4">Stream</text>
  <text x="111" y="138.4">Pipe</text>
 </g>
 <g fill="var(--text-secondary)" font-family="source_sans_3,sans-serif" font-size="16px" text-anchor="middle">
  <text x="110.4" y="42.6">High-level data serialization</text>
  <text x="110.5" y="84.6">Buffered access</text>
  <text x="110.4" y="162.6">Low-level OS support</text>
 </g>
</svg>

Most applications only really need to use `Stream`, but `Pipe` can also be extended to perform application-specific data conversion.

## `Pipe`

`Pipe` is a low-level abstract base class that encapsulates a file, a network socket or a conversion routine. Applications rarely use this class directly; [`Stream`](#stream) provides a more convenient interface for data serialization.

Reading or writing data through a `Pipe` usually involves a system call into the underlying OS, which is expensive. To transfer data at faster rates, it's better to pass large chunks of data (in the ~2-64 KB range) to each `read` or `write` call.

`read` is a potentially blocking call. `tryRead` is designed not to block, so it can be called from within a main loop without causing any frame spikes. `write` may also block until the underlying data destination is ready to receive. Not all member functions are supported by every `Pipe` instance. The caller is responsible for only calling functions that are supported by each instance. Any underlying resources used by the `Pipe`, including file handles, are released when the `Pipe` is destroyed.

Only one thread may call `read`, `tryRead`, `write`, `flush` or `seekTo` at a time on a particular `Pipe` instance. `disable`, however, is safe to call from any thread at any time. Once a pipe has been disabled, it stops receiving data from the underlying connection, `read` and `write` become non-blocking, and any currently blocked threads are released.

`u64 Pipe::getFileSize()`
> Returns the total size of the underlying file.

`u32 Pipe::getPermissions() const`
> Returns the pipe's permissions.
>
> | Flag | Meaning |
> | ---- | ------- |
> | `Pipe::CanRead` | The pipe supports `read` and `tryRead`. |
> | `Pipe::CanWrite` | The pipe supports `write` and `flush`. |
> | `Pipe::CanSeek` | The pipe supports `seekTo` and `getFileSize`. |

`u32 Pipe::read(MutStringView buf)`
> Reads up to `buf.numBytes` bytes into `buf`. Returns the number of bytes actually read.
> The calling thread will block until at least one byte is successfully read from the underlying data source.
> Returns 0 when no more data can be read because of end-of-file, the pipe being disabled, or an I/O error.
> `buf` must not be empty.

`Pipe::ReadResult Pipe::tryRead(MutStringView buf)`
> Reads as much data as possible without blocking. `buf` must not be empty. `Pipe::ReadResult` has the following data members:
>
> | Member | Description |
> | ---- | ------- |
> | `Kind kind` | Outcome of the read. |
> | `u32 numBytes` | Number of bytes written to `buf`. Only valid when `kind` is `Data`. |
>
> `Pipe::ReadResult::Kind` values:
>
> | Value | Meaning |
> | ---- | ------- |
> | `Data` | `numBytes` bytes were written to the destination buffer. |
> | `EndOfFile` | The source is exhausted; no more data will arrive. |
> | `WouldBlock` | No data is available right now, but more may arrive later. |
> | `Error` | The read failed due to an I/O error. |
>
> Every value other than `Data` means that no bytes were written to `buf`.

`bool Pipe::write(StringView buf)`
> Writes the bytes in `buf`. Returns `true` if all bytes were written. Can return `false` if the pipe is disabled or the peer connection was closed. This function may block until the pipe's destination is ready to receive.

`void Pipe::seekTo(s64 offset)`
> Seeks to the specified byte offset. Only supported by seekable pipes.

`void Pipe::flush(bool toDevice = false)`
> Flushes any buffered writes. If `toDevice` is true, ensures data reaches the physical device.

`void Pipe::disable()`
> Disables communication on this pipe. It's safe to call this from any thread at any time.
> If another thread is blocked inside `read` or `write`, `disable` unblocks it.
> After `disable` returns, readers will encounter end-of-file quickly, though a small amount
> of trailing data may remain available to read.

`String readAllRemainingData(Pipe* inPipe)`
> Reads and returns all remaining data from `inPipe` through end-of-file. This function may block until the pipe's writer closes. It does not close the pipe.

## `Stream`

A `Stream` wraps a `Pipe` and provides buffered I/O operations. Streams handle the details of buffering data for efficient reads and writes.

### Buffered Access

`Stream` exposes the following public data members.

| Member | Description |
| --- | --- |
| `char* curByte` | Current position in the active buffer. After `makeReadable()` succeeds, read bytes beginning here and advance `curByte` by the number consumed. After `makeWritable()` succeeds, write bytes beginning here and advance `curByte` by the number produced. Never advance it past `endByte`. |
| `char* endByte` | One past the end of the active readable or writable range. Callers may compare this with `curByte`, but should not change it. A call that refills, flushes, seeks, closes or otherwise changes the stream may invalidate the current buffer range. |
| `bool atEof` | Set when the stream has observed end-of-file. Buffered bytes may still remain when this is true, so consume the range through `endByte` before treating the input as exhausted. Callers should inspect this value but should not modify it. |
| `bool inputError` | Set when an input operation reports an error. This lets callers distinguish an error from would-block or ordinary end-of-file after `tryMakeReadable()` returns `false`. Callers should inspect this value but should not modify it. |

For direct reading, call `makeReadable()` or `tryMakeReadable()` before using the range. For direct
writing, call `makeWritable()` first.

`Stream::Stream()`
> Constructs an empty stream that is not connected to any pipe.

`Stream::Stream(Pipe* pipe, bool isPipeOwner)`
> Constructs a stream from a pipe. If `isPipeOwner` is true, the stream will destroy the pipe when closed.

`Stream::Stream(Pipe* pipe, bool isPipeOwner, u32 permissions)`
> Constructs a stream with a restricted subset of the pipe's capabilities.

`Stream::Stream(Stream&& other)`
> Move constructor.

`Stream& Stream::operator=(Stream&& other)`
> Move assignment.

`bool Stream::isOpen()`
> Returns `true` if the stream is connected to a valid pipe.

`explicit Stream::operator bool()`
> Same as `isOpen()`.

`u32 Stream::getPermissions() const`
> Returns the stream's permissions.
>
> | Flag | Meaning |
> | ---- | ------- |
> | `Stream::CanRead` | The stream supports reading. |
> | `Stream::CanWrite` | The stream supports writing. |
> | `Stream::CanSeek` | The stream supports `seekTo`. |

`Pipe* Stream::getPipe() const`
> Returns the underlying pipe for a pipe-backed stream, or `nullptr` for other stream types.

`void Stream::disablePipe()`
> Disables the underlying pipe, which can release operations blocked on that pipe. It's also legal to
> call this function on a non-pipe stream, in which case it has no effect.

`void Stream::close()`
> Flushes and closes the stream. If the stream owns the pipe, the pipe is destroyed.

`bool Stream::makeReadable(u32 minBytes = 1)`
> This function blocks until `minBytes` are available, in which case it returns `true`.
> If end-of-file is reached before `minBytes` is available, returns `false`. `minBytes` must be greater than 0.

`bool Stream::tryMakeReadable()`
> This function makes as much data as possible readable without blocking.
> Returns `true` if one or more bytes of data are made readable.
> A `false` result can mean that no data is immediately available, end-of-file was reached or an
> I/O error occurred. Inspect `atEof` and `inputError` to distinguish those cases.

`bool Stream::makeWritable(u32 minBytes = 1)`
> Ensures at least `minBytes` of space are available in the write buffer.
> May block until the underlying destination is ready to receive.

`bool Stream::hasRemainingBytes() bool`
> Returns `true` if there are any bytes remaining in the read buffer.

`u32 Stream::numRemainingBytes() const`
> Returns the number of bytes currently available in the read buffer.

`StringView Stream::viewRemainingBytes() const`
> Returns a view of the bytes currently in the read buffer.

`MutStringView Stream::viewRemainingBytesMut()`
> Returns a mutable view of the read buffer.

`void Stream::flush(bool toDevice = false)`
> Writes any buffered data to the underlying pipe. If `toDevice` is true, also flushes the pipe to the physical device.

`u64 Stream::getSeekPos()`
> Returns the current seek position in the stream.

`void Stream::seekTo(u64 seekPos)`
> Seeks to the specified position. Only works with seekable pipes.

### High-Level Data Serialization

`char Stream::peekByte()`
> Returns the next byte in the input stream, or `0` if at end-of-file.

`char Stream::readByte()`
> Reads and returns a single byte.

`u32 Stream::read(MutStringView dst)`
> Reads up to `dst.numBytes` bytes into `dst`. Returns the number of bytes actually read.

`u32 Stream::tryRead(MutStringView dst)`
> Reads as much data as possible without blocking, up to `dst.numBytes` bytes. Returns the number
> of bytes actually read and leaves the rest of `dst` unchanged. A return value of 0 can mean that
> no data is immediately available, end-of-file was reached or an I/O error occurred. Inspect
> `atEof` and `inputError` to distinguish those cases.

`u32 Stream::skip(u32 numBytes)`
> Skips up to `numBytes` in the input. Returns the number of bytes actually skipped.

`bool Stream::write(char c)`
> Writes a single byte.

`u32 Stream::write(StringView bytes)`
> Writes the given bytes to the stream. Returns the number of bytes written.

`void Stream::format(StringView fmt, const Args&... args)`
> Writes formatted text using `{}` placeholders.

## `MemStream`

A `MemStream` writes to an in-memory buffer that grows as needed. This is useful for building strings or serializing data.

Copy construction and copy assignment create independent deep copies with the same contents and seek position.

`String MemStream::copyToString() const`
> Copies the complete contents to a string without changing the stream.

`String MemStream::moveToString()`
> Returns the complete contents and closes the stream. Transfers its storage directly when possible.

## `ViewStream`

A `ViewStream` reads from a fixed memory buffer (a `StringView`). This is useful for parsing strings or data already in memory.

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

`u64 readU64FromText(Stream& in, u32 radix = 10)`
> Parses an unsigned integer from the stream. The `radix` parameter specifies the number base (e.g., 10 for decimal, 16 for hexadecimal).

`s64 readS64FromText(Stream& in, u32 radix = 10)`
> Parses a signed integer from the stream. Handles optional leading `-` sign.

`double readDoubleFromText(Stream& in, u32 radix = 10)`
> Parses a floating-point number from the stream.

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
