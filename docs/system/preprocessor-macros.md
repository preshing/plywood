Preprocessor Macros (`ply-system.h`)
==================================

`ply-system.h` makes the following preprocessor macros available for the rest of the program to use.

## Platform Detection

The header file automatically detects the target platform and enables one or more of the following preprocessor macros. Each macro is defined as `1` if it describes the target platform; otherwise it's left undefined. Provided for convenience when building platform-specific features in your own code.

| | |
| --- | --- |
| `PLY_WINDOWS` | Defined when compiling for Windows. |
| `PLY_LINUX` | Defined when compiling for Linux or Android. |
| `PLY_ANDROID` | Defined when compiling for Android. |
| `PLY_APPLE` | Defined when compiling for macOS or iOS. |
| `PLY_MACOS` | Defined when compiling for macOS. |
| `PLY_IOS` | Defined when compiling for iOS. |
| `PLY_POSIX` | Defined when compiling for Linux, macOS, Android or iOS. Indicates that POSIX API is available. |
| `PLY_MINGW` | Defined when compiling for [MinGW](https://www.mingw-w64.org/).  |

`PLY_PTR_SIZE`
> The size of a pointer, in bytes. Defined as `4` when compiling for a 32-bit architecture and `8` when compiling for a 64-bit architecture.

## Compiler-Specific Wrappers

These macros are wrappers around compiler-specific extensions. They mainly hide differences between MSVC and GCC/Clang.

`PLY_NO_INLINE`
> Prevents inline class methods from being considered as inlining candidates.

`PLY_FORCE_INLINE`
> A stronger version of C++'s `inline` keyword.

`PLY_DEBUG_BREAK()`
> Emits a CPU instruction that causes the process to halt and invoke any attached debugger.

`PLY_FORCE_CRASH()`
> Emits a CPU instruction that forces the process to crash immediately. Used internally by `PLY_ASSERT` when an assertion fails.

`PLY_COMPILER_BARRIER()`
> Prevents the compiler from [reordering adjacent memory operations](https://preshing.com/20120625/memory-ordering-at-compile-time/). Can help ensure correct memory ordering in multithreaded code, though [atomic operations](/docs/system/multithreading.md#atomic) are usually a better option.

`PLY_NO_DISCARD`
> Equivalent to the `nodiscard` keyword that was added in C++17.

## General-Purpose

`PLY_STRINGIFY(arg)`
> Converts its argument to a string literal.
> ```
> PLY_STRINGIFY(__LINE__)  // expands to "42" if used on line 42
> ```

`PLY_CAT(a, b)`
> Concatenates two macro arguments into a single token.
> ```
> PLY_CAT(foo, __LINE__)  // expands to "foo42" if used on line 42
> ```

`PLY_UNIQUE_VARIABLE(prefix)`
> Creates a unique variable name by appending the current line number as a suffix. Equivalent to `PLY_CAT(prefix, __LINE__)`. Used internally by `PLY_ON_SCOPE_EXIT` and other macros.

`PLY_PTR_OFFSET(ptr, ofs)`
> Adds a byte offset to a pointer of any type and returns the result as `void*`.

`PLY_OFFSET_OF(type, member)`
> Returns the byte offset of a member within a struct or class. Similar to C++'s `offsetof` keyword but returns a `uptr`.

`PLY_STATIC_ARRAY_SIZE(arr)`
> Evaluates to the number of elements in a C-style array.
> ```
> char buf[64];
> PLY_STATIC_ARRAY_SIZE(buf)  // evaluates to 64
> ```

`PLY_UNUSED(x)`
> Used to silence compiler warnings about unused variables, such as return values that are only used for assertion checks.
> ```
> int rc = close(socket);
> PLY_ASSERT(rc == 0);
> PLY_UNUSED(rc);  // Silences compiler warning when assertions are disabled
> ```

`PLY_CALL_MEMBER(obj, pmf)`
> A macro for invoking pointer-to-member functions, as recommended by the [C++ FAQ](https://isocpp.org/wiki/faq/pointers-to-members#macro-for-ptr-to-memfn).
> ```
> void doSomething(Foo* obj, void (Foo::*pmf)()) {
>     PLY_CALL_MEMBER(obj, pmf)();
> }
> ```

`PLY_PUN_GUARD()`
> The C++ standard [forbids type punning](https://timsong-cpp.github.io/cppwp/n4950/basic.lval#11), since the compiler would generate less efficient code if it couldn't assume [strict aliasing](https://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html). Plywood uses type punning in some places anyway, and this macro indicates where it's used. It uses `PLY_COMPILER_BARRIER` internally to prevent the compiler from making assumptions about the contents of memory around the enclosed scope.

## Assertions

PLY_ASSERT(cond)

PLY_STATIC_ASSERT(cond)
