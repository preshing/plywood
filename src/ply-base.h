/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once

#if defined(PLY_CONFIG_HEADER)
#include PLY_CONFIG_HEADER
#endif

#include <type_traits>
#include <utility>
#include <float.h>
#include <initializer_list>
#include <new>

//--------------------------------------------
// Compiler detection
//--------------------------------------------

#if defined(_MSC_VER) // MSVC

#include <intrin.h>

#if defined(_M_X64)
#define PLY_PTR_SIZE 8
#elif defined(_M_IX86)
#define PLY_PTR_SIZE 4
#endif

#define PLY_NO_INLINE __declspec(noinline)
#define PLY_FORCE_INLINE __forceinline
#define PLY_DEBUG_BREAK() __debugbreak()
#define PLY_FORCE_CRASH() __ud2()
#define PLY_COMPILER_BARRIER() _ReadWriteBarrier()

#if _MSC_VER >= 1700
#define PLY_NO_DISCARD _Check_return_
#else
#define PLY_NO_DISCARD
#endif

#elif defined(__GNUC__) // GCC/Clang

#if defined(__APPLE__)
#define PLY_APPLE 1
#define PLY_POSIX 1
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#define PLY_IOS 1
#elif TARGET_OS_MAC
#define PLY_MACOS 1
#endif
#endif
#if defined(__FreeBSD__)
#define PLY_POSIX 1
#endif
#if defined(__linux__)
#define PLY_LINUX 1
#define PLY_POSIX 1
#if defined(__ANDROID__)
#define PLY_ANDROID 1
#endif
#endif
#if defined(__MINGW32__) || defined(__MINGW64__)
#define PLY_MINGW 1
#endif

#if defined(__x86_64__)
#define PLY_PTR_SIZE 8
#elif defined(__i386__)
#define PLY_PTR_SIZE 4
#elif defined(__arm__)
#define PLY_PTR_SIZE 4
#elif defined(__arm64__) || defined(__aarch64__)
#define PLY_PTR_SIZE 8
#endif

#define PLY_NO_INLINE __attribute__((noinline))
#define PLY_FORCE_INLINE inline __attribute__((always_inline))
#define PLY_DEBUG_BREAK() __builtin_trap()
#define PLY_FORCE_CRASH() __builtin_trap()
#define PLY_COMPILER_BARRIER() asm volatile("" ::: "memory")
#define PLY_NO_DISCARD __attribute__((warn_unused_result))

#endif

//--------------------------------------------
// Platform includes
//--------------------------------------------

#if defined(_WIN32)
#define PLY_WINDOWS 1
#endif

#if defined(PLY_WINDOWS) // Windows
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(PLY_POSIX) // Linux, macOS, iOS, Android
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#if defined(PLY_APPLE) // macOS & iOS
#include <mach/mach.h>
#include <mach/mach_time.h>
#else
#include <semaphore.h>
#endif
#endif

//  ▄▄   ▄▄
//  ███▄███  ▄▄▄▄   ▄▄▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██    ██  ▀▀ ██  ██ ▀█▄▄▄
//  ██   ██ ▀█▄▄██ ▀█▄▄▄ ██     ▀█▄▄█▀  ▄▄▄█▀
//

//--------------------------------------------
// Asserts
//--------------------------------------------

#if defined(PLY_WITH_ASSERTS)
#define PLY_ASSERT(cond) \
    do { \
        if (!(cond)) \
            PLY_FORCE_CRASH(); \
    } while (0)
#else
#define PLY_ASSERT(cond) \
    do { \
    } while (0)
#endif

//--------------------------------------------
// Common macros
//--------------------------------------------

#define PLY_STRINGIFY2(x) #x
#define PLY_STRINGIFY(x) PLY_STRINGIFY2(x)
#define PLY_CAT2(a, b) a##b
#define PLY_CAT(a, b) PLY_CAT2(a, b)
#define PLY_UNIQUE_VARIABLE(prefix) PLY_CAT(prefix, __LINE__)
#define PLY_PTR_OFFSET(ptr, ofs) ((void*) (((u8*) (void*) (ptr)) + (ofs)))
#define PLY_OFFSET_OF(type, member) uptr(&((type*) 0)->member)
#define PLY_STATIC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define PLY_UNUSED(x) ((void) x)
#define PLY_CALL_MEMBER(obj, pmf) ((obj).*(pmf))
#define PLY_STATIC_ASSERT(cond) static_assert(cond, #cond)

#if !defined(PLY_USE_NEW_ALLOCATOR)
#define PLY_USE_NEW_ALLOCATOR 1
#endif

namespace ply {

//--------------------------------------------
// PLY_PUN_GUARD
//--------------------------------------------

struct PunGuard {
    PunGuard() {
        PLY_COMPILER_BARRIER();
    }
    ~PunGuard() {
        PLY_COMPILER_BARRIER();
    }
};
#define PLY_PUN_GUARD ::ply::PunGuard PLY_UNIQUE_VARIABLE(_punGuard_)

//--------------------------------------------
// PLY_SET_IN_SCOPE
//--------------------------------------------

template <typename T, typename V>
struct SetInScope {
    T& target;                           // The variable to set/reset
    std::remove_reference_t<T> oldValue; // Backup of original value
    const V& newValueRef;                // Extends the lifetime of temporary values in the case of
                                         // eg. Set_In_Scope<String_View, String>

    template <typename U>
    SetInScope(T& target, U&& newValue) : target{target}, oldValue{std::move(target)}, newValueRef{newValue} {
        target = std::forward<U>(newValue);
    }
    ~SetInScope() {
        this->target = std::move(this->oldValue);
    }
};

#define PLY_SET_IN_SCOPE(target, value) \
    SetInScope<decltype(target), decltype(value)> PLY_UNIQUE_VARIABLE(setInScope) { \
        target, value \
    }

//--------------------------------------------
// PLY_ON_SCOPE_EXIT
//--------------------------------------------

template <typename Callback>
struct OnScopeExit {
    Callback cb;
    ~OnScopeExit() {
        cb();
    }
};
template <typename Callback>
OnScopeExit<Callback> setOnScopeExit(Callback&& cb) {
    return {std::forward<Callback>(cb)};
}
#define PLY_ON_SCOPE_EXIT(cb) auto PLY_UNIQUE_VARIABLE(onScopeExit) = setOnScopeExit([&] cb)

//   ▄▄▄▄  ▄▄▄▄▄ ▄▄▄▄ ▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██  ▀▀ ██     ██  ███ ██ ██  ██ ██
//   ▀▀▀█▄ ██▀▀   ██  ██▀███ ██▀▀██ ██▀▀
//  ▀█▄▄█▀ ██    ▄██▄ ██  ██ ██  ██ ██▄▄▄
//

template <typename T>
T&& declval();
template <typename...>
using void_t = void;

template <bool>
struct enableIfBool;
template <>
struct enableIfBool<true> {
    using type = int;
};
#define PLY_ENABLE_IF(x) typename ::ply::enableIfBool<(x)>::type = 0

template <typename>
struct enableIfType {
    using type = int;
};
#define PLY_ENABLE_IF_WELL_FORMED(x) typename ::ply::enableIfType<decltype(x)>::type = 0

// Why doesn't this work reliably without void_t<...>?
// In particular, hasGetLookupKeyMember stops working, seemingly because the member function returns non-void.
#define PLY_CHECK_WELL_FORMED(name, expr) \
    template <typename, typename = void> \
    static constexpr bool name = false; \
    template <typename T> \
    static constexpr bool name<T, void_t<decltype(expr)>> = true;

//  ▄▄  ▄▄                               ▄▄
//  ███ ██ ▄▄  ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄
//  ██▀███ ██  ██ ██ ██ ██ ██▄▄██ ██  ▀▀ ██ ██
//  ██  ██ ▀█▄▄██ ██ ██ ██ ▀█▄▄▄  ██     ██ ▀█▄▄▄
//

//--------------------------------------------
// Numeric types
//--------------------------------------------

using s8 = char;
using s16 = short;
using s32 = int;
using s64 = long long;
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
#if PLY_PTR_SIZE == 4
using sptr = s32;
using uptr = u32;
#else
using sptr = s64;
using uptr = u64;
#endif

//--------------------------------------------
// Numeric limits
//--------------------------------------------

template <typename T>
constexpr T getMinValue();
template <typename T>
constexpr T getMaxValue();
#define PLY_MAKE_LIMITS(T, lo, hi) \
    template <> \
    constexpr T getMinValue<T>() { \
        return lo; \
    } \
    template <> \
    constexpr T getMaxValue<T>() { \
        return hi; \
    }
PLY_MAKE_LIMITS(s8, -0x80, 0x7f)
PLY_MAKE_LIMITS(u8, 0, 0xff)
PLY_MAKE_LIMITS(s16, -0x8000, 0x7fff)
PLY_MAKE_LIMITS(u16, 0, 0xffff)
PLY_MAKE_LIMITS(s32, 0x80000000, 0x7fffffff)
PLY_MAKE_LIMITS(u32, 0, 0xffffffff)
#if defined(_MSC_VER)
PLY_STATIC_ASSERT(sizeof(long) == 4);
PLY_MAKE_LIMITS(long, 0x80000000, 0x7fffffff)
PLY_MAKE_LIMITS(unsigned long, 0, 0xffffffff)
#else
PLY_STATIC_ASSERT(sizeof(long) == 8);
PLY_MAKE_LIMITS(long, -0x8000000000000000l, 0x7fffffffffffffffl)
PLY_MAKE_LIMITS(unsigned long, 0, 0xfffffffffffffffful)
#endif
PLY_MAKE_LIMITS(s64, -0x8000000000000000ll, 0x7fffffffffffffffll)
PLY_MAKE_LIMITS(u64, 0, 0xffffffffffffffffull)
PLY_MAKE_LIMITS(float, -3.402823466e+38f, 3.402823466e+38f)
PLY_MAKE_LIMITS(double, -1.7976931348623158e+308, 1.7976931348623158e+308)

//--------------------------------------------
// Numeric functions
//--------------------------------------------

template <typename Type>
inline constexpr Type abs(Type val) {
    return (val >= 0) ? val : -val;
}
template <typename T>
inline constexpr T min(T val1, T val2) {
    return (val1 < val2) ? val1 : val2;
}
template <typename T>
inline constexpr T max(T val1, T val2) {
    return (val1 > val2) ? val1 : val2;
}
template <typename Type>
inline constexpr Type clamp(Type val, Type lo, Type hi) {
    return (val < lo) ? lo : (val < hi) ? val : hi;
}
inline constexpr u16 reverseBytes(u16 val) {
    return ((val >> 8) & 0xff) | ((val << 8) & 0xff00);
}
inline constexpr u32 reverseBytes(u32 val) {
    return ((val >> 24) & 0xff) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000) | ((val << 24) & 0xff000000u);
}
inline constexpr u64 reverseBytes(u64 val) {
    return ((u64) reverseBytes(u32(val)) << 32) | reverseBytes(u32(val >> 32));
}
#if PLY_IS_BIG_ENDIAN
template <typename Type>
inline constexpr Type convertLittleEndian(Type val) {
    return reverseBytes(val);
}
template <typename Type>
inline constexpr Type convertBigEndian(Type val) {
    return val;
}
#else
template <typename Type>
inline constexpr Type convertLittleEndian(Type val) {
    return val;
}
template <typename Type>
inline constexpr Type convertBigEndian(Type val) {
    return reverseBytes(val);
}
#endif
inline u32 isPowerOf2(u32 val) {
    return (val & (val - 1)) == 0;
}
inline u64 isPowerOf2(u64 val) {
    return (val & (val - 1)) == 0;
}
inline u32 alignToPowerOf2(u32 v, u32 a) {
    PLY_ASSERT(isPowerOf2(a));
    return (v + a - 1) & ~(a - 1);
}
inline u64 alignToPowerOf2(u64 v, u64 a) {
    PLY_ASSERT(isPowerOf2(a));
    return (v + a - 1) & ~(a - 1);
}
inline bool isAlignedToPowerOf2(u32 v, u32 a) {
    PLY_ASSERT(isPowerOf2(a));
    return (v & (a - 1)) == 0;
}
inline bool isAlignedToPowerOf2(u64 v, u64 a) {
    PLY_ASSERT(isPowerOf2(a));
    return (v & (a - 1)) == 0;
}
inline constexpr u32 roundUpToNearestPowerOf2(u32 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}
inline constexpr u64 roundUpToNearestPowerOf2(u64 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return v + 1;
}
template <typename DstType, typename SrcType>
constexpr bool isRepresentable(SrcType val) {
    if (((SrcType) (DstType) val) != val)
        return false;
    return (val > 0) == (((DstType) val) > 0);
}
template <typename DstType, typename SrcType>
constexpr DstType numericCast(SrcType val) {
    PLY_ASSERT(isRepresentable<DstType>(val));
    return (DstType) val;
}

//  ▄▄▄▄▄▄ ▄▄                      ▄▄▄        ▄▄▄▄▄          ▄▄
//    ██   ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄      ██ ▀▀       ██  ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄
//    ██   ██ ██ ██ ██ ██▄▄██     ▄█▀█▄▀▀     ██  ██  ▄▄▄██  ██   ██▄▄██
//    ██   ██ ██ ██ ██ ▀█▄▄▄      ▀█▄▄▀█▄     ██▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄
//

//----------------------------------------------------
// Unix Timestamps and DateTime objects
//----------------------------------------------------

// A DateTime object describes a calendar date and time of day. Each object is expressed in the time zone indicated by
// its timeZoneOffsetInMinutes member, which is relative to Coordinated Universal Time (UTC). For example, a
// timeZoneOffsetInMinutes of -300 corresponds to Eastern Standard Time (EST), which is 5 hours behind UTC.
struct DateTime {
    s32 year = 0;
    u8 month = 0;   // 1..12
    u8 day = 0;     // 1..31
    u8 weekday = 0; // Sunday = 0, Saturday = 6
    u8 hour = 0;    // 0..23
    u8 minute = 0;  // 0..59
    u8 second = 0;  // 0..59
    s16 timeZoneOffsetInMinutes = 0;
    u32 microsecond = 0; // 0..999999
};

// Returns the current number of microseconds since Jan 1 1970, 00:00 UTC according to the system clock.
s64 getUnixTimestamp();

// Converts a Unix timestamp to a DateTime object.
DateTime convertToDateTime(s64 systemTime); // Uses local time zone offset
DateTime convertToDateTime(s64 systemTime, s16 timeZoneOffsetInMinutes);

// Converts a DateTime object back to a Unix timestamp.
s64 convertToUnixTimestamp(const DateTime& dateTime);

//----------------------------------------------------
// High-Resolution Timer
//----------------------------------------------------

// Returns a high-resolution CPU timestamp.
inline u64 getCpuTicks() {
#if defined(PLY_WINDOWS)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
#elif defined(PLY_APPLE)
    return mach_absolute_time();
#elif defined(PLY_POSIX)
    struct timespec tick;
    clock_gettime(CLOCK_MONOTONIC, &tick);
    return (u64) tick.tv_sec * 1000000000ull + tick.tv_nsec;
#endif
}

// Returns the high-resolution timer frequency. To measure an interval of time in seconds, subtract two timestamps and
// divide the result by this value.
float getCpuTicksPerSecond();

//  ▄▄▄▄▄                    ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀█▄  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██ ██ ██
//  ██  ██ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄█▀ ██ ██ ██
//

// Random is a class that generates pseudorandom numbers using the xoroshiro128** algorithm, as
// described here: http://xorshift.di.unimi.it/
// generateU64() returns a uniformly distributed pseudorandom 64-bit integer. You can map the returned value
// to a smaller range by using the modulo operator or by discarding upper bits.
// generateFloat() is a convenience function that uses generateU64() to generate a uniformly distributed
// pseudorandom floating-point value between [0.0f, 1.0f).
// You can optionally specify a 64-bit integer seed when constructing an instance of Random. This makes the number
// sequence generated by generateU64() deterministic. Two Random instances constructed using the same seed always
// generate the same number sequence. The default constructor is self-seeding. The seed is calculated using various
// information from the runtime environment, such as the current time and thread ID, so that two default-constructed
// Random instances are unlikely to generate the same number sequence.

class Random {
private:
    u64 s[2];

public:
    Random();
    Random(u64 seed);
    u64 generateU64();
    u32 generateU32() {
        return (u32) this->generateU64();
    }
    float generateFloat() {
        return (u32) generateU64() / 4294967296.f;
    }
};

//  ▄▄▄▄▄▄ ▄▄                              ▄▄     ▄▄▄▄ ▄▄▄▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██      ██  ██  ██  ▄▄▄▄
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██      ██  ██  ██ ▀█▄▄▄
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██     ▄██▄ ██▄▄█▀  ▄▄▄█▀
//

// getCurrentThreadId() and getCurrentProcessId() return the current thread and process ID.
// TID and PID are type aliases for either u32 or u64, depending on the platform.

#if defined(PLY_WINDOWS)

using TID = u32;
using PID = u32;

inline TID getCurrentThreadId() {
#if defined(_M_X64)
    return ((DWORD*) __readgsqword(48))[18];
#elif defined(_M_IX86)
    return ((DWORD*) __readfsdword(24))[9];
#else
    return GetCurrentThreadID();
#endif
}

inline PID getCurrentProcessId() {
#if defined(_M_X64)
    return ((DWORD*) __readgsqword(48))[16];
#elif defined(_M_IX86)
    return ((DWORD*) __readfsdword(24))[8];
#else
    return GetCurrentProcessID();
#endif
}

#elif defined(PLY_APPLE)

using TID = std::conditional_t<sizeof(thread_port_t) == 4, u32, u64>;
using PID = std::conditional_t<sizeof(pid_t) == 4, u32, u64>;

inline TID getCurrentThreadId() {
    return pthread_mach_thread_np(pthread_self());
}

inline PID getCurrentProcessId() {
    return getpid();
}

#elif defined(PLY_POSIX)

using TID = std::conditional_t<sizeof(pthread_t) == 4, u32, u64>;
using PID = std::conditional_t<sizeof(pid_t) == 4, u32, u64>;

inline TID getCurrentThreadId() {
#if defined(__FreeBSD__)
    return pthread_getthreadid_np();
#elif defined(PLY_MINGW)
    return (TID) pthread_self().p;
#else
    return pthread_self();
#endif
}

inline PID getCurrentProcessId() {
    return getpid();
}

#endif

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

// Suspends the calling thread for the specified number of milliseconds.
inline void sleepMillis(u32 millis) {
#if defined(PLY_WINDOWS)
    Sleep((DWORD) millis);
#elif defined(PLY_POSIX)
    timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

template <typename>
struct Functor;

#if defined(PLY_WINDOWS)

DWORD WINAPI threadEntry(LPVOID param);

//----------------------------------------------------
// Windows implementation.
class Thread {
public:
    HANDLE handle = INVALID_HANDLE_VALUE;

    Thread() = default;
    Thread(Functor<void()>&& entry) {
        run(std::move(entry));
    }
    Thread(Thread&& other) : handle{other.handle} {
        other.handle = INVALID_HANDLE_VALUE;
    }
    ~Thread() {
        if (this->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(this->handle);
        }
    }
    Thread& operator=(Thread&& other) {
        this->~Thread();
        new (this) Thread{std::move(other)};
        return *this;
    }
    bool isValid() const {
        return this->handle != INVALID_HANDLE_VALUE;
    }
    // Starts a new thread that executes the specified callable.
    void run(Functor<void()>&& entry);
    // Releases this thread object's handle without waiting for the thread to finish.
    void detach() {
        PLY_ASSERT(this->handle != INVALID_HANDLE_VALUE);
        CloseHandle(this->handle);
        this->handle = INVALID_HANDLE_VALUE;
    }
    // Waits for the thread to finish execution.
    void join() {
        PLY_ASSERT(this->handle != INVALID_HANDLE_VALUE);
        WaitForSingleObject(this->handle, INFINITE);
        CloseHandle(this->handle);
        this->handle = INVALID_HANDLE_VALUE;
    }
};

#elif defined(PLY_POSIX)

void* threadEntry(void*);

//----------------------------------------------------
// POSIX implementation.
class Thread {
public:
    pthread_t handle;
    bool attached = false;

    Thread() = default;
    Thread(Functor<void()>&& entry) {
        run(std::move(entry));
    }
    ~Thread() {
        if (this->attached) {
            this->detach();
        }
    }
    bool isValid() {
        return this->attached;
    }
    // Starts a new thread that executes the specified callable.
    void run(Functor<void()>&& entry);
    // Detaches the thread so it can continue running after this object is destroyed.
    void detach() {
        PLY_ASSERT(this->attached);
        pthread_detach(this->handle);
        this->attached = false;
    }
    // Waits for the thread to finish execution.
    void join() {
        PLY_ASSERT(this->attached);
        void* retVal = nullptr;
        pthread_join(this->handle, &retVal);
        this->attached = false;
    }
};

#endif

inline Thread spawnThread(Functor<void()>&& entry) {
    return Thread{std::move(entry)};
}

//   ▄▄▄▄   ▄▄                   ▄▄
//  ██  ██ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄  ▄▄▄▄
//  ██▀▀██  ██   ██  ██ ██ ██ ██ ██ ██
//  ██  ██  ▀█▄▄ ▀█▄▄█▀ ██ ██ ██ ██ ▀█▄▄▄
//

template <typename T, int = sizeof(T)>
class Atomic;

enum MemoryOrder { Relaxed, Acquire, Release, AcqRel };

#if defined(_MSC_VER)

//----------------------------------------------------
// MSVC implementation.
template <typename T>
class Atomic<T, 1> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 1>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        T result = *(volatile T*) &this->value;
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        return result;
    }
    void store(T value, MemoryOrder order) {
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        *(volatile T*) &this->value = value;
    }
    T compareExchange(T expected, T desired, MemoryOrder) {
        return (T) _InterlockedCompareExchange8((volatile char*) &this->value, (char) desired, (char) expected);
    }
    T exchange(T desired, MemoryOrder) {
        return (T) _InterlockedExchange8((volatile char*) &this->value, (char) desired);
    }
    T fetchAdd(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd8((volatile char*) &this->value, (char) operand);
    }
    T fetchSub(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd8((volatile char*) &this->value, -(char) operand);
    }
    T fetchAnd(T operand, MemoryOrder) {
        return (T) _InterlockedAnd8((volatile char*) &this->value, (char) operand);
    }
    T fetchOr(T operand, MemoryOrder) {
        return (T) _InterlockedOr8((volatile char*) &this->value, (char) operand);
    }
};

template <typename T>
class Atomic<T, 2> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 2>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        T result = *(volatile T*) &this->value;
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        return result;
    }
    void store(T value, MemoryOrder order) {
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        *(volatile T*) &this->value = value;
    }
    T compareExchange(T expected, T desired, MemoryOrder) {
        return (T) _InterlockedCompareExchange16((volatile short*) &this->value, (short) desired, (short) expected);
    }
    T exchange(T desired, MemoryOrder) {
        return (T) _InterlockedExchange16((volatile short*) &this->value, (short) desired);
    }
    T fetchAdd(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd16((volatile short*) &this->value, (short) operand);
    }
    T fetchSub(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd16((volatile short*) &this->value, -(short) operand);
    }
    T fetchAnd(T operand, MemoryOrder) {
        return (T) _InterlockedAnd16((volatile short*) &this->value, (short) operand);
    }
    T fetchOr(T operand, MemoryOrder) {
        return (T) _InterlockedOr16((volatile short*) &this->value, (short) operand);
    }
};

template <typename T>
class Atomic<T, 4> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 4>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        T result = *(volatile T*) &this->value;
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        return result;
    }
    void store(T value, MemoryOrder order) {
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        *(volatile T*) &this->value = value;
    }
    T compareExchange(T expected, T desired, MemoryOrder) {
        return (T) _InterlockedCompareExchange((volatile long*) &this->value, (long) desired, (long) expected);
    }
    T exchange(T desired, MemoryOrder) {
        return (T) _InterlockedExchange((volatile long*) &this->value, (long) desired);
    }
    T fetchAdd(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd((volatile long*) &this->value, (long) operand);
    }
    T fetchSub(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd((volatile long*) &this->value, -(long) operand);
    }
    T fetchAnd(T operand, MemoryOrder) {
        return (T) _InterlockedAnd((volatile long*) &this->value, (long) operand);
    }
    T fetchOr(T operand, MemoryOrder) {
        return (T) _InterlockedOr((volatile long*) &this->value, (long) operand);
    }
};

template <typename T>
class Atomic<T, 8> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 8>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
#if PLY_PTR_SIZE == 8
        T result = *(volatile T*) &this->value;
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        return result;
#else
        if (order != Relaxed) {
            return _InterlockedCompareExchange64_acq((volatile __int64*) &this->value, 0, 0);
        }
        return *(volatile T*) &this->value;
#endif
    }
    void store(T value, MemoryOrder order) {
#if PLY_PTR_SIZE == 8
        if (order != Relaxed) {
            _ReadWriteBarrier();
        }
        *(volatile T*) &this->value = value;
#else
        if (order != Relaxed) {
            _InterlockedExchange64_rel((volatile __int64*) &this->value, value);
        } else {
            *(volatile T*) &this->value = value;
        }
#endif
    }
    T compareExchange(T expected, T desired, MemoryOrder) {
        return (T) _InterlockedCompareExchange64((volatile __int64*) &this->value, (__int64) desired,
                                                 (__int64) expected);
    }
    T exchange(T desired, MemoryOrder) {
        return (T) _InterlockedExchange64((volatile __int64*) &this->value, (__int64) desired);
    }
    T fetchAdd(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd64((volatile __int64*) &this->value, (__int64) operand);
    }
    T fetchSub(T operand, MemoryOrder) {
        return (T) _InterlockedExchangeAdd64((volatile __int64*) &this->value, -(__int64) operand);
    }
    T fetchAnd(T operand, MemoryOrder) {
        return (T) _InterlockedAnd64((volatile __int64*) &this->value, (__int64) operand);
    }
    T fetchOr(T operand, MemoryOrder) {
        return (T) _InterlockedOr64((volatile __int64*) &this->value, (__int64) operand);
    }
};

#elif defined(__GNUC__)

//----------------------------------------------------
// GCC/Clang implementation.
constexpr int toGccOrder(MemoryOrder order) {
    switch (order) {
        case Relaxed:
        default:
            return __ATOMIC_RELAXED;
        case Acquire:
            return __ATOMIC_ACQUIRE;
        case Release:
            return __ATOMIC_RELEASE;
        case AcqRel:
            return __ATOMIC_ACQ_REL;
    }
}

template <typename T>
class Atomic<T, 1> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 1>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        return __atomic_load_n(&this->value, toGccOrder(order));
    }
    void store(T value, MemoryOrder order) {
        __atomic_store_n(&this->value, value, toGccOrder(order));
    }
    T compareExchange(T expected, T desired, MemoryOrder order) {
        MemoryOrder failOrder = (order == AcqRel) ? Acquire : ((order == Release) ? Relaxed : order);
        __atomic_compare_exchange_n(&this->value, &expected, desired, false, toGccOrder(order), toGccOrder(failOrder));
        return expected;
    }
    T exchange(T desired, MemoryOrder order) {
        return __atomic_exchange_n(&this->value, desired, toGccOrder(order));
    }
    T fetchAdd(T operand, MemoryOrder order) {
        return __atomic_fetch_add(&this->value, operand, toGccOrder(order));
    }
    T fetchSub(T operand, MemoryOrder order) {
        return __atomic_fetch_sub(&this->value, operand, toGccOrder(order));
    }
    T fetchAnd(T operand, MemoryOrder order) {
        return __atomic_fetch_and(&this->value, operand, toGccOrder(order));
    }
    T fetchOr(T operand, MemoryOrder order) {
        return __atomic_fetch_or(&this->value, operand, toGccOrder(order));
    }
};

template <typename T>
class Atomic<T, 2> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 2>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        return __atomic_load_n(&this->value, toGccOrder(order));
    }
    void store(T value, MemoryOrder order) {
        __atomic_store_n(&this->value, value, toGccOrder(order));
    }
    T compareExchange(T expected, T desired, MemoryOrder order) {
        MemoryOrder failOrder = (order == AcqRel) ? Acquire : ((order == Release) ? Relaxed : order);
        __atomic_compare_exchange_n(&this->value, &expected, desired, false, toGccOrder(order), toGccOrder(failOrder));
        return expected;
    }
    T exchange(T desired, MemoryOrder order) {
        return __atomic_exchange_n(&this->value, desired, toGccOrder(order));
    }
    T fetchAdd(T operand, MemoryOrder order) {
        return __atomic_fetch_add(&this->value, operand, toGccOrder(order));
    }
    T fetchSub(T operand, MemoryOrder order) {
        return __atomic_fetch_sub(&this->value, operand, toGccOrder(order));
    }
    T fetchAnd(T operand, MemoryOrder order) {
        return __atomic_fetch_and(&this->value, operand, toGccOrder(order));
    }
    T fetchOr(T operand, MemoryOrder order) {
        return __atomic_fetch_or(&this->value, operand, toGccOrder(order));
    }
};

template <typename T>
class Atomic<T, 4> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 4>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        return __atomic_load_n(&this->value, toGccOrder(order));
    }
    void store(T value, MemoryOrder order) {
        __atomic_store_n(&this->value, value, toGccOrder(order));
    }
    T compareExchange(T expected, T desired, MemoryOrder order) {
        MemoryOrder failOrder = (order == AcqRel) ? Acquire : ((order == Release) ? Relaxed : order);
        __atomic_compare_exchange_n(&this->value, &expected, desired, false, toGccOrder(order), toGccOrder(failOrder));
        return expected;
    }
    T exchange(T desired, MemoryOrder order) {
        return __atomic_exchange_n(&this->value, desired, toGccOrder(order));
    }
    T fetchAdd(T operand, MemoryOrder order) {
        return __atomic_fetch_add(&this->value, operand, toGccOrder(order));
    }
    T fetchSub(T operand, MemoryOrder order) {
        return __atomic_fetch_sub(&this->value, operand, toGccOrder(order));
    }
    T fetchAnd(T operand, MemoryOrder order) {
        return __atomic_fetch_and(&this->value, operand, toGccOrder(order));
    }
    T fetchOr(T operand, MemoryOrder order) {
        return __atomic_fetch_or(&this->value, operand, toGccOrder(order));
    }
};

template <typename T>
class Atomic<T, 8> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 8>& other) : value{other.load(Relaxed)} {
    }
    // Hide operator=
    Atomic& operator=(T) = delete;
    // The copy assignment operator should only be called when there is no concurrent access to *this.
    Atomic& operator=(const Atomic& other) {
        this->value = other.load(Relaxed);
        return *this;
    }
    T load(MemoryOrder order) const {
        return __atomic_load_n(&this->value, toGccOrder(order));
    }
    void store(T value, MemoryOrder order) {
        __atomic_store_n(&this->value, value, toGccOrder(order));
    }
    T compareExchange(T expected, T desired, MemoryOrder order) {
        MemoryOrder failOrder = (order == AcqRel) ? Acquire : ((order == Release) ? Relaxed : order);
        __atomic_compare_exchange_n(&this->value, &expected, desired, false, toGccOrder(order), toGccOrder(failOrder));
        return expected;
    }
    T exchange(T desired, MemoryOrder order) {
        return __atomic_exchange_n(&this->value, desired, toGccOrder(order));
    }
    T fetchAdd(T operand, MemoryOrder order) {
        return __atomic_fetch_add(&this->value, operand, toGccOrder(order));
    }
    T fetchSub(T operand, MemoryOrder order) {
        return __atomic_fetch_sub(&this->value, operand, toGccOrder(order));
    }
    T fetchAnd(T operand, MemoryOrder order) {
        return __atomic_fetch_and(&this->value, operand, toGccOrder(order));
    }
    T fetchOr(T operand, MemoryOrder order) {
        return __atomic_fetch_or(&this->value, operand, toGccOrder(order));
    }
};

#endif

//  ▄▄▄▄▄▄ ▄▄                              ▄▄ ▄▄                        ▄▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ██     ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██ ██    ██  ██ ██     ▄▄▄██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██ ▄██▄
//

// Used as the return value of Thread_Local::setInScope()
template <template <typename> class TL, typename T>
class ThreadLocalScope {
private:
    TL<T>* var;
    T oldValue;

public:
    ThreadLocalScope(TL<T>* var, T newValue) : var{var} {
        this->oldValue = var->load();
        var->store(newValue);
    }

    ThreadLocalScope(const ThreadLocalScope&) = delete;
    ThreadLocalScope(ThreadLocalScope&& other) {
        this->var = other.var;
        this->oldValue = std::move(other.oldValue);
        other.var = nullptr;
    }

    ~ThreadLocalScope() {
        if (this->var) {
            this->var->store(this->oldValue);
        }
    }
};

#if defined(PLY_WINDOWS)

//----------------------------------------------------
// Windows implementation.
template <typename T>
class ThreadLocal {
private:
    PLY_STATIC_ASSERT(sizeof(T) <= PLY_PTR_SIZE);
    DWORD m_tlsIndex;

public:
    ThreadLocal() {
        m_tlsIndex = TlsAlloc();
        PLY_ASSERT(m_tlsIndex != TLS_OUT_OF_INDEXES);
    }

    ThreadLocal(const ThreadLocal&) = delete;

    ~ThreadLocal() {
        BOOL rc = TlsFree(m_tlsIndex);
        PLY_ASSERT(rc != 0);
        PLY_UNUSED(rc);
    }

    template <typename U = T, std::enable_if_t<std::is_pointer<U>::value, int> = 0>
    U load() const {
        LPVOID value = TlsGetValue(m_tlsIndex);
        PLY_ASSERT(value != 0 || GetLastError() == ERROR_SUCCESS);
        return (T) value;
    }

    template <typename U = T, std::enable_if_t<std::is_enum<U>::value || std::is_integral<U>::value, int> = 0>
    U load() const {
        LPVOID value = TlsGetValue(m_tlsIndex);
        PLY_ASSERT(value != 0 || GetLastError() == ERROR_SUCCESS);
        return (T) (uptr) value;
    }

    void store(T value) {
        BOOL rc = TlsSetValue(m_tlsIndex, (LPVOID) value);
        PLY_ASSERT(rc != 0);
        PLY_UNUSED(rc);
    }

    // In C++11, you can write auto scope = myTlvar.setInScope(value);
    using Scope = ThreadLocalScope<ThreadLocal, T>;
    Scope setInScope(T value) {
        return {this, value};
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// POSIX implementation.
template <typename T>
class ThreadLocal {
private:
    PLY_STATIC_ASSERT(sizeof(T) <= PLY_PTR_SIZE);
    pthread_key_t m_tlsKey;

public:
    ThreadLocal() {
        int rc = pthread_key_create(&m_tlsKey, NULL);
        PLY_ASSERT(rc == 0);
        PLY_UNUSED(rc);
    }

    ThreadLocal(const ThreadLocal&) = delete;

    ~ThreadLocal() {
        int rc = pthread_key_delete(m_tlsKey);
        PLY_ASSERT(rc == 0);
        PLY_UNUSED(rc);
    }

    template <typename U = T, std::enable_if_t<std::is_pointer<U>::value, int> = 0>
    U load() const {
        void* value = pthread_getspecific(m_tlsKey);
        return (T) value;
    }

    template <typename U = T, std::enable_if_t<std::is_enum<U>::value || std::is_integral<U>::value, int> = 0>
    U load() const {
        void* value = pthread_getspecific(m_tlsKey);
        return (T) (uptr) value;
    }

    template <typename U = T, std::enable_if_t<std::is_enum<U>::value || std::is_integral<U>::value, int> = 0>
    void store(U value) {
        int rc = pthread_setspecific(m_tlsKey, (void*) (uptr) value);
        PLY_ASSERT(rc == 0);
        PLY_UNUSED(rc);
    }

    // In C++11, you can write auto scope = myTlvar.setInScope(value);
    using Scope = ThreadLocalScope<ThreadLocal, T>;
    Scope setInScope(T value) {
        return {this, value};
    }
};

#endif

//  ▄▄   ▄▄         ▄▄
//  ███▄███ ▄▄  ▄▄ ▄██▄▄  ▄▄▄▄  ▄▄  ▄▄
//  ██▀█▀██ ██  ██  ██   ██▄▄██  ▀██▀
//  ██   ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄  ▄█▀▀█▄
//

#if defined(PLY_WINDOWS)

//----------------------------------------------------
// Windows implementation.
class Mutex {
private:
    SRWLOCK srwlock;
    friend class ConditionVariable;

public:
    Mutex() {
        InitializeSRWLock(&srwlock);
    }
    void lock() {
        AcquireSRWLockExclusive(&srwlock);
    }
    bool tryLock() {
        return TryAcquireSRWLockExclusive(&srwlock) != 0;
    }
    void unlock() {
        ReleaseSRWLockExclusive(&srwlock);
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// POSIX implementation.
class Mutex {
private:
    pthread_mutex_t mutex;
    friend class ConditionVariable;

public:
    Mutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    ~Mutex() {
        pthread_mutex_destroy(&mutex);
    }
    void lock() {
        pthread_mutex_lock(&mutex);
    }
    bool tryLock() {
        return pthread_mutex_trylock(&mutex) == 0;
    }
    void unlock() {
        pthread_mutex_unlock(&mutex);
    }
};

#endif

//  ▄▄                 ▄▄      ▄▄▄▄                           ▄▄
//  ██     ▄▄▄▄   ▄▄▄▄ ██  ▄▄ ██  ▀▀ ▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██
//  ██    ██  ██ ██    ██▄█▀  ██ ▀██ ██  ██  ▄▄▄██ ██  ▀▀ ██  ██
//  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄ ▀█▄▄██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄██
//

template <typename MutexType>
struct LockGuard {
    MutexType& mutex;

    LockGuard(MutexType& mutex) : mutex{mutex} {
        this->mutex.lock();
    }
    ~LockGuard() {
        this->mutex.unlock();
    }
};

//   ▄▄▄▄                    ▄▄ ▄▄  ▄▄   ▄▄               ▄▄   ▄▄               ▄▄        ▄▄     ▄▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██ ▄▄ ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄  ██   ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ██▄▄▄   ██   ▄▄▄▄
//  ██     ██  ██ ██  ██ ██  ██ ██  ██   ██ ██  ██ ██  ██  ██ ██   ▄▄▄██ ██  ▀▀ ██  ▄▄▄██ ██  ██  ██  ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██  ██ ▀█▄▄██ ██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██   ▀█▀   ▀█▄▄██ ██     ██ ▀█▄▄██ ██▄▄█▀ ▄██▄ ▀█▄▄▄
//

#if defined(PLY_WINDOWS)

//----------------------------------------------------
// Windows implementation.
class ConditionVariable {
private:
    CONDITION_VARIABLE condVar;

public:
    ConditionVariable() {
        InitializeConditionVariable(&condVar);
    }
    void wait(LockGuard<Mutex>& lockGuard) {
        SleepConditionVariableSRW(&condVar, &lockGuard.mutex.srwlock, INFINITE, 0);
    }
    void timedWait(LockGuard<Mutex>& lockGuard, u32 waitMillis) {
        if (waitMillis > 0) {
            SleepConditionVariableSRW(&condVar, &lockGuard.mutex.srwlock, waitMillis, 0);
        }
    }
    void wakeOne() {
        WakeConditionVariable(&condVar);
    }
    void wakeAll() {
        WakeAllConditionVariable(&condVar);
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// POSIX implementation.
class ConditionVariable {
private:
    pthread_cond_t cond;

public:
    ConditionVariable() {
        pthread_cond_init(&cond, NULL);
    }
    ~ConditionVariable() {
        pthread_cond_destroy(&cond);
    }
    void wait(LockGuard<Mutex>& lockGuard) {
        pthread_cond_wait(&cond, &lockGuard.mutex.mutex);
    }
    void timedWait(LockGuard<Mutex>& lockGuard, u32 waitMillis) {
        if (waitMillis > 0) {
            struct timespec ts;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec + waitMillis / 1000;
            ts.tv_nsec = (tv.tv_usec + (waitMillis % 1000) * 1000) * 1000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&cond, &lockGuard.mutex.mutex, &ts);
        }
    }
    void wakeOne() {
        pthread_cond_signal(&cond);
    }
    void wakeAll() {
        pthread_cond_broadcast(&cond);
    }
};

#endif

//  ▄▄▄▄▄                    ▄▄ ▄▄    ▄▄        ▄▄  ▄▄          ▄▄                 ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄  ██     ▄▄▄▄   ▄▄▄▄ ██  ▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██ ██    ██  ██ ██    ██▄█▀
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██  ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄
//

#if defined(PLY_WINDOWS)

//----------------------------------------------------
// Windows implementation.
struct ReadWriteLock {
    SRWLOCK srwLock;

    ReadWriteLock() {
        InitializeSRWLock(&this->srwLock);
    }
    ~ReadWriteLock() {
        // SRW locks do not need to be destroyed.
    }
    void lockExclusive() {
        AcquireSRWLockExclusive(&this->srwLock);
    }
    void unlockExclusive() {
        ReleaseSRWLockExclusive(&this->srwLock);
    }
    void lockShared() {
        AcquireSRWLockShared(&this->srwLock);
    }
    void unlockShared() {
        ReleaseSRWLockShared(&this->srwLock);
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// POSIX implementation.
struct ReadWriteLock {
    pthread_rwlock_t rwLock;

    ReadWriteLock() {
        pthread_rwlock_init(&this->rwLock, NULL);
    }
    ~ReadWriteLock() {
        pthread_rwlock_destroy(&this->rwLock);
    }
    void lockExclusive() {
        pthread_rwlock_wrlock(&this->rwLock);
    }
    void unlockExclusive() {
        pthread_rwlock_unlock(&this->rwLock);
    }
    void lockShared() {
        pthread_rwlock_rdlock(&this->rwLock);
    }
    void unlockShared() {
        pthread_rwlock_unlock(&this->rwLock);
    }
};

#endif

//   ▄▄▄▄                                ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██▄▄██ ██ ██ ██  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██  ▀▀ ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄▄  ██ ██ ██ ▀█▄▄██ ██▄▄█▀ ██  ██ ▀█▄▄█▀ ██     ▀█▄▄▄
//                                ██

#if defined(PLY_WINDOWS)

//----------------------------------------------------
// Windows implementation.
struct Semaphore {
    HANDLE sema;

    Semaphore() {
        this->sema = CreateSemaphore(NULL, 0, INT32_MAX, NULL);
    }
    ~Semaphore() {
        CloseHandle(this->sema);
    }
    void wait() {
        WaitForSingleObject(this->sema, INFINITE);
    }
    void signal(u32 count = 1) {
        ReleaseSemaphore(this->sema, (DWORD) count, NULL);
    }
};

#elif defined(PLY_APPLE)

//----------------------------------------------------
// macOS & iOS implementation.
struct Semaphore {
    semaphore_t sema;

    Semaphore() {
        semaphore_create(mach_task_self(), &this->sema, SYNC_POLICY_FIFO, 0);
    }
    ~Semaphore() {
        semaphore_destroy(mach_task_self(), this->sema);
    }
    void wait() {
        semaphore_wait(this->sema);
    }
    void signal(u32 count = 1) {
        while (count-- > 0) {
            semaphore_signal(this->sema);
        }
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// Other POSIX platforms implementation.
struct Semaphore {
    sem_t sema;

    Semaphore() {
        sem_init(&this->sema, 0, 0);
    }

    ~Semaphore() {
        sem_destroy(&this->sema);
    }
    void wait() {
        int rc;
        do {
            rc = sem_wait(&this->sema);
        } while (rc == -1 && errno == EINTR);
    }
    void signal(u32 count = 1) {
        while (count-- > 0) {
            sem_post(&this->sema);
        }
    }
};

#endif

//  ▄▄   ▄▄ ▄▄         ▄▄                 ▄▄▄  ▄▄   ▄▄
//  ██   ██ ▄▄ ▄▄▄▄▄  ▄██▄▄ ▄▄  ▄▄  ▄▄▄▄   ██  ███▄███  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄
//   ██ ██  ██ ██  ▀▀  ██   ██  ██  ▄▄▄██  ██  ██▀█▀██ ██▄▄██ ██ ██ ██ ██  ██ ██  ▀▀ ██  ██
//    ▀█▀   ██ ██      ▀█▄▄ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██   ██ ▀█▄▄▄  ██ ██ ██ ▀█▄▄█▀ ██     ▀█▄▄██
//                                                                                    ▄▄▄█▀

struct VirtualMemory {
    // Usage stats
    // The current total amount of address space that was reserved using allocRegion or reserveRegion
    static Atomic<uptr> totalReservedBytes;
    // The current total amount of memory that was committed using allocRegion or commitPages
    static Atomic<uptr> totalCommittedBytes;

    // Returned by getProperties()
    struct Properties {
        uptr regionAlignment = 0; // reserve/allocRegion sizes must be a multiple of this
        uptr pageSize = 0;        // commitPages sizes must be a multiple of this
    };

    // Returned by getSystemStats()
    struct SystemStats {
#if defined(PLY_WINDOWS)
        // System-specific stats reported by GetProcessMemoryInfo
        uptr privateUsage = 0;
        uptr workingSetSize = 0;
#else
        // System-specific stats reported by task_info (Apple platforms) or /proc/self/statm (Linux)
        uptr virtualSize = 0;
        uptr residentSize = 0;
#endif
    };

    //----------------------------------------------------
    // System information
    //----------------------------------------------------
    static Properties getProperties();
    static SystemStats getSystemStats();

    //----------------------------------------------------
    // Managing pages
    //----------------------------------------------------
    // Reserves a region of address space. Memory pages are initially uncommitted. Returns nullptr on failure. numBytes
    // must be a multiple of regionAlignment.
    static void* reserveRegion(uptr numBytes);
    // Unreserves a region of address space. numReservedBytes must match the argument passed to to reserveRegion.
    // Caller is responsible for passing the correct numCommittedBytes, otherwise stats will get out of sync.
    static void unreserveRegion(void* addr, uptr numReservedBytes, uptr numCommittedBytes);
    // Commits a subregion of reserved address space, making it legal to read and write to the subregion.
    // addr must be aligned to pageSize and numBytes must be a multiple of pageSize.
    static void commitPages(void* addr, uptr numBytes);
    // Decommits a subregion of previously committed memory.
    // addr must be aligned to pageSize and numBytes must be a multiple of pageSize.
    static void decommitPages(void* addr, uptr numBytes);

    //----------------------------------------------------
    // Allocating large blocks
    //----------------------------------------------------
    // Reserves and commits a region of address space. Returns nullptr on failure. Free using freeRegion. Don't
    // decommit any pages in the returned region, otherwise stats will get out of sync. numBytes must be a multiple of
    // regionAlignment.
    static void* allocRegion(uptr numBytes);
    // Decommits and unreserves a region of address space. numBytes must match the argument passed to allocRegion.
    static void freeRegion(void* addr, uptr numBytes);
};

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

} // namespace ply

namespace ply {

struct Heap {
    struct Stats {
        uptr totalBytesConsumed = 0;
        uptr totalSystemMemoryUsed = 0;
    };

    static void* alloc(uptr numBytes);
    static void* realloc(void* ptr, uptr numBytes);
    static void free(void* ptr);
    static void* allocAligned(uptr numBytes, u32 alignment);

    // Sets a callback that will be invoked whenever a heap allocation fails.
    static void setOutOfMemoryHandler(Functor<void()> handler);
    // Returns current heap allocation counters and system-memory usage totals.
    static Stats getStats();
    // Validates internal heap invariants in assert-enabled builds.
    static void validate();

    // Perfect forwarding
    template <typename T, typename... Args>
    static T* create(Args&&... args) {
        T* obj = (T*) alloc(sizeof(T));
        new (obj) T{std::forward<Args>(args)...};
        return obj;
    }

    template <typename T>
    static void destroy(T* obj) {
        if (obj) {
            obj->~T();
            free(obj);
        }
    }

private:
    static Functor<void()> outOfMemoryHandler;
};

#if defined(PLY_WINDOWS)

inline void Thread::run(Functor<void()>&& entry) {
    PLY_ASSERT(this->handle == INVALID_HANDLE_VALUE);
    auto* functor = Heap::create<Functor<void()>>(std::move(entry));
    this->handle = CreateThread(NULL, 0, threadEntry, functor, 0, NULL);
}

#elif defined(PLY_POSIX)

inline void Thread::run(Functor<void()>&& entry) {
    PLY_ASSERT(!this->attached);
    auto* functor = Heap::create<Functor<void()>>(std::move(entry));
    pthread_create(&this->handle, NULL, threadEntry, functor);
    this->attached = true;
}

#endif

//   ▄▄▄▄   ▄▄          ▄▄               ▄▄   ▄▄ ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄ ██   ██ ▄▄  ▄▄▄▄  ▄▄    ▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██  ██ ██  ██ ██▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██   ▀█▀   ██ ▀█▄▄▄   ██▀▀██
//                                 ▄▄▄█▀

class String;
template <typename>
class ArrayView;
template <typename>
class Array;

inline bool isWhite(char c) {
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v');
}
inline bool isAlpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
inline bool isDigit(char c) {
    return (c >= '0' && c <= '9');
}

class StringView {
private:
    const char* bytes_ = nullptr;
    u32 numBytes_ = 0;

public:
    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    StringView() = default;
    StringView(const char* s) : bytes_{s}, numBytes_{numericCast<u32>(::strlen(s))} {
    }
    StringView(const char* bytes, u32 numBytes) : bytes_{bytes}, numBytes_{numBytes} {
    }
    StringView(const char* startByte, const char* endByte)
        : bytes_{startByte}, numBytes_{numericCast<u32>(endByte - startByte)} {
    }
    StringView(const char& c) : bytes_{&c}, numBytes_{1} {
    }

    //----------------------------------------------------
    // Accessing string bytes
    //----------------------------------------------------

    const char* bytes() const {
        return this->bytes_;
    }
    char* bytes() {
        return const_cast<char*>(this->bytes_);
    }
    u32 numBytes() const {
        return this->numBytes_;
    }
    const char& operator[](u32 index) const {
        PLY_ASSERT(index < this->numBytes_);
        return this->bytes_[index];
    }
    const char& back(s32 ofs = -1) const {
        PLY_ASSERT(u32(-ofs - 1) < this->numBytes_);
        return this->bytes_[this->numBytes_ + ofs];
    }
    char* begin() {
        return const_cast<char*>(this->bytes_);
    }
    const char* begin() const {
        return this->bytes_;
    }
    char* end() {
        return const_cast<char*>(this->bytes_) + this->numBytes_;
    }
    const char* end() const {
        return this->bytes_ + this->numBytes_;
    }

    //----------------------------------------------------
    // Examining string contents
    //----------------------------------------------------

    bool isEmpty() const {
        return this->numBytes_ == 0;
    }
    explicit operator bool() const {
        return this->numBytes_ != 0;
    }
    bool startsWith(StringView arg) const;
    bool endsWith(StringView arg) const;
    s32 find(StringView pattern, u32 startPos = 0) const;
    template <typename Callable, PLY_ENABLE_IF_WELL_FORMED(declval<Callable>()(declval<char>()))>
    s32 find(const Callable& matchFunc, u32 startPos = 0) const {
        for (u32 i = startPos; i < this->numBytes_; i++) {
            if (matchFunc(this->bytes_[i]))
                return i;
        }
        return -1;
    }
    s32 reverseFind(StringView pattern, s32 startPos = -1) const;
    template <typename Callable, PLY_ENABLE_IF_WELL_FORMED(declval<Callable>()(declval<char>()))>
    s32 reverseFind(const Callable& matchFunc, s32 startPos = -1) const {
        if (startPos < 0) {
            startPos += this->numBytes_;
        }
        for (s32 i = startPos; i >= 0; i--) {
            if (matchFunc(this->bytes_[i]))
                return i;
        }
        return -1;
    }

    //----------------------------------------------------
    // Creating subviews
    //----------------------------------------------------

    StringView substr(u32 start) const {
        start = min(start, this->numBytes_);
        return {this->bytes_ + start, this->numBytes_ - start};
    }
    StringView substr(u32 start, u32 numBytes) const {
        start = min(start, this->numBytes_);
        numBytes = min(numBytes, this->numBytes_ - start);
        return {this->bytes_ + start, numBytes};
    }
    StringView left(u32 numBytes) const {
        numBytes = min(numBytes, this->numBytes_);
        return {this->bytes_, numBytes};
    }
    StringView shortenedBy(u32 numBytes) const {
        numBytes = min(numBytes, this->numBytes_);
        return {this->bytes_, this->numBytes_ - numBytes};
    }
    StringView right(u32 numBytes) const {
        numBytes = min(numBytes, this->numBytes_);
        return {this->bytes_ + this->numBytes_ - numBytes, numBytes};
    }
    StringView trim(bool (*matchFunc)(char) = isWhite, bool left = true, bool right = true) const;
    StringView trimLeft(bool (*matchFunc)(char) = isWhite) const {
        return this->trim(matchFunc, true, false);
    }
    StringView trimRight(bool (*matchFunc)(char) = isWhite) const {
        return this->trim(matchFunc, false, true);
    }

    //----------------------------------------------------
    // Creating new strings
    //----------------------------------------------------

    PLY_NO_DISCARD String upper() const;
    PLY_NO_DISCARD String lower() const;
    PLY_NO_DISCARD Array<StringView> split(StringView separator) const;
    PLY_NO_DISCARD String join(ArrayView<const StringView> comps) const;
    PLY_NO_DISCARD String replace(StringView oldSubstr, StringView newSubstr) const;

    //----------------------------------------------------
    // Pattern matching
    //----------------------------------------------------

    template <typename... Args>
    bool match(StringView pattern, Args*... args) const;
};

s32 compare(StringView a, StringView b);
inline bool operator==(StringView a, StringView b) {
    return compare(a, b) == 0;
}
inline bool operator!=(StringView a, StringView b) {
    return compare(a, b) != 0;
}
inline bool operator<(StringView a, StringView b) {
    return compare(a, b) < 0;
}
inline bool operator<=(StringView a, StringView b) {
    return compare(a, b) <= 0;
}
inline bool operator>(StringView a, StringView b) {
    return compare(a, b) > 0;
}
inline bool operator>=(StringView a, StringView b) {
    return compare(a, b) >= 0;
}
String operator+(StringView a, StringView b);
String operator*(StringView str, u32 count);

//  ▄▄   ▄▄         ▄▄    ▄▄▄▄   ▄▄          ▄▄               ▄▄   ▄▄ ▄▄
//  ███▄███ ▄▄  ▄▄ ▄██▄▄ ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄ ██   ██ ▄▄  ▄▄▄▄  ▄▄    ▄▄
//  ██▀█▀██ ██  ██  ██    ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██  ██ ██  ██ ██▄▄██ ██ ██ ██
//  ██   ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██   ▀█▀   ██ ▀█▄▄▄   ██▀▀██
//                                                      ▄▄▄█▀

struct MutStringView {
    char* bytes = nullptr;
    u32 numBytes = 0;

    MutStringView() = default;
    MutStringView(char* bytes, u32 numBytes) : bytes{bytes}, numBytes{numBytes} {
    }
    MutStringView(char* startByte, char* endByte) : bytes{startByte}, numBytes{numericCast<u32>(endByte - startByte)} {
    }
    operator const StringView&() const {
        return reinterpret_cast<const StringView&>(*this);
    }

    char* end() {
        return this->bytes + this->numBytes;
    }
    MutStringView subview(u32 numBytes) const {
        PLY_ASSERT(numBytes <= this->numBytes);
        return {this->bytes + numBytes, this->numBytes - numBytes};
    }
    MutStringView shortenedBy(s32 ofs) {
        PLY_ASSERT((u32) -ofs <= this->numBytes);
        return {this->bytes, this->numBytes += ofs};
    }
};

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

class String {
    char* bytes_ = nullptr;
    u32 numBytes_ = 0;

public:
    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    String() = default;
    String(const String& other) : String{StringView{other}} {
    }
    String(String&& other) : bytes_{other.bytes_}, numBytes_{other.numBytes_} {
        other.bytes_ = nullptr;
        other.numBytes_ = 0;
    }
    String(StringView other);
    String(const char* s) : String{StringView{s}} { // Needed?
    }
    ~String() {
        if (this->bytes_) {
            Heap::free(this->bytes_);
        }
    }

    //----------------------------------------------------
    // Assignment operators
    //----------------------------------------------------

    String& operator=(const String& other) {
        this->~String();
        new (this) String{other};
        return *this;
    }
    String& operator=(String&& other) {
        this->~String();
        new (this) String{std::move(other)};
        return *this;
    }

    //----------------------------------------------------
    // Type conversions
    //----------------------------------------------------

    operator StringView() const {
        return {this->bytes_, this->numBytes_};
    }

    //----------------------------------------------------
    // Accessing string bytes
    //----------------------------------------------------

    const char* bytes() const {
        return this->bytes_;
    }
    char* bytes() {
        return this->bytes_;
    }
    u32 numBytes() const {
        return this->numBytes_;
    }
    const char& operator[](u32 index) const {
        PLY_ASSERT(index < this->numBytes_);
        return this->bytes_[index];
    }
    char& operator[](u32 index) {
        PLY_ASSERT(index < this->numBytes_);
        return this->bytes_[index];
    }
    const char& back(s32 ofs = -1) const {
        PLY_ASSERT(u32(-ofs - 1) < this->numBytes_);
        return this->bytes_[this->numBytes_ + ofs];
    }
    char& back(s32 ofs = -1) {
        PLY_ASSERT(u32(-ofs - 1) < this->numBytes_);
        return this->bytes_[this->numBytes_ + ofs];
    }
    char* begin() {
        return this->bytes_;
    }
    const char* begin() const {
        return this->bytes_;
    }
    char* end() {
        return this->bytes_ + this->numBytes_;
    }
    const char* end() const {
        return this->bytes_ + this->numBytes_;
    }

    //----------------------------------------------------
    // Examining string contents
    //----------------------------------------------------

    bool isEmpty() const {
        return this->numBytes_ == 0;
    }
    explicit operator bool() const {
        return this->numBytes_ != 0;
    }
    bool startsWith(StringView arg) const {
        return ((StringView) * this).startsWith(arg);
    }
    bool endsWith(StringView arg) const {
        return ((StringView) * this).endsWith(arg);
    }
    s32 find(StringView pattern, u32 startPos = 0) const {
        return ((StringView) * this).find(pattern, startPos);
    }
    template <typename Callable>
    s32 find(const Callable& matchFunc, u32 startPos = 0) const {
        return ((StringView) * this).find(matchFunc, startPos);
    }
    s32 reverseFind(StringView pattern, s32 startPos = -1) const {
        return ((StringView) * this).reverseFind(pattern, startPos);
    }
    template <typename Callable>
    s32 reverseFind(const Callable& matchFunc, s32 startPos = -1) const {
        return ((StringView) * this).reverseFind(matchFunc, startPos);
    }

    //----------------------------------------------------
    // Creating subviews
    //----------------------------------------------------

    StringView substr(u32 start) const {
        PLY_ASSERT(start <= this->numBytes_);
        return {this->bytes_ + start, this->numBytes_ - start};
    }
    StringView substr(u32 start, u32 numBytes) const {
        PLY_ASSERT(start <= this->numBytes_);
        PLY_ASSERT(start + numBytes <= this->numBytes_);
        return {this->bytes_ + start, numBytes};
    }
    StringView left(u32 numBytes) const {
        PLY_ASSERT(numBytes <= this->numBytes_);
        return {this->bytes_, numBytes};
    }
    StringView shortenedBy(u32 numBytes) const {
        PLY_ASSERT(numBytes <= this->numBytes_);
        return {this->bytes_, this->numBytes_ - numBytes};
    }
    StringView right(u32 numBytes) const {
        PLY_ASSERT(numBytes <= this->numBytes_);
        return {this->bytes_ + this->numBytes_ - numBytes, numBytes};
    }
    StringView trim(bool (*matchFunc)(char) = isWhite, bool left = true, bool right = true) const {
        return ((StringView) * this).trim(matchFunc, left, right);
    }
    StringView trimLeft(bool (*matchFunc)(char) = isWhite) const {
        return ((StringView) * this).trim(matchFunc, true, false);
    }
    StringView trimRight(bool (*matchFunc)(char) = isWhite) const {
        return ((StringView) * this).trim(matchFunc, false, true);
    }

    //----------------------------------------------------
    // Creating new strings
    //----------------------------------------------------

    PLY_NO_DISCARD String upper() const {
        return ((StringView) * this).upper();
    }
    PLY_NO_DISCARD String lower() const {
        return ((StringView) * this).lower();
    }
    PLY_NO_DISCARD Array<StringView> split(StringView separator) const;
    PLY_NO_DISCARD String join(ArrayView<const StringView> comps) const;
    PLY_NO_DISCARD String replace(StringView oldSubstr, StringView newSubstr) const {
        return ((StringView) * this).replace(oldSubstr, newSubstr);
    }
    static String allocate(u32 numBytes);
    static String adopt(char* bytes, u32 numBytes) {
        String str;
        str.bytes_ = bytes;
        str.numBytes_ = numBytes;
        return str;
    }

    //----------------------------------------------------
    // Pattern matching
    //----------------------------------------------------

    template <typename... Args>
    bool match(StringView pattern, Args*... args) const;

    //----------------------------------------------------
    // Modifying string contents
    //----------------------------------------------------

    void clear() {
        if (this->bytes_) {
            Heap::free(this->bytes_);
        }
        this->bytes_ = nullptr;
        this->numBytes_ = 0;
    }
    void operator+=(StringView other) {
        *this = *this + other;
    }
    void resize(u32 numBytes);
    char* release() {
        char* r = this->bytes_;
        this->bytes_ = nullptr;
        this->numBytes_ = 0;
        return r;
    }

    //----------------------------------------------------
    // Formatting
    //----------------------------------------------------

    template <typename... Args>
    static String format(StringView fmt, const Args&... args);
    static String fromDateTime(StringView format, const DateTime& dateTime);
};

inline StringView getAnyLookupKey(const String& str) {
    return str;
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

// shuffleBits is a helper function that takes a 32-bit or 64-bit integer and mixes the bits.
// The implementation is taken from MurmurHash3's fmix32 and fmix64 functions:
// https://github.com/aappleby/smhasher/blob/0ff96f7835817a27d0487325b6c16033e2992eb5/src/MurmurHash3.cpp#L68-L90.

inline u32 shuffleBits(u32 h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline u32 unshuffleBits(u32 h) {
    h ^= h >> 16;
    h *= 0x7ed1b41d;
    h ^= (h ^ (h >> 13)) >> 13;
    h *= 0xa5cb9243;
    h ^= h >> 16;
    return h;
}

inline u64 shuffleBits(u64 h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

inline u64 unshuffleBits(u64 h) {
    h ^= h >> 33;
    h *= 0x9cb4b2f8129337db;
    h ^= h >> 33;
    h *= 0x4f74430c22a54005;
    h ^= h >> 33;
    return h;
}

//----------------------------------------------------
// HashBuilder is a helper class used to calculate hash values for aggregate data types.
// It uses MurmurHash3's hashing algorithm.

struct HashBuilder {
    u32 accumulator = 0;

    u32 getResult() const {
        return shuffleBits(this->accumulator);
    }
};

void addToHash(HashBuilder& builder, u32 value);
inline void addToHash(HashBuilder& builder, u8 value) {
    addToHash(builder, (u32) value);
}
inline void addToHash(HashBuilder& builder, u16 value) {
    addToHash(builder, (u32) value);
}
inline void addToHash(HashBuilder& builder, u64 value) {
    addToHash(builder, (u32) value);
    addToHash(builder, (u32) (value >> 32));
}
inline void addToHash(HashBuilder& builder, s8 value) {
    addToHash(builder, (u32) value);
}
inline void addToHash(HashBuilder& builder, s16 value) {
    addToHash(builder, (u32) value);
}
inline void addToHash(HashBuilder& builder, s32 value) {
    addToHash(builder, (u32) value);
}
inline void addToHash(HashBuilder& builder, s64 value) {
    addToHash(builder, (u64) value);
}
inline void addToHash(HashBuilder& builder, float value) {
    PLY_PUN_GUARD;
    addToHash(builder, *(u32*) &value);
}
inline void addToHash(HashBuilder& builder, double value) {
    PLY_PUN_GUARD;
    addToHash(builder, *(u64*) &value);
}
void addToHash(HashBuilder& builder, StringView str);

//----------------------------------------------------
// calculateHash() is a wrapper around HashBuilder that's used internally by Map and Set.

template <typename T>
u32 calculateHash(const T& item) {
    HashBuilder visitor;
    addToHash(visitor, item);
    return visitor.getResult();
}

// Specialize calculateHash() for pointers, u32 and u64.
// These specializations don't use Hash_Calculator; they just call shuffleBits directly.
template <typename T>
inline u32 calculateHash(T* item) {
    return (u32) shuffleBits((uptr) item);
}
inline u32 calculateHash(u32 item) {
    return shuffleBits(item);
}
inline u32 calculateHash(u64 item) {
    return (u32) shuffleBits(item);
}

//   ▄▄▄▄                              ▄▄   ▄▄ ▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄ ██   ██ ▄▄  ▄▄▄▄  ▄▄    ▄▄
//  ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██  ██ ██  ██ ██▄▄██ ██ ██ ██
//  ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██   ▀█▀   ██ ▀█▄▄▄   ██▀▀██
//                               ▄▄▄█▀

template <typename Item>
class ArrayView {
private:
    Item* items_ = nullptr;
    u32 numItems_ = 0;

public:
    // Constructors
    ArrayView() = default;
    ArrayView(Item* items, u32 numItems) : items_{items}, numItems_{numItems} {
    }
    template <typename U = Item, std::enable_if_t<std::is_const<U>::value, int> = 0>
    ArrayView(std::initializer_list<Item> initList)
        : items_{initList.begin()}, numItems_{numericCast<u32>(initList.size())} {
        PLY_ASSERT((uptr) initList.end() - (uptr) initList.begin() == sizeof(Item) * initList.size());
    }
    template <u32 N>
    ArrayView(Item (&s)[N]) : items_{s}, numItems_{N} {
    }

    Item& operator[](u32 index) & {
        PLY_ASSERT(index < this->numItems_);
        return this->items_[index];
    }
    Item&& operator[](u32 index) && {
        PLY_ASSERT(index < this->numItems_);
        return std::move(this->items_[index]);
    }
    const Item& operator[](u32 index) const& {
        PLY_ASSERT(index < this->numItems_);
        return this->items_[index];
    }
    Item& back(s32 offset = -1) & {
        PLY_ASSERT(u32(this->numItems_ + offset) < this->numItems_);
        return this->items_[this->numItems_ + offset];
    }
    Item&& back(s32 offset = -1) && {
        PLY_ASSERT(u32(this->numItems_ + offset) < this->numItems_);
        return std::move(this->items_[this->numItems_ + offset]);
    }
    const Item& back(s32 offset = -1) const& {
        PLY_ASSERT(u32(this->numItems_ + offset) < this->numItems_);
        return this->items_[this->numItems_ + offset];
    }
    Item* items() {
        return this->items_;
    }
    const Item* items() const {
        return this->items_;
    }
    u32 numItems() const {
        return numItems_;
    }
    bool isEmpty() const {
        return this->numItems_ == 0;
    }
    explicit operator bool() const {
        return this->numItems_ > 0;
    }
    operator ArrayView<const Item>() const {
        return {this->items_, this->numItems_};
    }
    static ArrayView<const Item> from(StringView view) {
        u32 numItems = view.numBytes() / sizeof(Item); // Divide by constant is fast
        return {(const Item*) view.bytes(), numItems};
    }
    StringView stringView() const {
        return {(const char*) this->items_, numericCast<u32>(this->numItems_ * sizeof(Item))};
    }
    static ArrayView<Item> from(MutStringView view) {
        u32 numItems = view.numBytes / sizeof(Item); // Divide by constant is fast
        return {(Item*) view.bytes, numItems};
    }
    MutStringView mutStringView() {
        return {(char*) this->items_, numericCast<u32>(this->numItems_ * sizeof(Item))};
    }
    ArrayView subview(u32 start) const {
        PLY_ASSERT(start <= numItems_);
        return {this->items_ + start, numItems_ - start};
    }
    ArrayView subview(u32 start, u32 numItems) const {
        PLY_ASSERT(start <= this->numItems_); // FIXME: Support different end parameters
        PLY_ASSERT(start + numItems <= this->numItems_);
        return {this->items_ + start, numItems};
    }
    ArrayView shortenedBy(u32 numItems) const {
        PLY_ASSERT(numItems <= this->numItems_);
        return {this->items_, this->numItems_ - numItems};
    }
    Item* begin() const {
        return this->items_;
    }
    Item* end() const {
        return this->items_ + this->numItems_;
    }
};

inline String String::join(ArrayView<const StringView> comps) const {
    return ((StringView) * this).join(comps);
}

template <typename>
struct ArrayTraits;
template <typename Item_>
struct ArrayTraits<ArrayView<Item_>> {
    using Item = Item_;
};
template <typename Item_, size_t N>
struct ArrayTraits<Item_[N]> {
    using Item = Item_;
};
template <typename Arr>
using ArrayItemType = typename ArrayTraits<std::remove_reference_t<Arr>>::Item;

#define PLY_ENABLE_IF_ARRAY_TYPE(Arr) typename enableIfType<ArrayItemType<Arr>>::type = 0

template <typename Arr0, typename Arr1, PLY_ENABLE_IF_ARRAY_TYPE(Arr0), PLY_ENABLE_IF_ARRAY_TYPE(Arr1)>
bool operator==(const Arr0& a, const Arr1& b) {
    if (a.numItems() != b.numItems())
        return false;
    for (u32 i = 0; i < a.numItems(); i++) {
        if (!(a[i] == b[i]))
            return false;
    }
    return true;
}

//   ▄▄▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄
//  ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██
//  ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██
//                               ▄▄▄█▀

template <typename Item>
class Array {
private:
    Item* items_ = nullptr;
    u32 numItems_ = 0;
    u32 allocated = 0;

    // Make all other Array specializations friend classes.
    template <typename>
    friend class Array;

    void alloc(u32 numItems) {
        PLY_ASSERT(!this->items_);
        this->allocated = roundUpToNearestPowerOf2(numItems);
        this->items_ = (Item*) Heap::alloc(uptr(this->allocated) * sizeof(Item));
        this->numItems_ = numItems;
    }

public:
    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    Array() = default;
    // Copy constructor.
    Array(const Array<Item>& otherArray) {
        this->alloc(otherArray.numItems_);
        for (u32 i = 0; i < otherArray.numItems_; i++) {
            new (&this->items_[i]) Item{otherArray.items_[i]};
        }
    }
    // Move constructor.
    Array(Array<Item>&& otherArray)
        : items_{otherArray.items_}, numItems_{otherArray.numItems_}, allocated{otherArray.allocated} {
        new (&otherArray) Array<Item>;
    }
    // Construct from any compatible array.
    template <typename T, PLY_ENABLE_IF_WELL_FORMED(ArrayView<const Item>(declval<T>()))>
    Array(T&& otherArray) {
        u32 numOtherItems = ArrayView<const Item>{otherArray}.numItems();
        this->alloc(numOtherItems);
        for (u32 i = 0; i < numOtherItems; i++) {
            new ((Item*) this->items_ + i) Item{std::forward<T>(otherArray)[i]};
        }
    }
    // Construct from initializer list.
    Array(std::initializer_list<Item> initList) {
        u32 initSize = numericCast<u32>(initList.size());
        this->alloc(initSize);
        const Item* src = initList.begin();
        for (u32 i = 0; i < initSize; i++) {
            new ((Item*) this->items_ + i) Item{src[i]};
        }
    }
    // Destructor.
    ~Array() {
        for (u32 i = 0; i < this->numItems_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        Heap::free(this->items_);
    }
    // Adopt an array from a raw pointer.
    static Array<Item> adopt(Item* items, u32 numItems) {
        return {items, numItems, numItems};
    }

    //----------------------------------------------------
    // Assignment operators
    //----------------------------------------------------

    // Copy assignment operator.
    Array& operator=(const Array<Item>& other) {
        if (this != &other) {
            this->~Array();
            new (this) Array{other};
        }
        return *this;
    }
    // Move assignment operator.
    Array& operator=(Array<Item>&& other) {
        if (this != &other) {
            this->~Array();
            new (this) Array{std::move(other)};
        }
        return *this;
    }
    // Assign from any compatible array.
    template <typename Other, PLY_ENABLE_IF_WELL_FORMED(ArrayView<const Item>(declval<Other>()))>
    Array& operator=(Other&& other) {
        Array<Item> arrayToFree{std::move(*this)};
        new (this) Array{std::forward<Other>(other)};
        return *this;
    }
    // Assign from initializer list.
    Array& operator=(std::initializer_list<Item> initList) {
        for (u32 i = 0; i < this->numItems_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        u32 initSize = numericCast<u32>(initList.size());
        this->alloc(initSize);
        const Item* src = initList.begin();
        for (u32 i = 0; i < initSize; i++) {
            new (&this->items_[i]) Item{src[i]};
        }
        return *this;
    }
    // Extend from array with move semantics.
    Array& operator+=(Array<Item>&& otherArray) {
        u32 numOtherItems = ArrayView<const Item>{otherArray}.numItems();
        this->reserve(this->numItems_ + numOtherItems);
        for (u32 i = 0; i < numOtherItems; i++) {
            new ((Item*) this->items_ + (this->numItems_ + i)) Item{std::move(otherArray[i])};
        }
        this->numItems_ += numOtherItems;
        return *this;
    }
    // Extend from any compatible array with copy semantics.
    template <typename Other, PLY_ENABLE_IF_WELL_FORMED(ArrayView<const Item>{declval<Other>()})>
    Array& operator+=(Other && other) {
        u32 numOtherItems = ArrayView<const Item>{other}.numItems();
        this->reserve(this->numItems_ + numOtherItems);
        for (u32 i = 0; i < numOtherItems; i++) {
            new ((Item*) this->items_ + (this->numItems_ + i)) Item{std::forward<Other>(other)[i]};
        }
        this->numItems_ += numOtherItems;
        return *this;
    }
    // Extend from initializer list.
    Array& operator+=(std::initializer_list<Item> initList) {
        u32 initSize = numericCast<u32>(initList.size());
        const Item* src = initList.begin();
        this->reserve(this->numItems_ + initSize);
        for (u32 i = 0; i < initSize; i++) {
            new ((Item*) this->items_ + (this->numItems_ + i)) Item{src[i]};
        }
        this->numItems_ += initSize;
        return *this;
    }

    //----------------------------------------------------
    // Item access
    //----------------------------------------------------

    // Subscript operators.
    Item& operator[](u32 index) & {
        PLY_ASSERT(index < this->numItems_);
        return ((Item*) this->items_)[index];
    }
    Item&& operator[](u32 index) && {
        PLY_ASSERT(index < this->numItems_);
        return std::move(((Item*) this->items_)[index]);
    }
    const Item& operator[](u32 index) const& {
        PLY_ASSERT(index < this->numItems_);
        return ((Item*) this->items_)[index];
    }
    // Access items relative to the back of the array.
    Item& back(s32 offset = -1) & {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->numItems_);
        return ((Item*) this->items_)[this->numItems_ + offset];
    }
    Item&& back(s32 offset = -1) && {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->numItems_);
        return std::move(((Item*) this->items_)[this->numItems_ + offset]);
    }
    const Item& back(s32 offset = -1) const& {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->numItems_);
        return ((Item*) this->items_)[this->numItems_ + offset];
    }
    // Return a pointer to the items in the array.
    Item* items() {
        return this->items_;
    }
    const Item* items() const {
        return this->items_;
    }
    // Return the number of item in the array. The number of allocated items may be greater.
    u32 numItems() const {
        return this->numItems_;
    }
    // Return true if the array is empty.
    bool isEmpty() const {
        return this->numItems_ == 0;
    }
    // Convert to true if the array is not empty. Can be used in if/while conditions.
    explicit operator bool() const {
        return this->numItems_ > 0;
    }
    // Return a subview of the array.
    ArrayView<Item> subview(u32 start) {
        return view().subview(start);
    }
    ArrayView<const Item> subview(u32 start) const {
        return view().subview(start);
    }
    ArrayView<Item> subview(u32 start, u32 numItems) {
        return view().subview(start, numItems);
    }
    ArrayView<const Item> subview(u32 start, u32 numItems) const {
        return view().subview(start, numItems);
    }
    // Return pointers suitable for iteration using range-for.
    Item* begin() {
        return (Item*) this->items_;
    }
    const Item* begin() const {
        return (Item*) this->items_;
    }
    Item* end() {
        return ((Item*) this->items_) + this->numItems_;
    }
    const Item* end() const {
        return ((Item*) this->items_) + this->numItems_;
    }

    //----------------------------------------------------
    // Modifying the array
    //----------------------------------------------------

    // Resize the array to a given number of items.
    void resize(u32 numItems) {
        for (u32 i = numItems; i < this->numItems_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        this->reserve(numItems);
        for (u32 i = this->numItems_; i < numItems; i++) {
            new ((Item*) this->items_ + i) Item;
        }
        this->numItems_ = numItems;
    }
    // Clear the array.
    void clear() {
        for (u32 i = 0; i < this->numItems_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        Heap::free(this->items_);
        new (this) Array<Item>;
    }
    // Append an item to the array with copy semantics.
    Item& append(const Item& item) {
        // The argument must not be a reference to an existing item in the array:
        PLY_ASSERT((&item < (Item*) this->items_) || (&item >= (Item*) this->items_ + this->numItems_));
        if (this->numItems_ >= this->allocated) {
            this->reserve(this->numItems_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->numItems_) Item{item};
        this->numItems_++;
        return *result;
    }
    // Append an item to the array with move semantics.
    Item& append(Item&& item) {
        // The argument must not be a reference to an existing item in the array:
        PLY_ASSERT((&item < (Item*) this->items_) || (&item >= (Item*) this->items_ + this->numItems_));
        if (this->numItems_ >= this->allocated) {
            this->reserve(this->numItems_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->numItems_) Item{std::move(item)};
        this->numItems_++;
        return *result;
    }
    // Append an item to the array by invoking the constructor with the given arguments.
    template <typename... Args>
    Item& append(Args&&... args) {
        if (this->numItems_ >= this->allocated) {
            this->reserve(this->numItems_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->numItems_) Item{std::forward<Args>(args)...};
        this->numItems_++;
        return *result;
    }
    // Insert a default-constructed item at the given position.
    Item& insert(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos <= this->numItems_);
        this->reserve(this->numItems_ + count);
        memmove(static_cast<void*>((Item*) this->items_ + pos + count),
                static_cast<const void*>((Item*) this->items_ + pos), (this->numItems_ - pos) * sizeof(Item));
        for (u32 i = pos; i < pos + count; i++) {
            new ((Item*) this->items_ + i) Item;
        }
        this->numItems_ += count;
        return ((Item*) this->items_)[pos];
    }
    // Erase items and shift the items after the erased position(s) to the left.
    void erase(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos + count <= this->numItems_);
        for (u32 i = pos; i < pos + count; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        memmove(static_cast<void*>((Item*) this->items_ + pos),
                static_cast<const void*>((Item*) this->items_ + pos + count),
                (this->numItems_ - (pos + count)) * sizeof(Item));
        this->numItems_ -= count;
    }
    // Erase items and move the last items(s) to the erased position(s).
    void eraseQuick(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos + count <= this->numItems_);
        for (u32 i = pos; i < pos + count; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        memmove(static_cast<void*>((Item*) this->items_ + pos),
                static_cast<const void*>((Item*) this->items_ + this->numItems_ - count), count * sizeof(Item));
        this->numItems_ -= count;
    }
    // Remove the last item(s) from the array.
    void pop(u32 count = 1) {
        PLY_ASSERT(count <= this->numItems_);
        resize(this->numItems_ - count);
    }
    // Reserve space for a given number of items. The number is rounded up to the nearest power of 2.
    void reserve(u32 numItems) {
        if (numItems > this->allocated) {
            if (this->allocated == 0) {
                this->allocated = numItems;
            } else {
                this->allocated = roundUpToNearestPowerOf2(numItems); // FIXME: Generalize to other resize strategies?
            }
            this->items_ = (Item*) Heap::realloc(this->items_, uptr(this->allocated) * sizeof(Item));
        }
    }
    // Compact the array by compacting the heap memory to exactly fit the number of items.
    void compact() {
        this->allocated = this->numItems_;
        this->items_ = (Item*) Heap::realloc(this->items_, uptr(this->allocated) * sizeof(Item));
    }

    //----------------------------------------------------
    // Converting to other types
    //----------------------------------------------------

    // Release the array and return the items. The array is reset to an empty state.
    Item* release() {
        Item* items = (Item*) this->items_;
        new (this) Array<Item>; // Reset the array to an empty state.
        return items;
    }
    // Convert to an `ArrayView`.
    ArrayView<Item> view() {
        return {(Item*) this->items_, this->numItems_};
    }
    // Convert to a const `ArrayView`.
    ArrayView<const Item> view() const {
        return {(Item*) this->items_, this->numItems_};
    }
    // Convert to an `ArrayView`.
    operator ArrayView<Item>() {
        return {(Item*) this->items_, this->numItems_};
    }
    // Convert to a const `ArrayView`.
    operator ArrayView<const Item>() const {
        return {(Item*) this->items_, this->numItems_};
    }
    // Convert to a `StringView`.
    StringView stringView() const {
        return {(const char*) this->items_, numericCast<u32>(this->numItems_ * sizeof(Item))};
    }
    // Convert to a `MutStringView`.
    MutStringView mutStringView() const {
        return {(char*) this->items_, numericCast<u32>(this->numItems_ * sizeof(Item))};
    }
};

template <typename Item_>
struct ArrayTraits<Array<Item_>> {
    using Item = Item_;
};

template <typename Item_>
struct ArrayTraits<const Array<Item_>> {
    using Item = const Item_;
};

inline Array<StringView> String::split(StringView separator) const {
    return ((StringView) * this).split(separator);
}

//  ▄▄▄▄▄ ▄▄                   ▄▄  ▄▄▄▄
//  ██    ▄▄ ▄▄  ▄▄  ▄▄▄▄   ▄▄▄██ ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄
//  ██▀▀  ██  ▀██▀  ██▄▄██ ██  ██ ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██
//  ██    ██ ▄█▀▀█▄ ▀█▄▄▄  ▀█▄▄██ ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██
//                                                             ▄▄▄█▀

template <typename Item>
struct InitItems {
    static void init(Item*) {
    }
    template <typename Arg, typename... RemainingArgs>
    static void init(Item* items, Arg&& arg, RemainingArgs&&... remainingArgs) {
        new (items) Item{std::forward<Arg>(arg)};
        init(items + 1, std::forward<RemainingArgs>(remainingArgs)...);
    }
};

template <typename Item, u32 NumItems>
struct FixedArray {
private:
#if PLY_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4200) // nonstandard extension used: zero-sized array in struct/union
#endif
    Item items_[NumItems];
#if PLY_COMPILER_MSVC
#pragma warning(pop)
#endif

public:
    FixedArray() = default;
    FixedArray(std::initializer_list<Item> args) {
        PLY_ASSERT(NumItems == args.size());
        const Item* src = args.begin();
        for (u32 i = 0; i < NumItems; i++) {
            new (this->items_ + i) Item{src[i]};
        }
    }
    template <typename... Args>
    FixedArray(Args&&... args) {
        PLY_STATIC_ASSERT(NumItems == sizeof...(Args));
        InitItems<Item>::init(this->items_, std::forward<Args>(args)...);
    }
    constexpr u32 numItems() const {
        return NumItems;
    }
    Item* items() {
        return this->items_;
    }
    const Item* items() const {
        return this->items_;
    }
    Item& operator[](u32 i) & {
        PLY_ASSERT(i < NumItems);
        return this->items_[i];
    }
    const Item& operator[](u32 i) const& {
        PLY_ASSERT(i < NumItems);
        return this->items_[i];
    }
    Item&& operator[](u32 i) && {
        PLY_ASSERT(i < NumItems);
        return std::move(this->items_[i]);
    }
    ArrayView<Item> view() {
        return {this->items_, NumItems};
    }
    ArrayView<const Item> view() const {
        return {this->items_, NumItems};
    }
    operator ArrayView<Item>() {
        return {this->items_, NumItems};
    }
    operator ArrayView<const Item>() const {
        return {(const Item*) this->items_, NumItems};
    }
    MutStringView mutStringView() {
        return {reinterpret_cast<char*>(this->items_), numericCast<u32>(NumItems * sizeof(Item))};
    }
    StringView stringView() const {
        return {reinterpret_cast<const char*>(this->items_), numericCast<u32>(NumItems * sizeof(Item))};
    }
    Item* begin() {
        return this->items_;
    }
    Item* end() {
        return this->items_ + NumItems;
    }
    const Item* begin() const {
        return this->items_;
    }
    const Item* end() const {
        return this->items_ + NumItems;
    }
};

template <typename Item_, u32 NumItems>
struct ArrayTraits<FixedArray<Item_, NumItems>> {
    using Item = Item_;
};

//  ▄▄  ▄▄               ▄▄     ▄▄                  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ██     ▄▄▄▄   ▄▄▄▄  ██  ▄▄ ▄▄  ▄▄ ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██    ██  ██ ██  ██ ██▄█▀  ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄██ ██▄▄█▀
//                                                                ██

// First, we introduce an overloaded function getAnyLookupKey to map arbitrary items to lookup keys.
// It's basically a small set of function templates, each taking a single argument, that are selectively enabled
// at compile time using SFINAE.

// Define an alias template hasGetLookupKeyMember. This provides a convenient way to selectively enable function
// candidates, using SFINAE, based on whether a given type defines a getLookupKey member function.
PLY_CHECK_WELL_FORMED(hasGetLookupKeyMember, declval<const T>().getLookupKey())

template <typename Item, PLY_ENABLE_IF(hasGetLookupKeyMember<Item>)>
static auto getAnyLookupKey(const Item& item) {
    return item.getLookupKey();
}
// Otherwise, for primitive data types like u32 and float, this overload will simply return the item itself.
template <typename Item, PLY_ENABLE_IF(!hasGetLookupKeyMember<Item>)>
static const Item& getAnyLookupKey(const Item& item) {
    return item;
}
template <typename Item>
using LookupKey = std::decay_t<decltype(getAnyLookupKey(declval<Item>()))>;

u32 getBestNumHashIndices(u32 numItems);

//----------------------------------------------------
template <typename Key, typename Subclass>
struct HashLookup {
    s32* indices = nullptr;
    u32 numIndices = 0;
    u32 numAllocatedIndices = 0;

    HashLookup() = default;
    HashLookup(const HashLookup& other)
        : indices{(s32*) Heap::alloc(sizeof(s32) * other.numAllocatedIndices)}, numIndices{other.numIndices},
          numAllocatedIndices{other.numAllocatedIndices} {
        for (u32 i = 0; i < this->numAllocatedIndices; i++) {
            this->indices[i] = other.indices[i];
        }
    }
    HashLookup(HashLookup&& other)
        : indices{other.indices}, numIndices{other.numIndices}, numAllocatedIndices{other.numAllocatedIndices} {
        new (&other) HashLookup;
    }
    ~HashLookup() {
        Heap::free(this->indices);
    }
    HashLookup& operator=(const HashLookup& other) {
        if (this != &other) {
            this->~HashLookup();
            new (this) HashLookup{other};
        }
        return *this;
    }
    HashLookup& operator=(HashLookup&& other) {
        if (this != &other) {
            this->~HashLookup();
            new (this) HashLookup{std::move(other)};
        }
        return *this;
    }

private:
    PLY_NO_INLINE void reindex(u32 numAllocatedIndices) {
        PLY_ASSERT(isPowerOf2(numAllocatedIndices));
        u32 mask = numAllocatedIndices - 1;

        // Allocate new indices.
        s32* newIndices = (s32*) Heap::alloc(sizeof(s32) * numAllocatedIndices);
        for (u32 i = 0; i < numAllocatedIndices; i++) {
            newIndices[i] = -1;
        }

        // Rebuild indices.
        for (u32 oldIdx = 0; oldIdx < this->numAllocatedIndices; oldIdx++) {
            s32 itemIndex = this->indices[oldIdx];
            if (itemIndex >= 0) {
                for (u32 newIdx = calculateHash(static_cast<Subclass*>(this)->getKey(itemIndex));; newIdx++) {
                    if (newIndices[newIdx & mask] < 0) {
                        newIndices[newIdx & mask] = itemIndex;
                        break;
                    }
                }
            }
        }

        Heap::free(this->indices);
        this->indices = newIndices;
        this->numAllocatedIndices = numAllocatedIndices;
    }

public:
    PLY_NO_INLINE s32 findIndex(const Key& key) const {
        if (!this->indices)
            return -1;
        PLY_ASSERT(isPowerOf2(this->numAllocatedIndices));
        u32 mask = this->numAllocatedIndices - 1;
        for (u32 idx = calculateHash(key);; idx++) {
            s32 itemIndex = this->indices[idx & mask];
            if (itemIndex < 0)
                return -1;
            if (key == static_cast<const Subclass*>(this)->getKey(itemIndex))
                return itemIndex;
        }
    }

    struct InsertIndexResult {
        u32 index;
        bool wasFound;
    };

    PLY_NO_INLINE InsertIndexResult insertIndex(const Key& key) {
        u32 minAllocated = getBestNumHashIndices(this->numIndices + 1);
        if (this->numAllocatedIndices < minAllocated) {
            this->reindex(minAllocated);
        }
        PLY_ASSERT(isPowerOf2(this->numAllocatedIndices));
        u32 mask = this->numAllocatedIndices - 1;
        for (u32 idx = calculateHash(key);; idx++) {
            s32 itemIndex = this->indices[idx & mask];
            if (itemIndex < 0) {
                u32 newIndex = static_cast<Subclass*>(this)->addItem(key);
                this->indices[idx & mask] = newIndex;
                this->numIndices++;
                return {newIndex, false};
            }
            if (key == static_cast<const Subclass*>(this)->getKey(itemIndex)) {
                return {numericCast<u32>(itemIndex), true};
            }
        }
    }
};

//   ▄▄▄▄          ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄
//   ▀▀▀█▄ ██▄▄██  ██
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄
//

PLY_CHECK_WELL_FORMED(isConstructibleFromKey, T{declval<const LookupKey<T>&>()})

template <typename Item>
struct Set : HashLookup<LookupKey<Item>, Set<Item>> {
    using Key = LookupKey<Item>;

    Array<Item> items_;

private:
    friend struct HashLookup<Key, Set<Item>>;

    auto getKey(u32 index) const {
        return getAnyLookupKey(this->items_[index]);
    }

    template <typename U = Item, PLY_ENABLE_IF(isConstructibleFromKey<U>)>
    u32 addItem(const Key& key) {
        u32 index = this->items_.numItems();
        this->items_.append(key);
        return index;
    }
    template <typename U = Item, PLY_ENABLE_IF(!isConstructibleFromKey<U>)>
    u32 addItem(const Key&) {
        u32 index = this->items_.numItems();
        this->items_.append();
        return index;
    }

public:
    ArrayView<Item> items() {
        return this->items_;
    }
    ArrayView<const Item> items() const {
        return this->items_;
    }

    Item* find(const Key& key) {
        s32 itemIndex = this->findIndex(key);
        if (itemIndex < 0)
            return nullptr;
        return &this->items_[itemIndex];
    }

    const Item* find(const Key& key) const {
        return const_cast<Set*>(this)->find(key);
    }

    struct InsertResult {
        Item* item;
        bool wasFound;
    };

    template <typename K = Key, PLY_ENABLE_IF(isConstructibleFromKey<K>)>
    InsertResult insert(const K& key) {
        auto result = this->insertIndex(key);
        return {&this->items_[numericCast<u32>(result.index)], result.wasFound};
    }

    InsertResult insertItem(Item&& item) {
        auto result = this->insertIndex(getAnyLookupKey(item));
        Item* dstItem = &this->items_[numericCast<u32>(result.index)];
        *dstItem = std::move(item);
        return {dstItem, result.wasFound};
    }

    PLY_NO_INLINE bool erase(const Key& key) {
        if (!this->items_)
            return false;
        PLY_ASSERT(isPowerOf2(this->numAllocatedIndices));
        u32 mask = this->numAllocatedIndices - 1;
        for (u32 idx = calculateHash(key);; idx++) {
            s32 itemIndex = this->indices[idx & mask];
            if (itemIndex < 0)
                return false;

            if (key == getAnyLookupKey(this->items_[itemIndex])) {
                // Found the item to erase.
                u32 lastIndex = this->items_.numItems() - 1;
                if ((u32) itemIndex < lastIndex) {
                    // Move the last item to the erased item's index.
                    for (u32 j = calculateHash(getAnyLookupKey(this->items_[lastIndex]));; j++) {
                        PLY_ASSERT(this->indices[j & mask] >= 0);
                        if ((u32) this->indices[j & mask] == lastIndex) {
                            this->indices[j & mask] = itemIndex;
                            break;
                        }
                    }
                }

                // Erase the item from the array.
                this->items_.eraseQuick(itemIndex);

                // Free the slot in the indices array.
                this->indices[idx & mask] = -1;

                // Check subsequent indices to see if any should move into the newly freed slot.
                for (u32 trailingIdx = idx + 1;; trailingIdx++) {
                    s32 trailingItemIndex = this->indices[trailingIdx & mask];
                    if (trailingItemIndex < 0) {
                        // No more trailing indices.
                        break;
                    }
                    u32 trailingItemHash = calculateHash(getAnyLookupKey(this->items_[trailingItemIndex]));
                    if (((trailingIdx - trailingItemHash) & mask) >= ((trailingIdx - idx) & mask)) {
                        // Move this index.
                        this->indices[idx & mask] = trailingItemIndex;
                        this->indices[trailingIdx & mask] = -1;
                        idx = trailingIdx; // This is the new freed slot.
                    }
                }
                return true;
            }
        }
    }

    void clear() {
        this->~Set();
        new (this) Set;
    }

    const Item* begin() const {
        return this->items_.begin();
    }
    const Item* end() const {
        return this->items_.end();
    }
};

//  ▄▄   ▄▄
//  ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██  ██
//  ██   ██ ▀█▄▄██ ██▄▄█▀
//                 ██

template <typename Key, typename Value>
struct Map : HashLookup<LookupKey<Key>, Map<Key, Value>> {
    using K = LookupKey<Key>;

    struct Item {
        Key key;
        Value value;

        Item(const K& key) : key{key} {
        }
        const Key& getLookupKey() const {
            return this->key;
        }
    };
    Array<Item> items_;

private:
    friend struct HashLookup<K, Map<Key, Value>>;

    auto getKey(u32 index) const {
        return getAnyLookupKey(this->items_[index]);
    }

    template <typename U = Item, PLY_ENABLE_IF(isConstructibleFromKey<U>)>
    u32 addItem(const K& key) {
        u32 index = this->items_.numItems();
        this->items_.append(key);
        return index;
    }
    template <typename U = Item, PLY_ENABLE_IF(!isConstructibleFromKey<U>)>
    u32 addItem(const K&) {
        u32 index = this->items_.numItems();
        this->items_.append();
        return index;
    }

public:
    Value* find(const K& key) {
        s32 itemIndex = this->findIndex(key);
        if (itemIndex < 0)
            return nullptr;
        return &this->items_[itemIndex].value;
    }

    const Value* find(const K& key) const {
        return const_cast<Map*>(this)->find(key);
    }

    ArrayView<Item> items() {
        return this->items_;
    }
    ArrayView<const Item> items() const {
        return this->items_;
    }

    struct InsertResult {
        Value* value;
        bool wasFound;
    };

    InsertResult insert(const K& key) {
        auto result = this->insertIndex(key);
        return {&this->items_[result.index].value, result.wasFound};
    }

    bool erase(const K& key) {
        if (!this->items_)
            return false;
        PLY_ASSERT(isPowerOf2(this->numAllocatedIndices));
        u32 mask = this->numAllocatedIndices - 1;
        for (u32 idx = calculateHash(key);; idx++) {
            s32 itemIndex = this->indices[idx & mask];
            if (itemIndex < 0)
                return false;

            if (key == getAnyLookupKey(this->items_[itemIndex])) {
                // Found the item to erase.
                u32 lastIndex = this->items_.numItems() - 1;
                if ((u32) itemIndex < lastIndex) {
                    // Move the last item to the erased item's index.
                    for (u32 j = calculateHash(getAnyLookupKey(this->items_[lastIndex]));; j++) {
                        PLY_ASSERT(this->indices[j & mask] >= 0);
                        if ((u32) this->indices[j & mask] == lastIndex) {
                            this->indices[j & mask] = itemIndex;
                            break;
                        }
                    }
                }

                // Erase the item from the array.
                this->items_.eraseQuick(itemIndex);

                // Free the slot in the indices array.
                this->indices[idx & mask] = -1;

                // Check subsequent indices to see if any should move into the newly freed slot.
                for (u32 trailingIdx = idx + 1;; trailingIdx++) {
                    s32 trailingItemIndex = this->indices[trailingIdx & mask];
                    if (trailingItemIndex < 0) {
                        // No more trailing indices.
                        break;
                    }
                    u32 trailingItemHash = calculateHash(getAnyLookupKey(this->items_[trailingItemIndex]));
                    if (((trailingIdx - trailingItemHash) & mask) >= ((trailingIdx - idx) & mask)) {
                        // Move this index.
                        this->indices[idx & mask] = trailingItemIndex;
                        this->indices[trailingIdx & mask] = -1;
                        idx = trailingIdx; // This is the new freed slot.
                    }
                }
                return true;
            }
        }
    }

    void clear() {
        this->~Map();
        new (this) Map;
    }

    const Item* begin() const {
        return this->items_.begin();
    }
    const Item* end() const {
        return this->items_.end();
    }
};

//   ▄▄▄▄                             ▄▄
//  ██  ██ ▄▄    ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄██
//  ██  ██ ██ ██ ██ ██  ██ ██▄▄██ ██  ██
//  ▀█▄▄█▀  ██▀▀██  ██  ██ ▀█▄▄▄  ▀█▄▄██
//

// ply::destroy() calls the object's destroy() member function, if any.
// Otherwise, it calls Heap::destroy().
// Null pointers are silently ignored in both cases.
// Also, if you overload destroy(Item*) in the same namespace as Item,
// Owned<> will call your overloaded destroy() function using ADL.
PLY_CHECK_WELL_FORMED(hasDestroyMember, declval<T>().destroy())

template <typename Item, PLY_ENABLE_IF(hasDestroyMember<Item>)>
void destroy(Item* obj) {
    if (obj) {
        obj->destroy();
    }
}
template <typename Item, PLY_ENABLE_IF(!hasDestroyMember<Item>)>
void destroy(Item* obj) {
    if (obj) {
        Heap::destroy(obj);
    }
}

// ply::duplicate() calls the object's duplicate() member function, if any.
// Otherwise, it allocates a new object on the heap and calls the copy contructor.
// If a nulltr is passed in, nullptr is returned.
// Also, if you overload duplicate(Item*) in the same namespace as Item,
// Owned<> will call your overloaded duplicate() function using ADL.
PLY_CHECK_WELL_FORMED(hasDuplicateMember, declval<T>().duplicate())

template <typename Item, PLY_ENABLE_IF(hasDuplicateMember<Item>)>
Item* duplicate(Item* obj) {
    if (obj)
        return obj->duplicate();
    else
        return nullptr;
}
template <typename Item, PLY_ENABLE_IF(!hasDuplicateMember<Item>)>
Item* duplicate(Item* obj) {
    if (obj)
        return Heap::create<Item>(*obj);
    else
        return nullptr;
}

template <typename Item>
class Owned {
private:
    template <typename>
    friend class Owned;
    Item* ptr = nullptr;

public:
    Owned() = default;
    Owned(Item* ptr) : ptr{ptr} { // FIXME: Replace with Owned<Item>::adopt()
    }
    Owned(const Owned& other) : ptr{duplicate(other.ptr)} {
    }
    Owned(Owned&& other) : ptr{other.release()} {
    }
    template <typename Derived, typename std::enable_if_t<std::is_base_of<Item, Derived>::value, int> = 0>
    Owned(const Owned<Derived>& other) : ptr{duplicate(other.ptr)} {
    }
    template <typename Derived, typename std::enable_if_t<std::is_base_of<Item, Derived>::value, int> = 0>
    Owned(Owned<Derived>&& other) : ptr{other.release()} {
    }
    ~Owned() {
        destroy(this->ptr);
    }
    static Owned adopt(Item* ptr) {
        Owned result;
        result.ptr = ptr;
        return result;
    }
    Owned& operator=(const Owned& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        Item* prev = this->ptr;
        this->ptr = duplicate(other.ptr);
        // destroy() is called last in case it indirectly results in this Owned<> instance being destructed.
        destroy(prev);
        return *this;
    }
    Owned& operator=(Owned&& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        Item* prev = this->ptr;
        this->ptr = other.release();
        // destroy() is called last in case it indirectly results in this Owned<> instance being destructed.
        destroy(prev);
        return *this;
    }
    template <typename Derived, typename std::enable_if_t<std::is_base_of<Item, Derived>::value, int> = 0>
    Owned& operator=(const Owned<Derived>& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        Item* prev = this->ptr;
        this->ptr = duplicate(other.ptr);
        // destroy() is called last in case it indirectly results in this Owned<> instance being destructed.
        destroy(prev);
        return *this;
    }
    template <typename Derived, typename std::enable_if_t<std::is_base_of<Item, Derived>::value, int> = 0>
    Owned& operator=(Owned<Derived>&& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        Item* prev = this->ptr;
        this->ptr = other.release();
        // destroy() is called last in case it indirectly results in this Owned<> instance being destructed.
        destroy(prev);
        return *this;
    }
    Owned& operator=(Item* ptr) {
        PLY_ASSERT(!this->ptr || this->ptr != ptr);
        if (this->ptr) {
            destroy(this->ptr);
        }
        this->ptr = ptr;
        return *this;
    }
    Item* operator->() const {
        return this->ptr;
    }
    operator Item*() const {
        return this->ptr;
    }
    Item* get() const {
        return this->ptr;
    }
    Item* release() {
        Item* ptr = this->ptr;
        this->ptr = nullptr;
        return ptr;
    }
    void clear() {
        Item* prev = this->ptr;
        this->ptr = nullptr;
        // destroy() is called last in case it indirectly results in this Owned<> instance being destructed.
        destroy(prev);
    }
    auto getLookupKey() const {
        return getAnyLookupKey(*this->ptr);
    }
};

//  ▄▄▄▄▄           ▄▄▄
//  ██  ██  ▄▄▄▄   ██    ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄
//  ██▀▀█▄ ██▄▄██ ▀██▀▀ ██▄▄██ ██  ▀▀ ██▄▄██ ██  ██ ██    ██▄▄██
//  ██  ██ ▀█▄▄▄   ██   ▀█▄▄▄  ██     ▀█▄▄▄  ██  ██ ▀█▄▄▄ ▀█▄▄▄
//

template <typename Item>
class Reference {
private:
    Item* ptr;

public:
    Reference() : ptr(nullptr) {
    }
    Reference(Item* ptr) : ptr(ptr) {
        if (this->ptr) {
            this->ptr->incRefCount();
        }
    }
    Reference(const Reference& ref) : ptr(ref.ptr) {
        if (this->ptr) {
            this->ptr->incRefCount();
        }
    }
    Reference(Reference&& ref) : ptr(ref.ptr) {
        ref.ptr = nullptr;
    }
    ~Reference() {
        if (this->ptr) {
            this->ptr->decRefCount();
        }
    }
    Item* operator->() const {
        return this->ptr;
    }
    operator Item*() const {
        return this->ptr;
    }
    Reference& operator=(Item* ptr) {
        Item* prev = this->ptr;
        this->ptr = ptr;
        if (this->ptr) {
            this->ptr->incRefCount();
        }
        if (prev) {
            prev->decRefCount();
        }
        return *this;
    }
    Reference& operator=(const Reference& ref) {
        Item* prev = this->ptr;
        this->ptr = ref.ptr;
        if (this->ptr) {
            this->ptr->incRefCount();
        }
        if (prev) {
            prev->decRefCount();
        }
        return *this;
    }
    Reference& operator=(Reference&& ref) {
        if (this == &ref)
            return *this;
        if (this->ptr) {
            this->ptr->decRefCount();
        }
        this->ptr = ref.ptr;
        ref.ptr = nullptr;
        return *this;
    }
    explicit operator bool() const {
        return (this->ptr != nullptr);
    }
    Item* release() {
        Item* ptr = this->ptr;
        this->ptr = nullptr;
        return ptr;
    };
    void clear() {
        Item* prev = this->ptr;
        this->ptr = nullptr;
        if (prev) {
            // decRefCount() should be called last in case destructing the referenced object
            // results in destructing the Reference itself.
            prev->decRefCount();
        }
    }
};

//  ▄▄▄▄▄           ▄▄▄  ▄▄▄▄                        ▄▄              ▄▄
//  ██  ██  ▄▄▄▄   ██   ██  ▀▀  ▄▄▄▄  ▄▄  ▄▄ ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄   ▄▄▄██
//  ██▀▀█▄ ██▄▄██ ▀██▀▀ ██     ██  ██ ██  ██ ██  ██  ██   ██▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄   ██   ▀█▄▄█▀ ▀█▄▄█▀ ▀█▄▄██ ██  ██  ▀█▄▄ ▀█▄▄▄  ▀█▄▄██
//

template <typename Subclass>
class RefCounted {
private:
    Atomic<u32> refCount = 0;

public:
    void incRefCount() {
        u32 oldCount = this->refCount.fetchAdd(1, AcqRel);
        PLY_ASSERT(oldCount < 5000);
        PLY_UNUSED(oldCount);
    }
    void decRefCount() {
        s32 oldCount = this->refCount.fetchSub(1, AcqRel);
        PLY_ASSERT(oldCount < 5000);
        if (oldCount == 1) {
            static_cast<Subclass*>(this)->onRefCountZero();
        }
    }
    s32 getRefCount() const {
        return this->refCount.load(Acquire);
    }
};

//  ▄▄▄▄▄                      ▄▄
//  ██    ▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀  ██  ██ ██  ██ ██     ██   ██  ██ ██  ▀▀
//  ██    ▀█▄▄██ ██  ██ ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██
//

// Functor is a template class that acts like a function pointer but can
// also target lambda expressions that contain captured variables.

template <typename>
struct Functor;

PLY_CHECK_WELL_FORMED(isCopyConstructible, T{declval<const T>()});

template <typename T, PLY_ENABLE_IF(isCopyConstructible<T>)>
void copyConstructWrapper(T* dst, const T* src) {
    new (dst) T{*src};
}
template <typename T, PLY_ENABLE_IF(!isCopyConstructible<T>)>
void copyConstructWrapper(T*, const T*) {
    PLY_FORCE_CRASH();
}

template <typename Return, typename... Args>
struct Functor<Return(Args...)> {
    struct DynamicOps {
        void (*copyConstruct)(Functor*, const Functor*) = nullptr;
        void (*destruct)(Functor*) = nullptr;
    };

    Return (*thunk)(const Functor*, Args...) = nullptr;
    void* thunkArg = nullptr;
    DynamicOps* dynOps = nullptr;

    Functor() = default;
    Functor(Return (*target)(Args...)) {
        this->thunk = [](const Functor* f, Args... args) -> Return {
            return (*(Return (*)(Args...)) f->thunkArg)(std::forward<Args>(args)...);
        };
        this->thunkArg = (void*) target;
    }
    Functor(const Functor& o) {
        if (o.dynOps) {
            o.dynOps->copyConstruct(this, &o);
        } else {
            this->thunk = o.thunk;
            this->thunkArg = o.thunkArg;
            this->dynOps = o.dynOps;
        }
    }
    Functor(Functor&& o) {
        this->thunk = o.thunk;
        this->thunkArg = o.thunkArg;
        this->dynOps = o.dynOps;
        new (&o) Functor;
    }
    // Construct from any callable object such as a lambda expression.
    template <typename Callable, typename = decltype(declval<Callable>()(declval<Args>()...)),
              std::enable_if_t<!std::is_same<Functor, std::decay_t<Callable>>::value, int> = 0>
    Functor(Callable&& callable) {
        using CallableType = std::decay_t<Callable>; // Remove const (if any)
        this->thunk = [](const Functor* f, Args... args) -> Return {
            return (*(const CallableType*) f->thunkArg)(std::forward<Args>(args)...);
        };
        this->thunkArg = (void*) Heap::create<CallableType>(std::forward<Callable>(callable));
        static DynamicOps dynOpsForCallable = {
            [](Functor* dst, const Functor* src) { // copyConstruct
                dst->thunk = src->thunk;
                dst->thunkArg = Heap::alloc(sizeof(CallableType));
                copyConstructWrapper<CallableType>((CallableType*) dst->thunkArg, (const CallableType*) src->thunkArg);
                dst->dynOps = src->dynOps;
            },
            [](Functor* f) { // destruct
                Heap::destroy<CallableType>((CallableType*) f->thunkArg);
            },
        };
        this->dynOps = &dynOpsForCallable;
    }
    ~Functor() {
        if (this->dynOps) {
            this->dynOps->destruct(this);
        }
    }
    Functor& operator=(const Functor& o) {
        this->~Functor();
        new (this) Functor{o};
        return *this;
    }
    Functor& operator=(Functor&& o) {
        this->~Functor();
        new (this) Functor{std::move(o)};
        return *this;
    }
    explicit operator bool() const {
        return this->thunk != nullptr;
    }
    template <typename... CallArgs>
    Return operator()(CallArgs&&... args) const {
        PLY_ASSERT(this->thunk);
        return this->thunk(this, std::forward<CallArgs>(args)...);
    }
};

//  ▄▄   ▄▄               ▄▄                ▄▄
//  ██   ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//   ██ ██   ▄▄▄██ ██  ▀▀ ██  ▄▄▄██ ██  ██  ██
//    ▀█▀   ▀█▄▄██ ██     ██ ▀█▄▄██ ██  ██  ▀█▄▄
//

// Helper to get the index of a type in a parameter pack
template <typename T, u32 Index>
constexpr u32 getTypeIndex() {
    return 0;
}
template <typename T, u32 Index, typename First, typename... Rest>
constexpr u32 getTypeIndex() {
    return std::is_same<T, First>::value ? Index : getTypeIndex<T, Index + 1, Rest...>();
}
constexpr uptr maxSize(uptr val) {
    return val;
}
template <typename... Rest>
constexpr uptr maxSize(uptr val1, uptr val2, Rest... rest) {
    return maxSize((val1 > val2) ? val1 : val2, rest...);
}

template <typename... Subtypes>
struct Variant {
private:
    u32 subtype = 0;
    alignas(maxSize(alignof(Subtypes)...)) char storage[maxSize(sizeof(Subtypes)...)];

    // Helper to copy the current subobject
    template <u32 Index>
    void copyHelper(const Variant& other) {
    }
    template <u32 Index, typename First, typename... Rest>
    void copyHelper(const Variant& other) {
        if (this->subtype == Index) {
            new (this->storage) First{*reinterpret_cast<const First*>(other.storage)};
        } else {
            this->copyHelper<Index + 1, Rest...>(other);
        }
    }

    // Helper to move the current subobject
    template <u32 Index>
    void moveHelper(Variant&& other) {
    }
    template <u32 Index, typename First, typename... Rest>
    void moveHelper(Variant&& other) {
        if (this->subtype == Index) {
            new (this->storage) First{std::move(*reinterpret_cast<First*>(other.storage))};
        } else {
            this->moveHelper<Index + 1, Rest...>(std::move(other));
        }
    }

    // Helper to destruct the current subobject
    template <u32 Index>
    void destructHelper() {
    }
    template <u32 Index, typename First, typename... Rest>
    void destructHelper() {
        if (this->subtype == Index) {
            reinterpret_cast<First*>(this->storage)->~First();
        } else {
            this->destructHelper<Index + 1, Rest...>();
        }
    }

public:
    // Map a type to its subtype index
    template <typename T>
    static constexpr u32 indexOf = ply::getTypeIndex<std::decay_t<T>, 1, Subtypes...>();

    // Construct an empty variant
    Variant() = default;

    // Constructor for each type in the parameter pack
    template <typename T, typename std::enable_if<(indexOf<T> > 0), int>::type = 0>
    Variant(T&& value) {
        this->subtype = indexOf<T>;
        new (storage) std::decay_t<T>(std::forward<T>(value));
    }

    // Copy constructor
    Variant(const Variant& other) : subtype{other.subtype} {
        this->copyHelper<1, Subtypes...>(other);
    }

    // Move constructor
    Variant(Variant&& other) : subtype{other.subtype} {
        this->moveHelper<1, Subtypes...>(std::move(other));
        other.subtype = 0;
    }

    // Destructor
    ~Variant() {
        this->destructHelper<1, Subtypes...>();
    }

    // Copy assignment
    Variant& operator=(const Variant& other) {
        this->destructHelper<1, Subtypes...>();
        this->subtype = other.subtype;
        this->copyHelper<1, Subtypes...>(other);
        return *this;
    }

    // Move assignment
    Variant& operator=(Variant&& other) {
        this->destructHelper<1, Subtypes...>();
        this->subtype = other.subtype;
        this->moveHelper<1, Subtypes...>(std::move(other));
        other.subtype = 0;
        return *this;
    }

    // Assignment operator for each type in the parameter pack
    template <typename T, typename std::enable_if<(indexOf<T> > 0), int>::type = 0>
    Variant& operator=(T&& value) {
        // Destruct the current object if any
        if (this->subtype > 0) {
            this->destructHelper<1, Subtypes...>();
        }
        // Construct the new object
        this->subtype = indexOf<T>;
        new (storage) std::decay_t<T>(std::forward<T>(value));
        return *this;
    }

    // Get current subtype index
    u32 getSubtypeIndex() const {
        return this->subtype;
    }

    // Check if the variant is currently empty
    bool isEmpty() const {
        return this->subtype == 0;
    }

    // Check if the variant currently holds a specific type
    template <typename T, typename std::enable_if<(indexOf<T> > 0), int>::type = 0>
    bool is() const {
        return this->subtype == indexOf<T>;
    }

    // Dynamic casts
    template <typename T, typename std::enable_if<(indexOf<T> > 0), int>::type = 0>
    T* as() {
        if (this->subtype != indexOf<T>)
            return nullptr;
        return reinterpret_cast<T*>(this->storage);
    }
    template <typename T, typename std::enable_if<(indexOf<T> > 0), int>::type = 0>
    const T* as() const {
        if (this->subtype != indexOf<T>)
            return nullptr;
        return reinterpret_cast<const T*>(this->storage);
    }

    // Switch to
    template <typename T, typename... Args>
    T& switchTo(Args&&... args) {
        this->destructHelper<1, Subtypes...>();
        this->subtype = indexOf<T>;
        new (this->storage) T{std::forward<Args>(args)...};
        return *reinterpret_cast<T*>(this->storage);
    }
};

//   ▄▄▄▄  ▄▄▄                       ▄▄  ▄▄   ▄▄
//  ██  ██  ██   ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄██▄▄ ██▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄
//  ██▀▀██  ██  ██  ██ ██  ██ ██  ▀▀ ██  ██   ██  ██ ██ ██ ██ ▀█▄▄▄
//  ██  ██ ▄██▄ ▀█▄▄██ ▀█▄▄█▀ ██     ██  ▀█▄▄ ██  ██ ██ ██ ██  ▄▄▄█▀
//               ▄▄▄█▀

template <typename T, typename U, PLY_ENABLE_IF_WELL_FORMED(declval<T>() == declval<U>())>
s32 find(ArrayView<T> arr, const U& item, u32 startPos = 0) {
    for (u32 i = startPos; i < arr.numItems(); i++) {
        if (arr[i] == item)
            return i;
    }
    return -1;
}
template <typename T, typename Callback, PLY_ENABLE_IF_WELL_FORMED(declval<Callback>()(declval<T>()))>
s32 find(ArrayView<T> arr, const Callback& callback, u32 startPos = 0) {
    for (u32 i = startPos; i < arr.numItems(); i++) {
        if (callback(arr[i]))
            return i;
    }
    return -1;
}
template <typename Arr, typename Arg, PLY_ENABLE_IF_ARRAY_TYPE(Arr)>
s32 find(const Arr& arr, const Arg& arg, u32 startPos = 0) {
    return find(ArrayView<const ArrayItemType<Arr>>{arr}, arg, startPos);
}

template <typename T, typename U, PLY_ENABLE_IF_WELL_FORMED(declval<T>() == declval<U>())>
s32 reverseFind(ArrayView<const T> arr, const U& item, s32 startPos = -1) {
    if (startPos < 0) {
        startPos += arr.numItems();
    }
    for (s32 i = startPos; i >= 0; i--) {
        if (arr[i] == item)
            return i;
    }
    return -1;
}
template <typename T, typename Callback, PLY_ENABLE_IF_WELL_FORMED(declval<Callback>()(declval<T>()))>
s32 reverseFind(ArrayView<const T> arr, const Callback& callback, s32 startPos = -1) {
    if (startPos < 0) {
        startPos += arr.numItems();
    }
    for (s32 i = startPos; i >= 0; i--) {
        if (callback(arr[i]))
            return i;
    }
    return -1;
}
template <typename Arr, typename Arg, PLY_ENABLE_IF_ARRAY_TYPE(Arr)>
s32 reverseFind(const Arr& arr, const Arg& arg, s32 startPos = -1) {
    return reverseFind(ArrayView<const ArrayItemType<Arr>>{arr}, arg, startPos);
}

template <typename T>
bool defaultLess(const T& a, const T& b) {
    return a < b;
}
template <typename T, typename IsLess = decltype(defaultLess<T>)>
void sort(ArrayView<T> view, const IsLess& isLess = defaultLess<T>) {
    if (view.numItems() <= 1)
        return;
    u32 lo = 0;
    u32 hi = view.numItems() - 1;
    u32 pivot = view.numItems() / 2;
    for (;;) {
        while (lo < hi && isLess(view[lo], view[pivot])) {
            lo++;
        }
        while (lo < hi && isLess(view[pivot], view[hi])) {
            hi--;
        }
        if (lo >= hi)
            break;
        // view[lo] is >= pivot
        // All slots to left of lo are < pivot
        // view[hi] <= pivot
        // All slots to the right of hi are > pivot
        PLY_ASSERT(!isLess(view[lo], view[pivot]));
        PLY_ASSERT(!isLess(view[pivot], view[hi]));
        PLY_ASSERT(lo < hi);
        std::swap(view[lo], view[hi]);
        if (lo == pivot) {
            pivot = hi;
        } else if (hi == pivot) {
            pivot = lo;
        }
        lo++;
    }
    PLY_ASSERT((s32) hi >= 0);
    // Now, everything to left of lo is <= pivot, and everything from hi onwards is >= pivot.
    PLY_ASSERT(hi <= lo);
    while (lo > 1) {
        if (!isLess(view[lo - 1], view[pivot])) {
            lo--;
        } else {
            sort(view.subview(0, lo), isLess);
            break;
        }
    }
    while (hi + 1 < view.numItems()) {
        if (!isLess(view[pivot], view[hi])) {
            hi++;
        } else {
            sort(view.subview(hi), isLess);
            break;
        }
    }
}
template <typename Arr, typename IsLess = decltype(defaultLess<ArrayItemType<Arr>>)>
void sort(Arr& arr, const IsLess& isLess = defaultLess<ArrayItemType<Arr>>) {
    using T = ArrayItemType<Arr>;
    sort(ArrayView<T>{arr}, isLess);
}

enum FindType {
    FindGreaterThan,
    FindGreaterThanOrEqual,
};

template <typename Key>
bool meetsCondition(const Key& a, const Key& b, FindType findType) {
    switch (findType) {
        case FindGreaterThan:
            return a > b;
        case FindGreaterThanOrEqual:
            return a >= b;
        default:
            return false;
    }
}

template <typename Item>
u32 binarySearch(ArrayView<Item> arr, const LookupKey<Item>& desiredKey, FindType findType) {
    u32 lo = 0;              // Start of the search range.
    u32 hi = arr.numItems(); // End of the search range.
    while (lo < hi) {
        u32 mid = (lo + hi) / 2; // Middle index that sits roughly halfway between lo and hi.
        PLY_ASSERT((mid >= lo) && (mid < hi));
        auto midKey = getAnyLookupKey(arr[mid]);
        if (meetsCondition(midKey, desiredKey, findType)) {
            // The middle key meets the search condition. Make it the new end of the search range.
            hi = mid;
        } else {
            // The middle key doesn't meet the search condition. The search range should start after this.
            lo = mid + 1;
        }
    }
    PLY_ASSERT(lo == hi);
    return lo;
}
template <typename Arr, typename Key>
u32 binarySearch(Arr& arr, const Key& desiredKey, FindType findType) {
    using Item = ArrayItemType<Arr>;
    return binarySearch(ArrayView<Item>{arr}, desiredKey, findType);
}

//  ▄▄▄▄▄  ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀▀  ██ ██  ██ ██▄▄██
//  ██     ██ ██▄▄█▀ ▀█▄▄▄
//            ██

class Pipe {
protected:
    u64 seekPos = 0;
    u32 flags = 0;

    Pipe() = default;

public:
    static constexpr u32 HAS_READ_PERMISSION = 0x1;
    static constexpr u32 HAS_WRITE_PERMISSION = 0x2;
    static constexpr u32 CAN_SEEK = 0x4;

    virtual ~Pipe() = default;
    // read() only returns 0 at EOF. Otherwise, it blocks until data is available.
    virtual u32 read(MutStringView buf);
    virtual bool write(StringView buf);
    virtual void flush(bool toDevice = false);
    virtual u64 getFileSize();
    virtual void seekTo(s64 offset);
    u32 getFlags() const {
        return this->flags;
    }
};

#if defined(PLY_WINDOWS)

class PipeHandle : public Pipe {
public:
    HANDLE handle = INVALID_HANDLE_VALUE;

    PipeHandle(HANDLE h, u32 flags) : handle(h) {
        this->flags = flags;
    }
    virtual ~PipeHandle();
    virtual u32 read(MutStringView buf) override;
    virtual bool write(StringView buf) override;
    virtual void flush(bool toDevice = false) override;
    virtual u64 getFileSize() override;
    virtual void seekTo(s64 offset) override;
};

#elif defined(PLY_POSIX)

class Pipe_FD : public Pipe {
public:
    int fd = -1;

    Pipe_FD() {
    }
    Pipe_FD(Pipe_FD&& other) : fd{other.fd} {
        other.fd = -1;
    }
    Pipe_FD(int fd, u32 flags) : fd{fd} {
        this->flags = flags;
    }
    Pipe_FD& operator=(Pipe_FD&& other) {
        this->fd = other.fd;
        other.fd = -1;
        return *this;
    }
    virtual ~Pipe_FD();
    virtual u32 read(MutStringView buf) override;
    virtual bool write(StringView buf) override;
    virtual void flush(bool toDevice = false) override;
    virtual u64 getFileSize() override;
    virtual void seekTo(s64 offset) override;
};

#endif

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

struct Stream {
    enum class Type : u8 {
        None,
        Pipe,
        Mem,
        View,
    };
    enum class Mode : u8 {
        None,
        Reading,
        Writing,
    };

    static constexpr u32 BUFFER_SIZE = 32000;
    static constexpr u32 MAX_CONSECUTIVE_BYTES = 2048;

    struct PipeData {
        Pipe* pipe = nullptr;
        char* buffer = nullptr;
        u64 seekPosAtBuffer = 0;
    };
    struct MemData {
        Array<char*> buffers;
        u32 bufferIndex = 0;
        u32 numBytesInLastBuffer = 0;
        // Temporary buffer used when caller requests consecutive bytes that straddle buffer boundaries:
        char* tempBuffer = nullptr;
        u32 tempBufferOffset = 0; // Offset of overlap buffer relative to storage buffer
    };
    struct ViewData {
        char* startByte = nullptr;
    };

    char* curByte = nullptr;
    char* endByte = nullptr;
    Type type = Type::None;
    Mode mode = Mode::None;
    bool isPipeOwner = false; // only used if type == Type::Pipe
    bool hasReadPermission = false;
    bool hasWritePermission = false;
    bool usingTempBuffer = false; // only used if type == Type::Mem
    bool atEof = false;
    bool inputError = false;
    union {
        PipeData pipe; // if type == Type::Pipe
        MemData mem;   // if type == Type::Mem
        ViewData view; // if type == Type::View
    };

    Stream();
    Stream(Pipe* pipe, bool isPipeOwner);
    Stream(Stream&& other);
    ~Stream();
    Stream& operator=(Stream&& other) {
        PLY_ASSERT(this != &other);
        this->~Stream();
        new (this) Stream{std::move(other)};
        return *this;
    }

    bool isOpen() {
        return this->curByte != nullptr;
    }
    explicit operator bool() {
        return this->curByte != nullptr;
    }
    void close() {
        this->~Stream();
        new (this) Stream; // Illegal to use after this.
    }

    //--------------------------------------------
    // Main read & write functions
    bool makeReadable(u32 minBytes = 1) {
        if ((this->mode == Mode::Reading) && (this->curByte + minBytes <= this->endByte))
            return true;
        else
            return makeReadableInternal(minBytes);
    }
    bool makeWritable(u32 minBytes = 1) {
        if ((this->mode == Mode::Writing) && (this->curByte + minBytes <= this->endByte))
            return true;
        else
            return makeWritableInternal(minBytes);
    }
    bool hasRemainingBytes() const {
        return this->endByte > this->curByte;
    }
    u32 numRemainingBytes() const {
        return numericCast<u32>(this->endByte - this->curByte);
    }
    StringView viewRemainingBytes() const {
        return {this->curByte, numericCast<u32>(this->endByte - this->curByte)};
    }
    MutStringView viewRemainingBytesMut() {
        return {this->curByte, numericCast<u32>(this->endByte - this->curByte)};
    }
    void flush(bool toDevice = false);

    //--------------------------------------------
    // Read wrappers
    char peekByte() {
        if (this->curByte < this->endByte)
            return *this->curByte;
        else
            return this->peekByteInternal();
    }
    char readByte() {
        if (this->curByte < this->endByte)
            return *this->curByte++;
        else
            return this->readByteInternal();
    }
    u32 read(MutStringView dst) {
        if (dst.numBytes <= this->numRemainingBytes()) {
            memcpy(dst.bytes, this->curByte, dst.numBytes);
            this->curByte += dst.numBytes;
            return dst.numBytes;
        } else
            return this->readInternal(dst);
    }
    u32 skip(u32 numBytes) {
        if (numBytes <= this->numRemainingBytes()) {
            this->curByte += numBytes;
            return numBytes;
        } else
            return this->skipInternal(numBytes);
    }

    //--------------------------------------------
    // Write wrappers
    bool write(char c) {
        if (!this->makeWritable())
            return false;
        *this->curByte++ = c;
        return true;
    }
    u32 write(StringView bytes);
    template <typename... Args>
    void format(StringView fmt, const Args&... args);

    //--------------------------------------------
    // Seeking
    u64 getSeekPos();
    void seekTo(u64 seekPos);

protected:
    bool makeReadableInternal(u32 numBytes);
    bool makeWritableInternal(u32 numBytes);
    char peekByteInternal();
    char readByteInternal();
    void flushMemWrites();
    u32 readInternal(MutStringView dst);
    u32 skipInternal(u32 numBytes);
};

template <typename T>
T nativeRead(Stream& in) {
    T result;
    in.read({(char*) &result, sizeof(result)});
    return result;
}

template <typename T>
void nativeWrite(Stream& out, const T& value) {
    out.write({(const char*) &value, sizeof(value)});
}

//--------------------------------------------
class MemStream : public Stream {
public:
    MemStream();
    String moveToString();
};

//--------------------------------------------
struct ViewStream : Stream {
    ViewStream() = default;
    explicit ViewStream(StringView view);
    explicit ViewStream(MutStringView view);

    void seekTo(char* byte) {
        PLY_ASSERT((byte >= this->view.startByte) && (byte <= this->endByte));
        this->curByte = byte;
        this->atEof = false;
        this->inputError = false;
    }
    template <typename... Args>
    bool match(StringView pattern, Args*... args);
};

//   ▄▄▄▄   ▄▄                     ▄▄                   ▄▄     ▄▄▄▄     ▄▄  ▄▄▄▄
//  ██  ▀▀ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██      ██     ▄█▀ ██  ██
//   ▀▀▀█▄  ██    ▄▄▄██ ██  ██ ██  ██  ▄▄▄██ ██  ▀▀ ██  ██      ██   ▄█▀   ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄██     ▄██▄ ██     ▀█▄▄█▀
//

Pipe* getStdInPipe();
Pipe* getStdOutPipe();
Pipe* getStdErrPipe();

enum ConsoleMode { TEXT, BINARY };

Stream getStdIn(ConsoleMode mode = TEXT);
Stream getStdOut(ConsoleMode mode = TEXT);
Stream getStdErr(ConsoleMode mode = TEXT);

//  ▄▄▄▄▄                    ▄▄ ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                         ▄▄▄█▀

static constexpr u32 ID_WITH_DOLLAR_SIGN = 0x1;
static constexpr u32 ID_WITH_DASH = 0x2;

enum class QuotedStringType {
    C,
    JavaScript,
    JSON,
    Python,
};

enum class QuotedStringError {
    NoOpeningQuote,
    UnexpectedEndOfLine,
    UnexpectedEndOfFile,
    BadEscapeSequence,
};

String readLine(Stream& in);
StringView readLine(ViewStream& viewIn);
String readWhitespace(Stream& in);
StringView readWhitespace(ViewStream& in);
void skipWhitespace(Stream& in);
String readIdentifier(Stream& in, u32 flags = 0);
StringView readIdentifier(ViewStream& viewIn, u32 flags = 0);
u64 readU64FromText(Stream& in, u32 radix = 10);
s64 readS64FromText(Stream& in, u32 radix = 10);
double readDoubleFromText(Stream& in, u32 radix = 10);
String readQuotedString(Stream& in, QuotedStringType type = QuotedStringType::C,
                        bool strict = true, Functor<void(QuotedStringError)> errorCallback = {});

using MatchArg = Variant<String*, StringView*, u32*, s32*, u64*, s64*, double*, float*, bool*>;
bool matchWithArgs(ViewStream& in, StringView pattern, ArrayView<const MatchArg> matchArgs);

template <typename... Args>
bool String::match(StringView pattern, Args*... args) const {
    FixedArray<MatchArg, sizeof...(Args)> matchArgs{args...};
    ViewStream in{*this};
    return matchWithArgs(in, pattern, matchArgs);
}
template <typename... Args>
bool StringView::match(StringView pattern, Args*... args) const {
    FixedArray<MatchArg, sizeof...(Args)> matchArgs{args...};
    ViewStream in{*this};
    return matchWithArgs(in, pattern, matchArgs);
}
template <typename... Args>
bool ViewStream::match(StringView pattern, Args*... args) {
    FixedArray<MatchArg, sizeof...(Args)> matchArgs{args...};
    return matchWithArgs(*this, pattern, matchArgs);
}

//  ▄▄    ▄▄        ▄▄  ▄▄   ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//   ██▀▀██  ██     ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                      ▄▄▄█▀

void printNumber(Stream& out, u64 value, u32 radix = 10, bool capitalize = false);
void printNumber(Stream& out, s64 value, u32 radix = 10, bool capitalize = false);
inline void printNumber(Stream& out, u32 value, u32 radix = 10, bool capitalize = false) {
    return printNumber(out, (u64) value, radix, capitalize);
}
inline void printNumber(Stream& out, s32 value, u32 radix = 10, bool capitalize = false) {
    return printNumber(out, (s64) value, radix, capitalize);
}
void printNumber(Stream& out, double value, u32 radix = 10, bool capitalize = false);
void printEscapedString(Stream& out, StringView str);
void printXmlEscapedString(Stream& out, StringView str);

struct FormatArg {
    Variant<StringView, bool, s64, u64, double> var;

    FormatArg(StringView view = {}) : var{view} {
    }
    FormatArg(const char* str) : var{StringView{str}} {
    }
    FormatArg(const char& c) : var{StringView{c}} {
    }
    explicit FormatArg(bool v) : var{v} {
    }
    FormatArg(s64 v) : var{v} {
    }
    FormatArg(u64 v) : var{v} {
    }
    FormatArg(s32 v) : var{(s64) v} {
    }
    FormatArg(u32 v) : var{(u64) v} {
    }
    FormatArg(double v) : var{v} {
    }
};

void formatWithArgs(Stream& out, StringView fmt, ArrayView<const FormatArg> args);
template <typename... Args>
String String::format(StringView fmt, const Args&... args) {
    MemStream mem;
    mem.format(fmt, args...);
    return mem.moveToString();
}
template <typename... Args>
void Stream::format(StringView fmt, const Args&... args) {
    FixedArray<FormatArg, sizeof...(Args)> fa{args...};
    formatWithArgs(*this, fmt, fa);
}

// Prints a DateTime object as human-readable text using a format string.
void printDateTime(Stream& out, StringView format, const DateTime& dateTime);

inline String String::fromDateTime(StringView format, const DateTime& dateTime) {
    MemStream mem;
    printDateTime(mem, format, dateTime);
    return mem.moveToString();
}

//  ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

enum UnicodeType {
    NOT_UNICODE,
    UTF8,
    UTF16_LE,
    UTF16_BE,
};

struct ExtendedTextParams {
    ArrayView<s32> lut; // Lookup table: byte -> Unicode codepoint.
    Map<u32, u8> reverseLut;
    s32 missingChar = 255; // If negative, missing characters are skipped.
};

enum DecodeStatus {
    DS_OK,
    DS_ILL_FORMED,      // Not at EOF.
    DS_NOT_ENOUGH_DATA, // Can still decode an ill-formed codepoint.
};

struct DecodeResult {
    s32 point = -1;
    u32 numBytes = 0;
    DecodeStatus status = DS_OK;
};

// Returns the number of bytes written to buf.
u32 encodeUnicode(FixedArray<char, 4>& buf, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams = nullptr);
DecodeResult decodeUnicode(StringView str, UnicodeType unicodeType, ExtendedTextParams* extParams = nullptr);

bool encodeUnicode(Stream& out, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams = nullptr);
DecodeResult decodeUnicode(Stream& in, UnicodeType unicodeType,
                           ExtendedTextParams* extParams = nullptr); // -1 at EOF

class InPipeConvertUnicode : public Pipe {
public:
    Stream in;
    UnicodeType srcType;
    ExtendedTextParams* extParams = nullptr;

    // shimStorage is used to split multibyte characters at buffer boundaries.
    FixedArray<char, 4> shimStorage;
    StringView shimUsed;

    InPipeConvertUnicode(Stream&& in, UnicodeType type = NOT_UNICODE) : in{std::move(in)}, srcType{type} {
        PLY_ASSERT(this->in.hasReadPermission);
        this->flags = Pipe::HAS_READ_PERMISSION;
    }
    // Fill dstBuf with UTF-8-encoded data.
    virtual u32 read(MutStringView dstBuf) override;
};

class OutPipeConvertUnicode : public Pipe {
public:
    Stream childOut;
    UnicodeType dstType;
    ExtendedTextParams* extParams = nullptr;

    // shimStorage is used to join multibyte characters at buffer boundaries.
    char shimStorage[4];
    u32 shimUsed = false;

    OutPipeConvertUnicode(Stream&& childOut, UnicodeType type = NOT_UNICODE)
        : childOut{std::move(childOut)}, dstType{type} {
        PLY_ASSERT(this->childOut.hasWritePermission);
        this->flags = Pipe::HAS_WRITE_PERMISSION;
    }
    // srcBuf expects UTF-8-encoded data.
    virtual bool write(StringView srcBuf) override;
    virtual void flush(bool toDevice = false) override;
};

//  ▄▄▄▄▄▄                ▄▄   ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄ ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██   ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

struct TextFormat {
    enum NewLine {
        LF,
        CRLF,
    };

    UnicodeType unicodeType = UTF8;
    NewLine newLine = LF;
    bool bom = true;
};

TextFormat get_default_utf8_format();
TextFormat autodetectTextFormat(Stream& in);

//  ▄▄▄▄▄ ▄▄ ▄▄▄                               ▄▄
//  ██    ▄▄  ██   ▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀  ██  ██  ██▄▄██ ▀█▄▄▄  ██  ██ ▀█▄▄▄   ██   ██▄▄██ ██ ██ ██
//  ██    ██ ▄██▄ ▀█▄▄▄   ▄▄▄█▀ ▀█▄▄██  ▄▄▄█▀  ▀█▄▄ ▀█▄▄▄  ██ ██ ██
//                               ▄▄▄█▀

enum PathFormat {
    WindowsPath,
    POSIXPath,
};

enum FSResult {
    FS_UNKNOWN = 0,
    FS_NOT_FOUND,
    FS_LOCKED,
    FS_ACCESS_DENIED,
    FS_OK,
    FS_ALREADY_EXISTS,
    FS_UNCHANGED,
};

enum ExistsResult {
    ER_NOT_FOUND,
    ER_FILE,
    ER_DIRECTORY,
};

struct DirectoryEntry {
    FSResult result = FS_UNKNOWN; // Result of getFileInfo()
    String name;
    bool isDir = false;
    u64 fileSize = 0;            // Size of the file in bytes
    double creationTime = 0;     // The file's POSIX creation time
    double accessTime = 0;       // The file's POSIX access time
    double modificationTime = 0; // The file's POSIX modification time

    bool isFile() const {
        return !this->isDir;
    }
};

struct WalkTriple {
    String dirPath;
    Array<String> dirNames;
    Array<DirectoryEntry> files;
};

class DirectoryWalker {
private:
    struct StackItem {
        String path;
        Array<String> dirNames;
        u32 dirIndex;
    };

    friend struct Filesystem;
    WalkTriple triple;
    Array<StackItem> stack;

    void visit(StringView dirPath);

public:
    DirectoryWalker() = default;
    DirectoryWalker(DirectoryWalker&&) = default;

    // Range-for support:
    struct Iterator {
        DirectoryWalker* walker;
        WalkTriple& operator*() {
            return this->walker->triple;
        }
        void operator++();
        bool operator!=(const Iterator&) const {
            return !this->walker->triple.dirPath.isEmpty();
        }
    };
    Iterator begin() {
        return {this};
    }
    Iterator end() {
        return {this};
    }
};

struct Filesystem {
    static ThreadLocal<FSResult> lastResult_;

    static FSResult setLastResult(FSResult result) {
        lastResult_.store(result);
        return result;
    }
    static FSResult lastResult() {
        return lastResult_.load();
    }

#if defined(PLY_WINDOWS)
    static constexpr PathFormat pathFormat() {
        return WindowsPath;
    }

    // Read_Write_Lock used to mitigate data race issues with SetCurrentDirectoryW:
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
    static ReadWriteLock workingDirLock;

    // Direct access to Windows handles:
    static HANDLE openHandleForRead(StringView path);
    static HANDLE openHandleForWrite(StringView path);
    static DirectoryEntry getFileInfo(HANDLE handle);
#elif defined(PLY_POSIX)
    static constexpr PathFormat pathFormat() {
        return POSIXPath;
    }

    static int openFdForRead(StringView path);
    static int openFdForWrite(StringView path);
#endif

    static Array<DirectoryEntry> listDir(StringView path);
    static FSResult makeDir(StringView path);
    static String getWorkingDirectory();
    static FSResult setWorkingDirectory(StringView path);
    static ExistsResult exists(StringView path);
    static Owned<Pipe> openPipeForRead(StringView path);
    static Owned<Pipe> openPipeForWrite(StringView path);
    static FSResult moveFile(StringView srcPath, StringView dstPath);
    static FSResult deleteFile(StringView path);
    static FSResult removeDirTree(StringView dirPath);
    static DirectoryEntry getFileInfo(StringView path);

    static FSResult copyFile(StringView srcPath, StringView dstPath);
    static bool isDir(StringView path) {
        return Filesystem::exists(path) == ER_DIRECTORY;
    }
    static DirectoryWalker walk(StringView top);
    static FSResult makeDirs(StringView path);
    static Stream openBinaryForRead(StringView path);
    static Stream openBinaryForWrite(StringView path);
    static Stream openTextForRead(StringView path, const TextFormat& format = get_default_utf8_format());
    static Stream openTextForReadAutodetect(StringView path, TextFormat* outFormat = nullptr);
    static Stream openTextForWrite(StringView path, const TextFormat& format = get_default_utf8_format());
    static String loadBinary(StringView path);
    static String loadText(StringView path, const TextFormat& format = get_default_utf8_format());
    static String loadTextAutodetect(StringView path, TextFormat* outFormat = nullptr);
    static FSResult saveBinary(StringView path, StringView contents);
    static FSResult saveText(StringView path, StringView strContents,
                             const TextFormat& format = get_default_utf8_format());
};

//  ▄▄▄▄▄          ▄▄   ▄▄
//  ██  ██  ▄▄▄▄  ▄██▄▄ ██▄▄▄
//  ██▀▀▀   ▄▄▄██  ██   ██  ██
//  ██     ▀█▄▄██  ▀█▄▄ ██  ██
//

struct SplitPath {
    StringView directory;
    StringView filename;
};

struct SplitExtension {
    StringView baseName;
    StringView extension;
};

// Generic path manipulation functions:
constexpr char getPathSeparator(PathFormat fmt) {
    return (fmt == PathFormat::WindowsPath) ? '\\' : '/';
}
constexpr bool isPathSeparator(PathFormat fmt, char c) {
    return (c == '/') || ((fmt == PathFormat::WindowsPath) && (c == '\\'));
}
StringView getDriveLetter(PathFormat fmt, StringView path);
bool isAbsolutePath(PathFormat fmt, StringView path);
inline bool isRelativePath(PathFormat fmt, StringView path) {
    return !isAbsolutePath(fmt, path);
}
String makeAbsolutePath(PathFormat fmt, StringView path);
String makeRelativePath(PathFormat fmt, StringView ancestor, StringView descendant);
SplitPath splitPath(PathFormat fmt, StringView path);
SplitExtension splitFileExtension(PathFormat fmt, StringView path);
Array<StringView> splitPathFull(PathFormat fmt, StringView path);
String joinPathFromArray(PathFormat fmt, ArrayView<const StringView> components);
template <typename... StringViews>
String joinPath(PathFormat fmt, StringViews&&... pathComponentArgs) {
    FixedArray<StringView, sizeof...(StringViews)> components{std::forward<StringViews>(pathComponentArgs)...};
    return joinPathFromArray(fmt, components);
}
// Returns true if the string is matched by a single-component glob pattern.
bool matchGlobPattern(StringView str, StringView pattern);
// Returns true if the relative path is matched by a single `.gitignore`-style pattern.
bool matchGitIgnorePattern(StringView relativePath, bool isDir, StringView pattern);

// Native path manipulation functions:
constexpr char getPathSeparator() {
    return getPathSeparator(Filesystem::pathFormat());
}
constexpr bool isPathSeparator(char c) {
    return isPathSeparator(Filesystem::pathFormat(), c);
}
inline StringView getDriveLetter(StringView path) {
    return getDriveLetter(Filesystem::pathFormat(), path);
}
inline bool isAbsolutePath(StringView path) {
    return isAbsolutePath(Filesystem::pathFormat(), path);
}
inline bool isRelativePath(StringView path) {
    return !isAbsolutePath(Filesystem::pathFormat(), path);
}
inline String makeAbsolutePath(StringView path) {
    return makeAbsolutePath(Filesystem::pathFormat(), path);
}
inline String makeRelativePath(StringView ancestor, StringView descendant) {
    return makeRelativePath(Filesystem::pathFormat(), ancestor, descendant);
}
inline SplitPath splitPath(StringView path) {
    return splitPath(Filesystem::pathFormat(), path);
}
inline SplitExtension splitFileExtension(StringView path) {
    return splitFileExtension(Filesystem::pathFormat(), path);
}
inline Array<StringView> splitPathFull(StringView path) {
    return splitPathFull(Filesystem::pathFormat(), path);
}
template <typename... StringViews>
String joinPath(StringViews&&... pathComponentArgs) {
    FixedArray<StringView, sizeof...(StringViews)> components{std::forward<StringViews>(pathComponentArgs)...};
    return joinPathFromArray(Filesystem::pathFormat(), components);
}

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if PLY_WITH_DIRECTORY_WATCHER

class DirectoryWatcher {
public:
    String root;
    Functor<void(StringView path, bool mustRecurse)> callback;

private:
    Thread watcherThread;
#if defined(PLY_WINDOWS)
    HANDLE endEvent = INVALID_HANDLE_VALUE;
#elif defined(PLY_APPLE)
    void* runLoop = nullptr;
#else
#error DirectoryWatcher not supported on this platform!
#endif

    void runWatcher();

public:
    DirectoryWatcher();
    DirectoryWatcher(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback) {
        this->start(root, std::move(callback));
    }
    DirectoryWatcher(StringView root, const Functor<void(StringView path, bool mustRecurse)>& callback) {
        this->start(root, Functor<void(StringView, bool)>{callback});
    }
    ~DirectoryWatcher() {
        this->stop();
    }

    void start(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback);
    void stop();
};

#endif

//   ▄▄▄▄         ▄▄
//  ██  ▀▀ ▄▄  ▄▄ ██▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██  ██ ██  ██ ██  ██ ██  ▀▀ ██  ██ ██    ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ██▄▄█▀ ██▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//                       ██

String getCurrentExecutablePath();

struct Subprocess {
    enum PipeType {
        PIPE_OPEN,
        PIPE_REDIRECT, // This will redirect output to /dev/null if corresponding Out_Pipe
                       // (stdOutPipe/stdErrPipe) is unopened
        PIPE_STD_OUT,
    };

    struct Output {
        PipeType stdOut = PIPE_REDIRECT;
        PipeType stdErr = PIPE_REDIRECT;
        Pipe* stdOutPipe = nullptr;
        Pipe* stdErrPipe = nullptr;

        static Output ignore() {
            return {};
        }
        static Output inherit() {
            Output h;
            h.stdOutPipe = getStdOutPipe();
            h.stdErrPipe = getStdErrPipe();
            return h;
        }
        static Output openSeparate() {
            Output h;
            h.stdOut = PIPE_OPEN;
            h.stdErr = PIPE_OPEN;
            return h;
        }
        static Output openMerged() {
            Output h;
            h.stdOut = PIPE_OPEN;
            h.stdErr = PIPE_STD_OUT;
            return h;
        }
        static Output openStdOutOnly() {
            Output h;
            h.stdOut = PIPE_OPEN;
            return h;
        }
    };

    struct Input {
        PipeType stdIn = PIPE_REDIRECT;
        Pipe* stdInPipe = nullptr;

        static Input ignore() {
            return {};
        }
        static Input inherit() {
            return {PIPE_REDIRECT, getStdInPipe()};
        }
        static Input open() {
            return {PIPE_OPEN, nullptr};
        }
    };

    // Members
    Owned<Pipe> writeToStdIn;
    Owned<Pipe> readFromStdOut;
    Owned<Pipe> readFromStdErr;

#if defined(PLY_WINDOWS)
    HANDLE childProcess = INVALID_HANDLE_VALUE;
    HANDLE childMainThread = INVALID_HANDLE_VALUE;
#elif defined(PLY_POSIX)
    int childPid = -1;
#endif

    Subprocess() = default;

    static Owned<Subprocess> exec(StringView exePath, ArrayView<const StringView> args, StringView initialDir,
                                  const Output& output, const Input& input = Input::open());
    static Owned<Subprocess> execArgStr(StringView exePath, StringView argStr, StringView initialDir,
                                        const Output& output, const Input& input = Input::open());
    s32 join();
};

} // namespace ply
