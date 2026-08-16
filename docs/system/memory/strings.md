Strings (`ply-system.h`)
========================

Plywood strings are general-purpose blocks of memory allocated from [the heap](/docs/system/memory/heap.md). They're
often used to store UTF-8 text, but they can also store any other type of binary data.

There are three main classes for working with strings:

* [`String`](#string) owns a heap-allocated memory block and frees it when the destructor is called.
* [`StringView`](#string-view) is a read-only view into an existing block of memory.
* [`MutStringView`](#mut-string-view) is a read-write view into an existing block of memory.

Plywood strings aren't null-terminated unless you add an explicit null byte to the end.

```
String str = "Hello, world!";
// Append a null terminator to the string before calling puts:
puts((str + '\0').bytes());
```

These classes aren't thread-safe. Concurrent reads are OK, but functions that modify the same string must not be called concurrently.

## Common Methods

The following member functions are available on both `String` and `StringView` instances.

### Accessing String Bytes

{context class=String}

`char& operator[](u32 index)`
`const char& operator[](u32 index) const`
> Subscript operator with runtime bounds checking. Can be used to read individual bytes or modify indvidual bytes (for `String` only). If `index` is out of range, an [assert](/docs/system/preprocessor-macros.md#assert) will be triggered.
> ```
> String str = "Hello, world!";
> char c = str[4];  // 'o'
> str[0] = 'J';     // "Jello, world!"
> ```

`char& back(s32 ofs = -1)`
`const char& back(s32 ofs = -1) const`
> Returns a reference to a byte relative to the end of the string. By default, returns the last byte. Pass `-2` for the second-to-last byte, and so on.

`char* begin()`
`const char* begin() const`
> Returns a pointer to the first byte of the string. Suitable for range-for iteration.

`char* end()`
`const char* end() const`
> Returns a pointer to one past the last byte of the string. Suitable for range-for iteration.
> ```
> String str = "Hello";
> for (char c : str) {
>     // Iterates over 'H', 'e', 'l', 'l', 'o'.
> }
> ```

### Examining String Contents

`bool isEmpty() const`
> Returns `true` if the string contains no bytes.

`bool startsWith(StringView arg) const`
> Returns `true` if the string begins with the specified prefix.

`bool endsWith(StringView arg) const`
> Returns `true` if the string ends with the specified suffix.

`s32 find(StringView substr, u32 startPos = 0) const`
> Searches for the first occurrence of `substr` starting from `startPos`. Returns the index of the first match, or `-1` if not found.

`s32 find(const MatchFunc& matchFunc, u32 startPos = 0) const`
> Searches for the first byte that satisfies the match function starting from `startPos`. Returns the index of the first match, or `-1` if not found.

`s32 reverseFind(StringView substr, s32 startPos = -1) const`
> Searches backwards for the last occurrence of `substr` starting from `startPos` (or the end if `-1`). Returns the index of the match, or `-1` if not found.

`s32 reverseFind(const MatchFunc& matchFunc, s32 startPos = -1) const`
> Searches backwards for the last byte that satisfies the match function. Returns the index of the match, or `-1` if not found.

### Creating Subviews

`StringView substr(u32 start) const`
> Returns a view of the substring starting at `start` and extending to the end of the string.

`StringView substr(u32 start, u32 numBytes) const`
> Returns a view of the substring starting at `start` with length `numBytes`.

`StringView left(u32 numBytes) const`
> Returns a view of the first `numBytes` bytes of the string.

`StringView shortenedBy(u32 numBytes) const`
> Returns a view of the string with `numBytes` removed from the end.

`StringView right(u32 numBytes) const`
> Returns a view of the last `numBytes` bytes of the string.

`StringView trim(bool (*matchFunc)(char) = isWhite, bool left = true, bool right = true) const`
> Returns a view with matching characters removed from both ends. By default, trims whitespace characters.

`StringView trimLeft(bool (*matchFunc)(char) = isWhite) const`
> Returns a view with matching characters removed from the beginning. By default, trims whitespace.

`StringView trimRight(bool (*matchFunc)(char) = isWhite) const`
> Returns a view with matching characters removed from the end. By default, trims whitespace.

### Creating New Strings

`String upper() const`
> Returns a new string with all ASCII lowercase letters converted to uppercase. Non-ASCII bytes are unchanged.

`String lower() const`
> Returns a new string with all ASCII uppercase letters converted to lowercase. Non-ASCII bytes are unchanged.

`Array<StringView> split(StringView separator) const;`
> Splits the string at each occurrence of `separator` and returns an array of views.

`String join(ArrayView<const StringView> comps) const;`
> Joins an array of string components using this string as the separator.

`String operator+(StringView other)`
> Returns a new string with the contents of this string followed by the contents of `other`.

### Pattern Matching

`template <typename... Args> bool match(StringView pattern, const Args&&... args)`
> Matches the string against a pattern containing `{}` placeholders. If the match succeeds, captured values are written to the output arguments.

## `String`

The `String` class owns a block of memory allocated from the [Plywood heap](/docs/system/memory/heap.md). The memory is freed when the `String` object is destroyed.

`String` objects are movable, copyable and construct to an empty string by default. In addition to the [common string functions](#common) listed in the previous section, they provide the following member functions:

### Type Conversions

`String(StringView other)`
> Constructs a new string by copying the contents of a `StringView`.

`String(const char* s)`
> Constructs a string from a null-terminated C string. The characters are copied into a newly allocated memory block except for the null terminator.

`operator StringView() const`
> Implicitly converts the string to a `StringView`. This allows `String` objects to be passed directly to functions that expect `StringView` parameters.

`MutStringView mutStringView() const`
> Returns a `MutStringView` that refers to the string's internal memory block.

### Modifying String Contents

`void clear()`
> Frees the internal memory block and resets to an empty state.

`void operator+=(StringView other)`
> Appends the bytes from `other` to this string, reallocating if necessary.

`void resize(u32 numBytes)`
> Resizes the string to the specified length, reallocating if necessary. New bytes are uninitialized.

`char* release()`
> Releases ownership of the internal memory block and returns a pointer to it. The caller is responsible for freeing the memory later using `Heap::free`.

### Creating New Strings

`static String allocate(u32 numBytes)`
> Allocates a new string of the specified size. The contents are uninitialized.

`static String adopt(char* bytes, u32 numBytes)`
> Creates a `String` object that takes ownership of an existing buffer. The buffer must have been allocated from the Plywood heap and will be freed when the `String` is destroyed.

### Formatting

`static String format(StringView fmt, const Args&... args)`
> Creates a formatted string using `{}` placeholders. Arguments are converted to text and substituted in order.
>
> Braces can be escaped as `{{` and `}}`. A format specifier can be placed after `:` inside the placeholder:
>
> ```cpp
> String::format("{:>8}", "name");     // "    name"
> String::format("{:_^8}", "name");    // "__name__"
> String::format("{:+d}", 42);         // "+42"
> String::format("{:08x}", 255);       // "000000ff"
> String::format("{:.2f}", 12.345);    // "12.35"
> String::format("{:.4s}", "abcdef");  // "abcd"
> String::format("{:&}", "<tag>");      // "&lt;tag&gt;"
> ```
>
> Supported specifiers are a small subset of `std::format` with an additional type `&` for XML-escaped string output:
>
> - Width: decimal byte count, for example `{:8}`.
> - Precision: decimal byte count for strings or decimal places for floating-point values, for example `{:.4s}` or
>   `{:.2f}`.
> - Type: `s` for strings, `&` for XML-escaped strings, `b`/`B`, `o`, `d`, `x`/`X` for integers, and `f`/`F`,
>   `e`/`E` for floating-point values.
> - Sign: `+`, `-` or space for numeric values.
> - Fill and align: `<`, `>` or `^`, optionally preceded by a one-byte fill character, for example `{:*>8}`.

`static String fromDateTime(const DateTime& dateTime);`
> Creates a string representation of a `DateTime` object using a format string. See the [Time and Date](/docs/system/time-and-date.md) chapter for format specifiers.

## `StringView`

By convention, Plywood passes `StringView` to functions by value instead of by reference, since copying a `StringView` doesn't copy the underlying bytes.

[TBD]

## `MutStringView`

By convention, Plywood passes `MutStringView` objects to functions by value instead of by reference.

[TBD]
