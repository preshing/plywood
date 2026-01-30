## Project Overview

Plywood is a low-level C++ base library for building cross-platform native software. It provides a simple, portable C++ interface over OS features and commonly-used data structures & algorithms. Additional components for 2D/3D math, networking and text parsing are also included.

**Supported Platforms**: Windows, macOS, iOS, Linux, Android

**Directory Structure**:
- `src/` - Contains pairs of `.h`/`.cpp` files organized by feature category. Projects can compile and link with only the features they need.
    - `ply-base.h/cpp` - Core: OS access, data structures, string formatting, Unicode conversion, threading, memory
    - `ply-math.h/cpp` - Vectors, matrices, quaternions for 2D and 3D graphics and layout
    - `ply-network.h/cpp` - TCP/IP networking (IPv4/IPv6)
    - `ply-btree.h` - B-Tree for sorted key-value storage
    - `ply-json.h/cpp` - JSON parser/serializer
    - `ply-tokenizer.h/cpp` - Text tokenization utilities
    - `ply-markdown.h/cpp` - Markdown parser with HTML output
    - `ply-cpp.h/cpp` - Experimental C++ parser
- `apps/` - Sample applications. Each subdirectory contains a `CMakeLists.txt` file to build the app.
    - `base-tests` - Main test suite to ensure correctness of `ply-base`, `ply-math` and `ply-btree`
    - `bigfont` - Converts its argument to a banner-style comment using Unicode block characters
    - `cpp-tests` - Test suite for C++ parser
    - `generate-docs` - Converts the documentation to HTML files written to `docs/build/`
    - `markdown-tests` - Test suite for Markdown parser
    - `webserver` - Runs a webserver to serve the generated documentation on port 8080
- `docs/` - Project documentation in Markdown format
    - `contents.json` - Table of contents

## Coding Conventions

- Follow the style of existing code such as `src/ply-base.cpp`
- Types use CamelCase; functions/variables use snake_case
- C++14 features only
- `.clang-format`: 120 character line limit, 4-space indentation, Attach-style braces
- All library types/functions are defined in the `ply` namespace; dependent apps use `using namespace ply`
- **Important**: Avoid using C/C++ Standard Library functions. Always use `ply-base.h` primitives instead.

## Overview of `ply-base.h`

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
- `get_min_value<T>()`/`get_max_value<T>` - Numeric limits
- `abs min max clamp` - Function templates
- Alignment functions end with `_power_of_2`
- `numeric_cast` - Asserts if conversion doesn't fit

**Time & date**:
- `struct DateTime`
- `get_unix_timestamp`
- `convert_to_date_time`
- `convert_to_unix_timestamp`

**CPU profiling**:
- `get_cpu_ticks`
- `get_cpu_ticks_per_second`

**`Heap`**: Memory allocator
- Call `Heap::alloc/realloc/free/alloc_aligned` instead of `malloc/free`
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
    - Accessing string bytes: `bytes/num_bytes/[]/back/begin/end`; `[]` performs bounds checking
    - Examining contents: `is_empty/operator bool/starts_with/ends_with/find/reverse_find`
    - Creating subviews: `substr/left/shortened_by/right/trim/trim_left/trim_right`
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
- `get_stdin get_stdout get_stderr` - Returns temporary `Stream`s over standard handles

**Threads and processes**:
- `get_current_thread_id get_current_process_id get_current_executable_path`
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
- Generic algorithms: `find reverse_find sort binary_search`
- Reading text: `read_line read_whitespace skip_whitespace read_identifier read_u64_from_text read_s64_from_text read_double_from_text read_quoted_string`
- Writing text: `print_number print_escaped_string print_xml_escaped_string`
- Unicode conversion: `encode_unicode decode_unicode`
- `Filesystem` - Filesystem operations, opening files, text format detection
- Path manipulation: `get_path_separator get_drive_letter is_absolute_path is_relative_path make_absolute_path make_relative_path split_path split_file_extension split_path_full join_path`
- `DirectoryWatcher` - Watches a directory for changes

