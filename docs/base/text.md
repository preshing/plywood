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

While `Stream::format` handles most formatting needs, these functions provide finer control over number formatting and string escaping.

{apiSummary}
void printNumber(Stream& out, u64 value, u32 radix = 10, bool capitalize = false)
void printNumber(Stream& out, s64 value, u32 radix = 10, bool capitalize = false)
void printNumber(Stream& out, u32 value, u32 radix = 10, bool capitalize = false)
void printNumber(Stream& out, s32 value, u32 radix = 10, bool capitalize = false) 
void printNumber(Stream& out, double value, u32 radix = 10, bool capitalize = false)
void printEscapedString(Stream& out, StringView str)
void printXmlEscapedString(Stream& out, StringView str)
{/apiSummary}

`void printNumber(Stream& out, u64 value, u32 radix = 10, bool capitalize = false)`
`void printNumber(Stream& out, s64 value, u32 radix = 10, bool capitalize = false)`
`void printNumber(Stream& out, u32 value, u32 radix = 10, bool capitalize = false)`
`void printNumber(Stream& out, s32 value, u32 radix = 10, bool capitalize = false)`
`void printNumber(Stream& out, double value, u32 radix = 10, bool capitalize = false)`
> Writes a number to the stream. Use `radix` to specify the base (e.g., 16 for hexadecimal). Set `capitalize` to true for uppercase hex digits.

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
bool isWhitespace(char c)
bool isAsciiLetter(char c)
bool isDecimalDigit(char c)
{/apiSummary}

`bool isWhitespace(char c)`
> Returns `true` if `c` is a whitespace character (space, tab, newline, etc.).

`bool isAsciiLetter(char c)`
> Returns `true` if `c` is an ASCII letter (a-z or A-Z).

`bool isDecimalDigit(char c)`
> Returns `true` if `c` is a decimal digit (0-9).
