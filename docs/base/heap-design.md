{title text="Heap Design" include="ply-base.h" namespace="ply"}

This document describes the internal design of the `HeapImpl` class, which implements a bespoke general-purpose memory allocator for Plywood.

## Internal Heap State

The global heap state is defined in `HeapImpl::HeapState`:

```cpp
struct HeapState {
    Mutex mutex;                              // Thread safety

    // Linked list of virtual memory segments
    VMSegment* vmSegments = nullptr;

    // Table of small bins (32 bins for sizes < 256 bytes)
    FreeNode* smallBins[SmallBinCount] = {};

    // Table of tree bins (32 bins for sizes >= 256 bytes)
    TreeNode* treeBins[TreeBinCount] = {};

    // Linked list of direct-mapped chunks (large allocations)
    Chunk* directMappedChunks = nullptr;

    // Designated victim chunk - hint for next allocation
    Chunk* designatedVictim = nullptr;

    // Top chunk per segment
    struct SegmentTop { VMSegment* segment; Chunk* topChunk; };
    Array<SegmentTop> segmentTops;

    // Internal counters
    uptr totalBytesConsumed = 0;
    uptr totalSystemMemoryUsed = 0;
};
```

### Virtual Memory Segments

Each `VMSegment` represents a region of virtual memory allocated from the OS:

```cpp
struct VMSegment {
    VMSegment* next;
    VMSegment* prev;
    void* baseAddr;    // Base address (after header)
    uptr numBytes;     // Usable space size
};
```

Segments are linked in a doubly-linked list. The first `sizeof(VMSegment)` bytes of each region store the segment header, and the remainder is divided into chunks.

### Small Bins

Small bins are a table of 32 doubly-linked lists, each handling a specific size class:
- Bin `i` handles sizes in range `[(i+1)*8, (i+2)*8)` bytes
- Sizes 8-255 bytes are handled by small bins
- Free chunks are added to the front of the appropriate bin (LIFO order)

### Tree Bins

Tree bins are a table of 32 balanced binary search trees for larger allocations:
- Bin `i` handles sizes in range `[256*2^i, 256*2^(i+1))` bytes
- Each tree is organized by the highest set bit of the chunk size
- Best-fit search is performed when allocating from tree bins

### Direct-Mapped Chunks

Large allocations (>= 1MB) bypass the bin system and are allocated directly from `VirtualMemory`. These chunks are tracked in a simple linked list.

### Designated Victim

The designated victim is a hint for the next allocation - typically the largest free chunk. This helps reduce fragmentation by reusing large free blocks.

## Virtual Memory Segments

Each virtual memory segment is divided into chunks with the following header format:

```cpp
struct Chunk {
    Chunk* prevChunk;     // Pointer to previous chunk in memory
    u64 sizeAndFlags;     // Size in bits 2-63, flags in bits 0-1
};
```

**Flags:**
- Bit 0: `inUse` (1 = allocated, 0 = free)
- Bit 1: `prevInUse` (1 = previous chunk is allocated)

### Chunk Layout

```
+------------------+------------------+------------------+
|  Chunk N         |  Chunk N+1       |  Chunk N+2       |
+------------------+------------------+------------------+
| prevChunk=N-1    | prevChunk=N      | prevChunk=N+1    |
| size=256, flags  | size=512, flags  | size=128, flags  |
| [user data]      | [user data]      | [user data]      |
+------------------+------------------+------------------+
```

### Coalescing

When a chunk is freed, it is immediately coalesced with adjacent free chunks:
1. Check if the next chunk is free (`!next->getInUse() && next->getPrevInUse()`)
2. If so, merge by adding the next chunk's size to the current chunk
3. Check if the previous chunk is free
4. If so, merge the previous chunk with the current chunk

This immediate coalescing reduces fragmentation.

## How Allocation Works

### Allocation Algorithm (Pseudocode)

```
allocate(numBytes):
    adjustedSize = align(numBytes, alignment)
    
    if adjustedSize >= LargeAllocThreshold:
        return createLargeChunk(adjustedSize)
    
    lock(mutex)
    
    // Try small bins
    if adjustedSize < SmallBinMax:
        chunk = findInSmallBins(adjustedSize)
        if chunk:
            chunk->setInUse(true)
            return chunk
    
    // Try tree bins (best-fit)
    chunk = findBestFitInTreeBins(adjustedSize)
    if chunk:
        chunk->setInUse(true)
        return chunk
    
    // Try designated victim
    if designatedVictim->getSize() >= adjustedSize:
        chunk = designatedVictim
        chunk->setInUse(true)
        designatedVictim = nullptr
        return chunk
    
    // Try segment tops
    chunk = findFreeChunkInSegments(adjustedSize)
    if chunk:
        chunk->setInUse(true)
        return chunk
    
    // Create new segment
    segment = createVMSegment(min(4KB, adjustedSize * 4))
    return initFirstChunk(segment)
```

### Small Bin Codepath

1. Calculate the size class: `index = size >> 3`
2. Search bins from the minimum required size class upward
3. Remove the first chunk from the matching bin
4. Mark the chunk as in-use
5. Return the chunk's user memory

**Data structure changes:** The chunk is removed from the bin's linked list.

### Tree Bin Codepath

1. Calculate the tree bin index based on size
2. Perform BFS to find the best-fit chunk in all tree bins
3. Remove the chunk from its tree
4. Mark the chunk as in-use
5. Return the chunk's user memory

**Data structure changes:** The chunk is removed from the tree, and tree pointers are updated.

### Direct-Mapped Codepath

1. Calculate the required size aligned to page boundary
2. Call `VirtualMemory::allocRegion()` to get a new region
3. Create a chunk header at the start of the region
4. Add the chunk to the direct-mapped list
5. Update counters
6. Return the chunk's user memory

**Data structure changes:** A new segment is added to `vmSegments`, and the chunk is added to `directMappedChunks`.

## Debugging

The `Heap::validate()` function checks heap invariants when `PLY_WITH_ASSERTS` is enabled:

- Verifies small bin integrity (chunks are free and have correct sizes)
- Verifies tree bin integrity (no cycles, correct parent pointers)
- Verifies direct-mapped chunks are in-use and large enough
- Verifies designated victim is not in bins
- Verifies segment chain integrity
- Recomputes total system memory and compares with counters

## Compile-Time Switch

The new allocator can be disabled by defining `PLY_USE_NEW_ALLOCATOR=0` at compile time, which reverts to the original dlmalloc-backed implementation.
