/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-base.h"

#if defined(PLY_WINDOWS)
#include <shellapi.h>
#include <Psapi.h>
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
#if defined(PLY_APPLE)
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/task.h>
#if PLY_WITH_DIRECTORY_WATCHER
#include <CoreServices/CoreServices.h>
#endif
#endif
#endif

#if PLY_USE_DLMALLOC
extern "C" {
void* dlmalloc(ply::uptr);
void* dlrealloc(void*, ply::uptr);
void dlfree(void*);
void* dlmemalign(ply::uptr, ply::uptr);

struct DLMallocStats {
    ply::uptr totalBytesConsumed;
    ply::uptr totalSystemMemoryUsed;
};
void dlget_heap_stats(DLMallocStats* stats);
}
#endif

namespace ply {

//  ▄▄▄▄▄▄ ▄▄                      ▄▄▄        ▄▄▄▄▄          ▄▄
//    ██   ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄      ██ ▀▀       ██  ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄
//    ██   ██ ██ ██ ██ ██▄▄██     ▄█▀█▄▀▀     ██  ██  ▄▄▄██  ██   ██▄▄██
//    ██   ██ ██ ██ ██ ▀█▄▄▄      ▀█▄▄▀█▄     ██▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄
//

#if defined(PLY_WINDOWS)

float getCpuTicksPerSecond() {
    static LARGE_INTEGER frequency;
    BOOL rc = QueryPerformanceFrequency(&frequency);
    PLY_ASSERT(rc);
    PLY_UNUSED(rc);
    return (float) frequency.QuadPart;
}

s64 getUnixTimestamp() {
    FILETIME fileTime;
    ULARGE_INTEGER largeInteger;

    GetSystemTimeAsFileTime(&fileTime);
    largeInteger.LowPart = fileTime.dwLowDateTime;
    largeInteger.HighPart = fileTime.dwHighDateTime;
    return s64(largeInteger.QuadPart / 10) - 11644473600000000ll;
}

#elif defined(PLY_POSIX)

#if PLY_USE_POSIX_2008_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif

s64 getUnixTimestamp() {
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

// Based on http://howardhinnant.github.io/dateAlgorithms.html
static void setDateFromEpochDays(DateTime* dateTime, s32 days) {
    // Calculate weekday from Unix epoch days (Jan 1, 1970 was Thursday = 4)
    dateTime->weekday = (u8) (days >= -4 ? (days + 4) % 7 : (days + 5) % 7 + 6);
    // Convert from Unix epoch to civil epoch for date calculation
    days += 719468;
    s32 era = (days >= 0 ? days : days - 146096) / 146097;
    u32 doe = u32(days - era * 146097);                              // [0, 146096]
    u32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    s32 y = s32(yoe) + era * 400;
    u32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    u32 mp = (5 * doy + 2) / 153;                      // [0, 11]
    u32 d = doy - (153 * mp + 2) / 5 + 1;              // [1, 31]
    u32 m = mp + (mp < 10 ? 3 : -9);                   // [1, 12]
    dateTime->year = y + (m <= 2);
    dateTime->month = (u8) m;
    dateTime->day = (u8) d;
}

// Based on http://howardhinnant.github.io/dateAlgorithms.html
static s32 getEpochDaysFromDate(const DateTime& dateTime) {
    s32 m = dateTime.month;
    s32 y = dateTime.year - (m <= 2);
    s32 era = (y >= 0 ? y : y - 399) / 400;
    u32 yoe = u32(y - era * 400);                                         // [0, 399]
    u32 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + dateTime.day - 1; // [0, 365]
    u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                      // [0, 146096]
    return era * 146097 + doe - 719468;
}

static s64 floorDiv(s64 dividend, s64 divisor) {
    return (dividend > 0 ? dividend : dividend - divisor + 1) / divisor;
};

DateTime convertToDateTime(s64 timestamp, s16 timeZoneOffsetInMinutes) {
    // Adjust timestamp by the time zone offset
    timestamp = timestamp + s64(timeZoneOffsetInMinutes) * 60 * 1000000;

    const s64 microsecsPerDay = 86400000000ll;
    s64 days = floorDiv(timestamp, microsecsPerDay);
    s64 microsecsInDay = timestamp - (days * microsecsPerDay);

    DateTime dateTime;
    setDateFromEpochDays(&dateTime, numericCast<u32>(days));
    u32 secs = numericCast<u32>(microsecsInDay / 1000000);
    u32 minutes = secs / 60;
    u32 hours = minutes / 60;
    dateTime.hour = (u8) hours;
    dateTime.minute = (u8) (minutes - hours * 60);
    dateTime.second = (u8) (secs - minutes * 60);
    dateTime.microsecond = (u32) (microsecsInDay - u64(secs) * 1000000);

    dateTime.timeZoneOffsetInMinutes = timeZoneOffsetInMinutes;
    return dateTime;
}

s16 getLocalTimeZoneOffset() {
#if defined(PLY_WINDOWS)
    TIME_ZONE_INFORMATION tzInfo;
    DWORD result = GetTimeZoneInformation(&tzInfo);
    // Bias is in minutes, and is the offset to add to UTC to get local time
    // Windows returns it as UTC = local + bias, so we negate it
    s32 bias = tzInfo.Bias;
    if (result == TIME_ZONE_ID_DAYLIGHT) {
        bias += tzInfo.DaylightBias;
    } else if (result == TIME_ZONE_ID_STANDARD) {
        bias += tzInfo.StandardBias;
    }
    return (s16) -bias;
#elif defined(PLY_POSIX)
    time_t now = time(nullptr);
    struct tm localTm;
    localtime_r(&now, &localTm);
    // tm_gmtoff is the offset in seconds east of UTC
    return (s16) (localTm.tm_gmtoff / 60);
#else
    return 0;
#endif
}

DateTime convertToDateTime(s64 timestamp) {
    return convertToDateTime(timestamp, getLocalTimeZoneOffset());
}

s64 convertToUnixTimestamp(const DateTime& dateTime) {
    static constexpr s64 microsecsPerDay = 86400000000ll;
    s32 days = getEpochDaysFromDate(dateTime);
    s32 minutes = s32(dateTime.hour) * 60 + dateTime.minute;
    s32 seconds = minutes * 60 + dateTime.second;
    s64 localTimestamp = s64(days) * microsecsPerDay + s64(seconds) * 1000000 + dateTime.microsecond;
    // Adjust back to UTC by subtracting the time zone offset
    return localTimestamp - s64(dateTime.timeZoneOffsetInMinutes) * 60 * 1000000;
}

static String zeroPad(u32 value, u32 width) {
    String str = String::format("{}", value);
    if (str.numBytes() >= width) {
        return str;
    }
    return (String{"000000"}.left(width - str.numBytes()) + str);
}

void printDateTime(Stream& out, StringView format, const DateTime& dateTime) {
    static const char* abbreviatedWeekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* fullWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char* abbreviatedMonths[] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* fullMonths[] = {"",     "January", "February",  "March",   "April",    "May",     "June",
                                       "July", "August",  "September", "October", "November", "December"};

    for (u32 i = 0; i < format.numBytes(); i++) {
        if (format[i] == '%' && i + 1 < format.numBytes()) {
            char spec = format[i + 1];
            i++; // Skip the specifier
            switch (spec) {
                case 'a': // abbreviated weekday
                    out.write(abbreviatedWeekdays[dateTime.weekday]);
                    break;
                case 'A': // full weekday
                    out.write(fullWeekdays[dateTime.weekday]);
                    break;
                case 'b': // abbreviated month name
                    out.write(abbreviatedMonths[dateTime.month]);
                    break;
                case 'B': // full month name
                    out.write(fullMonths[dateTime.month]);
                    break;
                case 'd': // day of the month with leading zero
                    out.write(zeroPad(dateTime.day, 2));
                    break;
                case 'e': // day of the month
                    out.format("{}", dateTime.day);
                    break;
                case 'H': // hour with leading zero (24-hour clock)
                    out.write(zeroPad(dateTime.hour, 2));
                    break;
                case 'k': // hour (24-hour clock)
                    out.format("{}", dateTime.hour);
                    break;
                case 'l': { // hour (12-hour clock)
                    u8 hour12 = dateTime.hour % 12;
                    if (hour12 == 0)
                        hour12 = 12;
                    out.format("{}", hour12);
                    break;
                }
                case 'm': // month with leading zero
                    out.write(zeroPad(dateTime.month, 2));
                    break;
                case 'M': // minute with leading zero
                    out.write(zeroPad(dateTime.minute, 2));
                    break;
                case 'p': // AM or PM
                    out.write(dateTime.hour < 12 ? "AM" : "PM");
                    break;
                case 'P': // am or pm
                    out.write(dateTime.hour < 12 ? "am" : "pm");
                    break;
                case 'S': // second with leading zero
                    out.write(zeroPad(dateTime.second, 2));
                    break;
                case 'y': // two-digit year
                    out.write(zeroPad(dateTime.year % 100, 2));
                    break;
                case 'Y': // year
                    out.format("{}", dateTime.year);
                    break;
                case 'L': // millisecond with leading zeros
                    out.write(zeroPad(dateTime.microsecond / 1000, 3));
                    break;
                case 'R': // microsecond with leading zeros
                    out.write(zeroPad(dateTime.microsecond, 6));
                    break;
                case 'Z': { // signed time zone offset
                    s16 offset = dateTime.timeZoneOffsetInMinutes;
                    char sign = offset >= 0 ? '+' : '-';
                    if (offset < 0)
                        offset = -offset;
                    s16 hours = offset / 60;
                    s16 mins = offset % 60;
                    out.write(sign);
                    out.write(zeroPad(hours, 2));
                    out.write(':');
                    out.write(zeroPad(mins, 2));
                    break;
                }
                case '%': // literal %
                    out.write('%');
                    break;
                default:
                    // Unknown specifier, output as-is
                    out.write('%');
                    out.write(spec);
                    break;
            }
        } else {
            out.write(format[i]);
        }
    }
}

//  ▄▄▄▄▄                    ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀█▄  ▄▄▄██ ██  ██ ██  ██ ██  ██ ██ ██ ██
//  ██  ██ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄█▀ ██ ██ ██
//

Random::Random() {
    // Seed using misc. information from the environment
    u64 t = getUnixTimestamp();
    t = shuffleBits(t);
    s[0] = shuffleBits(t | 1);

    t = getCpuTicks();
    t = shuffleBits(t) + (shuffleBits(getCurrentThreadId()) << 1);
    s[1] = shuffleBits(t | 1);

    for (u32 i = 0; i < 10; i++) {
        generateU64();
    }
}

Random::Random(u64 seed) {
    s[0] = shuffleBits(seed + 1);
    s[1] = shuffleBits(s[0] + 1);
    generateU64();
    generateU64();
}

static inline u64 rotl(const u64 x, int k) {
    return (x << k) | (x >> (64 - k));
}

u64 Random::generateU64() {
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

#if defined(PLY_WINDOWS)

DWORD WINAPI threadEntry(LPVOID param) {
    Functor<void()>* entry = static_cast<Functor<void()>*>(param);
    (*entry)();
    Heap::destroy(entry);
    return 0;
}

#elif defined(PLY_POSIX)

void* threadEntry(void* arg) {
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

Atomic<uptr> VirtualMemory::totalReservedBytes = 0;
Atomic<uptr> VirtualMemory::totalCommittedBytes = 0;

#if defined(PLY_WINDOWS)

//--------------------------------------------
// Windows
//--------------------------------------------

VirtualMemory::Properties VirtualMemory::getProperties() {
    static VirtualMemory::Properties props = []() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        PLY_ASSERT(isPowerOf2((u32) sysInfo.dwAllocationGranularity));
        PLY_ASSERT(isPowerOf2((u32) sysInfo.dwPageSize));
        return VirtualMemory::Properties{sysInfo.dwAllocationGranularity, sysInfo.dwPageSize};
    }();
    return props;
}

VirtualMemory::SystemStats VirtualMemory::getSystemStats() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc));

    VirtualMemory::SystemStats usageStats;
    usageStats.privateUsage = pmc.PrivateUsage;
    usageStats.workingSetSize = pmc.WorkingSetSize;
    return usageStats;
}

void* VirtualMemory::reserveRegion(uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().regionAlignment));

    void* addr = VirtualAlloc(0, (SIZE_T) numBytes, MEM_RESERVE, PAGE_READWRITE);
    if (addr == NULL)
        return nullptr;
    VirtualMemory::totalReservedBytes.fetchAdd(numBytes, Relaxed);
    return addr;
}

void VirtualMemory::unreserveRegion(void* addr, uptr numReservedBytes, uptr numCommittedBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().regionAlignment));
    PLY_ASSERT(isAlignedToPowerOf2(numReservedBytes, VirtualMemory::getProperties().regionAlignment));
    PLY_ASSERT(isAlignedToPowerOf2(numCommittedBytes, VirtualMemory::getProperties().pageSize));

#if defined(PLY_WITH_ASSERTS)
    MEMORY_BASIC_INFORMATION memInfo;
    SIZE_T rc = VirtualQuery(addr, &memInfo, sizeof(memInfo));
    PLY_ASSERT(rc != 0);
    PLY_UNUSED(rc);
    PLY_ASSERT(memInfo.BaseAddress == addr);
    PLY_ASSERT(memInfo.AllocationBase == addr);
    // The entire address space range must be reserved as one block:
    PLY_ASSERT(memInfo.RegionSize <= numReservedBytes);
#endif

    BOOL rc2 = VirtualFree(addr, 0, MEM_RELEASE);
    PLY_ASSERT(rc2);
    PLY_UNUSED(rc2);
    VirtualMemory::totalReservedBytes.fetchSub(numReservedBytes, Relaxed);
    VirtualMemory::totalCommittedBytes.fetchSub(numCommittedBytes, Relaxed);
}

void VirtualMemory::commitPages(void* addr, uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().pageSize));
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().pageSize));

    LPVOID result = VirtualAlloc(addr, (SIZE_T) numBytes, MEM_COMMIT, PAGE_READWRITE);
    PLY_ASSERT(result != NULL); // Failure is considered fatal
    PLY_UNUSED(result);
    VirtualMemory::totalCommittedBytes.fetchAdd(numBytes, Relaxed);
}

void VirtualMemory::decommitPages(void* addr, uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().pageSize));
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().pageSize));

    BOOL rc = VirtualFree(addr, numBytes, MEM_DECOMMIT);
    PLY_ASSERT(rc);
    PLY_UNUSED(rc);
    VirtualMemory::totalCommittedBytes.fetchSub(numBytes, Relaxed);
}

void* VirtualMemory::allocRegion(uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().regionAlignment));

    void* addr = VirtualAlloc(0, (SIZE_T) numBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (addr == NULL)
        return nullptr;
    VirtualMemory::totalReservedBytes.fetchAdd(numBytes, Relaxed);
    VirtualMemory::totalCommittedBytes.fetchAdd(numBytes, Relaxed);
    return addr;
}

void VirtualMemory::freeRegion(void* addr, uptr numBytes) {
    VirtualMemory::unreserveRegion(addr, numBytes, numBytes);
}

#elif defined(PLY_POSIX)

//--------------------------------------------
// POSIX
//--------------------------------------------

VirtualMemory::Properties VirtualMemory::getProperties() {
    static VirtualMemory::Properties props = []() {
        long result = sysconf(_SC_PAGE_SIZE);
        PLY_ASSERT(isPowerOf2((u64) result));
        return VirtualMemory::Properties{(uptr) result, (uptr) result};
    }();
    return props;
}

VirtualMemory::SystemStats VirtualMemory::getSystemStats() {
    VirtualMemory::SystemStats usageStats;

#if defined(PLY_APPLE)
    struct mach_task_basic_info taskInfoData;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &taskInfoData, &count) == KERN_SUCCESS) {
        usageStats.virtualSize = taskInfoData.virtual_size;
        usageStats.residentSize = taskInfoData.resident_size;
    }
#else
    Stream in = Filesystem::openBinaryForRead("/proc/self/statm");
    if (in) {
        u64 vmPages = readU64FromText(in);
        skipWhitespace(in);
        u64 rssPages = readU64FromText(in);
        uptr pageSize = VirtualMemory::getProperties().pageSize;
        usageStats.virtualSize = vmPages * pageSize;
        usageStats.residentSize = rssPages * pageSize;
    }
#endif

    return usageStats;
}

void* VirtualMemory::reserveRegion(uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().regionAlignment));

    void* addr = mmap(0, numBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        return nullptr;
    VirtualMemory::totalReservedBytes.fetchAdd(numBytes, Relaxed);
    return addr;
}

void VirtualMemory::unreserveRegion(void* addr, uptr numReservedBytes, uptr numCommittedBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().regionAlignment));
    PLY_ASSERT(isAlignedToPowerOf2(numReservedBytes, VirtualMemory::getProperties().regionAlignment));
    PLY_ASSERT(isAlignedToPowerOf2(numCommittedBytes, VirtualMemory::getProperties().pageSize));

    int rc = munmap(addr, numReservedBytes);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    VirtualMemory::totalReservedBytes.fetchSub(numReservedBytes, Relaxed);
    VirtualMemory::totalCommittedBytes.fetchSub(numCommittedBytes, Relaxed);
}

void VirtualMemory::commitPages(void* addr, uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().pageSize));
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().pageSize));

    int rc = mprotect(addr, numBytes, PROT_READ | PROT_WRITE);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    VirtualMemory::totalCommittedBytes.fetchAdd(numBytes, Relaxed);
}

void VirtualMemory::decommitPages(void* addr, uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2((uptr) addr, VirtualMemory::getProperties().pageSize));
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().pageSize));

    int rc = madvise(addr, numBytes, MADV_DONTNEED);
    PLY_ASSERT(rc == 0);
    rc = mprotect(addr, numBytes, PROT_NONE);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    VirtualMemory::totalCommittedBytes.fetchSub(numBytes, Relaxed);
}

void* VirtualMemory::allocRegion(uptr numBytes) {
    PLY_ASSERT(isAlignedToPowerOf2(numBytes, VirtualMemory::getProperties().regionAlignment));

    void* addr = mmap(0, numBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        return nullptr;
    VirtualMemory::totalReservedBytes.fetchAdd(numBytes, Relaxed);
    VirtualMemory::totalCommittedBytes.fetchAdd(numBytes, Relaxed);
    return addr;
}

void VirtualMemory::freeRegion(void* addr, uptr numBytes) {
    VirtualMemory::unreserveRegion(addr, numBytes, numBytes);
}

#endif

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

Functor<void()> Heap::outOfMemoryHandler;

void Heap::setOutOfMemoryHandler(Functor<void()> handler) {
    outOfMemoryHandler = std::move(handler);
}

#if !PLY_USE_DLMALLOC

// Implements the bespoke heap allocator used by ply::Heap.
class HeapImpl {
private:
    //--------------------------------------------------------------------
    // 1. Constants
    //--------------------------------------------------------------------

    // Small bins use 8-byte size classes for chunk sizes below 256 bytes.
    static constexpr u32 NumSmallBins = 32;
    // Tree bins store larger free chunks with size-ordered search trees.
    static constexpr u32 NumTreeBins = 32;
    // The width of each small-bin size class.
    static constexpr uptr SmallBinStep = 8;
    // The largest chunk size routed through small bins.
    static constexpr uptr SmallBinLimit = NumSmallBins * SmallBinStep;
    // All chunk payload addresses are aligned to 16 bytes.
    static constexpr uptr ChunkAlignment = 16;
    // Boundary-tag flag marking an allocated chunk.
    static constexpr uptr InUseBit = 1;
    // Boundary-tag flag marking that the previous chunk is allocated.
    static constexpr uptr PrevInUseBit = 2;
    // Boundary-tag flag marking a chunk directly mapped from the OS.
    static constexpr uptr DirectMappedBit = 4;
    // Mask of all boundary-tag flags.
    static constexpr uptr FlagMask = InUseBit | PrevInUseBit | DirectMappedBit;
    // Requests at or above this chunk size are mapped directly from the OS.
    static constexpr uptr DirectMapThreshold = 256 * 1024;
    // Default size used when provisioning a new virtual-memory segment.
    static constexpr uptr DefaultSegmentBytes = 1024 * 1024;

    //--------------------------------------------------------------------
    // 2. Types
    //--------------------------------------------------------------------

    // Header words common to regular chunks and fence-post sentinels.
    struct ChunkHeader {
        uptr prevFoot = 0;
        uptr head = 0;
    };

    // A heap chunk with boundary tags and intrusive free-list/tree metadata.
    struct Chunk : ChunkHeader {
        union {
            struct {
                Chunk* smallPrev;
                Chunk* smallNext;
            };
            struct {
                Chunk* treeParent;
                Chunk* treeLeft;
                Chunk* treeRight;
            };
        };

        // Returns the chunk size encoded in the boundary tag.
        uptr getSize() const {
            return this->head & ~FlagMask;
        }
        // Returns true if the chunk is currently allocated.
        bool getInUse() const {
            return (this->head & InUseBit) != 0;
        }
        // Returns true if the previous chunk is currently allocated.
        bool getPrevInUse() const {
            return (this->head & PrevInUseBit) != 0;
        }
        // Returns true if the chunk came from direct virtual-memory mapping.
        bool getDirectMapped() const {
            return (this->head & DirectMappedBit) != 0;
        }
        // Returns the user pointer associated with this chunk.
        void* memFromChunk() {
            return (void*) (((u8*) this) + sizeof(ChunkHeader));
        }
        // Returns the immutable user pointer associated with this chunk.
        const void* memFromChunk() const {
            return (const void*) (((const u8*) this) + sizeof(ChunkHeader));
        }
        // Converts a user pointer back to its owning chunk.
        static Chunk* chunkFromMem(void* mem) {
            return (Chunk*) (((u8*) mem) - sizeof(ChunkHeader));
        }
        // Returns the next chunk in memory.
        Chunk* nextChunk() {
            return (Chunk*) (((u8*) this) + this->getSize());
        }
        // Returns the next chunk header in memory.
        ChunkHeader* nextHeader() {
            return (ChunkHeader*) (((u8*) this) + this->getSize());
        }
        // Returns the previous chunk in memory. Caller must ensure prev is free.
        Chunk* prevChunk() {
            return (Chunk*) (((u8*) this) - this->prevFoot);
        }
        // Sets the full boundary tag for this chunk.
        void setSizeAndFlags(uptr size, bool inUse, bool prevInUse, bool directMapped = false) {
            this->head =
                size | (inUse ? InUseBit : 0) | (prevInUse ? PrevInUseBit : 0) | (directMapped ? DirectMappedBit : 0);
        }
    };

    // Tracks a reserved/committed virtual-memory segment used for regular chunks.
    struct Segment {
        Segment* prev = nullptr;
        Segment* next = nullptr;
        uptr numBytes = 0;
        Chunk* firstChunk = nullptr;
        ChunkHeader* fence = nullptr;
    };

    // Tracks a large direct-mapped chunk and its backing virtual-memory mapping.
    struct DirectChunk {
        DirectChunk* prev = nullptr;
        DirectChunk* next = nullptr;
        uptr mappingSize = 0;
        Chunk chunk;
    };

    // Tracks metadata for pointers returned from Heap::allocAligned.
    struct AlignedAllocHeader {
        AlignedAllocHeader* prev = nullptr;
        AlignedAllocHeader* next = nullptr;
        void* alignedPtr = nullptr;
        void* basePtr = nullptr;
        uptr requestedBytes = 0;
    };

    //--------------------------------------------------------------------
    // 3. Chunk headers
    //--------------------------------------------------------------------

    // Returns the aligned minimum chunk size that can hold free-chunk metadata.
    static uptr minChunkSize() {
        return uptr(alignToPowerOf2((u64) sizeof(Chunk), (u64) ChunkAlignment));
    }

    // Converts a chunk size request into an aligned chunk size including header.
    static uptr requestToChunkSize(uptr numBytes) {
        if (numBytes == 0) {
            numBytes = 1;
        }
        if (numBytes > getMaxValue<uptr>() - sizeof(ChunkHeader) - ChunkAlignment) {
            return 0;
        }
        uptr chunkSize = numBytes + sizeof(ChunkHeader);
        chunkSize = uptr(alignToPowerOf2((u64) chunkSize, (u64) ChunkAlignment));
        if (chunkSize < minChunkSize()) {
            chunkSize = minChunkSize();
        }
        return chunkSize;
    }

    // Returns true when a chunk should be managed by small bins.
    static bool isSmallChunk(uptr chunkSize) {
        return chunkSize < SmallBinLimit;
    }

    // Returns the small-bin index for a given chunk size.
    static u32 smallBinIndex(uptr chunkSize) {
        PLY_ASSERT(chunkSize < SmallBinLimit);
        return numericCast<u32>(chunkSize / SmallBinStep);
    }

    // Returns floor(log2(value)) for non-zero inputs.
    static u32 floorLog2(uptr value) {
        PLY_ASSERT(value > 0);
        u32 log2 = 0;
        while (value >>= 1) {
            log2++;
        }
        return log2;
    }

    // Returns the tree-bin index for a chunk size of at least 256 bytes.
    static u32 treeBinIndex(uptr chunkSize) {
        PLY_ASSERT(chunkSize >= SmallBinLimit);
        u32 lg = floorLog2(chunkSize);
        if (lg <= 8) {
            return 0;
        }
        u32 idx = (lg - 8) * 2;
        idx += numericCast<u32>((chunkSize >> (lg - 1)) & 1);
        if (idx >= NumTreeBins) {
            idx = NumTreeBins - 1;
        }
        return idx;
    }

    // Returns the first set-bit index of a non-zero bitmap.
    static u32 firstSetBit(u32 bits) {
        PLY_ASSERT(bits != 0);
        u32 idx = 0;
        while ((bits & 1) == 0) {
            bits >>= 1;
            idx++;
        }
        return idx;
    }

    // Writes boundary tags for a free chunk and updates its successor's back-link.
    static void writeFreeChunkHeader(Chunk* chunk, uptr chunkSize, bool prevInUse) {
        chunk->setSizeAndFlags(chunkSize, false, prevInUse, false);
        ChunkHeader* next = chunk->nextHeader();
        next->prevFoot = chunkSize;
        next->head &= ~PrevInUseBit;
    }

    // Writes boundary tags for an allocated chunk and updates successor state.
    static void writeInUseChunkHeader(Chunk* chunk, uptr chunkSize, bool prevInUse) {
        chunk->setSizeAndFlags(chunkSize, true, prevInUse, false);
        ChunkHeader* next = chunk->nextHeader();
        next->prevFoot = chunkSize;
        next->head |= PrevInUseBit;
    }

    //--------------------------------------------------------------------
    // 4. Global heap state
    //--------------------------------------------------------------------

    // Synchronizes all allocator state.
    Mutex mutex;
    // Lazily initializes VM settings and initial segment metadata.
    bool initialized = false;
    // Caches system page and region alignment details.
    VirtualMemory::Properties vmProps;
    // Head of the linked list of regular VM segments.
    Segment* segmentHead = nullptr;
    // Tail of the linked list of regular VM segments.
    Segment* segmentTail = nullptr;
    // Heads of small-bin free lists.
    Chunk* smallBins[NumSmallBins] = {};
    // Roots of size-ordered tree bins.
    Chunk* treeBins[NumTreeBins] = {};
    // Bitmask indicating non-empty small bins.
    u32 smallMap = 0;
    // Bitmask indicating non-empty tree bins.
    u32 treeMap = 0;
    // Designated-victim free chunk kept out of bins.
    Chunk* designatedVictim = nullptr;
    // Top/wilderness free chunk at the end of one segment.
    Chunk* top = nullptr;
    // Linked list of direct-mapped large chunks.
    DirectChunk* directHead = nullptr;
    // Linked list of explicitly aligned allocations.
    AlignedAllocHeader* alignedHead = nullptr;
    // Cached allocator counters returned by Heap::getStats().
    Heap::Stats stats;

    //--------------------------------------------------------------------
    // 5. Small bin functions
    //--------------------------------------------------------------------

    // Inserts a free chunk into the appropriate small bin.
    void insertSmallChunk(Chunk* chunk) {
        PLY_ASSERT(isSmallChunk(chunk->getSize()));
        u32 idx = smallBinIndex(chunk->getSize());
        chunk->smallPrev = nullptr;
        chunk->smallNext = this->smallBins[idx];
        if (this->smallBins[idx]) {
            this->smallBins[idx]->smallPrev = chunk;
        }
        this->smallBins[idx] = chunk;
        this->smallMap |= (1u << idx);
    }

    // Removes a free chunk from its small-bin linked list.
    void unlinkSmallChunk(Chunk* chunk) {
        PLY_ASSERT(isSmallChunk(chunk->getSize()));
        u32 idx = smallBinIndex(chunk->getSize());
        if (chunk->smallPrev) {
            chunk->smallPrev->smallNext = chunk->smallNext;
        } else {
            PLY_ASSERT(this->smallBins[idx] == chunk);
            this->smallBins[idx] = chunk->smallNext;
        }
        if (chunk->smallNext) {
            chunk->smallNext->smallPrev = chunk->smallPrev;
        }
        if (!this->smallBins[idx]) {
            this->smallMap &= ~(1u << idx);
        }
        chunk->smallPrev = nullptr;
        chunk->smallNext = nullptr;
    }

    //--------------------------------------------------------------------
    // 6. Tree bin functions
    //--------------------------------------------------------------------

    // Inserts a free chunk into the appropriate tree bin.
    void insertTreeChunk(Chunk* chunk) {
        PLY_ASSERT(!isSmallChunk(chunk->getSize()));
        u32 idx = treeBinIndex(chunk->getSize());
        Chunk*& root = this->treeBins[idx];
        chunk->treeParent = nullptr;
        chunk->treeLeft = nullptr;
        chunk->treeRight = nullptr;
        if (!root) {
            root = chunk;
            this->treeMap |= (1u << idx);
            return;
        }
        Chunk* node = root;
        for (;;) {
            bool goLeft =
                (chunk->getSize() < node->getSize()) || ((chunk->getSize() == node->getSize()) && (chunk < node));
            Chunk*& child = goLeft ? node->treeLeft : node->treeRight;
            if (!child) {
                child = chunk;
                chunk->treeParent = node;
                break;
            }
            node = child;
        }
        this->treeMap |= (1u << idx);
    }

    // Removes a free chunk from its tree bin.
    void unlinkTreeChunk(Chunk* chunk) {
        PLY_ASSERT(!isSmallChunk(chunk->getSize()));
        u32 idx = treeBinIndex(chunk->getSize());
        Chunk*& root = this->treeBins[idx];
        Chunk* replacement = nullptr;
        if (!chunk->treeLeft) {
            replacement = chunk->treeRight;
        } else if (!chunk->treeRight) {
            replacement = chunk->treeLeft;
        } else {
            Chunk* successor = chunk->treeRight;
            while (successor->treeLeft) {
                successor = successor->treeLeft;
            }
            if (successor->treeParent != chunk) {
                Chunk* successorRight = successor->treeRight;
                successor->treeParent->treeLeft = successorRight;
                if (successorRight) {
                    successorRight->treeParent = successor->treeParent;
                }
                successor->treeRight = chunk->treeRight;
                successor->treeRight->treeParent = successor;
            }
            successor->treeLeft = chunk->treeLeft;
            successor->treeLeft->treeParent = successor;
            replacement = successor;
        }

        if (replacement) {
            replacement->treeParent = chunk->treeParent;
        }
        if (!chunk->treeParent) {
            root = replacement;
        } else if (chunk->treeParent->treeLeft == chunk) {
            chunk->treeParent->treeLeft = replacement;
        } else {
            chunk->treeParent->treeRight = replacement;
        }
        chunk->treeParent = nullptr;
        chunk->treeLeft = nullptr;
        chunk->treeRight = nullptr;
        if (!root) {
            this->treeMap &= ~(1u << idx);
        }
    }

    // Removes a free chunk from whichever bin structure currently owns it.
    void unlinkBinnedChunk(Chunk* chunk) {
        if (isSmallChunk(chunk->getSize())) {
            unlinkSmallChunk(chunk);
        } else {
            unlinkTreeChunk(chunk);
        }
    }

    // Inserts a free chunk into the appropriate small or tree bin.
    void insertBinnedChunk(Chunk* chunk) {
        if (isSmallChunk(chunk->getSize())) {
            insertSmallChunk(chunk);
        } else {
            insertTreeChunk(chunk);
        }
    }

    // Returns the best-fit free chunk from tree bins for a minimum chunk size.
    Chunk* findBestTreeChunk(uptr chunkSize) {
        Chunk* best = nullptr;
        uptr bestSize = getMaxValue<uptr>();
        u32 startBin = isSmallChunk(chunkSize) ? 0u : treeBinIndex(chunkSize);
        for (u32 i = startBin; i < NumTreeBins; i++) {
            Chunk* node = this->treeBins[i];
            if (!node) {
                continue;
            }
            Chunk* candidate = nullptr;
            while (node) {
                if (node->getSize() >= chunkSize) {
                    candidate = node;
                    node = node->treeLeft;
                } else {
                    node = node->treeRight;
                }
            }
            if (candidate &&
                (candidate->getSize() < bestSize || (candidate->getSize() == bestSize && candidate < best))) {
                best = candidate;
                bestSize = candidate->getSize();
                if (bestSize == chunkSize) {
                    break;
                }
            }
        }
        return best;
    }

    //--------------------------------------------------------------------
    // 7. Virtual memory segment management
    //--------------------------------------------------------------------

    // Initializes VM configuration and allocator bookkeeping.
    void ensureInitialized() {
        if (this->initialized) {
            return;
        }
        this->vmProps = VirtualMemory::getProperties();
        this->initialized = true;
    }

    // Returns the segment that owns a regular chunk pointer.
    Segment* findSegmentForChunk(const Chunk* chunk) const {
        const u8* ptr = (const u8*) chunk;
        for (Segment* seg = this->segmentHead; seg; seg = seg->next) {
            const u8* begin = (const u8*) seg;
            const u8* end = begin + seg->numBytes;
            if (ptr >= begin && ptr < end) {
                return seg;
            }
        }
        return nullptr;
    }

    // Provisions a new VM segment and installs its free space as the top chunk.
    bool addSegment(uptr minimumTopBytes) {
        uptr neededBytes = sizeof(Segment) + minimumTopBytes + sizeof(ChunkHeader) + ChunkAlignment;
        uptr regionBytes = max(DefaultSegmentBytes, neededBytes);
        regionBytes = uptr(alignToPowerOf2((u64) regionBytes, (u64) this->vmProps.regionAlignment));
        void* region = VirtualMemory::allocRegion(regionBytes);
        if (!region) {
            return false;
        }

        Segment* segment = (Segment*) region;
        segment->prev = this->segmentTail;
        segment->next = nullptr;
        segment->numBytes = regionBytes;
        if (this->segmentTail) {
            this->segmentTail->next = segment;
        } else {
            this->segmentHead = segment;
        }
        this->segmentTail = segment;

        u8* chunkStart = (u8*) region + sizeof(Segment);
        chunkStart = (u8*) (uptr(alignToPowerOf2((u64) (uptr) chunkStart, (u64) ChunkAlignment)));
        ChunkHeader* fence = (ChunkHeader*) (((u8*) region) + regionBytes - sizeof(ChunkHeader));
        uptr topBytes = uptr((u8*) fence - chunkStart);
        if (topBytes < minChunkSize()) {
            if (segment->prev) {
                segment->prev->next = nullptr;
            } else {
                this->segmentHead = nullptr;
            }
            this->segmentTail = segment->prev;
            VirtualMemory::freeRegion(region, regionBytes);
            return false;
        }

        Chunk* newTop = (Chunk*) chunkStart;
        writeFreeChunkHeader(newTop, topBytes, true);
        fence->head = InUseBit;

        segment->firstChunk = newTop;
        segment->fence = fence;

        if (this->top) {
            if (this->designatedVictim) {
                insertBinnedChunk(this->designatedVictim);
            }
            this->designatedVictim = this->top;
        }
        this->top = newTop;
        this->stats.totalSystemMemoryUsed += regionBytes;
        return true;
    }

    // Releases a fully free segment back to the operating system.
    bool releaseSegmentIfCompletelyFree(Chunk* chunk) {
        Segment* segment = findSegmentForChunk(chunk);
        if (!segment) {
            return false;
        }
        if (chunk != segment->firstChunk) {
            return false;
        }
        if (chunk->nextHeader() != segment->fence) {
            return false;
        }

        if (this->top == chunk) {
            this->top = nullptr;
        }
        if (this->designatedVictim == chunk) {
            this->designatedVictim = nullptr;
        }

        if (segment->prev) {
            segment->prev->next = segment->next;
        } else {
            this->segmentHead = segment->next;
        }
        if (segment->next) {
            segment->next->prev = segment->prev;
        } else {
            this->segmentTail = segment->prev;
        }

        this->stats.totalSystemMemoryUsed -= segment->numBytes;
        VirtualMemory::freeRegion(segment, segment->numBytes);
        return true;
    }

    //--------------------------------------------------------------------
    // 8. Large chunk functions that use the system's virtual memory API directly
    //--------------------------------------------------------------------

    // Converts an in-use direct-mapped chunk pointer to its direct-map header.
    static DirectChunk* directFromChunk(Chunk* chunk) {
        return (DirectChunk*) (((u8*) chunk) - PLY_OFFSET_OF(DirectChunk, chunk));
    }

    // Allocates a large chunk directly from the operating system.
    void* allocateDirectMappedChunk(uptr chunkSize) {
        uptr payloadBytes = chunkSize - sizeof(ChunkHeader);
        uptr mapBytes = sizeof(DirectChunk) + payloadBytes;
        mapBytes = uptr(alignToPowerOf2((u64) mapBytes, (u64) this->vmProps.regionAlignment));
        DirectChunk* direct = (DirectChunk*) VirtualMemory::allocRegion(mapBytes);
        if (!direct) {
            return nullptr;
        }
        direct->mappingSize = mapBytes;
        direct->prev = nullptr;
        direct->next = this->directHead;
        if (this->directHead) {
            this->directHead->prev = direct;
        }
        this->directHead = direct;

        direct->chunk.setSizeAndFlags(chunkSize, true, true, true);
        this->stats.totalBytesConsumed += chunkSize;
        this->stats.totalSystemMemoryUsed += mapBytes;
        return direct->chunk.memFromChunk();
    }

    // Frees a direct-mapped chunk and releases its virtual-memory mapping.
    void freeDirectMappedChunk(Chunk* chunk) {
        DirectChunk* direct = directFromChunk(chunk);
        if (direct->prev) {
            direct->prev->next = direct->next;
        } else {
            this->directHead = direct->next;
        }
        if (direct->next) {
            direct->next->prev = direct->prev;
        }
        this->stats.totalBytesConsumed -= chunk->getSize();
        this->stats.totalSystemMemoryUsed -= direct->mappingSize;
        VirtualMemory::freeRegion(direct, direct->mappingSize);
    }

    //--------------------------------------------------------------------
    // 9. Internal allocation and free functions that wrap all of the above
    //--------------------------------------------------------------------

    // Looks up aligned-allocation metadata for an exact user pointer.
    AlignedAllocHeader* findAlignedHeader(void* ptr) const {
        for (AlignedAllocHeader* header = this->alignedHead; header; header = header->next) {
            if (header->alignedPtr == ptr) {
                return header;
            }
        }
        return nullptr;
    }

    // Adds a header to the aligned-allocation metadata list.
    void linkAlignedHeader(AlignedAllocHeader* header) {
        header->prev = nullptr;
        header->next = this->alignedHead;
        if (this->alignedHead) {
            this->alignedHead->prev = header;
        }
        this->alignedHead = header;
    }

    // Removes a header from the aligned-allocation metadata list.
    void unlinkAlignedHeader(AlignedAllocHeader* header) {
        if (header->prev) {
            header->prev->next = header->next;
        } else {
            this->alignedHead = header->next;
        }
        if (header->next) {
            header->next->prev = header->prev;
        }
        header->prev = nullptr;
        header->next = nullptr;
    }

    // Places a single free chunk in designated-victim storage.
    void storeDesignatedVictim(Chunk* chunk) {
        PLY_ASSERT(!chunk->getInUse());
        if (this->designatedVictim) {
            insertBinnedChunk(this->designatedVictim);
        }
        this->designatedVictim = chunk;
    }

    // Splits a free chunk for allocation and routes any remainder to designated-victim storage.
    Chunk* useFreeChunk(Chunk* chunk, uptr neededSize) {
        uptr chunkBytes = chunk->getSize();
        bool prevInUse = chunk->getPrevInUse();
        uptr remainderBytes = chunkBytes - neededSize;
        if (remainderBytes >= minChunkSize()) {
            Chunk* remainder = (Chunk*) (((u8*) chunk) + neededSize);
            writeFreeChunkHeader(remainder, remainderBytes, true);
            writeInUseChunkHeader(chunk, neededSize, prevInUse);
            storeDesignatedVictim(remainder);
        } else {
            writeInUseChunkHeader(chunk, chunkBytes, prevInUse);
        }
        this->stats.totalBytesConsumed += chunk->getSize();
        return chunk;
    }

    // Attempts to satisfy an allocation from small bins.
    Chunk* takeSmallBinChunk(uptr neededSize) {
        if (!isSmallChunk(neededSize)) {
            return nullptr;
        }
        u32 idx = smallBinIndex(neededSize);
        u32 bits = this->smallMap & (~0u << idx);
        if (!bits) {
            return nullptr;
        }
        u32 chosen = firstSetBit(bits);
        Chunk* chunk = this->smallBins[chosen];
        unlinkSmallChunk(chunk);
        return useFreeChunk(chunk, neededSize);
    }

    // Attempts to satisfy an allocation from the designated-victim chunk.
    Chunk* takeDesignatedVictimChunk(uptr neededSize) {
        if (!this->designatedVictim || (this->designatedVictim->getSize() < neededSize)) {
            return nullptr;
        }
        Chunk* chunk = this->designatedVictim;
        this->designatedVictim = nullptr;
        return useFreeChunk(chunk, neededSize);
    }

    // Attempts to satisfy an allocation from tree bins.
    Chunk* takeTreeBinChunk(uptr neededSize) {
        Chunk* chunk = findBestTreeChunk(neededSize);
        if (!chunk) {
            return nullptr;
        }
        unlinkTreeChunk(chunk);
        return useFreeChunk(chunk, neededSize);
    }

    // Allocates from the wilderness chunk, provisioning a new segment if needed.
    Chunk* takeTopChunk(uptr neededSize) {
        while (!this->top || (this->top->getSize() < neededSize)) {
            if (!addSegment(neededSize)) {
                return nullptr;
            }
        }

        Chunk* chunk = this->top;
        uptr topBytes = chunk->getSize();
        bool prevInUse = chunk->getPrevInUse();
        uptr remainderBytes = topBytes - neededSize;
        if (remainderBytes >= minChunkSize()) {
            Chunk* remainder = (Chunk*) (((u8*) chunk) + neededSize);
            writeFreeChunkHeader(remainder, remainderBytes, true);
            writeInUseChunkHeader(chunk, neededSize, prevInUse);
            this->top = remainder;
        } else {
            writeInUseChunkHeader(chunk, topBytes, prevInUse);
            this->top = nullptr;
        }
        this->stats.totalBytesConsumed += chunk->getSize();
        return chunk;
    }

    // Allocates a regular (non-direct-mapped) chunk using bins, DV, and top.
    Chunk* allocRegularChunk(uptr neededSize) {
        Chunk* chunk = takeSmallBinChunk(neededSize);
        if (!chunk) {
            chunk = takeDesignatedVictimChunk(neededSize);
        }
        if (!chunk) {
            chunk = takeTreeBinChunk(neededSize);
        }
        if (!chunk) {
            chunk = takeTopChunk(neededSize);
        }
        return chunk;
    }

    // Coalesces and stores a free chunk after deallocation or shrink-split.
    void recycleFreeChunk(Chunk* chunk) {
        Chunk* base = chunk;
        uptr chunkBytes = chunk->getSize();

        if (!base->getPrevInUse()) {
            Chunk* prev = base->prevChunk();
            if (prev == this->designatedVictim) {
                this->designatedVictim = nullptr;
            } else if (prev == this->top) {
                this->top = nullptr;
            } else {
                unlinkBinnedChunk(prev);
            }
            chunkBytes += prev->getSize();
            base = prev;
        }

        ChunkHeader* nextHeader = (ChunkHeader*) (((u8*) base) + chunkBytes);
        if ((nextHeader->head & InUseBit) == 0) {
            Chunk* next = (Chunk*) nextHeader;
            if (next == this->designatedVictim) {
                this->designatedVictim = nullptr;
            } else if (next == this->top) {
                this->top = nullptr;
            } else {
                unlinkBinnedChunk(next);
            }
            chunkBytes += next->getSize();
        }

        writeFreeChunkHeader(base, chunkBytes, base->getPrevInUse());

        if (releaseSegmentIfCompletelyFree(base)) {
            return;
        }
        if (this->top && (base->nextHeader() == findSegmentForChunk(base)->fence) && (this->top != base)) {
            storeDesignatedVictim(base);
            return;
        }
        if (!this->top && (base->nextHeader() == findSegmentForChunk(base)->fence)) {
            this->top = base;
            return;
        }
        if (base == this->top) {
            return;
        }
        storeDesignatedVictim(base);
    }

    // Allocates a chunk with default alignment from all allocator structures.
    void* allocLocked(uptr numBytes) {
        ensureInitialized();
        uptr neededSize = requestToChunkSize(numBytes);
        if (neededSize == 0) {
            return nullptr;
        }
        if (neededSize >= DirectMapThreshold) {
            return allocateDirectMappedChunk(neededSize);
        }
        Chunk* chunk = allocRegularChunk(neededSize);
        return chunk ? chunk->memFromChunk() : nullptr;
    }

    // Frees a chunk allocated from either segment storage or direct mapping.
    void freeLocked(void* ptr) {
        if (!ptr) {
            return;
        }
        if (AlignedAllocHeader* header = findAlignedHeader(ptr)) {
            ptr = header->basePtr;
            unlinkAlignedHeader(header);
        }
        Chunk* chunk = Chunk::chunkFromMem(ptr);
        if (chunk->getDirectMapped()) {
            freeDirectMappedChunk(chunk);
            return;
        }
        this->stats.totalBytesConsumed -= chunk->getSize();
        writeFreeChunkHeader(chunk, chunk->getSize(), chunk->getPrevInUse());
        recycleFreeChunk(chunk);
    }

    // Reallocates a chunk while preserving existing contents.
    void* reallocLocked(void* ptr, uptr numBytes) {
        if (!ptr) {
            return allocLocked(numBytes);
        }
        if (numBytes == 0) {
            freeLocked(ptr);
            return nullptr;
        }

        if (AlignedAllocHeader* header = findAlignedHeader(ptr)) {
            void* newPtr = allocLocked(numBytes);
            if (!newPtr) {
                return nullptr;
            }
            memcpy(newPtr, ptr, min(header->requestedBytes, numBytes));
            void* basePtr = header->basePtr;
            unlinkAlignedHeader(header);
            freeLocked(basePtr);
            return newPtr;
        }

        Chunk* chunk = Chunk::chunkFromMem(ptr);
        uptr neededSize = requestToChunkSize(numBytes);
        if (neededSize == 0) {
            return nullptr;
        }

        if (chunk->getDirectMapped()) {
            uptr oldPayloadBytes = chunk->getSize() - sizeof(ChunkHeader);
            if (neededSize <= chunk->getSize()) {
                uptr oldSize = chunk->getSize();
                chunk->setSizeAndFlags(neededSize, true, true, true);
                this->stats.totalBytesConsumed -= (oldSize - neededSize);
                return ptr;
            }
            void* newPtr = allocLocked(numBytes);
            if (!newPtr) {
                return nullptr;
            }
            memcpy(newPtr, ptr, min(oldPayloadBytes, numBytes));
            freeDirectMappedChunk(chunk);
            return newPtr;
        }

        uptr oldSize = chunk->getSize();
        uptr oldPayloadBytes = oldSize - sizeof(ChunkHeader);
        if (neededSize <= oldSize) {
            uptr remainderBytes = oldSize - neededSize;
            if (remainderBytes >= minChunkSize()) {
                bool prevInUse = chunk->getPrevInUse();
                writeInUseChunkHeader(chunk, neededSize, prevInUse);
                Chunk* remainder = (Chunk*) (((u8*) chunk) + neededSize);
                writeFreeChunkHeader(remainder, remainderBytes, true);
                this->stats.totalBytesConsumed -= remainderBytes;
                recycleFreeChunk(remainder);
            }
            return ptr;
        }

        ChunkHeader* nextHeader = chunk->nextHeader();
        if ((nextHeader->head & InUseBit) == 0) {
            Chunk* next = (Chunk*) nextHeader;
            uptr combined = oldSize + next->getSize();
            if (combined >= neededSize) {
                if (next == this->designatedVictim) {
                    this->designatedVictim = nullptr;
                } else if (next == this->top) {
                    this->top = nullptr;
                } else {
                    unlinkBinnedChunk(next);
                }

                uptr remainderBytes = combined - neededSize;
                bool prevInUse = chunk->getPrevInUse();
                if (remainderBytes >= minChunkSize()) {
                    writeInUseChunkHeader(chunk, neededSize, prevInUse);
                    Chunk* remainder = (Chunk*) (((u8*) chunk) + neededSize);
                    writeFreeChunkHeader(remainder, remainderBytes, true);
                    recycleFreeChunk(remainder);
                    this->stats.totalBytesConsumed += (neededSize - oldSize);
                } else {
                    writeInUseChunkHeader(chunk, combined, prevInUse);
                    this->stats.totalBytesConsumed += (combined - oldSize);
                }
                return ptr;
            }
        }

        void* newPtr = allocLocked(numBytes);
        if (!newPtr) {
            return nullptr;
        }
        memcpy(newPtr, ptr, min(oldPayloadBytes, numBytes));
        freeLocked(ptr);
        return newPtr;
    }

    // Allocates a chunk with explicit alignment and tracks metadata for safe free/realloc.
    void* allocAlignedLocked(uptr numBytes, u32 alignment) {
        ensureInitialized();
        if (alignment < ChunkAlignment) {
            alignment = numericCast<u32>(ChunkAlignment);
        } else {
            PLY_ASSERT(isPowerOf2(alignment));
        }
        if (alignment == ChunkAlignment) {
            return allocLocked(numBytes);
        }
        if (numBytes > getMaxValue<uptr>() - alignment - sizeof(AlignedAllocHeader)) {
            return nullptr;
        }
        uptr requestBytes = numBytes + alignment + sizeof(AlignedAllocHeader);
        void* basePtr = allocLocked(requestBytes);
        if (!basePtr) {
            return nullptr;
        }
        uptr alignedAddr = uptr(alignToPowerOf2((u64) (uptr(basePtr) + sizeof(AlignedAllocHeader)), (u64) alignment));
        AlignedAllocHeader* header = (AlignedAllocHeader*) (alignedAddr - sizeof(AlignedAllocHeader));
        header->alignedPtr = (void*) alignedAddr;
        header->basePtr = basePtr;
        header->requestedBytes = numBytes;
        linkAlignedHeader(header);
        return header->alignedPtr;
    }

    //--------------------------------------------------------------------
    // 10. Validation helpers
    //--------------------------------------------------------------------

#if defined(PLY_WITH_ASSERTS)
    // Counts occurrences of a chunk pointer in all small bins.
    u32 countChunkInSmallBins(const Chunk* target) const {
        u32 count = 0;
        for (u32 i = 0; i < NumSmallBins; i++) {
            for (Chunk* node = this->smallBins[i]; node; node = node->smallNext) {
                if (node == target) {
                    count++;
                }
            }
        }
        return count;
    }

    // Counts occurrences of a chunk pointer in a single tree.
    static u32 countChunkInTree(const Chunk* root, const Chunk* target) {
        if (!root) {
            return 0;
        }
        u32 count = (root == target) ? 1u : 0u;
        count += countChunkInTree(root->treeLeft, target);
        count += countChunkInTree(root->treeRight, target);
        return count;
    }

    // Counts occurrences of a chunk pointer in all tree bins.
    u32 countChunkInTreeBins(const Chunk* target) const {
        u32 count = 0;
        for (u32 i = 0; i < NumTreeBins; i++) {
            count += countChunkInTree(this->treeBins[i], target);
        }
        return count;
    }

    // Validates ordering and parent links of one tree bin.
    static void validateTree(Chunk* node, Chunk* parent, uptr minSize, const Chunk* minPtr, uptr maxSize,
                             const Chunk* maxPtr, u32 expectedBin, uptr* nodeCount) {
        if (!node) {
            return;
        }
        (*nodeCount)++;
        PLY_ASSERT(node->treeParent == parent);
        PLY_ASSERT(!node->getInUse());
        PLY_ASSERT(!isSmallChunk(node->getSize()));
        PLY_ASSERT(treeBinIndex(node->getSize()) == expectedBin);

        bool aboveMin = (node->getSize() > minSize) || ((node->getSize() == minSize) && (node > minPtr));
        bool belowMax = (node->getSize() < maxSize) || ((node->getSize() == maxSize) && (node < maxPtr));
        PLY_ASSERT(aboveMin);
        PLY_ASSERT(belowMax);

        validateTree(node->treeLeft, node, minSize, minPtr, node->getSize(), node, expectedBin, nodeCount);
        validateTree(node->treeRight, node, node->getSize(), node, maxSize, maxPtr, expectedBin, nodeCount);
    }

    // Validates complete allocator consistency and counter integrity.
    void validateLocked() const {
        uptr recomputedAllocatedBytes = 0;
        uptr recomputedSystemBytes = 0;
        uptr freeChunksExpectedInBins = 0;

        if (this->top) {
            PLY_ASSERT(!this->top->getInUse());
            PLY_ASSERT(!this->top->getDirectMapped());
            Segment* topSeg = this->findSegmentForChunk(this->top);
            PLY_ASSERT(topSeg != nullptr);
            PLY_ASSERT(this->top->nextHeader() == topSeg->fence);
        }
        if (this->designatedVictim) {
            PLY_ASSERT(!this->designatedVictim->getInUse());
            PLY_ASSERT(!this->designatedVictim->getDirectMapped());
            PLY_ASSERT(this->findSegmentForChunk(this->designatedVictim) != nullptr);
        }
        if (this->top && this->designatedVictim) {
            PLY_ASSERT(this->top != this->designatedVictim);
        }

        for (Segment* seg = this->segmentHead; seg; seg = seg->next) {
            recomputedSystemBytes += seg->numBytes;
            PLY_ASSERT(seg->firstChunk != nullptr);
            PLY_ASSERT(seg->fence != nullptr);
            Chunk* chunk = seg->firstChunk;
            while ((ChunkHeader*) chunk != seg->fence) {
                uptr chunkSize = chunk->getSize();
                PLY_ASSERT(chunkSize >= minChunkSize());
                PLY_ASSERT(isAlignedToPowerOf2((u64) chunkSize, (u64) ChunkAlignment));
                PLY_ASSERT((chunk->head & DirectMappedBit) == 0);
                ChunkHeader* next = chunk->nextHeader();
                PLY_ASSERT(next->prevFoot == chunkSize);
                PLY_ASSERT(((next->head & PrevInUseBit) != 0) == chunk->getInUse());

                if (chunk->getInUse()) {
                    recomputedAllocatedBytes += chunkSize;
                } else {
                    PLY_ASSERT((next->head & InUseBit) != 0);
                    u32 smallCount = countChunkInSmallBins(chunk);
                    u32 treeCount = countChunkInTreeBins(chunk);
                    if (chunk == this->top || chunk == this->designatedVictim) {
                        PLY_ASSERT(smallCount == 0);
                        PLY_ASSERT(treeCount == 0);
                    } else if (isSmallChunk(chunkSize)) {
                        PLY_ASSERT(smallCount == 1);
                        PLY_ASSERT(treeCount == 0);
                        freeChunksExpectedInBins++;
                    } else {
                        PLY_ASSERT(smallCount == 0);
                        PLY_ASSERT(treeCount == 1);
                        freeChunksExpectedInBins++;
                    }
                }
                chunk = (Chunk*) next;
            }
            PLY_ASSERT((seg->fence->head & InUseBit) != 0);
        }

        uptr countedSmallNodes = 0;
        for (u32 i = 0; i < NumSmallBins; i++) {
            bool hasNodes = (this->smallBins[i] != nullptr);
            PLY_ASSERT(((this->smallMap >> i) & 1u) == (hasNodes ? 1u : 0u));
            Chunk* prev = nullptr;
            for (Chunk* node = this->smallBins[i]; node; node = node->smallNext) {
                countedSmallNodes++;
                PLY_ASSERT(!node->getInUse());
                PLY_ASSERT(isSmallChunk(node->getSize()));
                PLY_ASSERT(smallBinIndex(node->getSize()) == i);
                PLY_ASSERT(node->smallPrev == prev);
                PLY_ASSERT(node != this->top);
                PLY_ASSERT(node != this->designatedVictim);
                PLY_ASSERT(this->findSegmentForChunk(node) != nullptr);
                prev = node;
            }
        }

        uptr countedTreeNodes = 0;
        for (u32 i = 0; i < NumTreeBins; i++) {
            bool hasNodes = (this->treeBins[i] != nullptr);
            PLY_ASSERT(((this->treeMap >> i) & 1u) == (hasNodes ? 1u : 0u));
            if (!this->treeBins[i]) {
                continue;
            }
            PLY_ASSERT(this->treeBins[i]->treeParent == nullptr);
            validateTree(this->treeBins[i], nullptr, 0, nullptr, getMaxValue<uptr>(), (const Chunk*) -1, i,
                         &countedTreeNodes);
        }

        PLY_ASSERT(countedSmallNodes + countedTreeNodes == freeChunksExpectedInBins);

        for (DirectChunk* direct = this->directHead; direct; direct = direct->next) {
            if (direct->next) {
                PLY_ASSERT(direct->next->prev == direct);
            }
            if (direct->prev) {
                PLY_ASSERT(direct->prev->next == direct);
            }
            PLY_ASSERT(direct->chunk.getInUse());
            PLY_ASSERT(direct->chunk.getDirectMapped());
            recomputedAllocatedBytes += direct->chunk.getSize();
            recomputedSystemBytes += direct->mappingSize;
        }

        for (AlignedAllocHeader* header = this->alignedHead; header; header = header->next) {
            if (header->next) {
                PLY_ASSERT(header->next->prev == header);
            }
            if (header->prev) {
                PLY_ASSERT(header->prev->next == header);
            }
            PLY_ASSERT(header->alignedPtr != nullptr);
            PLY_ASSERT(header->basePtr != nullptr);
            PLY_ASSERT((uptr) header->alignedPtr >= (uptr) header->basePtr + sizeof(AlignedAllocHeader));
            Chunk* base = Chunk::chunkFromMem(header->basePtr);
            PLY_ASSERT(base->getInUse());
        }

        PLY_ASSERT(recomputedAllocatedBytes == this->stats.totalBytesConsumed);
        PLY_ASSERT(recomputedSystemBytes == this->stats.totalSystemMemoryUsed);
    }
#endif

public:
    // Allocates memory from the heap under a lock.
    void* alloc(uptr numBytes) {
        LockGuard<Mutex> lock{this->mutex};
        return allocLocked(numBytes);
    }

    // Reallocates memory from the heap under a lock.
    void* realloc(void* ptr, uptr numBytes) {
        LockGuard<Mutex> lock{this->mutex};
        return reallocLocked(ptr, numBytes);
    }

    // Frees memory from the heap under a lock.
    void free(void* ptr) {
        LockGuard<Mutex> lock{this->mutex};
        freeLocked(ptr);
    }

    // Allocates aligned memory from the heap under a lock.
    void* allocAligned(uptr numBytes, u32 alignment) {
        LockGuard<Mutex> lock{this->mutex};
        return allocAlignedLocked(numBytes, alignment);
    }

    // Returns heap usage counters under a lock.
    Heap::Stats getStats() {
        LockGuard<Mutex> lock{this->mutex};
        ensureInitialized();
        return this->stats;
    }

#if defined(PLY_WITH_ASSERTS)
    // Validates heap internal invariants under a lock.
    void validate() {
        LockGuard<Mutex> lock{this->mutex};
        ensureInitialized();
        validateLocked();
    }
#endif
};

// Returns the singleton allocator implementation instance.
static HeapImpl& getHeapImpl() {
    static HeapImpl impl;
    return impl;
}

// Allocates memory from the selected allocator implementation.
void* Heap::alloc(uptr numBytes) {
    void* ptr = getHeapImpl().alloc(numBytes);
    if (!ptr && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return ptr;
}

// Reallocates memory from the selected allocator implementation.
void* Heap::realloc(void* ptr, uptr numBytes) {
    void* newPtr = getHeapImpl().realloc(ptr, numBytes);
    if (!newPtr && numBytes != 0 && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return newPtr;
}

// Frees memory from the selected allocator implementation.
void Heap::free(void* ptr) {
    getHeapImpl().free(ptr);
}

// Allocates aligned memory from the selected allocator implementation.
void* Heap::allocAligned(uptr numBytes, u32 alignment) {
    void* ptr = getHeapImpl().allocAligned(numBytes, alignment);
    if (!ptr && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return ptr;
}

// Returns allocator statistics from the selected allocator implementation.
Heap::Stats Heap::getStats() {
    return getHeapImpl().getStats();
}

// Validates allocator invariants when assertions are enabled.
void Heap::validate() {
#if defined(PLY_WITH_ASSERTS)
    getHeapImpl().validate();
#endif
}

#else // PLY_USE_DLMALLOC

void* Heap::alloc(uptr numBytes) {
    void* ptr = dlmalloc(numBytes);
    if (!ptr && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return ptr;
}

void* Heap::realloc(void* ptr, uptr numBytes) {
    void* newPtr = dlrealloc(ptr, numBytes);
    if (!newPtr && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return newPtr;
}

void Heap::free(void* ptr) {
    dlfree(ptr);
}

void* Heap::allocAligned(uptr numBytes, u32 alignment) {
    void* ptr = dlmemalign(alignment, numBytes);
    if (!ptr && outOfMemoryHandler) {
        outOfMemoryHandler();
    }
    return ptr;
}

Heap::Stats Heap::getStats() {
    Heap::Stats stats;
    static_assert(sizeof(DLMallocStats) == sizeof(Heap::Stats), "DLMallocStats layout mismatch");
    static_assert(alignof(DLMallocStats) == alignof(Heap::Stats), "DLMallocStats alignment mismatch");
    dlget_heap_stats((DLMallocStats*) &stats);
    return stats;
}

void Heap::validate() {
#if defined(PLY_WITH_ASSERTS)
    // Validation is only implemented by the bespoke allocator.
#endif
}

#endif // PLY_USE_DLMALLOC

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

bool StringView::startsWith(StringView other) const {
    if (other.numBytes_ > this->numBytes_)
        return false;
    return memcmp(this->bytes_, other.bytes_, other.numBytes_) == 0;
}

bool StringView::endsWith(StringView other) const {
    if (other.numBytes_ > this->numBytes_)
        return false;
    return memcmp(this->bytes_ + this->numBytes_ - other.numBytes_, other.bytes_, other.numBytes_) == 0;
}

StringView StringView::trim(bool (*matchFunc)(char), bool left, bool right) const {
    const char* start = this->bytes_;
    const char* end = start + this->numBytes_;
    if (left) {
        while ((start < end) && matchFunc(*start)) {
            start++;
        }
    }
    if (right) {
        while ((start < end) && matchFunc(end[-1])) {
            end--;
        }
    }
    return {start, end};
}

Array<StringView> StringView::split(StringView separator) const {
    Array<StringView> result;
    u32 start = 0;
    while (start < this->numBytes_) {
        s32 pos = this->find(separator, start);
        if (pos < 0) {
            // No more separators found, add the rest
            StringView remainder = this->substr(start);
            if (remainder.numBytes_ > 0) {
                result.append(remainder);
            }
            break;
        }
        // Add the part before the separator (if non-empty)
        if ((u32) pos > start) {
            result.append(this->substr(start, pos - start));
        }
        start = pos + separator.numBytes_;
    }
    if (result.isEmpty()) {
        result.append({});
    }
    return result;
}

String StringView::replace(StringView oldSubstr, StringView newSubstr) const {
    PLY_ASSERT(oldSubstr.numBytes_ > 0);
    MemStream out;
    u32 limit = this->numBytes_ - oldSubstr.numBytes_;
    u32 i = 0;
    for (; i < limit; i++) {
        if (memcmp(this->bytes_ + i, oldSubstr.bytes_, oldSubstr.numBytes_) == 0) {
            out.write(newSubstr);
            i += oldSubstr.numBytes_ - 1;
        } else {
            out.write(this->bytes_[i]);
        }
    }
    if (i < this->numBytes_) {
        out.write({this->bytes_ + i, this->bytes_ + this->numBytes_});
    }
    return out.moveToString();
}

String StringView::upper() const {
    String result = String::allocate(this->numBytes_);
    for (u32 i = 0; i < this->numBytes_; i++) {
        char c = this->bytes_[i];
        if (c >= 'a' && c <= 'z') {
            c += 'A' - 'a';
        }
        result[i] = c;
    }
    return result;
}

String StringView::lower() const {
    String result = String::allocate(this->numBytes_);
    for (u32 i = 0; i < this->numBytes_; i++) {
        char c = this->bytes_[i];
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
        result[i] = c;
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
    return out.moveToString();
}

s32 compare(StringView a, StringView b) {
    u32 compareBytes = min(a.numBytes(), b.numBytes());
    const u8* u0 = (const u8*) a.bytes();
    const u8* u1 = (const u8*) b.bytes();
    const u8* u_end0 = u0 + compareBytes;
    while (u0 < u_end0) {
        s32 diff = *u0 - *u1;
        if (diff != 0)
            return diff;
        u0++;
        u1++;
    }
    return a.numBytes() - b.numBytes();
}

String operator+(StringView a, StringView b) {
    String result = String::allocate(a.numBytes() + b.numBytes());
    memcpy(result.bytes(), a.bytes(), a.numBytes());
    memcpy(result.bytes() + a.numBytes(), b.bytes(), b.numBytes());
    return result;
}

String operator*(StringView str, u32 count) {
    String result = String::allocate(str.numBytes() * count);
    char* dst = result.bytes();
    for (u32 i = 0; i < count; i++) {
        memcpy(dst, str.bytes(), str.numBytes());
        dst += str.numBytes();
    }
    return result;
}

s32 StringView::find(StringView pattern, u32 startPos) const {
    if (startPos + pattern.numBytes_ > this->numBytes_)
        return -1;
    u32 limit = this->numBytes_ - pattern.numBytes_;
    for (; startPos <= limit; startPos++) {
        for (u32 i = 0; i < pattern.numBytes_; i++) {
            if (pattern.bytes_[i] != this->bytes_[startPos + i])
                goto next;
        }
        return startPos;
    next:;
    }
    return -1;
}

s32 StringView::reverseFind(StringView pattern, s32 startPos) const {
    if (startPos < 0) {
        startPos += this->numBytes_;
    }
    if (startPos + pattern.numBytes_ >= this->numBytes_) {
        startPos = (s32) this->numBytes_ - pattern.numBytes_;
    }
    for (; startPos >= 0; startPos--) {
        for (u32 i = 0; i < pattern.numBytes_; i++) {
            if (pattern.bytes_[i] != this->bytes_[startPos + i])
                goto next;
        }
        // Found a match.
        return startPos;
    next:;
    }
    return -1;
}

struct MatchState {
    ViewStream* str;
    ViewStream* pattern;
    ArrayView<const MatchArg> matchArgs;
    u32 argIndex = 0;
};

enum class MatchMode {
    Matching,
    Skipping,
};

// matchPatternSegment reads pattern elements from state.pattern up until the next `)` or `$`.
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
static bool matchPatternSegment(MatchState& state, MatchMode mode) {
    // Variables to track the success of the current segment.
    bool anyClauseSucceeded = false;
    bool currentClauseSucceeded = true;
    char* inputAtStartOfCurrentClause = state.str->curByte;
    Array<Functor<void()>> outputVariablesToCommit;

    // Main loop to read the pattern segment one element at a time.
    while (state.pattern->makeReadable()) {
        // Variables to track the success of the current element.
        char* patternAtStartOfCurrentElement = state.pattern->curByte;
        char* inputAtStartOfCurrentElement = state.str->curByte;
        u32 argIndexAtStartOfCurrentElement = state.argIndex;
        bool elementMatched = false;

        // Check the current pattern element.
        char patternElement = *state.pattern->curByte++;
        if (patternElement == '%') {
            // It's a format specifier. Read the expected value type.
            if (!state.pattern->makeReadable()) {
                PLY_ASSERT(0); // Expected specification char after %
                return false;
            }

            // Get the next argument to capture the value.
            u32 i = state.argIndex++;
            const MatchArg& arg = state.matchArgs[i];

            // Clear any previous input errors.
            state.str->inputError = false;

            char spec = *state.pattern->curByte++;
            if (spec == 'i') { // Identifier
                if (mode == MatchMode::Matching) {
                    StringView id = readIdentifier(*state.str);
                    if (!id.isEmpty()) {
                        elementMatched = true;
                        outputVariablesToCommit.append([arg, id]() {
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
                        u64 val = readU64FromText(*state.str);
                        if (!state.str->inputError) {
                            elementMatched = true;
                            outputVariablesToCommit.append([arg, val]() {
                                if (auto* ptr = arg.as<u64*>()) {
                                    **ptr = val;
                                } else {
                                    **arg.as<u32*>() = (u32) val;
                                }
                            });
                        }
                    } else if (arg.is<s64*>() || arg.is<s32*>()) {
                        s64 val = readS64FromText(*state.str);
                        if (!state.str->inputError) {
                            elementMatched = true;
                            outputVariablesToCommit.append([arg, val]() {
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
                    double val = readDoubleFromText(*state.str);
                    if (!state.str->inputError) {
                        elementMatched = true;
                        outputVariablesToCommit.append([arg, val]() {
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
                    String val = readQuotedString(*state.str, QuotedStringType::C, true);
                    if (!state.str->inputError) {
                        elementMatched = true;
                        outputVariablesToCommit.append([arg, val = std::move(val)]() {
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
        } else if (patternElement == ' ') {
            // It's a space character. Try to match whitespace.
            if (mode == MatchMode::Matching) {
                if (state.str->makeReadable() && isWhite(*state.str->curByte)) {
                    elementMatched = true;
                    state.str->curByte++;
                }
            }
        } else if (patternElement == '(') {
            // It's a left parenthesis. Read a sub-pattern segment recursively.
            elementMatched = matchPatternSegment(state, mode);
            if (!state.pattern->makeReadable()) {
                PLY_ASSERT(0); // Expected a character after the opening parenthesis.
                return false;
            }
            char c = *state.pattern->curByte++;
            PLY_ASSERT(c == ')'); // Expected a closing parenthesis.
        } else if ((patternElement == ')') || (patternElement == '$')) {
            state.pattern->curByte--;
            break;
        } else if (patternElement == '|') {
            // It's a vertical bar. End the current clause and begin reading the next clause.
            if (mode == MatchMode::Matching) {
                if (currentClauseSucceeded) {
                    anyClauseSucceeded = true;
                    for (Functor<void()>& commit : outputVariablesToCommit) {
                        commit();
                    }
                    outputVariablesToCommit.clear();
                    mode = MatchMode::Skipping;
                } else {
                    // Reset status variables and try to match the next clause.
                    currentClauseSucceeded = true;
                    state.str->curByte = inputAtStartOfCurrentClause;
                    outputVariablesToCommit.clear();
                }
            }
            continue; // Skip the check for ? or * qualifiers
        } else if (patternElement == '\'') {
            // It's a single quote. Treat the next pattern character as a literal character.
            if (!state.pattern->makeReadable()) {
                PLY_ASSERT(0); // Expected a character to follow `'`.
                return false;
            }
            char escaped = *state.pattern->curByte++;
            if (mode == MatchMode::Matching) {
                if (state.str->makeReadable() && *state.str->curByte == escaped) {
                    elementMatched = true;
                    state.str->curByte++;
                }
            }
        } else {
            PLY_ASSERT((patternElement != '*') && (patternElement != '?')); // Unexpected quantifier.
            if (mode == MatchMode::Matching) {
                if (state.str->makeReadable() && *state.str->curByte == patternElement) {
                    elementMatched = true;
                    state.str->curByte++;
                }
            }
        }

        if (state.pattern->makeReadable()) {
            char c = *state.pattern->curByte;
            if (c == '?') {
                // It's a question mark. Make the current element optional.
                state.pattern->curByte++;
                if ((mode == MatchMode::Matching) && !elementMatched) {
                    // The current element didn't match, but was optional.
                    // Revert the input string to the start of the current element.
                    state.str->curByte = inputAtStartOfCurrentElement;
                }
                continue;
            } else if (c == '*') {
                // It's a star. Make the current element repeatable.
                state.pattern->curByte++;
                // It's illegal to capture variables inside repeated elements:
                PLY_ASSERT(state.argIndex == argIndexAtStartOfCurrentElement); // No repeated captures!
                if ((mode == MatchMode::Matching) && elementMatched) {
                    // The current element matched. Rewind the pattern to the start of the current element
                    // and try to match it again.
                    state.pattern->curByte = patternAtStartOfCurrentElement;
                } else {
                    // The current element didn't match, but was repeatable.
                    // Revert the input string to the start of the current element.
                    state.str->curByte = inputAtStartOfCurrentElement;
                }
                continue;
            }
        }

        if ((mode == MatchMode::Matching) && !elementMatched) {
            // The current element didn't match.
            currentClauseSucceeded = false;
        }
    }

    // We reached the end of the segment.
    if ((mode == MatchMode::Matching) && currentClauseSucceeded) {
        anyClauseSucceeded = true;
        for (Functor<void()>& commit : outputVariablesToCommit) {
            commit();
        }
    }

    return anyClauseSucceeded;
}

bool matchWithArgs(ViewStream& in, StringView pattern, ArrayView<const MatchArg> matchArgs) {
    MatchState state;
    ViewStream patternIn{pattern};
    state.str = &in;
    state.pattern = &patternIn;
    state.matchArgs = matchArgs;
    state.argIndex = 0;

    if (!matchPatternSegment(state, MatchMode::Matching))
        return false;

    if (state.pattern->makeReadable() && (*state.pattern->curByte == '$')) {
        if (state.str->makeReadable())
            return false; // Expected end of string
    }

    // Check that we consumed all match args
    PLY_ASSERT(state.argIndex == matchArgs.numItems());

    return true;
}

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

String::String(StringView other) : bytes_{(char*) Heap::alloc(other.numBytes())}, numBytes_{other.numBytes()} {
    memcpy(this->bytes_, other.bytes(), other.numBytes());
}

String String::allocate(u32 numBytes) {
    String result;
    result.bytes_ = (char*) Heap::alloc(numBytes);
    result.numBytes_ = numBytes;
    return result;
}

void String::resize(u32 numBytes) {
    this->bytes_ = (char*) Heap::realloc(this->bytes_, numBytes);
    this->numBytes_ = numBytes;
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

void addToHash(HashBuilder& builder, u32 value) {
    value *= 0xcc9e2d51u;
    value = (value << 15) | (value >> 17);
    value *= 0x1b873593u;
    builder.accumulator ^= value;
    builder.accumulator = (builder.accumulator << 13) | (builder.accumulator >> 19);
    builder.accumulator = builder.accumulator * 5 + 0xe6546b64u;
}

void addToHash(HashBuilder& builder, StringView str) {
    // FIXME: More work is needed for platforms that don't support unaligned reads
    u32 i = 0;
    while (i + 4 <= str.numBytes()) {
        addToHash(builder, *(const u32*) (str.bytes() + i)); // May be unaligned
        i += 4;
    }
    if (i < str.numBytes()) {
        // Avoid potential unaligned read across page boundary
        u32 v = 0;
        while (i < str.numBytes()) {
            v = (v << 8) | *(const u8*) (str.bytes() + i);
            i++;
        }
        addToHash(builder, v);
    }
}

//  ▄▄  ▄▄               ▄▄     ▄▄                  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ██     ▄▄▄▄   ▄▄▄▄  ██  ▄▄ ▄▄  ▄▄ ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██    ██  ██ ██  ██ ██▄█▀  ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄██ ██▄▄█▀
//                                                                ██

u32 getBestNumHashIndices(u32 numItems) {
    if (numItems >= 8) {
        return roundUpToNearestPowerOf2(u32((u64{numItems} * 5) >> 2));
    }
    return (numItems < 4) ? 4 : 8;
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

void Pipe::flush(bool toDevice) {
    PLY_ASSERT(0);
}

u64 Pipe::getFileSize() {
    // This method is unsupported by the subclass. Do not call.
    PLY_ASSERT(0);
    return 0;
}

void Pipe::seekTo(s64 offset) {
    PLY_ASSERT(0);
}

#if defined(PLY_WINDOWS)

PipeHandle::~PipeHandle() {
    if (this->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(this->handle);
    }
}

u32 PipeHandle::read(MutStringView buf) {
    DWORD readBytes;
    BOOL rc = ReadFile(this->handle, buf.bytes, buf.numBytes, &readBytes, NULL);
    if (!rc) // Handles ERROR_BROKEN_PIPE and other errors.
        return 0;
    return readBytes; // 0 when attempting to read past EOF.
}

bool PipeHandle::write(StringView buf) {
    while (buf.numBytes() > 0) {
        DWORD desiredBytes = min<DWORD>(buf.numBytes(), UINT32_MAX);
        DWORD writtenBytes;
        BOOL rc = WriteFile(this->handle, buf.bytes(), desiredBytes, &writtenBytes, NULL);
        if (!rc) // Handles ERROR_NO_DATA and other errors.
            return false;
        buf = buf.substr(writtenBytes);
    }
    return true;
}

void PipeHandle::flush(bool toDevice) {
    if (toDevice) {
        FlushFileBuffers(this->handle);
    }
}

u64 PipeHandle::getFileSize() {
    LARGE_INTEGER fileSize;
    GetFileSizeEx(this->handle, &fileSize);
    return fileSize.QuadPart;
}

void PipeHandle::seekTo(s64 offset) {
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
        rc = (s32)::read(this->fd, buf.bytes, buf.numBytes);
    } while (rc == -1 && errno == EINTR);
    PLY_ASSERT(rc >= 0); // Note: Will probably need to detect closed pipes here
    if (rc < 0)
        return 0;
    return rc;
}

bool Pipe_FD::write(StringView buf) {
    PLY_ASSERT(this->fd >= 0);
    while (buf.numBytes() > 0) {
        s32 sent = (s32)::write(this->fd, buf.bytes(), buf.numBytes());
        if (sent <= 0)
            return false;
        PLY_ASSERT((u32) sent <= buf.numBytes());
        buf = buf.substr(sent);
    }
    return true;
}

void Pipe_FD::flush(bool toDevice) {
    // FIXME: Implement as per
    // https://github.com/libuv/libuv/issues/1579#issue-262113760
}

u64 Pipe_FD::getFileSize() {
    PLY_ASSERT(this->fd >= 0);
    struct stat buf;
    int rc = fstat(this->fd, &buf);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
    return buf.st_size;
}

void Pipe_FD::seekTo(s64 offset) {
    PLY_ASSERT(this->fd >= 0);
    off_t rc = lseek(this->fd, numericCast<off_t>(offset), SEEK_SET);
    PLY_ASSERT(rc == 0);
    PLY_UNUSED(rc);
}

#endif

//---------------------

struct NewLineFilter {
    struct Params {
        const char* srcByte = nullptr;
        const char* srcEndByte = nullptr;
        char* dstByte = nullptr;
        char* dstEndByte = nullptr;
    };

    bool crlf = false; // If true, outputs \r\n instead of \n
    bool needsLf = false;

    void process(Params* params) {
        while (params->dstByte < params->dstEndByte) {
            u8 c = 0;
            if (this->needsLf) {
                c = '\n';
                this->needsLf = false;
            } else {
                for (;;) {
                    if (params->srcByte >= params->srcEndByte)
                        return; // src has been consumed
                    c = *params->srcByte++;
                    if (c == '\r') {
                        // Output nothing
                    } else {
                        if (c == '\n' && this->crlf) {
                            c = '\r';
                            this->needsLf = true;
                        }
                        break;
                    }
                }
            }
            *params->dstByte++ = c;
        }
    }
};

//-----------------------------------------------------------------------

class InPipeNewLineFilter : public Pipe {
public:
    Stream in;
    NewLineFilter filter;

    InPipeNewLineFilter(Stream&& in) : in{std::move(in)} {
        PLY_ASSERT(this->in.hasReadPermission);
        this->flags = Pipe::HAS_READ_PERMISSION;
    }
    virtual u32 read(MutStringView buf) override;
};

u32 InPipeNewLineFilter::read(MutStringView buf) {
    PLY_ASSERT(buf.numBytes > 0);

    NewLineFilter::Params params;
    params.dstByte = buf.bytes;
    params.dstEndByte = buf.bytes + buf.numBytes;
    for (;;) {
        params.srcByte = this->in.curByte;
        params.srcEndByte = this->in.endByte;
        this->filter.process(&params);

        this->in.curByte = const_cast<char*>(params.srcByte);
        u32 numBytesWritten = numericCast<u32>(params.dstByte - buf.bytes);
        if (numBytesWritten > 0)
            return numBytesWritten;

        PLY_ASSERT(!this->in.hasRemainingBytes());
        if (!this->in.makeReadable())
            return 0;
    }
}

//-----------------------------------------------------------------------

class OutPipeNewLineFilter : public Pipe {
public:
    Stream out;
    NewLineFilter filter;

    OutPipeNewLineFilter(Stream&& out, bool writeCrlf) : out{std::move(out)} {
        PLY_ASSERT(this->out.hasWritePermission);
        this->flags = Pipe::HAS_WRITE_PERMISSION;
        this->filter.crlf = writeCrlf;
    }
    virtual bool write(StringView buf) override;
    virtual void flush(bool toDevice) override;
};

bool OutPipeNewLineFilter::write(StringView buf) {
    u32 desiredTotalBytesRead = buf.numBytes();
    u32 totalBytesRead = 0;
    for (;;) {
        this->out.makeWritable();

        // If tryMakeBytesAvailable fails, process() will do nothing and we'll simply
        // return below:
        NewLineFilter::Params params;
        params.srcByte = buf.bytes();
        params.srcEndByte = buf.bytes() + buf.numBytes();
        params.dstByte = this->out.curByte;
        params.dstEndByte = this->out.endByte;
        this->filter.process(&params);
        this->out.curByte = params.dstByte;
        u32 numBytesRead = numericCast<u32>(params.srcByte - buf.bytes());
        if (numBytesRead == 0) {
            PLY_ASSERT(totalBytesRead <= desiredTotalBytesRead);
            return totalBytesRead >= desiredTotalBytesRead;
        }
        totalBytesRead += numBytesRead;
        buf = buf.substr(numBytesRead);
    }
}

void OutPipeNewLineFilter::flush(bool toDevice) {
    // Forward flush command down the output chain.
    this->out.flush(toDevice);
};

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

Stream::Stream() {
}

Stream::Stream(Pipe* pipe, bool isPipeOwner) {
    if (pipe) {
        this->type = Type::Pipe;
        new (&this->pipe) PipeData;
        this->pipe.pipe = pipe;
        this->isPipeOwner = isPipeOwner;
        this->pipe.buffer = (char*) Heap::alloc(BUFFER_SIZE);
        this->curByte = this->pipe.buffer;
        this->endByte = this->pipe.buffer;
        this->hasReadPermission = (pipe->getFlags() & Pipe::HAS_READ_PERMISSION) != 0;
        this->hasWritePermission = (pipe->getFlags() & Pipe::HAS_WRITE_PERMISSION) != 0;
    }
}

Stream::Stream(Stream&& other) {
    this->curByte = other.curByte;
    this->endByte = other.endByte;
    this->type = other.type;
    this->mode = other.mode;
    this->isPipeOwner = other.isPipeOwner;
    this->hasReadPermission = other.hasReadPermission;
    this->hasWritePermission = other.hasWritePermission;
    this->atEof = other.atEof;
    this->inputError = other.inputError;
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
        if (this->hasWritePermission) {
            this->flush();
        }
        if (this->isPipeOwner) {
            Heap::destroy(this->pipe.pipe);
        }
        Heap::free(this->pipe.buffer);
    } else if (this->type == Type::Mem) {
        for (char* buf : this->mem.buffers) {
            Heap::free(buf);
        }
        if (this->mem.tempBuffer) {
            Heap::free(this->mem.tempBuffer);
        }
        this->mem.~MemData();
    }
}

void Stream::flushMemWrites() {
    PLY_ASSERT(this->type == Type::Mem);
    if (this->mode == Mode::Writing) {
        if (this->usingTempBuffer) {
            u32 numBytesWritten = numericCast<u32>(this->curByte - this->mem.tempBuffer);
            u32 spaceAvailable = BUFFER_SIZE - this->mem.tempBufferOffset;
            memcpy(this->mem.buffers[this->mem.bufferIndex] + this->mem.tempBufferOffset, this->mem.tempBuffer,
                   min(numBytesWritten, spaceAvailable));
            if (spaceAvailable < numBytesWritten) {
                if (this->mem.bufferIndex + 1 >= this->mem.buffers.numItems()) {
                    this->mem.buffers.append((char*) Heap::alloc(BUFFER_SIZE));
                    memcpy(this->mem.buffers.back(), this->mem.tempBuffer + spaceAvailable,
                           numBytesWritten - spaceAvailable);
                    this->mem.numBytesInLastBuffer = numBytesWritten - spaceAvailable;
                }
                this->mem.bufferIndex++;
                this->curByte = this->mem.buffers[this->mem.bufferIndex] + (numBytesWritten - spaceAvailable);
            } else {
                this->curByte = this->mem.buffers[this->mem.bufferIndex] + this->mem.tempBufferOffset + numBytesWritten;
            }
            this->endByte = this->mem.buffers[this->mem.bufferIndex] + BUFFER_SIZE;
            this->usingTempBuffer = false;
        } else {
            if (this->mem.bufferIndex + 1 == this->mem.buffers.numItems()) {
                // Extend number of bytes in the last buffer.
                this->mem.numBytesInLastBuffer =
                    max(this->mem.numBytesInLastBuffer, numericCast<u32>(this->curByte - this->mem.buffers.back()));
            }
        }
    }
}

bool Stream::makeReadableInternal(u32 minBytes) {
    PLY_ASSERT(this->hasReadPermission);
    PLY_ASSERT(minBytes <= MAX_CONSECUTIVE_BYTES);
    if ((this->mode == Mode::Reading) && (this->numRemainingBytes() >= minBytes))
        return true;

    if (this->type == Type::Pipe) {
        if (this->mode == Mode::Writing) {
            // Write any buffered data to the pipe.
            this->pipe.pipe->write({this->pipe.buffer, this->curByte});
            this->pipe.seekPosAtBuffer += (this->curByte - this->pipe.buffer);
        }
        if (this->mode != Mode::Reading) {
            // Reset buffer contents.
            this->curByte = this->pipe.buffer;
            this->endByte = this->pipe.buffer;
            this->mode = Mode::Reading;
        } else {
            // Keep any bytes we have.
            u32 numToPreserve = this->numRemainingBytes();
            if (numToPreserve > 0) {
                memmove(this->pipe.buffer, this->curByte, this->numRemainingBytes());
            }
            this->pipe.seekPosAtBuffer += (this->curByte - this->pipe.buffer);
            this->curByte = this->pipe.buffer;
            this->endByte = this->pipe.buffer + numToPreserve;
        }

        do {
            // Load data into the buffer.
            u32 numBytesLoaded = this->pipe.pipe->read({this->endByte, BUFFER_SIZE - this->numRemainingBytes()});
            if (numBytesLoaded == 0) {
                if (!this->hasRemainingBytes()) {
                    this->atEof = true;
                }
                return false;
            }
            this->endByte += numBytesLoaded;
        } while (this->numRemainingBytes() < minBytes);

        // We have at least the number of bytes the caller asked for.
        return true;
    } else if (this->type == Type::Mem) {
        this->flushMemWrites();

        if (!this->hasRemainingBytes()) {
            if (this->mem.bufferIndex + 1 < this->mem.buffers.numItems()) {
                this->mem.bufferIndex++;
                this->curByte = this->mem.buffers[this->mem.bufferIndex];
                if (this->mem.bufferIndex + 1 < this->mem.buffers.numItems()) {
                    this->endByte = this->curByte + BUFFER_SIZE;
                } else {
                    this->endByte = this->curByte + this->mem.numBytesInLastBuffer;
                }
            }
        } else if (this->numRemainingBytes() < minBytes) {
            if (this->mem.bufferIndex + 1 < this->mem.buffers.numItems()) {
                u32 numBytesInNextBuffer = BUFFER_SIZE;
                if (this->mem.bufferIndex + 2 == this->mem.buffers.numItems()) {
                    numBytesInNextBuffer = this->mem.numBytesInLastBuffer;
                }
                u32 numBytesToExpose = min(minBytes, this->numRemainingBytes() + numBytesInNextBuffer);
                if (!this->mem.tempBuffer) {
                    this->mem.tempBuffer = (char*) Heap::alloc(MAX_CONSECUTIVE_BYTES);
                }
                memcpy(this->mem.tempBuffer, this->curByte, this->numRemainingBytes());
                memcpy(this->mem.tempBuffer + this->numRemainingBytes(), this->mem.buffers[this->mem.bufferIndex + 1],
                       numBytesToExpose - this->numRemainingBytes());
                this->mem.tempBufferOffset = numericCast<u32>(this->curByte - this->mem.buffers[this->mem.bufferIndex]);
                this->usingTempBuffer = true;
                this->curByte = this->mem.tempBuffer;
                this->endByte = this->mem.tempBuffer + numBytesToExpose;
            }
        }
        if (!this->hasRemainingBytes()) {
            this->atEof = true;
        }
        return (this->numRemainingBytes() >= minBytes);
    } else if (this->type == Type::View) {
        this->mode = Mode::Reading;
        if (this->curByte >= this->endByte) {
            this->atEof = true;
        }
        return (this->numRemainingBytes() >= minBytes);
    }

    PLY_ASSERT(0); // Shouldn't get here.
    return false;
}

bool Stream::makeWritableInternal(u32 minBytes) {
    PLY_ASSERT(this->hasWritePermission);
    PLY_ASSERT(minBytes <= MAX_CONSECUTIVE_BYTES);
    if ((this->mode == Mode::Writing) && (this->numRemainingBytes() >= minBytes))
        return true;

    if (this->type == Type::Pipe) {
        if (this->mode == Mode::Writing) {
            // Write buffered data to the pipe.
            this->pipe.pipe->write({this->pipe.buffer, this->curByte});
            this->pipe.seekPosAtBuffer += (this->curByte - this->pipe.buffer);
        }

        // Make entire buffer available for writing.
        this->curByte = this->pipe.buffer;
        this->endByte = this->curByte + BUFFER_SIZE;
        this->atEof = false;
    } else if (this->type == Type::Mem) {
        this->flushMemWrites();

        if (!this->hasRemainingBytes()) {
            this->mem.bufferIndex++;
            if (this->mem.bufferIndex >= this->mem.buffers.numItems()) {
                this->mem.buffers.append((char*) Heap::alloc(BUFFER_SIZE));
                this->mem.numBytesInLastBuffer = 0;
            }
            this->curByte = this->mem.buffers[this->mem.bufferIndex];
            this->endByte = this->curByte + BUFFER_SIZE;
        } else if (this->numRemainingBytes() < minBytes) {
            if (!this->mem.tempBuffer) {
                this->mem.tempBuffer = (char*) Heap::alloc(MAX_CONSECUTIVE_BYTES);
            }
            this->mem.tempBufferOffset = numericCast<u32>(this->curByte - this->mem.buffers[this->mem.bufferIndex]);
            this->usingTempBuffer = true;
            this->curByte = this->mem.tempBuffer;
            this->endByte = this->mem.tempBuffer + minBytes;
        }
        this->atEof = false;
    } else if (this->type == Type::View) {
        this->atEof = true;
    } else {
        PLY_ASSERT(0); // Shouldn't get here.
    }

    this->mode = Mode::Writing;
    return !this->atEof;
}

char Stream::peekByteInternal() {
    if (!this->makeReadable())
        return 0;
    PLY_ASSERT(this->curByte < this->endByte);
    return *this->curByte;
}

char Stream::readByteInternal() {
    if (!this->makeReadable())
        return 0;
    PLY_ASSERT(this->curByte < this->endByte);
    return *this->curByte++;
}

u32 Stream::readInternal(MutStringView dst) {
    u32 numBytesRead = 0;
    while (dst.numBytes > 0) {
        if (!this->makeReadable()) {
            memset(dst.bytes, 0, dst.numBytes);
            break;
        }
        u32 toCopy = min(dst.numBytes, this->numRemainingBytes());
        memcpy(dst.bytes, this->curByte, toCopy);
        this->curByte += toCopy;
        dst = dst.subview(toCopy);
        numBytesRead += toCopy;
    }
    return numBytesRead;
}

u32 Stream::skipInternal(u32 numBytes) {
    u32 numBytesSkipped = 0;
    while (numBytes > 0) {
        if (!this->makeReadable())
            break;
        u32 toSkip = min(numBytes, this->numRemainingBytes());
        this->curByte += toSkip;
        numBytes -= toSkip;
        numBytesSkipped += toSkip;
    }
    return numBytesSkipped;
}

void Stream::flush(bool toDevice) {
    PLY_ASSERT(this->hasWritePermission);
    if (this->mode != Mode::Writing)
        return;
    if (this->type == Type::Pipe) {
        PLY_ASSERT(this->pipe.pipe);
        PLY_ASSERT(this->pipe.buffer + BUFFER_SIZE == this->endByte);

        // Write buffered data to the pipe.
        this->pipe.pipe->write({this->pipe.buffer, this->curByte});
        this->curByte = this->pipe.buffer;

        // Forward flush command down the output chain.
        this->pipe.pipe->flush(toDevice);
    } else if (this->type == Type::Mem) {
        this->flushMemWrites();
    }
}

u32 Stream::write(StringView src) {
    u32 totalCopied = 0;
    while (src && this->makeWritable()) {
        // Copy as much data as possible to the current block.
        u32 toCopy = min(this->numRemainingBytes(), src.numBytes());
        memcpy(this->curByte, src.bytes(), toCopy);
        this->curByte += toCopy;
        src = src.substr(toCopy);
        totalCopied += toCopy;
    }
    return totalCopied;
}

u64 Stream::getSeekPos() {
    if (this->type == Type::Pipe) {
        return this->pipe.seekPosAtBuffer + (this->curByte - this->pipe.buffer);
    } else if (this->type == Type::Mem) {
        if (this->usingTempBuffer) {
            return (this->mem.bufferIndex * BUFFER_SIZE) + this->mem.tempBufferOffset +
                   (this->curByte - this->mem.tempBuffer);
        } else {
            char* buf = this->mem.buffers[this->mem.bufferIndex];
            return (this->mem.bufferIndex * BUFFER_SIZE) + (this->curByte - buf);
        }
    } else if (this->type == Type::View) {
        return (this->curByte - this->view.startByte);
    }
    PLY_ASSERT(0); // Shouldn't get here.
    return 0;
}

void Stream::seekTo(u64 seekPos) {
    if (this->type == Type::Pipe) {
        PLY_ASSERT((this->pipe.pipe->getFlags() & Pipe::CAN_SEEK) != 0);
        s64 relativeToBuffer = seekPos - this->pipe.seekPosAtBuffer;
        u32 numBytesInBuffer = numericCast<u32>(this->endByte - this->pipe.buffer);
        if (relativeToBuffer >= 0 && relativeToBuffer <= numBytesInBuffer) {
            this->curByte = this->pipe.buffer + relativeToBuffer;
        } else {
            this->pipe.pipe->seekTo(seekPos);
            this->curByte = this->pipe.buffer;
            this->endByte = this->pipe.buffer;
        }
    } else if (this->type == Type::Mem) {
        this->flushMemWrites();

        u32 bufferIndex = numericCast<u32>(seekPos / BUFFER_SIZE);
        PLY_ASSERT(bufferIndex < this->mem.buffers.numItems());
        this->mem.bufferIndex = bufferIndex;
        char* buf = this->mem.buffers[bufferIndex];
        u32 offsetInBuffer = numericCast<u32>(seekPos - u64(bufferIndex) * BUFFER_SIZE);
        u32 numBytesInBuffer = BUFFER_SIZE;
        if (bufferIndex == this->mem.buffers.numItems() - 1) {
            numBytesInBuffer = this->mem.numBytesInLastBuffer;
            PLY_ASSERT(bufferIndex < this->mem.buffers.numItems());
            PLY_ASSERT(offsetInBuffer <= numBytesInBuffer);
        }
        this->curByte = buf + offsetInBuffer;
        this->endByte = buf + numBytesInBuffer;
    } else if (this->type == Type::View) {
        PLY_ASSERT(seekPos <= numericCast<uptr>(this->endByte - this->view.startByte));
        this->curByte = this->view.startByte + seekPos;
    } else {
        PLY_ASSERT(0); // Shouldn't get here.
    }
    this->atEof = false;
    this->inputError = false;
}

//--------------------------------------------
MemStream::MemStream() {
    this->type = Type::Mem;
    new (&this->mem) MemData;
    char* buf = (char*) Heap::alloc(BUFFER_SIZE);
    this->mem.buffers.append(buf);
    this->curByte = buf;
    this->endByte = buf + BUFFER_SIZE;
    this->hasReadPermission = true;
    this->hasWritePermission = true;
}

String MemStream::moveToString() {
    PLY_ASSERT(this->type == Type::Mem);

    if (this->mem.bufferIndex + 1 == this->mem.buffers.numItems()) {
        // Extend number of bytes in the last buffer.
        this->mem.numBytesInLastBuffer =
            max(this->mem.numBytesInLastBuffer, numericCast<u32>(this->curByte - this->mem.buffers.back()));
    }

    if (this->mem.buffers.numItems() == 1) {
        u32 numBytes = this->mem.numBytesInLastBuffer;
        char* bytes = (char*) Heap::realloc(this->mem.buffers[0], numBytes);
        this->mem.~MemData();
        new (this) Stream;
        return String::adopt(bytes, numBytes);
    }

    u32 numBytes = (this->mem.buffers.numItems() - 1) * BUFFER_SIZE + this->mem.numBytesInLastBuffer;
    char* bytes = (char*) Heap::alloc(numBytes);
    for (u32 i = 0; i < this->mem.buffers.numItems(); i++) {
        u32 toCopy = min(BUFFER_SIZE, numBytes - (BUFFER_SIZE * i));
        memcpy(bytes + (BUFFER_SIZE * i), this->mem.buffers[i], toCopy);
    }
    this->close();
    return String::adopt(bytes, numBytes);
}

//--------------------------------------------
ViewStream::ViewStream(StringView view) {
    this->type = Type::View;
    new (&this->view) ViewData;
    this->view.startByte = const_cast<char*>(view.bytes());
    this->curByte = this->view.startByte;
    this->endByte = this->curByte + view.numBytes();
    this->hasReadPermission = true;
}

ViewStream::ViewStream(MutStringView view) {
    this->type = Type::View;
    new (&this->view) ViewData;
    this->view.startByte = view.bytes;
    this->curByte = this->view.startByte;
    this->endByte = this->curByte + view.numBytes;
    this->hasReadPermission = true;
    this->hasWritePermission = true;
}

//  ▄▄▄▄▄                    ▄▄ ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄   ▄▄▄██ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██▀▀█▄ ██▄▄██  ▄▄▄██ ██  ██ ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                         ▄▄▄█▀

String readLine(Stream& in) {
    MemStream mem;
    while (in.makeReadable() && mem.makeWritable()) {
        u32 numBytesRemaining = min(in.numRemainingBytes(), mem.numRemainingBytes());
        for (u32 i = 0; i < numBytesRemaining; i++) {
            char c = *in.curByte++;
            *mem.curByte++ = c;
            if (c == '\n')
                goto done;
        }
    }
done:
    return mem.moveToString();
}

StringView readLine(ViewStream& viewIn) {
    char* startByte = viewIn.curByte;
    while (viewIn.curByte < viewIn.endByte) {
        char c = *viewIn.curByte++;
        if (c == '\n')
            break;
    }
    if (startByte == viewIn.curByte) {
        viewIn.atEof = true;
    }
    return {startByte, viewIn.curByte};
}

String readWhitespace(Stream& in) {
    MemStream mem;
    while (in.makeReadable() && mem.makeWritable()) {
        u32 numBytesRemaining = min(in.numRemainingBytes(), mem.numRemainingBytes());
        for (u32 i = 0; i < numBytesRemaining; i++) {
            char c = *in.curByte;
            if (!isWhite(c))
                goto done;
            in.curByte++;
            *mem.curByte++ = c;
        }
    }
done:
    return mem.moveToString();
}

StringView readWhitespace(ViewStream& viewIn) {
    char* startByte = viewIn.curByte;
    while (viewIn.curByte < viewIn.endByte) {
        char c = *viewIn.curByte;
        if (!isWhite(c))
            break;
        viewIn.curByte++;
    }
    if (startByte == viewIn.curByte) {
        viewIn.atEof = true;
    }
    return {startByte, viewIn.curByte};
}

void skipWhitespace(Stream& in) {
    while (in.makeReadable()) {
        u32 numBytesRemaining = in.numRemainingBytes();
        for (u32 i = 0; i < numBytesRemaining; i++) {
            char c = *in.curByte;
            if (!isWhite(c))
                return;
            in.curByte++;
        }
    }
}

String readIdentifier(Stream& in, u32 flags) {
    bool first = true;
    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    if ((flags & ID_WITH_DOLLAR_SIGN) != 0) {
        mask[1] |= 0x10; // '$'
    }
    if ((flags & ID_WITH_DASH) != 0) {
        mask[1] |= 0x2000; // '-'
    }

    MemStream mem;
    while (in.makeReadable() && mem.makeWritable()) {
        u32 numBytesRemaining = min(in.numRemainingBytes(), mem.numRemainingBytes());
        for (u32 i = 0; i < numBytesRemaining; i++) {
            char c = *in.curByte;
            if ((mask[c >> 5] & (1 << (c & 31))) == 0)
                goto done;
            in.curByte++;
            *mem.curByte++ = c;
            if (first) {
                mask[1] |= 0x3ff0000; // accept digits after first unit
                first = false;
            };
        }
    }
done:
    return mem.moveToString();
}

StringView readIdentifier(ViewStream& viewIn, u32 flags) {
    bool first = true;
    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    if ((flags & ID_WITH_DOLLAR_SIGN) != 0) {
        mask[1] |= 0x10; // '$'
    }
    if ((flags & ID_WITH_DASH) != 0) {
        mask[1] |= 0x2000; // '-'
    }

    char* startByte = viewIn.curByte;
    while (viewIn.curByte < viewIn.endByte) {
        char c = *viewIn.curByte;
        if ((mask[c >> 5] & (1 << (c & 31))) == 0)
            goto done;
        viewIn.curByte++;
        if (first) {
            mask[1] |= 0x3ff0000; // accept digits after first unit
            first = false;
        };
    }
done:
    if (startByte == viewIn.curByte) {
        viewIn.atEof = true;
    }
    return {startByte, viewIn.curByte};
}

u8 digitFromChar(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    char lower = (c | 32);
    if ((lower >= 'a') && (lower <= 'z'))
        return lower - 'a' + 10;
    return 255;
}

u64 readU64FromText(Stream& in, u32 radix) {
    PLY_ASSERT(radix > 0 && radix <= 36);
    u64 result = 0;
    bool anyDigits = false;
    bool overflow = false;
    for (;;) {
        if (!in.makeReadable())
            break;
        u8 digit = digitFromChar(*in.curByte);
        if (digit >= radix)
            break;
        in.curByte++;
        // FIXME: When available, check for (multiplicative & additive) overflow using
        // https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html#Integer-Overflow-Builtins
        // and equivalent intrinsics instead of the following.
        // Note: 0x71c71c71c71c71b is the largest value that won't overflow for any
        // radix <= 36. We test against this constant first to avoid the costly integer
        // division.
        if (result > 0x71c71c71c71c71b && result > (getMaxValue<u64>() - digit) / radix) {
            overflow = true;
        }
        result = result * radix + digit;
        anyDigits = true;
    }
    if (!anyDigits || overflow) {
        in.inputError = true;
        return 0;
    }
    return result;
}

s64 readS64FromText(Stream& in, u32 radix) {
    bool negate = false;

    if (in.makeReadable() && (*in.curByte == '-')) {
        negate = true;
        in.curByte++;
    }

    u64 unsignedComponent = readU64FromText(in, radix);
    if (negate) {
        s64 result = -(s64) unsignedComponent;
        if (result > 0) {
            in.inputError = true;
        }
        return result;
    } else {
        s64 result = unsignedComponent;
        if (result < 0) {
            in.inputError = true;
        }
        return result;
    }
}

struct DoubleComponentOut {
    double result = 0;
    bool anyDigits = false;
};

void readDoubleComponent(DoubleComponentOut* compOut, Stream& in, u32 radix) {
    double value = 0.0;
    double dr = (double) radix;
    for (;;) {
        if (!in.makeReadable())
            break;
        u8 digit = digitFromChar(*in.curByte);
        if (digit >= radix)
            break;
        in.curByte++;
        value = value * dr + digit;
        compOut->anyDigits = true;
    }
    compOut->result = value;
}

double readDoubleFromText(Stream& in, u32 radix) {
    PLY_ASSERT(radix <= 36);
    DoubleComponentOut comp;

    // Parse the optional minus sign
    bool negate = false;
    if (in.makeReadable() && (*in.curByte == '-')) {
        in.curByte++;
        negate = true;
    }

    // Parse the mantissa
    readDoubleComponent(&comp, in, radix);
    double value = comp.result;

    // Parse the optional fractional part
    if (in.makeReadable() && (*in.curByte == '.')) {
        in.curByte++;
        double significance = 1.0;
        u64 numer = 0;
        u64 denom = 1;
        for (;;) {
            if (!in.makeReadable())
                break;
            u8 digit = digitFromChar(*in.curByte);
            if (digit >= radix)
                break;
            in.curByte++;
            u64 denomWithNextDigit = denom * radix;
            if (denomWithNextDigit < denom) {
                // denominator overflowed
                double ooDenom = 1.0 / denom;
                value += significance * numer * ooDenom;
                significance *= ooDenom;
                numer = digit;
                denom = radix;
            } else {
                numer = numer * radix + digit;
                denom = denomWithNextDigit;
            }
        }
        value += significance * numer / denom;
    }

    // Parse optional exponent suffix
    if (comp.anyDigits && in.makeReadable() && ((*in.curByte | 0x20) == 'e')) {
        in.curByte++;
        bool negateExp = false;
        if (in.makeReadable()) {
            if (*in.curByte == '+') {
                in.curByte++;
            } else if (*in.curByte == '-') {
                in.curByte++;
                negateExp = true;
            }
        }
        comp.anyDigits = false;
        readDoubleComponent(&comp, in, radix);
        value *= pow((double) radix, negateExp ? -comp.result : comp.result);
    }

    if (!comp.anyDigits) {
        in.inputError = true;
    }

    return negate ? -value : value;
}

// Reads a fixed-width hexadecimal escape and returns the decoded value.
static bool readHexEscape(Stream& in, u32 numDigits, u32& value) {
    value = 0;
    for (u32 i = 0; i < numDigits; i++) {
        if (!in.makeReadable())
            return false;
        u8 digit = digitFromChar(*in.curByte);
        if (digit >= 16)
            return false;
        value = (value << 4) | digit;
        in.curByte++;
    }
    return true;
}

// Describes the quoting and escape rules for a specific string-literal language.
struct QuotedStringProfile {
    bool allowSingleQuote = false;
    bool allowTripleQuote = false;
    bool allowLineContinuation = false;
    bool allowSlashEscape = false;
    bool allowQuestionMarkEscape = false;
    bool allowBellEscape = false;
    bool allowVerticalTabEscape = false;
    bool allowNullEscape = false;
    bool allowOctalEscape = false;
    bool allowHexEscape = false;
    bool variableLengthHexEscape = false;
    bool allowUEscape = false;
    bool allowBraceUEscape = false;
    bool allowBigUEscape = false;
    bool combineUtf16Surrogates = false;
};

// Returns the parsing profile for a quoted-string language preset.
static QuotedStringProfile getQuotedStringProfile(QuotedStringType type) {
    QuotedStringProfile profile;
    switch (type) {
        case QuotedStringType::C: {
            profile.allowLineContinuation = true;
            profile.allowQuestionMarkEscape = true;
            profile.allowBellEscape = true;
            profile.allowVerticalTabEscape = true;
            profile.allowNullEscape = true;
            profile.allowOctalEscape = true;
            profile.allowHexEscape = true;
            profile.variableLengthHexEscape = true;
            profile.allowUEscape = true;
            profile.allowBigUEscape = true;
            break;
        }

        case QuotedStringType::JavaScript: {
            profile.allowSingleQuote = true;
            profile.allowLineContinuation = true;
            profile.allowSlashEscape = true;
            profile.allowVerticalTabEscape = true;
            profile.allowNullEscape = true;
            profile.allowHexEscape = true;
            profile.allowUEscape = true;
            profile.allowBraceUEscape = true;
            profile.combineUtf16Surrogates = true;
            break;
        }

        case QuotedStringType::JSON: {
            profile.allowSlashEscape = true;
            profile.allowUEscape = true;
            profile.combineUtf16Surrogates = true;
            break;
        }

        case QuotedStringType::Python: {
            profile.allowSingleQuote = true;
            profile.allowTripleQuote = true;
            profile.allowLineContinuation = true;
            profile.allowBellEscape = true;
            profile.allowVerticalTabEscape = true;
            profile.allowNullEscape = true;
            profile.allowOctalEscape = true;
            profile.allowHexEscape = true;
            profile.allowUEscape = true;
            profile.allowBigUEscape = true;
            break;
        }

        default: {
            PLY_ASSERT(0);
            break;
        }
    }
    return profile;
}

// Reads one or more hexadecimal digits and returns false if none were consumed.
// Note: This is very similar to readU64FromText. Revisit later.
static bool readVariableHexEscape(Stream& in, u32& value) {
    value = 0;
    u32 numDigits = 0;
    while (in.makeReadable()) {
        u8 digit = digitFromChar(*in.curByte);
        if (digit >= 16)
            break;
        value = (value << 4) | digit;
        in.curByte++;
        numDigits++;
    }
    return numDigits > 0;
}

// Reads a JavaScript-style braced Unicode escape such as `\u{1f600}`.
static bool readBraceUnicodeEscape(Stream& in, u32& codepoint) {
    codepoint = 0;
    if (!in.makeReadable() || (*in.curByte != '{'))
        return false;
    in.curByte++;
    u32 numDigits = 0;
    while (in.makeReadable()) {
        u8 digit = digitFromChar(*in.curByte);
        if (digit < 16) {
            if (numDigits == 6)
                return false;
            codepoint = (codepoint << 4) | digit;
            in.curByte++;
            numDigits++;
            continue;
        }
        if (*in.curByte == '}') {
            in.curByte++;
            return numDigits > 0;
        }
        return false;
    }
    return false;
}

// Decodes a simple one-character escape if the current profile supports it.
static bool decodeSimpleEscape(const QuotedStringProfile& profile, u8 code, char& result) {
    switch (code) {
        case '\\':
        case '"':
        case '\'':
            result = (char) code;
            return true;
        case '/':
            if (profile.allowSlashEscape) {
                result = '/';
                return true;
            }
            return false;
        case '?':
            if (profile.allowQuestionMarkEscape) {
                result = '?';
                return true;
            }
            return false;
        case 'a':
            if (profile.allowBellEscape) {
                result = '\a';
                return true;
            }
            return false;
        case 'b':
            result = '\b';
            return true;
        case 'f':
            result = '\f';
            return true;
        case 'n':
            result = '\n';
            return true;
        case 'r':
            result = '\r';
            return true;
        case 't':
            result = '\t';
            return true;
        case 'v':
            if (profile.allowVerticalTabEscape) {
                result = '\v';
                return true;
            }
            return false;
        case '0':
            if (profile.allowNullEscape) {
                result = '\0';
                return true;
            }
            return false;
        default:
            return false;
    }
}

String readQuotedString(Stream& in, QuotedStringType type, bool strict,
                        Functor<void(QuotedStringError)> errorCallback) {
    QuotedStringProfile profile = getQuotedStringProfile(type);
    auto handleError = [&](QuotedStringError errorCode) {
        in.inputError = true;
        if (errorCallback) {
            errorCallback(errorCode);
        }
    };

    // Get opening quote
    if (!in.makeReadable()) {
        handleError(QuotedStringError::UnexpectedEndOfFile);
        return {};
    }
    u8 quoteType = *in.curByte;
    if (!(quoteType == '"' || (profile.allowSingleQuote && quoteType == '\''))) {
        handleError(QuotedStringError::NoOpeningQuote);
        return {};
    }
    in.curByte++;

    // Parse rest of quoted string
    MemStream out;
    bool multiline = false;
    if (profile.allowTripleQuote && in.makeReadable(2) && (in.curByte[0] == quoteType) &&
        (in.curByte[1] == quoteType)) {
        multiline = true;
        in.curByte += 2;
    }

    for (;;) {
        if (!in.makeReadable()) {
            handleError(QuotedStringError::UnexpectedEndOfFile);
            break; // end of string
        }

        u8 nextByte = *in.curByte;
        if (nextByte == quoteType) {
            // A matching quote either ends the string or becomes literal content inside a triple-quoted string.
            if (multiline) {
                if (in.makeReadable(3) && (in.curByte[1] == quoteType) && (in.curByte[2] == quoteType)) {
                    in.curByte += 3;
                    break; // end of string
                }
                out.write(quoteType);
                in.curByte++;
            } else {
                in.curByte++;
                break; // end of string
            }
        } else {
            switch (nextByte) {
                case '\r':
                case '\n': {
                    // Newlines are only accepted verbatim when parsing a multiline string.
                    if (multiline) {
                        if (nextByte == '\n') {
                            out.write(nextByte);
                        }
                        in.curByte++;
                    } else {
                        handleError(QuotedStringError::UnexpectedEndOfLine);
                        goto endOfString;
                    }
                    break;
                }

                case '\\': {
                    // Escape sequence
                    const char* escapeStart = in.curByte;
                    in.curByte++;
                    if (!in.makeReadable()) {
                        handleError(QuotedStringError::UnexpectedEndOfFile);
                        goto endOfString;
                    }
                    u8 code = *in.curByte;
                    if (code == '\r' || code == '\n') {
                        // Treat backslash-newline as a line continuation when the language profile permits it.
                        if (profile.allowLineContinuation) {
                            if (code == '\r') {
                                in.curByte++;
                                if (in.makeReadable() && (*in.curByte == '\n'))
                                    in.curByte++;
                            } else {
                                in.curByte++;
                            }
                            break;
                        }
                        handleError(QuotedStringError::UnexpectedEndOfLine);
                        goto endOfString;
                    }

                    char decoded = 0;
                    if (decodeSimpleEscape(profile, code, decoded)) {
                        // Handle the common one-character escapes, with `\0` optionally extending into octal.
                        if ((code == '0') && profile.allowOctalEscape) {
                            u32 codepoint = 0;
                            u32 numDigits = 0;
                            while (in.makeReadable() && numDigits < 3) {
                                u8 digit = *in.curByte - '0';
                                if (digit >= 8)
                                    break;
                                codepoint = (codepoint << 3) | digit;
                                in.curByte++;
                                numDigits++;
                            }
                            out.write((char) codepoint);
                        } else {
                            out.write(decoded);
                            in.curByte++;
                        }
                        break;
                    }

                    if (profile.allowOctalEscape && (code >= '0') && (code <= '7')) {
                        // Parse a standalone octal escape when the profile allows it.
                        u32 codepoint = 0;
                        u32 numDigits = 0;
                        while (in.makeReadable() && numDigits < 3) {
                            u8 digit = *in.curByte - '0';
                            if (digit >= 8)
                                break;
                            codepoint = (codepoint << 3) | digit;
                            in.curByte++;
                            numDigits++;
                        }
                        out.write((char) codepoint);
                        break;
                    }

                    if (code == 'x') {
                        // Parse a byte-oriented hexadecimal escape, preserving the original bytes if recovery is
                        // allowed.
                        in.curByte++;
                        u32 codepoint = 0;
                        bool ok = false;
                        if (profile.allowHexEscape) {
                            ok = profile.variableLengthHexEscape ? readVariableHexEscape(in, codepoint)
                                                                 : readHexEscape(in, 2, codepoint);
                        } else if (!strict) {
                            ok = profile.variableLengthHexEscape ? readVariableHexEscape(in, codepoint)
                                                                 : readHexEscape(in, 2, codepoint);
                        }
                        if (!ok || !encodeUnicode(out, UTF8, codepoint)) {
                            if (strict) {
                                handleError(QuotedStringError::BadEscapeSequence);
                                goto endOfString;
                            }
                            out.write(StringView{escapeStart, in.curByte});
                        } else {
                            continue;
                        }
                        break;
                    }

                    if (code == 'u') {
                        // Parse a Unicode escape, optionally combining UTF-16 surrogate pairs for formats that use
                        // them.
                        in.curByte++;
                        u32 codepoint = 0;
                        bool ok = profile.allowUEscape;
                        bool usedBraceEscape = false;
                        if (ok && profile.allowBraceUEscape && in.makeReadable() && (*in.curByte == '{')) {
                            usedBraceEscape = true;
                            ok = readBraceUnicodeEscape(in, codepoint);
                        } else if (ok) {
                            ok = readHexEscape(in, 4, codepoint);
                        } else if (!strict) {
                            if (in.makeReadable() && (*in.curByte == '{')) {
                                usedBraceEscape = true;
                                ok = readBraceUnicodeEscape(in, codepoint);
                            } else {
                                ok = readHexEscape(in, 4, codepoint);
                            }
                        }
                        if (ok && profile.combineUtf16Surrogates) {
                            if (codepoint >= 0xd800 && codepoint < 0xdc00) {
                                u32 highSurrogate = codepoint;
                                if (!usedBraceEscape && in.makeReadable(6) && (in.curByte[0] == '\\') &&
                                    (in.curByte[1] == 'u')) {
                                    in.curByte += 2;
                                    u32 lowSurrogate = 0;
                                    if (!readHexEscape(in, 4, lowSurrogate) || (lowSurrogate < 0xdc00) ||
                                        (lowSurrogate >= 0xe000)) {
                                        ok = false;
                                    } else {
                                        codepoint =
                                            0x10000 + (((highSurrogate - 0xd800) << 10) | (lowSurrogate - 0xdc00));
                                    }
                                } else {
                                    ok = false;
                                }
                            } else if (codepoint >= 0xdc00 && codepoint < 0xe000) {
                                ok = false;
                            }
                        }

                        if (ok && !encodeUnicode(out, UTF8, codepoint)) {
                            ok = false;
                        }

                        if (!ok) {
                            if (strict) {
                                handleError(QuotedStringError::BadEscapeSequence);
                                goto endOfString;
                            }
                            out.write(StringView{escapeStart, in.curByte});
                        } else {
                            continue;
                        }
                        break;
                    }

                    if (code == 'U') {
                        // Parse an eight-digit Unicode escape used by languages such as C and Python.
                        in.curByte++;
                        u32 codepoint = 0;
                        bool ok = (profile.allowBigUEscape || !strict) && readHexEscape(in, 8, codepoint) &&
                                  encodeUnicode(out, UTF8, codepoint);
                        if (!ok) {
                            if (strict) {
                                handleError(QuotedStringError::BadEscapeSequence);
                                goto endOfString;
                            }
                            out.write(StringView{escapeStart, in.curByte});
                        } else {
                            continue;
                        }
                        break;
                    }

                    if (!strict) {
                        // In permissive mode, keep the escaped byte verbatim when no recognized escape matched.
                        out.write((char) code);
                        in.curByte++;
                        break;
                    }

                    // In strict mode, any remaining escape form is invalid.
                    switch (code) {
                        case '\r':
                        case '\n': {
                            handleError(QuotedStringError::UnexpectedEndOfLine);
                            goto endOfString;
                        }
                        default: {
                            handleError(QuotedStringError::BadEscapeSequence);
                            break;
                        }
                    }
                    in.curByte++;
                    break;
                }

                default: {
                    // Ordinary bytes are copied through unchanged.
                    out.write(nextByte);
                    in.curByte++;
                    break;
                }
            }
        }
    }

endOfString:
    return out.moveToString();
}

//  ▄▄    ▄▄        ▄▄  ▄▄   ▄▄                   ▄▄▄▄▄▄                ▄▄
//  ██ ▄▄ ██ ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄       ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ▀█▄██▄█▀ ██  ▀▀ ██  ██   ██ ██  ██ ██  ██       ██   ██▄▄██  ▀██▀   ██
//   ██▀▀██  ██     ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██       ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄
//                                      ▄▄▄█▀

inline char toDigit(u32 d, bool capitalize = false) {
    const char* digitTable =
        capitalize ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    return (d <= 35) ? digitTable[d] : '?';
}

void printNumber(Stream& out, u64 value, const NumberFormat& format) {
    PLY_ASSERT(format.radix >= 2);
    PLY_ASSERT(format.zeroPad <= 64); // Zero padding must fit in the temporary digit buffer.
    if (format.signMode == NumberFormat::ShowPlus) {
        out.write('+');
    } else if (format.signMode == NumberFormat::LeaveSpaceForSign) {
        out.write(' ');
    }
    char digitBuffer[64];
    s32 digitIndex = PLY_STATIC_ARRAY_SIZE(digitBuffer);

    // Convert digits right-to-left.
    if (value == 0) {
        digitBuffer[--digitIndex] = '0';
    } else {
        while (value > 0) {
            u64 quotient = value / format.radix;
            u32 digit = u32(value - quotient * format.radix);
            digitBuffer[--digitIndex] = toDigit(digit, format.capitalize);
            value = quotient;
        }
    }
    // Add leading zeros.
    while ((u32) PLY_STATIC_ARRAY_SIZE(digitBuffer) - digitIndex < format.zeroPad) {
        digitBuffer[--digitIndex] = '0';
    }
    out.write(StringView{digitBuffer + digitIndex, (u32) PLY_STATIC_ARRAY_SIZE(digitBuffer) - digitIndex});
}

void printNumber(Stream& out, s64 value, const NumberFormat& format) {
    if (value < 0) {
        out.write('-');
        NumberFormat format2 = format;
        format2.signMode = NumberFormat::ShowMinusOnly;
        printNumber(out, (u64) - (value + 1) + 1, format2);
    } else {
        printNumber(out, (u64) value, format);
    }
}

void printNumber(Stream& out, u32 value, const NumberFormat& format) {
    return printNumber(out, (u64) value, format);
}

void printNumber(Stream& out, s32 value, const NumberFormat& format) {
    return printNumber(out, (s64) value, format);
}

static NumberFormat makeNumberFormat(u32 targetNumDigits, u32 radix = 10, bool capitalize = false) {
    NumberFormat format;
    format.radix = radix;
    format.capitalize = capitalize;
    format.zeroPad = targetNumDigits;
    return format;
}

void printNumber(Stream& outs, double value, const NumberFormat& format) {
    PLY_ASSERT(format.radix >= 2);
    PLY_ASSERT(format.fractionalPrecision <= 18); // Precision must fit in a u64 scale.

    // Print the sign and work with the absolute value.
    u64 bits;
    memcpy(&bits, &value, sizeof(bits));
    bool isNegative = ((bits >> 63) != 0);
    if (isNegative) {
        value = -value;
        outs.write('-');
    } else if (format.signMode == NumberFormat::ShowPlus) {
        outs.write('+');
    } else if (format.signMode == NumberFormat::LeaveSpaceForSign) {
        outs.write(' ');
    }

    // Handle nan and inf.
    if (isnan(value)) {
        outs.write(format.capitalize ? "NAN" : "nan");
        return;
    } else if (isinf(value)) {
        outs.write(format.capitalize ? "INF" : "inf");
        return;
    }

    // Autoselect scientific or regular notation.
    NumberFormat::FloatMode floatMode = format.floatMode;
    if (floatMode == NumberFormat::Auto) {
        double minRegular = 0.5 / pow((double) format.radix, format.fractionalPrecision);
        if ((value > 1e10) || (value > 0.0 && value < minRegular)) {
            floatMode = NumberFormat::Scientific;
        } else {
            floatMode = NumberFormat::Regular;
        }
    }

    // Print value using regular or scientific notation.
    if (floatMode == NumberFormat::Regular) {
        u64 scale = 1;
        for (u32 i = 0; i < format.fractionalPrecision; i++) {
            PLY_ASSERT(scale <= getMaxValue<u64>() / format.radix);
            scale *= format.radix;
        }

        // Round after scaling and print exactly fractionalPrecision fractional digits.
        u64 fixedPoint = (u64) (value * (double) scale + 0.5);
        printNumber(outs, fixedPoint / scale, makeNumberFormat(1, format.radix, format.capitalize));
        if (format.fractionalPrecision > 0) {
            outs.write('.');
            printNumber(outs, fixedPoint % scale,
                        makeNumberFormat(format.fractionalPrecision, format.radix, format.capitalize));
        }
    } else if (floatMode == NumberFormat::Scientific) {
        u64 scale = 1;
        for (u32 i = 0; i < format.fractionalPrecision; i++) {
            PLY_ASSERT(scale <= getMaxValue<u64>() / format.radix);
            scale *= format.radix;
        }

        // Round the mantissa and renormalize if it crosses the radix.
        s32 exponent = 0;
        if (value != 0) {
            exponent = (s32) floor(log(value) / log(format.radix));
            value /= pow((double) format.radix, exponent);
        }
        u64 mantissa = (u64) (value * (double) scale + 0.5);
        PLY_ASSERT(scale <= getMaxValue<u64>() / format.radix);
        if (mantissa / format.radix >= scale) {
            mantissa /= format.radix;
            exponent++;
        }
        printNumber(outs, mantissa / scale, makeNumberFormat(1, format.radix, format.capitalize));
        if (format.fractionalPrecision > 0) {
            outs.write('.');
            printNumber(outs, mantissa % scale,
                        makeNumberFormat(format.fractionalPrecision, format.radix, format.capitalize));
        }
        outs.write(format.capitalize ? 'E' : 'e');
        outs.write(exponent < 0 ? '-' : '+');
        printNumber(outs, (u64) abs(exponent), makeNumberFormat(2));
    }
}

void printEscapedString(Stream& out, StringView str) {
    ViewStream vin{str};
    while (vin.hasRemainingBytes()) {
        const char* start = vin.curByte;
        DecodeResult decoded = decodeUnicode(vin, UTF8);
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
                    out.write(StringView{start, vin.curByte});
                } else {
                    out.format("\\{}{}", toDigit((decoded.point >> 4) & 0xf), toDigit(decoded.point & 0xf));
                }
                break;
            }
        }
    }
}

void printXmlEscapedString(Stream& out, StringView str) {
    ViewStream vin{str};
    while (vin.hasRemainingBytes()) {
        const char* start = vin.curByte;
        DecodeResult decoded = decodeUnicode(vin, UTF8);
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
                out.write(StringView{start, vin.curByte});
                break;
            }
        }
    }
}

//--------------------------------------------------------
// String::format
//--------------------------------------------------------

struct FormatSpec {
    char fill = ' ';
    char align = 0;
    char sign = '-';
    u32 width = 0;
    s32 precision = -1;
    char type = 0;
};

FormatSpec parseFormatSpec(StringView fmtSpec) {
    FormatSpec fs;
    u32 pos = 0;

    if ((pos < fmtSpec.numBytes()) && (fmtSpec[pos] == ':'))
        pos++;
    PLY_ASSERT(pos <= fmtSpec.numBytes()); // Invalid format specifier.

    // Parse fill-and-align. The fill character is a single byte.
    bool hasFillAndAlign = (pos + 1 < fmtSpec.numBytes()) &&
                           (fmtSpec[pos + 1] == '<' || fmtSpec[pos + 1] == '>' || fmtSpec[pos + 1] == '^');
    if (hasFillAndAlign) {
        fs.fill = fmtSpec[pos++];
        fs.align = fmtSpec[pos++];
    } else if ((pos < fmtSpec.numBytes()) && (fmtSpec[pos] == '<' || fmtSpec[pos] == '>' || fmtSpec[pos] == '^')) {
        fs.align = fmtSpec[pos++];
    }

    // Parse sign and zero-fill shorthand.
    if ((pos < fmtSpec.numBytes()) && (fmtSpec[pos] == '+' || fmtSpec[pos] == '-' || fmtSpec[pos] == ' ')) {
        fs.sign = fmtSpec[pos++];
    }
    if ((pos < fmtSpec.numBytes()) && (fmtSpec[pos] == '0')) {
        fs.fill = '0';
        if (!fs.align)
            fs.align = '>';
        pos++;
    }

    // Parse width, precision and type.
    while ((pos < fmtSpec.numBytes()) && isDigit(fmtSpec[pos])) {
        fs.width = fs.width * 10 + fmtSpec[pos++] - '0';
    }
    if ((pos < fmtSpec.numBytes()) && (fmtSpec[pos] == '.')) {
        fs.precision = 0;
        pos++;
        PLY_ASSERT(pos < fmtSpec.numBytes() && isDigit(fmtSpec[pos])); // Missing precision.
        while ((pos < fmtSpec.numBytes()) && isDigit(fmtSpec[pos])) {
            fs.precision = fs.precision * 10 + fmtSpec[pos++] - '0';
        }
    }
    if (pos < fmtSpec.numBytes())
        fs.type = fmtSpec[pos++];
    PLY_ASSERT(pos == fmtSpec.numBytes()); // Invalid format specifier.
    return fs;
}

// Converts a parsed FormatSpec into a NumberFormat.
NumberFormat makeNumberFormat(const FormatSpec& fs) {
    NumberFormat format;
    char type = fs.type ? fs.type : 'd';
    if (type == 'b' || type == 'B') {
        format.radix = 2;
    } else if (type == 'o') {
        format.radix = 8;
    } else if (type == 'd') {
        format.radix = 10;
    } else {
        format.radix = 16;
    }
    format.capitalize = (type >= 'A' && type <= 'Z');
    format.zeroPad = fs.precision >= 0 ? fs.precision : 0;
    return format;
}

void printArg(Stream& out, StringView fmtSpec, const FormatArg& arg) {
    FormatSpec fs = parseFormatSpec(fmtSpec);
    MemStream mem;
    if (arg.var.is<StringView>()) {
        PLY_ASSERT(!fs.type || fs.type == 's' || fs.type == '&'); // Invalid format type for string.
        StringView str = *arg.var.as<StringView>();
        if (fs.precision >= 0)
            str = str.left(min(str.numBytes(), (u32) fs.precision));
        if (fs.type == '&') {
            printXmlEscapedString(mem, str);
        } else {
            mem.write(str);
        }
    } else if (arg.var.is<bool>()) {
        PLY_ASSERT(!fs.type || fs.type == 's'); // Invalid format type for bool.
        mem.write(*arg.var.as<bool>() ? "true" : "false");
    } else if (arg.var.is<s64>()) {
        char type = fs.type ? fs.type : 'd';
        PLY_ASSERT(type == 'b' || type == 'B' || type == 'o' || type == 'd' || type == 'x' || type == 'X');
        NumberFormat format = makeNumberFormat(fs);
        s64 value = *arg.var.as<s64>();
        if (value < 0) {
            mem.write('-');
            printNumber(mem, (u64) - (value + 1) + 1, format);
        } else {
            if (fs.sign != '-')
                mem.write(fs.sign);
            printNumber(mem, (u64) value, format);
        }
    } else if (arg.var.is<u64>()) {
        char type = fs.type ? fs.type : 'd';
        PLY_ASSERT(type == 'b' || type == 'B' || type == 'o' || type == 'd' || type == 'x' || type == 'X');
        if (fs.sign != '-')
            mem.write(fs.sign);
        printNumber(mem, *arg.var.as<u64>(), makeNumberFormat(fs));
    } else if (arg.var.is<double>()) {
        char type = fs.type ? fs.type : (fs.precision >= 0 ? 'f' : 0);
        PLY_ASSERT(!type || type == 'f' || type == 'F' || type == 'e' ||
                   type == 'E'); // Invalid format type for double.
        NumberFormat format;
        if (fs.sign == '+')
            format.signMode = NumberFormat::ShowPlus;
        else if (fs.sign == ' ')
            format.signMode = NumberFormat::LeaveSpaceForSign;
        format.capitalize = type == 'F' || type == 'E';
        if (type) {
            format.floatMode = (type == 'e' || type == 'E') ? NumberFormat::Scientific : NumberFormat::Regular;
            format.fractionalPrecision = fs.precision >= 0 ? fs.precision : 6;
        }
        printNumber(mem, *arg.var.as<double>(), format);
    } else {
        PLY_ASSERT(0); // Invalid argument type.
    }

    String str = mem.moveToString();
    u32 width = max(str.numBytes(), fs.width);
    u32 padding = width - str.numBytes();
    char align = fs.align ? fs.align : arg.var.is<StringView>() ? '<' : '>';

    // Add padding around the already-rendered argument.
    if (align == '<') {
        out.write(str);
        for (u32 i = 0; i < padding; i++)
            out.write(fs.fill);
    } else if (align == '^') {
        for (u32 i = 0; i < padding / 2; i++)
            out.write(fs.fill);
        out.write(str);
        for (u32 i = 0; i < padding - padding / 2; i++)
            out.write(fs.fill);
    } else if (fs.fill == '0' && str.numBytes() > 0 && (str[0] == '-' || str[0] == '+' || str[0] == ' ')) {
        out.write(str[0]);
        for (u32 i = 0; i < padding; i++)
            out.write(fs.fill);
        out.write(str.substr(1));
    } else {
        for (u32 i = 0; i < padding; i++)
            out.write(fs.fill);
        out.write(str);
    }
}

void formatWithArgs(Stream& out, StringView fmt, ArrayView<const FormatArg> args) {
    u32 argIndex = 0;
    u32 pos = 0;
    while (pos < fmt.numBytes()) {
        char c = fmt[pos++];
        if (c == '{') {
            u32 specStart = pos;
            PLY_ASSERT(pos < fmt.numBytes()); // Missing '}' after '{'.
            if (fmt[pos] == '{') {
                pos++;
                out.write('{');
            } else {
                do {
                    PLY_ASSERT(pos < fmt.numBytes()); // Missing '}' after '{'.
                } while (fmt[pos++] != '}');
                PLY_ASSERT(argIndex < args.numItems()); // Not enough arguments for format string.
                printArg(out, fmt.substr(specStart, pos - 1 - specStart), args[argIndex]);
                argIndex++;
            }
        } else if (c == '}') {
            PLY_ASSERT((pos < fmt.numBytes()) && (fmt[pos] == '}')); // '}' must be followed by another '}'.
            pos++;
            out.write('}');
        } else {
            out.write(c);
        }
    }
    PLY_ASSERT(argIndex == args.numItems()); // Too many arguments for format string.
}

//   ▄▄▄▄   ▄▄                     ▄▄                   ▄▄     ▄▄▄▄     ▄▄  ▄▄▄▄
//  ██  ▀▀ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██      ██     ▄█▀ ██  ██
//   ▀▀▀█▄  ██    ▄▄▄██ ██  ██ ██  ██  ▄▄▄██ ██  ▀▀ ██  ██      ██   ▄█▀   ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ▀█▄▄██ ██  ██ ▀█▄▄██ ▀█▄▄██ ██     ▀█▄▄██     ▄██▄ ██     ▀█▄▄█▀
//

#if defined(PLY_WINDOWS)

Pipe* getStdInPipe() {
    static PipeHandle inPipe{GetStdHandle(STD_INPUT_HANDLE), Pipe::HAS_READ_PERMISSION};
    return &inPipe;
}

Pipe* getStdOutPipe() {
    static PipeHandle outPipe{GetStdHandle(STD_OUTPUT_HANDLE), Pipe::HAS_WRITE_PERMISSION};
    return &outPipe;
}

Pipe* getStdErrPipe() {
    static PipeHandle errorPipe{GetStdHandle(STD_ERROR_HANDLE), Pipe::HAS_WRITE_PERMISSION};
    return &errorPipe;
}

#elif defined(PLY_POSIX)

Pipe* getStdInPipe() {
    static Pipe_FD inPipe{STDIN_FILENO, Pipe::HAS_READ_PERMISSION};
    return &inPipe;
}

Pipe* getStdOutPipe() {
    static Pipe_FD outPipe{STDOUT_FILENO, Pipe::HAS_WRITE_PERMISSION};
    return &outPipe;
}

Pipe* getStdErrPipe() {
    static Pipe_FD errorPipe{STDERR_FILENO, Pipe::HAS_WRITE_PERMISSION};
    return &errorPipe;
}

#endif

Stream getStdIn(ConsoleMode mode) {
    if (mode == ConsoleMode::TEXT) {
        Stream in{getStdInPipe(), false};
        // Always create a filter to make newlines consistent.
        return {Heap::create<InPipeNewLineFilter>(std::move(in)), true};
    } else {
        return {getStdInPipe(), false};
    }
}

Stream getStdOut(ConsoleMode mode) {
    Stream out{getStdOutPipe(), false};
    bool writeCrlf = false;
#if defined(PLY_WINDOWS)
    writeCrlf = true;
#endif
    // Always create a filter to make newlines consistent.
    return {Heap::create<OutPipeNewLineFilter>(std::move(out), writeCrlf), true};
}

Stream getStdErr(ConsoleMode mode) {
    Stream out{getStdErrPipe(), false};
    bool writeCrlf = false;
#if defined(PLY_WINDOWS)
    writeCrlf = true;
#endif
    // Always create a filter to make newlines consistent.
    return {Heap::create<OutPipeNewLineFilter>(std::move(out), writeCrlf), true};
}

//  ▄▄                         ▄▄
//  ██     ▄▄▄▄   ▄▄▄▄▄  ▄▄▄▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██    ██  ██ ██  ██ ██  ██ ██ ██  ██ ██  ██
//  ██▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██
//                ▄▄▄█▀  ▄▄▄█▀            ▄▄▄█▀

void logMessageInternal(StringView fmt, ArrayView<const FormatArg> args) {
    Stream out = getStdErr();
    formatWithArgs(out, fmt, args);
    if (!fmt.endsWith('\n')) {
        out.write('\n');
    }
}

//   ▄▄▄▄                                       ▄▄       ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄  ▄▄   ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄     ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██     ██  ██ ██  ██ ▀█▄ ▄█▀ ██▄▄██ ██  ▀▀  ██       ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██  ██   ▀█▀   ▀█▄▄▄  ██      ▀█▄▄     ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

u32 encodeUnicode(FixedArray<char, 4>& buf, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams) {
    if (unicodeType == NOT_UNICODE) {
        s32 c;
        if (extParams) {
            // Use lookup table.
            if (u8* value = extParams->reverseLut.find(codepoint)) {
                c = *value;
            } else {
                c = extParams->missingChar;
            }
        } else {
            // Encode this codepoint directly as a byte.
            c = max((s32) codepoint, 255);
        }
        if (c < 0)
            return 0; // Optionally skip unrepresentable character.
        buf[0] = (char) c;
        return 1;

    } else if (unicodeType == UTF8) {
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
    } else if (unicodeType == UTF16_BE) {
#else
    } else if (unicodeType == UTF16_LE) {
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
    } else if (unicodeType == UTF16_LE) {
#else
    } else if (unicodeType == UTF16_BE) {
#endif
        if (codepoint < 0x10000) {
            // Note: 0xd800 to 0xd8ff are invalid Unicode codepoints reserved for UTF-16
            // surrogates. Such codepoints will simply be written as unpaired
            // surrogates.
            *(u16*) &buf[0] = reverseBytes((u16) codepoint);
            return 2;
        } else {
            // Codepoints >= 0x10000 are encoded as a pair of surrogate units.
            u32 adjusted = codepoint - 0x10000;
            *(u16*) &buf[0] = reverseBytes(u16(0xd800 + ((adjusted >> 10) & 0x3ff)));
            *(u16*) &buf[2] = reverseBytes(u16(0xdc00 + (adjusted & 0x3ff)));
            return 4;
        }
    } else {
        // Shouldn't get here.
        PLY_ASSERT(0);
    }

    return false;
}

bool encodeUnicode(Stream& out, UnicodeType unicodeType, u32 codepoint, ExtendedTextParams* extParams) {
    out.makeWritable();
    if (out.numRemainingBytes() >= 4) {
        // Encode directly into the output buffer.
        u32 numBytes = encodeUnicode(*(FixedArray<char, 4>*) out.curByte, unicodeType, codepoint, extParams);
        out.curByte += numBytes;
        return true;
    } else {
        // Encode into a temporary buffer.
        FixedArray<char, 4> buf;
        u32 numBytes = encodeUnicode(buf, unicodeType, codepoint, extParams);
        // Write the encoded bytes to the output stream.
        out.write({&buf[0], numBytes});
        return !out.atEof;
    }
}

DecodeResult decodeUnicode(StringView str, UnicodeType unicodeType, ExtendedTextParams* extParams) {
    if (str.isEmpty())
        return {-1, 0, DS_NOT_ENOUGH_DATA};

    if (unicodeType == NOT_UNICODE) {
        u8 b = (u8) str.bytes()[0];
        if (extParams) // Use lookup table if available.
            return {extParams->lut[b], 1, DS_OK};
        return {b, 1, DS_OK};
    } else if (unicodeType == UTF8) {
        // (Note: Ill-formed encodings are interpreted as sequences of individual bytes.)
        s32 value = 0;
        u32 numContinuationBytes = 0;
        u8 b = (u8) str.bytes()[0];

        if (b < 0x80) {
            // 1-byte encoding: 0xxxxxxx
            return {b, 1, DS_OK};
        } else if (b < 0xc0) {
            // Unexpected continuation byte: 10xxxxxx
            return {b, 1, DS_ILL_FORMED};
        } else if (b < 0xe0) {
            // 2-byte encoding: 110xxxxx 10xxxxxx
            value = b & 0x1f;
            numContinuationBytes = 1;
        } else if (b < 0xf0) {
            // 3-byte encoding: 1110xxxx 10xxxxxx 10xxxxxx
            value = b & 0xf;
            numContinuationBytes = 2;
        } else if (b < 0xf8) {
            // 4-byte encoding: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            value = b & 0x7;
            numContinuationBytes = 3;
        } else {
            // Illegal byte.
            return {b, 1, DS_ILL_FORMED};
        }

        if (str.numBytes() < numContinuationBytes + 1) {
            // Not enough bytes in buffer for continuation bytes.
            return {b, 1, DS_NOT_ENOUGH_DATA};
        }

        for (u32 i = 0; i < numContinuationBytes; i++) {
            u8 c = (u8) str.bytes()[i + 1];
            if ((c >> 6) != 2) // Must be a continuation byte
                return {b, 1, DS_ILL_FORMED};
            value = (value << 6) | ((u8) str.bytes()[i + 1] & 0x3f);
        }

        return {value, numContinuationBytes + 1, DS_OK};
#if PLY_IS_BIG_ENDIAN
    } else if (unicodeType == UTF16_BE) {
#else
    } else if (unicodeType == UTF16_LE) {
#endif
        if (str.numBytes() < 2) {
            return {-1, 0, DS_NOT_ENOUGH_DATA};
        }

        u16 first = *(const u16*) &str.bytes()[0];

        if (first >= 0xd800 && first < 0xdc00) {
            if (str.numBytes() < 4) {
                // A second 16-bit surrogate is expected, but not enough data.
                return {first, 2, DS_NOT_ENOUGH_DATA};
            }
            u16 second = *(const u16*) &str.bytes()[2];
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
    } else if (unicodeType == UTF16_LE) {
#else
    } else if (unicodeType == UTF16_BE) {
#endif
        if (str.numBytes() < 2) {
            return {-1, 0, DS_NOT_ENOUGH_DATA};
        }

        u16 first = reverseBytes(*(const u16*) &str.bytes()[0]);

        if (first >= 0xd800 && first < 0xdc00) {
            if (str.numBytes() < 4) {
                // A second 16-bit surrogate is expected, but not enough data.
                return {first, 2, DS_NOT_ENOUGH_DATA};
            }
            u16 second = reverseBytes(*(const u16*) &str.bytes()[2]);
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

DecodeResult decodeUnicode(Stream& in, UnicodeType unicodeType, ExtendedTextParams* extParams) {
    // Try to get at least four bytes to read.
    in.makeReadable(4);
    if (!in.hasRemainingBytes())
        return {-1, 0, DS_NOT_ENOUGH_DATA};

    DecodeResult result = decodeUnicode(in.viewRemainingBytes(), unicodeType, extParams);
    in.curByte += result.numBytes;
    return result;
}

//--------------------------------------------------------------

bool copyFromShim(Stream& dstOut, StringView& shimUsed) {
    if (shimUsed) {
        u32 toCopy = min(dstOut.numRemainingBytes(), shimUsed.numBytes());
        dstOut.write(shimUsed);
        shimUsed = shimUsed.substr(toCopy);
        if (shimUsed)
            return true; // Destination buffer is full.
    }
    shimUsed = {};
    return false;
}

// Fill dstBuf with UTF-8-encoded data.
u32 InPipeConvertUnicode::read(MutStringView dstBuf) {
    ViewStream dstOut{dstBuf};

    // If the shim contains data, copy it first.
    if (copyFromShim(dstOut, this->shimUsed))
        return dstBuf.numBytes; // Destination buffer is full.

    while (true) {
        // Decode a codepoint from input stream.
        s32 codepoint = decodeUnicode(this->in, this->srcType).point;
        if (codepoint < 0)
            break; // Reached EOF.

        // Convert codepoint to UTF-8.
        u32 w = dstOut.numRemainingBytes();
        if (w >= 4) {
            encodeUnicode(dstOut, UTF8, codepoint);
        } else {
            // Use shim as an intermediate buffer.
            ViewStream s{this->shimStorage.mutStringView()};
            encodeUnicode(s, UTF8, codepoint);
            this->shimUsed =
                StringView{this->shimStorage.items(), numericCast<u32>(s.curByte - this->shimStorage.items())};
            if (copyFromShim(dstOut, this->shimUsed))
                break; // Destination buffer is full.
        }
    }

    return numericCast<u32>(dstOut.curByte - dstBuf.bytes);
}

// srcBuf expects UTF-8-encoded data.
bool OutPipeConvertUnicode::write(StringView srcBuf) {
    ViewStream srcIn{srcBuf};

    // If the shim contains data, join it with the source buffer.
    if (this->shimUsed > 0) {
        u32 numBytesAppended = min(srcBuf.numBytes(), 4 - this->shimUsed);
        memcpy(this->shimStorage + this->shimUsed, srcBuf.bytes(), numBytesAppended);
        this->shimUsed += numBytesAppended;

        // Decode a codepoint from the shim using UTF-8.
        ViewStream s{StringView{this->shimStorage, this->shimUsed}};
        DecodeResult decoded = decodeUnicode(s, UTF8, nullptr);
        if (decoded.status == DS_NOT_ENOUGH_DATA) {
            PLY_ASSERT(numBytesAppended == srcBuf.numBytes());
            return true; // Not enough data available in shim.
        }

        // Convert codepoint to the destination encoding.
        encodeUnicode(this->childOut, this->dstType, decoded.point, this->extParams);

        // Skip ahead in the source buffer and clear the shim.
        srcIn.curByte += numBytesAppended;
        this->shimUsed = 0;
    }

    while (!this->childOut.atEof) {
        // Decode a codepoint from the source buffer using UTF-8.
        DecodeResult decoded = decodeUnicode(srcIn, UTF8, nullptr);
        if (decoded.status == DS_NOT_ENOUGH_DATA) {
            // Not enough data available. Copy the rest of the source buffer to shim,
            // including the previous byte consumed by decode().
            this->shimUsed = srcIn.numRemainingBytes() + 1;
            PLY_ASSERT(this->shimUsed < 4);
            memcpy(this->shimStorage, srcIn.curByte - 1, this->shimUsed);
            return true;
        }

        // Convert codepoint to the destination encoding.
        encodeUnicode(this->childOut, this->dstType, decoded.point, this->extParams);
    }

    return false; // We reached the end of the Stream.
}

void OutPipeConvertUnicode::flush(bool toDevice) {
    // The shim may still contain an incomplete (thus invalid) UTF-8 sequence.
    for (u32 i = 0; i < this->shimUsed; i++) {
        // Interpret each byte as a separate codepoint.
        encodeUnicode(this->childOut, this->dstType, (u8) this->shimStorage[i], this->extParams);
    }
    this->shimUsed = 0;

    // Forward flush command down the output chain.
    this->childOut.flush(toDevice);
}

//  ▄▄▄▄▄▄                ▄▄         ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄       ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██         ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ▄▄▄▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

TextFormat get_default_utf8_format() {
    TextFormat tff;
#if defined(PLY_WINDOWS)
    tff.newLine = TextFormat::NewLine::CRLF;
#endif
    return tff;
}

struct TextFileStats {
    u32 numPoints = 0;
    u32 numValidPoints = 0;
    u32 totalPointValue = 0; // This value won't be accurate if byte encoding is detected
    u32 numLines = 0;
    u32 numCrlf = 0;
    u32 numControl = 0; // non-whitespace points < 32, including nulls
    u32 numNull = 0;
    u32 numPlainAscii = 0; // includes whitespace, excludes control characters < 32
    u32 numWhitespace = 0;
    u32 numExtended = 0;
    float ooNumPoints = 0.f;

    u32 numInvalidPoints() const {
        return this->numPoints - this->numValidPoints;
    }
    TextFormat::NewLine getNewLineType() const {
        PLY_ASSERT(this->numCrlf <= this->numLines);
        if (this->numCrlf == 0 || this->numCrlf * 2 < this->numLines) {
            return TextFormat::NewLine::LF;
        } else {
            return TextFormat::NewLine::CRLF;
        }
    }
    float getScore() const {
        return (2.5f * this->numWhitespace + this->numPlainAscii - 100.f * this->numInvalidPoints() -
                50.f * this->numControl + 5.f * this->numExtended) *
               this->ooNumPoints;
    }
};

u32 scanTextFile(TextFileStats* stats, Stream& in, UnicodeType unicodeType, u32 maxBytes) {
    bool prevWasCr = false;
    while (in.getSeekPos() < maxBytes) {
        DecodeResult decoded = decodeUnicode(in, unicodeType, nullptr);
        if (decoded.point < 0)
            break; // EOF/error
        stats->numPoints++;
        if (decoded.status == DS_OK) {
            stats->numValidPoints++;
            stats->totalPointValue += decoded.point;
            if (decoded.point < 32) {
                if (decoded.point == '\n') {
                    stats->numPlainAscii++;
                    stats->numLines++;
                    stats->numWhitespace++;
                    if (prevWasCr) {
                        stats->numCrlf++;
                    }
                } else if (decoded.point == '\t') {
                    stats->numPlainAscii++;
                    stats->numWhitespace++;
                } else if (decoded.point == '\r') {
                    stats->numPlainAscii++;
                } else {
                    stats->numControl++;
                    if (decoded.point == 0) {
                        stats->numNull++;
                    }
                }
            } else if (decoded.point < 127) {
                stats->numPlainAscii++;
                if (decoded.point == ' ') {
                    stats->numWhitespace++;
                }
            } else if (decoded.point >= 65536) {
                stats->numExtended++;
            }
        }
        prevWasCr = (decoded.point == '\r');
    }
    if (stats->numPoints > 0) {
        stats->ooNumPoints = 1.f / stats->numPoints;
    }
    return numericCast<u32>(in.getSeekPos());
}

static constexpr u32 NumBytesForTextFormatDetection = 100000;

TextFormat guessFileEncoding(Stream& in) {
    TextFileStats stats8;

    // Try UTF8 first:
    u32 numBytesRead = scanTextFile(&stats8, in, UTF8, NumBytesForTextFormatDetection);
    if (numBytesRead == 0) {
        // Empty file
        return {UTF8, TextFormat::NewLine::LF, false};
    }
    in.seekTo(0);
    if (stats8.numInvalidPoints() == 0 && stats8.numControl == 0) {
        // No UTF-8 encoding errors, and no weird control characters/nulls. Pick UTF-8.
        return {UTF8, stats8.getNewLineType(), false};
    }

    // If more than 20% of the high bytes in UTF-8 are encoding errors, reinterpret
    // UTF-8 as just bytes.
    UnicodeType encoding8 = UTF8;
    {
        u32 numHighBytes = numBytesRead - stats8.numPlainAscii - stats8.numControl;
        if (stats8.numInvalidPoints() >= numHighBytes * 0.2f) {
            // Too many UTF-8 errors. Consider it bytes.
            encoding8 = NOT_UNICODE;
            stats8.numPoints = numBytesRead;
            stats8.numValidPoints = numBytesRead;
        }
    }

    // Examine both UTF16 endianness:
    TextFileStats stats16_le;
    scanTextFile(&stats16_le, in, UTF16_LE, NumBytesForTextFormatDetection);
    in.seekTo(0);

    TextFileStats stats16_be;
    scanTextFile(&stats16_be, in, UTF16_BE, NumBytesForTextFormatDetection);
    in.seekTo(0);

    // Choose the better UTF16 candidate:
    TextFileStats* stats = &stats16_le;
    UnicodeType encoding = UTF16_LE;
    if (stats16_be.getScore() > stats16_le.getScore()) {
        stats = &stats16_be;
        encoding = UTF16_BE;
    }

    // Choose between the UTF16 and 8-bit encoding:
    if (stats8.getScore() >= stats->getScore()) {
        stats = &stats8;
        encoding = encoding8;
    }

    // Return best guess
    return {encoding, stats->getNewLineType(), false};
}

TextFormat autodetectTextFormat(Stream& in) {
    TextFormat tff;
    tff.bom = false;
    in.makeReadable(3);
    if (in.viewRemainingBytes().left(3) == "\xef\xbb\xbf") {
        in.curByte += 3;
        tff.unicodeType = UTF8;
        tff.bom = true;
    } else if (in.viewRemainingBytes().left(2) == "\xff\xfe") {
        in.curByte += 2;
        tff.unicodeType = UTF16_LE;
        tff.bom = true;
    } else if (in.viewRemainingBytes().left(2) == "\xfe\xff") {
        in.curByte += 2;
        tff.unicodeType = UTF16_BE;
        tff.bom = true;
    }
    if (!tff.bom) {
        return guessFileEncoding(in);
    } else {
        // Detect LF or CRLF
        TextFileStats stats;
        scanTextFile(&stats, in, tff.unicodeType, NumBytesForTextFormatDetection);
        in.seekTo(0);
        tff.newLine = stats.getNewLineType();
        return tff;
    }
}

//-----------------------------------------------------------------------

Owned<Pipe> createImporter(Stream&& in, const TextFormat& enc) {
    if (enc.bom) {
        in.makeReadable(3);
        if (enc.unicodeType == UTF8) {
            if (in.viewRemainingBytes().left(3) == "\xef\xbb\xbf") {
                in.curByte += 3;
            }
        } else if (enc.unicodeType == UTF16_LE) {
            if (in.viewRemainingBytes().left(2) == "\xff\xfe") {
                in.curByte += 2;
            }
        } else if (enc.unicodeType == UTF16_BE) {
            if (in.viewRemainingBytes().left(2) == "\xfe\xff") {
                in.curByte += 2;
            }
        } else {
            PLY_ASSERT(0); // NON_UNICODE shouldn't have a BOM
        }
    }

    // Install converter from UTF-16 if needed
    Stream importer;
    if (enc.unicodeType == UTF8) {
        importer = std::move(in);
    } else {
        importer = Stream{Heap::create<InPipeConvertUnicode>(std::move(in), enc.unicodeType), true};
    }

    // Install newline filter (basically just eats \r)
    return Heap::create<InPipeNewLineFilter>(std::move(importer));
}

Owned<OutPipeNewLineFilter> createExporter(Stream&& out, const TextFormat& enc) {
    Stream exporter = std::move(out);

    switch (enc.unicodeType) {
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

    return Heap::create<OutPipeNewLineFilter>(std::move(exporter), enc.newLine == TextFormat::CRLF);
}

//-----------------------------------------------------------------------
// WStringView
//-----------------------------------------------------------------------
struct WStringView {
    const char16_t* units = nullptr;
    u32 numUnits = 0;

    WStringView() = default;
    WStringView(const char16_t* units, u32 numUnits) : units{units}, numUnits{numUnits} {
    }
    StringView rawBytes() const {
        return {(const char*) this->units, this->numUnits << 1};
    }
#if defined(PLY_WINDOWS)
    WStringView(LPCWSTR units) : units{(const char16_t*) units}, numUnits{numericCast<u32>(wcslen(units))} {
    }
    WStringView(LPCWSTR units, u32 numUnits) : units{(const char16_t*) units}, numUnits{numUnits} {
    }
#endif
};

//-----------------------------------------------------------------------
// WString
//-----------------------------------------------------------------------
struct WString {
    using View = WStringView;
    char16_t* units = nullptr;
    u32 numUnits = 0;

    WString() = default;
    WString(WString&& other) : units{other.units}, numUnits{other.numUnits} {
        other.units = nullptr;
        other.numUnits = 0;
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
    static WString moveFromString(String&& other) {
        PLY_ASSERT(isAlignedToPowerOf2(uptr(other.bytes()), 2));
        PLY_ASSERT(isAlignedToPowerOf2(other.numBytes(), 2));
        WString result;
        result.numUnits = other.numBytes() >> 1;
        result.units = (char16_t*) other.release();
        return result;
    }

    bool includesNullTerminator() const {
        return this->numUnits > 0 && this->units[this->numUnits - 1] == 0;
    }
    static WString allocate(u32 numUnits) {
        WString result;
        result.units = (char16_t*) Heap::alloc(numUnits << 1);
        result.numUnits = numUnits;
        return result;
    }

#if defined(PLY_WINDOWS)
    operator LPWSTR() const {
        PLY_ASSERT(this->includesNullTerminator()); // must be null terminated
        return (LPWSTR) this->units;
    }
#endif
};

WString toWstring(StringView str) {
    ViewStream string{str};
    MemStream origMemOut;
    OutPipeConvertUnicode encoder{std::move(origMemOut), UTF16_LE};
    encoder.write(str);
    encoder.flush(false);
    MemStream* memOut = static_cast<MemStream*>(&encoder.childOut);
    nativeWrite(*memOut, (u16) 0); // Null terminator
    return WString::moveFromString(memOut->moveToString());
}

String fromWstring(WStringView str) {
    InPipeConvertUnicode decoder{ViewStream{str.rawBytes()}, UTF16_LE};
    MemStream out;
    while (out.makeWritable()) {
        MutStringView buf{out.curByte, out.endByte};
        u32 numBytes = decoder.read(buf);
        if (numBytes == 0)
            break;
        out.curByte += numBytes;
    }
    return out.moveToString();
}

//  ▄▄▄▄▄          ▄▄   ▄▄
//  ██  ██  ▄▄▄▄  ▄██▄▄ ██▄▄▄
//  ██▀▀▀   ▄▄▄██  ██   ██  ██
//  ██     ▀█▄▄██  ▀█▄▄ ██  ██
//

#if defined(PLY_WINDOWS)

String getCurrentExecutablePath() {
    u32 numUnits = 1024;
    for (;;) {
        WString wstr = WString::allocate(numUnits);
        DWORD rc = GetModuleFileNameW(NULL, (LPWSTR) wstr.units, numUnits);
        if (rc < numUnits) {
            WStringView wsubstr = {wstr.units, rc};
            if (wsubstr.numUnits >= 4 && wsubstr.rawBytes().left(8) == StringView{(const char*) L"\\\\?\\", 8}) {
                // Drop leading "\\\\?\\":
                wsubstr.units += 4;
                wsubstr.numUnits -= 4;
            }
            return fromWstring(wsubstr);
        }
        numUnits *= 2;
    }
}

#elif defined(PLY_LINUX)

String getCurrentExecutablePath() {
    u32 numBytes = 1024;
    for (;;) {
        String str = String::allocate(numBytes);
        ssize_t rc = readlink("/proc/self/exe", str.bytes(), numBytes);
        if (rc < 0) {
            return {};
        }
        if ((u32) rc < numBytes) {
            return str.left(rc);
        }
        numBytes *= 2;
    }
}

#elif defined(PLY_APPLE)

String getCurrentExecutablePath() {
    u32 numBytes = 1024;
    String str = String::allocate(numBytes);
    if (_NSGetExecutablePath(str.bytes(), &numBytes) != 0) {
        // numBytes now contains the required size
        str = String::allocate(numBytes);
        _NSGetExecutablePath(str.bytes(), &numBytes);
    }
    return String{str.bytes()}; // Trim to null terminator
}

#endif

inline bool isSepByte(PathFormat fmt, char c) {
    return c == '/' || (fmt == WindowsPath && c == '\\');
}

StringView getDriveLetter(PathFormat fmt, StringView path) {
    if (fmt != WindowsPath)
        return {};
    if (path.numBytes() < 2)
        return {};
    char d = path.bytes()[0];
    bool driveIsAsciiLetter = (d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z');
    if (driveIsAsciiLetter && path.bytes()[1] == ':') {
        return path.left(2);
    }
    return {};
}

bool isAbsolutePath(PathFormat fmt, StringView path) {
    if (fmt == WindowsPath) {
        return (path.numBytes() >= 3) && getDriveLetter(fmt, path) && isSepByte(fmt, path[2]);
    } else {
        return (path.numBytes() >= 1) && isSepByte(fmt, path[0]);
    }
}

String makeAbsolutePath(PathFormat fmt, StringView path) {
    if (isAbsolutePath(fmt, path))
        return path;
    return joinPath(fmt, Filesystem::getWorkingDirectory(), path);
}

SplitPath splitPath(PathFormat fmt, StringView path) {
    s32 lastSepIndex = path.reverseFind([&](char c) { return isSepByte(fmt, c); });
    if (lastSepIndex >= 0) {
        s32 prefixLen = path.reverseFind([&](char c) { return !isSepByte(fmt, c); }, lastSepIndex) + 1;
        if (path.left(prefixLen) == getDriveLetter(fmt, path)) {
            prefixLen++; // If prefix is the root, include a separator character
        }
        return {path.left(prefixLen), path.substr(lastSepIndex + 1)};
    } else {
        return {String{}, path};
    }
}

SplitExtension splitFileExtension(PathFormat fmt, StringView path) {
    StringView lastComp = path;
    s32 slashPos = lastComp.reverseFind([&](char c) { return isSepByte(fmt, c); });
    if (slashPos >= 0) {
        lastComp = lastComp.substr(slashPos + 1);
    }
    s32 dotPos = lastComp.reverseFind([](u32 c) { return c == '.'; });
    if (dotPos < 0 || dotPos == 0) {
        dotPos = lastComp.numBytes();
    }
    return {lastComp.left(dotPos), lastComp.substr(dotPos)};
}

Array<StringView> splitPathFull(PathFormat fmt, StringView path) {
    Array<StringView> result;
    if (getDriveLetter(fmt, path)) {
        if (isAbsolutePath(fmt, path)) {
            // Root with drive letter
            result.append(path.left(3));
            path = path.substr(3);
            while (path.numBytes() > 0 && isSepByte(fmt, path[0])) {
                path = path.substr(1);
            }
        } else {
            // Drive letter only
            result.append(path.left(2));
            path = path.substr(2);
        }
    } else if (path.numBytes() > 0 && isSepByte(fmt, path[0])) {
        // Starts with path separator
        result.append(path.left(1));
        path = path.substr(1);
        while (path.numBytes() > 0 && isSepByte(fmt, path[0])) {
            path = path.substr(1);
        }
    }
    if (path.numBytes() > 0) {
        for (;;) {
            PLY_ASSERT(path.numBytes() > 0);
            PLY_ASSERT(!isSepByte(fmt, path[0]));
            s32 sepPos = path.find([&](char c) { return isSepByte(fmt, c); });
            if (sepPos < 0) {
                result.append(path);
                break;
            }
            result.append(path.left(sepPos));
            path = path.substr(sepPos);
            s32 nonSepPos = path.find([&](char c) { return !isSepByte(fmt, c); });
            if (nonSepPos < 0) {
                // Empty final component
                result.append({});
                break;
            }
            path = path.substr(nonSepPos);
        }
    }
    return result;
}

struct PathComponentIterator {
    char firstComp[3] = {0};

    void iterateOver(PathFormat fmt, ArrayView<const StringView> components,
                     const Functor<void(StringView)>& callback) {
        s32 absoluteIndex = -1;
        s32 driveLetterIndex = -1;
        for (s32 i = components.numItems() - 1; i >= 0; i--) {
            if (absoluteIndex < 0 && isAbsolutePath(fmt, components[i])) {
                absoluteIndex = i;
            }
            if (getDriveLetter(fmt, components[i])) {
                driveLetterIndex = i;
                break;
            }
        }

        // Special first component if there's a drive letter and/or absolute component:
        if (driveLetterIndex >= 0) {
            firstComp[0] = components[driveLetterIndex][0];
            firstComp[1] = ':';
            if (absoluteIndex >= 0) {
                firstComp[2] = getPathSeparator(fmt);
                callback(StringView{firstComp, 3});
            } else {
                callback(StringView{firstComp, 2});
            }
        }

        // Choose component to start iterating from:
        u32 i = driveLetterIndex >= 0 ? driveLetterIndex : 0;
        if (absoluteIndex >= 0) {
            PLY_ASSERT((u32) absoluteIndex >= i);
            i = absoluteIndex;
            if (driveLetterIndex < 0) {
                PLY_ASSERT(firstComp[0] == 0);
                firstComp[0] = getPathSeparator(fmt);
                callback(StringView{firstComp, 1});
            }
        }

        // Iterate over components. Remember, we've already sent the drive letter and/or
        // initial slash as its own component (if any).
        for (; i < components.numItems(); i++) {
            StringView comp = components[i];
            if ((s32) i == driveLetterIndex) {
                comp = comp.substr(2);
            }

            s32 nonSep = comp.find([fmt](char c) { return !isSepByte(fmt, c); });
            while (nonSep >= 0) {
                s32 sep = comp.find([fmt](char c) { return isSepByte(fmt, c); }, nonSep + 1);
                if (sep < 0) {
                    callback(comp.substr(nonSep));
                    break;
                } else {
                    callback(comp.substr(nonSep, sep - nonSep));
                    nonSep = comp.find([fmt](char c) { return !isSepByte(fmt, c); }, sep + 1);
                }
            }
        }
    }

    // Note: Keep the PathComponentIterator alive while using the return value
    Array<StringView> getNormalizedComps(PathFormat fmt, ArrayView<const StringView> components) {
        Array<StringView> normComps;
        u32 upCount = 0;
        this->iterateOver(fmt, components, [&](StringView comp) { //
            if (comp == "..") {
                if (normComps.numItems() > upCount) {
                    normComps.pop();
                } else {
                    PLY_ASSERT(normComps.numItems() == upCount);
                    normComps.append("..");
                }
            } else if (comp != "." && !comp.isEmpty()) {
                normComps.append(comp);
            }
        });
        return normComps;
    }
};

String joinPathFromArray(PathFormat fmt, ArrayView<const StringView> components) {
    PathComponentIterator compIter;
    Array<StringView> normComps = compIter.getNormalizedComps(fmt, components);
    if (normComps.isEmpty()) {
        if (components.numItems() > 0 && components.back().isEmpty()) {
            return StringView{"."} + getPathSeparator(fmt);
        } else {
            return ".";
        }
    } else {
        MemStream out;
        bool needSep = false;
        for (StringView comp : normComps) {
            if (needSep) {
                out.write(getPathSeparator(fmt));
            } else {
                if (comp.numBytes() > 0) {
                    needSep = !isSepByte(fmt, comp[comp.numBytes() - 1]);
                }
            }
            out.write(comp);
        }
        if ((components.back().isEmpty() || isSepByte(fmt, components.back().back())) && needSep) {
            out.write(getPathSeparator(fmt));
        }
        return out.moveToString();
    }
}

String makeRelativePath(PathFormat fmt, StringView ancestor, StringView descendant) {
    // This function requires either both absolute paths or both relative paths:
    PLY_ASSERT(isAbsolutePath(fmt, ancestor) == isAbsolutePath(fmt, descendant));

    // FIXME: Implement fastpath when descendant starts with ancestor and there are no
    // ".", ".." components.

    PathComponentIterator ancestorCompIter;
    Array<StringView> ancestorComps = ancestorCompIter.getNormalizedComps(fmt, {ancestor});
    PathComponentIterator descendantCompIter;
    Array<StringView> descendantComps = descendantCompIter.getNormalizedComps(fmt, {descendant});

    // Determine number of matching components
    u32 mc = 0;
    while (mc < ancestorComps.numItems() && mc < descendantComps.numItems()) {
        if (ancestorComps[mc] != descendantComps[mc])
            break;
        mc++;
    }

    // Determine number of ".." to output (will be 0 if drive letters mismatch)
    u32 upFolders = 0;
    if (!isAbsolutePath(fmt, ancestor) || mc > 0) {
        upFolders = ancestorComps.numItems() - mc;
    }

    // Form relative path (or absolute path if drive letters mismatch)
    MemStream out;
    bool needSep = false;
    for (u32 i = 0; i < upFolders; i++) {
        if (needSep) {
            out.write(getPathSeparator(fmt));
        }
        out.write("..");
        needSep = true;
    }
    for (u32 i = mc; i < descendantComps.numItems(); i++) {
        if (needSep) {
            out.write(getPathSeparator(fmt));
        }
        out.write(descendantComps[i]);
        needSep = !isSepByte(fmt, descendantComps[i].back());
    }

    // .
    if (out.getSeekPos() == 0) {
        out.write(".");
        needSep = true;
    }

    // Trailing slash
    if (descendant.numBytes() > 0 && isSepByte(fmt, descendant.back()) && needSep) {
        out.write(getPathSeparator(fmt));
    }

    return out.moveToString();
}

//-------------------------------------------------------------
// Filename globbing
//-------------------------------------------------------------

// Returns true if a POSIX bracket character class contains the byte.
static bool globAsciiClassMatches(StringView name, char c) {
    u8 uc = (u8) c;
    if (name == "alnum")
        return isAlpha(c) || isDigit(c);
    if (name == "alpha")
        return isAlpha(c);
    if (name == "blank")
        return c == ' ' || c == '\t';
    if (name == "cntrl")
        return uc < 0x20 || uc == 0x7f;
    if (name == "digit")
        return isDigit(c);
    if (name == "graph")
        return uc >= 0x21 && uc <= 0x7e;
    if (name == "lower")
        return (c >= 'a') && (c <= 'z');
    if (name == "print")
        return uc >= 0x20 && uc <= 0x7e;
    if (name == "punct")
        return (uc >= 0x21 && uc <= 0x7e) && !isAlpha(c) && !isDigit(c);
    if (name == "space")
        return isWhite(c);
    if (name == "upper")
        return (c >= 'A') && (c <= 'Z');
    if (name == "xdigit")
        return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    return false;
}

// Returns true if there are an odd number of backslashes before pos.
bool isEscaped(StringView str, u32 pos) {
    u32 backslashCount = 0;
    while ((pos > 0) && (str[pos - 1] == '\\')) {
        backslashCount++;
        pos--;
    }
    return (backslashCount % 2) == 1;
}

// Returns true if the string is matched by a glob pattern.
// The pattern must not contain any unescaped path separators.
// Pattern components:
// * : match zero or more arbitrary UTF-8 encoded characters
// ? : match a single UTF-8 encoded character
// [a-z] : match a character range
// [[:digit:]] : match a character class
bool matchGlobPattern(StringView str, StringView pattern) {
    // Advance by one UTF-8 encoded character.
    auto nextUTF8Char = [](StringView str, u32 pos) -> u32 {
        return pos + decodeUnicode(str.substr(pos), UTF8).numBytes;
    };

    // Scan both strings from the beginning of the current string views.
    u32 si = 0;
    u32 pi = 0;
    while (pi < pattern.numBytes()) {
        // A star matches zero or more characters, so try the rest of the pattern at each position.
        if (pattern[pi] == '*') {
            while ((pi < pattern.numBytes()) && (pattern[pi] == '*'))
                pi++;
            if (pi == pattern.numBytes())
                return true;
            for (; si < str.numBytes(); si = nextUTF8Char(str, si)) {
                if (matchGlobPattern(str.substr(si), pattern.substr(pi)))
                    return true;
            }
            return matchGlobPattern(str.substr(si), pattern.substr(pi));
        }

        // If the pattern still has non-star content, the string must have a character to match.
        if (si == str.numBytes())
            return false;

        // A question mark consumes exactly one UTF-8 encoded character.
        if (pattern[pi] == '?') {
            si = nextUTF8Char(str, si);
            pi++;
            continue;
        }

        // A backslash escapes the next pattern byte.
        if ((pattern[pi] == '\\') && (pi + 1 < pattern.numBytes())) {
            pi++;
        } else if (pattern[pi] == '[') {
            // Parse a bracket expression and test it against the current string byte.
            u32 i = pi + 1;
            bool inverted = (i < pattern.numBytes()) && (pattern[i] == '!');
            bool matched = false;
            i += inverted;
            while ((i < pattern.numBytes()) && (pattern[i] != ']')) {
                // Check for POSIX-style ASCII character classes such as [[:digit:]].
                if ((i + 3 < pattern.numBytes()) && (pattern[i] == '[') && (pattern[i + 1] == ':')) {
                    u32 end = i + 2;
                    while ((end + 1 < pattern.numBytes()) && !((pattern[end] == ':') && (pattern[end + 1] == ']')))
                        end++;
                    if (end + 1 < pattern.numBytes()) {
                        matched |= globAsciiClassMatches(pattern.substr(i + 2, end - i - 2), str[si]);
                        i = end + 2;
                        continue;
                    }
                }

                // Check for a range subexpression.
                if ((i + 2 < pattern.numBytes()) && (pattern[i + 1] == '-') && (pattern[i + 2] != ']')) {
                    // Match a byte range within the bracket expression.
                    matched |= str[si] >= pattern[i] && str[si] <= pattern[i + 2];
                    i += 3;
                } else {
                    // Match a literal byte within the bracket expression.
                    matched |= str[si] == pattern[i];
                    i++;
                }
            }

            // If the bracket expression matched, consume the string character and the expression.
            if ((i < pattern.numBytes()) && (matched == !inverted)) {
                si = nextUTF8Char(str, si);
                pi = i + 1;
                continue;
            }

            // A closed bracket expression that didn't match makes the whole pattern fail.
            if (i < pattern.numBytes())
                return false;
        }

        // Match a literal byte after escape and bracket handling have had their chance.
        if (str[si++] != pattern[pi++])
            return false;
    }

    // The pattern matched only if it consumed the entire string.
    return si == str.numBytes();
}

// Returns true if the path components match the gitignore pattern components.
struct GitIgnorePatternMatcher {
    Array<StringView> pathComps;
    Array<StringView> patternComps;
    bool dirOnly = false;
    bool isDir = false;
};

static bool matchGitIgnorePatternComps(GitIgnorePatternMatcher& matcher, u32 pathIndex, u32 patternIndex) {
    // Check if every pattern component already matched.
    if (patternIndex == matcher.patternComps.numItems()) {
        // Yes. Non-directory-only patterns can match files or directories.
        if (!matcher.dirOnly)
            return true;

        // If the original path is a directory, the directory-only pattern matched it directly.
        if (matcher.isDir)
            return true;

        // Otherwise, the match is only directory-like if there are unmatched path components.
        bool matchHasRemainingPathComps = pathIndex < matcher.pathComps.numItems();
        return matchHasRemainingPathComps;
    }

    // A double-star component can match zero or more path components.
    if (matcher.patternComps[patternIndex] == "**") {
        // A trailing double-star matches the rest of the path.
        if (patternIndex + 1 == matcher.patternComps.numItems())
            return matcher.isDir || (pathIndex < matcher.pathComps.numItems());

        // Try matching the rest of the pattern at each possible remaining path position.
        for (u32 i = pathIndex; i <= matcher.pathComps.numItems(); i++) {
            if (matchGitIgnorePatternComps(matcher, i, patternIndex + 1))
                return true;
        }

        // No expansion of the double-star allowed the remaining pattern to match.
        return false;
    }

    // Ordinary components must match the current path component before advancing both.
    return (pathIndex < matcher.pathComps.numItems()) &&
           matchGlobPattern(matcher.pathComps[pathIndex], matcher.patternComps[patternIndex]) &&
           matchGitIgnorePatternComps(matcher, pathIndex + 1, patternIndex + 1);
}

// pattern must use forward slashes, even on Windows.
// relativePath uses the native path format.
bool matchGitIgnorePattern(StringView relativePath, bool isDir, StringView pattern) {
    GitIgnorePatternMatcher matcher;
    matcher.isDir = isDir;

    // Split the relative path into components.
    matcher.pathComps = splitPathFull(relativePath);

    // Check if the pattern ends with unescaped '/'.
    if (pattern.endsWith("/") && !isEscaped(pattern, pattern.numBytes() - 1)) {
        // It ends with '/'. Trim the '/' and note that the pattern should only match directories.
        matcher.dirOnly = true;
        pattern = pattern.shortenedBy(1);
    }

    // Split the pattern into components.
    bool anySlash = false;
    u32 startPos = 0;
    for (u32 i = 0; i < pattern.numBytes(); i++) {
        if ((pattern[i] == '/') && !isEscaped(pattern, i)) {
            // It's an unescaped '/'.
            anySlash = true;
            if (i > startPos) {
                matcher.patternComps.append(pattern.substr(startPos, i - startPos));
            }
            // Skip multiple slashes.
            do {
                i++;
            } while ((i + 1 < pattern.numBytes()) && (pattern[i + 1] == '/'));
            startPos = i;
        }
    }
    // Add the final component.
    if (pattern.numBytes() > startPos) {
        matcher.patternComps.append(pattern.substr(startPos));
    }

    // An empty pattern can't match anything.
    if (matcher.patternComps.isEmpty())
        return false;

    // Patterns containing slashes are matched against the entire relative path.
    if (anySlash)
        return matchGitIgnorePatternComps(matcher, 0, 0);

    // Patterns without slashes can match any path component.
    for (u32 i = 0; i < matcher.pathComps.numItems(); i++) {
        bool nameMatches = matchGlobPattern(matcher.pathComps[i], matcher.patternComps[0]);
        bool matchedCompIsDir = matcher.isDir || (i + 1 < matcher.pathComps.numItems());
        bool dirOnlyPatternMatches = !matcher.dirOnly || matchedCompIsDir;
        if (nameMatches && dirOnlyPatternMatches)
            return true;
    }

    // No path component matched the pattern.
    return false;
}

//  ▄▄▄▄▄ ▄▄ ▄▄▄                               ▄▄
//  ██    ▄▄  ██   ▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄▄▄
//  ██▀▀  ██  ██  ██▄▄██ ▀█▄▄▄  ██  ██ ▀█▄▄▄   ██   ██▄▄██ ██ ██ ██
//  ██    ██ ▄██▄ ▀█▄▄▄   ▄▄▄█▀ ▀█▄▄██  ▄▄▄█▀  ▀█▄▄ ▀█▄▄▄  ██ ██ ██
//                               ▄▄▄█▀

WString win32_path_arg(StringView path, bool allowExtended = true) {
    ViewStream pathIn{path};
    MemStream out;
    if (allowExtended && isAbsolutePath(WindowsPath, path)) {
        out.write(ArrayView<const char16_t>{u"\\\\?\\", 4}.stringView());
    }
    while (true) {
        s32 codepoint = decodeUnicode(pathIn, UTF8).point;
        if (codepoint < 0)
            break;
        if (codepoint == '/') {
            codepoint = '\\'; // Fix slashes.
        }
        encodeUnicode(out, UTF16_LE, codepoint);
    }
    nativeWrite(out, (u16) 0); // Null terminator.
    return WString::moveFromString(out.moveToString());
}

ThreadLocal<FSResult> Filesystem::lastResult_;

void DirectoryWalker::visit(StringView dirPath) {
    this->triple.dirPath = dirPath;
    this->triple.dirNames.clear();
    this->triple.files.clear();
    for (DirectoryEntry& entry : Filesystem::listDir(dirPath)) {
        if (entry.isDir) {
            this->triple.dirNames.append(std::move(entry.name));
        } else {
            this->triple.files.append(std::move(entry));
        }
    }
}

void DirectoryWalker::Iterator::operator++() {
    if (!this->walker->triple.dirNames.isEmpty()) {
        StackItem& item = this->walker->stack.append();
        item.path = std::move(this->walker->triple.dirPath);
        item.dirNames = std::move(this->walker->triple.dirNames);
        item.dirIndex = 0;
    } else {
        this->walker->triple.dirPath.clear();
        this->walker->triple.dirNames.clear();
        this->walker->triple.files.clear();
    }
    while (!this->walker->stack.isEmpty()) {
        StackItem& item = this->walker->stack.back();
        if (item.dirIndex < item.dirNames.numItems()) {
            this->walker->visit(joinPath(item.path, item.dirNames[item.dirIndex]));
            item.dirIndex++;
            return;
        }
        this->walker->stack.pop();
    }
    // End of walk
    PLY_ASSERT(this->walker->triple.dirPath.isEmpty());
}

FSResult Filesystem::copyFile(StringView srcPath, StringView dstPath) {
    Owned<Pipe> in = Filesystem::openPipeForRead(srcPath);
    if (Filesystem::lastResult() != FS_OK)
        return Filesystem::lastResult();
    PLY_ASSERT(in);

    Stream out = Filesystem::openBinaryForWrite(dstPath);
    if (Filesystem::lastResult() != FS_OK)
        return Filesystem::lastResult();
    PLY_ASSERT(out.isOpen());

    for (;;) {
        out.makeWritable();
        u32 numBytesRead = in->read(out.viewRemainingBytesMut());
        if (numBytesRead == 0)
            break;
        out.curByte += numBytesRead;
    }

    // FIXME: More robust, detect bad copies
    return FS_OK;
}

DirectoryWalker Filesystem::walk(StringView top) {
    DirectoryWalker walker;
    walker.visit(top);
    return walker;
}

FSResult Filesystem::makeDirs(StringView path) {
    if (path == getDriveLetter(path)) {
        return Filesystem::setLastResult(FS_OK);
    }
    ExistsResult er = Filesystem::exists(path);
    if (er == ER_DIRECTORY) {
        return Filesystem::setLastResult(FS_ALREADY_EXISTS);
    } else if (er == ER_FILE) {
        return Filesystem::setLastResult(FS_ACCESS_DENIED);
    } else {
        SplitPath split = splitPath(path);
        if (!split.directory.isEmpty() && !split.filename.isEmpty()) {
            FSResult r = makeDirs(split.directory);
            if (r != FS_OK && r != FS_ALREADY_EXISTS)
                return r;
        }
        return Filesystem::makeDir(path);
    }
}

Stream Filesystem::openBinaryForRead(StringView path) {
    return {Filesystem::openPipeForRead(path).release(), true};
}

Stream Filesystem::openBinaryForWrite(StringView path) {
    return {Filesystem::openPipeForWrite(path).release(), true};
}

Stream Filesystem::openTextForRead(StringView path, const TextFormat& textFormat) {
    if (Stream in = Filesystem::openBinaryForRead(path))
        return {createImporter(std::move(in), textFormat).release(), true};
    return {};
}

Stream Filesystem::openTextForReadAutodetect(StringView path, TextFormat* outFormat) {
    if (Stream in = Filesystem::openBinaryForRead(path)) {
        TextFormat textFormat = autodetectTextFormat(in);
        if (outFormat) {
            *outFormat = textFormat;
        }
        return {createImporter(std::move(in), textFormat).release(), true};
    }
    return {};
}

String Filesystem::loadBinary(StringView path) {
    String result;
    Owned<Pipe> inPipe = Filesystem::openPipeForRead(path);
    if (inPipe) {
        u64 fileSize = inPipe->getFileSize();
        // Files >= 4GB cannot be loaded this way:
        result.resize(numericCast<u32>(fileSize));
        inPipe->read({result.bytes(), result.numBytes()});
    }
    return result;
}

String readAllRemainingBytes(Pipe* inPipe) {
    MemStream mem;
    for (;;) {
        mem.makeWritable();
        u32 numBytesRead = inPipe->read(mem.viewRemainingBytesMut());
        if (numBytesRead == 0)
            break;
        mem.curByte += numBytesRead;
    }
    return mem.moveToString();
}

String Filesystem::loadText(StringView path, const TextFormat& textFormat) {
    if (Stream in = Filesystem::openBinaryForRead(path)) {
        Owned<Pipe> importer = createImporter(std::move(in), textFormat);
        return readAllRemainingBytes(importer);
    }
    return {};
}

String Filesystem::loadTextAutodetect(StringView path, TextFormat* outFormat) {
    if (Stream in = Filesystem::openBinaryForRead(path)) {
        TextFormat textFormat = autodetectTextFormat(in);
        if (outFormat) {
            *outFormat = textFormat;
        }

        Owned<Pipe> importer = createImporter(std::move(in), textFormat);
        return readAllRemainingBytes(importer);
    }
    return {};
}

Stream Filesystem::openTextForWrite(StringView path, const TextFormat& textFormat) {
    if (Stream out = Filesystem::openBinaryForWrite(path))
        return {createExporter(std::move(out), textFormat).release(), true};
    return {};
}

FSResult Filesystem::saveBinary(StringView path, StringView view) {
    // FIXME: Write to temporary file first, then rename atomically
    Owned<Pipe> outPipe = Filesystem::openPipeForWrite(path);
    FSResult result = Filesystem::lastResult();
    if (result != FS_OK) {
        return result;
    }
    outPipe->write(view);
    return result;
}

FSResult Filesystem::saveText(StringView path, StringView strContents, const TextFormat& enc) {
    Owned<OutPipeNewLineFilter> exporter = createExporter(MemStream{}, enc);
    exporter->write(strContents);
    exporter->flush(false);
    String rawData = static_cast<MemStream*>(&exporter->out)->moveToString();
    return Filesystem::saveBinary(path, rawData);
}

#if defined(PLY_WINDOWS)

//-----------------------------------------------
// Windows
//-----------------------------------------------

#define PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS 0

ReadWriteLock Filesystem::workingDirLock;

inline double windowsToPosixTime(const FILETIME& fileTime) {
    return (u64(fileTime.dwHighDateTime) << 32 | fileTime.dwLowDateTime) / 10000000.0 - 11644473600.0;
}

void dirEntryFromData(DirectoryEntry* entry, WIN32_FIND_DATAW findData) {
    entry->name = fromWstring(findData.cFileName);
    entry->isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry->fileSize = u64(findData.nFileSizeHigh) << 32 | findData.nFileSizeLow;
    entry->creationTime = windowsToPosixTime(findData.ftCreationTime);
    entry->accessTime = windowsToPosixTime(findData.ftLastAccessTime);
    entry->modificationTime = windowsToPosixTime(findData.ftLastWriteTime);
}

Array<DirectoryEntry> Filesystem::listDir(StringView path) {
    Array<DirectoryEntry> result;
    HANDLE hfind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findData;

    String pattern = joinPath(WindowsPath, path, "*");
    hfind = FindFirstFileW(win32_path_arg(pattern), &findData);
    if (hfind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME: {
                Filesystem::setLastResult(FS_NOT_FOUND);
                return result;
            }
            case ERROR_ACCESS_DENIED: {
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                return result;
            }
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                return result;
            }
        }
    }

    while (true) {
        DirectoryEntry entry;
        dirEntryFromData(&entry, findData);
        if (entry.name != "." && entry.name != "..") {
            result.append(std::move(entry));
        }

        BOOL rc = FindNextFileW(hfind, &findData);
        if (!rc) {
            DWORD err = GetLastError();
            switch (err) {
                case ERROR_NO_MORE_FILES: {
                    Filesystem::setLastResult(FS_OK);
                    return result;
                }
                case ERROR_FILE_INVALID: {
                    Filesystem::setLastResult(FS_NOT_FOUND);
                    return result;
                }
                default: {
                    PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                    Filesystem::setLastResult(FS_UNKNOWN);
                    return result;
                }
            }
        }
    }
}

FSResult Filesystem::makeDir(StringView path) {
    BOOL rc = CreateDirectoryW(win32_path_arg(path), NULL);
    if (rc) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_ALREADY_EXISTS:
                return Filesystem::setLastResult(FS_ALREADY_EXISTS);
            case ERROR_ACCESS_DENIED:
                return Filesystem::setLastResult(FS_ACCESS_DENIED);
            case ERROR_INVALID_NAME:
                return Filesystem::setLastResult(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::setLastResult(FS_UNKNOWN);
            }
        }
    }
}

FSResult Filesystem::setWorkingDirectory(StringView path) {
    BOOL rc;
    {
        // This ReadWriteLock is used to mitigate data race issues with
        // SetCurrentDirectoryW:
        // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
        Filesystem::workingDirLock.lockExclusive();
        rc = SetCurrentDirectoryW(win32_path_arg(path));
        Filesystem::workingDirLock.unlockExclusive();
    }
    if (rc) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_PATH_NOT_FOUND:
                return Filesystem::setLastResult(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::setLastResult(FS_UNKNOWN);
            }
        }
    }
}

String Filesystem::getWorkingDirectory() {
    u32 numUnitsWithNullTerm = MAX_PATH + 1;
    for (;;) {
        WString win32_path = WString::allocate(numUnitsWithNullTerm);
        DWORD rc;
        {
            // This ReadWriteLock is used to mitigate data race issues with
            // SetCurrentDirectoryW:
            // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcurrentdirectory
            Filesystem::workingDirLock.lockShared();
            rc = GetCurrentDirectoryW(numUnitsWithNullTerm, (LPWSTR) win32_path.units);
            Filesystem::workingDirLock.unlockShared();
        }
        if (rc == 0) {
            PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
            Filesystem::setLastResult(FS_UNKNOWN);
            return {};
        }
        PLY_ASSERT(rc != numUnitsWithNullTerm);
        if (rc < numUnitsWithNullTerm) {
            // GetCurrentDirectoryW: If the function succeeds, the return value
            // specifies the number of characters that are written to the buffer, not
            // including the terminating null character.
            WStringView truncated_win32_path = {win32_path.units, rc};
            if (truncated_win32_path.numUnits >= 4 &&
                truncated_win32_path.rawBytes().left(8) == StringView{(const char*) L"\\\\?\\", 8}) {
                // Drop leading "\\\\?\\":
                truncated_win32_path.units += 4;
                truncated_win32_path.numUnits -= 4;
            }
            Filesystem::setLastResult(FS_OK);
            return fromWstring(truncated_win32_path);
        }
        // GetCurrentDirectoryW: If the buffer that is pointed to by lpBuffer is not
        // large enough, the return value specifies the required size of the buffer, in
        // characters, including the null-terminating character.
        numUnitsWithNullTerm = rc;
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

HANDLE Filesystem::openHandleForRead(StringView path) {
    // Should this use FILE_SHARE_DELETE or FILE_SHARE_WRITE?
    HANDLE handle = CreateFileW(win32_path_arg(path), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE) {
        Filesystem::setLastResult(FS_OK);
    } else {
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
                Filesystem::setLastResult(FS_NOT_FOUND);
                break;

            case ERROR_SHARING_VIOLATION:
                Filesystem::setLastResult(FS_LOCKED);
                break;

            case ERROR_ACCESS_DENIED:
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                break;
        }
    }
    return handle;
}

Owned<Pipe> Filesystem::openPipeForRead(StringView path) {
    HANDLE handle = openHandleForRead(path);
    if (handle == INVALID_HANDLE_VALUE)
        return nullptr;
    return Heap::create<PipeHandle>(handle, Pipe::HAS_READ_PERMISSION | Pipe::CAN_SEEK);
}

HANDLE Filesystem::openHandleForWrite(StringView path) {
    // FIXME: Needs graceful handling of ERROR_SHARING_VIOLATION
    // Should this use FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE?
    HANDLE handle =
        CreateFileW(win32_path_arg(path), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE) {
        Filesystem::setLastResult(FS_OK);
    } else {
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_NAME:
                Filesystem::setLastResult(FS_NOT_FOUND);
                break;

            case ERROR_SHARING_VIOLATION:
                Filesystem::setLastResult(FS_LOCKED);
                break;

            case ERROR_ACCESS_DENIED:
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                break;
        }
    }
    return handle;
}

Owned<Pipe> Filesystem::openPipeForWrite(StringView path) {
    HANDLE handle = openHandleForWrite(path);
    if (handle == INVALID_HANDLE_VALUE)
        return nullptr;
    return Heap::create<PipeHandle>(handle, Pipe::HAS_WRITE_PERMISSION | Pipe::CAN_SEEK);
}

FSResult Filesystem::moveFile(StringView srcPath, StringView dstPath) {
    BOOL rc = MoveFileExW(win32_path_arg(srcPath), win32_path_arg(dstPath), MOVEFILE_REPLACE_EXISTING);
    if (rc) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        DWORD error = GetLastError();
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::setLastResult(FS_UNKNOWN);
    }
}

FSResult Filesystem::deleteFile(StringView path) {
    BOOL rc = DeleteFileW(win32_path_arg(path));
    if (rc) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        DWORD err = GetLastError();
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::setLastResult(FS_UNKNOWN);
    }
}

FSResult Filesystem::removeDirTree(StringView dirPath) {
    String absPath = dirPath;
    if (!isAbsolutePath(WindowsPath, dirPath)) {
        absPath = joinPath(WindowsPath, Filesystem::getWorkingDirectory(), dirPath);
    }
    OutPipeConvertUnicode out{MemStream{}, UTF16_LE};
    out.write(absPath);
    out.childOut.write({"\0\0\0\0", 4}); // double null terminated
    MemStream* memOut = static_cast<MemStream*>(&out.childOut);
    WString wstr = WString::moveFromString(memOut->moveToString());
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

DirectoryEntry Filesystem::getFileInfo(HANDLE handle) {
    DirectoryEntry entry;
    FILETIME creationTime = {0, 0};
    FILETIME lastAccessTime = {0, 0};
    FILETIME lastWriteTime = {0, 0};
    BOOL rc = GetFileTime(handle, &creationTime, &lastAccessTime, &lastWriteTime);
    if (rc) {
        entry.creationTime = windowsToPosixTime(creationTime);
        entry.accessTime = windowsToPosixTime(lastAccessTime);
        entry.modificationTime = windowsToPosixTime(lastWriteTime);
    } else {
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        entry.result = FS_UNKNOWN;
    }

    LARGE_INTEGER fileSize;
    rc = GetFileSizeEx(handle, &fileSize);
    if (rc) {
        entry.fileSize = fileSize.QuadPart;
    } else {
        PLY_ASSERT(PLY_FSWIN32_ALLOW_UNKNOWN_ERRORS);
        entry.result = FS_UNKNOWN;
    }

    entry.result = FS_OK;
    Filesystem::setLastResult(FS_OK);
    return entry;
}

DirectoryEntry Filesystem::getFileInfo(StringView path) {
    HANDLE handle = Filesystem::openHandleForRead(path);
    if (handle == INVALID_HANDLE_VALUE) {
        DirectoryEntry entry;
        entry.result = Filesystem::lastResult();
        return entry;
    }

    DirectoryEntry entry = Filesystem::getFileInfo(handle);
    CloseHandle(handle);
    return entry;
}

#elif defined(PLY_POSIX)

//-----------------------------------------------
// POSIX
//-----------------------------------------------

#define PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS 0

Array<DirectoryEntry> Filesystem::listDir(StringView path) {
    Array<DirectoryEntry> result;

    DIR* dir = opendir((path + '\0').bytes());
    if (!dir) {
        switch (errno) {
            case ENOENT: {
                Filesystem::setLastResult(FS_NOT_FOUND);
                return result;
            }
            case EACCES: {
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                return result;
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                return result;
            }
        }
    }

    while (true) {
        errno = 0;
        struct dirent* rde = readdir(dir);
        if (!rde) {
            if (errno == 0) {
                Filesystem::setLastResult(FS_OK);
            } else {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
            }
            break;
        }

        DirectoryEntry entry;
        entry.name = rde->d_name;

        // d_type is not POSIX, but it exists on OSX and Linux.
        if (rde->d_type == DT_REG) {
            entry.isDir = false;
        } else if (rde->d_type == DT_DIR) {
            if (rde->d_name[0] == '.') {
                if (rde->d_name[1] == 0 || (rde->d_name[1] == '.' && rde->d_name[2] == 0))
                    continue;
            }
            entry.isDir = true;
        }

        // Get additional file information
        String joinedPath = joinPath(POSIXPath, path, entry.name);
        struct stat buf;
        int rc = stat((joinedPath + '\0').bytes(), &buf);
        if (rc != 0) {
            if (errno == ENOENT)
                continue;
            PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
            Filesystem::setLastResult(FS_UNKNOWN);
            break;
        }

        if (!entry.isDir) {
            entry.fileSize = buf.st_size;
        }
        entry.creationTime = buf.st_ctime;
        entry.accessTime = buf.st_atime;
        entry.modificationTime = buf.st_mtime;

        result.append(std::move(entry));
    }

    closedir(dir);
    return result;
}

FSResult Filesystem::makeDir(StringView path) {
    int rc = mkdir((path + '\0').bytes(), mode_t(0755));
    if (rc == 0) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        switch (errno) {
            case EEXIST:
            case EISDIR: {
                return Filesystem::setLastResult(FS_ALREADY_EXISTS);
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::setLastResult(FS_UNKNOWN);
            }
        }
    }
}

FSResult Filesystem::setWorkingDirectory(StringView path) {
    int rc = chdir((path + '\0').bytes());
    if (rc == 0) {
        return Filesystem::setLastResult(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                return Filesystem::setLastResult(FS_NOT_FOUND);
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::setLastResult(FS_UNKNOWN);
            }
        }
    }
}

String Filesystem::getWorkingDirectory() {
    u32 numUnitsWithNullTerm = PATH_MAX + 1;
    String path = String::allocate(numUnitsWithNullTerm);
    for (;;) {
        char* rs = getcwd(path.bytes(), numUnitsWithNullTerm);
        if (rs) {
            s32 len = path.find('\0');
            PLY_ASSERT(len >= 0);
            path.resize(len);
            Filesystem::setLastResult(FS_OK);
            return path;
        } else {
            switch (errno) {
                case ERANGE: {
                    numUnitsWithNullTerm *= 2;
                    path.resize(numUnitsWithNullTerm);
                    break;
                }
                default: {
                    PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                    Filesystem::setLastResult(FS_UNKNOWN);
                    return {};
                }
            }
        }
    }
}

ExistsResult Filesystem::exists(StringView path) {
    struct stat buf;
    int rc = stat((path + '\0').bytes(), &buf);
    if (rc == 0)
        return (buf.st_mode & S_IFMT) == S_IFDIR ? ER_DIRECTORY : ER_FILE;
    if (errno != ENOENT) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
    }
    return ER_NOT_FOUND;
}

int Filesystem::openFdForRead(StringView path) {
    int fd = open((path + '\0').bytes(), O_RDONLY | O_CLOEXEC);
    if (fd != -1) {
        Filesystem::setLastResult(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                Filesystem::setLastResult(FS_NOT_FOUND);
                break;

            case EACCES:
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                break;
        }
    }
    return fd;
}

Owned<Pipe> Filesystem::openPipeForRead(StringView path) {
    int fd = openFdForRead(path);
    if (fd == -1)
        return nullptr;
    return Heap::create<Pipe_FD>(fd, Pipe::HAS_READ_PERMISSION | Pipe::CAN_SEEK);
}

int Filesystem::openFdForWrite(StringView path) {
    int fd = open((path + '\0').bytes(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode_t(0644));
    if (fd != -1) {
        Filesystem::setLastResult(FS_OK);
    } else {
        switch (errno) {
            case ENOENT:
                Filesystem::setLastResult(FS_NOT_FOUND);
                break;

            case EACCES:
                Filesystem::setLastResult(FS_ACCESS_DENIED);
                break;

            default:
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                break;
        }
    }
    return fd;
}

Owned<Pipe> Filesystem::openPipeForWrite(StringView path) {
    int fd = openFdForWrite(path);
    if (fd == -1)
        return nullptr;
    return Heap::create<Pipe_FD>(fd, Pipe::HAS_WRITE_PERMISSION | Pipe::CAN_SEEK);
}

FSResult Filesystem::moveFile(StringView srcPath, StringView dstPath) {
    int rc = rename((srcPath + '\0').bytes(), (dstPath + '\0').bytes());
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::setLastResult(FS_UNKNOWN);
    }
    return Filesystem::setLastResult(FS_OK);
}

FSResult Filesystem::deleteFile(StringView path) {
    int rc = unlink((path + '\0').bytes());
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::setLastResult(FS_UNKNOWN);
    }
    return Filesystem::setLastResult(FS_OK);
}

FSResult Filesystem::removeDirTree(StringView dirPath) {
    for (const DirectoryEntry& entry : Filesystem::listDir(dirPath)) {
        String joined = joinPath(POSIXPath, dirPath, entry.name);
        if (entry.isDir) {
            FSResult fsResult = Filesystem::removeDirTree(joined);
            if (fsResult != FS_OK) {
                return fsResult;
            }
        } else {
            int rc = unlink((joined + '\0').bytes());
            if (rc != 0) {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                return Filesystem::setLastResult(FS_UNKNOWN);
            }
        }
    }
    int rc = rmdir((dirPath + '\0').bytes());
    if (rc != 0) {
        PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
        return Filesystem::setLastResult(FS_UNKNOWN);
    }
    return Filesystem::setLastResult(FS_OK);
}

DirectoryEntry Filesystem::getFileInfo(StringView path) {
    DirectoryEntry entry;
    struct stat buf;
    int rc = stat((path + '\0').bytes(), &buf);
    if (rc != 0) {
        switch (errno) {
            case ENOENT: {
                entry.result = Filesystem::setLastResult(FS_NOT_FOUND);
                break;
            }
            default: {
                PLY_ASSERT(PLY_FSPOSIX_ALLOW_UNKNOWN_ERRORS);
                Filesystem::setLastResult(FS_UNKNOWN);
                break;
            }
        }
    } else {
        entry.result = Filesystem::setLastResult(FS_OK);
        entry.fileSize = buf.st_size;
        entry.creationTime = buf.st_ctime;
        entry.accessTime = buf.st_atime;
        entry.modificationTime = buf.st_mtime;
    }
    return entry;
}

#endif

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if PLY_WITH_DIRECTORY_WATCHER
#if defined(PLY_WINDOWS)

void DirectoryWatcher::runWatcher() {
    // FIXME: prepend \\?\ to the path to get past MAX_PATH limitation
    HANDLE hDirectory = CreateFileW(win32_path_arg(this->root), FILE_LIST_DIRECTORY,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    PLY_ASSERT(hDirectory != INVALID_HANDLE_VALUE);
    HANDLE hChangeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    PLY_ASSERT(hChangeEvent != INVALID_HANDLE_VALUE);
    static const DWORD notifyInfoSize = 65536;
    FILE_NOTIFY_INFORMATION* notifyInfo = (FILE_NOTIFY_INFORMATION*) Heap::alloc(notifyInfoSize);
    for (;;) {
        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = hChangeEvent;
        BOOL rc =
            ReadDirectoryChangesW(hDirectory, notifyInfo, notifyInfoSize, TRUE,
                                  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                      FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                  NULL, &overlapped, NULL);
        // FIXME: Handle ERROR_NOTIFY_ENUM_DIR
        HANDLE events[2] = {this->endEvent, hChangeEvent};
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        PLY_ASSERT(waitResult >= WAIT_OBJECT_0 && waitResult <= WAIT_OBJECT_0 + 1);
        if (waitResult == WAIT_OBJECT_0)
            break;
        FILE_NOTIFY_INFORMATION* r = notifyInfo;
        for (;;) {
            // "The file name is in the Unicode character format and is not
            // null-terminated."
            // https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-_file_notify_information
            String path = fromWstring({r->FileName, u32(r->FileNameLength / sizeof(WCHAR))});
            bool isDirectory = false;
            DWORD attribs;
            {
                // FIXME: Avoid some of the UTF-8 <--> UTF-16 conversions done here
                String fullPath = joinPath(WindowsPath, this->root, path);
                attribs = GetFileAttributesW(win32_path_arg(fullPath));
            }
            if (attribs != INVALID_FILE_ATTRIBUTES) {
                isDirectory = (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }
            this->callback(path, isDirectory);
            if (r->NextEntryOffset == 0)
                break;
            r = (FILE_NOTIFY_INFORMATION*) PLY_PTR_OFFSET(r, r->NextEntryOffset);
        }
    }
    Heap::free(notifyInfo);
    CloseHandle(hChangeEvent);
    CloseHandle(hDirectory);
}

DirectoryWatcher::DirectoryWatcher() {
}

void DirectoryWatcher::start(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback) {
    PLY_ASSERT(this->root.isEmpty());
    PLY_ASSERT(!this->callback);
    PLY_ASSERT(this->endEvent == INVALID_HANDLE_VALUE);
    PLY_ASSERT(!this->watcherThread.isValid());
    this->root = root;
    this->callback = std::move(callback);
    this->endEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    this->watcherThread.run([this]() { runWatcher(); });
}

void DirectoryWatcher::stop() {
    if (this->watcherThread.isValid()) {
        SetEvent(this->endEvent);
        this->watcherThread.join();
        CloseHandle(this->endEvent);
        this->endEvent = INVALID_HANDLE_VALUE;
    }
}

#elif defined(PLY_MACOS)

void myCallback(ConstFSEventStreamRef streamRef, void* clientCallBackInfo, size_t numEvents, void* eventPaths,
                const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[]) {
    DirectoryWatcher* watcher = (DirectoryWatcher*) clientCallBackInfo;
    char** paths = (char**) eventPaths;
    for (size_t i = 0; i < numEvents; i++) {
        /* flags are unsigned long, IDs are uint64_t */
        StringView p = paths[i];
        FSEventStreamEventFlags flags = eventFlags[i];
        PLY_ASSERT(p.startsWith(watcher->root));
        p = p.substr(watcher->root.numBytes);

        // puts(String::format("change {} in {}, flags {}/0x{}", eventIds[i],
        // String::convert(p), flags, String::toHex(flags)).bytes());
        bool mustRecurse = false;
        if ((flags & kFSEventStreamEventFlagMustScanSubDirs) != 0) {
            mustRecurse = true;
        }
        if ((flags & kFSEventStreamEventFlagItemIsDir) != 0) {
            mustRecurse = true;
        }
        // FIXME: check kFSEventStreamEventFlagEventIdsWrapped
        watcher->callback(p, mustRecurse);
    }
}

void DirectoryWatcher::runWatcher() {
    this->runLoop = CFRunLoopGetCurrent();
    CFStringRef rootPath = CFStringCreateWithCString(NULL, this->root.bytes(), kCFStringEncodingASCII);
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void**) &rootPath, 1, NULL);
    FSEventStreamContext context;
    context.version = 0;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;
    // FIXME: should use kFSEventStreamCreateFlagWatchRoot to check if the folder being
    // watched gets moved?
    FSEventStreamRef stream =
        FSEventStreamCreate(NULL, myCallback, &context, pathsToWatch, kFSEventStreamEventIdSinceNow,
                            0.15, // latency
                            kFSEventStreamCreateFlagFileEvents);
    CFRelease(pathsToWatch);
    CFRelease(rootPath);
    // FIXME: Replace with FSEventStreamSetDispatchQueue as per compiler deprecation warnings.
    FSEventStreamScheduleWithRunLoop(stream, (CFRunLoopRef) this->runLoop, kCFRunLoopDefaultMode);
    Boolean rc = FSEventStreamStart(stream);
    PLY_ASSERT(rc == TRUE);
    PLY_UNUSED(rc);

    CFRunLoopRun();

    FSEventStreamStop(stream);
    // FIXME: Replace with FSEventStreamSetDispatchQueue as per compiler deprecation warnings.
    FSEventStreamUnscheduleFromRunLoop(stream, (CFRunLoopRef) this->runLoop, kCFRunLoopDefaultMode);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
}

DirectoryWatcher::DirectoryWatcher() {
}

void DirectoryWatcher::start(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback) {
    PLY_ASSERT(this->root.isEmpty());
    PLY_ASSERT(!this->callback);
    PLY_ASSERT(!this->watcherThread.isValid());
    this->root = root;
    this->callback = std::move(callback);
    this->watcherThread.run([this]() { runWatcher(); });
}

void DirectoryWatcher::stop() {
    if (this->watcherThread.isValid()) {
        CFRunLoopStop((CFRunLoopRef) this->runLoop);
        this->watcherThread.join();
    }
}

#endif
#endif // PLY_WITH_DIRECTORY_WATCHER

} // namespace ply
