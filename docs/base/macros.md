{title text="Preprocessor Macros" include="ply-base.h" namespace="ply"}

Plywood defines the following preprocessor macros.

{api_summary}
-- Assertions
PLY_ASSERT(cond)
PLY_STATIC_ASSERT(cond)
-- Compiler-Specific Wrappers
PLY_NO_INLINE
PLY_FORCE_INLINE
PLY_DEBUG_BREAK()
PLY_FORCE_CRASH()
PLY_COMPILER_BARRIER()
PLY_NO_DISCARD
-- General-Purpose Macros
PLY_STRINGIFY(arg)
PLY_CAT(a, b)
PLY_UNIQUE_VARIABLE(prefix)
PLY_PTR_OFFSET(ptr, ofs)
PLY_OFFSET_OF(type, member)
PLY_STATIC_ARRAY_SIZE(arr)
PLY_UNUSED(x)
PLY_CALL_MEMBER(obj, pmf)
PLY_PUN_GUARD()
-- Platform Identification
PLY_WINDOWS
PLY_POSIX
PLY_MACOS
PLY_IOS
PLY_ANDROID
PLY_MINGW
PLY_PTR_SIZE
{/api_summary}

## Assertions

PLY_ASSERT(cond)

PLY_STATIC_ASSERT(cond)

## Compiler-Specific Wrappers

These macros are wrappers around compiler-specific extensions. They mainly hide differences between MSVC and GCC/Clang.

{api_descriptions}
PLY_NO_INLINE
--
Prevents inline class methods from being considered as inlining candidates. Defined as `__declspec(noinline)` on MSVC and `__attribute__((noinline))` on GCC and Clang.

>>
PLY_FORCE_INLINE
--
A stronger version of C++'s `inline` keyword. Defined as `__forceinline` on MSVC and `__attribute__((always_inline))` on GCC and Clang.

>>
PLY_DEBUG_BREAK()
--
Emits a CPU instruction that causes the process to halt and invoke any attached debugger. Can be inserted into the code as an alternative to conditional breakpoints, which are often slow. Defined as `__debugbreak()` in MSVC and `__builtin_trap` in GCC and Clang.

>>
PLY_FORCE_CRASH()
--
Emits a CPU instruction that causes the program to crash immediately. Defined as `__ud2()` on MSVC and `__builtin_trap()` on GCC and Clang. Used internally by `PLY_ASSERT` when an assertion fails.

>>
PLY_COMPILER_BARRIER()
--
Prevents the compiler from [reordering adjacent memory operations](https://preshing.com/20120625/memory-ordering-at-compile-time/). Can help ensure correct memory ordering in multithreaded code, though [atomic operations](/docs/base/threads#atomic) are usually a better option. Defined as `_ReadWriteBarrier()` on MSVC and `asm volatile("" ::: "memory")` on GCC and Clang.

>>
PLY_NO_DISCARD
--
Equivalent to the `nodiscard` keyword that was added in C++17. Defined as `_Check_return_` on MSVC and `__attribute__((warn_unused_result))` on GCC and Clang.
{/api_descriptions}

## General-Purpose Macros

{api_descriptions}
PLY_STRINGIFY(arg)
--
Converts its argument to a string literal.

    PLY_STRINGIFY(__LINE__)  // expands to "42" if used on line 42

>>
PLY_CAT(a, b)
--
Concatenates two macro arguments into a single token.

    PLY_CAT(foo, __LINE__)  // expands to "foo42" if used on line 42

>>
PLY_UNIQUE_VARIABLE(prefix)
--
Creates a unique variable name by appending the current line number as a suffix. Equivalent to `PLY_CAT(prefix, __LINE__)`. Used internally by `PLY_ON_SCOPE_EXIT` and other macros.

>>
PLY_PTR_OFFSET(ptr, ofs)
--
Adds a byte offset to a pointer of any type and returns the result as `void*`.

>>
PLY_OFFSET_OF(type, member)
--
Returns the byte offset of a member within a struct or class. Similar to C++'s `offsetof` keyword but returns a `uptr`.

>>
PLY_STATIC_ARRAY_SIZE(arr)
--
Evaluates to the number of elements in a C-style array.

    char buf[64];
    PLY_STATIC_ARRAY_SIZE(buf)  // evaluates to 64

>>
PLY_UNUSED(x)
--
Used to silence compiler warnings about unused variables, such as return values that are only used for assertion checks.

    int rc = close(socket);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);  // Silences compiler warning when assertions are disabled

>>
PLY_CALL_MEMBER(obj, pmf)
--
A macro for invoking pointer-to-member functions, as recommended by the [C++ FAQ](https://isocpp.org/wiki/faq/pointers-to-members#macro-for-ptr-to-memfn).

    void do_something(Foo* obj, void (Foo::*pmf)()) {
        PLY_CALL_MEMBER(obj, pmf)();
    }

>>
PLY_PUN_GUARD()
--
The C++ standard [forbids type punning](https://timsong-cpp.github.io/cppwp/n4950/basic.lval#11), since the compiler would generate less efficient code if it couldn't assume [strict aliasing](https://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html). Plywood uses type punning in some places anyway, and this macro indicates where it's used. It uses `PLY_COMPILER_BARRIER` internally to prevent the compiler from making assumptions about the contents of memory around the enclosed scope.
{/api_descriptions}

## Platform Identification

This table shows blah blah blah. In application code, you often need to blah blah, so this gives you a way to do that.
Most just use compiler-specific predefined macros to determine it and offer a convenient mnemonic.

{api_descriptions}
PLY_PTR_SIZE
--
The size of a pointer in bytes. Defined as `4` when compiling for a 32-bit platform and `8` when compiling for a 64-bit platform. These days, most computers and mobile devices are 64-bit platforms, but it's still possible to compile 32-bit software if you need to support 32-bit Windows 10 or a 32-bit Linux. WebAssembly is strictly a 32-bit platform.

>>
PLY_WINDOWS
--
Defined when compiling for Windows. Indicates that the Windows SDK is available.

>>
PLY_POSIX
--
Defined when compiling for Linux, macOS, Android or iOS. Indicates that POSIX API is available.

>>
PLY_MACOS
--
Defined when compiling for macOS. Indicates that the macOS SDK is available.

>>
PLY_IOS
--
Defined when compiling for iOS. Indicates that the iOS SDK is available.

>>
PLY_ANDROID
--
Defined when compiling for Android. Indicates that the Android SDK is available.

>>
PLY_MINGW
--
Defined when compiling for [MinGW](https://www.mingw-w64.org/). 
{/api_descriptions}

