/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "ply-base.h"

#if defined(_WIN32)
#include <shellapi.h>
#elif defined(PLY_POSIX)
#include <string>
#include <fstream>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#if defined(__APPLE__)
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#endif

namespace ply {

//  ▄▄▄▄▄▄ ▄▄                      ▄▄▄        ▄▄▄▄▄          ▄▄
//    ██   ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄      ██ ▀▀       ██  ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄
//    ██   ██ ██ ██ ██ ██▄▄██     ▄█▀█▄▀▀     ██  ██  ▄▄▄██  ██   ██▄▄██
//    ██   ██ ██ ██ ██ ▀█▄▄▄      ▀█▄▄▀█▄     ██▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄
//

#if defined(_WIN32)

float get_cpu_ticks_per_second() {
    static LARGE_INTEGER frequency;
    BOOL rc = QueryPerformanceFrequency(&frequency);
    PLY_ASSERT(rc);
    PLY_UNUSED(rc);
    return (float) frequency.QuadPart;
}

s64 get_current_timestamp() {
    FILETIME file_time;
    ULARGE_INTEGER large_integer;

    GetSystemTimeAsFileTime(&file_time);
    large_integer.LowPart = file_time.dwLowDateTime;
    large_integer.HighPart = file_time.dwHighDateTime;
    return s64(large_integer.QuadPart / 10) - 11644473600000000ll;
}

#elif defined(PLY_POSIX)

#if PLY_USE_POSIX_2008_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif

s64 get_current_timestamp() {
#if PLY_USE_POSIX_2008_CLOCK
    struct timespec tick;
    clock_gettime(CLOCK_REALTIME, &tick);
    return (s64) tick.tv_sec * 1000000 + tick.tv_nsec / 1000;
#else
    struct timeval tick;
    gettimeofday(&tick, NULL);
    return (s64) tick.tv_sec * 1000000 + tick.tv_usec;
#endif
}

#endif

// Based on http://howardhinnant.github.io/date_algorithms.html
static void set_date_from_epoch_days(DateTime* date_time, s32 days) {
    days += 719468;
    s32 era = (days >= 0 ? days : days - 146096) / 146097;
    u32 doe = u32(days - era * 146097);                              // [0, 146096]
    u32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    s32 y = s32(yoe) + era * 400;
    u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    u32 mp = (5 * doy + 2) / 153;                      // [0, 11]
    u32 d = doy - (153 * mp + 2) / 5 + 1;              // [1, 31]
    u32 m = mp + (mp < 10 ? 3 : -9);                   // [1, 12]
    date_time->year = y + (m <= 2);
    date_time->month = (u8) m;
    date_time->day = (u8) d;
    date_time->weekday = (u8) (days >= -4 ? (days + 4) % 7 : (days + 5) % 7 + 6);
}

// Based on http://howardhinnant.github.io/date_algorithms.html
static s32 get_epoch_days_from_date(const DateTime& date_time) {
    s32 m = date_time.month;
    s32 y = date_time.year - (m <= 2);
    s32 era = (y >= 0 ? y : y - 399) / 400;
    u32 yoe = u32(y - era * 400);                                          // [0, 399]
    u32 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + date_time.day - 1; // [0, 365]
    u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                       // [0, 146096]
    return era * 146097 + doe - 719468;
}

static s64 floor_div(s64 dividend, s64 divisor) {
    return (dividend > 0 ? dividend : dividend - divisor + 1) / divisor;
};

DateTime convert_to_calendar_time(s64 timestamp) {
    const s64 microsecs_per_day = 86400000000ll;
    s64 days = floor_div(timestamp, microsecs_per_day);
    s64 microsecs_in_day = timestamp - (days * microsecs_per_day);

    DateTime date_time;
    set_date_from_epoch_days(&date_time, numeric_cast<u32>(days));
    u32 secs = numeric_cast<u32>(microsecs_in_day / 1000000);
    u32 minutes = secs / 60;
    u32 hours = minutes / 60;
    date_time.hour = (u8) hours;
    date_time.minute = (u8) (minutes - hours * 60);
    date_time.second = (u8) (secs - minutes * 60);
    date_time.microsecond = (u32) (microsecs_in_day - u64(secs) * 1000000);
    return date_time;
}

s64 convert_to_timestamp(const DateTime& date_time) {
    static constexpr s64 microsecs_per_day = 86400000000ll;
    s32 days = get_epoch_days_from_date(date_time);
    s32 minutes = s32(date_time.hour) * 60 + date_time.minute;
    s32 seconds = minutes * 60 + date_time.second;
    return s64(days) * microsecs_per_day + s64(seconds) * 1000000 + date_time.microsecond;
}

//  ▄▄▄▄▄                    ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀█▄  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██ ██ ██
//  ██  ██ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄█▀ ██ ██ ██
//

Random::Random() {
    // Seed using misc. information from the environment
    u64 t = get_current_timestamp();
    t = shuffle_bits(t);
    s[0] = shuffle_bits(t | 1);

    t = get_cpu_ticks();
    t = shuffle_bits(t) + (shuffle_bits(get_current_thread_id()) << 1);
    s[1] = shuffle_bits(t | 1);

    for (u32 i = 0; i < 10; i++) {
        generate_u64();
    }
}

Random::Random(u64 seed) {
    s[0] = shuffle_bits(seed + 1);
    s[1] = shuffle_bits(s[0] + 1);
    generate_u64();
    generate_u64();
}

static inline u64 rotl(const u64 x, int k) {
    return (x << k) | (x >> (64 - k));
}

u64 Random::generate_u64() {
    const u64 s0 = s[0];
    u64 s1 = s[1];
    const u64 result = rotl(s0 * 5, 7) * 9;

    s1 ^= s0;
    s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
    s[1] = rotl(s1, 37);                   // c

    return result;
}

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

#if defined(_WIN32)

DWORD WINAPI thread_entry(LPVOID param) {
    Functor<void()>* entry = static_cast<Functor<void()>*>(param);
    (*entry)();
    Heap::destroy(entry);
    return 0;
}

#elif defined(PLY_POSIX)

void* thread_entry(void* arg) {
    Functor<void()>* entry = static_cast<Functor<void()>*>(arg);
    (*entry)();
    Heap::destroy(entry);
    return nullptr;
}

#endif

//  ▄▄   ▄▄ ▄▄         ▄▄                 ▄▄▄  ▄▄   ▄▄
//  ██   ██ ▄▄ ▄▄▄▄▄  ▄██▄▄ ▄▄  ▄▄  ▄▄▄▄   ██  ███▄███  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄
//   ██ ██  ██ ██  ▀▀  ██   ██  ██  ▄▄▄██  ██  ██▀█▀██ ██▄▄██ ██ ██ ██ ██  ██ ██  ▀▀ ██  ██
//    ▀█▀   ██ ██      ▀█▄▄ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██   ██ ▀█▄▄▄  ██ ██ ██ ▀█▄▄█▀ ██     ▀█▄▄██
//                                                                                    ▄▄▄█▀

#if defined(_WIN32)

VirtualMemory::Info VirtualMemory::get_info() {
    static VirtualMemory::Info info = []() {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        PLY_ASSERT(is_power_of_2((u32) sys_info.dwAllocationGranularity));
        PLY_ASSERT(is_power_of_2((u32) sys_info.dwPageSize));
        return VirtualMemory::Info{sys_info.dwAllocationGranularity, sys_info.dwPageSize};
    }();
    return info;
}

bool VirtualMemory::alloc(char*& out_addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

    DWORD type = MEM_RESERVE | MEM_COMMIT;
    out_addr = (char*) VirtualAlloc(0, (SIZE_T) num_bytes, type, PAGE_READWRITE);
    return (out_addr != NULL);
}

bool VirtualMemory::reserve(char*& out_addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

    DWORD type = MEM_RESERVE;
    out_addr = (char*) VirtualAlloc(0, (SIZE_T) num_bytes, type, PAGE_READWRITE);
    return (out_addr != NULL);
}

void VirtualMemory::commit(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().page_size));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().page_size));

    DWORD type = MEM_COMMIT;
    LPVOID result = VirtualAlloc(addr, (SIZE_T) num_bytes, type, PAGE_READWRITE);
    PLY_ASSERT(result != NULL);
    PLY_UNUSED(result);
}

void VirtualMemory::decommit(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().page_size));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().page_size));

    DWORD type = MEM_COMMIT;
    LPVOID result = VirtualAlloc(addr, (SIZE_T) num_bytes, type, PAGE_READWRITE);
    PLY_ASSERT(result != NULL);
    PLY_UNUSED(result);
}

void VirtualMemory::free(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().alloc_alignment));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

#if defined(PLY_WITH_ASSERTS)
    {
        // Must be entire reserved address space range
        MEMORY_BASIC_INFORMATION mem_info;
        SIZE_T rc = VirtualQuery(addr, &mem_info, sizeof(mem_info));
        PLY_ASSERT(rc != 0);
        PLY_UNUSED(rc);
        PLY_ASSERT(mem_info.BaseAddress == addr);
        PLY_ASSERT(mem_info.AllocationBase == addr);
        PLY_ASSERT(mem_info.RegionSize <= num_bytes);
    }
#endif
    BOOL rc2 = VirtualFree(addr, 0, MEM_RELEASE);
    PLY_ASSERT(rc2);
    PLY_UNUSED(rc2);
}

#elif defined(PLY_POSIX)

VirtualMemory::Info VirtualMemory::get_info() {
    static VirtualMemory::Info info = []() {
        long result = sysconf(_SC_PAGE_SIZE);
        PLY_ASSERT(is_power_of_2((u64) result));
        return VirtualMemory::Info{(uptr) result, (uptr) result};
    }();
    return info;
}

bool VirtualMemory::alloc(char*& out_addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

    out_addr = (char*) mmap(0, num_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    PLY_ASSERT(out_addr != MAP_FAILED);
    return true;
}

bool VirtualMemory::reserve(char*& out_addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

    out_addr = (char*) mmap(0, num_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    PLY_ASSERT(out_addr != MAP_FAILED);
    return true;
}

void VirtualMemory::commit(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().page_size));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().page_size));

    int rc = mprotect(addr, num_bytes, PROT_READ | PROT_WRITE);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
}

void VirtualMemory::decommit(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().page_size));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().page_size));

    int rc = madvise(addr, num_bytes, MADV_DONTNEED);
    PLY_ASSERT(rc == 0);
    rc = mprotect(addr, num_bytes, PROT_NONE);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
}

void VirtualMemory::free(char* addr, uptr num_bytes) {
    PLY_ASSERT(is_aligned_to_power_of_2((uptr) addr, VirtualMemory::get_info().alloc_alignment));
    PLY_ASSERT(is_aligned_to_power_of_2(num_bytes, VirtualMemory::get_info().alloc_alignment));

    munmap(addr, num_bytes);
}

#endif

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

/*
Heap_Stats get_heap_stats() {
    Heap_Stats stats;
    Lock_Guard<Mutex> guard(this->mutex);
    memory_dl::dlmalloc_stats(&this->mstate, stats);
    return stats;
}
*/

#if !defined(PLY_OVERRIDE_NEW)
#define PLY_OVERRIDE_NEW 1
#endif

#if PLY_OVERRIDE_NEW

} // namespace ply

//---------------------------------------------------------------------------
// Override operators new/delete
// C++ allows us to replace global operators new/delete with our own thanks to weak linking.
//---------------------------------------------------------------------------
void* operator new(std::size_t size) {
    return ply::Heap::alloc(size);
}

void* operator new[](std::size_t size) {
    return ply::Heap::alloc(size);
}

void operator delete(void* ptr) noexcept {
    ply::Heap::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    ply::Heap::free(ptr);
}

void* operator new(std::size_t size, std::nothrow_t const&) noexcept {
    return ply::Heap::alloc(size);
}

void* operator new[](std::size_t size, std::nothrow_t const&) noexcept {
    return ply::Heap::alloc(size);
}

void operator delete(void* ptr, std::nothrow_t const&) noexcept {
    ply::Heap::free(ptr);
}

void operator delete[](void* ptr, std::nothrow_t const&) noexcept {
    ply::Heap::free(ptr);
}

namespace ply {

#endif // PLY_OVERRIDE_NEW

//   ▄▄▄▄   ▄▄          ▄▄               ▄▄   ▄▄ ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄ ██   ██ ▄▄  ▄▄▄▄  ▄▄    ▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██  ██ ██  ██ ██▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██   ▀█▀   ██ ▀█▄▄▄   ██▀▀██
//                                 ▄▄▄█▀

bool StringView::starts_with(StringView other) const {
    if (other.num_bytes > num_bytes)
        return false;
    return memcmp(bytes, other.bytes, other.num_bytes) == 0;
}

bool StringView::ends_with(StringView other) const {
    if (other.num_bytes > num_bytes)
        return false;
    return memcmp(bytes + num_bytes - other.num_bytes, other.bytes, other.num_bytes) == 0;
}

StringView StringView::trim(bool (*match_func)(char), bool left, bool right) const {
    const char* start = this->bytes;
    const char* end = start + this->num_bytes;
    if (left) {
        while ((start < end) && match_func(*start)) {
            start++;
        }
    }
    if (right) {
        while ((start < end) && match_func(end[-1])) {
            end--;
        }
    }
    return {start, end};
}

Array<StringView> StringView::split_byte(char sep) const {
    Array<StringView> result;
    const char* cur = this->bytes;
    const char* end = this->bytes + this->num_bytes;
    const char* split_start = nullptr;
    while (cur < end) {
        if (*cur == sep) {
            if (split_start) {
                result.append({split_start, cur});
                split_start = nullptr;
            }
        } else {
            if (!split_start) {
                split_start = cur;
            }
        }
        cur++;
    }
    if (split_start) {
        result.append({split_start, cur});
    }
    if (result.is_empty()) {
        result.append({});
    }
    return result;
}

String StringView::replace(StringView old_substr, StringView new_substr) const {
    PLY_ASSERT(old_substr.num_bytes > 0);
    MemStream out;
    u32 limit = this->num_bytes - old_substr.num_bytes;
    u32 i = 0;
    for (; i < limit; i++) {
        if (memcmp(this->bytes + i, old_substr.bytes, old_substr.num_bytes) == 0) {
            out.write(new_substr);
            i += old_substr.num_bytes - 1;
        } else {
            out.write(this->bytes[i]);
        }
    }
    if (i < this->num_bytes) {
        out.write({this->bytes + i, this->bytes + num_bytes});
    }
    return out.move_to_string();
}

String StringView::upper_asc() const {
    String result = String::allocate(this->num_bytes);
    for (u32 i = 0; i < this->num_bytes; i++) {
        char c = this->bytes[i];
        if (c >= 'a' && c <= 'z') {
            c += 'A' - 'a';
        }
        result.bytes[i] = c;
    }
    return result;
}

String StringView::lower_asc() const {
    String result = String::allocate(this->num_bytes);
    for (u32 i = 0; i < this->num_bytes; i++) {
        char c = this->bytes[i];
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
        result.bytes[i] = c;
    }
    return result;
}

String StringView::join(ArrayView<const StringView> comps) const {
    MemStream out;
    bool first = true;
    for (StringView comp : comps) {
        if (!first) {
            out.write(*this);
        }
        out.write(comp);
        first = false;
    }
    return out.move_to_string();
}

s32 compare(StringView a, StringView b) {
    u32 compare_bytes = min(a.num_bytes, b.num_bytes);
    const u8* u0 = (const u8*) a.bytes;
    const u8* u1 = (const u8*) b.bytes;
    const u8* u_end0 = u0 + compare_bytes;
    while (u0 < u_end0) {
        s32 diff = *u0 - *u1;
        if (diff != 0)
            return diff;
        u0++;
        u1++;
    }
    return a.num_bytes - b.num_bytes;
}

String operator+(StringView a, StringView b) {
    String result = String::allocate(a.num_bytes + b.num_bytes);
    memcpy(result.bytes, a.bytes, a.num_bytes);
    memcpy(result.bytes + a.num_bytes, b.bytes, b.num_bytes);
    return result;
}

String operator*(StringView str, u32 count) {
    String result = String::allocate(str.num_bytes * count);
    char* dst = result.bytes;
    for (u32 i = 0; i < count; i++) {
        memcpy(dst, str.bytes, str.num_bytes);
        dst += str.num_bytes;
    }
    return result;
}

s32 StringView::find(StringView pattern, u32 start_pos) const {
    if (start_pos + pattern.num_bytes > this->num_bytes)
        return -1;
    u32 limit = this->num_bytes - pattern.num_bytes;
    for (; start_pos <= limit; start_pos++) {
        for (u32 i = 0; i < pattern.num_bytes; i++) {
            if (pattern.bytes[i] != this->bytes[start_pos + i])
                goto next;
        }
        return start_pos;
    next:;
    }
    return -1;
}

s32 StringView::reverse_find(StringView pattern, s32 start_pos) const {
    if (start_pos < 0) {
        start_pos += this->num_bytes;
    }
    if (start_pos + pattern.num_bytes >= this->num_bytes) {
        start_pos = (s32) this->num_bytes - pattern.num_bytes;
    }
    for (; start_pos >= 0; start_pos--) {
        for (u32 i = 0; i < pattern.num_bytes; i++) {
            if (pattern.bytes[i] != this->bytes[start_pos + i])
                goto next;
        }
        // Found a match.
        return start_pos;
    next:;
    }
    return -1;
}

struct MatchState {
    ViewStream* str;
    ViewStream* pattern;
    ArrayView<const MatchArg> match_args;
    u32 arg_index = 0;
};

enum class MatchMode {
    Matching,
    Skipping,
};

// match_pattern_segment reads pattern elements from state.pattern up until the next `)` or `$`.
// If mode == Matching, it checks whether the input string matches the pattern segment, returning true if successful.
// If mode == Skipping, the pattern is read without checking whether the input string matches.
// When the pattern element is a format specifier (like `%d`), it attempts to read a formatted value from the input
// string.
// `%d` reads integer values.
// `%f` reads floating-point values.
// `%q` reads quoted strings.
// `%i` reads identifiers.
// If successful, the value is captured by an lambda expression, which later commits the value to an output argument
// if the rest of the segment matches successfully.
// When the pattern element is a space ` `, it checks whether the input string contains a whitespace character,
// including spaces, tabs, and newlines. When the pattern element is a parenthesis `(`, it reads a sub-pattern segment
// recursively. The pattern can contain alternative clauses separated by `|`. When a `|` is encountered in the pattern
// string, it ends the current clause and begins reading the next clause. If the previous clause was successful, the
// next clause is skipped. If the previous clause was unsuccessful, the input string is reverted to the start of the
// previous clause and the next clause is tried. Empty clauses are not permitted. When a single quote `'` is
// encountered, the next character is interpreted as a literal character and matches against the input string. All other
// characters except for '?' and '*' are matched literally against the input string.
// '?' and '*' are treated as qualifiers that can follow other pattern elements except for `|`.
// `?` treats the previous element as optional. If it fails, the input string is reverted to the start of the pattern
// element and matching continues normally.
// `*` turns the previous element into a repeating element. If it matches, we try to match the element again, repeating
// as many times as possible. If it fails, the input string is reverted to the start of the pattern element (as of the
// latest iteration) and matching continues normally.
static bool match_pattern_segment(MatchState& state, MatchMode mode) {
    // Variables to track the success of the current segment.
    bool any_clause_succeeded = false;
    bool current_clause_succeeded = true;
    char* pattern_at_start_of_current_clause = state.pattern->cur_byte;
    char* input_at_start_of_current_clause = state.str->cur_byte;
    Array<Functor<void()>> output_variables_to_commit;

    // Main loop to read the pattern segment one element at a time.
    while (state.pattern->make_readable()) {
        // Variables to track the success of the current element.
        char* pattern_at_start_of_current_element = state.pattern->cur_byte;
        char* input_at_start_of_current_element = state.str->cur_byte;
        u32 arg_index_at_start_of_current_element = state.arg_index;
        bool element_matched = false;

        // Check the current pattern element.
        char pattern_element = *state.pattern->cur_byte++;
        if (pattern_element == '%') {
            // It's a format specifier. Read the expected value type.
            if (!state.pattern->make_readable()) {
                PLY_ASSERT(0); // Expected specification char after %
                return false;
            }

            // Get the next argument to capture the value.
            u32 i = state.arg_index++;
            const MatchArg& arg = state.match_args[i];

            // Clear any previous input errors.
            state.str->input_error = false;

            char spec = *state.pattern->cur_byte++;
            if (spec == 'i') { // Identifier
                if (mode == MatchMode::Matching) {
                    StringView id = read_identifier(*state.str);
                    if (!id.is_empty()) {
                        element_matched = true;
                        output_variables_to_commit.append([arg, id]() {
                            if (auto* ptr = arg.as<StringView*>()) {
                                **ptr = id;
                            } else if (auto* ptr = arg.as<String*>()) {
                                **ptr = id;
                            } else {
                                PLY_ASSERT(0); // Argument type incompatible with %i specifier
                            }
                        });
                    }
                }
            } else if (spec == 'd') { // Integer
                if (mode == MatchMode::Matching) {
                    if (arg.is<u64*>() || arg.is<u32*>()) {
                        u64 val = read_u64_from_text(*state.str);
                        if (!state.str->input_error) {
                            element_matched = true;
                            output_variables_to_commit.append([arg, val]() {
                                if (auto* ptr = arg.as<u64*>()) {
                                    **ptr = val;
                                } else {
                                    **arg.as<u32*>() = (u32) val;
                                }
                            });
                        }
                    } else if (arg.is<s64*>() || arg.is<s32*>()) {
                        s64 val = read_s64_from_text(*state.str);
                        if (!state.str->input_error) {
                            element_matched = true;
                            output_variables_to_commit.append([arg, val]() {
                                if (auto* ptr = arg.as<s64*>()) {
                                    **ptr = val;
                                } else {
                                    **arg.as<s32*>() = (s32) val;
                                }
                            });
                        }
                    } else {
                        PLY_ASSERT(0); // Argument type incompatible with %d specifier
                    }
                }
            } else if (spec == 'f') { // Float
                if (mode == MatchMode::Matching) {
                    double val = read_double_from_text(*state.str);
                    if (!state.str->input_error) {
                        element_matched = true;
                        output_variables_to_commit.append([arg, val]() {
                            if (auto* ptr = arg.as<double*>()) {
                                **ptr = val;
                            } else {
                                **arg.as<float*>() = (float) val;
                            }
                        });
                    }
                }
            } else if (spec == 'q') { // Quoted string
                if (mode == MatchMode::Matching) {
                    String val = read_quoted_string(*state.str);
                    if (!state.str->input_error) {
                        element_matched = true;
                        output_variables_to_commit.append([arg, val = std::move(val)]() {
                            if (auto* ptr = arg.as<String*>()) {
                                **ptr = std::move(val);
                            } else {
                                PLY_ASSERT(0); // Argument type incompatible with %q specifier
                            }
                        });
                    }
                }
            } else {
                PLY_ASSERT(0); // Unknown format specifier
            }
        } else if (pattern_element == ' ') {
            // It's a space character. Try to match whitespace.
            if (mode == MatchMode::Matching) {
                if (state.str->make_readable() && is_whitespace(*state.str->cur_byte)) {
                    element_matched = true;
                    state.str->cur_byte++;
                }
            }
        } else if (pattern_element == '(') {
            // It's a left parenthesis. Read a sub-pattern segment recursively.
            element_matched = match_pattern_segment(state, mode);
            if (!state.pattern->make_readable()) {
                PLY_ASSERT(0); // Expected a character after the opening parenthesis.
                return false;
            }
            char c = *state.pattern->cur_byte++;
            PLY_ASSERT(c == ')'); // Expected a closing parenthesis.
        } else if ((pattern_element == ')') || (pattern_element == '$')) {
            state.pattern->cur_byte--;
            break;
        } else if (pattern_element == '|') {
            // It's a vertical bar. End the current clause and begin reading the next clause.
            if (mode == MatchMode::Matching) {
                if (current_clause_succeeded) {
                    any_clause_succeeded = true;
                    for (Functor<void()>& commit : output_variables_to_commit) {
                        commit();
                    }
                    output_variables_to_commit.clear();
                    mode = MatchMode::Skipping;
                } else {
                    // Reset status variables and try to match the next clause.
                    current_clause_succeeded = true;
                    pattern_at_start_of_current_clause = state.pattern->cur_byte;
                    state.str->cur_byte = input_at_start_of_current_clause;
                    output_variables_to_commit.clear();
                }
            }
            continue; // Skip the check for ? or * qualifiers
        } else if (pattern_element == '\'') {
            // It's a single quote. Treat the next pattern character as a literal character.
            if (!state.pattern->make_readable()) {
                PLY_ASSERT(0); // Expected a character to follow `'`.
                return false;
            }
            char escaped = *state.pattern->cur_byte++;
            if (mode == MatchMode::Matching) {
                if (state.str->make_readable() && *state.str->cur_byte == escaped) {
                    element_matched = true;
                    state.str->cur_byte++;
                }
            }
        } else {
            PLY_ASSERT((pattern_element != '*') && (pattern_element != '?')); // Unexpected quantifier.
            if (mode == MatchMode::Matching) {
                if (state.str->make_readable() && *state.str->cur_byte == pattern_element) {
                    element_matched = true;
                    state.str->cur_byte++;
                }
            }
        }

        if (state.pattern->make_readable()) {
            char c = *state.pattern->cur_byte;
            if (c == '?') {
                // It's a question mark. Make the current element optional.
                state.pattern->cur_byte++;
                if ((mode == MatchMode::Matching) && !element_matched) {
                    // The current element didn't match, but was optional.
                    // Revert the input string to the start of the current element.
                    state.str->cur_byte = input_at_start_of_current_element;
                }
                continue;
            } else if (c == '*') {
                // It's a star. Make the current element repeatable.
                state.pattern->cur_byte++;
                // It's illegal to capture variables inside repeated elements:
                PLY_ASSERT(state.arg_index == arg_index_at_start_of_current_element); // No repeated captures!
                if ((mode == MatchMode::Matching) && element_matched) {
                    // The current element matched. Rewind the pattern to the start of the current element
                    // and try to match it again.
                    state.pattern->cur_byte = pattern_at_start_of_current_element;
                } else {
                    // The current element didn't match, but was repeatable.
                    // Revert the input string to the start of the current element.
                    state.str->cur_byte = input_at_start_of_current_element;
                }
                continue;
            }
        }

        if ((mode == MatchMode::Matching) && !element_matched) {
            // The current element didn't match.
            current_clause_succeeded = false;
        }
    }

    // We reached the end of the segment.
    if ((mode == MatchMode::Matching) && current_clause_succeeded) {
        any_clause_succeeded = true;
        for (Functor<void()>& commit : output_variables_to_commit) {
            commit();
        }
    }

    return any_clause_succeeded;
}

bool match_with_args(ViewStream& in, StringView pattern, ArrayView<const MatchArg> match_args) {
    MatchState state;
    ViewStream pattern_in{pattern};
    state.str = &in;
    state.pattern = &pattern_in;
    state.match_args = match_args;
    state.arg_index = 0;

    if (!match_pattern_segment(state, MatchMode::Matching))
        return false;

    if (state.pattern->make_readable() && (*state.pattern->cur_byte == '$')) {
        if (state.str->make_readable())
            return false; // Expected end of string
    }

    // Check that we consumed all match args
    PLY_ASSERT(state.arg_index == match_args.num_items());

    return true;
}

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

String::String(StringView other) : bytes{(char*) Heap::alloc(other.num_bytes)}, num_bytes{other.num_bytes} {
    memcpy(this->bytes, other.bytes, other.num_bytes);
}

String String::allocate(u32 num_bytes) {
    String result;
    result.bytes = (char*) Heap::alloc(num_bytes);
    result.num_bytes = num_bytes;
    return result;
}

void String::resize(u32 num_bytes) {
    this->bytes = (char*) Heap::realloc(this->bytes, num_bytes);
    this->num_bytes = num_bytes;
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

void add_to_hash(HashBuilder& builder, u32 value) {
    value *= 0xcc9e2d51u;
    value = (value << 15) | (value >> 17);
    value *= 0x1b873593u;
    builder.accumulator ^= value;
    builder.accumulator = (builder.accumulator << 13) | (builder.accumulator >> 19);
    builder.accumulator = builder.accumulator * 5 + 0xe6546b64u;
}

void add_to_hash(HashBuilder& builder, StringView str) {
    // FIXME: More work is needed for platforms that don't support unaligned reads
    while (str.num_bytes >= 4) {
        add_to_hash(builder, *(const u32*) str.bytes); // May be unaligned
        str.bytes += 4;
        str.num_bytes -= 4;
    }
    if (str.num_bytes > 0) {
        // Avoid potential unaligned read across page boundary
        u32 v = 0;
        while (str.num_bytes > 0) {
            v = (v << 8) | *(const u8*) str.bytes;
            str.bytes++;
            str.num_bytes--;
        }
        add_to_hash(builder, v);
    }
}

//  ▄▄  ▄▄               ▄▄     ▄▄                  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ██     ▄▄▄▄   ▄▄▄▄  ██  ▄▄ ▄▄  ▄▄ ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██    ██  ██ ██  ██ ██▄█▀  ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄██ ██▄▄█▀
//                                                                ██

u32 get_best_num_hash_indices(u32 num_items) {
    if (num_items >= 8) {
        return round_up_to_nearest_to_power_of_2(u32((u64{num_items} * 5) >> 2));
    }
    return (num_items < 4) ? 4 : 8;
}

//  ▄▄▄▄▄  ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀▀  ██ ██  ██ ██▄▄██
//  ██     ██ ██▄▄█▀ ▀█▄▄▄
//            ██

// Define static constexpr members for ODR-usage
constexpr u32 Pipe::HAS_READ_PERMISSION;
constexpr u32 Pipe::HAS_WRITE_PERMISSION;
constexpr u32 Pipe::CAN_SEEK;

u32 Pipe::read(MutStringView buf) {
    PLY_ASSERT(0);
    return 0;
}

bool Pipe::write(StringView buf) {
    PLY_ASSERT(0);
    return false;
}

void Pipe::flush(bool to_device) {
    PLY_ASSERT(0);
}

u64 Pipe::get_file_size() {
    // This method is unsupported by the subclass. Do not call.
    PLY_ASSERT(0);
    return 0;
}

void Pipe::seek_to(s64 offset) {
    PLY_ASSERT(0);
}

#if defined(_WIN32)

PipeHandle::~PipeHandle() {
    if (this->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(this->handle);
    }
}

u32 PipeHandle::read(MutStringView buf) {
    DWORD read_bytes;
    BOOL rc = ReadFile(this->handle, buf.bytes, buf.num_bytes, &read_bytes, NULL);
    if (!rc) // Handles ERROR_BROKEN_PIPE and other errors.
        return 0;
    return read_bytes; // 0 when attempting to read past EOF.
}

bool PipeHandle::write(StringView buf) {
    while (buf.num_bytes > 0) {
        DWORD desired_bytes = min<DWORD>(buf.num_bytes, UINT32_MAX);
        DWORD written_bytes;
        BOOL rc = WriteFile(this->handle, buf.bytes, desired_bytes, &written_bytes, NULL);
        if (!rc) // Handles ERROR_NO_DATA and other errors.
            return false;
        buf.bytes += written_bytes;
        buf.num_bytes -= written_bytes;
    }
    return true;
}

void PipeHandle::flush(bool to_device) {
    if (to_device) {
        FlushFileBuffers(this->handle);
    }
}

u64 PipeHandle::get_file_size() {
    LARGE_INTEGER file_size;
    GetFileSizeEx(this->handle, &file_size);
    return file_size.QuadPart;
}

void PipeHandle::seek_to(s64 offset) {
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    SetFilePointerEx(this->handle, distance, NULL, FILE_BEGIN);
}

#elif defined(PLY_POSIX)

Pipe_FD::~Pipe_FD() {
    if (this->fd >= 0) {
        int rc = ::close(this->fd);
        PLY_ASSERT(rc == 0);
        PLY_UNUSED(rc);
    }
}

u32 Pipe_FD::read(MutStringView buf) {
    PLY_ASSERT(this->fd >= 0);
    // Retry as long as read() keeps failing due to EINTR caused by the debugger:
    s32 rc;
    do {
        rc = (s32)::read(this->fd, buf.bytes, buf.num_bytes);
    } while (rc == -1 && errno == EINTR);
    PLY_ASSERT(rc >= 0); // Note: Will probably need to detect closed pipes here
    if (rc < 0)
        return 0;
    return rc;
}

bool Pipe_FD::write(StringView buf) {
    PLY_ASSERT(this->fd >= 0);
    while (buf.num_bytes > 0) {
        s32 sent = (s32)::write(this->fd, buf.bytes, buf.num_bytes);
        if (sent <= 0)
            return false;
        PLY_ASSERT((u32) sent <= buf.num_bytes);
        buf.bytes += sent;
        buf.num_bytes -= sent;
    }
    return true;
}

void Pipe_FD::flush(bool to_device) {
    // FIXME: Implement as per
    // https://github.com/libuv/libuv/issues/1579#issue-262113760
}

u64 Pipe_FD::get_file_size() {
    PLY_ASSERT(this->fd >= 0);
    struct stat buf;
    int rc = fstat(this->fd, &buf);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    return buf.st_size;
}

void Pipe_FD::seek_to(s64 offset) {
    PLY_ASSERT(this->fd >= 0);
    off_t rc = lseek(this->fd, numeric_cast<off_t>(offset), SEEK_SET);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
}

#endif

//---------------------

struct NewLineFilter {
    struct Params {
        const char* src_byte = nullptr;
        const char* src_end_byte = nullptr;
        char* dst_byte = nullptr;
        char* dst_end_byte = nullptr;
    };

    bool crlf = false; // If true, outputs \r\n instead of \n
    bool needs_lf = false;

    void process(Params* params) {
        while (params->dst_byte < params->dst_end_byte) {
            u8 c = 0;
            if (this->needs_lf) {
                c = '\n';
                this->needs_lf = false;
            } else {
                for (;;) {
                    if (params->src_byte >= params->src_end_byte)
                        return; // src has been consumed
                    c = *params->src_byte++;
                    if (c == '\r') {
                        // Output nothing
                    } else {
                        if (c == '\n' && this->crlf) {
                            c = '\r';
                            this->needs_lf = true;
                        }
                        break;
                    }
                }
            }
            *params->dst_byte++ = c;
        }
    }
};

//-----------------------------------------------------------------------

class InPipeNewLineFilter : public Pipe {
public:
    Stream in;
    NewLineFilter filter;

    InPipeNewLineFilter(Stream&& in) : in{std::move(in)} {
        PLY_ASSERT(this->in.has_read_permission);
        this->flags = Pipe::HAS_READ_PERMISSION;
    }
    virtual u32 read(MutStringView buf) override;
};

u32 InPipeNewLineFilter::read(MutStringView buf) {
    PLY_ASSERT(buf.num_bytes > 0);

    NewLineFilter::Params params;
    params.dst_byte = buf.bytes;
    params.dst_end_byte = buf.bytes + buf.num_bytes;
    for (;;) {
        params.src_byte = this->in.cur_byte;
        params.src_end_byte = this->in.end_byte;
        this->filter.process(&params);

        this->in.cur_byte = const_cast<char*>(params.src_byte);
        u32 num_bytes_written = numeric_cast<u32>(params.dst_byte - buf.bytes);
        if (num_bytes_written > 0)
            return num_bytes_written;

        PLY_ASSERT(this->in.num_remaining_bytes() == 0);
        if (!this->in.make_readable())
            return 0;
    }
}

//-----------------------------------------------------------------------

class OutPipeNewLineFilter : public Pipe {
public:
    Stream out;
    NewLineFilter filter;

    OutPipeNewLineFilter(Stream&& out, bool write_crlf) : out{std::move(out)} {
        PLY_ASSERT(this->out.has_write_permission);
        this->flags = Pipe::HAS_WRITE_PERMISSION;
        this->filter.crlf = write_crlf;
    }
    virtual bool write(StringView buf) override;
    virtual void flush(bool to_device) override;
};

bool OutPipeNewLineFilter::write(StringView buf) {
    u32 desired_total_bytes_read = buf.num_bytes;
    u32 total_bytes_read = 0;
    for (;;) {
        this->out.make_writable();

        // If try_make_bytes_available fails, process() will do nothing and we'll simply
        // return below:
        NewLineFilter::Params params;
        params.src_byte = buf.bytes;
        params.src_end_byte = buf.bytes + buf.num_bytes;
        params.dst_byte = this->out.cur_byte;
        params.dst_end_byte = this->out.end_byte;
        this->filter.process(&params);
        this->out.cur_byte = params.dst_byte;
        u32 num_bytes_read = numeric_cast<u32>(params.src_byte - buf.bytes);
        if (num_bytes_read == 0) {
            PLY_ASSERT(total_bytes_read <= desired_total_bytes_read);
            return total_bytes_read >= desired_total_bytes_read;
        }
        total_bytes_read += num_bytes_read;
        buf = buf.substr(num_bytes_read);
    }
}

void OutPipeNewLineFilter::flush(bool to_device) {
    // Forward flush command down the output chain.
    this->out.flush(to_device);
};

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

Stream::Stream() {
}

Stream::Stream(Pipe* pipe, bool is_pipe_owner) {
    if (pipe) {
        this->type = Type::Pipe;
        new (&this->pipe) PipeData;
        this->pipe.pipe = pipe;
        this->is_pipe_owner = is_pipe_owner;
        this->pipe.buffer = (char*) Heap::alloc(BUFFER_SIZE);
        this->cur_byte = this->pipe.buffer;
        this->end_byte = this->pipe.buffer;
        this->has_read_permission = (pipe->get_flags() & Pipe::HAS_READ_PERMISSION) != 0;
        this->has_write_permission = (pipe->get_flags() & Pipe::HAS_WRITE_PERMISSION) != 0;
    }
}

Stream::Stream(Stream&& other) {
    this->cur_byte = other.cur_byte;
    this->end_byte = other.end_byte;
    this->type = other.type;
    this->mode = other.mode;
    this->is_pipe_owner = other.is_pipe_owner;
    this->has_read_permission = other.has_read_permission;
    this->has_write_permission = other.has_write_permission;
    this->at_eof = other.at_eof;
    this->input_error = other.input_error;
    if (this->type == Type::Pipe) {
        new (&this->pipe) PipeData{std::move(other.pipe)};
    } else if (this->type == Type::Mem) {
        new (&this->mem) MemData{std::move(other.mem)};
    } else if (this->type == Type::View) {
        new (&this->view) ViewData{std::move(other.view)};
    }
    new (&other) Stream;
}

Stream::~Stream() {
    if (this->type == Type::Pipe) {
        PLY_ASSERT(this->pipe.pipe);
        if (this->has_write_permission) {
            this->flush();
        }
        if (this->is_pipe_owner) {
            Heap::destroy(this->pipe.pipe);
        }
        Heap::free(this->pipe.buffer);
    } else if (this->type == Type::Mem) {
        for (char* buf : this->mem.buffers) {
            Heap::free(buf);
        }
        if (this->mem.temp_buffer) {
            Heap::free(this->mem.temp_buffer);
        }
        this->mem.~MemData();
    }
}

void Stream::flush_mem_writes() {
    PLY_ASSERT(this->type == Type::Mem);
    if (this->mode == Mode::Writing) {
        if (this->using_temp_buffer) {
            u32 num_bytes_written = numeric_cast<u32>(this->cur_byte - this->mem.temp_buffer);
            u32 space_available = BUFFER_SIZE - this->mem.temp_buffer_offset;
            memcpy(this->mem.buffers[this->mem.buffer_index] + this->mem.temp_buffer_offset, this->mem.temp_buffer,
                   min(num_bytes_written, space_available));
            if (space_available < num_bytes_written) {
                if (this->mem.buffer_index + 1 >= this->mem.buffers.num_items()) {
                    this->mem.buffers.append((char*) Heap::alloc(BUFFER_SIZE));
                    memcpy(this->mem.buffers.back(), this->mem.temp_buffer + space_available,
                           num_bytes_written - space_available);
                    this->mem.num_bytes_in_last_buffer = num_bytes_written - space_available;
                }
                this->mem.buffer_index++;
                this->cur_byte = this->mem.buffers[this->mem.buffer_index] + (num_bytes_written - space_available);
            } else {
                this->cur_byte =
                    this->mem.buffers[this->mem.buffer_index] + this->mem.temp_buffer_offset + num_bytes_written;
            }
            this->end_byte = this->mem.buffers[this->mem.buffer_index] + BUFFER_SIZE;
            this->using_temp_buffer = false;
        } else {
            if (this->mem.buffer_index + 1 == this->mem.buffers.num_items()) {
                // Extend number of bytes in the last buffer.
                this->mem.num_bytes_in_last_buffer = max(this->mem.num_bytes_in_last_buffer,
                                                         numeric_cast<u32>(this->cur_byte - this->mem.buffers.back()));
            }
        }
    }
}

bool Stream::make_readable_internal(u32 min_bytes) {
    PLY_ASSERT(this->has_read_permission);
    PLY_ASSERT(min_bytes <= MAX_CONSECUTIVE_BYTES);
    if ((this->mode == Mode::Reading) && (this->num_remaining_bytes() >= min_bytes))
        return true;

    if (this->type == Type::Pipe) {
        if (this->mode == Mode::Writing) {
            // Write any buffered data to the pipe.
            this->pipe.pipe->write({this->pipe.buffer, this->cur_byte});
            this->pipe.seek_pos_at_buffer += (this->cur_byte - this->pipe.buffer);
        }
        if (this->mode != Mode::Reading) {
            // Reset buffer contents.
            this->cur_byte = this->pipe.buffer;
            this->end_byte = this->pipe.buffer;
            this->mode = Mode::Reading;
        } else {
            // Keep any bytes we have.
            u32 num_to_preserve = this->num_remaining_bytes();
            if (num_to_preserve > 0) {
                memmove(this->pipe.buffer, this->cur_byte, this->num_remaining_bytes());
            }
            this->pipe.seek_pos_at_buffer += (this->cur_byte - this->pipe.buffer);
            this->cur_byte = this->pipe.buffer;
            this->end_byte = this->pipe.buffer + num_to_preserve;
        }

        do {
            // Load data into the buffer.
            u32 num_bytes_loaded = this->pipe.pipe->read({this->end_byte, BUFFER_SIZE - this->num_remaining_bytes()});
            if (num_bytes_loaded == 0) {
                if (this->num_remaining_bytes() == 0) {
                    this->at_eof = true;
                }
                return false;
            }
            this->end_byte += num_bytes_loaded;
        } while (this->num_remaining_bytes() < min_bytes);

        // We have at least the number of bytes the caller asked for.
        return true;
    } else if (this->type == Type::Mem) {
        this->flush_mem_writes();

        if (this->num_remaining_bytes() == 0) {
            if (this->mem.buffer_index + 1 < this->mem.buffers.num_items()) {
                this->mem.buffer_index++;
                this->cur_byte = this->mem.buffers[this->mem.buffer_index];
                if (this->mem.buffer_index + 1 < this->mem.buffers.num_items()) {
                    this->end_byte = this->cur_byte + BUFFER_SIZE;
                } else {
                    this->end_byte = this->cur_byte + this->mem.num_bytes_in_last_buffer;
                }
            }
        } else if (this->num_remaining_bytes() < min_bytes) {
            if (this->mem.buffer_index + 1 < this->mem.buffers.num_items()) {
                u32 num_bytes_in_next_buffer = BUFFER_SIZE;
                if (this->mem.buffer_index + 2 == this->mem.buffers.num_items()) {
                    num_bytes_in_next_buffer = this->mem.num_bytes_in_last_buffer;
                }
                u32 num_bytes_to_expose = min(min_bytes, this->num_remaining_bytes() + num_bytes_in_next_buffer);
                if (!this->mem.temp_buffer) {
                    this->mem.temp_buffer = (char*) Heap::alloc(MAX_CONSECUTIVE_BYTES);
                }
                memcpy(this->mem.temp_buffer, this->cur_byte, this->num_remaining_bytes());
                memcpy(this->mem.temp_buffer + this->num_remaining_bytes(),
                       this->mem.buffers[this->mem.buffer_index + 1],
                       num_bytes_to_expose - this->num_remaining_bytes());
                this->mem.temp_buffer_offset =
                    numeric_cast<u32>(this->cur_byte - this->mem.buffers[this->mem.buffer_index]);
                this->using_temp_buffer = true;
                this->cur_byte = this->mem.temp_buffer;
                this->end_byte = this->mem.temp_buffer + num_bytes_to_expose;
            }
        }
        if (this->num_remaining_bytes() == 0) {
            this->at_eof = true;
        }
        return (this->num_remaining_bytes() >= min_bytes);
    } else if (this->type == Type::View) {
        this->mode = Mode::Reading;
        if (this->cur_byte >= this->end_byte) {
            this->at_eof = true;
        }
        return (this->num_remaining_bytes() >= min_bytes);
    }

    PLY_ASSERT(0); // Shouldn't get here.
    return false;
}

bool Stream::make_writable_internal(u32 min_bytes) {
    PLY_ASSERT(this->has_write_permission);
    PLY_ASSERT(min_bytes <= MAX_CONSECUTIVE_BYTES);
    if ((this->mode == Mode::Writing) && (this->num_remaining_bytes() >= min_bytes))
        return true;

    if (this->type == Type::Pipe) {
        if (this->mode == Mode::Writing) {
            // Write buffered data to the pipe.
            this->pipe.pipe->write({this->pipe.buffer, this->cur_byte});
            this->pipe.seek_pos_at_buffer += (this->cur_byte - this->pipe.buffer);
        }

        // Make entire buffer available for writing.
        this->cur_byte = this->pipe.buffer;
        this->end_byte = this->cur_byte + BUFFER_SIZE;
        this->at_eof = false;
    } else if (this->type == Type::Mem) {
        this->flush_mem_writes();

        if (this->num_remaining_bytes() == 0) {
            this->mem.buffer_index++;
            if (this->mem.buffer_index >= this->mem.buffers.num_items()) {
                this->mem.buffers.append((char*) Heap::alloc(BUFFER_SIZE));
                this->mem.num_bytes_in_last_buffer = 0;
            }
            this->cur_byte = this->mem.buffers[this->mem.buffer_index];
            this->end_byte = this->cur_byte + BUFFER_SIZE;
        } else if (this->num_remaining_bytes() < min_bytes) {
            if (!this->mem.temp_buffer) {
                this->mem.temp_buffer = (char*) Heap::alloc(MAX_CONSECUTIVE_BYTES);
            }
            this->mem.temp_buffer_offset =
                numeric_cast<u32>(this->cur_byte - this->mem.buffers[this->mem.buffer_index]);
            this->using_temp_buffer = true;
            this->cur_byte = this->mem.temp_buffer;
            this->end_byte = this->mem.temp_buffer + min_bytes;
        }
        this->at_eof = false;
    } else if (this->type == Type::View) {
        this->at_eof = true;
    } else {
        PLY_ASSERT(0); // Shouldn't get here.
    }

    this->mode = Mode::Writing;
    return !this->at_eof;
}

char Stream::read_byte_internal() {
    if (!this->make_readable())
        return 0;
    PLY_ASSERT(this->cur_byte < this->end_byte);
    return *this->cur_byte++;
}

u32 Stream::read_internal(MutStringView dst) {
    u32 num_bytes_read = 0;
    while (dst.num_bytes > 0) {
        if (!this->make_readable()) {
            memset(dst.bytes, 0, dst.num_bytes);
            break;
        }
        u32 to_copy = min(dst.num_bytes, this->num_remaining_bytes());
        memcpy(dst.bytes, this->cur_byte, to_copy);
        this->cur_byte += to_copy;
        dst = dst.subview(to_copy);
        num_bytes_read += to_copy;
    }
    return num_bytes_read;
}

u32 Stream::skip_internal(u32 num_bytes) {
    u32 num_bytes_skipped = 0;
    while (num_bytes > 0) {
        if (!this->make_readable())
            break;
        u32 to_skip = min(num_bytes, this->num_remaining_bytes());
        this->cur_byte += to_skip;
        num_bytes -= to_skip;
        num_bytes_skipped += to_skip;
    }
    return num_bytes_skipped;
}

void Stream::flush(bool to_device) {
    PLY_ASSERT(this->has_write_permission);
    if (this->mode != Mode::Writing)
        return;
    if (this->type == Type::Pipe) {
        PLY_ASSERT(this->pipe.pipe);
        PLY_ASSERT(this->pipe.buffer + BUFFER_SIZE == this->end_byte);

        // Write buffered data to the pipe.
        this->pipe.pipe->write({this->pipe.buffer, this->cur_byte});
        this->cur_byte = this->pipe.buffer;

        // Forward flush command down the output chain.
        this->pipe.pipe->flush(to_device);
    } else if (this->type == Type::Mem) {
        this->flush_mem_writes();
    }
}

u32 Stream::write(StringView src) {
    u32 total_copied = 0;
    while (src && this->make_writable()) {
        // Copy as much data as possible to the current block.
        u32 to_copy = min(this->num_remaining_bytes(), src.num_bytes);
        memcpy(this->cur_byte, src.bytes, to_copy);
        this->cur_byte += to_copy;
        src = src.substr(to_copy);
        total_copied += to_copy;
    }
    return total_copied;
}

u64 Stream::get_seek_pos() {
    if (this->type == Type::Pipe) {
        return this->pipe.seek_pos_at_buffer + (this->cur_byte - this->pipe.buffer);
    } else if (this->type == Type::Mem) {
        if (this->using_temp_buffer) {
            return (this->mem.buffer_index * BUFFER_SIZE) + this->mem.temp_buffer_offset +
                   (this->cur_byte - this->mem.temp_buffer);
        } else {
            char* buf = this->mem.buffers[this->mem.buffer_index];
            return (this->mem.buffer_index * BUFFER_SIZE) + (this->cur_byte - buf);
        }
    } else if (this->type == Type::View) {
        return (this->cur_byte - this->view.start_byte);
    }
    PLY_ASSERT(0); // Shouldn't get here.
    return 0;
}

void Stream::seek_to(u64 seek_pos) {
    if (this->type == Type::Pipe) {
        PLY_ASSERT((this->pipe.pipe->get_flags() & Pipe::CAN_SEEK) != 0);
        s64 relative_to_buffer = seek_pos - this->pipe.seek_pos_at_buffer;
        u32 num_bytes_in_buffer = numeric_cast<u32>(this->end_byte - this->pipe.buffer);
        if (relative_to_buffer >= 0 && relative_to_buffer <= num_bytes_in_buffer) {
            this->cur_byte = this->pipe.buffer + relative_to_buffer;
        } else {
            this->pipe.pipe->seek_to(seek_pos);
            this->cur_byte = this->pipe.buffer;
            this->end_byte = this->pipe.buffer;
        }
    } else if (this->type == Type::Mem) {
        this->flush_mem_writes();

        u32 buffer_index = numeric_cast<u32>(seek_pos / BUFFER_SIZE);
        PLY_ASSERT(buffer_index < this->mem.buffers.num_items());
        this->mem.buffer_index = buffer_index;
        char* buf = this->mem.buffers[buffer_index];
        u32 offset_in_buffer = numeric_cast<u32>(seek_pos - u64(buffer_index) * BUFFER_SIZE);
        u32 num_bytes_in_buffer = BUFFER_SIZE;
        if (buffer_index == this->mem.buffers.num_items() - 1) {
            num_bytes_in_buffer = this->mem.num_bytes_in_last_buffer;
            PLY_ASSERT(buffer_index < this->mem.buffers.num_items());
            PLY_ASSERT(offset_in_buffer <= num_bytes_in_buffer);
        }
        this->cur_byte = buf + offset_in_buffer;
        this->end_byte = buf + num_bytes_in_buffer;
    } else if (this->type == Type::View) {
        PLY_ASSERT(seek_pos <= this->end_byte - this->view.start_byte);
        this->cur_byte = this->view.start_byte + seek_pos;
    } else {
        PLY_ASSERT(0); // Shouldn't get here.
    }
    this->at_eof = false;
    this->input_error = false;
}

//--------------------------------------------
MemStream::MemStream() {
    this->type = Type::Mem;
    new (&this->mem) MemData;
    char* buf = (char*) Heap::alloc(BUFFER_SIZE);
    this->mem.buffers.append(buf);
    this->cur_byte = buf;
    this->end_byte = buf + BUFFER_SIZE;
    this->has_read_permission = true;
    this->has_write_permission = true;
}

String MemStream::move_to_string() {
    PLY_ASSERT(this->type == Type::Mem);

    if (this->mem.buffer_index + 1 == this->mem.buffers.num_items()) {
        // Extend number of bytes in the last buffer.
        this->mem.num_bytes_in_last_buffer =
            max(this->mem.num_bytes_in_last_buffer, numeric_cast<u32>(this->cur_byte - this->mem.buffers.back()));
    }

    if (this->mem.buffers.num_items() == 1) {
        u32 num_bytes = this->mem.num_bytes_in_last_buffer;
        char* bytes = (char*) Heap::realloc(this->mem.buffers[0], num_bytes);
        this->mem.~MemData();
        new (this) Stream;
        return String::adopt(bytes, num_bytes);
    }

    u32 num_bytes = (this->mem.buffers.num_items() - 1) * BUFFER_SIZE + this->mem.num_bytes_in_last_buffer;
    char* bytes = (char*) Heap::alloc(num_bytes);
    for (u32 i = 0; i < this->mem.buffers.num_items(); i++) {
        u32 to_copy = min(BUFFER_SIZE, num_bytes - (BUFFER_SIZE * i));
        memcpy(bytes + (BUFFER_SIZE * i), this->mem.buffers[i], to_copy);
    }
    this->close();
    return String::adopt(bytes, num_bytes);
}

//--------------------------------------------
ViewStream::ViewStream(StringView view) {
    this->type = Type::View;
    new (&this->view) ViewData;
    this->view.start_byte = const_cast<char*>(view.bytes);
    this->cur_byte = this->view.start_byte;
    this->end_byte = this->cur_byte + view.num_bytes;
    this->has_read_permission = true;
}

ViewStream::ViewStream(MutStringView view) {
    this->type = Type::View;
    new (&this->view) ViewData;
    this->view.start_byte = view.bytes;
    this->cur_byte = this->view.start_byte;
    this->end_byte = this->cur_byte + view.num_bytes;
    this->has_read_permission = true;
    this->has_write_permission = true;
}

//  ▄▄▄▄▄                    ▄▄ ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                         ▄▄▄█▀

String read_line(Stream& in) {
    MemStream mem;
    while (in.make_readable() && mem.make_writable()) {
        u32 num_bytes_remaining = min(in.num_remaining_bytes(), mem.num_remaining_bytes());
        for (u32 i = 0; i < num_bytes_remaining; i++) {
            char c = *in.cur_byte++;
            *mem.cur_byte++ = c;
            if (c == '\n')
                goto done;
        }
    }
done:
    return mem.move_to_string();
}

StringView read_line(ViewStream& view_in) {
    char* start_byte = view_in.cur_byte;
    while (view_in.cur_byte < view_in.end_byte) {
        char c = *view_in.cur_byte++;
        if (c == '\n')
            break;
    }
    return {start_byte, view_in.cur_byte};
}

String read_whitespace(Stream& in) {
    MemStream mem;
    while (in.make_readable() && mem.make_writable()) {
        u32 num_bytes_remaining = min(in.num_remaining_bytes(), mem.num_remaining_bytes());
        for (u32 i = 0; i < num_bytes_remaining; i++) {
            char c = *in.cur_byte;
            if (!is_whitespace(c))
                goto done;
            in.cur_byte++;
            *mem.cur_byte++ = c;
        }
    }
done:
    return mem.move_to_string();
}

StringView read_whitespace(ViewStream& view_in) {
    char* start_byte = view_in.cur_byte;
    while (view_in.cur_byte < view_in.end_byte) {
        char c = *view_in.cur_byte;
        if (!is_whitespace(c))
            break;
        view_in.cur_byte++;
    }
    return {start_byte, view_in.cur_byte};
}

void skip_whitespace(Stream& in) {
    while (in.make_readable()) {
        u32 num_bytes_remaining = in.num_remaining_bytes();
        for (u32 i = 0; i < num_bytes_remaining; i++) {
            char c = *in.cur_byte;
            if (!is_whitespace(c))
                return;
            in.cur_byte++;
        }
    }
}

String read_identifier(Stream& in, u32 flags) {
    bool first = true;
    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    if ((flags & ID_WITH_DOLLAR_SIGN) != 0) {
        mask[1] |= 0x10; // '$'
    }
    if ((flags & ID_WITH_DASH) != 0) {
        mask[1] |= 0x2000; // '-'
    }

    MemStream mem;
    while (in.make_readable() && mem.make_writable()) {
        u32 num_bytes_remaining = min(in.num_remaining_bytes(), mem.num_remaining_bytes());
        for (u32 i = 0; i < num_bytes_remaining; i++) {
            char c = *in.cur_byte;
            if ((mask[c >> 5] & (1 << (c & 31))) == 0)
                goto done;
            in.cur_byte++;
            *mem.cur_byte++ = c;
            if (first) {
                mask[1] |= 0x3ff0000; // accept digits after first unit
                first = false;
            };
        }
    }
done:
    return mem.move_to_string();
}

StringView read_identifier(ViewStream& view_in, u32 flags) {
    bool first = true;
    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    if ((flags & ID_WITH_DOLLAR_SIGN) != 0) {
        mask[1] |= 0x10; // '$'
    }
    if ((flags & ID_WITH_DASH) != 0) {
        mask[1] |= 0x2000; // '-'
    }

    char* start_byte = view_in.cur_byte;
    while (view_in.cur_byte < view_in.end_byte) {
        char c = *view_in.cur_byte;
        if ((mask[c >> 5] & (1 << (c & 31))) == 0)
            goto done;
        view_in.cur_byte++;
        if (first) {
            mask[1] |= 0x3ff0000; // accept digits after first unit
            first = false;
        };
    }
done:
    return {start_byte, view_in.cur_byte};
}

u8 digit_from_char(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    char lower = (c | 32);
    if ((lower >= 'a') && (lower <= 'z'))
        return lower - 'a' + 10;
    return 255;
}

u64 read_u64_from_text(Stream& in, u32 radix) {
    PLY_ASSERT(radix > 0 && radix <= 36);
    u64 result = 0;
    bool any_digits = false;
    bool overflow = false;
    for (;;) {
        if (!in.make_readable())
            break;
        u8 digit = digit_from_char(*in.cur_byte);
        if (digit >= radix)
            break;
        in.cur_byte++;
        // FIXME: When available, check for (multiplicative & additive) overflow using
        // https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html#Integer-Overflow-Builtins
        // and equivalent intrinsics instead of the following.
        // Note: 0x71c71c71c71c71b is the largest value that won't overflow for any
        // radix <= 36. We test against this constant first to avoid the costly integer
        // division.
        if (result > 0x71c71c71c71c71b && result > (get_max_value<u64>() - digit) / radix) {
            overflow = true;
        }
        result = result * radix + digit;
        any_digits = true;
    }
    if (!any_digits || overflow) {
        in.input_error = true;
        return 0;
    }
    return result;
}

s64 read_s64_from_text(Stream& in, u32 radix) {
    bool negate = false;

    if (in.make_readable() && (*in.cur_byte == '-')) {
        negate = true;
        in.cur_byte++;
    }

    u64 unsigned_component = read_u64_from_text(in, radix);
    if (negate) {
        s64 result = -(s64) unsigned_component;
        if (result > 0) {
            in.input_error = true;
        }
        return result;
    } else {
        s64 result = unsigned_component;
        if (result < 0) {
            in.input_error = true;
        }
        return result;
    }
}

struct DoubleComponentOut {
    double result = 0;
    bool any_digits = false;
};

void read_double_component(DoubleComponentOut* comp_out, Stream& in, u32 radix) {
    double value = 0.0;
    double dr = (double) radix;
    for (;;) {
        if (!in.make_readable())
            break;
        u8 digit = digit_from_char(*in.cur_byte);
        if (digit >= radix)
            break;
        in.cur_byte++;
        value = value * dr + digit;
        comp_out->any_digits = true;
    }
    comp_out->result = value;
}

double read_double_from_text(Stream& in, u32 radix) {
    PLY_ASSERT(radix <= 36);
    DoubleComponentOut comp;

    // Parse the optional minus sign
    bool negate = false;
    if (in.make_readable() && (*in.cur_byte == '-')) {
        in.cur_byte++;
        negate = true;
    }

    // Parse the mantissa
    read_double_component(&comp, in, radix);
    double value = comp.result;

    // Parse the optional fractional part
    if (in.make_readable() && (*in.cur_byte == '.')) {
        in.cur_byte++;
        double significance = 1.0;
        u64 numer = 0;
        u64 denom = 1;
        for (;;) {
            if (!in.make_readable())
                break;
            u8 digit = digit_from_char(*in.cur_byte);
            if (digit >= radix)
                break;
            in.cur_byte++;
            u64 denom_with_next_digit = denom * radix;
            if (denom_with_next_digit < denom) {
                // denominator overflowed
                double oo_denom = 1.0 / denom;
                value += significance * numer * oo_denom;
                significance *= oo_denom;
                numer = digit;
                denom = radix;
            } else {
                numer = numer * radix + digit;
                denom = denom_with_next_digit;
            }
        }
        value += significance * numer / denom;
    }

    // Parse optional exponent suffix
    if (comp.any_digits && in.make_readable() && ((*in.cur_byte | 0x20) == 'e')) {
        in.cur_byte++;
        bool negate_exp = false;
        if (in.make_readable()) {
            if (*in.cur_byte == '+') {
                in.cur_byte++;
            } else if (*in.cur_byte == '-') {
                in.cur_byte++;
                negate_exp = true;
            }
        }
        comp.any_digits = false;
        read_double_component(&comp, in, radix);
        value *= pow((double) radix, negate_exp ? -comp.result : comp.result);
    }

    if (!comp.any_digits) {
        in.input_error = true;
    }

    return negate ? -value : value;
}

String read_quoted_string(Stream& in, u32 flags, Functor<void(QS_Error_Code)> error_callback) {
    auto handle_error = [&](QS_Error_Code error_code) {
        in.input_error = true;
        if (error_callback) {
            error_callback(error_code);
        }
    };

    // Get opening quote
    if (!in.make_readable()) {
        handle_error(QS_UNEXPECTED_END_OF_FILE);
        return {};
    }
    u8 quote_type = *in.cur_byte;
    if (!(quote_type == '"' || ((flags & QS_ALLOW_SINGLE_QUOTE) && quote_type == '\''))) {
        handle_error(QS_NO_OPENING_QUOTE);
        return {};
    }
    in.cur_byte++;

    // Parse rest of quoted string
    MemStream out;
    u32 quote_run = 1;
    bool multiline = false;
    for (;;) {
        if (!in.make_readable()) {
            handle_error(QS_UNEXPECTED_END_OF_FILE);
            break; // end of string
        }

        u8 next_byte = *in.cur_byte;
        if (next_byte == quote_type) {
            in.cur_byte++;
            if (quote_run == 0) {
                if (multiline) {
                    quote_run++;
                } else {
                    break; // end of string
                }
            } else {
                quote_run++;
                if (quote_run == 3) {
                    if (multiline) {
                        break; // end of string
                    } else {
                        multiline = true;
                        quote_run = 0;
                    }
                }
            }
        } else {
            // FIXME: Check fmt::AllowMultilineWithTriple (and other flags)
            if (quote_run > 0) {
                if (multiline) {
                    for (u32 i = 0; i < quote_run; i++) {
                        out.write(quote_type);
                    }
                } else if (quote_run == 2) {
                    break; // empty string
                }
                quote_run = 0;
            }

            switch (next_byte) {
                case '\r':
                case '\n': {
                    if (multiline) {
                        if (next_byte == '\n') {
                            out.write(next_byte);
                        }
                        in.cur_byte++;
                    } else {
                        handle_error(QS_UNEXPECTED_END_OF_LINE);
                        goto end_of_string;
                    }
                    break;
                }

                case '\\': {
                    // Escape sequence
                    in.cur_byte++;
                    if (!in.make_readable()) {
                        handle_error(QS_UNEXPECTED_END_OF_FILE);
                        goto end_of_string;
                    }
                    u8 code = *in.cur_byte;
                    switch (code) {
                        case '\r':
                        case '\n': {
                            handle_error(QS_UNEXPECTED_END_OF_LINE);
                            goto end_of_string;
                        }

                        case '\\':
                        case '\'':
                        case '"': {
                            out.write(code);
                            break;
                        }

                        case 'r': {
                            out.write('\r');
                            break;
                        }

                        case 'n': {
                            out.write('\n');
                            break;
                        }

                        case 't': {
                            out.write('\t');
                            break;
                        }

                            // FIXME: Implement escape hex codes
                            // case 'x':

                        default: {
                            handle_error(QS_BAD_ESCAPE_SEQUENCE);
                            break;
                        }
                    }
                    in.cur_byte++;
                    break;
                }

                default: {
                    out.write(next_byte);
                    in.cur_byte++;
                    break;
                }
            }
        }
    }

end_of_string:
    return out.move_to_string();
}

//  ▄▄    ▄▄        ▄▄  ▄▄   ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//   ██▀▀██  ██     ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                      ▄▄▄█▀

inline char to_digit(u32 d, bool capitalize = false) {
    const char* digit_table =
        capitalize ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    return (d <= 35) ? digit_table[d] : '?';
}

void print_number(Stream& outs, u64 value, u32 radix, bool capitalize) {
    PLY_ASSERT(radix >= 2);
    char digit_buffer[64];
    s32 digit_index = PLY_STATIC_ARRAY_SIZE(digit_buffer);

    if (value == 0) {
        digit_buffer[--digit_index] = '0';
    } else {
        while (value > 0) {
            u64 quotient = value / radix;
            u32 digit = u32(value - quotient * radix);
            PLY_ASSERT(digit_index > 0);
            digit_buffer[--digit_index] = to_digit(digit, capitalize);
            value = quotient;
        }
    }

    outs.write(StringView{digit_buffer + digit_index, (u32) PLY_STATIC_ARRAY_SIZE(digit_buffer) - digit_index});
}

void print_number(Stream& outs, s64 value, u32 radix, bool capitalize) {
    if (value >= 0) {
        print_number(outs, (u64) value, radix, capitalize);
    } else {
        outs.write('-');
        print_number(outs, (u64) -value, radix, capitalize);
    }
}

void print_number(Stream& outs, double value, u32 radix, bool capitalize) {
    PLY_ASSERT(radix >= 2);

#if PLY_COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    if (*(s64*) &value < 0) {
#if PLY_COMPILER_GCC
#pragma GCC diagnostic pop
#endif
        value = -value;
        outs.write('-');
    }
    if (isnan(value)) {
        outs.write("nan");
    } else if (isinf(value)) {
        outs.write("inf");
    } else {
        u32 radix3 = radix * radix * radix;
        u32 radix6 = radix3 * radix3;
        if (value == 0.0 || (value * radix3 > radix && value < radix6)) {
            u64 fixed_point = u64(value * radix3);
            print_number(outs, fixed_point / radix3, radix, capitalize);
            outs.write('.');
            u64 fractional_part = fixed_point % radix3;
            {
                // Print zeroed
                char digit_buffer[3];
                for (s32 i = 2; i >= 0; i--) {
                    u64 quotient = fractional_part / radix;
                    u32 digit = u32(fractional_part - quotient * radix);
                    digit_buffer[i] = to_digit(digit, capitalize);
                    fractional_part = quotient;
                }
                outs.write(StringView{digit_buffer, PLY_STATIC_ARRAY_SIZE(digit_buffer)});
            }
        } else {
            // Scientific notation
            double log_base = log(value) / log(radix);
            double exponent = floor(log_base);
            double m = value / pow(radix, exponent); // mantissa (initially)
            s32 digit = clamp((s32) floor(m), 1, (s32) radix - 1);
            outs.write(to_digit(digit, capitalize));
            outs.write('.');
            for (u32 i = 0; i < 3; i++) {
                m = (m - digit) * radix;
                digit = clamp((s32) floor(m), 0, (s32) radix - 1);
                outs.write(to_digit(digit, capitalize));
            }
            outs.write('e');
            print_number(outs, (s64) exponent, radix, capitalize);
        }
    }
}

void print_escaped_string(Stream& out, StringView str) {
    ViewStream vin{str};
    while (vin.num_remaining_bytes() > 0) {
        const char* start = vin.cur_byte;
        DecodeResult decoded = decode_unicode(vin, UTF8);
        switch (decoded.point) {
            case '"': {
                out.write("\\\"");
                break;
            }
            case '\\': {
                out.write("\\\\");
                break;
            }
            case '\r': {
                out.write("\\r");
                break;
            }
            case '\n': {
                out.write("\\n");
                break;
            }
            case '\t': {
                out.write("\\t");
                break;
            }
            default: {
                if (decoded.point >= 32) {
                    // This will preserve badly encoded UTF8 characters exactly as they are in
                    // the source string:
                    out.write(StringView{start, vin.cur_byte});
                } else {
                    static const char* digits = "0123456789abcdef";
                    out.format("\\{}{}", digits[(decoded.point >> 4) & 0xf], digits[decoded.point & 0xf]);
                }
                break;
            }
        }
    }
}

void print_xml_escaped_string(Stream& out, StringView str) {
    ViewStream vin{str};
    while (vin.num_remaining_bytes() > 0) {
        const char* start = vin.cur_byte;
        DecodeResult decoded = decode_unicode(vin, UTF8);
        switch (decoded.point) {
            case '<': {
                out.write("&lt;");
                break;
            }
            case '>': {
                out.write("&gt;");
                break;
            }
            case '"': {
                out.write("&quot;");
                break;
            }
            case '&': {
                out.write("&amp;");
                break;
            }
            default: {
                // This will preserve badly encoded UTF8 characters exactly as they are in
                // the source string:
                out.write(StringView{start, vin.cur_byte});
                break;
            }
        }
    }
}

void print_arg(Stream& out, StringView fmt_spec, const FormatArg& arg) {
    bool xml_escape = false;
    u32 pos = 0;
    while (pos < fmt_spec.num_bytes) {
        char c = fmt_spec[pos++];
        if (c == '&') {
            PLY_ASSERT(arg.var.is<StringView>()); // Argument must be a StringView.
            xml_escape = true;
        } else {
            PLY_ASSERT(0); // Invalid format specifier.
        }
    }
    if (arg.var.is<StringView>()) {
        if (xml_escape) {
            print_xml_escaped_string(out, *arg.var.as<StringView>());
        } else {
            out.write(*arg.var.as<StringView>());
        }
    } else if (arg.var.is<bool>()) {
        out.write(*arg.var.as<bool>() ? "true" : "false");
    } else if (arg.var.is<s64>()) {
        print_number(out, *arg.var.as<s64>());
    } else if (arg.var.is<u64>()) {
        print_number(out, *arg.var.as<u64>());
    } else if (arg.var.is<double>()) {
        print_number(out, *arg.var.as<double>());
    } else {
        PLY_ASSERT(0); // Invalid argument type.
    }
}

void format_with_args(Stream& out, StringView fmt, ArrayView<const FormatArg> args) {
    u32 arg_index = 0;
    u32 pos = 0;
    while (pos < fmt.num_bytes) {
        char c = fmt[pos++];
        if (c == '{') {
            u32 spec_start = pos;
            PLY_ASSERT(pos < fmt.num_bytes); // Missing '}' after '{'.
            if (fmt[pos] == '{') {
                pos++;
                out.write('{');
            } else {
                do {
                    PLY_ASSERT(pos < fmt.num_bytes); // Missing '}' after '{'.
                } while (fmt[pos++] != '}');
                PLY_ASSERT(arg_index < args.num_items()); // Not enough arguments for format string.
                print_arg(out, fmt.substr(spec_start, pos - 1 - spec_start), args[arg_index]);
                arg_index++;
            }
        } else if (c == '}') {
            PLY_ASSERT((pos < fmt.num_bytes) && (fmt[pos] == '}')); // '}' must be followed by another '}'.
            pos++;
            out.write('}');
        } else {
            out.write(c);
        }
    }
    PLY_ASSERT(arg_index == args.num_items()); // Too many arguments for format string.
}

//   ▄▄▄▄   ▄▄                     ▄▄                   ▄▄     ▄▄▄▄     ▄▄  ▄▄▄▄
//  ██  ▀▀ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██      ██     ▄█▀ ██  ██
//   ▀▀▀█▄  ██    ▄▄▄██ ██  ██ ██  ██  ▄▄▄██ ██  ▀▀ ██  ██      ██   ▄█▀   ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄██     ▄██▄ ██     ▀█▄▄█▀
//

#if defined(_WIN32)

Pipe* get_stdin_pipe() {
    static PipeHandle in_pipe{GetStdHandle(STD_INPUT_HANDLE), Pipe::HAS_READ_PERMISSION};
    return &in_pipe;
}

Pipe* get_stdout_pipe() {
    static PipeHandle out_pipe{GetStdHandle(STD_OUTPUT_HANDLE), Pipe::HAS_WRITE_PERMISSION};
    return &out_pipe;
}

Pipe* get_stderr_pipe() {
    static PipeHandle error_pipe{GetStdHandle(STD_ERROR_HANDLE), Pipe::HAS_WRITE_PERMISSION};
    return &error_pipe;
}

#elif defined(PLY_POSIX)

Pipe* get_stdin_pipe() {
    static Pipe_FD in_pipe{STDIN_FILENO, Pipe::HAS_READ_PERMISSION};
    return &in_pipe;
}

Pipe* get_stdout_pipe() {
    static Pipe_FD out_pipe{STDOUT_FILENO, Pipe::HAS_WRITE_PERMISSION};
    return &out_pipe;
}

Pipe* get_stderr_pipe() {
    static Pipe_FD error_pipe{STDERR_FILENO, Pipe::HAS_WRITE_PERMISSION};
    return &error_pipe;
}

#endif

Stream get_stdin(ConsoleMode mode) {
    if (mode == ConsoleMode::TEXT) {
        Stream in{get_stdin_pipe(), false};
        // Always create a filter to make newlines consistent.
        return {Heap::create<InPipeNewLineFilter>(std::move(in)), true};
    } else {
        return {get_stdin_pipe(), false};
    }
}

Stream get_stdout(ConsoleMode mode) {
    Stream out{get_stdout_pipe(), false};
    bool write_crlf = false;
#if defined(_WIN32)
    write_crlf = true;
#endif
    // Always create a filter to make newlines consistent.
    return {Heap::create<OutPipeNewLineFilter>(std::move(out), write_crlf), true};
}

Stream get_stderr(ConsoleMode mode) {
    Stream out{get_stderr_pipe(), false};
    bool write_crlf = false;
#if defined(_WIN32)
    write_crlf = true;
#endif
    // Always create a filter to make newlines consistent.
    return {Heap::create<OutPipeNewLineFilter>(std::move(out), write_crlf), true};
}

//  ▄▄                         ▄▄
//  ██     ▄▄▄▄   ▄▄▄▄▄  ▄▄▄▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██    ██  ██ ██  ██ ██  ██ ██ ██  ██ ██  ██
//  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██
//                ▄▄▄█▀  ▄▄▄█▀            ▄▄▄█▀

void log_message_internal(StringView fmt, ArrayView<const FormatArg> args) {
    Stream out = get_stderr();
    format_with_args(out, fmt, args);
    if (!fmt.ends_with('\n')) {
        out.write('\n');
    }
}

//   ▄▄▄▄                                       ▄▄       ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄     ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██     ██  ██ ██  ██ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀  ██       ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██  ██   ▀█▀   ▀█▄▄▄  ██      ▀█▄▄     ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

u32 encode_unicode(FixedArray<char, 4>& buf, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params) {
    if (unicode_type == NOT_UNICODE) {
        s32 c;
        if (ext_params) {
            // Use lookup table.
            if (u8* value = ext_params->reverse_lut.find(codepoint)) {
                c = *value;
            } else {
                c = ext_params->missing_char;
            }
        } else {
            // Encode this codepoint directly as a byte.
            c = max((s32) codepoint, 255);
        }
        if (c < 0)
            return 0; // Optionally skip unrepresentable character.
        buf[0] = (char) c;
        return 1;

    } else if (unicode_type == UTF8) {
        if (codepoint < 0x80) {
            // 1-byte encoding: 0xxxxxxx
            buf[0] = char(codepoint);
            return 1;
        } else if (codepoint < 0x800) {
            // 2-byte encoding: 110xxxxx 10xxxxxx
            buf[0] = char(0xc0 | (codepoint >> 6));
            buf[1] = char(0x80 | (codepoint & 0x3f));
            return 2;
        } else if (codepoint < 0x10000) {
            // 3-byte encoding: 1110xxxx 10xxxxxx 10xxxxxx
            buf[0] = char(0xe0 | (codepoint >> 12));
            buf[1] = char(0x80 | ((codepoint >> 6) & 0x3f));
            buf[2] = char(0x80 | ((codepoint & 0x3f)));
            return 3;
        } else {
            // 4-byte encoding: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            buf[0] = char(0xf0 | (codepoint >> 18));
            buf[1] = char(0x80 | ((codepoint >> 12) & 0x3f));
            buf[2] = char(0x80 | ((codepoint >> 6) & 0x3f));
            buf[3] = char(0x80 | (codepoint & 0x3f));
            return 4;
        }
#if PLY_IS_BIG_ENDIAN
    } else if (unicode_type == UTF16_BE) {
#else
    } else if (unicode_type == UTF16_LE) {
#endif
        if (codepoint < 0x10000) {
            // Note: 0xd800 to 0xd8ff are invalid Unicode codepoints reserved for UTF-16
            // surrogates. Such codepoints will simply be written as unpaired
            // surrogates.
            *(u16*) &buf[0] = (u16) codepoint;
            return 2;
        } else {
            // Codepoints >= 0x10000 are encoded as a pair of surrogate units.
            u32 adjusted = codepoint - 0x10000;
            *(u16*) &buf[0] = u16(0xd800 + ((adjusted >> 10) & 0x3ff));
            *(u16*) &buf[2] = u16(0xdc00 + (adjusted & 0x3ff));
            return 4;
        }
#if PLY_IS_BIG_ENDIAN
    } else if (unicode_type == UTF16_LE) {
#else
    } else if (unicode_type == UTF16_BE) {
#endif
        if (codepoint < 0x10000) {
            // Note: 0xd800 to 0xd8ff are invalid Unicode codepoints reserved for UTF-16
            // surrogates. Such codepoints will simply be written as unpaired
            // surrogates.
            *(u16*) &buf[0] = reverse_bytes((u16) codepoint);
            return 2;
        } else {
            // Codepoints >= 0x10000 are encoded as a pair of surrogate units.
            u32 adjusted = codepoint - 0x10000;
            *(u16*) &buf[0] = reverse_bytes(u16(0xd800 + ((adjusted >> 10) & 0x3ff)));
            *(u16*) &buf[2] = reverse_bytes(u16(0xdc00 + (adjusted & 0x3ff)));
            return 4;
        }
    } else {
        // Shouldn't get here.
        PLY_ASSERT(0);
    }

    return false;
}

bool encode_unicode(Stream& out, UnicodeType unicode_type, u32 codepoint, ExtendedTextParams* ext_params) {
    out.make_writable();
    if (out.num_remaining_bytes() >= 4) {
        // Encode directly into the output buffer.
        u32 num_bytes = encode_unicode(*(FixedArray<char, 4>*) out.cur_byte, unicode_type, codepoint, ext_params);
        out.cur_byte += num_bytes;
        return true;
    } else {
        // Encode into a temporary buffer.
        FixedArray<char, 4> buf;
        u32 num_bytes = encode_unicode(buf, unicode_type, codepoint, ext_params);
        // Write the encoded bytes to the output stream.
        out.write({&buf[0], num_bytes});
        return !out.at_eof;
    }
}

DecodeResult decode_unicode(StringView str, UnicodeType unicode_type, ExtendedTextParams* ext_params) {
    if (str.is_empty())
        return {-1, 0, DS_NOT_ENOUGH_DATA};

    if (unicode_type == NOT_UNICODE) {
        u8 b = (u8) str.bytes[0];
        if (ext_params) // Use lookup table if available.
            return {ext_params->lut[b], 1, DS_OK};
        return {b, 1, DS_OK};
    } else if (unicode_type == UTF8) {
        // (Note: Ill-formed encodings are interpreted as sequences of individual bytes.)
        s32 value = 0;
        u32 num_continuation_bytes = 0;
        u8 b = (u8) str.bytes[0];

        if (b < 0x80) {
            // 1-byte encoding: 0xxxxxxx
            return {b, 1, DS_OK};
        } else if (b < 0xc0) {
            // Unexpected continuation byte: 10xxxxxx
            return {b, 1, DS_ILL_FORMED};
        } else if (b < 0xe0) {
            // 2-byte encoding: 110xxxxx 10xxxxxx
            value = b & 0x1f;
            num_continuation_bytes = 1;
        } else if (b < 0xf0) {
            // 3-byte encoding: 1110xxxx 10xxxxxx 10xxxxxx
            value = b & 0xf;
            num_continuation_bytes = 2;
        } else if (b < 0xf8) {
            // 4-byte encoding: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            value = b & 0x7;
            num_continuation_bytes = 3;
        } else {
            // Illegal byte.
            return {b, 1, DS_ILL_FORMED};
        }

        if (str.num_bytes < num_continuation_bytes + 1) {
            // Not enough bytes in buffer for continuation bytes.
            return {b, 1, DS_NOT_ENOUGH_DATA};
        }

        for (u32 i = 0; i < num_continuation_bytes; i++) {
            u8 c = (u8) str.bytes[i + 1];
            if ((c >> 6) != 2) // Must be a continuation byte
                return {b, 1, DS_ILL_FORMED};
            value = (value << 6) | ((u8) str.bytes[i + 1] & 0x3f);
        }

        return {value, num_continuation_bytes + 1, DS_OK};
#if PLY_IS_BIG_ENDIAN
    } else if (unicode_type == UTF16_BE) {
#else
    } else if (unicode_type == UTF16_LE) {
#endif
        if (str.num_bytes < 2) {
            return {-1, 0, DS_NOT_ENOUGH_DATA};
        }

        u16 first = *(const u16*) &str.bytes[0];

        if (first >= 0xd800 && first < 0xdc00) {
            if (str.num_bytes < 4) {
                // A second 16-bit surrogate is expected, but not enough data.
                return {first, 2, DS_NOT_ENOUGH_DATA};
            }
            u16 second = *(const u16*) &str.bytes[2];
            if (second >= 0xdc00 && second < 0xe000) {
                // We got a valid pair of 16-bit surrogates.
                return {0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00), 4, DS_OK};
            }

            // Unpaired surrogate.
            return {first, 2, DS_ILL_FORMED};
        }

        // It's a single 16-bit unit.
        return {first, 2, DS_OK};
#if PLY_IS_BIG_ENDIAN
    } else if (unicode_type == UTF16_LE) {
#else
    } else if (unicode_type == UTF16_BE) {
#endif
        if (str.num_bytes < 2) {
            return {-1, 0, DS_NOT_ENOUGH_DATA};
        }

        u16 first = reverse_bytes(*(const u16*) &str.bytes[0]);

        if (first >= 0xd800 && first < 0xdc00) {
            if (str.num_bytes < 4) {
                // A second 16-bit surrogate is expected, but not enough data.
                return {first, 2, DS_NOT_ENOUGH_DATA};
            }
            u16 second = reverse_bytes(*(const u16*) &str.bytes[2]);
            if (second >= 0xdc00 && second < 0xe000) {
                // We got a valid pair of 16-bit surrogates.
                return {0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00), 4, DS_OK};
            }

            // Unpaired surrogate.
            return {first, 2, DS_ILL_FORMED};
        }

        // It's a single 16-bit unit.
        return {first, 2, DS_OK};
    }

    PLY_ASSERT(0); // Shouldn't get here.
    return {-1, 0, DS_NOT_ENOUGH_DATA};
}

DecodeResult decode_unicode(Stream& in, UnicodeType unicode_type, ExtendedTextParams* ext_params) {
    // Try to get at least four bytes to read.
    in.make_readable(4);
    if (in.num_remaining_bytes() == 0)
        return {-1, 0, DS_NOT_ENOUGH_DATA};

    DecodeResult result = decode_unicode(in.view_remaining_bytes(), unicode_type, ext_params);
    in.cur_byte += result.num_bytes;
    return result;
}

//--------------------------------------------------------------

bool copy_from_shim(Stream& dst_out, StringView& shim_used) {
    if (shim_used) {
        u32 to_copy = min(dst_out.num_remaining_bytes(), shim_used.num_bytes);
        dst_out.write(shim_used);
        shim_used = shim_used.substr(to_copy);
        if (shim_used)
            return true; // Destination buffer is full.
    }
    shim_used = {};
    return false;
}

// Fill dst_buf with UTF-8-encoded data.
u32 InPipeConvertUnicode::read(MutStringView dst_buf) {
    ViewStream dst_out{dst_buf};

    // If the shim contains data, copy it first.
    if (copy_from_shim(dst_out, this->shim_used))
        return dst_buf.num_bytes; // Destination buffer is full.

    while (true) {
        // Decode a codepoint from input stream.
        s32 codepoint = decode_unicode(this->in, this->src_type).point;
        if (codepoint < 0)
            break; // Reached EOF.

        // Convert codepoint to UTF-8.
        u32 w = dst_out.num_remaining_bytes();
        if (w >= 4) {
            encode_unicode(dst_out, UTF8, codepoint);
        } else {
            // Use shim as an intermediate buffer.
            ViewStream s{this->shim_storage.mut_string_view()};
            encode_unicode(s, UTF8, codepoint);
            this->shim_used =
                StringView{this->shim_storage.items(), numeric_cast<u32>(s.cur_byte - this->shim_storage.items())};
            if (copy_from_shim(dst_out, this->shim_used))
                break; // Destination buffer is full.
        }
    }

    return numeric_cast<u32>(dst_out.cur_byte - dst_buf.bytes);
}

// src_buf expects UTF-8-encoded data.
bool OutPipeConvertUnicode::write(StringView src_buf) {
    ViewStream src_in{src_buf};

    // If the shim contains data, join it with the source buffer.
    if (this->shim_used > 0) {
        u32 num_bytes_appended = min(src_buf.num_bytes, 4 - this->shim_used);
        memcpy(this->shim_storage + this->shim_used, src_buf.bytes, num_bytes_appended);
        this->shim_used += num_bytes_appended;

        // Decode a codepoint from the shim using UTF-8.
        ViewStream s{StringView{this->shim_storage, this->shim_used}};
        DecodeResult decoded = decode_unicode(s, UTF8, nullptr);
        if (decoded.status == DS_NOT_ENOUGH_DATA) {
            PLY_ASSERT(num_bytes_appended == src_buf.num_bytes);
            return true; // Not enough data available in shim.
        }

        // Convert codepoint to the destination encoding.
        encode_unicode(this->child_out, this->dst_type, decoded.point, this->ext_params);

        // Skip ahead in the source buffer and clear the shim.
        src_in.cur_byte += num_bytes_appended;
        this->shim_used = 0;
    }

    while (!this->child_out.at_eof) {
        // Decode a codepoint from the source buffer using UTF-8.
        DecodeResult decoded = decode_unicode(src_in, UTF8, nullptr);
        if (decoded.status == DS_NOT_ENOUGH_DATA) {
            // Not enough data available. Copy the rest of the source buffer to shim,
            // including the previous byte consumed by decode().
            this->shim_used = src_in.num_remaining_bytes() + 1;
            PLY_ASSERT(this->shim_used < 4);
            memcpy(this->shim_storage, src_in.cur_byte - 1, this->shim_used);
            return true;
        }

        // Convert codepoint to the destination encoding.
        encode_unicode(this->child_out, this->dst_type, decoded.point, this->ext_params);
    }

    return false; // We reached the end of the Stream.
}

void OutPipeConvertUnicode::flush(bool to_device) {
    // The shim may still contain an incomplete (thus invalid) UTF-8 sequence.
    for (u32 i = 0; i < this->shim_used; i++) {
        // Interpret each byte as a separate codepoint.
        encode_unicode(this->child_out, this->dst_type, (u8) this->shim_storage[i], this->ext_params);
    }
    this->shim_used = 0;

    // Forward flush command down the output chain.
    this->child_out.flush(to_device);
}

//  ▄▄▄▄▄▄                ▄▄         ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄       ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██         ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ▄▄▄▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

TextFormat get_default_utf8_format() {
    TextFormat tff;
#if defined(_WIN32)
    tff.new_line = TextFormat::NewLine::CRLF;
#endif
    return tff;
}

struct TextFileStats {
    u32 num_points = 0;
    u32 num_valid_points = 0;
    u32 total_point_value = 0; // This value won't be accurate if byte encoding is detected
    u32 num_lines = 0;
    u32 num_crlf = 0;
    u32 num_control = 0; // non-whitespace points < 32, including nulls
    u32 num_null = 0;
    u32 num_plain_ascii = 0; // includes whitespace, excludes control characters < 32
    u32 num_whitespace = 0;
    u32 num_extended = 0;
    float oo_num_points = 0.f;

    u32 num_invalid_points() const {
        return this->num_points - this->num_valid_points;
    }
    TextFormat::NewLine get_new_line_type() const {
        PLY_ASSERT(this->num_crlf <= this->num_lines);
        if (this->num_crlf == 0 || this->num_crlf * 2 < this->num_lines) {
            return TextFormat::NewLine::LF;
        } else {
            return TextFormat::NewLine::CRLF;
        }
    }
    float get_score() const {
        return (2.5f * this->num_whitespace + this->num_plain_ascii - 100.f * this->num_invalid_points() -
                50.f * this->num_control + 5.f * this->num_extended) *
               this->oo_num_points;
    }
};

u32 scan_text_file(TextFileStats* stats, Stream& in, UnicodeType unicode_type, u32 max_bytes) {
    bool prev_was_cr = false;
    while (in.get_seek_pos() < max_bytes) {
        DecodeResult decoded = decode_unicode(in, unicode_type, nullptr);
        if (decoded.point < 0)
            break; // EOF/error
        stats->num_points++;
        if (decoded.status == DS_OK) {
            stats->num_valid_points++;
            stats->total_point_value += decoded.point;
            if (decoded.point < 32) {
                if (decoded.point == '\n') {
                    stats->num_plain_ascii++;
                    stats->num_lines++;
                    stats->num_whitespace++;
                    if (prev_was_cr) {
                        stats->num_crlf++;
                    }
                } else if (decoded.point == '\t') {
                    stats->num_plain_ascii++;
                    stats->num_whitespace++;
                } else if (decoded.point == '\r') {
                    stats->num_plain_ascii++;
                } else {
                    stats->num_control++;
                    if (decoded.point == 0) {
                        stats->num_null++;
                    }
                }
            } else if (decoded.point < 127) {
                stats->num_plain_ascii++;
                if (decoded.point == ' ') {
                    stats->num_whitespace++;
                }
            } else if (decoded.point >= 65536) {
                stats->num_extended++;
            }
        }
        prev_was_cr = (decoded.point == '\r');
    }
    if (stats->num_points > 0) {
        stats->oo_num_points = 1.f / stats->num_points;
    }
    return numeric_cast<u32>(in.get_seek_pos());
}

static constexpr u32 NumBytesForTextFormatDetection = 100000;

TextFormat guess_file_encoding(Stream& in) {
    TextFileStats stats8;

    // Try UTF8 first:
    u32 num_bytes_read = scan_text_file(&stats8, in, UTF8, NumBytesForTextFormatDetection);
    if (num_bytes_read == 0) {
        // Empty file
        return {UTF8, TextFormat::NewLine::LF, false};
    }
    in.seek_to(0);
    if (stats8.num_invalid_points() == 0 && stats8.num_control == 0) {
        // No UTF-8 encoding errors, and no weird control characters/nulls. Pick UTF-8.
        return {UTF8, stats8.get_new_line_type(), false};
    }

    // If more than 20% of the high bytes in UTF-8 are encoding errors, reinterpret
    // UTF-8 as just bytes.
    UnicodeType encoding8 = UTF8;
    {
        u32 num_high_bytes = num_bytes_read - stats8.num_plain_ascii - stats8.num_control;
        if (stats8.num_invalid_points() >= num_high_bytes * 0.2f) {
            // Too many UTF-8 errors. Consider it bytes.
            encoding8 = NOT_UNICODE;
            stats8.num_points = num_bytes_read;
            stats8.num_valid_points = num_bytes_read;
        }
    }

    // Examine both UTF16 endianness:
    TextFileStats stats16_le;
    scan_text_file(&stats16_le, in, UTF16_LE, NumBytesForTextFormatDetection);
    in.seek_to(0);

    TextFileStats stats16_be;
    scan_text_file(&stats16_be, in, UTF16_BE, NumBytesForTextFormatDetection);
    in.seek_to(0);

    // Choose the better UTF16 candidate:
    TextFileStats* stats = &stats16_le;
    UnicodeType encoding = UTF16_LE;
    if (stats16_be.get_score() > stats16_le.get_score()) {
        stats = &stats16_be;
        encoding = UTF16_BE;
    }

    // Choose between the UTF16 and 8-bit encoding:
    if (stats8.get_score() >= stats->get_score()) {
        stats = &stats8;
        encoding = encoding8;
    }

    // Return best guess
    return {encoding, stats->get_new_line_type(), false};
}

TextFormat autodetect_text_format(Stream& in) {
    TextFormat tff;
    tff.bom = false;
    in.make_readable(3);
    if (in.view_remaining_bytes().left(3) == "\xef\xbb\xbf") {
        in.cur_byte += 3;
        tff.unicode_type = UTF8;
        tff.bom = true;
    } else if (in.view_remaining_bytes().left(2) == "\xff\xfe") {
        in.cur_byte += 2;
        tff.unicode_type = UTF16_LE;
        tff.bom = true;
    } else if (in.view_remaining_bytes().left(2) == "\xfe\xff") {
        in.cur_byte += 2;
        tff.unicode_type = UTF16_BE;
        tff.bom = true;
    }
    if (!tff.bom) {
        return guess_file_encoding(in);
    } else {
        // Detect LF or CRLF
        TextFileStats stats;
        scan_text_file(&stats, in, tff.unicode_type, NumBytesForTextFormatDetection);
        in.seek_to(0);
        tff.new_line = stats.get_new_line_type();
        return tff;
    }
}

//-----------------------------------------------------------------------

Owned<Pipe> create_importer(Stream&& in, const TextFormat& enc) {
    if (enc.bom) {
        in.make_readable(3);
        if (enc.unicode_type == UTF8) {
            if (in.view_remaining_bytes().left(3) == "\xef\xbb\xbf") {
                in.cur_byte += 3;
            }
        } else if (enc.unicode_type == UTF16_LE) {
            if (in.view_remaining_bytes().left(2) == "\xff\xfe") {
                in.cur_byte += 2;
            }
        } else if (enc.unicode_type == UTF16_BE) {
            if (in.view_remaining_bytes().left(2) == "\xfe\xff") {
                in.cur_byte += 2;
            }
        } else {
            PLY_ASSERT(0); // NON_UNICODE shouldn't have a BOM
        }
    }

    // Install converter from UTF-16 if needed
    Stream importer;
    if (enc.unicode_type == UTF8) {
        importer = std::move(in);
    } else {
        importer = Stream{Heap::create<InPipeConvertUnicode>(std::move(in), enc.unicode_type), true};
    }

    // Install newline filter (basically just eats \r)
    return Heap::create<InPipeNewLineFilter>(std::move(importer));
}

Owned<OutPipeNewLineFilter> create_exporter(Stream&& out, const TextFormat& enc) {
    Stream exporter = std::move(out);

    switch (enc.unicode_type) {
        case NOT_UNICODE: { // FIXME: Bytes needs to be converted
            break;
        }

        case UTF8: {
            if (enc.bom) {
                exporter.write({"\xef\xbb\xbf", 3});
            }
            break;
        }

        case UTF16_LE: {
            if (enc.bom) {
                exporter.write({"\xff\xfe", 2});
            }
            exporter = Stream{Heap::create<OutPipeConvertUnicode>(std::move(exporter), UTF16_LE), true};
            break;
        }

        case UTF16_BE: {
            if (enc.bom) {
                exporter.write({"\xfe\xff", 2});
            }
            exporter = Stream{Heap::create<OutPipeConvertUnicode>(std::move(exporter), UTF16_BE), true};
            break;
        }
    }

    return Heap::create<OutPipeNewLineFilter>(std::move(exporter), enc.new_line == TextFormat::CRLF);
}

//-----------------------------------------------------------------------
// WStringView
//-----------------------------------------------------------------------
struct WStringView {
    const char16_t* units = nullptr;
    u32 num_units = 0;

    WStringView() = default;
    WStringView(const char16_t* units, u32 num_units) : units{units}, num_units{num_units} {
    }
    StringView raw_bytes() const {
        return {(const char*) this->units, this->num_units << 1};
    }
#if defined(_WIN32)
    WStringView(LPCWSTR units) : units{(const char16_t*) units}, num_units{numeric_cast<u32>(wcslen(units))} {
    }
    WStringView(LPCWSTR units, u32 num_units) : units{(const char16_t*) units}, num_units{num_units} {
    }
#endif
};

//-----------------------------------------------------------------------
// WString
//-----------------------------------------------------------------------
struct WString {
    using View = WStringView;
    char16_t* units = nullptr;
    u32 num_units = 0;

    WString() = default;
    WString(WString&& other) : units{other.units}, num_units{other.num_units} {
        other.units = nullptr;
        other.num_units = 0;
    }
    ~WString() {
        if (units) {
            Heap::free(units);
        }
    }
    void operator=(WString&& other) {
        this->~WString();
        new (this) WString{std::move(other)};
    }
    static WString move_from_string(String&& other) {
        PLY_ASSERT(is_aligned_to_power_of_2(uptr(other.bytes), 2));
        PLY_ASSERT(is_aligned_to_power_of_2(other.num_bytes, 2));
        WString result;
        result.units = (char16_t*) other.bytes;
        result.num_units = other.num_bytes >> 1;
        other.bytes = nullptr;
        other.num_bytes = 0;
        return result;
    }

    bool includes_null_terminator() const {
        return this->num_units > 0 && this->units[this->num_units - 1] == 0;
    }
    static WString allocate(u32 num_units) {
        WString result;
        result.units = (char16_t*) Heap::alloc(num_units << 1);
        result.num_units = num_units;
        return result;
    }

#if defined(_WIN32)
    operator LPWSTR() const {
        PLY_ASSERT(this->includes_null_terminator()); // must be null terminated
        return (LPWSTR) this->units;
    }
#endif
};

WString to_wstring(StringView str) {
    ViewStream string{str};
    MemStream orig_mem_out;
    OutPipeConvertUnicode encoder{std::move(orig_mem_out), UTF16_LE};
    encoder.write(str);
    encoder.flush(false);
    MemStream* mem_out = static_cast<MemStream*>(&encoder.child_out);
    native_write(*mem_out, (u16) 0); // Null terminator
    return WString::move_from_string(mem_out->move_to_string());
}

String from_wstring(WStringView str) {
    InPipeConvertUnicode decoder{ViewStream{str.raw_bytes()}, UTF16_LE};
    MemStream out;
    while (out.make_writable()) {
        MutStringView buf{out.cur_byte, out.end_byte};
        u32 num_bytes = decoder.read(buf);
        if (num_bytes == 0)
            break;
        out.cur_byte += num_bytes;
    }
    return out.move_to_string();
}

//  ▄▄▄▄▄          ▄▄   ▄▄
//  ██  ██  ▄▄▄▄  ▄██▄▄ ██▄▄▄
//  ██▀▀▀   ▄▄▄██  ██   ██  ██
//  ██     ▀█▄▄██  ▀█▄▄ ██  ██
//

inline bool is_sep_byte(PathFormat fmt, char c) {
    return c == '/' || (fmt == WindowsPath && c == '\\');
}

StringView get_drive_letter(PathFormat fmt, StringView path) {
    if (fmt != WindowsPath)
        return {};
    if (path.num_bytes < 2)
        return {};
    char d = path.bytes[0];
    bool drive_is_ascii_letter = (d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z');
    if (drive_is_ascii_letter && path.bytes[1] == ':') {
        return path.left(2);
    }
    return {};
}

bool is_absolute_path(PathFormat fmt, StringView path) {
    if (fmt == WindowsPath) {
        return (path.num_bytes >= 3) && get_drive_letter(fmt, path) && is_sep_byte(fmt, path[2]);
    } else {
        return (path.num_bytes >= 1) && is_sep_byte(fmt, path[0]);
    }
}
SplitPath split_path(PathFormat fmt, StringView path) {
    s32 last_sep_index = path.reverse_find([&](char c) { return is_sep_byte(fmt, c); });
    if (last_sep_index >= 0) {
        s32 prefix_len = path.reverse_find([&](char c) { return !is_sep_byte(fmt, c); }, last_sep_index) + 1;
        if (path.left(prefix_len) == get_drive_letter(fmt, path)) {
            prefix_len++; // If prefix is the root, include a separator character
        }
        return {path.left(prefix_len), path.substr(last_sep_index + 1)};
    } else {
        return {String{}, path};
    }
}

SplitExtension split_file_extension(PathFormat fmt, StringView path) {
    StringView last_comp = path;
    s32 slash_pos = last_comp.reverse_find([&](char c) { return is_sep_byte(fmt, c); });
    if (slash_pos >= 0) {
        last_comp = last_comp.substr(slash_pos + 1);
    }
    s32 dot_pos = last_comp.reverse_find([](u32 c) { return c == '.'; });
    if (dot_pos < 0 || dot_pos == 0) {
        dot_pos = last_comp.num_bytes;
    }
    return {last_comp.left(dot_pos), last_comp.substr(dot_pos)};
}

Array<StringView> split_path_full(PathFormat fmt, StringView path) {
    Array<StringView> result;
    if (get_drive_letter(fmt, path)) {
        if (is_absolute_path(fmt, path)) {
            // Root with drive letter
            result.append(path.left(3));
            path = path.substr(3);
            while (path.num_bytes > 0 && is_sep_byte(fmt, path[0])) {
                path = path.substr(1);
            }
        } else {
            // Drive letter only
            result.append(path.left(2));
            path = path.substr(2);
        }
    } else if (path.num_bytes > 0 && is_sep_byte(fmt, path[0])) {
        // Starts with path separator
        result.append(path.left(1));
        path = path.substr(1);
        while (path.num_bytes > 0 && is_sep_byte(fmt, path[0])) {
            path = path.substr(1);
        }
    }
    if (path.num_bytes > 0) {
        for (;;) {
            PLY_ASSERT(path.num_bytes > 0);
            PLY_ASSERT(!is_sep_byte(fmt, path[0]));
            s32 sep_pos = path.find([&](char c) { return is_sep_byte(fmt, c); });
            if (sep_pos < 0) {
                result.append(path);
                break;
            }
            result.append(path.left(sep_pos));
            path = path.substr(sep_pos);
            s32 non_sep_pos = path.find([&](char c) { return !is_sep_byte(fmt, c); });
            if (non_sep_pos < 0) {
                // Empty final component
                result.append({});
                break;
            }
            path = path.substr(non_sep_pos);
        }
    }
    return result;
}

struct PathComponentIterator {
    char first_comp[3] = {0};

    void iterate_over(PathFormat fmt, ArrayView<const StringView> components,
                      const Functor<void(StringView)>& callback) {
        s32 absolute_index = -1;
        s32 drive_letter_index = -1;
        for (s32 i = components.num_items() - 1; i >= 0; i--) {
            if (absolute_index < 0 && is_absolute_path(fmt, components[i])) {
                absolute_index = i;
            }
            if (get_drive_letter(fmt, components[i])) {
                drive_letter_index = i;
                break;
            }
        }

        // Special first component if there's a drive letter and/or absolute component:
        if (drive_letter_index >= 0) {
            first_comp[0] = components[drive_letter_index][0];
            first_comp[1] = ':';
            if (absolute_index >= 0) {
                first_comp[2] = get_path_separator(fmt);
                callback(StringView{first_comp, 3});
            } else {
                callback(StringView{first_comp, 2});
            }
        }

        // Choose component to start iterating from:
        u32 i = drive_letter_index >= 0 ? drive_letter_index : 0;
        if (absolute_index >= 0) {
            PLY_ASSERT((u32) absolute_index >= i);
            i = absolute_index;
            if (drive_letter_index < 0) {
                PLY_ASSERT(first_comp[0] == 0);
                first_comp[0] = get_path_separator(fmt);
                callback(StringView{first_comp, 1});
            }
        }

        // Iterate over components. Remember, we've already sent the drive letter and/or
        // initial slash as its own component (if any).
        for (; i < components.num_items(); i++) {
            StringView comp = components[i];
            if ((s32) i == drive_letter_index) {
                comp = comp.substr(2);
            }

            s32 non_sep = comp.find([fmt](char c) { return !is_sep_byte(fmt, c); });
            while (non_sep >= 0) {
                s32 sep = comp.find([fmt](char c) { return is_sep_byte(fmt, c); }, non_sep + 1);
                if (sep < 0) {
                    callback(comp.substr(non_sep));
                    break;
                } else {
                    callback(comp.substr(non_sep, sep - non_sep));
                    non_sep = comp.find([fmt](char c) { return !is_sep_byte(fmt, c); }, sep + 1);
                }
            }
        }
    }

    // Note: Keep the PathComponentIterator alive while using the return value
    Array<StringView> get_normalized_comps(PathFormat fmt, ArrayView<const StringView> components) {
        Array<StringView> norm_comps;
        u32 up_count = 0;
        this->iterate_over(fmt, components, [&](StringView comp) { //
            if (comp == "..") {
                if (norm_comps.num_items() > up_count) {
                    norm_comps.pop();
                } else {
                    PLY_ASSERT(norm_comps.num_items() == up_count);
                    norm_comps.append("..");
                }
            } else if (comp != "." && !comp.is_empty()) {
                norm_comps.append(comp);
            }
        });
        return norm_comps;
    }
};

String join_path_from_array(PathFormat fmt, ArrayView<const StringView> components) {
    PathComponentIterator comp_iter;
    Array<StringView> norm_comps = comp_iter.get_normalized_comps(fmt, components);
    if (norm_comps.is_empty()) {
        if (components.num_items() > 0 && components.back().is_empty()) {
            return StringView{"."} + get_path_separator(fmt);
        } else {
            return ".";
        }
    } else {
        MemStream out;
        bool need_sep = false;
        for (StringView comp : norm_comps) {
            if (need_sep) {
                out.write(get_path_separator(fmt));
            } else {
                if (comp.num_bytes > 0) {
                    need_sep = !is_sep_byte(fmt, comp[comp.num_bytes - 1]);
                }
            }
            out.write(comp);
        }
        if ((components.back().is_empty() || is_sep_byte(fmt, components.back().back())) && need_sep) {
            out.write(get_path_separator(fmt));
        }
        return out.move_to_string();
    }
}

String make_relative_path(PathFormat fmt, StringView ancestor, StringView descendant) {
    // This function requires either both absolute paths or both relative paths:
    PLY_ASSERT(is_absolute_path(fmt, ancestor) == is_absolute_path(fmt, descendant));

    // FIXME: Implement fastpath when descendant starts with ancestor and there are no
    // ".", ".." components.

    PathComponentIterator ancestor_comp_iter;
    Array<StringView> ancestor_comps = ancestor_comp_iter.get_normalized_comps(fmt, {ancestor});
    PathComponentIterator descendant_comp_iter;
    Array<StringView> descendant_comps = descendant_comp_iter.get_normalized_comps(fmt, {descendant});

    // Determine number of matching components
    u32 mc = 0;
    while (mc < ancestor_comps.num_items() && mc < descendant_comps.num_items()) {
        if (ancestor_comps[mc] != descendant_comps[mc])
            break;
        mc++;
    }

    // Determine number of ".." to output (will be 0 if drive letters mismatch)
    u32 up_folders = 0;
    if (!is_absolute_path(fmt, ancestor) || mc > 0) {
        up_folders = ancestor_comps.num_items() - mc;
    }

    // Form relative path (or absolute path if drive letters mismatch)
    MemStream out;
    bool need_sep = false;
    for (u32 i = 0; i < up_folders; i++) {
        if (need_sep) {
            out.write(get_path_separator(fmt));
        }
        out.write("..");
        need_sep = true;
    }
    for (u32 i = mc; i < descendant_comps.num_items(); i++) {
        if (need_sep) {
            out.write(get_path_separator(fmt));
        }
        out.write(descendant_comps[i]);
        need_sep = !is_sep_byte(fmt, descendant_comps[i].back());
    }

    // .
    if (out.get_seek_pos() == 0) {
        out.write(".");
        need_sep = true;
    }

    // Trailing slash
    if (descendant.num_bytes > 0 && is_sep_byte(fmt, descendant.back()) && need_sep) {
        out.write(get_path_separator(fmt));
    }

    return out.move_to_string();
}

//  ▄▄▄▄▄ ▄▄ ▄▄▄                               ▄▄
//  ██    ▄▄  ██   ▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀  ██  ██  ██▄▄██ ▀█▄▄▄  ██  ██ ▀█▄▄▄   ██   ██▄▄██ ██ ██ ██
//  ██    ██ ▄██▄ ▀█▄▄▄   ▄▄▄█▀ ▀█▄▄██  ▄▄▄█▀  ▀█▄▄ ▀█▄▄▄  ██ ██ ██
//                               ▄▄▄█▀

WString win32_path_arg(StringView path, bool allow_extended = true) {
    ViewStream path_in{path};
    MemStream out;
    if (allow_extended && is_absolute_path(WindowsPath, path)) {
        out.write(ArrayView<const char16_t>{u"\\\\?\\", 4}.string_view());
    }
    while (true) {
        s32 codepoint = decode_unicode(path_in, UTF8).point;
        if (codepoint < 0)
            break;
        if (codepoint == '/') {
            codepoint = '\\'; // Fix slashes.
        }
        encode_unicode(out, UTF16_LE, codepoint);
    }
    native_write(out, (u16) 0); // Null terminator.
    return WString::move_from_string(out.move_to_string());
}

ThreadLocal<FSResult> Filesystem::last_result_;

void DirectoryWalker::visit(StringView dir_path) {
    this->triple.dir_path = dir_path;
    this->triple.dir_names.clear();
    this->triple.files.clear();
    for (DirectoryEntry& entry : Filesystem::list_dir(dir_path)) {
        if (entry.is_dir) {
            this->triple.dir_names.append(std::move(entry.name));
        } else {
            this->triple.files.append(std::move(entry));
        }
    }
}

void DirectoryWalker::Iterator::operator++() {
    if (!this->walker->triple.dir_names.is_empty()) {
        StackItem& item = this->walker->stack.append();
        item.path = std::move(this->walker->triple.dir_path);
        item.dir_names = std::move(this->walker->triple.dir_names);
        item.dir_index = 0;
    } else {
        this->walker->triple.dir_path.clear();
        this->walker->triple.dir_names.clear();
        this->walker->triple.files.clear();
    }
    while (!this->walker->stack.is_empty()) {
        StackItem& item = this->walker->stack.back();
        if (item.dir_index < item.dir_names.num_items()) {
            this->walker->visit(join_path(item.path, item.dir_names[item.dir_index]));
            item.dir_index++;
            return;
        }
        this->walker->stack.pop();
    }
    // End of walk
    PLY_ASSERT(this->walker->triple.dir_path.is_empty());
}

FSResult Filesystem::copy_file(StringView src_path, StringView dst_path) {
    Owned<Pipe> in = Filesystem::open_pipe_for_read(src_path);
    if (Filesystem::last_result() != FS_OK)
        return Filesystem::last_result();
    PLY_ASSERT(in);

    Stream out = Filesystem::open_binary_for_write(dst_path);
    if (Filesystem::last_result() != FS_OK)
        return Filesystem::last_result();
    PLY_ASSERT(out.is_open());

    for (;;) {
        out.make_writable();
        u32 num_bytes_read = in->read(out.view_remaining_bytes_mut());
        if (num_bytes_read == 0)
            break;
        out.cur_byte += num_bytes_read;
    }

    // FIXME: More robust, detect bad copies
    return FS_OK;
}

DirectoryWalker Filesystem::walk(StringView top) {
    DirectoryWalker walker;
    walker.visit(top);
    return walker;
}

FSResult Filesystem::make_dirs(StringView path) {
    if (path == get_drive_letter(path)) {
        return Filesystem::set_last_result(FS_OK);
    }
    ExistsResult er = Filesystem::exists(path);
    if (er == ER_DIRECTORY) {
        return Filesystem::set_last_result(FS_ALREADY_EXISTS);
    } else if (er == ER_FILE) {
        return Filesystem::set_last_result(FS_ACCESS_DENIED);
    } else {
        SplitPath split = split_path(path);
        if (!split.directory.is_empty() && !split.filename.is_empty()) {
            FSResult r = make_dirs(split.directory);
            if (r != FS_OK && r != FS_ALREADY_EXISTS)
                return r;
        }
        return Filesystem::make_dir(path);
    }
}

Stream Filesystem::open_binary_for_read(StringView path) {
    return {Filesystem::open_pipe_for_read(path).release(), true};
}

Stream Filesystem::open_binary_for_write(StringView path) {
    return {Filesystem::open_pipe_for_write(path).release(), true};
}

Stream Filesystem::open_text_for_read(StringView path, const TextFormat& text_format) {
    if (Stream in = Filesystem::open_binary_for_read(path))
        return {create_importer(std::move(in), text_format).release(), true};
    return {};
}

Stream Filesystem::open_text_for_read_autodetect(StringView path, TextFormat* out_format) {
    if (Stream in = Filesystem::open_binary_for_read(path)) {
        TextFormat text_format = autodetect_text_format(in);
        if (out_format) {
            *out_format = text_format;
        }
        return {create_importer(std::move(in), text_format).release(), true};
    }
    return {};
}

String Filesystem::load_binary(StringView path) {
    String result;
    Owned<Pipe> in_pipe = Filesystem::open_pipe_for_read(path);
    if (in_pipe) {
        u64 file_size = in_pipe->get_file_size();
        // Files >= 4GB cannot be loaded this way:
        result.resize(numeric_cast<u32>(file_size));
        in_pipe->read({result.bytes, result.num_bytes});
    }
    return result;
}

String read_all_remaining_bytes(Pipe* in_pipe) {
    MemStream mem;
    for (;;) {
        mem.make_writable();
        u32 num_bytes_read = in_pipe->read(mem.view_remaining_bytes_mut());
        if (num_bytes_read == 0)
            break;
        mem.cur_byte += num_bytes_read;
    }
    return mem.move_to_string();
}

String Filesystem::load_text(StringView path, const TextFormat& text_format) {
    if (Stream in = Filesystem::open_binary_for_read(path)) {
        Owned<Pipe> importer = create_importer(std::move(in), text_format);
        return read_all_remaining_bytes(importer);
    }
    return {};
}

String Filesystem::load_text_autodetect(StringView path, TextFormat* out_format) {
    if (Stream in = Filesystem::open_binary_for_read(path)) {
        TextFormat text_format = autodetect_text_format(in);
        if (out_format) {
            *out_format = text_format;
        }

        Owned<Pipe> importer = create_importer(std::move(in), text_format);
        return read_all_remaining_bytes(importer);
    }
    return {};
}

Stream Filesystem::open_text_for_write(StringView path, const TextFormat& text_format) {
    if (Stream out = Filesystem::open_binary_for_write(path))
        return {create_exporter(std::move(out), text_format).release(), true};
    return {};
}

FSResult Filesystem::save_binary(StringView path, StringView view) {
    // FIXME: Write to temporary file first, then rename atomically
    Owned<Pipe> out_pipe = Filesystem::open_pipe_for_write(path);
    FSResult result = Filesystem::last_result();
    if (result != FS_OK) {
        return result;
    }
    out_pipe->write(view);
    return result;
}

FSResult Filesystem::save_text(StringView path, StringView str_contents, const TextFormat& enc) {
    Owned<OutPipeNewLineFilter> exporter = create_exporter(MemStream{}, enc);
    exporter->write(str_contents);
    exporter->flush(false);
    String raw_data = static_cast<MemStream*>(&exporter->out)->move_to_string();
    return Filesystem::save_binary(path, raw_data);
}

#if defined(_WIN32)

//-----------------------------------------------
// Windows
//-----------------------------------------------

#define PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS 0

ReadWriteLock Filesystem::working_dir_lock;

inline double windows_to_posix_time(const FILETIME& file_time) {
    return (u64(file_time.dwHighDateTime) << 32 | file_time.dwLowDateTime) / 10000000.0 - 11644473600.0;
}

void dir_entry_from_data(DirectoryEntry* entry, WIN32_FIND_DATAW find_data) {
    entry->name = from_wstring(find_data.cFileName);
    entry->is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry->file_size = u64(find_data.nFileSizeHigh) << 32 | find_data.nFileSizeLow;
    entry->creation_time = windows_to_posix_time(find_data.ftCreationTime);
    entry->access_time = windows_to_posix_time(find_data.ftLastAccessTime);
    entry->modification_time = windows_to_posix_time(find_data.ftLastWriteTime);
}

Array<DirectoryEntry> Filesystem::list_dir(StringView path) {
    Array<DirectoryEntry> result;
    HANDLE hfind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW find_data;

    String pattern = join_path(WindowsPath, path, "*");
    hfind = FindFirstFileW(win32_path_arg(pattern), &find_data);
    if (hfind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME: {
                Filesystem::set_last_result(FS_NOT_FOUND);
                return result;
            }
            case ERROR_ACCESS_DENIED: {
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                return result;
            }
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                return result;
            }
        }
    }

    while (true) {
        DirectoryEntry entry;
        dir_entry_from_data(&entry, find_data);
        if (entry.name != "." && entry.name != "..") {
            result.append(std::move(entry));
        }

        BOOL rc = FindNextFileW(hfind, &find_data);
        if (!rc) {
            DWORD err = GetLastError();
            switch (err) {
                case ERROR_NO_MORE_FILES: {
                    Filesystem::set_last_result(FS_OK);
                    return result;
                }
                case ERROR_FILE_INVALID: {
                    Filesystem::set_last_result(FS_NOT_FOUND);
                    return result;
                }
                default: {
                    PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                    Filesystem::set_last_result(FS_UNKNOWN);
                    return result;
                }
            }
        }
    }
}

FSResult Filesystem::make_dir(StringView path) {
    BOOL rc = CreateDirectoryW(win32_path_arg(path), NULL);
    if (rc) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_ALREADY_EXISTS:
                return Filesystem::set_last_result(FS_ALREADY_EXISTS);
            case ERROR_ACCESS_DENIED:
                return Filesystem::set_last_result(FS_ACCESS_DENIED);
            case ERROR_INVALID_NAME:
                return Filesystem::set_last_result(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::set_last_result(FS_UNKNOWN);
            }
        }
    }
}

FSResult Filesystem::set_working_directory(StringView path) {
    BOOL rc;
    {
        // This ReadWriteLock is used to mitigate data race issues with
        // SetCurrentDirectoryW:
        // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
        Filesystem::working_dir_lock.lock_exclusive();
        rc = SetCurrentDirectoryW(win32_path_arg(path));
        Filesystem::working_dir_lock.unlock_exclusive();
    }
    if (rc) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_PATH_NOT_FOUND:
                return Filesystem::set_last_result(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::set_last_result(FS_UNKNOWN);
            }
        }
    }
}

String Filesystem::get_working_directory() {
    u32 num_units_with_null_term = MAX_PATH + 1;
    for (;;) {
        WString win32_path = WString::allocate(num_units_with_null_term);
        DWORD rc;
        {
            // This ReadWriteLock is used to mitigate data race issues with
            // SetCurrentDirectoryW:
            // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
            Filesystem::working_dir_lock.lock_shared();
            rc = GetCurrentDirectoryW(num_units_with_null_term, (LPWSTR) win32_path.units);
            Filesystem::working_dir_lock.unlock_shared();
        }
        if (rc == 0) {
            PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
            Filesystem::set_last_result(FS_UNKNOWN);
            return {};
        }
        PLY_ASSERT(rc != num_units_with_null_term);
        if (rc < num_units_with_null_term) {
            // GetCurrentDirectoryW: If the function succeeds, the return value
            // specifies the number of characters that are written to the buffer, not
            // including the terminating null character.
            WStringView truncated_win32_path = {win32_path.units, rc};
            if (truncated_win32_path.num_units >= 4 &&
                truncated_win32_path.raw_bytes().left(8) == StringView{(const char*) L"\\\\?\\", 8}) {
                // Drop leading "\\\\?\\":
                truncated_win32_path.units += 4;
                truncated_win32_path.num_units -= 4;
            }
            Filesystem::set_last_result(FS_OK);
            return from_wstring(truncated_win32_path);
        }
        // GetCurrentDirectoryW: If the buffer that is pointed to by lp_buffer is not
        // large enough, the return value specifies the required size of the buffer, in
        // characters, including the null-terminating character.
        num_units_with_null_term = rc;
    }
}

ExistsResult Filesystem::exists(StringView path) {
    // FIXME: Do something sensible when passed "C:" and other drive letters
    DWORD attribs = GetFileAttributesW(win32_path_arg(path));
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME: {
                return ER_NOT_FOUND;
            }
            default: {
#if defined(PLY_WITH_ASSERTS)
                PLY_FORCE_CRASH(); // Unrecognized error
#endif
                return ER_NOT_FOUND;
            }
        }
    } else if ((attribs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return ER_DIRECTORY;
    } else {
        return ER_FILE;
    }
}

HANDLE Filesystem::open_handle_for_read(StringView path) {
    // Should this use FILE_SHARE_DELETE or FILE_SHARE_WRITE?
    HANDLE handle = CreateFileW(win32_path_arg(path), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE) {
        Filesystem::set_last_result(FS_OK);
    } else {
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
                Filesystem::set_last_result(FS_NOT_FOUND);
                break;

            case ERROR_SHARING_VIOLATION:
                Filesystem::set_last_result(FS_LOCKED);
                break;

            case ERROR_ACCESS_DENIED:
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                break;
        }
    }
    return handle;
}

Owned<Pipe> Filesystem::open_pipe_for_read(StringView path) {
    HANDLE handle = open_handle_for_read(path);
    if (handle == INVALID_HANDLE_VALUE)
        return nullptr;
    return Heap::create<PipeHandle>(handle, Pipe::HAS_READ_PERMISSION | Pipe::CAN_SEEK);
}

HANDLE Filesystem::open_handle_for_write(StringView path) {
    // FIXME: Needs graceful handling of ERROR_SHARING_VIOLATION
    // Should this use FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE?
    HANDLE handle =
        CreateFileW(win32_path_arg(path), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE) {
        Filesystem::set_last_result(FS_OK);
    } else {
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
                Filesystem::set_last_result(FS_NOT_FOUND);
                break;

            case ERROR_SHARING_VIOLATION:
                Filesystem::set_last_result(FS_LOCKED);
                break;

            case ERROR_ACCESS_DENIED:
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                break;
        }
    }
    return handle;
}

Owned<Pipe> Filesystem::open_pipe_for_write(StringView path) {
    HANDLE handle = open_handle_for_write(path);
    if (handle == INVALID_HANDLE_VALUE)
        return nullptr;
    return Heap::create<PipeHandle>(handle, Pipe::HAS_WRITE_PERMISSION | Pipe::CAN_SEEK);
}

FSResult Filesystem::move_file(StringView src_path, StringView dst_path) {
    BOOL rc = MoveFileExW(win32_path_arg(src_path), win32_path_arg(dst_path), MOVEFILE_REPLACE_EXISTING);
    if (rc) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        DWORD error = GetLastError();
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::set_last_result(FS_UNKNOWN);
    }
}

FSResult Filesystem::delete_file(StringView path) {
    BOOL rc = DeleteFileW(win32_path_arg(path));
    if (rc) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        DWORD err = GetLastError();
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::set_last_result(FS_UNKNOWN);
    }
}

FSResult Filesystem::remove_dir_tree(StringView dir_path) {
    String abs_path = dir_path;
    if (!is_absolute_path(WindowsPath, dir_path)) {
        abs_path = join_path(WindowsPath, Filesystem::get_working_directory(), dir_path);
    }
    OutPipeConvertUnicode out{MemStream{}, UTF16_LE};
    out.write(abs_path);
    out.child_out.write({"\0\0\0\0", 4}); // double null terminated
    MemStream* mem_out = static_cast<MemStream*>(&out.child_out);
    WString wstr = WString::move_from_string(mem_out->move_to_string());
    SHFILEOPSTRUCTW shfo;
    memset(&shfo, 0, sizeof(shfo));
    shfo.hwnd = NULL;
    shfo.wFunc = FO_DELETE;
    shfo.pFrom = wstr;
    shfo.pTo = NULL;
    shfo.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
    shfo.fAnyOperationsAborted = FALSE;
    shfo.hNameMappings = NULL;
    shfo.lpszProgressTitle = NULL;
    int rc = SHFileOperationW(&shfo);
    return (rc == 0) ? FS_OK : FS_ACCESS_DENIED;
}

DirectoryEntry Filesystem::get_file_info(HANDLE handle) {
    DirectoryEntry entry;
    FILETIME creation_time = {0, 0};
    FILETIME last_access_time = {0, 0};
    FILETIME last_write_time = {0, 0};
    BOOL rc = GetFileTime(handle, &creation_time, &last_access_time, &last_write_time);
    if (rc) {
        entry.creation_time = windows_to_posix_time(creation_time);
        entry.access_time = windows_to_posix_time(last_access_time);
        entry.modification_time = windows_to_posix_time(last_write_time);
    } else {
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        entry.result = FS_UNKNOWN;
    }

    LARGE_INTEGER file_size;
    rc = GetFileSizeEx(handle, &file_size);
    if (rc) {
        entry.file_size = file_size.QuadPart;
    } else {
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        entry.result = FS_UNKNOWN;
    }

    entry.result = FS_OK;
    Filesystem::set_last_result(FS_OK);
    return entry;
}

DirectoryEntry Filesystem::get_file_info(StringView path) {
    HANDLE handle = Filesystem::open_handle_for_read(path);
    if (handle == INVALID_HANDLE_VALUE) {
        DirectoryEntry entry;
        entry.result = Filesystem::last_result();
        return entry;
    }

    DirectoryEntry entry = Filesystem::get_file_info(handle);
    CloseHandle(handle);
    return entry;
}

#elif defined(PLY_POSIX)

//-----------------------------------------------
// POSIX
//-----------------------------------------------

#define PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS 0

Array<DirectoryEntry> Filesystem::list_dir(StringView path) {
    Array<DirectoryEntry> result;

    DIR* dir = opendir((path + '\0').bytes);
    if (!dir) {
        switch (errno) {
            case ENOENT: {
                Filesystem::set_last_result(FS_NOT_FOUND);
                return result;
            }
            case EACCES: {
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                return result;
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                return result;
            }
        }
    }

    while (true) {
        errno = 0;
        struct dirent* rde = readdir(dir);
        if (!rde) {
            if (errno == 0) {
                Filesystem::set_last_result(FS_OK);
            } else {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
            }
            break;
        }

        DirectoryEntry entry;
        entry.name = rde->d_name;

        // d_type is not POSIX, but it exists on OSX and Linux.
        if (rde->d_type == DT_REG) {
            entry.is_dir = false;
        } else if (rde->d_type == DT_DIR) {
            if (rde->d_name[0] == '.') {
                if (rde->d_name[1] == 0 || (rde->d_name[1] == '.' && rde->d_name[2] == 0))
                    continue;
            }
            entry.is_dir = true;
        }

        // Get additional file information
        String joined_path = join_path(POSIXPath, path, entry.name);
        struct stat buf;
        int rc = stat((joined_path + '\0').bytes, &buf);
        if (rc != 0) {
            if (errno == ENOENT)
                continue;
            PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
            Filesystem::set_last_result(FS_UNKNOWN);
            break;
        }

        if (!entry.is_dir) {
            entry.file_size = buf.st_size;
        }
        entry.creation_time = buf.st_ctime;
        entry.access_time = buf.st_atime;
        entry.modification_time = buf.st_mtime;

        result.append(std::move(entry));
    }

    closedir(dir);
    return result;
}

FSResult Filesystem::make_dir(StringView path) {
    int rc = mkdir((path + '\0').bytes, mode_t(0755));
    if (rc == 0) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        switch (errno) {
            case EEXIST:
            case EISDIR: {
                return Filesystem::set_last_result(FS_ALREADY_EXISTS);
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::set_last_result(FS_UNKNOWN);
            }
        }
    }
}

FSResult Filesystem::set_working_directory(StringView path) {
    int rc = chdir((path + '\0').bytes);
    if (rc == 0) {
        return Filesystem::set_last_result(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                return Filesystem::set_last_result(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::set_last_result(FS_UNKNOWN);
            }
        }
    }
}

String Filesystem::get_working_directory() {
    u32 num_units_with_null_term = PATH_MAX + 1;
    String path = String::allocate(num_units_with_null_term);
    for (;;) {
        char* rs = getcwd(path.bytes, num_units_with_null_term);
        if (rs) {
            s32 len = path.find('\0');
            PLY_ASSERT(len >= 0);
            path.resize(len);
            Filesystem::set_last_result(FS_OK);
            return path;
        } else {
            switch (errno) {
                case ERANGE: {
                    num_units_with_null_term *= 2;
                    path.resize(num_units_with_null_term);
                    break;
                }
                default: {
                    PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                    Filesystem::set_last_result(FS_UNKNOWN);
                    return {};
                }
            }
        }
    }
}

ExistsResult Filesystem::exists(StringView path) {
    struct stat buf;
    int rc = stat((path + '\0').bytes, &buf);
    if (rc == 0)
        return (buf.st_mode & S_IFMT) == S_IFDIR ? ER_DIRECTORY : ER_FILE;
    if (errno != ENOENT) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
    }
    return ER_NOT_FOUND;
}

int Filesystem::open_fd_for_read(StringView path) {
    int fd = open((path + '\0').bytes, O_RDONLY | O_CLOEXEC);
    if (fd != -1) {
        Filesystem::set_last_result(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                Filesystem::set_last_result(FS_NOT_FOUND);
                break;

            case EACCES:
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                break;
        }
    }
    return fd;
}

Owned<Pipe> Filesystem::open_pipe_for_read(StringView path) {
    int fd = open_fd_for_read(path);
    if (fd == -1)
        return nullptr;
    return Heap::create<Pipe_FD>(fd, Pipe::HAS_READ_PERMISSION | Pipe::CAN_SEEK);
}

int Filesystem::open_fd_for_write(StringView path) {
    int fd = open((path + '\0').bytes, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode_t(0644));
    if (fd != -1) {
        Filesystem::set_last_result(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                Filesystem::set_last_result(FS_NOT_FOUND);
                break;

            case EACCES:
                Filesystem::set_last_result(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                break;
        }
    }
    return fd;
}

Owned<Pipe> Filesystem::open_pipe_for_write(StringView path) {
    int fd = open_fd_for_write(path);
    if (fd == -1)
        return nullptr;
    return Heap::create<Pipe_FD>(fd, Pipe::HAS_WRITE_PERMISSION | Pipe::CAN_SEEK);
}

FSResult Filesystem::move_file(StringView src_path, StringView dst_path) {
    int rc = rename((src_path + '\0').bytes, (dst_path + '\0').bytes);
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::set_last_result(FS_UNKNOWN);
    }
    return Filesystem::set_last_result(FS_OK);
}

FSResult Filesystem::delete_file(StringView path) {
    int rc = unlink((path + '\0').bytes);
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::set_last_result(FS_UNKNOWN);
    }
    return Filesystem::set_last_result(FS_OK);
}

FSResult Filesystem::remove_dir_tree(StringView dir_path) {
    for (const DirectoryEntry& entry : Filesystem::list_dir(dir_path)) {
        String joined = join_path(POSIXPath, dir_path, entry.name);
        if (entry.is_dir) {
            FSResult fs_result = Filesystem::remove_dir_tree(joined);
            if (fs_result != FS_OK) {
                return fs_result;
            }
        } else {
            int rc = unlink((joined + '\0').bytes);
            if (rc != 0) {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::set_last_result(FS_UNKNOWN);
            }
        }
    }
    int rc = rmdir((dir_path + '\0').bytes);
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::set_last_result(FS_UNKNOWN);
    }
    return Filesystem::set_last_result(FS_OK);
}

DirectoryEntry Filesystem::get_file_info(StringView path) {
    DirectoryEntry entry;
    struct stat buf;
    int rc = stat((path + '\0').bytes, &buf);
    if (rc != 0) {
        switch (errno) {
            case ENOENT: {
                entry.result = Filesystem::set_last_result(FS_NOT_FOUND);
                break;
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::set_last_result(FS_UNKNOWN);
                break;
            }
        }
    } else {
        entry.result = Filesystem::set_last_result(FS_OK);
        entry.file_size = buf.st_size;
        entry.creation_time = buf.st_ctime;
        entry.access_time = buf.st_atime;
        entry.modification_time = buf.st_mtime;
    }
    return entry;
}

#endif

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if defined(_WIN32)

void DirectoryWatcher::run_watcher() {
    // FIXME: prepend \\?\ to the path to get past MAX_PATH limitation
    HANDLE h_directory = CreateFileW(win32_path_arg(this->root), FILE_LIST_DIRECTORY,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    PLY_ASSERT(h_directory != INVALID_HANDLE_VALUE);
    HANDLE h_change_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    PLY_ASSERT(h_change_event != INVALID_HANDLE_VALUE);
    static const DWORD notify_info_size = 65536;
    FILE_NOTIFY_INFORMATION* notify_info = (FILE_NOTIFY_INFORMATION*) Heap::alloc(notify_info_size);
    for (;;) {
        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = h_change_event;
        BOOL rc =
            ReadDirectoryChangesW(h_directory, notify_info, notify_info_size, TRUE,
                                  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                      FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                  NULL, &overlapped, NULL);
        // FIXME: Handle ERROR_NOTIFY_ENUM_DIR
        HANDLE events[2] = {this->end_event, h_change_event};
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        PLY_ASSERT(wait_result >= WAIT_OBJECT_0 && wait_result <= WAIT_OBJECT_0 + 1);
        if (wait_result == WAIT_OBJECT_0)
            break;
        FILE_NOTIFY_INFORMATION* r = notify_info;
        for (;;) {
            // "The file name is in the Unicode character format and is not
            // null-terminated."
            // https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-_file_notify_information
            String path = from_wstring({r->FileName, u32(r->FileNameLength / sizeof(WCHAR))});
            bool is_directory = false;
            DWORD attribs;
            {
                // FIXME: Avoid some of the UTF-8 <--> UTF-16 conversions done here
                String full_path = join_path(WindowsPath, this->root, path);
                attribs = GetFileAttributesW(win32_path_arg(full_path));
            }
            if (attribs != INVALID_FILE_ATTRIBUTES) {
                is_directory = (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }
            this->callback(path, is_directory);
            if (r->NextEntryOffset == 0)
                break;
            r = (FILE_NOTIFY_INFORMATION*) PLY_PTR_OFFSET(r, r->NextEntryOffset);
        }
    }
    Heap::free(notify_info);
    CloseHandle(h_change_event);
    CloseHandle(h_directory);
}

DirectoryWatcher::DirectoryWatcher() {
}

void DirectoryWatcher::start(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback) {
    PLY_ASSERT(this->root.is_empty());
    PLY_ASSERT(!this->callback);
    PLY_ASSERT(this->end_event == INVALID_HANDLE_VALUE);
    PLY_ASSERT(!this->watcher_thread.is_valid());
    this->root = root;
    this->callback = std::move(callback);
    this->end_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    this->watcher_thread.run([this]() { run_watcher(); });
}

void DirectoryWatcher::stop() {
    if (this->watcher_thread.is_valid()) {
        SetEvent(this->end_event);
        this->watcher_thread.join();
        CloseHandle(this->end_event);
        this->end_event = INVALID_HANDLE_VALUE;
    }
}

#elif defined(PLY_MACOS)

void my_callback(ConstFSEventStreamRef stream_ref, void* client_call_back_info, size_t num_events, void* event_paths,
                 const FSEventStreamEventFlags event_flags[], const FSEventStreamEventId event_ids[]) {
    DirectoryWatcher* watcher = (DirectoryWatcher*) client_call_back_info;
    char** paths = (char**) event_paths;
    for (size_t i = 0; i < num_events; i++) {
        /* flags are unsigned long, IDs are uint64_t */
        StringView p = paths[i];
        FSEventStreamEventFlags flags = event_flags[i];
        PLY_ASSERT(p.starts_with(watcher->root));
        p = p.sub_str(watcher->root.num_bytes);

        // puts(String::format("change {} in {}, flags {}/0x{}", event_ids[i],
        // String::convert(p), flags, String::to_hex(flags)).bytes());
        bool must_recurse = false;
        if ((flags & kFSEventStreamEventFlagMustScanSubDirs) != 0) {
            must_recurse = true;
        }
        if ((flags & kFSEventStreamEventFlagItemIsDir) != 0) {
            must_recurse = true;
        }
        // FIXME: check kFSEventStreamEventFlagEventIdsWrapped
        watcher->callback(p, must_recurse);
    }
}

void DirectoryWatcher::run_watcher() {
    this->run_loop = CFRunLoopGetCurrent();
    CFStringRef root_path = CFStringCreateWithCString(NULL, this->root.bytes, kCFStringEncodingASCII);
    CFArrayRef paths_to_watch = CFArrayCreate(NULL, (const void**) &root_path, 1, NULL);
    FSEventStreamContext context;
    context.version = 0;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copy_description = NULL;
    // FIXME: should use kFSEventStreamCreateFlagWatchRoot to check if the folder being
    // watched gets moved?
    FSEventStreamRef stream =
        FSEventStreamCreate(NULL, my_callback, &context, paths_to_watch, kFSEventStreamEventIdSinceNow,
                            0.15, // latency
                            kFSEventStreamCreateFlagFileEvents);
    CFRelease(paths_to_watch);
    CFRelease(root_path);
    FSEventStreamScheduleWithRunLoop(stream, (CFRunLoopRef) this->run_loop, kCFRunLoopDefaultMode);
    Boolean rc = FSEventStreamStart(stream);
    PLY_ASSERT(rc == TRUE);
    PLY_UNUSED(rc);

    CFRunLoopRun();

    FSEventStreamStop(stream);
    FSEventStreamUnscheduleFromRunLoop(stream, (CFRunLoopRef) this->run_loop, kCFRunLoopDefaultMode);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
}

DirectoryWatcher::DirectoryWatcher() {
}

void DirectoryWatcher::start(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback) {
    PLY_ASSERT(this->root.is_empty());
    PLY_ASSERT(!this->callback);
    PLY_ASSERT(!this->watcher_thread.is_valid());
    this->root = root;
    this->callback = std::move(callback);
    this->watcher_thread.run([this]() { run_watcher(); });
}

void DirectoryWatcher::stop() {
    if (this->watcher_thread.is_valid()) {
        CFRunLoopStop((CFRunLoopRef) this->run_loop);
        this->watcher_thread.join();
    }
}

#endif

} // namespace ply
