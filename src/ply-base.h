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
#define PLY_POSIX 1
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

#if defined(_WIN32) // Windows
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
#if defined(__MACH__) // macOS & iOS
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
#define PLY_PUN_GUARD ::ply::PunGuard PLY_UNIQUE_VARIABLE(_pun_guard_)

//--------------------------------------------
// PLY_SET_IN_SCOPE
//--------------------------------------------

template <typename T, typename V>
struct SetInScope {
    T& target;                            // The variable to set/reset
    std::remove_reference_t<T> old_value; // Backup of original value
    const V& new_value_ref;               // Extends the lifetime of temporary values in the case of
                                          // eg. Set_In_Scope<String_View, String>

    template <typename U>
    SetInScope(T& target, U&& new_value) : target{target}, old_value{std::move(target)}, new_value_ref{new_value} {
        target = std::forward<U>(new_value);
    }
    ~SetInScope() {
        this->target = std::move(this->old_value);
    }
};

#define PLY_SET_IN_SCOPE(target, value) \
    SetInScope<decltype(target), decltype(value)> PLY_UNIQUE_VARIABLE(set_in_scope) { \
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
OnScopeExit<Callback> set_on_scope_exit(Callback&& cb) {
    return {std::forward<Callback>(cb)};
}
#define PLY_ON_SCOPE_EXIT(cb) auto PLY_UNIQUE_VARIABLE(on_scope_exit) = set_on_scope_exit([&] cb)

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
struct enable_if_bool;
template <>
struct enable_if_bool<true> {
    using type = int;
};
#define PLY_ENABLE_IF(x) typename ::ply::enable_if_bool<(x)>::type = 0

template <typename>
struct enable_if_type {
    using type = int;
};
#define PLY_ENABLE_IF_WELL_FORMED(x) typename ::ply::enable_if_type<decltype(x)>::type = 0

// Why doesn't this work reliably without void_t<...>?
// In particular, has_get_lookup_key_member stops working, seemingly because the member function returns non-void.
#define PLY_CHECK_WELL_FORMED(name, expr) \
    template <typename, typename = void> \
    static constexpr bool name = false; \
    template <typename T> \
    static constexpr bool name<T, void_t<decltype(expr)>> = true;

template <typename T>
constexpr bool is_unsigned = (T(0) < T(-1));

//  ▄▄  ▄▄                               ▄▄
//  ███ ██ ▄▄  ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄
//  ██▀███ ██  ██ ██ ██ ██ ██▄▄██ ██  ▀▀ ██ ██    ▀█▄▄▄
//  ██  ██ ▀█▄▄██ ██ ██ ██ ▀█▄▄▄  ██     ██ ▀█▄▄▄  ▄▄▄█▀
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
constexpr T get_min_value();
template <typename T>
constexpr T get_max_value();
#define PLY_MAKE_LIMITS(T, lo, hi) \
    template <> \
    constexpr T get_min_value<T>() { \
        return lo; \
    } \
    template <> \
    constexpr T get_max_value<T>() { \
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
inline constexpr u16 reverse_bytes(u16 val) {
    return ((val >> 8) & 0xff) | ((val << 8) & 0xff00);
}
inline constexpr u32 reverse_bytes(u32 val) {
    return ((val >> 24) & 0xff) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000) | ((val << 24) & 0xff000000u);
}
inline constexpr u64 reverse_bytes(u64 val) {
    return ((u64) reverse_bytes(u32(val)) << 32) | reverse_bytes(u32(val >> 32));
}
#if PLY_IS_BIG_ENDIAN
template <typename Type>
inline constexpr Type convert_little_endian(Type val) {
    return reverse_bytes(val);
}
template <typename Type>
inline constexpr Type convert_big_endian(Type val) {
    return val;
}
#else
template <typename Type>
inline constexpr Type convert_little_endian(Type val) {
    return val;
}
template <typename Type>
inline constexpr Type convert_big_endian(Type val) {
    return reverse_bytes(val);
}
#endif
inline u32 is_power_of_2(u32 val) {
    return (val & (val - 1)) == 0;
}
inline u64 is_power_of_2(u64 val) {
    return (val & (val - 1)) == 0;
}
inline u32 align_to_power_of_2(u32 v, u32 a) {
    PLY_ASSERT(is_power_of_2(a));
    return (v + a - 1) & ~(a - 1);
}
inline u64 align_to_power_of_2(u64 v, u64 a) {
    PLY_ASSERT(is_power_of_2(a));
    return (v + a - 1) & ~(a - 1);
}
inline bool is_aligned_to_power_of_2(u32 v, u32 a) {
    PLY_ASSERT(is_power_of_2(a));
    return (v & (a - 1)) == 0;
}
inline bool is_aligned_to_power_of_2(u64 v, u64 a) {
    PLY_ASSERT(is_power_of_2(a));
    return (v & (a - 1)) == 0;
}
inline constexpr u32 round_up_to_nearest_to_power_of_2(u32 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}
inline constexpr u64 round_up_to_nearest_to_power_of_2(u64 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return v + 1;
}
template <typename DstType, typename SrcType, PLY_ENABLE_IF_WELL_FORMED(declval<DstType>() == declval<SrcType>())>
constexpr DstType numeric_cast(SrcType val) {
    PLY_ASSERT((DstType) val == val);
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
// its time_zone_offset_in_minutes member, which is relative to Coordinated Universal Time (UTC). For example, a
// time_zone_offset_in_minutes of -300 corresponds to Eastern Standard Time (EST), which is 5 hours behind UTC.
struct DateTime {
    s32 year = 0;
    u8 month = 0;   // 1..12
    u8 day = 0;     // 1..31
    u8 weekday = 0; // Sunday = 0, Saturday = 6
    u8 hour = 0;    // 0..23
    u8 minute = 0;  // 0..59
    u8 second = 0;  // 0..59
    s16 time_zone_offset_in_minutes = 0;
    u32 microsecond = 0; // 0..999999
};

// Returns the current number of microseconds since Jan 1 1970, 00:00 UTC according to the system clock.
s64 get_unix_timestamp();

// Converts a Unix timestamp to a DateTime object.
DateTime convert_to_date_time(s64 system_time);
DateTime convert_to_date_time(s64 system_time, s16 time_zone_offset_in_minutes);

// Converts a DateTime object back to a Unix timestamp.
s64 convert_to_unix_timestamp(const DateTime& date_time);

//----------------------------------------------------
// High-Resolution Timer
//----------------------------------------------------

// Returns a high-resolution CPU timestamp.
inline u64 get_cpu_ticks() {
#if defined(_WIN32)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
#elif defined(__MACH__)
    return mach_absolute_time();
#elif defined(PLY_POSIX)
    struct timespec tick;
    clock_gettime(CLOCK_MONOTONIC, &tick);
    return (u64) tick.tv_sec * 1000000000ull + tick.tv_nsec;
#endif
}

// Returns the high-resolution timer frequency. To measure an interval of time in seconds, subtract two timestamps and
// divide the result by this value.
float get_cpu_ticks_per_second();

//  ▄▄▄▄▄                    ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀█▄  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██ ██ ██
//  ██  ██ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄█▀ ██ ██ ██
//

// Random is a class that generates pseudorandom numbers using the xoroshiro128** algorithm, as
// described here: http://xorshift.di.unimi.it/
// generate_u64() returns a uniformly distributed pseudorandom 64-bit integer. You can map the returned value
// to a smaller range by using the modulo operator or by discarding upper bits.
// generate_float() is a convenience function that uses generate_u64() to generate a uniformly distributed
// pseudorandom floating-point value between [0.0f, 1.0f).
// You can optionally specify a 64-bit integer seed when constructing an instance of Random. This makes the number
// sequence generated by generate_u64() deterministic. Two Random instances constructed using the same seed always
// generate the same number sequence. The default constructor is self-seeding. The seed is calculated using various
// information from the runtime environment, such as the current time and thread ID, so that two default-constructed
// Random instances are unlikely to generate the same number sequence.

class Random {
private:
    u64 s[2];

public:
    Random();
    Random(u64 seed);
    u64 generate_u64();
    u32 generate_u32() {
        return (u32) this->generate_u64();
    }
    float generate_float() {
        return (u32) generate_u64() / 4294967296.f;
    }
};

//  ▄▄▄▄▄▄ ▄▄                              ▄▄     ▄▄▄▄ ▄▄▄▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██      ██  ██  ██  ▄▄▄▄
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██      ██  ██  ██ ▀█▄▄▄
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██     ▄██▄ ██▄▄█▀  ▄▄▄█▀
//

// get_current_thread_id() and get_current_process_id() return the current thread and process ID.
// TID and PID are type aliases for either u32 or u64, depending on the platform.

#if defined(_WIN32)

using TID = u32;
using PID = u32;

inline TID get_current_thread_id() {
#if defined(_M_X64)
    return ((DWORD*) __readgsqword(48))[18];
#elif defined(_M_IX86)
    return ((DWORD*) __readfsdword(24))[9];
#else
    return GetCurrentThreadID();
#endif
}

inline PID get_current_process_id() {
#if defined(_M_X64)
    return ((DWORD*) __readgsqword(48))[16];
#elif defined(_M_IX86)
    return ((DWORD*) __readfsdword(24))[8];
#else
    return GetCurrentProcessID();
#endif
}

#elif defined(__MACH__)

using TID = std::conditional_t<sizeof(thread_port_t) == 4, u32, u64>;
using PID = std::conditional_t<sizeof(pid_t) == 4, u32, u64>;

inline TID get_current_thread_id() {
    return pthread_mach_thread_np(pthread_self());
}

inline PID get_current_process_id() {
    return getpid();
}

#elif defined(PLY_POSIX)

using TID = std::conditional_t<sizeof(pthread_t) == 4, u32, u64>;
using PID = std::conditional_t<sizeof(pid_t) == 4, u32, u64>;

inline TID get_current_thread_id() {
#if defined(__FreeBSD__)
    return pthread_getthreadid_np();
#elif defined(PLY_MINGW)
    return (TID) pthread_self().p;
#else
    return pthread_self();
#endif
}

inline PID get_current_process_id() {
    return getpid();
}

#endif

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

// Suspends the calling thread for the specified number of milliseconds.
inline void sleep_millis(u32 millis) {
#if defined(_WIN32)
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

#if defined(_WIN32)

DWORD WINAPI thread_entry(LPVOID param);

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
    bool is_valid() const {
        return this->handle != INVALID_HANDLE_VALUE;
    }
    void run(Functor<void()>&& entry);
    void join() {
        PLY_ASSERT(this->handle != INVALID_HANDLE_VALUE);
        WaitForSingleObject(this->handle, INFINITE);
        CloseHandle(this->handle);
        this->handle = INVALID_HANDLE_VALUE;
    }
};

#elif defined(PLY_POSIX)

void* thread_entry(void*);

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
            pthread_detach(this->handle);
        }
    }
    bool is_valid() {
        return this->attached;
    }
    void run(Functor<void()>&& entry);
    void join() {
        PLY_ASSERT(this->attached);
        void* ret_val = nullptr;
        pthread_join(this->handle, &ret_val);
        this->attached = false;
    }
};

#endif

inline Thread spawn_thread(Functor<void()>&& entry) {
    return Thread{std::move(entry)};
}

//   ▄▄▄▄   ▄▄                   ▄▄
//  ██  ██ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄  ▄▄▄▄
//  ██▀▀██  ██   ██  ██ ██ ██ ██ ██ ██
//  ██  ██  ▀█▄▄ ▀█▄▄█▀ ██ ██ ██ ██ ▀█▄▄▄
//

template <typename T, int = sizeof(T)>
class Atomic;

#if defined(_MSC_VER)

//----------------------------------------------------
// MSVC implementation.
template <typename T>
class Atomic<T, 4> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 4>& other) : value{other.value} {
    }
    // Hide operator=
    Atomic& operator=(T value) = delete;
    Atomic& operator=(const Atomic<T, 4>& other) {
        this->value = other.value;
        return *this;
    }
    T load_nonatomic() const {
        return this->value;
    }
    T load_acquire() const {
        T result = *(volatile T*) &this->value;
        _ReadWriteBarrier();
        return result;
    }
    void store_nonatomic(T value) {
        this->value = value;
    }
    void store_release(T value) {
        _ReadWriteBarrier();
        *(volatile T*) &this->value = value;
    }
    T compare_exchange_acq_rel(T expected, T desired) {
        return (T) _InterlockedCompareExchange((volatile long*) &this->value, (long) desired, (long) expected);
    }
    T exchange_acq_rel(T desired) {
        return (T) _InterlockedExchange((volatile long*) &this->value, (long) desired);
    }
    T fetch_add_acq_rel(T operand) {
        return (T) _InterlockedExchangeAdd((volatile long*) &this->value, (long) operand);
    }
    T fetch_sub_acq_rel(T operand) {
        return (T) _InterlockedExchangeAdd((volatile long*) &this->value, -(long) operand);
    }
    T fetch_and_acq_rel(T operand) {
        return (T) _InterlockedAnd((volatile long*) &this->value, (long) operand);
    }
    T fetch_or_acq_rel(T operand) {
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
    Atomic(const Atomic<T, 8>& other) : value{other.value} {
    }
    // Hide operator=
    Atomic& operator=(T value) = delete;
    Atomic& operator=(const Atomic<T, 8>& other) {
        this->value = other.value;
        return *this;
    }
    T load_nonatomic() const {
        return this->value;
    }
    T load_acquire() const {
#if PLY_PTR_SIZE == 8
        T result = *(volatile T*) &this->value;
        _ReadWriteBarrier();
        return result;
#else
        return _InterlockedCompareExchange64_acq((volatile __int64*) &this->value, 0, 0);
#endif
    }
    void store_nonatomic(T value) {
        this->value = value;
    }
    void store_release(T value) {
#if PLY_PTR_SIZE == 8
        _ReadWriteBarrier();
        *(volatile T*) &this->value = value;
#else
        _InterlockedExchange64_rel((volatile __int64*) &this->value, value);
#endif
    }
    T compare_exchange_acq_rel(T expected, T desired) {
        return (T) _InterlockedCompareExchange64((volatile __int64*) &this->value, (__int64) desired,
                                                 (__int64) expected);
    }
    T exchange_acq_rel(T desired) {
        return (T) _InterlockedExchange64((volatile __int64*) &this->value, (__int64) desired);
    }
    T fetch_add_acq_rel(T operand) {
        return (T) _InterlockedAdd64((volatile __int64*) &this->value, (__int64) operand);
    }
    T fetch_sub_acq_rel(T operand) {
        return (T) _InterlockedAdd64((volatile __int64*) &this->value, -(__int64) operand);
    }
    T fetch_and_acq_rel(T operand) {
        return (T) _InterlockedAnd64((volatile __int64*) &this->value, (__int64) operand);
    }
    T fetch_or_acq_rel(T operand) {
        return (T) _InterlockedOr64((volatile __int64*) &this->value, (__int64) operand);
    }
};

#elif defined(__GNUC__)

//----------------------------------------------------
// GCC/Clang implementation.
template <typename T>
class Atomic<T, 4> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 4>& other) : value{other.value} {
    }
    // Hide operator=
    Atomic& operator=(T value) = delete;
    Atomic& operator=(const Atomic<T, 4>& other) {
        this->value = other.value;
        return *this;
    }
    T load_nonatomic() const {
        return this->value;
    }
    T load_acquire() const {
        return __atomic_load_n(&this->value, __ATOMIC_ACQUIRE);
    }
    void store_nonatomic(T value) {
        this->value = value;
    }
    void store_release(T value) {
        __atomic_store_n(&this->value, value, __ATOMIC_RELEASE);
    }
    T compare_exchange_acq_rel(T expected, T desired) {
        __atomic_compare_exchange(&this->value, &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQ_REL);
        return expected;
    }
    T exchange_acq_rel(T desired) {
        return __atomic_exchange(&this->value, desired, __ATOMIC_ACQ_REL);
    }
    T fetch_add_acq_rel(T operand) {
        return __atomic_fetch_add(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_sub_acq_rel(T operand) {
        return __atomic_fetch_sub(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_and_acq_rel(T operand) {
        return __atomic_fetch_and(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_or_acq_rel(T operand) {
        return __atomic_fetch_or(&this->value, operand, __ATOMIC_ACQ_REL);
    }
};

template <typename T>
class Atomic<T, 8> {
protected:
    T value = 0;

public:
    Atomic(T value = 0) : value{value} {
    }
    Atomic(const Atomic<T, 4>& other) : value{other.value} {
    }
    // Hide operator=
    Atomic& operator=(T value) = delete;
    Atomic& operator=(const Atomic<T, 4>& other) {
        this->value = other.value;
        return *this;
    }
    T load_nonatomic() const {
        return this->value;
    }
    T load_acquire() const {
        return __atomic_load(&this->value, __ATOMIC_ACQUIRE);
    }
    void store_nonatomic(T value) {
        this->value = value;
    }
    void store_release(T value) {
        __atomic_store(&this->value, value, __ATOMIC_RELEASE);
    }
    T compare_exchange_acq_rel(T expected, T desired) {
        __atomic_compare_exchange(&this->value, &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQ_REL);
        return expected;
    }
    T exchange_acq_rel(T desired) {
        return __atomic_exchange(&this->value, desired, __ATOMIC_ACQ_REL);
    }
    T fetch_add_acq_rel(T operand) {
        return __atomic_fetch_add(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_sub_acq_rel(T operand) {
        return __atomic_fetch_sub(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_and_acq_rel(T operand) {
        return __atomic_fetch_and(&this->value, operand, __ATOMIC_ACQ_REL);
    }
    T fetch_or_acq_rel(T operand) {
        return __atomic_fetch_or(&this->value, operand, __ATOMIC_ACQ_REL);
    }
};

#endif

//  ▄▄▄▄▄▄ ▄▄                              ▄▄ ▄▄                        ▄▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ██     ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██ ██    ██  ██ ██     ▄▄▄██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██ ▄██▄
//

// Used as the return value of Thread_Local::set_in_scope()
template <template <typename> class TL, typename T>
class ThreadLocalScope {
private:
    TL<T>* var;
    T old_value;

public:
    ThreadLocalScope(TL<T>* var, T new_value) : var{var} {
        this->old_value = var->load();
        var->store(new_value);
    }

    ThreadLocalScope(const ThreadLocalScope&) = delete;
    ThreadLocalScope(ThreadLocalScope&& other) {
        this->var = other->var;
        this->old_value = std::move(other.old_value);
        other->var = nullptr;
    }

    ~ThreadLocalScope() {
        if (this->var) {
            this->var->store(this->old_value);
        }
    }
};

#if defined(_WIN32)

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

    // In C++11, you can write auto scope = my_tlvar.set_in_scope(value);
    using Scope = ThreadLocalScope<ThreadLocal, T>;
    Scope set_in_scope(T value) {
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

    // In C++11, you can write auto scope = my_tlvar.set_in_scope(value);
    using Scope = ThreadLocalScope<ThreadLocal, T>;
    Scope set_in_scope(T value) {
        return {this, value};
    }
};

#endif

//  ▄▄   ▄▄         ▄▄
//  ███▄███ ▄▄  ▄▄ ▄██▄▄  ▄▄▄▄  ▄▄  ▄▄
//  ██▀█▀██ ██  ██  ██   ██▄▄██  ▀██▀
//  ██   ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄  ▄█▀▀█▄
//

#if defined(_WIN32)

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
    bool try_lock() {
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
    bool try_lock() {
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

#if defined(_WIN32)

//----------------------------------------------------
// Windows implementation.
class ConditionVariable {
private:
    CONDITION_VARIABLE cond_var;

public:
    ConditionVariable() {
        InitializeConditionVariable(&cond_var);
    }
    void wait(LockGuard<Mutex>& lock_guard) {
        SleepConditionVariableSRW(&cond_var, &lock_guard.mutex.srwlock, INFINITE, 0);
    }
    void timed_wait(LockGuard<Mutex>& lock_guard, u32 wait_millis) {
        if (wait_millis > 0) {
            SleepConditionVariableSRW(&cond_var, &lock_guard.mutex.srwlock, wait_millis, 0);
        }
    }
    void wake_one() {
        WakeConditionVariable(&cond_var);
    }
    void wake_all() {
        WakeAllConditionVariable(&cond_var);
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
    void wait(LockGuard<Mutex>& lock_guard) {
        pthread_cond_wait(&cond, &lock_guard.mutex.mutex);
    }
    void timed_wait(LockGuard<Mutex>& lock_guard, u32 wait_millis) {
        if (wait_millis > 0) {
            struct timespec ts;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec + wait_millis / 1000;
            ts.tv_nsec = (tv.tv_usec + (wait_millis % 1000) * 1000) * 1000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&cond, &lock_guard.mutex.mutex, &ts);
        }
    }
    void wake_one() {
        pthread_cond_signal(&cond);
    }
    void wake_all() {
        pthread_cond_broadcast(&cond);
    }
};

#endif

//  ▄▄▄▄▄                    ▄▄ ▄▄    ▄▄        ▄▄  ▄▄          ▄▄                 ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄  ██     ▄▄▄▄   ▄▄▄▄ ██  ▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██▄▄██ ██    ██  ██ ██    ██▄█▀
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██  ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄
//

#if defined(_WIN32)

//----------------------------------------------------
// Windows implementation.
struct ReadWriteLock {
    SRWLOCK srw_lock;

    ReadWriteLock() {
        InitializeSRWLock(&this->srw_lock);
    }
    ~ReadWriteLock() {
        // SRW locks do not need to be destroyed.
    }
    void lock_exclusive() {
        AcquireSRWLockExclusive(&this->srw_lock);
    }
    void unlock_exclusive() {
        ReleaseSRWLockExclusive(&this->srw_lock);
    }
    void lock_shared() {
        AcquireSRWLockShared(&this->srw_lock);
    }
    void unlock_shared() {
        ReleaseSRWLockShared(&this->srw_lock);
    }
};

#elif defined(PLY_POSIX)

//----------------------------------------------------
// POSIX implementation.
struct ReadWriteLock {
    pthread_rwlock_t rw_lock;

    ReadWriteLock() {
        pthread_rwlock_init(&this->rw_lock, NULL);
    }
    ~ReadWriteLock() {
        pthread_rwlock_destroy(&this->rw_lock);
    }
    void lock_exclusive() {
        pthread_rwlock_wrlock(&this->rw_lock);
    }
    void unlock_exclusive() {
        pthread_rwlock_unlock(&this->rw_lock);
    }
    void lock_shared() {
        pthread_rwlock_rdlock(&this->rw_lock);
    }
    void unlock_shared() {
        pthread_rwlock_unlock(&this->rw_lock);
    }
};

#endif

//   ▄▄▄▄                                ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██▄▄██ ██ ██ ██  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██  ▀▀ ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄▄  ██ ██ ██ ▀█▄▄██ ██▄▄█▀ ██  ██ ▀█▄▄█▀ ██     ▀█▄▄▄
//                                ██

#if defined(_WIN32)

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

#elif defined(__MACH__)

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
    struct Info {
        uptr alloc_alignment;
        uptr page_size;
    };

    static Info get_info();
    static bool alloc(char*& out_addr, uptr num_bytes);
    static bool reserve(char*& out_addr, uptr num_bytes);
    static void commit(char* addr, uptr num_bytes);
    static void decommit(char* addr, uptr num_bytes);
    static void free(char* addr, uptr num_bytes); // must not be a combined region
};

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

} // namespace ply

extern "C" {
void* dlmalloc(ply::uptr);
void* dlrealloc(void*, ply::uptr);
void dlfree(void*);
void* dlmemalign(ply::uptr, ply::uptr);
};

namespace ply {

struct Heap {
    static void* alloc(uptr num_bytes) {
        return dlmalloc(num_bytes);
    }
    static void* realloc(void* ptr, uptr num_bytes) {
        return dlrealloc(ptr, num_bytes);
    }
    static void free(void* ptr) {
        dlfree(ptr);
    }
    static void* alloc_aligned(uptr num_bytes, u32 alignment) {
        return dlmemalign(num_bytes, alignment);
    }

    // Perfect forwarding
    template <typename T, typename... Args>
    static T* create(Args&&... args) {
        T* obj = (T*) alloc(sizeof(T));

        new (obj) T{std::forward<Args>(args)...};
        return obj;
    }

    template <typename T>
    static void destroy(T* obj) {
        obj->~T();
        free(obj);
    }
};

#if defined(_WIN32)

inline void Thread::run(Functor<void()>&& entry) {
    PLY_ASSERT(this->handle == INVALID_HANDLE_VALUE);
    auto* functor = Heap::create<Functor<void()>>(std::move(entry));
    this->handle = CreateThread(NULL, 0, thread_entry, functor, 0, NULL);
}

#elif defined(PLY_POSIX)

inline void Thread::run(Functor<void()>&& entry) {
    PLY_ASSERT(!this->attached);
    auto* functor = Heap::create<Functor<void()>>(std::move(entry));
    pthread_create(&this->handle, NULL, thread_entry, functor);
    this->attached = true;
}

#endif

//   ▄▄▄▄   ▄▄          ▄▄               ▄▄   ▄▄ ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄ ██   ██ ▄▄  ▄▄▄▄  ▄▄    ▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██  ██ ██  ██ ██▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██   ▀█▀   ██ ▀█▄▄▄   ██▀▀██
//                                 ▄▄▄█▀

struct String;
template <typename>
class ArrayView;
template <typename>
class Array;

inline bool is_whitespace(char c) {
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}
inline bool is_ascii_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
inline bool is_decimal_digit(char c) {
    return (c >= '0' && c <= '9');
}

class StringView {
private:
    const char* bytes_ = nullptr;
    u32 num_bytes_ = 0;

public:
    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    StringView() = default;
    StringView(const char* s) : bytes_{s}, num_bytes_{numeric_cast<u32>(::strlen(s))} {
    }
    StringView(const char* bytes, u32 num_bytes) : bytes_{bytes}, num_bytes_{num_bytes} {
    }
    StringView(const char* start_byte, const char* end_byte)
        : bytes_{start_byte}, num_bytes_{numeric_cast<u32>(end_byte - start_byte)} {
    }
    StringView(const char& c) : bytes_{&c}, num_bytes_{1} {
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
    u32 num_bytes() const {
        return this->num_bytes_;
    }
    const char& operator[](u32 index) const {
        PLY_ASSERT(index < this->num_bytes_);
        return this->bytes_[index];
    }
    const char& back(s32 ofs = -1) const {
        PLY_ASSERT(u32(-ofs - 1) < this->num_bytes_);
        return this->bytes_[this->num_bytes_ + ofs];
    }

    //----------------------------------------------------
    // Examining string contents
    //----------------------------------------------------

    bool is_empty() const {
        return this->num_bytes_ == 0;
    }
    explicit operator bool() const {
        return this->num_bytes_ != 0;
    }
    bool starts_with(StringView arg) const;
    bool ends_with(StringView arg) const;
    s32 find(StringView pattern, u32 start_pos = 0) const;
    template <typename Callable, PLY_ENABLE_IF_WELL_FORMED(declval<Callable>()(declval<char>()))>
    s32 find(const Callable& match_func, u32 start_pos = 0) const {
        for (u32 i = start_pos; i < this->num_bytes_; i++) {
            if (match_func(this->bytes_[i]))
                return i;
        }
        return -1;
    }
    s32 reverse_find(StringView pattern, s32 start_pos = -1) const;
    template <typename Callable, PLY_ENABLE_IF_WELL_FORMED(declval<Callable>()(declval<char>()))>
    s32 reverse_find(const Callable& match_func, s32 start_pos = -1) const {
        if (start_pos < 0) {
            start_pos += this->num_bytes_;
        }
        for (s32 i = start_pos; i >= 0; i--) {
            if (match_func(this->bytes_[i]))
                return i;
        }
        return -1;
    }
    StringView substr(u32 start) const {
        start = min(start, this->num_bytes_);
        return {this->bytes_ + start, this->num_bytes_ - start};
    }
    StringView substr(u32 start, u32 num_bytes) const {
        start = min(start, this->num_bytes_);
        num_bytes = min(num_bytes, this->num_bytes_ - start);
        return {this->bytes_ + start, num_bytes};
    }
    StringView left(u32 num_bytes) const {
        num_bytes = min(num_bytes, this->num_bytes_);
        return {this->bytes_, num_bytes};
    }
    StringView shortened_by(u32 num_bytes) const {
        num_bytes = min(num_bytes, this->num_bytes_);
        return {this->bytes_, this->num_bytes_ - num_bytes};
    }
    StringView right(u32 num_bytes) const {
        num_bytes = min(num_bytes, this->num_bytes_);
        return {this->bytes_ + this->num_bytes_ - num_bytes, num_bytes};
    }
    StringView trim(bool (*match_func)(char) = is_whitespace, bool left = true, bool right = true) const;
    StringView trim_left(bool (*match_func)(char) = is_whitespace) const {
        return this->trim(match_func, true, false);
    }
    StringView trim_right(bool (*match_func)(char) = is_whitespace) const {
        return this->trim(match_func, false, true);
    }

    //----------------------------------------------------
    // Creating new strings
    //----------------------------------------------------

    PLY_NO_DISCARD String upper() const;
    PLY_NO_DISCARD String lower() const;
    PLY_NO_DISCARD Array<StringView> split_byte(char sep) const;
    PLY_NO_DISCARD String join(ArrayView<const StringView> comps) const;
    PLY_NO_DISCARD String replace(StringView old_substr, StringView new_substr) const;

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
    u32 num_bytes = 0;

    MutStringView() = default;
    MutStringView(char* bytes, u32 num_bytes) : bytes{bytes}, num_bytes{num_bytes} {
    }
    MutStringView(char* start_byte, char* end_byte)
        : bytes{start_byte}, num_bytes{numeric_cast<u32>(end_byte - start_byte)} {
    }
    operator const StringView&() const {
        return reinterpret_cast<const StringView&>(*this);
    }

    char* end() {
        return this->bytes + this->num_bytes;
    }
    MutStringView subview(u32 num_bytes) const {
        PLY_ASSERT(num_bytes <= this->num_bytes);
        return {this->bytes + num_bytes, this->num_bytes - num_bytes};
    }
    MutStringView shortened_by(s32 ofs) {
        PLY_ASSERT((u32) -ofs <= this->num_bytes);
        return {this->bytes, this->num_bytes += ofs};
    }
};

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

struct String {
    char* bytes = nullptr;
    u32 num_bytes = 0;

    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    String() = default;
    String(const String& other) : String{StringView{other}} {
    }
    String(String&& other) : bytes{other.bytes}, num_bytes{other.num_bytes} {
        other.bytes = nullptr;
        other.num_bytes = 0;
    }
    String(StringView other);
    String(const char* s) : String{StringView{s}} { // Needed?
    }
    static String allocate(u32 num_bytes);
    static String adopt(char* bytes, u32 num_bytes) {
        String str;
        str.bytes = bytes;
        str.num_bytes = num_bytes;
        return str;
    }
    ~String() {
        if (this->bytes) {
            Heap::free(this->bytes);
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
    void operator+=(StringView other) {
        *this = *this + other;
    }

    //----------------------------------------------------
    // Type conversions
    //----------------------------------------------------

    operator StringView() const {
        return {this->bytes, this->num_bytes};
    }
    explicit operator bool() const {
        return this->num_bytes != 0;
    }

    //----------------------------------------------------
    // Accessing string bytes
    //----------------------------------------------------

    const char& operator[](u32 index) const {
        PLY_ASSERT(index < this->num_bytes);
        return this->bytes[index];
    }
    char& operator[](u32 index) {
        PLY_ASSERT(index < this->num_bytes);
        return this->bytes[index];
    }
    const char& back(s32 ofs = -1) const {
        PLY_ASSERT(u32(-ofs - 1) < this->num_bytes);
        return this->bytes[this->num_bytes + ofs];
    }
    char& back(s32 ofs = -1) {
        PLY_ASSERT(u32(-ofs - 1) < this->num_bytes);
        return this->bytes[this->num_bytes + ofs];
    }
    char* begin() {
        return this->bytes;
    }
    const char* begin() const {
        return this->bytes;
    }
    char* end() {
        return this->bytes + this->num_bytes;
    }
    const char* end() const {
        return this->bytes + this->num_bytes;
    }

    //----------------------------------------------------
    // Examining string contents
    //----------------------------------------------------

    bool is_empty() const {
        return this->num_bytes == 0;
    }
    bool starts_with(StringView arg) const {
        return ((StringView) * this).starts_with(arg);
    }
    bool ends_with(StringView arg) const {
        return ((StringView) * this).ends_with(arg);
    }
    s32 find(StringView pattern, u32 start_pos = 0) const {
        return ((StringView) * this).find(pattern, start_pos);
    }
    template <typename Callable>
    s32 find(const Callable& match_func, u32 start_pos = 0) const {
        return ((StringView) * this).find(match_func, start_pos);
    }
    s32 reverse_find(StringView pattern, s32 start_pos = -1) const {
        return ((StringView) * this).reverse_find(pattern, start_pos);
    }
    template <typename Callable>
    s32 reverse_find(const Callable& match_func, s32 start_pos = -1) const {
        return ((StringView) * this).reverse_find(match_func, start_pos);
    }

    //----------------------------------------------------
    // Creating subviews
    //----------------------------------------------------

    StringView substr(u32 start) const {
        PLY_ASSERT(start <= num_bytes);
        return {this->bytes + start, this->num_bytes - start};
    }
    StringView substr(u32 start, u32 num_bytes) const {
        PLY_ASSERT(start <= this->num_bytes);
        PLY_ASSERT(start + num_bytes <= this->num_bytes);
        return {this->bytes + start, num_bytes};
    }
    StringView left(u32 num_bytes) const {
        PLY_ASSERT(num_bytes <= this->num_bytes);
        return {this->bytes, num_bytes};
    }
    StringView shortened_by(u32 num_bytes) const {
        PLY_ASSERT(num_bytes <= this->num_bytes);
        return {this->bytes, this->num_bytes - num_bytes};
    }
    StringView right(u32 num_bytes) const {
        PLY_ASSERT(num_bytes <= this->num_bytes);
        return {this->bytes + this->num_bytes - num_bytes, num_bytes};
    }
    StringView trim(bool (*match_func)(char) = is_whitespace, bool left = true, bool right = true) const {
        return ((StringView) * this).trim(match_func, left, right);
    }
    StringView trim_left(bool (*match_func)(char) = is_whitespace) const {
        return ((StringView) * this).trim(match_func, true, false);
    }
    StringView trim_right(bool (*match_func)(char) = is_whitespace) const {
        return ((StringView) * this).trim(match_func, false, true);
    }

    //----------------------------------------------------
    // Creating new strings
    //----------------------------------------------------

    PLY_NO_DISCARD String replace(StringView old_substr, StringView new_substr) const {
        return ((StringView) * this).replace(old_substr, new_substr);
    }
    PLY_NO_DISCARD String upper() const {
        return ((StringView) * this).upper();
    }
    PLY_NO_DISCARD String lower() const {
        return ((StringView) * this).lower();
    }
    PLY_NO_DISCARD Array<StringView> split_byte(char sep) const;
    PLY_NO_DISCARD String join(ArrayView<const StringView> comps) const;

    //----------------------------------------------------
    // Modifying the string
    //----------------------------------------------------

    void clear() {
        if (this->bytes) {
            Heap::free(this->bytes);
        }
        this->bytes = nullptr;
        this->num_bytes = 0;
    }
    void resize(u32 num_bytes);
    char* release() {
        char* r = this->bytes;
        this->bytes = nullptr;
        this->num_bytes = 0;
        return r;
    }

    //----------------------------------------------------
    // Formatting and matching
    //----------------------------------------------------

    template <typename... Args>
    static String format(StringView fmt, const Args&... args);
    template <typename... Args>
    bool match(StringView pattern, Args*... args) const;
};

inline StringView get_any_lookup_key(const String& str) {
    return str;
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

// shuffle_bits is a helper function that takes a 32-bit or 64-bit integer and mixes the bits.
// The implementation is taken from MurmurHash3's fmix32 and fmix64 functions:
// https://github.com/aappleby/smhasher/blob/0ff96f7835817a27d0487325b6c16033e2992eb5/src/MurmurHash3.cpp#L68-L90.

inline u32 shuffle_bits(u32 h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline u32 unshuffle_bits(u32 h) {
    h ^= h >> 16;
    h *= 0x7ed1b41d;
    h ^= (h ^ (h >> 13)) >> 13;
    h *= 0xa5cb9243;
    h ^= h >> 16;
    return h;
}

inline u64 shuffle_bits(u64 h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

inline u64 unshuffle_bits(u64 h) {
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

    u32 get_result() const {
        return shuffle_bits(this->accumulator);
    }
};

void add_to_hash(HashBuilder& builder, u32 value);
inline void add_to_hash(HashBuilder& builder, u8 value) {
    add_to_hash(builder, (u32) value);
}
inline void add_to_hash(HashBuilder& builder, u16 value) {
    add_to_hash(builder, (u32) value);
}
inline void add_to_hash(HashBuilder& builder, u64 value) {
    add_to_hash(builder, (u32) value);
    add_to_hash(builder, (u32) (value >> 32));
}
inline void add_to_hash(HashBuilder& builder, s8 value) {
    add_to_hash(builder, (u32) value);
}
inline void add_to_hash(HashBuilder& builder, s16 value) {
    add_to_hash(builder, (u32) value);
}
inline void add_to_hash(HashBuilder& builder, s32 value) {
    add_to_hash(builder, (u32) value);
}
inline void add_to_hash(HashBuilder& builder, s64 value) {
    add_to_hash(builder, (u64) value);
}
inline void add_to_hash(HashBuilder& builder, float value) {
    PLY_PUN_GUARD;
    add_to_hash(builder, *(u32*) &value);
}
inline void add_to_hash(HashBuilder& builder, double value) {
    PLY_PUN_GUARD;
    add_to_hash(builder, *(u64*) &value);
}
void add_to_hash(HashBuilder& builder, StringView str);

//----------------------------------------------------
// calculate_hash() is a wrapper around HashBuilder that's used internally by Map and Set.

template <typename T>
u32 calculate_hash(const T& item) {
    HashBuilder visitor;
    add_to_hash(visitor, item);
    return visitor.get_result();
}

// Specialize calculate_hash() for pointers, u32 and u64.
// These specializations don't use Hash_Calculator; they just call shuffle_bits directly.
template <typename T>
inline u32 calculate_hash(T* item) {
    return (u32) shuffle_bits((uptr) item);
}
inline u32 calculate_hash(u32 item) {
    return shuffle_bits(item);
}
inline u32 calculate_hash(u64 item) {
    return (u32) shuffle_bits(item);
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
    u32 num_items_ = 0;

public:
    // Constructors
    ArrayView() = default;
    ArrayView(Item* items, u32 num_items) : items_{items}, num_items_{num_items} {
    }
    template <typename U = Item, std::enable_if_t<std::is_const<U>::value, int> = 0>
    ArrayView(std::initializer_list<Item> init_list)
        : items_{init_list.begin()}, num_items_{numeric_cast<u32>(init_list.size())} {
        PLY_ASSERT((uptr) init_list.end() - (uptr) init_list.begin() == sizeof(Item) * init_list.size());
    }
    template <u32 N>
    ArrayView(Item (&s)[N]) : items_{s}, num_items_{N} {
    }

    Item& operator[](u32 index) & {
        PLY_ASSERT(index < this->num_items_);
        return this->items_[index];
    }
    Item&& operator[](u32 index) && {
        PLY_ASSERT(index < this->num_items_);
        return std::move(this->items_[index]);
    }
    const Item& operator[](u32 index) const& {
        PLY_ASSERT(index < this->num_items_);
        return this->items_[index];
    }
    Item& back(s32 offset = -1) & {
        PLY_ASSERT(u32(this->num_items_ + offset) < this->num_items_);
        return this->items_[this->num_items_ + offset];
    }
    Item&& back(s32 offset = -1) && {
        PLY_ASSERT(u32(this->num_items_ + offset) < this->num_items_);
        return std::move(this->items_[this->num_items_ + offset]);
    }
    const Item& back(s32 offset = -1) const& {
        PLY_ASSERT(u32(this->num_items_ + offset) < this->num_items_);
        return this->items_[this->num_items_ + offset];
    }
    Item* items() {
        return this->items_;
    }
    const Item* items() const {
        return this->items_;
    }
    u32 num_items() const {
        return num_items_;
    }
    bool is_empty() const {
        return this->num_items_ == 0;
    }
    explicit operator bool() const {
        return this->num_items_ > 0;
    }
    operator ArrayView<const Item>() const {
        return {this->items_, this->num_items_};
    }
    static ArrayView<const Item> from(StringView view) {
        u32 num_items = view.num_bytes() / sizeof(Item); // Divide by constant is fast
        return {(const Item*) view.bytes(), num_items};
    }
    StringView string_view() const {
        return {(const char*) this->items_, numeric_cast<u32>(this->num_items_ * sizeof(Item))};
    }
    static ArrayView<Item> from(MutStringView view) {
        u32 num_items = view.num_bytes / sizeof(Item); // Divide by constant is fast
        return {(Item*) view.bytes, num_items};
    }
    MutStringView mut_string_view() {
        return {(char*) this->items_, numeric_cast<u32>(this->num_items_ * sizeof(Item))};
    }
    ArrayView subview(u32 start) const {
        PLY_ASSERT(start <= num_items_);
        return {this->items_ + start, num_items_ - start};
    }
    ArrayView subview(u32 start, u32 num_items) const {
        PLY_ASSERT(start <= this->num_items_); // FIXME: Support different end parameters
        PLY_ASSERT(start + num_items <= this->num_items_);
        return {this->items_ + start, num_items};
    }
    ArrayView shortened_by(u32 num_items) const {
        PLY_ASSERT(num_items <= this->num_items_);
        return {this->items_, this->num_items_ - num_items};
    }
    Item* begin() const {
        return this->items_;
    }
    Item* end() const {
        return this->items_ + this->num_items_;
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

#define PLY_ENABLE_IF_ARRAY_TYPE(Arr) typename enable_if_type<ArrayItemType<Arr>>::type = 0

template <typename Arr0, typename Arr1, PLY_ENABLE_IF_ARRAY_TYPE(Arr0), PLY_ENABLE_IF_ARRAY_TYPE(Arr1)>
bool operator==(const Arr0& a, const Arr1& b) {
    if (a.num_items() != b.num_items())
        return false;
    for (u32 i = 0; i < a.num_items(); i++) {
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
    u32 num_items_ = 0;
    u32 allocated = 0;

    // Make all other Array specializations friend classes.
    template <typename>
    friend class Array;

    void alloc(u32 num_items) {
        PLY_ASSERT(!this->items_);
        this->allocated = round_up_to_nearest_to_power_of_2(num_items);
        this->items_ = (Item*) Heap::alloc(uptr(this->allocated) * sizeof(Item));
        this->num_items_ = num_items;
    }

public:
    //----------------------------------------------------
    // Constructors
    //----------------------------------------------------

    Array() = default;
    // Copy constructor.
    Array(const Array<Item>& other_array) {
        this->alloc(other_array.num_items_);
        for (u32 i = 0; i < other_array.num_items_; i++) {
            new (&this->items_[i]) Item{other_array.items_[i]};
        }
    }
    // Move constructor.
    Array(Array<Item>&& other_array)
        : items_{other_array.items_}, num_items_{other_array.num_items_}, allocated{other_array.allocated} {
        new (&other_array) Array<Item>;
    }
    // Construct from any compatible array.
    template <typename T, PLY_ENABLE_IF_WELL_FORMED(ArrayView<const Item>(declval<T>()))>
    Array(T&& other_array) {
        u32 num_other_items = ArrayView<const Item>{other_array}.num_items();
        this->alloc(num_other_items);
        for (u32 i = 0; i < num_other_items; i++) {
            new ((Item*) this->items_ + i) Item{std::forward<T>(other_array)[i]};
        }
    }
    // Construct from initializer list.
    Array(std::initializer_list<Item> init_list) {
        u32 init_size = numeric_cast<u32>(init_list.size());
        this->alloc(init_size);
        const Item* src = init_list.begin();
        for (u32 i = 0; i < init_size; i++) {
            new ((Item*) this->items_ + i) Item{src[i]};
        }
    }
    // Destructor.
    ~Array() {
        for (u32 i = 0; i < this->num_items_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        Heap::free(this->items_);
    }
    // Adopt an array from a raw pointer.
    static Array<Item> adopt(Item* items, u32 num_items) {
        return {items, num_items, num_items};
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
        Array<Item> array_to_free{std::move(*this)};
        new (this) Array{std::forward<Other>(other)};
        return *this;
    }
    // Assign from initializer list.
    Array& operator=(std::initializer_list<Item> init_list) {
        for (u32 i = 0; i < this->num_items_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        u32 init_size = numeric_cast<u32>(init_list.size());
        this->alloc(init_size);
        const Item* src = init_list.begin();
        for (u32 i = 0; i < init_size; i++) {
            new (&this->items_[i]) Item{src[i]};
        }
        return *this;
    }
    // Extend from array with move semantics.
    Array& operator+=(Array<Item>&& other_array) {
        u32 num_other_items = ArrayView<const Item>{other_array}.num_items();
        this->reserve(this->num_items_ + num_other_items);
        for (u32 i = 0; i < num_other_items; i++) {
            new ((Item*) this->items_ + (this->num_items_ + i)) Item{std::move(other_array[i])};
        }
        this->num_items_ += num_other_items;
        return *this;
    }
    // Extend from any compatible array with copy semantics.
    template <typename Other, PLY_ENABLE_IF_WELL_FORMED(ArrayView<const Item>{declval<Other>()})>
    Array& operator+=(Other && other) {
        u32 num_other_items = ArrayView<const Item>{other}.num_items();
        this->reserve(this->num_items_ + num_other_items);
        for (u32 i = 0; i < num_other_items; i++) {
            new ((Item*) this->items_ + (this->num_items_ + i)) Item{std::forward<Other>(other)[i]};
        }
        this->num_items_ += num_other_items;
        return *this;
    }
    // Extend from initializer list.
    Array& operator+=(std::initializer_list<Item> init_list) {
        u32 init_size = numeric_cast<u32>(init_list.size());
        const Item* src = init_list.begin();
        this->reserve(this->num_items_ + init_size);
        for (u32 i = 0; i < init_size; i++) {
            new ((Item*) this->items_ + (this->num_items_ + i)) Item{src[i]};
        }
        this->num_items_ += init_size;
        return *this;
    }

    //----------------------------------------------------
    // Item access
    //----------------------------------------------------

    // Subscript operators.
    Item& operator[](u32 index) & {
        PLY_ASSERT(index < this->num_items_);
        return ((Item*) this->items_)[index];
    }
    Item&& operator[](u32 index) && {
        PLY_ASSERT(index < this->num_items_);
        return std::move(((Item*) this->items_)[index]);
    }
    const Item& operator[](u32 index) const& {
        PLY_ASSERT(index < this->num_items_);
        return ((Item*) this->items_)[index];
    }
    // Access items relative to the back of the array.
    Item& back(s32 offset = -1) & {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->num_items_);
        return ((Item*) this->items_)[this->num_items_ + offset];
    }
    Item&& back(s32 offset = -1) && {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->num_items_);
        return std::move(((Item*) this->items_)[this->num_items_ + offset]);
    }
    const Item& back(s32 offset = -1) const& {
        PLY_ASSERT(offset < 0 && u32(-offset) <= this->num_items_);
        return ((Item*) this->items_)[this->num_items_ + offset];
    }
    // Return a pointer to the items in the array.
    Item* items() {
        return this->items_;
    }
    const Item* items() const {
        return this->items_;
    }
    // Return the number of item in the array. The number of allocated items may be greater.
    u32 num_items() const {
        return this->num_items_;
    }
    // Return true if the array is empty.
    bool is_empty() const {
        return this->num_items_ == 0;
    }
    // Convert to true if the array is not empty. Can be used in if/while conditions.
    explicit operator bool() const {
        return this->num_items_ > 0;
    }
    // Return a subview of the array.
    ArrayView<Item> subview(u32 start) {
        return view().subview(start);
    }
    ArrayView<const Item> subview(u32 start) const {
        return view().subview(start);
    }
    ArrayView<Item> subview(u32 start, u32 num_items) {
        return view().subview(start, num_items);
    }
    ArrayView<const Item> subview(u32 start, u32 num_items) const {
        return view().subview(start, num_items);
    }
    // Return pointers suitable for iteration using range-for.
    Item* begin() {
        return (Item*) this->items_;
    }
    const Item* begin() const {
        return (Item*) this->items_;
    }
    Item* end() {
        return ((Item*) this->items_) + this->num_items_;
    }
    const Item* end() const {
        return ((Item*) this->items_) + this->num_items_;
    }

    //----------------------------------------------------
    // Modifying the array
    //----------------------------------------------------

    // Resize the array to a given number of items.
    void resize(u32 num_items) {
        for (u32 i = num_items; i < this->num_items_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        this->reserve(num_items);
        for (u32 i = this->num_items_; i < num_items; i++) {
            new ((Item*) this->items_ + i) Item;
        }
        this->num_items_ = num_items;
    }
    // Clear the array.
    void clear() {
        for (u32 i = 0; i < this->num_items_; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        Heap::free(this->items_);
        new (this) Array<Item>;
    }
    // Append an item to the array with copy semantics.
    Item& append(const Item& item) {
        // The argument must not be a reference to an existing item in the array:
        PLY_ASSERT((&item < (Item*) this->items_) || (&item >= (Item*) this->items_ + this->num_items_));
        if (this->num_items_ >= this->allocated) {
            this->reserve(this->num_items_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->num_items_) Item{item};
        this->num_items_++;
        return *result;
    }
    // Append an item to the array with move semantics.
    Item& append(Item&& item) {
        // The argument must not be a reference to an existing item in the array:
        PLY_ASSERT((&item < (Item*) this->items_) || (&item >= (Item*) this->items_ + this->num_items_));
        if (this->num_items_ >= this->allocated) {
            this->reserve(this->num_items_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->num_items_) Item{std::move(item)};
        this->num_items_++;
        return *result;
    }
    // Append an item to the array by invoking the constructor with the given arguments.
    template <typename... Args>
    Item& append(Args&&... args) {
        if (this->num_items_ >= this->allocated) {
            this->reserve(this->num_items_ + 1);
        }
        Item* result = new ((Item*) this->items_ + this->num_items_) Item{std::forward<Args>(args)...};
        this->num_items_++;
        return *result;
    }
    // Insert a default-constructed item at the given position.
    Item& insert(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos <= this->num_items_);
        this->reserve(this->num_items_ + count);
        memmove(static_cast<void*>((Item*) this->items_ + pos + count),
                static_cast<const void*>((Item*) this->items_ + pos), (this->num_items_ - pos) * sizeof(Item));
        for (u32 i = pos; i < pos + count; i++) {
            new ((Item*) this->items_ + i) Item;
        }
        this->num_items_ += count;
        return ((Item*) this->items_)[pos];
    }
    // Erase items and shift the items after the erased position(s) to the left.
    void erase(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos + count <= this->num_items_);
        for (u32 i = pos; i < pos + count; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        memmove(static_cast<void*>((Item*) this->items_ + pos),
                static_cast<const void*>((Item*) this->items_ + pos + count),
                (this->num_items_ - (pos + count)) * sizeof(Item));
        this->num_items_ -= count;
    }
    // Erase items and move the last items(s) to the erased position(s).
    void erase_quick(u32 pos, u32 count = 1) {
        PLY_ASSERT(pos + count <= this->num_items_);
        for (u32 i = pos; i < pos + count; i++) {
            ((Item*) this->items_)[i].~Item();
        }
        memmove(static_cast<void*>((Item*) this->items_ + pos),
                static_cast<const void*>((Item*) this->items_ + this->num_items_ - count), count * sizeof(Item));
        this->num_items_ -= count;
    }
    // Remove the last item(s) from the array.
    void pop(u32 count = 1) {
        PLY_ASSERT(count <= this->num_items_);
        resize(this->num_items_ - count);
    }
    // Reserve space for a given number of items. The number is rounded up to the nearest power of 2.
    void reserve(u32 num_items) {
        if (num_items > this->allocated) {
            this->allocated =
                round_up_to_nearest_to_power_of_2(num_items); // FIXME: Generalize to other resize strategies?
            this->items_ = (Item*) Heap::realloc(this->items_, uptr(this->allocated) * sizeof(Item));
        }
    }
    // Compact the array by compacting the heap memory to exactly fit the number of items.
    void compact() {
        this->allocated = this->num_items_;
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
        return {(Item*) this->items_, this->num_items_};
    }
    // Convert to a const `ArrayView`.
    ArrayView<const Item> view() const {
        return {(Item*) this->items_, this->num_items_};
    }
    // Convert to an `ArrayView`.
    operator ArrayView<Item>() {
        return {(Item*) this->items_, this->num_items_};
    }
    // Convert to a const `ArrayView`.
    operator ArrayView<const Item>() const {
        return {(Item*) this->items_, this->num_items_};
    }
    // Convert to a `StringView`.
    StringView string_view() const {
        return {(const char*) this->items_, numeric_cast<u32>(this->num_items_ * sizeof(Item))};
    }
    // Convert to a `MutStringView`.
    MutStringView mut_string_view() const {
        return {(char*) this->items_, numeric_cast<u32>(this->num_items_ * sizeof(Item))};
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

inline Array<StringView> String::split_byte(char sep) const {
    return ((StringView) * this).split_byte(sep);
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
    static void init(Item* items, Arg&& arg, RemainingArgs&&... remaining_args) {
        new (items) Item{std::forward<Arg>(arg)};
        init(items + 1, std::forward<RemainingArgs>(remaining_args)...);
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
    constexpr u32 num_items() const {
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
    MutStringView mut_string_view() {
        return {reinterpret_cast<char*>(this->items_), numeric_cast<u32>(NumItems * sizeof(Item))};
    }
    StringView string_view() const {
        return {reinterpret_cast<const char*>(this->items_), numeric_cast<u32>(NumItems * sizeof(Item))};
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

// First, we introduce an overloaded function get_any_lookup_key to map arbitrary items to lookup keys.
// It's basically a small set of function templates, each taking a single argument, that are selectively enabled
// at compile time using SFINAE.

// Define an alias template has_get_lookup_key_member. This provides a convenient way to selectively enable function
// candidates, using SFINAE, based on whether a given type defines a get_lookup_key member function.
PLY_CHECK_WELL_FORMED(has_get_lookup_key_member, declval<const T>().get_lookup_key())

template <typename Item, PLY_ENABLE_IF(has_get_lookup_key_member<Item>)>
static auto get_any_lookup_key(const Item& item) {
    return item.get_lookup_key();
}
// Otherwise, for primitive data types like u32 and float, this overload will simply return the item itself.
template <typename Item, PLY_ENABLE_IF(!has_get_lookup_key_member<Item>)>
static const Item& get_any_lookup_key(const Item& item) {
    return item;
}
template <typename Item>
using LookupKey = std::decay_t<decltype(get_any_lookup_key(declval<Item>()))>;

u32 get_best_num_hash_indices(u32 num_items);

//----------------------------------------------------
template <typename Key, typename Subclass>
struct HashLookup {
    s32* indices = nullptr;
    u32 num_indices = 0;
    u32 num_allocated_indices = 0;

    HashLookup() = default;
    HashLookup(const HashLookup& other)
        : indices{(s32*) Heap::alloc(sizeof(s32) * other.num_allocated_indices)}, num_indices{other.num_indices},
          num_allocated_indices{other.num_allocated_indices} {
        for (u32 i = 0; i < this->num_allocated_indices; i++) {
            this->indices[i] = other.indices[i];
        }
    }
    HashLookup(HashLookup&& other)
        : indices{other.indices}, num_indices{other.num_indices}, num_allocated_indices{other.num_allocated_indices} {
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
    PLY_NO_INLINE void reindex(u32 num_allocated_indices) {
        PLY_ASSERT(is_power_of_2(num_allocated_indices));
        u32 mask = num_allocated_indices - 1;

        // Allocate new indices.
        s32* new_indices = (s32*) Heap::alloc(sizeof(s32) * num_allocated_indices);
        for (u32 i = 0; i < num_allocated_indices; i++) {
            new_indices[i] = -1;
        }

        // Rebuild indices.
        for (u32 old_idx = 0; old_idx < this->num_allocated_indices; old_idx++) {
            s32 item_index = this->indices[old_idx];
            if (item_index >= 0) {
                for (u32 new_idx = calculate_hash(static_cast<Subclass*>(this)->get_key(item_index));; new_idx++) {
                    if (new_indices[new_idx & mask] < 0) {
                        new_indices[new_idx & mask] = item_index;
                        break;
                    }
                }
            }
        }

        Heap::free(this->indices);
        this->indices = new_indices;
        this->num_allocated_indices = num_allocated_indices;
    }

public:
    PLY_NO_INLINE s32 find_index(const Key& key) const {
        if (!this->indices)
            return -1;
        PLY_ASSERT(is_power_of_2(this->num_allocated_indices));
        u32 mask = this->num_allocated_indices - 1;
        for (u32 idx = calculate_hash(key);; idx++) {
            s32 item_index = this->indices[idx & mask];
            if (item_index < 0)
                return -1;
            if (key == static_cast<const Subclass*>(this)->get_key(item_index))
                return item_index;
        }
    }

    struct InsertIndexResult {
        u32 index;
        bool was_found;
    };

    PLY_NO_INLINE InsertIndexResult insert_index(const Key& key) {
        u32 min_allocated = get_best_num_hash_indices(this->num_indices + 1);
        if (this->num_allocated_indices < min_allocated) {
            this->reindex(min_allocated);
        }
        PLY_ASSERT(is_power_of_2(this->num_allocated_indices));
        u32 mask = this->num_allocated_indices - 1;
        for (u32 idx = calculate_hash(key);; idx++) {
            s32 item_index = this->indices[idx & mask];
            if (item_index < 0) {
                u32 new_index = static_cast<Subclass*>(this)->add_item(key);
                this->indices[idx & mask] = new_index;
                this->num_indices++;
                return {new_index, false};
            }
            if (key == static_cast<const Subclass*>(this)->get_key(item_index)) {
                return {numeric_cast<u32>(item_index), true};
            }
        }
    }
};

//   ▄▄▄▄          ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄
//   ▀▀▀█▄ ██▄▄██  ██
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄
//


PLY_CHECK_WELL_FORMED(is_constructible_from_key, T{declval<const LookupKey<T>&>()})

template <typename Item>
struct Set : HashLookup<LookupKey<Item>, Set<Item>> {
    using Key = LookupKey<Item>;

    Array<Item> items_;

private:
    friend struct HashLookup<Key, Set<Item>>;

    auto get_key(u32 index) const {
        return get_any_lookup_key(this->items_[index]);
    }

    template <typename U = Item, PLY_ENABLE_IF(is_constructible_from_key<U>)>
    u32 add_item(const Key& key) {
        u32 index = this->items_.num_items();
        this->items_.append(key);
        return index;
    }
    template <typename U = Item, PLY_ENABLE_IF(!is_constructible_from_key<U>)>
    u32 add_item(const Key&) {
        u32 index = this->items_.num_items();
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
        s32 item_index = this->find_index(key);
        if (item_index < 0)
            return nullptr;
        return &this->items_[item_index];
    }

    const Item* find(const Key& key) const {
        return const_cast<Set*>(this)->find(key);
    }

    struct InsertResult {
        Item* item;
        bool was_found;
    };

    template <typename K = Key, PLY_ENABLE_IF(is_constructible_from_key<K>)>
    InsertResult insert(const K& key) {
        auto result = this->insert_index(key);
        return {&this->items_[numeric_cast<u32>(result.index)], result.was_found};
    }

    InsertResult insert_item(Item&& item) {
        auto result = this->insert_index(get_any_lookup_key(item));
        Item* dst_item = &this->items_[numeric_cast<u32>(result.index)];
        *dst_item = std::move(item);
        return {dst_item, result.was_found};
    }

    PLY_NO_INLINE bool erase(const Key& key) {
        if (!this->items_)
            return false;
        PLY_ASSERT(is_power_of_2(this->num_allocated_indices));
        u32 mask = this->num_allocated_indices - 1;
        for (u32 idx = calculate_hash(key);; idx++) {
            s32 item_index = this->indices[idx & mask];
            if (item_index < 0)
                return false;

            if (key == get_any_lookup_key(this->items_[item_index])) {
                // Found the item to erase.
                u32 last_index = this->items_.num_items() - 1;
                if ((u32) item_index < last_index) {
                    // Move the last item to the erased item's index.
                    for (u32 j = calculate_hash(get_any_lookup_key(this->items_[last_index]));; j++) {
                        PLY_ASSERT(this->indices[j & mask] >= 0);
                        if ((u32) this->indices[j & mask] == last_index) {
                            this->indices[j & mask] = item_index;
                            break;
                        }
                    }
                }

                // Erase the item from the array.
                this->items_.erase_quick(item_index);

                // Free the slot in the indices array.
                this->indices[idx & mask] = -1;

                // Check subsequent indices to see if any should move into the newly freed slot.
                for (u32 trailing_idx = idx + 1;; trailing_idx++) {
                    s32 trailing_item_index = this->indices[trailing_idx & mask];
                    if (trailing_item_index < 0) {
                        // No more trailing indices.
                        break;
                    }
                    u32 trailing_item_hash = calculate_hash(get_any_lookup_key(this->items_[trailing_item_index]));
                    if (((trailing_idx - trailing_item_hash) & mask) >= ((trailing_idx - idx) & mask)) {
                        // Move this index.
                        this->indices[idx & mask] = trailing_item_index;
                        this->indices[trailing_idx & mask] = -1;
                        idx = trailing_idx; // This is the new freed slot.
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
struct Map {
    using K = LookupKey<Key>;

    struct Item {
        Key key;
        Value value;

        Item(const K& key) : key{key} {
        }
        const Key& get_lookup_key() const {
            return this->key;
        }
    };

    Set<Item> item_set;

    Value* find(const K& key) {
        Item* item = this->item_set.find(key);
        if (!item)
            return nullptr;
        return &item->value;
    }

    const Value* find(const K& key) const {
        return const_cast<Map*>(this)->find(key);
    }

    ArrayView<Item> items() {
        return this->item_set.items();
    }
    ArrayView<const Item> items() const {
        return this->item_set.items();
    }

    struct InsertResult {
        Value* value;
        bool was_found;
    };

    InsertResult insert(const K& key) {
        auto result = this->item_set.insert(key);
        return {&result.item->value, result.was_found};
    }

    void erase(const K& key) {
        this->item_set.erase(key);
    }

    void clear() {
        this->item_set.clear();
    }

    const Item* begin() const {
        return this->item_set.begin();
    }
    const Item* end() const {
        return this->item_set.end();
    }
};

//   ▄▄▄▄                             ▄▄
//  ██  ██ ▄▄    ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄██
//  ██  ██ ██ ██ ██ ██  ██ ██▄▄██ ██  ██
//  ▀█▄▄█▀  ██▀▀██  ██  ██ ▀█▄▄▄  ▀█▄▄██
//

PLY_CHECK_WELL_FORMED(has_destroy_member, declval<T>().destroy())

template <typename Item, PLY_ENABLE_IF(has_destroy_member<Item>)>
void destroy(Item* obj) {
    if (obj) {
        obj->destroy();
    }
}
template <typename Item, PLY_ENABLE_IF(!has_destroy_member<Item>)>
void destroy(Item* obj) {
    if (obj) {
        Heap::destroy(obj);
    }
}

template <typename Item>
class Owned {
private:
    template <typename>
    friend class Owned;
    Item* ptr;

public:
    Owned() : ptr{nullptr} {
    }
    Owned(Item* ptr) : ptr{ptr} { // FIXME: Replace with Owned<Item>::adopt()
    }
    Owned(Owned&& other) : ptr{other.release()} {
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
    Owned& operator=(Owned&& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        destroy(this->ptr);
        this->ptr = other.release();
        return *this;
    }
    template <typename Derived, typename std::enable_if_t<std::is_base_of<Item, Derived>::value, int> = 0>
    Owned& operator=(Owned<Derived>&& other) {
        PLY_ASSERT(!this->ptr || this->ptr != other.ptr);
        destroy(this->ptr);
        this->ptr = other.release();
        return *this;
    }
    Owned& operator=(Item* ptr) {
        PLY_ASSERT(!this->ptr || this->ptr != ptr);
        destroy(this->ptr);
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
        destroy(this->ptr);
        this->ptr = nullptr;
    }
    auto get_lookup_key() const {
        return get_any_lookup_key(*this->ptr);
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
            this->ptr->inc_ref_count();
        }
    }
    Reference(const Reference& ref) : ptr(ref.ptr) {
        if (this->ptr) {
            this->ptr->inc_ref_count();
        }
    }
    Reference(Reference&& ref) : ptr(ref.ptr) {
        ref.ptr = nullptr;
    }
    ~Reference() {
        if (this->ptr) {
            this->ptr->dec_ref_count();
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
            this->ptr->inc_ref_count();
        }
        if (prev) {
            prev->dec_ref_count();
        }
        return *this;
    }
    Reference& operator=(const Reference& ref) {
        Item* prev = this->ptr;
        this->ptr = ref.ptr;
        if (this->ptr) {
            this->ptr->inc_ref_count();
        }
        if (prev) {
            prev->dec_ref_count();
        }
        return *this;
    }
    Reference& operator=(Reference&& ref) {
        if (this->ptr) {
            this->ptr->dec_ref_count();
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
        if (this->ptr) {
            this->ptr->dec_ref_count();
        }
        this->ptr = nullptr;
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
    Atomic<u32> ref_count = 0;

public:
    void inc_ref_count() {
        u32 old_count = this->ref_count.fetch_add_acq_rel(1);
        PLY_ASSERT(old_count < 5000);
        PLY_UNUSED(old_count);
    }
    void dec_ref_count() {
        s32 old_count = this->ref_count.fetch_sub_acq_rel(1);
        PLY_ASSERT(old_count < 5000);
        if (old_count == 1) {
            static_cast<Subclass*>(this)->on_ref_count_zero();
        }
    }
    s32 get_ref_count() const {
        return this->ref_count.load_acquire();
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

PLY_CHECK_WELL_FORMED(is_copy_constructible, T{declval<const T>()});

template <typename T, PLY_ENABLE_IF(is_copy_constructible<T>)>
void copy_construct_wrapper(T* dst, const T* src) {
    new (dst) T{*src};
}
template <typename T, PLY_ENABLE_IF(!is_copy_constructible<T>)>
void copy_construct_wrapper(T*, const T*) {
    PLY_FORCE_CRASH();
}

template <typename Return, typename... Args>
struct Functor<Return(Args...)> {
    struct DynamicOps {
        void (*copy_construct)(Functor*, const Functor*) = nullptr;
        void (*destruct)(Functor*) = nullptr;
    };

    Return (*thunk)(const Functor*, Args...) = nullptr;
    void* thunk_arg = nullptr;
    DynamicOps* dyn_ops = nullptr;

    Functor() = default;
    Functor(Return (*target)(Args...)) {
        this->thunk = [](const Functor* f, Args... args) -> Return {
            return (*(Return (*)(Args...)) f->thunk_arg)(std::forward<Args>(args)...);
        };
        this->thunk_arg = (void*) target;
    }
    Functor(const Functor& o) {
        if (o.dyn_ops) {
            o.dyn_ops->copy_construct(this, &o);
        } else {
            this->thunk = o.thunk;
            this->thunk_arg = o.thunk_arg;
            this->dyn_ops = o.dyn_ops;
        }
    }
    Functor(Functor&& o) {
        this->thunk = o.thunk;
        this->thunk_arg = o.thunk_arg;
        this->dyn_ops = o.dyn_ops;
        new (&o) Functor;
    }
    // Construct from any callable object such as a lambda expression.
    template <typename Callable, typename = decltype(declval<Callable>()(declval<Args>()...)),
              std::enable_if_t<!std::is_same<Functor, std::decay_t<Callable>>::value, int> = 0>
    Functor(Callable&& callable) {
        using CallableType = std::decay_t<Callable>; // Remove const (if any)
        this->thunk = [](const Functor* f, Args... args) -> Return {
            return (*(const CallableType*) f->thunk_arg)(std::forward<Args>(args)...);
        };
        this->thunk_arg = (void*) Heap::create<CallableType>(std::forward<Callable>(callable));
        static DynamicOps dyn_ops_for_callable = {
            [](Functor* dst, const Functor* src) { // copy_construct
                dst->thunk = src->thunk;
                dst->thunk_arg = Heap::alloc(sizeof(CallableType));
                copy_construct_wrapper<CallableType>((CallableType*) dst->thunk_arg,
                                                     (const CallableType*) src->thunk_arg);
                dst->dyn_ops = src->dyn_ops;
            },
            [](Functor* f) { // destruct
                Heap::destroy<CallableType>((CallableType*) f->thunk_arg);
            },
        };
        this->dyn_ops = &dyn_ops_for_callable;
    }
    ~Functor() {
        if (this->dyn_ops) {
            this->dyn_ops->destruct(this);
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
constexpr u32 get_type_index() {
    return 0;
}
template <typename T, u32 Index, typename First, typename... Rest>
constexpr u32 get_type_index() {
    return std::is_same<T, First>::value ? Index : get_type_index<T, Index + 1, Rest...>();
}
constexpr uptr max_size(uptr val) {
    return val;
}
template <typename... Rest>
constexpr uptr max_size(uptr val1, uptr val2, Rest... rest) {
    return max_size((val1 > val2) ? val1 : val2, rest...);
}

template <typename... Subtypes>
struct Variant {
private:
    u32 subtype = 0;
    alignas(max_size(alignof(Subtypes)...)) char storage[max_size(sizeof(Subtypes)...)];

    // Helper to copy the current subobject
    template <u32 Index>
    void copy_helper(const Variant& other) {
    }
    template <u32 Index, typename First, typename... Rest>
    void copy_helper(const Variant& other) {
        if (this->subtype == Index) {
            new (this->storage) First{*reinterpret_cast<const First*>(other.storage)};
        } else {
            this->copy_helper<Index + 1, Rest...>(other);
        }
    }

    // Helper to move the current subobject
    template <u32 Index>
    void move_helper(Variant&& other) {
    }
    template <u32 Index, typename First, typename... Rest>
    void move_helper(Variant&& other) {
        if (this->subtype == Index) {
            new (this->storage) First{std::move(*reinterpret_cast<First*>(other.storage))};
        } else {
            this->move_helper<Index + 1, Rest...>(std::move(other));
        }
    }

    // Helper to destruct the current subobject
    template <u32 Index>
    void destruct_helper() {
    }
    template <u32 Index, typename First, typename... Rest>
    void destruct_helper() {
        if (this->subtype == Index) {
            reinterpret_cast<First*>(this->storage)->~First();
        } else {
            this->destruct_helper<Index + 1, Rest...>();
        }
    }

public:
    // Map a type to its subtype index
    template <typename T>
    static constexpr u32 index_of = ply::get_type_index<std::decay_t<T>, 1, Subtypes...>();

    // Construct an empty variant
    Variant() = default;

    // Constructor for each type in the parameter pack
    template <typename T, typename std::enable_if<(index_of<T> > 0), int>::type = 0>
    Variant(T&& value) {
        this->subtype = index_of<T>;
        new (storage) std::decay_t<T>(std::forward<T>(value));
    }

    // Copy constructor
    Variant(const Variant& other) : subtype{other.subtype} {
        this->copy_helper<1, Subtypes...>(other);
    }

    // Move constructor
    Variant(Variant&& other) : subtype{other.subtype} {
        this->move_helper<1, Subtypes...>(std::move(other));
        other.subtype = 0;
    }

    // Destructor
    ~Variant() {
        this->destruct_helper<1, Subtypes...>();
    }

    // Copy assignment
    Variant& operator=(const Variant& other) {
        this->destruct_helper<1, Subtypes...>();
        this->subtype = other.subtype;
        this->copy_helper<1, Subtypes...>(other);
        return *this;
    }

    // Move assignment
    Variant& operator=(Variant&& other) {
        this->destruct_helper<1, Subtypes...>();
        this->subtype = other.subtype;
        this->move_helper<1, Subtypes...>(std::move(other));
        other.subtype = 0;
        return *this;
    }

    // Assignment operator for each type in the parameter pack
    template <typename T, typename std::enable_if<(index_of<T> > 0), int>::type = 0>
    Variant& operator=(T&& value) {
        // Destruct the current object if any
        if (this->subtype > 0) {
            this->destruct_helper<1, Subtypes...>();
        }
        // Construct the new object
        this->subtype = index_of<T>;
        new (storage) std::decay_t<T>(std::forward<T>(value));
        return *this;
    }

    // Get current subtype index
    u32 get_subtype_index() const {
        return this->subtype;
    }

    // Check if the variant is currently empty
    bool is_empty() const {
        return this->subtype == 0;
    }

    // Check if the variant currently holds a specific type
    template <typename T, typename std::enable_if<(index_of<T> > 0), int>::type = 0>
    bool is() const {
        return this->subtype == index_of<T>;
    }

    // Dynamic casts
    template <typename T, typename std::enable_if<(index_of<T> > 0), int>::type = 0>
    T* as() {
        if (this->subtype != index_of<T>)
            return nullptr;
        return reinterpret_cast<T*>(this->storage);
    }
    template <typename T, typename std::enable_if<(index_of<T> > 0), int>::type = 0>
    const T* as() const {
        if (this->subtype != index_of<T>)
            return nullptr;
        return reinterpret_cast<const T*>(this->storage);
    }

    // Switch to
    template <typename T, typename... Args>
    T& switch_to(Args&&... args) {
        this->destruct_helper<1, Subtypes...>();
        this->subtype = index_of<T>;
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
s32 find(ArrayView<T> arr, const U& item, u32 start_pos = 0) {
    for (u32 i = start_pos; i < arr.num_items(); i++) {
        if (arr[i] == item)
            return i;
    }
    return -1;
}
template <typename T, typename Callback, PLY_ENABLE_IF_WELL_FORMED(declval<Callback>()(declval<T>()))>
s32 find(ArrayView<T> arr, const Callback& callback, u32 start_pos = 0) {
    for (u32 i = start_pos; i < arr.num_items(); i++) {
        if (callback(arr[i]))
            return i;
    }
    return -1;
}
template <typename Arr, typename Arg, PLY_ENABLE_IF_ARRAY_TYPE(Arr)>
s32 find(const Arr& arr, const Arg& arg, u32 start_pos = 0) {
    return find(ArrayView<const ArrayItemType<Arr>>{arr}, arg, start_pos);
}

template <typename T, typename U, PLY_ENABLE_IF_WELL_FORMED(declval<T>() == declval<U>())>
s32 reverse_find(ArrayView<const T> arr, const U& item, s32 start_pos = -1) {
    if (start_pos < 0) {
        start_pos += arr.num_items();
    }
    for (s32 i = start_pos; i >= 0; i--) {
        if (arr[i] == item)
            return i;
    }
    return -1;
}
template <typename T, typename Callback, PLY_ENABLE_IF_WELL_FORMED(declval<Callback>()(declval<T>()))>
s32 reverse_find(ArrayView<const T> arr, const Callback& callback, s32 start_pos = -1) {
    if (start_pos < 0) {
        start_pos += arr.num_items();
    }
    for (s32 i = start_pos; i >= 0; i--) {
        if (callback(arr[i]))
            return i;
    }
    return -1;
}
template <typename Arr, typename Arg, PLY_ENABLE_IF_ARRAY_TYPE(Arr)>
s32 reverse_find(const Arr& arr, const Arg& arg, s32 start_pos = -1) {
    return reverse_find(ArrayView<const ArrayItemType<Arr>>{arr}, arg, start_pos);
}

template <typename T>
bool default_less(const T& a, const T& b) {
    return a < b;
}
template <typename T, typename IsLess = decltype(default_less<T>)>
void sort(ArrayView<T> view, const IsLess& is_less = default_less<T>) {
    if (view.num_items() <= 1)
        return;
    u32 lo = 0;
    u32 hi = view.num_items() - 1;
    u32 pivot = view.num_items() / 2;
    for (;;) {
        while (lo < hi && is_less(view[lo], view[pivot])) {
            lo++;
        }
        while (lo < hi && is_less(view[pivot], view[hi])) {
            hi--;
        }
        if (lo >= hi)
            break;
        // view[lo] is >= pivot
        // All slots to left of lo are < pivot
        // view[hi] <= pivot
        // All slots to the right of hi are > pivot
        PLY_ASSERT(!is_less(view[lo], view[pivot]));
        PLY_ASSERT(!is_less(view[pivot], view[hi]));
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
        if (!is_less(view[lo - 1], view[pivot])) {
            lo--;
        } else {
            sort(view.subview(0, lo), is_less);
            break;
        }
    }
    while (hi + 1 < view.num_items()) {
        if (!is_less(view[pivot], view[hi])) {
            hi++;
        } else {
            sort(view.subview(hi), is_less);
            break;
        }
    }
}
template <typename Arr, typename IsLess = decltype(default_less<ArrayItemType<Arr>>)>
void sort(Arr& arr, const IsLess& is_less = default_less<ArrayItemType<Arr>>) {
    using T = ArrayItemType<Arr>;
    sort(ArrayView<T>{arr}, is_less);
}

enum FindType {
    FindGreaterThan,
    FindGreaterThanOrEqual,
};

template <typename Key>
bool meets_condition(const Key& a, const Key& b, FindType find_type) {
    switch (find_type) {
        case FindGreaterThan:
            return a > b;
        case FindGreaterThanOrEqual:
            return a >= b;
        default:
            return false;
    }
}

template <typename Item>
u32 binary_search(ArrayView<Item> arr, const LookupKey<Item>& desired_key, FindType find_type) {
    u32 lo = 0;               // Start of the search range.
    u32 hi = arr.num_items(); // End of the search range.
    while (lo < hi) {
        u32 mid = (lo + hi) / 2; // Middle index that sits roughly halfway between lo and hi.
        PLY_ASSERT((mid >= lo) && (mid < hi));
        auto mid_key = get_any_lookup_key(arr[mid]);
        if (meets_condition(mid_key, desired_key, find_type)) {
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
u32 binary_search(Arr& arr, const Key& desired_key, FindType find_type) {
    using Item = ArrayItemType<Arr>;
    return binary_search(ArrayView<Item>{arr}, desired_key, find_type);
}

//  ▄▄▄▄▄  ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀▀  ██ ██  ██ ██▄▄██
//  ██     ██ ██▄▄█▀ ▀█▄▄▄
//            ██

class Pipe {
protected:
    u64 seek_pos = 0;
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
    virtual void flush(bool to_device = false);
    virtual u64 get_file_size();
    virtual void seek_to(s64 offset);
    u32 get_flags() const {
        return this->flags;
    }
};

#if defined(_WIN32)

class PipeHandle : public Pipe {
public:
    HANDLE handle = INVALID_HANDLE_VALUE;

    PipeHandle(HANDLE h, u32 flags) : handle(h) {
        this->flags = flags;
    }
    virtual ~PipeHandle();
    virtual u32 read(MutStringView buf) override;
    virtual bool write(StringView buf) override;
    virtual void flush(bool to_device = false) override;
    virtual u64 get_file_size() override;
    virtual void seek_to(s64 offset) override;
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
    virtual void flush(bool to_device = false) override;
    virtual u64 get_file_size() override;
    virtual void seek_to(s64 offset) override;
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
        u64 seek_pos_at_buffer = 0;
    };
    struct MemData {
        Array<char*> buffers;
        u32 buffer_index = 0;
        u32 num_bytes_in_last_buffer = 0;
        // Temporary buffer used when caller requests consecutive bytes that straddle buffer boundaries:
        char* temp_buffer = nullptr;
        u32 temp_buffer_offset = 0; // Offset of overlap buffer relative to storage buffer
    };
    struct ViewData {
        char* start_byte = nullptr;
    };

    char* cur_byte = nullptr;
    char* end_byte = nullptr;
    Type type = Type::None;
    Mode mode = Mode::None;
    bool is_pipe_owner = false; // only used if type == Type::Pipe
    bool has_read_permission = false;
    bool has_write_permission = false;
    bool using_temp_buffer = false; // only used if type == Type::Mem
    bool at_eof = false;
    bool input_error = false;
    union {
        PipeData pipe; // if type == Type::Pipe
        MemData mem;   // if type == Type::Mem
        ViewData view; // if type == Type::View
    };

    Stream();
    Stream(Pipe* pipe, bool is_pipe_owner);
    Stream(Stream&& other);
    ~Stream();
    Stream& operator=(Stream&& other) {
        PLY_ASSERT(this != &other);
        this->~Stream();
        new (this) Stream{std::move(other)};
        return *this;
    }

    bool is_open() {
        return this->cur_byte != nullptr;
    }
    explicit operator bool() {
        return this->cur_byte != nullptr;
    }
    void close() {
        this->~Stream();
        new (this) Stream; // Illegal to use after this.
    }

    //--------------------------------------------
    // Main read & write functions
    bool make_readable(u32 min_bytes = 1) {
        if ((this->mode == Mode::Reading) && (this->cur_byte + min_bytes <= this->end_byte))
            return true;
        else
            return make_readable_internal(min_bytes);
    }
    bool make_writable(u32 min_bytes = 1) {
        if ((this->mode == Mode::Writing) && (this->cur_byte + min_bytes <= this->end_byte))
            return true;
        else
            return make_writable_internal(min_bytes);
    }
    u32 num_remaining_bytes() const {
        return numeric_cast<u32>(this->end_byte - this->cur_byte);
    }
    StringView view_remaining_bytes() const {
        return {this->cur_byte, numeric_cast<u32>(this->end_byte - this->cur_byte)};
    }
    MutStringView view_remaining_bytes_mut() {
        return {this->cur_byte, numeric_cast<u32>(this->end_byte - this->cur_byte)};
    }
    void flush(bool to_device = false);

    //--------------------------------------------
    // Read wrappers
    char read_byte() {
        if (this->cur_byte < this->end_byte)
            return *this->cur_byte++;
        else
            return this->read_byte_internal();
    }
    u32 read(MutStringView dst) {
        if (dst.num_bytes <= this->num_remaining_bytes()) {
            memcpy(dst.bytes, this->cur_byte, dst.num_bytes);
            this->cur_byte += dst.num_bytes;
            return dst.num_bytes;
        } else
            return this->read_internal(dst);
    }
    u32 skip(u32 num_bytes) {
        if (num_bytes <= this->num_remaining_bytes()) {
            this->cur_byte += num_bytes;
            return num_bytes;
        } else
            return this->skip_internal(num_bytes);
    }

    //--------------------------------------------
    // Write wrappers
    bool write(char c) {
        if (!this->make_writable())
            return false;
        *this->cur_byte++ = c;
        return true;
    }
    u32 write(StringView bytes);
    template <typename... Args>
    void format(StringView fmt, const Args&... args);

    //--------------------------------------------
    // Seeking
    u64 get_seek_pos();
    void seek_to(u64 seek_pos);

protected:
    bool make_readable_internal(u32 num_bytes);
    bool make_writable_internal(u32 num_bytes);
    char read_byte_internal();
    void flush_mem_writes();
    u32 read_internal(MutStringView dst);
    u32 skip_internal(u32 num_bytes);
};

template <typename T>
T native_read(Stream& in) {
    T result;
    in.read({(char*) &result, sizeof(result)});
    return result;
}

template <typename T>
void native_write(Stream& out, const T& value) {
    out.write({(const char*) &value, sizeof(value)});
}

//--------------------------------------------
class MemStream : public Stream {
public:
    MemStream();
    String move_to_string();
};

//--------------------------------------------
struct ViewStream : Stream {
    ViewStream() = default;
    explicit ViewStream(StringView view);
    explicit ViewStream(MutStringView view);

    void seek_to(char* byte) {
        PLY_ASSERT((byte >= this->view.start_byte) && (byte <= this->end_byte));
        this->cur_byte = byte;
        this->at_eof = false;
        this->input_error = false;
    }
    template <typename... Args>
    bool match(StringView pattern, Args*... args);
};

//   ▄▄▄▄   ▄▄                     ▄▄                   ▄▄     ▄▄▄▄     ▄▄  ▄▄▄▄
//  ██  ▀▀ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██      ██     ▄█▀ ██  ██
//   ▀▀▀█▄  ██    ▄▄▄██ ██  ██ ██  ██  ▄▄▄██ ██  ▀▀ ██  ██      ██   ▄█▀   ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄██     ▄██▄ ██     ▀█▄▄█▀
//

Pipe* get_stdin_pipe();
Pipe* get_stdout_pipe();
Pipe* get_stderr_pipe();

enum ConsoleMode { TEXT, BINARY };

Stream get_stdin(ConsoleMode mode = TEXT);
Stream get_stdout(ConsoleMode mode = TEXT);
Stream get_stderr(ConsoleMode mode = TEXT);

//  ▄▄▄▄▄                    ▄▄ ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                         ▄▄▄█▀

static constexpr u32 QS_ALLOW_SINGLE_QUOTE = 0x1;
static constexpr u32 QS_ESCAPE_WITH_BACKSLASH = 0x2;
static constexpr u32 QS_COLLAPSE_DOUBLES = 0x4;
static constexpr u32 QS_ALLOW_MULTILINE_WITH_TRIPLE = 0x8;

static constexpr u32 ID_WITH_DOLLAR_SIGN = 0x1;
static constexpr u32 ID_WITH_DASH = 0x2;

enum QS_Error_Code {
    QS_NO_OPENING_QUOTE,
    QS_UNEXPECTED_END_OF_LINE,
    QS_UNEXPECTED_END_OF_FILE,
    QS_BAD_ESCAPE_SEQUENCE,
};

String read_line(Stream& in);
StringView read_line(ViewStream& view_in);
String read_whitespace(Stream& in);
StringView read_whitespace(ViewStream& in);
void skip_whitespace(Stream& in);
String read_identifier(Stream& in, u32 flags = 0);
StringView read_identifier(ViewStream& view_in, u32 flags = 0);
u64 read_u64_from_text(Stream& in, u32 radix = 10);
s64 read_s64_from_text(Stream& in, u32 radix = 10);
double read_double_from_text(Stream& in, u32 radix = 10);
String read_quoted_string(Stream& in, u32 flags = 0, Functor<void(QS_Error_Code)> error_callback = {});

using MatchArg = Variant<String*, StringView*, u32*, s32*, u64*, s64*, double*, float*, bool*>;
bool match_with_args(ViewStream& in, StringView pattern, ArrayView<const MatchArg> match_args);

template <typename... Args>
bool String::match(StringView pattern, Args*... args) const {
    FixedArray<MatchArg, sizeof...(Args)> match_args{args...};
    ViewStream in{*this};
    return match_with_args(in, pattern, match_args);
}
template <typename... Args>
bool StringView::match(StringView pattern, Args*... args) const {
    FixedArray<MatchArg, sizeof...(Args)> match_args{args...};
    ViewStream in{*this};
    return match_with_args(in, pattern, match_args);
}
template <typename... Args>
bool ViewStream::match(StringView pattern, Args*... args) {
    FixedArray<MatchArg, sizeof...(Args)> match_args{args...};
    return match_with_args(*this, pattern, match_args);
}

//  ▄▄    ▄▄        ▄▄  ▄▄   ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//   ██▀▀██  ██     ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                      ▄▄▄█▀

void print_number(Stream& out, u64 value, u32 radix = 10, bool capitalize = false);
void print_number(Stream& out, s64 value, u32 radix = 10, bool capitalize = false);
inline void print_number(Stream& out, u32 value, u32 radix = 10, bool capitalize = false) {
    return print_number(out, (u64) value, radix, capitalize);
}
inline void print_number(Stream& out, s32 value, u32 radix = 10, bool capitalize = false) {
    return print_number(out, (s64) value, radix, capitalize);
}
void print_number(Stream& out, double value, u32 radix = 10, bool capitalize = false);
void print_escaped_string(Stream& out, StringView str);
void print_xml_escaped_string(Stream& out, StringView str);

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

void format_with_args(Stream& out, StringView fmt, ArrayView<const FormatArg> args);
template <typename... Args>
String String::format(StringView fmt, const Args&... args) {
    MemStream mem;
    mem.format(fmt, args...);
    return mem.move_to_string();
}
template <typename... Args>
void Stream::format(StringView fmt, const Args&... args) {
    FixedArray<FormatArg, sizeof...(Args)> fa{args...};
    format_with_args(*this, fmt, fa);
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
    Map<u32, u8> reverse_lut;
    s32 missing_char = 255; // If negative, missing characters are skipped.
};

enum DecodeStatus {
    DS_OK,
    DS_ILL_FORMED,      // Not at EOF.
    DS_NOT_ENOUGH_DATA, // Can still decode an ill-formed codepoint.
};

struct DecodeResult {
    s32 point = -1;
    u32 num_bytes = 0;
    DecodeStatus status = DS_OK;
};

// Returns the number of bytes written to buf.
u32 encode_unicode(FixedArray<char, 4>& buf, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params);
DecodeResult decode_unicode(StringView str, UnicodeType unicode_type, ExtendedTextParams* ext_params = nullptr);

bool encode_unicode(Stream& out, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params = nullptr);
DecodeResult decode_unicode(Stream& in, UnicodeType unicode_type,
                            ExtendedTextParams* ext_params = nullptr); // -1 at EOF

class InPipeConvertUnicode : public Pipe {
public:
    Stream in;
    UnicodeType src_type;
    ExtendedTextParams* ext_params = nullptr;

    // shim_storage is used to split multibyte characters at buffer boundaries.
    FixedArray<char, 4> shim_storage;
    StringView shim_used;

    InPipeConvertUnicode(Stream&& in, UnicodeType type = NOT_UNICODE) : in{std::move(in)}, src_type{type} {
        PLY_ASSERT(this->in.has_read_permission);
        this->flags = Pipe::HAS_READ_PERMISSION;
    }
    // Fill dst_buf with UTF-8-encoded data.
    virtual u32 read(MutStringView dst_buf) override;
};

class OutPipeConvertUnicode : public Pipe {
public:
    Stream child_out;
    UnicodeType dst_type;
    ExtendedTextParams* ext_params = nullptr;

    // shim_storage is used to join multibyte characters at buffer boundaries.
    char shim_storage[4];
    u32 shim_used = false;

    OutPipeConvertUnicode(Stream&& child_out, UnicodeType type = NOT_UNICODE)
        : child_out{std::move(child_out)}, dst_type{type} {
        PLY_ASSERT(this->child_out.has_write_permission);
        this->flags = Pipe::HAS_WRITE_PERMISSION;
    }
    // src_buf expects UTF-8-encoded data.
    virtual bool write(StringView src_buf) override;
    virtual void flush(bool to_device = false) override;
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

    UnicodeType unicode_type = UTF8;
    NewLine new_line = LF;
    bool bom = true;
};

TextFormat get_default_utf8_format();
TextFormat autodetect_text_format(Stream& in);

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
    FSResult result = FS_UNKNOWN; // Result of get_file_info()
    String name;
    bool is_dir = false;
    u64 file_size = 0;            // Size of the file in bytes
    double creation_time = 0;     // The file's POSIX creation time
    double access_time = 0;       // The file's POSIX access time
    double modification_time = 0; // The file's POSIX modification time

    bool is_file() const {
        return !this->is_dir;
    }
};

struct WalkTriple {
    String dir_path;
    Array<String> dir_names;
    Array<DirectoryEntry> files;
};

class DirectoryWalker {
private:
    struct StackItem {
        String path;
        Array<String> dir_names;
        u32 dir_index;
    };

    friend struct Filesystem;
    WalkTriple triple;
    Array<StackItem> stack;

    void visit(StringView dir_path);

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
            return !this->walker->triple.dir_path.is_empty();
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
    static ThreadLocal<FSResult> last_result_;

    static FSResult set_last_result(FSResult result) {
        last_result_.store(result);
        return result;
    }
    static FSResult last_result() {
        return last_result_.load();
    }

#if defined(_WIN32)
    static constexpr PathFormat path_format() {
        return WindowsPath;
    }

    // Read_Write_Lock used to mitigate data race issues with SetCurrentDirectoryW:
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
    static ReadWriteLock working_dir_lock;

    // Direct access to Windows handles:
    static HANDLE open_handle_for_read(StringView path);
    static HANDLE open_handle_for_write(StringView path);
    static DirectoryEntry get_file_info(HANDLE handle);
#elif defined(PLY_POSIX)
    static constexpr PathFormat path_format() {
        return POSIXPath;
    }

    static int open_fd_for_read(StringView path);
    static int open_fd_for_write(StringView path);
#endif

    static Array<DirectoryEntry> list_dir(StringView path);
    static FSResult make_dir(StringView path);
    static String get_working_directory();
    static FSResult set_working_directory(StringView path);
    static ExistsResult exists(StringView path);
    static Owned<Pipe> open_pipe_for_read(StringView path);
    static Owned<Pipe> open_pipe_for_write(StringView path);
    static FSResult move_file(StringView src_path, StringView dst_path);
    static FSResult delete_file(StringView path);
    static FSResult remove_dir_tree(StringView dir_path);
    static DirectoryEntry get_file_info(StringView path);

    static FSResult copy_file(StringView src_path, StringView dst_path);
    static bool is_dir(StringView path) {
        return Filesystem::exists(path) == ER_DIRECTORY;
    }
    static DirectoryWalker walk(StringView top);
    static FSResult make_dirs(StringView path);
    static Stream open_binary_for_read(StringView path);
    static Stream open_binary_for_write(StringView path);
    static Stream open_text_for_read(StringView path, const TextFormat& format = get_default_utf8_format());
    static Stream open_text_for_read_autodetect(StringView path, TextFormat* out_format = nullptr);
    static Stream open_text_for_write(StringView path, const TextFormat& format = get_default_utf8_format());
    static String load_binary(StringView path);
    static String load_text(StringView path, const TextFormat& format = get_default_utf8_format());
    static String load_text_autodetect(StringView path, TextFormat* out_format = nullptr);
    static FSResult save_binary(StringView path, StringView contents);
    static FSResult save_text(StringView path, StringView str_contents,
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
    StringView base_name;
    StringView extension;
};

// Generic path manipulation functions:
constexpr char get_path_separator(PathFormat fmt) {
    return (fmt == PathFormat::WindowsPath) ? '\\' : '/';
}
StringView get_drive_letter(PathFormat fmt, StringView path);
bool is_absolute_path(PathFormat fmt, StringView path);
inline bool is_relative_path(PathFormat fmt, StringView path) {
    return !is_absolute_path(fmt, path);
}
String make_absolute_path(PathFormat fmt, StringView path);
String make_relative_path(PathFormat fmt, StringView ancestor, StringView descendant);
SplitPath split_path(PathFormat fmt, StringView path);
SplitExtension split_file_extension(PathFormat fmt, StringView path);
Array<StringView> split_path_full(PathFormat fmt, StringView path);
String join_path_from_array(PathFormat fmt, ArrayView<const StringView> components);
template <typename... StringViews>
String join_path(PathFormat fmt, StringViews&&... path_component_args) {
    FixedArray<StringView, sizeof...(StringViews)> components{std::forward<StringViews>(path_component_args)...};
    return join_path_from_array(fmt, components);
}

// Native path manipulation functions:
constexpr char get_path_separator() {
    return get_path_separator(Filesystem::path_format());
}
inline StringView get_drive_letter(StringView path) {
    return get_drive_letter(Filesystem::path_format(), path);
}
inline bool is_absolute_path(StringView path) {
    return is_absolute_path(Filesystem::path_format(), path);
}
inline bool is_relative_path(StringView path) {
    return !is_absolute_path(Filesystem::path_format(), path);
}
inline String make_absolute_path(StringView path) {
    return make_absolute_path(Filesystem::path_format(), path);
}
inline String make_relative_path(StringView ancestor, StringView descendant) {
    return make_relative_path(Filesystem::path_format(), ancestor, descendant);
}
inline SplitPath split_path(StringView path) {
    return split_path(Filesystem::path_format(), path);
}
inline SplitExtension split_file_extension(StringView path) {
    return split_file_extension(Filesystem::path_format(), path);
}
inline Array<StringView> split_path_full(StringView path) {
    return split_path_full(Filesystem::path_format(), path);
}
template <typename... StringViews>
String join_path(StringViews&&... path_component_args) {
    FixedArray<StringView, sizeof...(StringViews)> components{std::forward<StringViews>(path_component_args)...};
    return join_path_from_array(Filesystem::path_format(), components);
}

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if !defined(PLY_WITH_DIRECTORY_WATCHER)
#define PLY_WITH_DIRECTORY_WATCHER 0
#endif

#if PLY_WITH_DIRECTORY_WATCHER

class DirectoryWatcher {
public:
    String root;
    Functor<void(StringView path, bool must_recurse)> callback;

private:
    Thread watcher_thread;
#if defined(_WIN32)
    HANDLE end_event = INVALID_HANDLE_VALUE;
#elif defined(__APPLE__)
    void* run_loop = nullptr;
#endif

    void run_watcher();

public:
    DirectoryWatcher();
    DirectoryWatcher(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback) {
        this->start(root, std::move(callback));
    }
    DirectoryWatcher(StringView root, const Functor<void(StringView path, bool must_recurse)>& callback) {
        this->start(root, Functor<void(StringView, bool)>{callback});
    }
    ~DirectoryWatcher() {
        this->stop();
    }

    void start(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback);
    void stop();
};

#endif

//   ▄▄▄▄         ▄▄
//  ██  ▀▀ ▄▄  ▄▄ ██▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██  ██ ██  ██ ██  ██ ██  ▀▀ ██  ██ ██    ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ██▄▄█▀ ██▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//                       ██

struct Subprocess {
    enum PipeType {
        PIPE_OPEN,
        PIPE_REDIRECT, // This will redirect output to /dev/null if corresponding Out_Pipe
                       // (std_out_pipe/std_err_pipe) is unopened
        PIPE_STD_OUT,
    };

    struct Output {
        PipeType std_out = PIPE_REDIRECT;
        PipeType std_err = PIPE_REDIRECT;
        Pipe* std_out_pipe = nullptr;
        Pipe* std_err_pipe = nullptr;

        static Output ignore() {
            return {};
        }
        static Output inherit() {
            Output h;
            h.std_out_pipe = get_stdout_pipe();
            h.std_err_pipe = get_stderr_pipe();
            return h;
        }
        static Output open_separate() {
            Output h;
            h.std_out = PIPE_OPEN;
            h.std_err = PIPE_OPEN;
            return h;
        }
        static Output open_merged() {
            Output h;
            h.std_out = PIPE_OPEN;
            h.std_err = PIPE_STD_OUT;
            return h;
        }
        static Output open_std_out_only() {
            Output h;
            h.std_out = PIPE_OPEN;
            return h;
        }
    };

    struct Input {
        PipeType std_in = PIPE_REDIRECT;
        Pipe* std_in_pipe = nullptr;

        static Input ignore() {
            return {};
        }
        static Input inherit() {
            return {PIPE_REDIRECT, get_stdin_pipe()};
        }
        static Input open() {
            return {PIPE_OPEN, nullptr};
        }
    };

    // Members
    Owned<Pipe> write_to_std_in;
    Owned<Pipe> read_from_std_out;
    Owned<Pipe> read_from_std_err;

#if defined(_WIN32)
    HANDLE child_process = INVALID_HANDLE_VALUE;
    HANDLE child_main_thread = INVALID_HANDLE_VALUE;
#elif defined(PLY_POSIX)
    int child_pid = -1;
#endif

    Subprocess() = default;

    static Owned<Subprocess> exec(StringView exe_path, ArrayView<const StringView> args, StringView initial_dir,
                                  const Output& output, const Input& input = Input::open());
    static Owned<Subprocess> exec_arg_str(StringView exe_path, StringView arg_str, StringView initial_dir,
                                          const Output& output, const Input& input = Input::open());
    s32 join();
};

} // namespace ply
