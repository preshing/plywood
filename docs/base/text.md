{title text="Manipulating Text" include="ply-base.h" namespace="ply"}

Plywood provides utility functions for reading and writing text data.

## Reading Text

These functions parse common text patterns from input streams.

{apiSummary}
String readLine(Stream& in)
StringView readLine(ViewStream& viewIn)
String readWhitespace(Stream& in)
StringView readWhitespace(ViewStream& in)
void skipWhitespace(Stream& in)
String readIdentifier(Stream& in, u32 flags = 0)
StringView readIdentifier(ViewStream& viewIn, u32 flags = 0)
u64 readU64FromText(Stream& in, u32 radix = 10)
s64 readS64FromText(Stream& in, u32 radix = 10)
double readDoubleFromText(Stream& in, u32 radix = 10)
String readQuotedString(Stream& in, QuotedStringType type = QuotedStringType::C, bool strict = true,
                        Functor<void(QuotedStringError)> errorCallback = {})
{/apiSummary}

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

{apiSummary}
struct NumberFormat
void printNumber(Stream& out, s32 value, const NumberFormat& format = {})
void printNumber(Stream& out, u32 value, const NumberFormat& format = {})
void printNumber(Stream& out, s64 value, const NumberFormat& format = {})
void printNumber(Stream& out, u64 value, const NumberFormat& format = {})
void printNumber(Stream& out, double value, const NumberFormat& format = {})
void printEscapedString(Stream& out, StringView str)
void printXmlEscapedString(Stream& out, StringView str)
{/apiSummary}

`void printNumber(Stream& out, s32 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, u32 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, s64 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, u64 value, const NumberFormat& format = {})`
`void printNumber(Stream& out, double value, const NumberFormat& format = {})`
> Writes a number to the given stream. `NumberFormat` is defined as follows:
> ```
> struct NumberFormat {
>     enum FloatMode {
>         Regular,
>         Scientific,
>         Auto,
>     };
>     enum SignMode {
>         ShowMinusOnly,
>         ShowPlus,
>         LeaveSpaceForSign,
>     };
> 
>     u32 radix = 10;
>     bool capitalize = false;
>     // If the number uses fewer than zeroPad digits, it will be padded with leading zeros:
>     u32 zeroPad = 0;
>     SignMode signMode = ShowMinusOnly;
>     // These members are only used when printing floating-point values:
>     FloatMode floatMode = Auto;
>     u32 fractionalPrecision = 3;
> };
> ```

`void printEscapedString(Stream& out, StringView str)`
> Writes a string with C-style escape sequences for special characters (e.g., `\n`, `\t`, `\\`).

`void printXmlEscapedString(Stream& out, StringView str)`
> Writes a string with XML entity escaping (e.g., `&lt;`, `&gt;`, `&amp;`).

## Converting Unicode

These functions convert between Unicode codepoints and various encoded representations (UTF-8, UTF-16, etc.).

{apiSummary}
u32 encodeUnicode(FixedArray<char, 4>& buf, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams)
DecodeResult decodeUnicode(StringView str, UnicodeType unicodeType, ExtendedTextParams* extParams = nullptr)
bool encodeUnicode(Stream& out, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams = nullptr)
DecodeResult decodeUnicode(Stream& in, UnicodeType unicodeType, ExtendedTextParams* extParams = nullptr)
{/apiSummary}

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

{apiSummary}
bool isWhite(char c)
bool isAlpha(char c)
bool isDigit(char c)
{/apiSummary}

`bool isWhite(char c)`
> Returns `true` if `c` is a whitespace character (space, tab, newline, etc.).

`bool isAlpha(char c)`
> Returns `true` if `c` is an ASCII letter (a-z or A-Z).

`bool isDigit(char c)`
> Returns `true` if `c` is a decimal digit (0-9).
