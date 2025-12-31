{title text="Manipulating Text" include="ply-base.h" namespace="ply"}

Plywood provides utility functions for reading and writing text data.

## Reading Text

These functions parse common text patterns from input streams.

{api_summary}
String read_line(Stream& in)
StringView read_line(ViewStream& view_in)
String read_whitespace(Stream& in)
StringView read_whitespace(ViewStream& in)
void skip_whitespace(Stream& in)
String read_identifier(Stream& in, u32 flags = 0)
StringView read_identifier(ViewStream& view_in, u32 flags = 0)
u64 read_u64_from_text(Stream& in, u32 radix = 10)
s64 read_s64_from_text(Stream& in, u32 radix = 10)
double read_double_from_text(Stream& in, u32 radix = 10)
String read_quoted_string(Stream& in, u32 flags = 0, Functor<void(QS_Error_Code)> error_callback = {})
{/api_summary}

{api_descriptions}
String read_line(Stream& in)
StringView read_line(ViewStream& view_in)
--
Reads characters until a newline or end-of-file. The newline character is consumed but not included in the result.

>>
String read_whitespace(Stream& in)
StringView read_whitespace(ViewStream& in)
--
Reads and returns a sequence of whitespace characters.

>>
void skip_whitespace(Stream& in)
--
Consumes whitespace characters without returning them.

>>
String read_identifier(Stream& in, u32 flags = 0)
StringView read_identifier(ViewStream& view_in, u32 flags = 0)
--
Reads a C-style identifier (letters, digits, underscores, starting with a non-digit).

>>
u64 read_u64_from_text(Stream& in, u32 radix = 10)
--
Parses an unsigned integer from the stream. The `radix` parameter specifies the number base (e.g., 10 for decimal, 16 for hexadecimal).

>>
s64 read_s64_from_text(Stream& in, u32 radix = 10)
--
Parses a signed integer from the stream. Handles optional leading `-` sign.

>>
double read_double_from_text(Stream& in, u32 radix = 10)
--
Parses a floating-point number from the stream.

>>
String read_quoted_string(Stream& in, u32 flags = 0, Functor<void(QS_Error_Code)> error_callback = {})
--
Reads a quoted string, handling escape sequences. The opening quote must already be consumed. Calls `error_callback` if parsing fails.
{/api_descriptions}

## Writing Text

While `Stream::format` handles most formatting needs, these functions provide finer control over number formatting and string escaping.

{api_summary}
void print_number(Stream& out, u64 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, s64 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, u32 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, s32 value, u32 radix = 10, bool capitalize = false) 
void print_number(Stream& out, double value, u32 radix = 10, bool capitalize = false)
void print_escaped_string(Stream& out, StringView str)
void print_xml_escaped_string(Stream& out, StringView str)
{/api_summary}

{api_descriptions}
void print_number(Stream& out, u64 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, s64 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, u32 value, u32 radix = 10, bool capitalize = false)
void print_number(Stream& out, s32 value, u32 radix = 10, bool capitalize = false) 
void print_number(Stream& out, double value, u32 radix = 10, bool capitalize = false)
--
Writes a number to the stream. Use `radix` to specify the base (e.g., 16 for hexadecimal). Set `capitalize` to true for uppercase hex digits.

>>
void print_escaped_string(Stream& out, StringView str)
--
Writes a string with C-style escape sequences for special characters (e.g., `\n`, `\t`, `\\`).

>>
void print_xml_escaped_string(Stream& out, StringView str)
--
Writes a string with XML entity escaping (e.g., `&lt;`, `&gt;`, `&amp;`).
{/api_descriptions}

## Converting Unicode

These functions convert between Unicode codepoints and various encoded representations (UTF-8, UTF-16, etc.).

{api_summary}
u32 encode_unicode(FixedArray<char, 4>& buf, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params)
DecodeResult decode_unicode(StringView str, UnicodeType unicode_type, ExtendedTextParams* ext_params = nullptr)
bool encode_unicode(Stream& out, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params = nullptr)
DecodeResult decode_unicode(Stream& in, UnicodeType unicode_type, ExtendedTextParams* ext_params = nullptr)
{/api_summary}

{api_descriptions}
u32 encode_unicode(FixedArray<char, 4>& buf, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params)
--
Encodes a Unicode codepoint into the specified encoding. Returns the number of bytes written to `buf`.

>>
DecodeResult decode_unicode(StringView str, UnicodeType unicode_type, ExtendedTextParams* ext_params = nullptr)
--
Decodes a Unicode codepoint from the beginning of `str`. Returns the codepoint and number of bytes consumed.

>>
bool encode_unicode(Stream& out, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params = nullptr)
--
Encodes a Unicode codepoint and writes it to the stream.

>>
DecodeResult decode_unicode(Stream& in, UnicodeType unicode_type, ExtendedTextParams* ext_params = nullptr)
--
Decodes a Unicode codepoint from the stream.
{/api_descriptions}

## Convenience Functions

Character classification functions for common character types.

{api_summary}
bool is_whitespace(char c)
bool is_ascii_letter(char c)
bool is_decimal_digit(char c)
{/api_summary}

{api_descriptions}
bool is_whitespace(char c)
--
Returns `true` if `c` is a whitespace character (space, tab, newline, etc.).

>>
bool is_ascii_letter(char c)
--
Returns `true` if `c` is an ASCII letter (a-z or A-Z).

>>
bool is_decimal_digit(char c)
--
Returns `true` if `c` is a decimal digit (0-9).
{/api_descriptions}
