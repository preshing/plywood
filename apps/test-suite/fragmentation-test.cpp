/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-base.h>
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

// Helper functions
u32 getRandomAllocationSize(bool allowLarge) {
    if (allowLarge && ((random.generateU32() % 10) == 0)) {
        return (random.generateU32() % (LargeBlockMax - LargeBlockMin)) + LargeBlockMin;
    }
    if ((random.generateU32() % 4) == 0) {
        return (random.generateU32() % 15000) + 5000;
    }
    return (random.generateU32() % 500) + 10;
}

void logStatus() {
    uptr totalSystemMemoryUsed = Heap::getStats().totalSystemMemoryUsed;
    getStdOut().format("{}, {}, {}\n", logCounter, totalAllocatedBytes, totalSystemMemoryUsed);
    logCounter++;
}

void addRandomBlock(bool allowLarge) {
    u32 numBytes = getRandomAllocationSize(allowLarge);
    allBlocks.append({Heap::alloc(numBytes), numBytes});
    totalAllocatedBytes += numBytes;
    logStatus();
}

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
bool runFragmentationTest() {
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

    return true;
}
