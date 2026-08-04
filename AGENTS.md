# Project Overview

Plywood is a low-level C++ runtime library for building cross-platform native software.
It provides a simple, portable C++ API over OS features and commonly-used data structures and algorithms.
Additional components for 2D/3D math, networking and text parsing are also included.

**Supported Platforms**: Windows, macOS, iOS, Linux and Android. WebAssembly is planned.

**Directory Structure**:
- `src/` - Contains pairs of `.h`/`.cpp` files organized by feature category.
  Projects can compile and link with only the features they need.
    - `ply-base.h/cpp` - Core: OS access, data structures, string formatting, Unicode conversion, threading, memory
    - `ply-math.h/cpp` - Vectors, matrices, quaternions for graphics and layout
    - `ply-network.h/cpp` - TCP/IP networking (IPv4/IPv6)
    - `ply-json.h/cpp` - JSON parser/serializer
    - `ply-tokenizer.h/cpp` - Text tokenization utilities
    - `ply-markdown.h/cpp` - Markdown parser with HTML output
    - `ply-cpp.h/cpp` - Experimental C++ parser
- `apps/` - Sample applications. Each subdirectory contains a `CMakeLists.txt` file to build the app.
    - `test-suite` - Combined base, Unicode loading, Markdown, C++ parser and heap fragmentation test suites
    - `banner-comment` - Converts its argument to a banner-style comment using Unicode block characters
    - `generate-docs` - Converts the documentation to HTML files written to `docs/build/`
    - `serve-docs` - Runs an HTTP server to serve the generated documentation on port 8080
- `docs/` - Project documentation in Markdown format
    - `contents.json` - Table of contents

## Building and Running Sample Apps

Each subdirectory in `apps/` is a standalone CMake project. All apps write their executables to the
repository's `bin/` subdirectory. These apps require ongoing maintenance, so prefer Debug builds when doing
general development. On Linux and macOS, CMake will generate a Debug-configured makefile by default:
```
$ cmake -B apps/<sample-name>/build apps/<sample-name>
$ cmake --build apps/<sample-name>/build
$ bin/<sample-name>
```

On Windows, CMake will generate a multi-configuration Visual Studio solution by default:
```
> cmake -B apps\<sample-name>\build apps\<sample-name>
> cmake --build apps\<sample-name>\build --config=Debug
> bin\<sample-name>.exe
```

## Coding Conventions

(**Note**: These coding conventions are not yet universally enforced across the entire project.
Your assistance in updating existing code to become more compliant would be appreciated.
Feel free to improve any existing code that you end up touching!)

- Minimize use of the C/C++ Standard Library; prefer using the Plywood base API instead.
- C++14 feature limit.
- Follow the same coding style as existing code in the `src` folder.
    - Types use PascalCase; functions and variables use camelCase.
    - 120 character line limit, 4-space indentation, Attach-style braces.
- The Plywood public API and all its implementation details are defined inside the `ply` namespace.
    - For convenient name lookups, outer scopes can import names directly using `using namespace ply`.
- Organize all functions and type defintions into logical categories that layer in a natural way.
- Use `banner-comment` to generate banners that identify top-level categories in the source code. eg.
    //  ▄▄▄▄▄
    //  ██  ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
    //  ██▀▀█▄  ▄▄▄██ ██  ██ ██  ██ ██▄▄██ ██  ▀▀
    //  ██▄▄█▀ ▀█▄▄██ ██  ██ ██  ██ ▀█▄▄▄  ██
    //
- Use smaller banners to identify sub-categories in the source code.
    //--------------------------------------------------------------------
    // Smaller banner
    //--------------------------------------------------------------------
- Immediately before each function or type, provide a brief comment to summarize what that function or type is for.
- For each of these categories and subcategories, there should up-to-date documentation that is easily found in the Markdown files located in the `docs` folder.
- Leave a brief comment before each significant block of code in a function body to describe the role it plays within the enclosing code section.
- Avoid adding too many small helper functions. Prefer to use direct C++ expressions when the meaning of those expression is clear from the surrounding comments.
- When the body of an `if` statement consists of exactly one `continue`, `return`, or `break` statement, omit curly braces.

## Overview of `ply-base.h`

The `src/ply-base.h` defines the public API of Plywood's base library in a single ~4500-line C++ header file.
All C++ code in the Plywood project should use this API while avoiding the Standard C/C++ Library API.

**Macros** (partial list): 
- `PLY_ASSERT` - Runtime assertions when PLY_WITH_ASSERTS is defined
- `PLY_STATIC_ASSERT`
- `PLY_STRINGIFY`
- `PLY_CAT`
- `PLY_UNIQUE_VARIABLE`
- `PLY_PTR_OFFSET`
- `PLY_OFFSET_OF`
- `PLY_STATIC_ARRAY_SIZE`
- `PLY_UNUSED`
- `PLY_CALL_MEMBER`
- `PLY_ON_SCOPE_EXIT` - Deferred execution
- `PLY_SET_IN_SCOPE` - Automatically restores previous value
- Platform identification: `PLY_WINDOWS PLY_LINUX PLY_ANDROID PLY_APPLE PLY_MACOS PLY_IOS PLY_POSIX PLY_MINGW`
- `PLY_PTR_SIZE` (in bytes)

**Integer types**: `s8 s16 s32 s64 u8 u16 u32 u64 sptr uptr`

**Numeric helpers**:
- `getMinValue<T>()`/`getMaxValue<T>` - Numeric limits
- `abs min max clamp` - Function templates
- Alignment functions end with suffix `PowerOf2`
- `numericCast` - Asserts if conversion doesn't fit

**Time & date**:
- `struct DateTime`
- `getUnixTimestamp`
- `convertToDateTime`
- `convertToUnixTimestamp`

**CPU profiling**:
- `getCpuTicks`
- `getCpuTicksPerSecond`

**`Heap`**: Memory allocator
- Call `Heap::alloc/realloc/free/allocAligned` instead of `malloc/free`
- `Heap::create/destroy` are direct wrappers that invoke constructors/destructors
- `operator new/delete` are globally overridden to use `Heap` unless `PLY_OVERRIDE_NEW=0` is defined

**String classes**:
- `String` owns memory
- `StringView` is a non-owning, read-only memory view
- `MutStringView` is a non-owning, mutable memory view
- `String` and `const char*` can be implicitly converted to `StringView`
- These classes typically hold UTF-8 text, but can also store binary data
- Strings aren't null-terminated unless done explicitly, eg. `(str + '\0').bytes()`
- Member functions common to `String` and `StringView`:
    - Accessing string bytes: `bytes/numBytes/[]/back/begin/end`; `[]` performs bounds checking
    - Examining contents: `isEmpty/operator bool/startsWith/endsWith/find/reverseFind`
    - Creating subviews: `substr/left/shortenedBy/right/trim/trimLeft/trimRight`
    - Creating new strings: `upper/lower/split/join/operator+`
    - Pattern matching: Use `match` instead of `sscanf` when possible
- `String`-specific member functions:
    - Modifying contents: `clear/+=/resize/release`
    - Creating new strings: `allocate/adopt`
    - Formatting: Use `String::format` instead of `sprintf`

**I/O streams and pipes**:
- `Pipe` - Abstract wrapper over file descriptors, sockets, compression algorithms or Unicode converters; always heap-allocated
- `Stream` - Buffered input/output over `Pipe`; can be created on the stack; gets flushed in the destructor
- `MemStream` - Specialization of `Stream` that uses a dynamic memory buffer
- `ViewStream` - Read-only `Stream` specialization that reads from a `StringView`
- `getStdIn getStdOut getStdErr` - Returns temporary `Stream`s over standard handles

**Threads and processes**:
- `getCurrentThreadId getCurrentProcessId getCurrentExecutablePath`
- `Thread` - Spawn and join threads
- `Atomic<T>` - 32 and 64-bit atomic types with relaxed, acquire and release semantics
- `ThreadLocal<T>` - Shared library-compatible thread local storage
- `Mutex`
- `ConditionVariable`
- `ReadWriteLock`
- `Semaphore`
- `Subprocess` - Spawn and join subprocesses with redirected I/O streams

**Other**:
- `Random` - Random number generation
- `VirtualMemory` - Reserve/commit pages in virtual address space
- `Array<Item>` - Dynamic array with bounds checking, owns its contents
- `FixedArray<Item, NumItems>` - Fixed-size array with bounds checking, owns its contents
- `ArrayView<Item>` - Non-owning array view
- Array template and C-style arrays can be implicitly converted to `ArrayView`
- `Set<Item>` - Associative map of items with hashable key
- `Map<Key, Value>` - Associative map of key/value pairs with hashable key
- `Owned<T>` - Owning pointer
- `Reference<T>` - Reference-counting pointer
- `RefCounted<Subclass>` - Mixin class
- `Functor<Return(Args...)>` - Can store and invoke callback functions or lambda expressions
- `Variant<Types...>` - Can hold one of several predefined types, like a type-checked tagged union
- Generic algorithms: `find reverseFind sort binarySearch`
- Reading text: `readLine readWhitespace skipWhitespace readIdentifier readU64FromText readS64FromText readDoubleFromText readQuotedString`
- Writing text: `printNumber printEscapedString printXmlEscapedString`
- Unicode conversion: `encodeUnicode decodeUnicode`
- `Filesystem` - Filesystem operations, opening files, text format detection
- Path manipulation: `getPathSeparator getDriveLetter isAbsolutePath isRelativePath makeAbsolutePath makeRelativePath splitPath splitFileExtension splitPathFull joinPath`
- `DirectoryWatcher` - Watches a directory for changes
