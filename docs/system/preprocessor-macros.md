Preprocessor Macros (`ply-system.h`)
==================================

## Platform Detection

Can be used to enable platform-specific features in user code.

`PLY_WINDOWS`
> Defined as 1 when compiling for Windows; otherwise undefined.

`PLY_LINUX`
> Defined as 1 when compiling for Linux or Android; otherwise undefined.

`PLY_ANDROID`
> Defined as 1 when compiling for Android; otherwise undefined.

`PLY_APPLE`
> Defined as 1 when compiling for macOS or iOS; otherwise undefined.

`PLY_MACOS`
> Defined as 1 when compiling for macOS; otherwise undefined.

`PLY_IOS`
> Defined as 1 when compiling for iOS; otherwise undefined.

`PLY_POSIX`
> Indicates that a POSIX-compatible API is available. Defined as 1 when compiling for Linux, macOS, Android or iOS; otherwise undefined.

`PLY_MINGW`
> Defined as 1 when compiling for [MinGW](https://www.mingw-w64.org/); otherwise undefined.

`PLY_PTR_SIZE`
> The size of a pointer, in bytes. Defined as `4` when compiling for a 32-bit architecture and `8` when compiling for a 64-bit architecture.

## Compiler-Specific Wrappers

Wrappers around compiler-specific extensions. Mainly used to hide differences between MSVC and GCC/Clang.

`PLY_NO_INLINE`
> Prevents inline class methods from being considered as inlining candidates.

`PLY_FORCE_INLINE`
> A stronger version of C++'s `inline` keyword.

`PLY_DEBUG_BREAK()`
> Emits a CPU instruction that causes the process to halt and invoke any attached debugger.

`PLY_FORCE_CRASH()`
> Emits a CPU instruction that forces the process to crash immediately. Used internally by `PLY_ASSERT` when an assertion fails.

`PLY_COMPILER_BARRIER()`
> Prevents the compiler from [reordering adjacent memory operations](https://preshing.com/20120625/memory-ordering-at-compile-time/). Can help ensure correct memory ordering in multithreaded code.

`PLY_NO_DISCARD`
> Equivalent to C++17's `nodiscard` keyword.

## General-Purpose Macros

Plywood versions of commonly-used C++ macros.

`PLY_STRINGIFY(arg)`
> Converts its argument to a string literal.
> ```
> PLY_STRINGIFY(__LINE__)  // Expands to "42" on line 42.
> ```

`PLY_CAT(a, b)`
> Concatenates two macro arguments into a single token.
> ```
> PLY_CAT(foo, __LINE__)  // Expands to "foo42" on line 42.
> ```

`PLY_UNIQUE_VARIABLE(prefix)`
> Creates a unique variable name by appending the current line number as a suffix. Equivalent to `PLY_CAT(prefix, __LINE__)`. Used internally by `PLY_ON_SCOPE_EXIT` and other macros.

`PLY_PTR_OFFSET(ptr, ofs)`
> Adds a byte offset to a pointer of any type and returns the result as `void*`.

`PLY_OFFSET_OF(type, member)`
> Returns the byte offset of a member within a struct or class. Similar to C++'s `offsetof` keyword, but returns a `uptr`.

`PLY_STATIC_ARRAY_SIZE(arr)`
> Evaluates to the number of elements in a C-style array.
> ```
> char buf[64];
> PLY_STATIC_ARRAY_SIZE(buf)  // Evaluates to 64.
> ```

`PLY_UNUSED(x)`
> Used to silence compiler warnings about unused variables, such as return values that are only used for assertion checks.
> ```
> int rc = close(socket);
> PLY_ASSERT(rc == 0);
> PLY_UNUSED(rc);  // Silences a compiler warning when assertions are disabled.
> ```

## Assertions

Plywood assertions are extremely simple: If the condition fails, they immediately force a crash using a single CPU instruction. This is enough to see which assertion failed when a debugger is attached, including when a crash dump is loaded.

`PLY_ASSERT(cond)`
> If the condition fails, immediately forces a crash. Only enabled when [`PLY_WITH_ASSERTS=1`](system/index.md#configuration-options).

`PLY_STATIC_ASSERT(cond)`
> Single-argument wrapper around C++'s [`static_assert`](https://en.cppreference.com/cpp/language/static_assert) that works with C++14 compilers.
