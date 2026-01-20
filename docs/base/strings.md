{title text="Strings" include="ply-base.h" namespace="ply"}

Plywood strings are general-purpose blocks of memory allocated from [the heap](/docs/base/memory#heap). They're often used to [manipulate text](/docs/base/text), but they can store binary data as well. There are three main classes for working with strings:

* `String` owns a block of memory and frees it when the destructor is called.
* `StringView` is a read-only view into a block of memory.
* `MutStringView` is a read-write view into a block of memory.

These classes all use 32-bit integers for indexing, which means they can refereance a maximum of roughly 4 GB of memory.

Plywood strings aren't null-terminated unless you add an explicit null byte to the end. This is sometimes necessary when passing strings to legacy functions that expect a null terminator.

    String str = "Hello, world!";
    // The legacy function 'puts' expects a null terminator:
    puts((str + '\0').bytes());

These classes aren't thread-safe. Functions that read from the same string can be called concurrently from separate threads, but functions that modify the same string must not be called concurrently.

## Common String Methods

The following member functions are implemented in both `String` and `StringView`:

{api_summary title="`String` and `StringView` member functions"}
-- Accessing String Bytes
char* bytes()
const char* bytes() const
u32 num_bytes() const
char& operator[](u32 index)
const char& operator[](u32 index) const
char& back(s32 ofs = -1)
const char& back(s32 ofs = -1) const
-- Examining String Contents
bool is_empty() const
explicit operator bool() const
bool starts_with(StringView arg) const
bool ends_with(StringView arg) const
s32 find(StringView substr, u32 start_pos = 0) const
s32 find(const MatchFunc& match_func, u32 start_pos = 0) const
s32 reverse_find(StringView substr, u32 start_pos) const
s32 reverse_find(const MatchFunc& match_func, u32 start_pos) const
StringView substr(u32 start) const
StringView substr(u32 start, u32 num_bytes) const
StringView left(u32 num_bytes) const
StringView shortened_by(u32 num_bytes) const
StringView right(u32 num_bytes) const
StringView trim(bool (*match_func)(char) = is_whitespace, bool left = true, bool right = true) const
StringView trim_left(bool (*match_func)(char) = is_whitespace) const
StringView trim_right(bool (*match_func)(char) = is_whitespace) const
-- Creating New Strings
String upper() const
String lower() const
Array<StringView> split(StringView separator) const;
String join(ArrayView<const StringView> comps) const;
String operator+(StringView other);
-- Pattern Matching
template <typename... Args> bool match(StringView pattern, const Args&&... args)
{/api_summary}

### Accessing String Bytes

{api_descriptions class=String}
char& operator[](u32 index)
const char& operator[](u32 index) const
--
Subscript operator with runtime bounds checking. Can be used to read individual bytes or modify indvidual bytes (for `String` only). If `index` is out of range, an [assert](/docs/base/macros#assert) will be triggered.

    String str = "Hello, world!";
    char c = str[4];  // 'o'
    str[0] = 'J';     // "Jello, world!"

>>
char& back(s32 ofs = -1)
const char& back(s32 ofs = -1) const
--
Returns a reference to a byte relative to the end of the string. By default, returns the last byte. Pass `-2` for the second-to-last byte, and so on.
{/api_descriptions}

### Examining String Contents

{api_descriptions class=String}
bool is_empty() const
--
Returns `true` if the string contains no bytes.

>>
bool starts_with(StringView arg) const
--
Returns `true` if the string begins with the specified prefix.

>>
bool ends_with(StringView arg) const
--
Returns `true` if the string ends with the specified suffix.

>>
s32 find(StringView substr, u32 start_pos = 0) const
--
Searches for the first occurrence of `substr` starting from `start_pos`. Returns the index of the first match, or `-1` if not found.

>>
s32 find(const MatchFunc& match_func, u32 start_pos = 0) const
--
Searches for the first byte that satisfies the match function starting from `start_pos`. Returns the index of the first match, or `-1` if not found.

>>
s32 reverse_find(StringView substr, s32 start_pos = -1) const
--
Searches backwards for the last occurrence of `substr` starting from `start_pos` (or the end if `-1`). Returns the index of the match, or `-1` if not found.

>>
s32 reverse_find(const MatchFunc& match_func, s32 start_pos = -1) const
--
Searches backwards for the last byte that satisfies the match function. Returns the index of the match, or `-1` if not found.

>>
StringView substr(u32 start) const
--
Returns a view of the substring starting at `start` and extending to the end of the string.

>>
StringView substr(u32 start, u32 num_bytes) const
--
Returns a view of the substring starting at `start` with length `num_bytes`.

>>
StringView left(u32 num_bytes) const
--
Returns a view of the first `num_bytes` bytes of the string.

>>
StringView shortened_by(u32 num_bytes) const
--
Returns a view of the string with `num_bytes` removed from the end.

>>
StringView right(u32 num_bytes) const
--
Returns a view of the last `num_bytes` bytes of the string.

>>
StringView trim(bool (*match_func)(char) = is_whitespace, bool left = true, bool right = true) const
--
Returns a view with matching characters removed from both ends. By default, trims whitespace characters.

>>
StringView trim_left(bool (*match_func)(char) = is_whitespace) const
--
Returns a view with matching characters removed from the beginning. By default, trims whitespace.

>>
StringView trim_right(bool (*match_func)(char) = is_whitespace) const
--
Returns a view with matching characters removed from the end. By default, trims whitespace.
{/api_descriptions}

### Creating New Strings

{api_descriptions class=String}
String upper() const
--
Returns a new string with all ASCII lowercase letters converted to uppercase. Non-ASCII bytes are unchanged.

>>
String lower() const
--
Returns a new string with all ASCII uppercase letters converted to lowercase. Non-ASCII bytes are unchanged.

>>
Array<StringView> split(StringView separator) const;
--
Splits the string at each occurrence of `separator` and returns an array of views.

>>
String join(ArrayView<const StringView> comps) const;
--
Joins an array of string components using this string as the separator.

>>
String operator+(StringView other)
--
Returns a new string with the contents of this string followed by the contents of `other`.
{/api_descriptions}

### Pattern Matching

{api_descriptions}
template <typename... Args> bool match(StringView pattern, const Args&&... args)
--
Matches the string against a pattern containing `{}` placeholders. If the match succeeds, captured values are written to the output arguments.
{/api_descriptions}

## `String`

The `String` class owns a block of memory allocated from the [Plywood heap](/docs/base/memory#heap). The memory is freed when the `String` object is destroyed.

`String` objects are movable, copyable and construct to an empty string by default. In addition to the [common string functions](#common) listed in the previous section, they provide the following member functions:

{api_summary class=String}
-- Type Conversions
String(StringView other)
String(const char* s)
operator StringView() const
-- Modifying String Contents
void clear()
String& operator+=(StringView other)
void resize(u32 num_bytes)
char* release()
-- Creating New Strings
static String allocate(u32 num_bytes)
static String adopt(char* bytes, u32 num_bytes)
-- Formatting
static String format(StringView fmt, const Args&... args)
static String from_date_time(const DateTime& date_time);
{/api_summary}

### Type Conversions

{api_descriptions class=String}
String(StringView other)
--
Constructs a new string by copying the contents of a `StringView`.

>>
String(const char* s)
--
Constructs a string from a null-terminated C string. The characters are copied into a newly allocated memory block except for the null terminator.

>>
operator StringView() const
--
Implicitly converts the string to a `StringView`. This allows `String` objects to be passed directly to functions that expect `StringView` parameters.
{/api_descriptions}

### Modifying String Contents

{api_descriptions class=String}
void clear()
--
Frees the internal memory block and resets to an empty state.

>>
void operator+=(StringView other)
--
Appends the bytes from `other` to this string, reallocating if necessary.

>>
void resize(u32 num_bytes)
--
Resizes the string to the specified length, reallocating if necessary. New bytes are uninitialized.

>>
char* release()
--
Releases ownership of the internal memory block and returns a pointer to it. The caller is responsible for freeing the memory later using `Heap::free`.
{/api_descriptions}

### Creating New Strings

{api_descriptions class=String}
static String allocate(u32 num_bytes)
--
Allocates a new string of the specified size. The contents are uninitialized.

>>
static String adopt(char* bytes, u32 num_bytes)
--
Creates a `String` object that takes ownership of an existing buffer. The buffer must have been allocated from the Plywood heap and will be freed when the `String` is destroyed.
{/api_descriptions}

### Formatting

{api_descriptions class=String}
static String format(StringView fmt, const Args&... args)
--
Creates a formatted string using `{}` placeholders. Arguments are converted to text and substituted in order.

[TBD]

>>
static String from_date_time(const DateTime& date_time);
--
Creates a string representation of a `DateTime` object using a format string. See the [Time and Date](/docs/base/time-and-date) chapter for format specifiers.
{/api_descriptions}

## `StringView`

By convention, Plywood passes `StringView` to functions by value instead of by reference, since copying a `StringView` doesn't copy the underlying bytes.

[TBD]

## `MutStringView`

By convention, Plywood passes `MutStringView` objects to functions by value instead of by reference.

[TBD]
