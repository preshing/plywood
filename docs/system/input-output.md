Input/Output (`ply-system.h`)
=============================

Plywood provides a composable I/O system built around `Stream` and `Pipe` classes. Streams provide buffered, high-level read/write operations, while pipes represent the underlying I/O mechanisms (files, sockets, memory buffers). You can also derive from `Pipe` to create adapters that perform encryption or compression.

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

## `Stream`

A `Stream` wraps a `Pipe` and provides buffered I/O operations. Streams handle the details of buffering data for efficient reads and writes.

{context class=Stream}

`Stream()`
> Constructs an empty stream that is not connected to any pipe.

`Stream(Pipe* pipe, bool isPipeOwner)`
> Constructs a stream from a pipe. If `isPipeOwner` is true, the stream will destroy the pipe when closed.

`Stream(Stream&& other)`
> Move constructor.

`Stream& operator=(Stream&& other)`
> Move assignment.

`bool isOpen()`
> Returns `true` if the stream is connected to a valid pipe.

`explicit operator bool()`
> Same as `isOpen()`.

`void close()`
> Flushes and closes the stream. If the stream owns the pipe, the pipe is destroyed.

`bool makeReadable(u32 minBytes = 1)`
> Ensures at least `minBytes` are available in the read buffer. Returns `false` if end-of-file is reached.

`bool makeWritable(u32 minBytes = 1)`
> Ensures at least `minBytes` of space are available in the write buffer.

`bool hasRemainingBytes() bool`
> Returns `true` if there are any bytes remaining in the read buffer.

`u32 numRemainingBytes() const`
> Returns the number of bytes currently available in the read buffer.

`StringView viewRemainingBytes() const`
> Returns a view of the bytes currently in the read buffer.

`MutStringView viewRemainingBytesMut()`
> Returns a mutable view of the read buffer.

`void flush(bool toDevice = false)`
> Writes any buffered data to the underlying pipe. If `toDevice` is true, also flushes the pipe to the physical device.

`char peekByte()`
> Returns the next byte in the input stream, or `0` if at end-of-file.

`char readByte()`
> Reads and returns a single byte.

`u32 read(MutStringView dst)`
> Reads up to `dst.numBytes` bytes into `dst`. Returns the number of bytes actually read.

`u32 skip(u32 numBytes)`
> Skips up to `numBytes` in the input. Returns the number of bytes actually skipped.

`bool write(char c)`
> Writes a single byte.

`u32 write(StringView bytes)`
> Writes the given bytes to the stream. Returns the number of bytes written.

`void format(StringView fmt, const Args&... args)`
> Writes formatted text using `{}` placeholders.

`u64 getSeekPos()`
> Returns the current seek position in the stream.

`void seekTo(u64 seekPos)`
> Seeks to the specified position. Only works with seekable pipes.

Remember to write `'\n'` for newlines. There's no `endl` like C++ iostreams—use `flush` to force output.

## `MemStream`

A `MemStream` writes to an in-memory buffer that grows as needed. This is useful for building strings or serializing data.

`MemStream duplicate() const`
> Creates an independent copy with the same contents, access mode and seek position.

## `ViewStream`

A `ViewStream` reads from a fixed memory buffer (a `StringView`). This is useful for parsing strings or data already in memory.

## `Pipe`

`Pipe` is the abstract base class for all I/O providers. Concrete implementations include file pipes, socket pipes, and memory pipes. You rarely need to work with pipes directly—use `Stream` instead.

{context class=Pipe}

`virtual ~Pipe()`
> Virtual destructor for proper cleanup of derived classes.

`virtual u32 read(MutStringView buf)`
> Reads up to `buf.numBytes` bytes into `buf`. Returns the number of bytes actually read. Returns 0 at end-of-file.

`virtual bool write(StringView buf)`
> Writes the bytes in `buf`. Returns `true` on success.

`virtual void flush(bool toDevice = false)`
> Flushes any buffered writes. If `toDevice` is true, ensures data reaches the physical device.

`virtual u64 getFileSize()`
> Returns the total size of the underlying file, or 0 if not applicable.

`virtual void seekTo(s64 offset)`
> Seeks to the specified byte offset. Only supported by seekable pipes.

`u32 getFlags() const`
> Returns the pipe's capability flags (readable, writable, seekable, etc.).

Plywood provides utility functions for reading and writing text data.

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
