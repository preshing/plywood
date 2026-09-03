/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-system.h>
#include "test-suite.h"
using namespace ply;

// Using an unnamed namespace to avoid redefinition errors on macOS
namespace {

// Configuration
static constexpr bool AllowLargeAllocs = true;
static constexpr u32 LargeBlockMin = 100000;
static constexpr u32 LargeBlockMax = 400000;

// Internal state
struct Allocation {
    void* addr = nullptr;
    u32 numBytes = 0;
};

Random random{0};
Array<Allocation> allBlocks;
u32 logCounter = 0;
u32 totalAllocatedBytes = 0;

// Chooses a small allocation most of the time and an occasional large allocation when allowed.
u32 getRandomAllocationSize(bool allowLarge) {
    if (allowLarge && ((random.generateU32() % 10) == 0)) {
        return (random.generateU32() % (LargeBlockMax - LargeBlockMin)) + LargeBlockMin;
    }
    if ((random.generateU32() % 4) == 0) {
        return (random.generateU32() % 15000) + 5000;
    }
    return (random.generateU32() % 500) + 10;
}

// Emits the current fragmentation statistics when verbose output is enabled.
void logStatus() {
    if (!options.verbose)
        return;
    uptr totalSystemMemoryUsed = Heap::getStats().totalSystemMemoryUsed;
    getStdOut().format("{}, {}, {}\n", logCounter, totalAllocatedBytes, totalSystemMemoryUsed);
    logCounter++;
}

// Allocates and tracks one randomly sized block.
void addRandomBlock(bool allowLarge) {
    u32 numBytes = getRandomAllocationSize(allowLarge);
    allBlocks.append({Heap::alloc(numBytes), numBytes});
    totalAllocatedBytes += numBytes;
    logStatus();
}

// Frees one randomly selected tracked block.
void freeRandomBlock() {
    if (allBlocks.numItems() > 0) {
        u32 i = random.generateU32() % allBlocks.numItems();
        Heap::free(allBlocks[i].addr);
        totalAllocatedBytes -= allBlocks[i].numBytes;
        allBlocks.eraseQuick(i);
        logStatus();
    }
}

} // unnamed namespace

// Runs the heap fragmentation stress test.
TestResult runFragmentationTest() {
    Array<u32> targetKB = {400, 100, 2000, 400, 5000, 2000, 10000, 8000, 10000, 400, 2000, 0};

    for (u32 i = 0; i < targetKB.numItems(); i++) {
        u32 targetAllocatedBytes = targetKB[i] * 1000;
        if (totalAllocatedBytes < targetAllocatedBytes) {
            // Grow irregularly
            while (totalAllocatedBytes < targetAllocatedBytes) {
                addRandomBlock(AllowLargeAllocs);
                freeRandomBlock();
                addRandomBlock(AllowLargeAllocs);
            }
        } else {
            // Shrink irregularly
            while (totalAllocatedBytes > targetAllocatedBytes) {
                freeRandomBlock();
                addRandomBlock(false);
                freeRandomBlock();
            }
        }
    }

    TestResult result;
    result.add(true);
    return result;
}
