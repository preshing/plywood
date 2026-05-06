/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "test-suite.h"
#include <ply-btree.h>
#include <ply-math.h>

//  ▄▄  ▄▄                               ▄▄
//  ███ ██ ▄▄  ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄
//  ██▀███ ██  ██ ██ ██ ██ ██▄▄██ ██  ▀▀ ██ ██
//  ██  ██ ▀█▄▄██ ██ ██ ██ ▀█▄▄▄  ██     ██ ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Numeric_

TEST_CASE("isRepresentable") {
    // Integer ranges
    check(isRepresentable<u32>(0));
    check(isRepresentable<u32>(123));
    check(!isRepresentable<u32>(-5));
    check(isRepresentable<s32>(-5));
    check(!isRepresentable<s8>(200));
    check(isRepresentable<u8>(200));
    check(!isRepresentable<s8>(u8(200)));
    check(isRepresentable<u16>(getMaxValue<s16>()));
    check(!isRepresentable<s16>(getMaxValue<u16>()));
    check(!isRepresentable<s32>(getMinValue<s64>()));
    check(!isRepresentable<s32>(getMaxValue<s64>()));
    check(!isRepresentable<s32>(getMaxValue<u64>()));
    check(!isRepresentable<u32>(getMinValue<s64>()));
    check(!isRepresentable<u32>(getMaxValue<s64>()));
    check(!isRepresentable<u32>(getMaxValue<u64>()));
    check(isRepresentable<s64>(getMinValue<s32>()));
    check(isRepresentable<s64>(getMaxValue<s32>()));
    check(isRepresentable<s64>(getMaxValue<u32>()));
    check(!isRepresentable<s64>(getMaxValue<u64>()));
    check(!isRepresentable<u64>(getMinValue<s32>()));
    check(isRepresentable<u64>(getMaxValue<s32>()));
    check(isRepresentable<u64>(getMaxValue<u32>()));
    check(isRepresentable<u64>(getMaxValue<s64>()));

    // float to int
    check(isRepresentable<u32>(123.0f));
    check(!isRepresentable<u32>(123.25f));
    check(isRepresentable<s32>(-2147483648.0f));

    // int to float
    check(isRepresentable<float>(16777216));
    check(!isRepresentable<float>(16777217));

    // float to float
    check(isRepresentable<double>(16777216.f));
    check(!isRepresentable<float>(16777217.0));
}

//  ▄▄▄▄▄▄ ▄▄                      ▄▄▄        ▄▄▄▄▄          ▄▄
//    ██   ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄      ██ ▀▀       ██  ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄
//    ██   ██ ██ ██ ██ ██▄▄██     ▄█▀█▄▀▀     ██  ██  ▄▄▄██  ██   ██▄▄██
//    ██   ██ ██ ██ ██ ▀█▄▄▄      ▀█▄▄▀█▄     ██▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄
//

TEST_CASE("String::fromDateTime") {
    DateTime dateTime;
    dateTime.year = 2025;
    dateTime.month = 12;
    dateTime.day = 1;
    dateTime.weekday = 1; // Monday
    dateTime.hour = 19;
    dateTime.minute = 0;
    dateTime.second = 1;
    dateTime.timeZoneOffsetInMinutes = -300;
    dateTime.microsecond = 234000;

    check(String::fromDateTime("%Y-%m-%d", dateTime) == "2025-12-01");
    check(String::fromDateTime("%H:%M:%S", dateTime) == "19:00:01");
    check(String::fromDateTime("%A, %B %e, %Y", dateTime) == "Monday, December 1, 2025");
    check(String::fromDateTime("%l:%M %p (UTC%Z)", dateTime) == "7:00 PM (UTC-05:00)");
    check(String::fromDateTime("[%Y:%m:%d %H:%M:%S.%L]", dateTime) == "[2025:12:01 19:00:01.234]");
}

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Thread_

TEST_CASE("Thread join") {
    int value = 0;
    Thread thread([&]() { value = 42; });
    thread.join();
    check(value == 42);
}

TEST_CASE("Atomic 8-bit operations") {
    Atomic<u8> value = 7;
    check(value.load(Relaxed) == 7);

    value.store(9, Release);
    check(value.load(Acquire) == 9);

    check(value.compareExchange(9, 12, AcqRel) == 9);
    check(value.load(Relaxed) == 12);
    check(value.compareExchange(9, 1, AcqRel) == 12);
    check(value.load(Relaxed) == 12);

    check(value.exchange(3, AcqRel) == 12);
    check(value.load(Relaxed) == 3);

    check(value.fetchAdd(4, AcqRel) == 3);
    check(value.load(Relaxed) == 7);
    check(value.fetchSub(2, AcqRel) == 7);
    check(value.load(Relaxed) == 5);

    check(value.fetchAnd(6, AcqRel) == 5);
    check(value.load(Relaxed) == 4);
    check(value.fetchOr(3, AcqRel) == 4);
    check(value.load(Relaxed) == 7);
}

TEST_CASE("Atomic 16-bit operations") {
    Atomic<u16> value = 0x1200;
    check(value.load(Relaxed) == 0x1200);

    value.store(0x1234, Release);
    check(value.load(Acquire) == 0x1234);

    check(value.compareExchange(0x1234, 0x4321, AcqRel) == 0x1234);
    check(value.load(Relaxed) == 0x4321);
    check(value.compareExchange(0x1234, 0xFFFF, AcqRel) == 0x4321);
    check(value.load(Relaxed) == 0x4321);

    check(value.exchange(0x0102, AcqRel) == 0x4321);
    check(value.load(Relaxed) == 0x0102);

    check(value.fetchAdd(0x0010, AcqRel) == 0x0102);
    check(value.load(Relaxed) == 0x0112);
    check(value.fetchSub(0x0003, AcqRel) == 0x0112);
    check(value.load(Relaxed) == 0x010F);

    check(value.fetchAnd(0x00FF, AcqRel) == 0x010F);
    check(value.load(Relaxed) == 0x000F);
    check(value.fetchOr(0x0F00, AcqRel) == 0x000F);
    check(value.load(Relaxed) == 0x0F0F);
}

TEST_CASE("Atomic 32-bit operations") {
    Atomic<u32> value = 0x12000000;
    check(value.load(Relaxed) == 0x12000000);

    value.store(0x12345678, Release);
    check(value.load(Acquire) == 0x12345678);

    check(value.compareExchange(0x12345678, 0x87654321, AcqRel) == 0x12345678);
    check(value.load(Relaxed) == 0x87654321);
    check(value.compareExchange(0x12345678, 0xFFFFFFFF, AcqRel) == 0x87654321);
    check(value.load(Relaxed) == 0x87654321);

    check(value.exchange(0x01020304, AcqRel) == 0x87654321);
    check(value.load(Relaxed) == 0x01020304);

    check(value.fetchAdd(0x00010010, AcqRel) == 0x01020304);
    check(value.load(Relaxed) == 0x01030314);
    check(value.fetchSub(0x00000024, AcqRel) == 0x01030314);
    check(value.load(Relaxed) == 0x010302F0);

    check(value.fetchAnd(0x00FF00FF, AcqRel) == 0x010302F0);
    check(value.load(Relaxed) == 0x000300F0);
    check(value.fetchOr(0x0F00000F, AcqRel) == 0x000300F0);
    check(value.load(Relaxed) == 0x0F0300FF);
}

TEST_CASE("Atomic 64-bit operations") {
    Atomic<u64> value = u64(0x1200000000000000ull);
    check(value.load(Relaxed) == u64(0x1200000000000000ull));

    value.store(u64(0x123456789ABCDEF0ull), Release);
    check(value.load(Acquire) == u64(0x123456789ABCDEF0ull));

    check(value.compareExchange(u64(0x123456789ABCDEF0ull), u64(0x0FEDCBA987654321ull), AcqRel) ==
          u64(0x123456789ABCDEF0ull));
    check(value.load(Relaxed) == u64(0x0FEDCBA987654321ull));
    check(value.compareExchange(u64(0x123456789ABCDEF0ull), u64(0xFFFFFFFFFFFFFFFFull), AcqRel) ==
          u64(0x0FEDCBA987654321ull));
    check(value.load(Relaxed) == u64(0x0FEDCBA987654321ull));

    check(value.exchange(u64(0x0102030405060708ull), AcqRel) == u64(0x0FEDCBA987654321ull));
    check(value.load(Relaxed) == u64(0x0102030405060708ull));

    check(value.fetchAdd(u64(0x0001001000010010ull), AcqRel) == u64(0x0102030405060708ull));
    check(value.load(Relaxed) == u64(0x0103031405070718ull));
    check(value.fetchSub(u64(0x0000000000000028ull), AcqRel) == u64(0x0103031405070718ull));
    check(value.load(Relaxed) == u64(0x01030314050706F0ull));

    check(value.fetchAnd(u64(0x00FF00FF00FF00FFull), AcqRel) == u64(0x01030314050706F0ull));
    check(value.load(Relaxed) == u64(0x00030014000700F0ull));
    check(value.fetchOr(u64(0x0F0000000000000Full), AcqRel) == u64(0x00030014000700F0ull));
    check(value.load(Relaxed) == u64(0x0F030014000700FFull));
}

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Heap_

TEST_CASE("Heap::getStats") {
    Heap::Stats beginStats = Heap::getStats();

    void* a = Heap::alloc(64);
    void* b = Heap::alloc(4096);
    check(a != nullptr);
    check(b != nullptr);

    Heap::Stats duringStats = Heap::getStats();
    check(duringStats.totalBytesConsumed >= beginStats.totalBytesConsumed + 64 + 4096);
    check(duringStats.totalSystemMemoryUsed >= duringStats.totalBytesConsumed);

    Heap::free(a);
    Heap::free(b);

    Heap::Stats endStats = Heap::getStats();
    check(endStats.totalBytesConsumed <= duringStats.totalBytesConsumed);
    check(endStats.totalSystemMemoryUsed >= endStats.totalBytesConsumed);
}

TEST_CASE("Heap::setOutOfMemoryHandler") {
    u32 outOfMemoryHandlerCallCount = 0;
    Heap::setOutOfMemoryHandler([&]() { outOfMemoryHandlerCallCount++; });

    void* ptr = Heap::alloc(getMaxValue<uptr>());
    check(ptr == nullptr);
    check(outOfMemoryHandlerCallCount == 1);

    Heap::setOutOfMemoryHandler({});
}

TEST_CASE("Heap::allocAligned") {
    const u32 alignments[] = {16, 32, 64, 128, 256};
    for (u32 alignment : alignments) {
        void* ptr = Heap::allocAligned(113, alignment);
        check(ptr != nullptr);
        check(isAlignedToPowerOf2((u64) (uptr) ptr, (u64) alignment));
        if (ptr) {
            memset(ptr, 0x5A, 113);
            Heap::free(ptr);
        }
    }
    Heap::validate();
}

TEST_CASE("Heap::realloc grow/shrink") {
    u32 initialBytes = 96;
    u8* ptr = (u8*) Heap::alloc(initialBytes);
    check(ptr != nullptr);
    if (!ptr) {
        return;
    }

    for (u32 i = 0; i < initialBytes; i++) {
        ptr[i] = (u8) (i ^ 0xA5);
    }

    u32 grownBytes = 2048;
    u8* grown = (u8*) Heap::realloc(ptr, grownBytes);
    check(grown != nullptr);
    if (!grown) {
        Heap::free(ptr);
        return;
    }
    for (u32 i = 0; i < initialBytes; i++) {
        check(grown[i] == (u8) (i ^ 0xA5));
    }

    u32 shrunkBytes = 64;
    u8* shrunk = (u8*) Heap::realloc(grown, shrunkBytes);
    check(shrunk != nullptr);
    if (!shrunk) {
        Heap::free(grown);
        return;
    }
    for (u32 i = 0; i < shrunkBytes; i++) {
        check(shrunk[i] == (u8) (i ^ 0xA5));
    }

    Heap::free(shrunk);
    Heap::validate();
}

TEST_CASE("Heap coalescing") {
    void* a = Heap::alloc(120);
    void* b = Heap::alloc(120);
    void* c = Heap::alloc(120);
    check(a != nullptr);
    check(b != nullptr);
    check(c != nullptr);
    if (!(a && b && c)) {
        Heap::free(a);
        Heap::free(b);
        Heap::free(c);
        return;
    }

    Heap::free(a);
    Heap::free(b);
    void* d = Heap::alloc(220);
    check(d != nullptr);
    check(d == a);

    Heap::free(d);
    Heap::free(c);
    Heap::validate();
}

TEST_CASE("Heap stress") {
    struct Block {
        void* ptr = nullptr;
        u32 numBytes = 0;
    };

    Random rand{0x12345678};
    Array<Block> blocks;
    for (u32 i = 0; i < 4000; i++) {
        u32 action = rand.generateU32() % 3;
        if (blocks.isEmpty() || action == 0) {
            u32 numBytes = (rand.generateU32() % 4096) + 1;
            if ((rand.generateU32() % 64) == 0) {
                numBytes += 256 * 1024;
            }
            void* ptr = Heap::alloc(numBytes);
            check(ptr != nullptr);
            if (ptr) {
                blocks.append({ptr, numBytes});
            }
        } else if (action == 1) {
            u32 index = rand.generateU32() % blocks.numItems();
            u32 newBytes = (rand.generateU32() % 4096) + 1;
            if ((rand.generateU32() % 64) == 0) {
                newBytes += 256 * 1024;
            }
            void* ptr = Heap::realloc(blocks[index].ptr, newBytes);
            check(ptr != nullptr);
            if (ptr) {
                blocks[index].ptr = ptr;
                blocks[index].numBytes = newBytes;
            }
        } else {
            u32 index = rand.generateU32() % blocks.numItems();
            Heap::free(blocks[index].ptr);
            blocks.eraseQuick(index);
        }

        if ((i % 128) == 0) {
            Heap::validate();
        }
    }

    for (const Block& block : blocks) {
        Heap::free(block.ptr);
    }
    Heap::validate();
}

TEST_CASE("Heap multithread smoke") {
    Atomic<u32> failures = 0;
    static constexpr u32 NumThreads = 4;
    static constexpr u32 Iterations = 1200;
    Thread threads[NumThreads];

    for (u32 t = 0; t < NumThreads; t++) {
        threads[t].run([&failures, t]() {
            Random rand{t + 1};
            Array<void*> ptrs;
            for (u32 i = 0; i < Iterations; i++) {
                if (ptrs.isEmpty() || (rand.generateU32() % 2) == 0) {
                    u32 numBytes = (rand.generateU32() % 2048) + 1;
                    void* ptr = Heap::alloc(numBytes);
                    if (ptr) {
                        ptrs.append(ptr);
                    } else {
                        failures.fetchAdd(1, AcqRel);
                    }
                } else {
                    u32 index = rand.generateU32() % ptrs.numItems();
                    Heap::free(ptrs[index]);
                    ptrs.eraseQuick(index);
                }
            }
            for (void* ptr : ptrs) {
                Heap::free(ptr);
            }
        });
    }
    for (u32 t = 0; t < NumThreads; t++) {
        threads[t].join();
    }

    check(failures.load(Acquire) == 0);
    Heap::validate();
}

TEST_CASE("Heap::validate") {
    Heap::validate();
    void* ptr = Heap::alloc(256);
    check(ptr != nullptr);
    Heap::validate();
    Heap::free(ptr);
    Heap::validate();
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Hashing_

TEST_CASE("shuffle_bits() 32") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u32 value = rand.generateU32();
        u32 shuffled = shuffleBits(value);
        u32 unshuffled = unshuffleBits(shuffled);
        check(value == unshuffled);
    }
}

TEST_CASE("shuffle_bits() 64") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u64 value = rand.generateU32();
        u64 shuffled = shuffleBits(value);
        u64 unshuffled = unshuffleBits(shuffled);
        check(value == unshuffled);
    }
}

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX String_

TEST_CASE("String self-assignment") {
    String str = "How now brown cow?";
    str = str.shortenedBy(1);
    check(str == "How now brown cow");
}

TEST_CASE("String find") {
    String str = "abcdefgh";
    check(str.find([](char x) { return x == 'c'; }) == 2);
    check(str.find([](char x) { return x == 'z'; }) < 0);
    check(str.find('c') == 2);
    check(str.find('z') < 0);
}

TEST_CASE("String reverseFind") {
    String str = "abcdefgh";
    check(str.reverseFind([](char x) { return x == 'c'; }) == 2);
    check(str.reverseFind([](char x) { return x == 'z'; }) < 0);
    check(str.reverseFind('c') == 2);
    check(str.reverseFind('z') < 0);
}

TEST_CASE("String split") {
    // Single char separator
    {
        String str = "apple,banana,cherry";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
        check(parts[2] == "cherry");
    }
    // Multi char separator
    {
        String str = "apple::banana::cherry";
        Array<StringView> parts = str.split("::");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
        check(parts[2] == "cherry");
    }
    // Multi char separator with partial match in content
    {
        String str = "apple::banana:cherry::date";
        Array<StringView> parts = str.split("::");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana:cherry");
        check(parts[2] == "date");
    }
    // Consecutive separators (empty parts are skipped)
    {
        String str = "apple,,banana";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 2);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
    }
    // No separator found
    {
        String str = "hello world";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 1);
        check(parts[0] == "hello world");
    }
    // Empty string
    {
        String str = "";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 1);
        check(parts[0] == "");
    }
    // Separator at ends
    {
        String str = ",apple,banana,";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 2);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
    }
}

TEST_CASE("String match identifier") {
    String str = "(hello)";
    StringView identifier;
    check(str.match("'(%i')$", &identifier));
    check(identifier == "hello");
}

TEST_CASE("String match integer") {
    StringView str = "count: 42";
    s32 value = 0;
    check(str.match("count: %d", &value));
    check(value == 42);
}

TEST_CASE("String match negative integer") {
    StringView str = "offset=-123";
    s32 value = 0;
    check(str.match("offset=%d", &value));
    check(value == -123);
}

TEST_CASE("String match float") {
    StringView str = "pi=3.14159";
    double value = 0;
    check(str.match("pi=%f", &value));
    check(value > 3.14 && value < 3.15);
}

TEST_CASE("String match quoted string") {
    StringView str = "name=\"hello world\"";
    String value;
    check(str.match("name=%q", &value));
    check(value == "hello world");
}

TEST_CASE("String match quoted string escapes") {
    StringView str = "name=\"hello \\\"world\\\"\"";
    String value;
    check(str.match("name=%q", &value));
    check(value == "hello \"world\"");
}

TEST_CASE("readQuotedString C strict hex and unicode escapes") {
    ViewStream in{"\"A\\x42\\u03c0\\U0001f600\""};
    String value = readQuotedString(in, QuotedStringType::C, true);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "AB\xcf\x80\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString JavaScript strict unicode escapes") {
    ViewStream in{"\"A\\x42\\u03c0\\u{1f600}\""};
    String value = readQuotedString(in, QuotedStringType::JavaScript, true);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "AB\xcf\x80\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString JSON strict surrogate pairs") {
    ViewStream in{"\"\\uD83D\\uDE00\""};
    String value = readQuotedString(in, QuotedStringType::JSON, true);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString Python strict triple quoted multiline") {
    ViewStream in{"'''hello\n\"there\"'''"};
    String value = readQuotedString(in, QuotedStringType::Python, true);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "hello\n\"there\"");
}

TEST_CASE("readQuotedString JSON strict rejects x escape") {
    ViewStream in{"\"\\x41\""};
    String value = readQuotedString(in, QuotedStringType::JSON, true);
    PLY_UNUSED(value);
    check(in.inputError);
}

TEST_CASE("readQuotedString JavaScript strict rejects big U escape") {
    ViewStream in{"\"\\U0001F600\""};
    String value = readQuotedString(in, QuotedStringType::JavaScript, true);
    PLY_UNUSED(value);
    check(in.inputError);
}

TEST_CASE("readQuotedString JSON strict rejects lone low surrogate") {
    ViewStream in{"\"\\uDE00\""};
    String value = readQuotedString(in, QuotedStringType::JSON, true);
    PLY_UNUSED(value);
    check(in.inputError);
}

TEST_CASE("readQuotedString JSON permissive keeps invalid single-character escapes") {
    ViewStream in{"\"\\#\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "#");
}

TEST_CASE("readQuotedString JSON permissive preserves malformed unicode escape text") {
    ViewStream in{"\"\\u12Z4\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "\\u12Z4");
}

TEST_CASE("readQuotedString C permissive preserves malformed big U escape text") {
    ViewStream in{"\"\\U0000G600\""};
    String value = readQuotedString(in, QuotedStringType::C, false);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "\\U0000G600");
}

TEST_CASE("readQuotedString JSON permissive preserves malformed surrogate text") {
    ViewStream in{"\"\\uD83Dx\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(!in.inputError);
    check(in.curByte == in.endByte);
    check(value == "\\uD83Dx");
}

TEST_CASE("String match whitespace") {
    StringView str = "hello   world";
    check(str.match("hello *world"));
}

TEST_CASE("String match whitespace with tab") {
    StringView str = "hello\tworld";
    check(str.match("hello world"));
}

TEST_CASE("String match optional whitespace") {
    StringView str1 = "hello world";
    StringView str2 = "helloworld";
    check(str1.match("hello ?world"));
    check(str2.match("hello ?world"));
}

TEST_CASE("String match escape character") {
    StringView str = "%test%";
    StringView id;
    check(str.match("'%%i'%", &id));
    check(id == "test");
}

TEST_CASE("String match group alternation") {
    StringView str1 = "color: red";
    StringView str2 = "color: blue";
    StringView str3 = "color: green";
    check(str1.match("color: (red|blue|green)"));
    check(str2.match("color: (red|blue|green)"));
    check(str3.match("color: (red|blue|green)"));
}

TEST_CASE("String match group alternation fail") {
    StringView str = "color: yellow";
    check(!str.match("color: (red|blue|green)"));
}

TEST_CASE("String match zero or more") {
    StringView str1 = "ab";
    StringView str2 = "aab";
    StringView str3 = "aaab";
    check(str1.match("a*b"));
    check(str2.match("a*b"));
    check(str3.match("a*b"));
}

TEST_CASE("String match zero or more empty") {
    StringView str = "b";
    check(str.match("a*b"));
}

TEST_CASE("String match optional character") {
    StringView str1 = "color";
    StringView str2 = "colour";
    check(str1.match("colou?r"));
    check(str2.match("colou?r"));
}

TEST_CASE("String match group zero or more") {
    StringView str1 = "start end";
    StringView str2 = "start foo end";
    StringView str3 = "start foo foo foo end";
    check(str1.match("start (foo )*end"));
    check(str2.match("start (foo )*end"));
    check(str3.match("start (foo )*end"));
}

TEST_CASE("String match nested groups") {
    check(StringView{""}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple,banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana, apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(!StringView{"orange"}.match("((apple|banana)(, *)?)*$"));
}

TEST_CASE("String match end anchor") {
    StringView str = "hello";
    check(str.match("hello$"));
    check(!str.match("hell$"));
}

TEST_CASE("String match multiple captures") {
    StringView str = "point(10, 20)";
    s32 x = 0, y = 0;
    check(str.match("point'(%d, %d')", &x, &y));
    check(x == 10);
    check(y == 20);
}

TEST_CASE("String match optional format specifier") {
    s32 value = -1;
    check(StringView{"item"}.match("item%d?$", &value));
    check(value == -1); // Unchanged since no number present
    check(StringView{"item42"}.match("item%d?$", &value));
    check(value == 42);
    check(!StringView{"item42extra"}.match("item%d?$", &value));
}

TEST_CASE("String match optional group with format specifier") {
    s32 num = -1;
    String text;
    // Test optional group with alternation containing format specifiers
    check(StringView{"start end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == -1); // Unchanged
    check(text.isEmpty());
    num = -1;
    check(StringView{"start num=42 end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == 42);
    num = -1;
    text = {};
    check(StringView{"start text=\"hello\" end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(text == "hello");
}

TEST_CASE("String match literal characters") {
    StringView str = "abc123xyz";
    check(str.match("abc123xyz"));
    check(!str.match("abc456xyz"));
}

//   ▄▄▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄
//  ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██
//  ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██
//                               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Array_

//--------------------------------
// Constructors
//--------------------------------
TEST_CASE("Array default constructor") {
    Array<u32> a;
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array construct from braced initializer list") {
    Array<u32> a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Copy construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array construct from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = std::move(a);
    check(a.isEmpty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

//--------------------------------
// Assignment Operators
//--------------------------------
TEST_CASE("Array assign from braced initializer list") {
    Array<u32> a;
    a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array assign, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array assign from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move assign Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a.isEmpty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array move assign from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array self-assignment") {
    Array<u32> a = {1, 1, 2, 3, 5, 8};
    a = a.subview(1);
    check(a == ArrayView<const u32>{1, 2, 3, 5, 8});
}

//--------------------------------
// Element Access
//--------------------------------
TEST_CASE("Array subscript lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a[0] == 4);
    check(a[1] == 5);
    check(a[2] == 6);
}

TEST_CASE("Array subscript modification") {
    Array<u32> a = {4, 5, 6};
    a[1] = 7;
    check(a == ArrayView<const u32>{4, 7, 6});
}

TEST_CASE("Array back lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a.back() == 6);
    check(a.back(-2) == 5);
}

TEST_CASE("Array back modification") {
    Array<u32> a = {4, 5, 6};
    a.back() = 7;
    check(a == ArrayView<const u32>{4, 5, 7});
}

TEST_CASE("Array iteration") {
    Array<u32> a = {4, 5, 6};
    u32 prev = 3;
    for (u32 i : a) {
        check(i == prev + 1);
        prev = i;
    }
}

TEST_CASE("Array iteration 2") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    for (u32 i : a) {
        b.append(i);
    }
    check(b == ArrayView<const u32>{4, 5, 6});
}

//--------------------------------
// Capacity
//--------------------------------
TEST_CASE("Array operator bool") {
    Array<u32> a;
    check(!(bool) a);
    a = {4, 5, 6};
    check((bool) a);
}

TEST_CASE("Array isEmpty") {
    Array<u32> a;
    check(a.isEmpty());
    a = {4, 5, 6};
    check(!a.isEmpty());
}

TEST_CASE("Array numItems") {
    Array<u32> a;
    check(a.numItems() == 0);
    a = {4, 5, 6};
    check(a.numItems() == 3);
}

//--------------------------------
// Modifers
//--------------------------------
TEST_CASE("Array clear") {
    Array<u32> a = {4, 5, 6};
    a.clear();
    check(a == ArrayView<const u32>{});
}

// FIXME: Add reserve() test?
// Ideally it would measure the number of allocations performed under the hood.

TEST_CASE("Array resize") {
    Array<u32> a;
    a.resize(3);
    check(a.numItems() == 3);
}

TEST_CASE("Array resize 2") {
    Array<String> a;
    a.resize(3);
    check(a == ArrayView<const StringView>{{}, {}, {}});
}

TEST_CASE("Array append, no move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(s0);
    a.append(s1);
    check(s0 == "hello");
    check(s1 == "there");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append with move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(std::move(s0));
    a.append(std::move(s1));
    check(s0 == "");
    check(s1 == "");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append, no move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(s);
    check(s == "hello");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array append String with move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(std::move(s));
    check(s == "");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array extend from braced initializer list") {
    Array<String> a;
    a += {"hello", "there"};
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray, no move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray with move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> without move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array pop") {
    Array<u32> a = {4, 5, 6};
    a.pop();
    check(a == ArrayView<const u32>{4, 5});
    a.pop(2);
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array insert") {
    Array<u32> a = {4, 5, 6};
    a.insert(2) = 7;
    check(a == ArrayView<const u32>{4, 5, 7, 6});
}

TEST_CASE("Array insert 2") {
    Array<String> a = {"hello", "there"};
    a.insert(1, 2);
    check(a == ArrayView<const StringView>{"hello", {}, {}, "there"});
}

TEST_CASE("Array erase") {
    Array<u32> a = {4, 5, 6};
    a.erase(0);
    check(a == ArrayView<const u32>{5, 6});

    Array<u32> b = {4, 5, 6, 7};
    b.erase(1, 2);
    check(b == ArrayView<const u32>{4, 7});
}

TEST_CASE("Array erase_quick") {
    Array<u32> a = {4, 5, 6};
    a.eraseQuick(0);
    check(a == ArrayView<const u32>{6, 5});

    Array<u32> b = {4, 5, 6, 7, 8, 9, 10};
    b.eraseQuick(1, 2);
    check(b == ArrayView<const u32>{4, 9, 10, 7, 8});
}

//   ▄▄▄▄          ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄
//   ▀▀▀█▄ ██▄▄██  ██
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Set_

struct TestHistogramBucket {
    u32 population = 0;
    u32 numTimesOccurred = 0;
};

TEST_CASE("Set stress test u32") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {{0, 0},  {1, 0},  {2, 0},  {4, 0},   {8, 0},
                                            {16, 0}, {32, 0}, {64, 0}, {128, 0}, {256, 0}};

    // Test setup.
    Set<u32> set;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 2500; iters++) {
        // Ensure the set and mirror array have the same number of items.
        PLY_ASSERT(set.items().numItems() == arr.numItems());

        // Decide what population size the set should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generateFloat();
        PLY_ASSERT(exp > 0);                      // Guaranteed because generateFloat returns numbers < 1.
        float randomPopulation = -logf(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desiredPopulation = (u32) clamp(randomPopulation - 4.f, 0.f, 512.f);

        // Add items to the set if needed.
        while (desiredPopulation > set.items().numItems()) {
            u32 valueToInsert = r.generateU32() % 1000;
            if (set.insert(valueToInsert).wasFound) {
                check(find(arr, valueToInsert) >= 0);
            } else {
                arr.append(valueToInsert);
            }
        }

        // Remove items from the set if needed.
        while (desiredPopulation < arr.numItems()) {
            u32 indexToRemove = r.generateU32() % arr.numItems();
            u32 valueToRemove = arr[indexToRemove];
            bool wasFound = set.erase(valueToRemove);
            check(wasFound);
            arr.eraseQuick(indexToRemove);
        }

        // Check its population.
        check(desiredPopulation == set.items().numItems());
        check(desiredPopulation == arr.numItems());
        for (s32 i = histogram.numItems() - 1; i >= 0; i--) {
            if (desiredPopulation >= histogram[i].population) {
                histogram[i].numTimesOccurred++;
                break;
            }
        }

        // Test find.
        sort(arr);
        for (u32 i = 0; i < arr.numItems(); i++) {
            check(set.find(arr[i]));
            if (i > 0) {
                check(arr[i] > arr[i - 1]); // No duplicates.
                u32 delta = arr[i] - arr[i - 1];
                if (delta > 1) {
                    u32 absentKey = arr[i - 1] + 1 + (r.generateU32() % (delta - 1));
                    check(!set.find(absentKey));
                }
            }
        }
    }
}

//  ▄▄   ▄▄
//  ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██  ██
//  ██   ██ ▀█▄▄██ ██▄▄█▀
//                 ██

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Map_

TEST_CASE("Map with String keys") {
    Map<String, u32> map;

    auto result1 = map.insert("apple");
    check(!result1.wasFound);
    *result1.value = 1;

    auto result2 = map.insert("banana");
    check(!result2.wasFound);
    *result2.value = 2;

    auto result3 = map.insert("cherry");
    check(!result3.wasFound);
    *result3.value = 3;

    // Find by string
    check(map.find("apple") != nullptr);
    check(*map.find("apple") == 1);
    check(map.find("banana") != nullptr);
    check(*map.find("banana") == 2);
    check(map.find("cherry") != nullptr);
    check(*map.find("cherry") == 3);

    // Find non-existing
    check(map.find("durian") == nullptr);
}

TEST_CASE("Map stress test") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {
        {0, 0}, {1, 0}, {2, 0}, {4, 0}, {8, 0}, {16, 0}, {32, 0}, {64, 0}, {128, 0}, {256, 0},
    };

    // Test setup.
    Map<u32, String> map;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 500; iters++) {
        // Ensure the map and mirror array have the same number of items.
        PLY_ASSERT(map.items().numItems() == arr.numItems());

        // Decide what population size the map should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generateFloat();
        PLY_ASSERT(exp > 0);                      // Guaranteed because generateFloat returns numbers < 1.
        float randomPopulation = -logf(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desiredPopulation = (u32) clamp(randomPopulation - 4.f, 0.f, 512.f);

        // Add items to the map if needed.
        while (desiredPopulation > map.items().numItems()) {
            u32 keyToInsert = r.generateU32() % 1000;
            auto result = map.insert(keyToInsert);
            if (result.wasFound) {
                check(find(arr, keyToInsert) >= 0);
            } else {
                *result.value = String::format("{}", keyToInsert);
                arr.append(keyToInsert);
            }
        }

        // Remove items from the map if needed.
        while (desiredPopulation < arr.numItems()) {
            u32 indexToRemove = r.generateU32() % arr.numItems();
            u32 keyToRemove = arr[indexToRemove];
            map.erase(keyToRemove);
            arr.eraseQuick(indexToRemove);
        }

        // Check its population.
        check(desiredPopulation == map.items().numItems());
        check(desiredPopulation == arr.numItems());
        for (s32 i = histogram.numItems() - 1; i >= 0; i--) {
            if (desiredPopulation >= histogram[i].population) {
                histogram[i].numTimesOccurred++;
                break;
            }
        }

        // Test find.
        sort(arr);
        for (u32 i = 0; i < arr.numItems(); i++) {
            String* found = map.find(arr[i]);
            check(found);
            check(*found == String::format("{}", arr[i]));
            if (i > 0) {
                check(arr[i] > arr[i - 1]); // No duplicates.
                u32 delta = arr[i] - arr[i - 1];
                if (delta > 1) {
                    u32 absentKey = arr[i - 1] + 1 + (r.generateU32() % (delta - 1));
                    check(!map.find(absentKey));
                }
            }
        }
    }
}

//  ▄▄▄▄▄  ▄▄▄▄▄▄
//  ██  ██   ██   ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀█▄   ██   ██  ▀▀ ██▄▄██ ██▄▄██
//  ██▄▄█▀   ██   ██     ▀█▄▄▄  ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX BTree_

TEST_CASE("BTree stress test u32") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {{0, 0},  {1, 0},  {2, 0},  {4, 0},   {8, 0},
                                            {16, 0}, {32, 0}, {64, 0}, {128, 0}, {256, 0}};

    // Test setup.
    BTree<u32> btree;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 2500; iters++) {
        // Ensure the B-tree and mirror array have the same number of items.
        PLY_ASSERT(btree.numItems == arr.numItems());

        // Decide what population size the B-tree should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generateFloat();
        PLY_ASSERT(exp > 0);                      // Guaranteed because generateFloat returns numbers < 1.
        float randomPopulation = -logf(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desiredPopulation = (u32) clamp(randomPopulation - 4.f, 0.f, 512.f);

        // Add items to the B-tree if needed.
        while (desiredPopulation > arr.numItems()) {
            u32 valueToInsert = r.generateU32() % 1000;
            arr.append(valueToInsert);
            btree.insert(valueToInsert);
#if defined(PLY_WITH_ASSERTS)
            btree.validate();
#endif
        }

        // Remove items from the B-tree if needed.
        while (desiredPopulation < arr.numItems()) {
            u32 indexToRemove = r.generateU32() % arr.numItems();
            u32 valueToRemove = arr[indexToRemove];
            bool wasFound = btree.erase(valueToRemove);
#if defined(PLY_WITH_ASSERTS)
            btree.validate();
#endif
            check(wasFound);
            arr.eraseQuick(indexToRemove);
        }

        // Check its population.
        check(desiredPopulation == arr.numItems());
        for (s32 i = histogram.numItems() - 1; i >= 0; i--) {
            if (desiredPopulation >= histogram[i].population) {
                histogram[i].numTimesOccurred++;
                break;
            }
        }

        // Test iteration.
        sort(arr);
        auto iter = btree.getFirstItem();
        for (u32 i = 0; i < arr.numItems(); i++) {
            check(iter);
            check(*iter == arr[i]);
            iter++;
        }
        check(!iter);

        // Test reverse iteration.
        iter = btree.getLastItem();
        for (s32 i = arr.numItems() - 1; i >= 0; i--) {
            check(iter);
            check(*iter == arr[i]);
            iter--;
        }
        check(!iter);

        // Test find.
        for (u32 i = 0; i < arr.numItems(); i++) {
            check(btree.find(arr[i]));
        }
    }
}

//  ▄▄   ▄▄               ▄▄                ▄▄
//  ██   ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//   ██ ██   ▄▄▄██ ██  ▀▀ ██  ▄▄▄██ ██  ██  ██
//    ▀█▀   ▀█▄▄██ ██     ██ ▀█▄▄██ ██  ██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Variant_

struct FruitBowl {
    struct Apple {
        String type;
    };
    struct Banana {
        bool peeled = false;
    };
    struct Cherry {
        u32 numOnStem = 1;
    };
    struct Date {};

    Variant<Apple, Banana, Cherry, Date> fruit;
};

TEST_CASE("Variant template") {
    FruitBowl bowl;
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());

    bowl.fruit = FruitBowl::Apple{"Cortland"};
    check(bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());
    check(bowl.fruit.as<FruitBowl::Apple>()->type == "Cortland");

    bowl.fruit = FruitBowl::Date{};
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(bowl.fruit.is<FruitBowl::Date>());

    FruitBowl bowl2{FruitBowl::Banana{}};
    check(bowl2.fruit.is<FruitBowl::Banana>());
    check(!bowl2.fruit.as<FruitBowl::Banana>()->peeled);

    FruitBowl bowl3 = {FruitBowl::Cherry{}};
    check(bowl3.fruit.is<FruitBowl::Cherry>());
    check(bowl3.fruit.as<FruitBowl::Cherry>()->numOnStem == 1);
}

//   ▄▄▄▄  ▄▄▄                       ▄▄  ▄▄   ▄▄
//  ██  ██  ██   ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄██▄▄ ██▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄
//  ██▀▀██  ██  ██  ██ ██  ██ ██  ▀▀ ██  ██   ██  ██ ██ ██ ██ ▀█▄▄▄
//  ██  ██ ▄██▄ ▀█▄▄██ ▀█▄▄█▀ ██     ██  ▀█▄▄ ██  ██ ██ ██ ██  ▄▄▄█▀
//               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Algorithm_

TEST_CASE("binarySearch() basic functionality") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 5, FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, 7, FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, 1, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 15, FindGreaterThanOrEqual) == 7);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 4, FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, 6, FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, 0, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 20, FindGreaterThanOrEqual) == 8);
}

TEST_CASE("binarySearch() with FindGreaterThan condition") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, 5, FindGreaterThan) == 3);
    check(binarySearch(arr, 7, FindGreaterThan) == 4);
    check(binarySearch(arr, 1, FindGreaterThan) == 1);
    check(binarySearch(arr, 15, FindGreaterThan) == 8);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, 4, FindGreaterThan) == 2);
    check(binarySearch(arr, 6, FindGreaterThan) == 3);
    check(binarySearch(arr, 0, FindGreaterThan) == 0);
    check(binarySearch(arr, 20, FindGreaterThan) == 8);
}

TEST_CASE("binarySearch() empty array") {
    Array<u32> emptyArr;

    // Empty array should always return 0 for any search
    check(binarySearch(emptyArr, 5, FindGreaterThanOrEqual) == 0);
    check(binarySearch(emptyArr, 5, FindGreaterThan) == 0);
    check(binarySearch(emptyArr, 0, FindGreaterThanOrEqual) == 0);
    check(binarySearch(emptyArr, 100, FindGreaterThan) == 0);
}

TEST_CASE("binarySearch() single element") {
    Array<u32> singleArr = {42};

    // Test with single element array
    check(binarySearch(singleArr, 42, FindGreaterThanOrEqual) == 0);
    check(binarySearch(singleArr, 42, FindGreaterThan) == 1);
    check(binarySearch(singleArr, 40, FindGreaterThanOrEqual) == 0);
    check(binarySearch(singleArr, 40, FindGreaterThan) == 0);
    check(binarySearch(singleArr, 50, FindGreaterThanOrEqual) == 1);
    check(binarySearch(singleArr, 50, FindGreaterThan) == 1);
}

TEST_CASE("binarySearch() with duplicates") {
    Array<u32> arr = {1, 3, 3, 3, 5, 7, 7, 9};

    // Test finding duplicates with Find_Greater_Than_Or_Equal (should find first occurrence)
    check(binarySearch(arr, 3, FindGreaterThanOrEqual) == 1);
    check(binarySearch(arr, 7, FindGreaterThanOrEqual) == 5);

    // Test finding duplicates with Find_Greater_Than (should find first element after duplicates)
    check(binarySearch(arr, 3, FindGreaterThan) == 4);
    check(binarySearch(arr, 7, FindGreaterThan) == 7);

    // Test finding elements between duplicates
    check(binarySearch(arr, 4, FindGreaterThanOrEqual) == 4);
    check(binarySearch(arr, 4, FindGreaterThan) == 4);
    check(binarySearch(arr, 6, FindGreaterThanOrEqual) == 5);
    check(binarySearch(arr, 6, FindGreaterThan) == 5);
}

TEST_CASE("binarySearch() all same elements") {
    Array<u32> arr = {5, 5, 5, 5, 5};

    // Test with all same elements
    check(binarySearch(arr, 5, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 5, FindGreaterThan) == 5);
    check(binarySearch(arr, 3, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 3, FindGreaterThan) == 0);
    check(binarySearch(arr, 7, FindGreaterThanOrEqual) == 5);
    check(binarySearch(arr, 7, FindGreaterThan) == 5);
}

TEST_CASE("binarySearch() with custom type") {
    struct TestItem {
        u32 value;
        String name;

        u32 getLookupKey() const {
            return value;
        }
    };

    Array<TestItem> arr = {{10, "ten"}, {20, "twenty"}, {30, "thirty"}, {40, "forty"}, {50, "fifty"}};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 30, FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, 40, FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, 10, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 50, FindGreaterThanOrEqual) == 4);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 25, FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, 35, FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, 5, FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, 60, FindGreaterThanOrEqual) == 5);

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, 30, FindGreaterThan) == 3);
    check(binarySearch(arr, 40, FindGreaterThan) == 4);
    check(binarySearch(arr, 10, FindGreaterThan) == 1);
    check(binarySearch(arr, 50, FindGreaterThan) == 5);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, 25, FindGreaterThan) == 2);
    check(binarySearch(arr, 35, FindGreaterThan) == 3);
    check(binarySearch(arr, 5, FindGreaterThan) == 0);
    check(binarySearch(arr, 60, FindGreaterThan) == 5);
}

TEST_CASE("binarySearch() with String type") {
    Array<String> arr = {"apple", "banana", "cherry", "date", "elderberry"};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, "cherry", FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, "date", FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, "apple", FindGreaterThanOrEqual) == 0);
    check(binarySearch(arr, "elderberry", FindGreaterThanOrEqual) == 4);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, "blueberry", FindGreaterThanOrEqual) == 2);
    check(binarySearch(arr, "coconut", FindGreaterThanOrEqual) == 3);
    check(binarySearch(arr, "apricot", FindGreaterThanOrEqual) == 1);
    check(binarySearch(arr, "fig", FindGreaterThanOrEqual) == 5);

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, "cherry", FindGreaterThan) == 3);
    check(binarySearch(arr, "date", FindGreaterThan) == 4);
    check(binarySearch(arr, "apple", FindGreaterThan) == 1);
    check(binarySearch(arr, "elderberry", FindGreaterThan) == 5);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, "blueberry", FindGreaterThan) == 2);
    check(binarySearch(arr, "coconut", FindGreaterThan) == 3);
    check(binarySearch(arr, "apricot", FindGreaterThan) == 1);
    check(binarySearch(arr, "fig", FindGreaterThan) == 5);
}

TEST_CASE("binarySearch() with different numeric types") {
    // Test with float array
    Array<float> floatArr = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    check(binarySearch(floatArr, 3.3f, FindGreaterThanOrEqual) == 2);
    check(binarySearch(floatArr, 3.0f, FindGreaterThanOrEqual) == 2);
    check(binarySearch(floatArr, 3.3f, FindGreaterThan) == 3);
    check(binarySearch(floatArr, 6.0f, FindGreaterThanOrEqual) == 5);

    // Test with double array
    Array<double> doubleArr = {1.1, 2.2, 3.3, 4.4, 5.5};
    check(binarySearch(doubleArr, 3.3, FindGreaterThanOrEqual) == 2);
    check(binarySearch(doubleArr, 3.0, FindGreaterThanOrEqual) == 2);
    check(binarySearch(doubleArr, 3.3, FindGreaterThan) == 3);
    check(binarySearch(doubleArr, 6.0, FindGreaterThanOrEqual) == 5);

    // Test with s32 array
    Array<s32> s32_arr = {-5, -3, -1, 1, 3, 5};
    check(binarySearch(s32_arr, -1, FindGreaterThanOrEqual) == 2);
    check(binarySearch(s32_arr, 0, FindGreaterThanOrEqual) == 3);
    check(binarySearch(s32_arr, -1, FindGreaterThan) == 3);
    check(binarySearch(s32_arr, 10, FindGreaterThanOrEqual) == 6);
}

//  ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Unicode_

TEST_CASE("Decode truncated UTF-8") {
    // e3 80 82 is the valid UTF-8 encoding of U+3002
    // e3 80 is the truncated version of it
    // As such, it should be decoded as two 8-bit characters
    OutPipeConvertUnicode conv{MemStream{}, UTF16_LE};
    conv.write("\xe3\x80");
    conv.flush(false);
    String result = static_cast<MemStream&>(conv.childOut).moveToString();
    check(result == StringView{"\xe3\x00\x80\x00", 4});
}

//  ▄▄▄▄▄▄                ▄▄   ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄ ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██   ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Text_Format_

struct ExtractedFormat {
    bool isValid = false;
    TextFormat format;
};

ExtractedFormat extractFormatFromName(StringView name) {
    TextFormat tf;

    Array<StringView> components = name.split(".");
    if (components.numItems() != 4)
        return {false, {}};

    if (components[1] == "utf8") {
        tf.unicodeType = UTF8;
    } else if (components[1] == "utf16le") {
        tf.unicodeType = UTF16_LE;
    } else if (components[1] == "utf16be") {
        tf.unicodeType = UTF16_BE;
    } else if (components[1] == "win1252") {
        tf.unicodeType = NOT_UNICODE;
    } else {
        return {false, {}};
    }

    if (components[2] == "lf") {
        tf.newLine = TextFormat::LF;
    } else if (components[2] == "crlf") {
        tf.newLine = TextFormat::CRLF;
    } else {
        return {false, {}};
    }

    if (components[3] == "bom") {
        tf.bom = true;
    } else if (components[3] == "nobom") {
        tf.bom = false;
    } else {
        return {false, {}};
    }

    return {true, tf};
}

TEST_CASE("Autodetect file encodings") {
    String testsFolder = joinPath(BASE_LIBRARY_TESTS_PATH, "text-files");
    u32 entryCount = 0;
    for (const DirectoryEntry& entry : Filesystem::listDir(testsFolder)) {
        if (!entry.isDir && entry.name.endsWith(".txt")) {
            ExtractedFormat expectedFormat = extractFormatFromName(entry.name.shortenedBy(4));
            check(expectedFormat.isValid);

            TextFormat detectedFormat;
            String contents = Filesystem::loadTextAutodetect(joinPath(testsFolder, entry.name), &detectedFormat);
            check(detectedFormat.unicodeType == expectedFormat.format.unicodeType);
            check(detectedFormat.newLine == expectedFormat.format.newLine);
            check(detectedFormat.bom == expectedFormat.format.bom);

            auto compareTo =
                Filesystem::loadBinary(joinPath(testsFolder, entry.name.split(".")[0] + ".utf8.lf.nobom.txt"));
            check(contents == compareTo);
            entryCount++;
        }
    }
    check(entryCount == 50);
}

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

TEST_CASE("Mem stream temp buffer") {
    Random random{0};
    for (u32 i = 0; i < 100; i++) {
        MemStream mem;
        u32 fileSize = Stream::BUFFER_SIZE * 10;
        u32 offset = 0;
        while (offset < fileSize) {
            check(offset == mem.getSeekPos());
            u32 numConsecutiveBytes =
                (random.generateU32() % (Stream::MAX_CONSECUTIVE_BYTES / 2)) + (Stream::MAX_CONSECUTIVE_BYTES / 2);
            check(mem.makeWritable(min(numConsecutiveBytes, fileSize - offset)));
            while (mem.curByte < mem.endByte) {
                *mem.curByte++ = (u8) shuffleBits(offset++);
                if (--numConsecutiveBytes == 0)
                    break;
            }
        }
        mem.seekTo(0);
        offset = 0;
        while (offset < fileSize) {
            check(offset == mem.getSeekPos());
            u32 numConsecutiveBytes =
                (random.generateU32() % (Stream::MAX_CONSECUTIVE_BYTES / 2)) + (Stream::MAX_CONSECUTIVE_BYTES / 2);
            mem.makeReadable(numConsecutiveBytes);
            check(mem.atEof == !mem.hasRemainingBytes());
            if (mem.atEof)
                break;
            while (mem.curByte < mem.endByte) {
                check((u8) *mem.curByte++ == (u8) shuffleBits(offset++));
                if (--numConsecutiveBytes == 0)
                    break;
            }
        }
        check(offset == fileSize);
    }
}

//  ▄▄   ▄▄ ▄▄         ▄▄                 ▄▄▄  ▄▄   ▄▄
//  ██   ██ ▄▄ ▄▄▄▄▄  ▄██▄▄ ▄▄  ▄▄  ▄▄▄▄   ██  ███▄███  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄
//   ██ ██  ██ ██  ▀▀  ██   ██  ██  ▄▄▄██  ██  ██▀█▀██ ██▄▄██ ██ ██ ██ ██  ██ ██  ▀▀ ██  ██
//    ▀█▀   ██ ██      ▀█▄▄ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██   ██ ▀█▄▄▄  ██ ██ ██ ▀█▄▄█▀ ██     ▀█▄▄██
//                                                                                    ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX VirtualMemory_

TEST_CASE("Usage stats with reserve/commit/decommit/unreserve") {
    VirtualMemory::Properties props = VirtualMemory::getProperties();
    uptr regionSize = max(props.pageSize * 4, props.regionAlignment);

    uptr initialReserved = VirtualMemory::totalReservedBytes.load(Relaxed);
    uptr initialCommitted = VirtualMemory::totalCommittedBytes.load(Relaxed);

    // Reserve region
    void* addr = VirtualMemory::reserveRegion(regionSize);
    check(addr != nullptr);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted);

    // Commit 3 pages
    VirtualMemory::commitPages(addr, props.pageSize * 3);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted + props.pageSize * 3);

    // Decommit 1 page
    VirtualMemory::decommitPages(addr, props.pageSize);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted + props.pageSize * 2);

    // Unreserve region (with 2 pages still committed)
    VirtualMemory::unreserveRegion(addr, regionSize, props.pageSize * 2);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted);
}

TEST_CASE("Usage stats with alloc/free") {
    VirtualMemory::Properties props = VirtualMemory::getProperties();
    uptr regionSize = props.regionAlignment * 2;

    uptr initialReserved = VirtualMemory::totalReservedBytes.load(Relaxed);
    uptr initialCommitted = VirtualMemory::totalCommittedBytes.load(Relaxed);

    // Alloc region (reserves and commits)
    void* addr = VirtualMemory::allocRegion(regionSize);
    check(addr != nullptr);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted + regionSize);

    // Free region (decommits and unreserves)
    VirtualMemory::freeRegion(addr, regionSize);
    check(VirtualMemory::totalReservedBytes.load(Relaxed) == initialReserved);
    check(VirtualMemory::totalCommittedBytes.load(Relaxed) == initialCommitted);
}

//  ▄▄▄▄▄          ▄▄   ▄▄
//  ██  ██  ▄▄▄▄  ▄██▄▄ ██▄▄▄
//  ██▀▀▀   ▄▄▄██  ██   ██  ██
//  ██     ▀█▄▄██  ▀█▄▄ ██  ██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Path_

TEST_CASE("matchGlobPattern()") {
    check(matchGlobPattern("file.cpp", "*.cpp"));
    check(matchGlobPattern("file1.cpp", "file?.cpp"));
    check(!matchGlobPattern("file12.cpp", "file?.cpp"));
    check(matchGlobPattern("ab", "a[b-d]"));
    check(matchGlobPattern("ad", "a[!b-c]"));
    check(!matchGlobPattern("ab", "a[!b-c]"));
    check(matchGlobPattern("a7", "a[[:digit:]]"));
    check(matchGlobPattern("a[b]", "a\\[b\\]"));
    check(matchGlobPattern("ab", "a\\b"));
}

TEST_CASE("matchGitIgnorePattern() with gitignore-style patterns") {
    // Basename-only patterns match at any depth and ignored directories ignore their descendants.
    check(matchGitIgnorePattern("foo", false, "foo"));
    check(matchGitIgnorePattern("a/foo", false, "foo"));
    check(matchGitIgnorePattern("a/foo/file.txt", false, "foo"));
    check(matchGitIgnorePattern("a\\foo\\file.txt", false, "foo"));
    check(!matchGitIgnorePattern("a/foo.txt", false, "foo"));

    // Leading and middle slashes anchor patterns to the supplied path root.
    check(matchGitIgnorePattern("foo", false, "/foo"));
    check(!matchGitIgnorePattern("a/foo", false, "/foo"));
    check(matchGitIgnorePattern("doc/frotz", false, "doc/frotz"));
    check(matchGitIgnorePattern("doc/frotz/file.txt", false, "doc/frotz"));
    check(!matchGitIgnorePattern("c/doc/frotz", false, "doc/frotz"));

    // A trailing slash restricts the match to directories, including matched ancestors.
    check(matchGitIgnorePattern("foo", true, "foo/"));
    check(!matchGitIgnorePattern("foo", false, "foo/"));
    check(matchGitIgnorePattern("a/foo/file.txt", false, "foo/"));
    check(!matchGitIgnorePattern("a/foo.txt", false, "foo/"));

    // Single-component wildcards do not cross path separators.
    check(matchGitIgnorePattern("logs/file.log", false, "*.log"));
    check(!matchGitIgnorePattern("logs/file.txt", false, "*.log"));
    check(matchGitIgnorePattern("Documentation/git.html", false, "Documentation/*.html"));
    check(matchGitIgnorePattern("Documentation\\git.html", false, "Documentation/*.html"));
    check(!matchGitIgnorePattern("Documentation/ppc/ppc.html", false, "Documentation/*.html"));

    // Double-star forms match zero or more directories.
    check(matchGitIgnorePattern("foo", false, "**/foo"));
    check(matchGitIgnorePattern("a/b/foo", false, "**/foo"));
    check(matchGitIgnorePattern("a/foo", false, "a/**/foo"));
    check(matchGitIgnorePattern("a/b/c/foo", false, "a/**/foo"));
    check(!matchGitIgnorePattern("b/a/foo", false, "a/**/foo"));
    check(matchGitIgnorePattern("build", true, "build/**"));
    check(!matchGitIgnorePattern("build", false, "build/**"));
    check(matchGitIgnorePattern("build/sub/file.txt", false, "build/**"));

    // Bracket expressions and escapes use Git's wildcard syntax.
    check(matchGitIgnorePattern("ab", false, "a[b]"));
    check(!matchGitIgnorePattern("a[b]", false, "a[b]"));
    check(matchGitIgnorePattern("a[b]", false, "a\\[b\\]"));
    check(matchGitIgnorePattern("ax", false, "a[!b]"));
    check(!matchGitIgnorePattern("ab", false, "a[!b]"));
    check(matchGitIgnorePattern("a7", false, "a[[:digit:]]"));
    check(!matchGitIgnorePattern("aa", false, "a[[:digit:]]"));

    // `#` and `!` are literal.
    check(matchGitIgnorePattern("#literal", false, "#literal"));
    check(matchGitIgnorePattern("!literal", false, "!literal"));
    check(!matchGitIgnorePattern("foo", false, "foo "));
    check(matchGitIgnorePattern("foo ", false, "foo "));
    check(matchGitIgnorePattern("foo ", false, "foo\\ "));
}

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if 0 // Disabled test
#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Directory_Watcher_

template <typename Item>
class MessageQueue {
    Mutex mutex;
    ConditionVariable cv;
    Array<Item> items;

public:
    void push(Item&& item) {
        LockGuard<Mutex> lock{mutex};
        items.append(std::move(item));
        cv.wakeOne();
    }
    bool pop(Item& item, u32 timeoutMs = 2000) {
        u64 timeLimit = getCpuTicks() + (u64) (timeoutMs * getCpuTicksPerSecond() / 1000.f);
        LockGuard<Mutex> lock{mutex};
        while (items.isEmpty()) {
            u64 now = getCpuTicks();
            s32 remainingMs = (s32) ((timeLimit - now) * (1000.f / getCpuTicksPerSecond()));
            if (remainingMs <= 0)
                return false;
            cv.timedWait(lock, remainingMs);
        }
        item = std::move(items[0]);
        items.erase(0);
        return true;
    }
};

TEST_CASE("DirectoryWatcher") {
    struct Event {
        String path;
        bool mustRecurse;
    };
    MessageQueue<Event> messageQueue;

    auto waitForEvent = [&](const Event& expectedEvent) -> bool {
        for (;;) {
            Event event;
            if (!messageQueue.pop(event))
                return false;
            if (event.path == expectedEvent.path && event.mustRecurse == expectedEvent.mustRecurse)
                return true;
        }
    };

    // Set up temp directory.
    String tempDir = joinPath(BUILD_DIR, "temp-dir-watcher");
    Filesystem::removeDirTree(tempDir); // Clean up from any previous run
    Filesystem::makeDir(tempDir);

    // Start the watcher.
    DirectoryWatcher watcher;
    watcher.start(tempDir, [&](StringView path, bool mustRecurse) {
        messageQueue.push({path, mustRecurse});
    });

    // Create a file in the temp directory.
    Filesystem::saveText(joinPath(tempDir, "first_file.txt"), "Hello, world!\n");
    check(waitForEvent({"first_file.txt", false}));

    // Create a subdirectory.
    Filesystem::makeDir(joinPath(tempDir, "subdir"));
    check(waitForEvent({"subdir", true}));

    // Modify the first file.
    Filesystem::saveText(joinPath(tempDir, "first_file.txt"), "Modified content!\n");
    check(waitForEvent({"first_file.txt", false}));

    // Create a file in the subdirectory.
    Filesystem::saveText(joinPath(tempDir, "subdir", "second_file.txt"), "Another file\n");
    check(waitForEvent({joinPath("subdir", "second_file.txt"), false}));

    // Delete the first file.
    Filesystem::deleteFile(joinPath(tempDir, "first_file.txt"));
    check(waitForEvent({"first_file.txt", false}));

    watcher.stop();
    Filesystem::removeDirTree(tempDir);
}
#endif
