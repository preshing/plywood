Memory Management (`ply-base.h`)
================================

At the lowest level, there's **virtual memory**, which can be mapped to physical memory using the underlying operating system.

The **heap** sits directly above that, dividing memory into variable-sized blocks that can be dynamically allocated and freed by the application as needed. Higher-level containers for managing memory, like `String`, `Array`, `Map`, `Owned` and `Reference`, are described in later sections.

## `Heap`

Plywood contains its own heap, which lets you allocate and free variable-sized blocks of memory.

The Plywood heap is separate from the C Standard Library's heap. Both heaps can coexist in the same program, but
memory allocated from a specific heap must always be freed using the same heap.

Heap backend selection is controlled by `PLY_USE_NEW_ALLOCATOR`:

- `PLY_USE_NEW_ALLOCATOR=1` (default): Uses Plywood's bespoke allocator in `ply::HeapImpl`.
- `PLY_USE_NEW_ALLOCATOR=0`: Uses the legacy [dlmalloc](https://gee.cs.oswego.edu/dl/html/malloc.html) backend.

For internal implementation details of the bespoke allocator, see [Heap Design](/docs/base/heap-design).

`Heap` is thread-safe. All member functions can be called concurrently from separate threads.

### Low-level Allocation

{context class=Heap}

`static void* alloc(uptr numBytes)`
> Allocates a block of memory from the Plywood heap. Equivalent to `malloc`. Always returns 16-byte aligned memory, suitable for SIMD vectors. Returns `nullptr` if allocation fails.

`static void* realloc(void* ptr, uptr numBytes)`
> Resizes a previously allocated block. The contents are preserved up to the smaller of the old and new sizes. May return a different pointer.

`static void* free(void* ptr)`
> Frees a previously allocated block, returning the memory to the heap.

`static void* allocAligned(uptr numBytes, u32 alignment)`
> Allocates memory with a specific alignment. Use for alignments greater than 16 bytes.

### Creating and Destroying Objects

By default, Plywood will override C++ `new` and `delete` to use the Plywood heap. If you don't want this behavior, perhaps because you're integrating Plywood into an existing application, define [`PLY_OVERRIDE_NEW=0`](/docs/configuration).

You can create and destroy C++ objects in the Plywood heap directly using `Heap::create` and `Heap::destroy`, which essentially work like `new` and `delete`:

`template <typename Type> static Type* create<Type>(Args&&... args)`
> Allocates heap memory for an object of type `Type` and calls the constructor. The provided arguments are passed to the constructor using [perfect forwarding](https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers).

`template <typename Type> static void destroy(Type* obj)`
> Invokes the destructor of an object and frees its memory back to the heap.

{example}
Owned<Foo> createFoo() {
    return Heap::create<Foo>();
}

void destroy(Foo* foo) {
    Heap::destroy(foo);
}
{/example}

### Monitoring the Heap

`static void setOutOfMemoryHandler(Functor<void()> handler)`
> Sets an out-of-memory handler. `handler` will be called if an allocation fails due to insufficient system memory.
>
> Out-of-memory events are usually unrecoverable. There's really no ideal way to handle them, other than to collect a report when the event occurs so that the issue can be investigated. In general, developers should establish a memory budget and aim to stay within it.

`static Heap::Stats getStats()`
> Returns statistics about heap usage. `numBytesAllocated` is the sum of the sizes of all allocated blocks. `virtualMemorySize`, a larger number, is the total amount of system memory used to store those blocks, including bookkeeping overhead and unused space.
> ```cpp
> struct Heap::Stats {
>     uptr totalBytesConsumed;     // Total number of bytes consumed by heap allocations including chunk headers.
>     uptr totalSystemMemoryUsed;  // Total number of bytes currently committed via the system's VirtualMemory API.
> }
> ```

`static void validate()`
> Validates the heap's internal consistency. Useful for debugging. Will force an immediate crash if the heap is
> corrupted, which is usually caused by a memory overrun or dangling pointer. Inserting calls to `validate` can help
> track down the cause of the corruption.
>
> `validate` performs checks only when `PLY_WITH_ASSERTS` is enabled.

## `VirtualMemory`

The `VirtualMemory` class is a platform-independent wrapper for mapping virtual memory to physical memory.

In C++ applications, memory is represented as a 32-bit or 64-bit address space known as virtual memory, which is divided into fixed-sized pages. Most pages are initially unusable and will cause an access violation or segmentation fault if accessed. To make pages usable, they must be mapped to physical memory by the underlying operating system.

### System Information

{context class=VirtualMemory}

`static VirtualMemory::Properties getProperties()`
> Returns information about the system's virtual memory page size and allocation alignment. `VirtualMemory::Properties` has these members:
>
> {table caption="`VirtualMemory::Properties` members"}
> `uptr`|regionAlignment|`reserveRegion` and `allocRegion` sizes must be a multiple of this
> `uptr`|pageSize|`commitPages` sizes must be a multiple of this
> {/table}

`static VirtualMemory::SystemStats getSystemStats()`
> Returns statistics about the current process's virtual memory usage. `VirtualMemory::SystemStats` has platform-specific members:
>
> On Windows:
>
> {table caption="`VirtualMemory::SystemStats` members (Windows)"}
> `uptr`|privateUsage
> `uptr`|workingSetSize
> {/table}
>
> On other platforms:
>
> {table caption="`VirtualMemory::SystemStats` members (POSIX)"}
> `uptr`|virtualSize
> `uptr`|residentSize
> {/table}

### Managing Pages

`static void* reserveRegion(uptr numBytes)`
> Reserves a region of address space. Memory pages are initially uncommitted. Returns `nullptr` on failure. `numBytes` must be a multiple of `regionAlignment`.

`static void unreserveRegion(void* addr, uptr numReservedBytes, uptr numCommittedBytes)`
> Unreserves a region of address space. `numReservedBytes` must match the argument passed to `reserveRegion`. Caller is responsible for passing the correct `numCommittedBytes`, otherwise stats will get out of sync.

`static void commitPages(void* addr, uptr numBytes)`
> Commits a subregion of reserved address space, making it legal to read and write to the subregion. `addr` must be aligned to `pageSize` and `numBytes` must be a multiple of `pageSize`.

`static void decommitPages(void* addr, uptr numBytes)`
> Decommits a subregion of previously committed memory. `addr` must be aligned to `pageSize` and `numBytes` must be a multiple of `pageSize`.

### Allocating Large Blocks

`static void* allocRegion(uptr numBytes)`
> Reserves and commits a region of address space. Returns `nullptr` on failure. Free using `freeRegion`. Don't decommit any pages in the returned region, otherwise stats will get out of sync. `numBytes` must be a multiple of `regionAlignment`.

`static void freeRegion(void* addr, uptr numBytes)`
> Decommits and unreserves a region of address space. `numBytes` must match the argument passed to `allocRegion`.

### Usage Stats

`static Atomic<uptr> totalReservedBytes`
> The current total amount of address space that was reserved using `allocRegion` or `reserveRegion`.

`static Atomic<uptr> totalCommittedBytes`
> The current total amount of memory that was committed using `allocRegion` or `commitPages`.

## Heap Implementation Details

The allocator is implemented by an internal `HeapImpl` object. Its global state is conceptually:

```cpp
struct HeapState {
    Mutex mutex;

    Segment* segmentHead;
    Segment* segmentTail;

    Chunk* smallBins[32];
    Chunk* treeBins[32];
    u32 smallMap;
    u32 treeMap;

    Chunk* designatedVictim;
    Chunk* top;

    DirectChunk* directHead;

    Heap::Stats stats;
};
```

- Boundary tags are what make constant-time coalescing possible.
- Immediate coalescing avoids long-lived adjacent free chunks, reducing fragmentation pressure.
- Small bins are fast for common tiny allocations; tree bins control fragmentation for larger sizes.
- The designated-victim chunk is a hot free chunk cache that avoids unnecessary bin traffic.
- `Heap::validate()` is your first debugging tool for corruption: call it frequently while narrowing bugs.

### Linked list of virtual memory segments allocated from the underlying OS

`segmentHead`/`segmentTail` track all regular heap segments allocated with `VirtualMemory::allocRegion`.
Each segment contains a sequence of boundary-tag chunks plus an in-use fence-post header at the end.

### Table of small bins

`smallBins[32]` stores free chunks with sizes `< 256` bytes.
Each bin is a doubly linked list. Size classes are 8-byte wide.

### Table of tree bins

`treeBins[32]` stores free chunks with sizes `>= 256` bytes.
Each tree is ordered by `(chunkSize, address)` and searched with best-fit logic.

### Linked list of direct-mapped chunks

`directHead` tracks large allocations that bypass regular segments and map memory directly from the OS.
These chunks are marked with a dedicated boundary-tag flag and are released with `VirtualMemory::freeRegion`.

### Internal counters

`stats.totalBytesConsumed` is the sum of in-use chunk sizes (including chunk headers).
`stats.totalSystemMemoryUsed` is the total currently mapped/committed memory used by segments and direct maps.

## Virtual Memory Segments

A regular segment is carved into sequential chunks:

```text
[Segment header][chunk][chunk][chunk]...[fence header]
```

Chunk headers use boundary tags:

```cpp
struct ChunkHeader {
    uptr prevFoot; // size of previous chunk when previous chunk is free
    uptr head;     // chunk size plus in-use flags
};

struct Chunk : ChunkHeader {
    // Free-chunk links:
    // - small-bin doubly-linked list pointers, or
    // - tree parent/left/right pointers
};
```

- Chunks are contiguous in memory and navigated by `size` fields.
- `prevFoot` plus the `prev-in-use` bit allows O(1) backward coalescing.
- Freeing is immediately coalescing: if adjacent chunks are free, they are merged at once.
- The final header in a segment is a permanently in-use fence post.

## How Allocation Works

Allocation routes through one of four paths: small bins, designated victim, tree bins, or top chunk. Very large requests use direct mapping.

### Pseudocode

```text
alloc(numBytes):
    size = align_and_add_header(numBytes)

    if size >= direct_map_threshold:
        return direct_map_alloc(size)

    chunk = find_small_bin_fit(size)
    if not chunk:
        chunk = use_designated_victim(size)
    if not chunk:
        chunk = find_best_fit_tree_chunk(size)
    if not chunk:
        chunk = split_from_top_or_grow_segments(size)

    return chunk_to_mem(chunk)
```

### Small-bin codepath

- For requests `< 256` bytes, small bins are checked first.
- If a suitable free chunk is found, it is unlinked.
- If the chunk is larger than needed, the tail remainder is split and kept as the designated-victim chunk.

### Tree-bin codepath

- For `>= 256` bytes, the allocator does a best-fit tree search.
- The chosen node is removed from its tree.
- The chunk is split if needed, and remainders are routed through designated-victim storage.

### Direct-mapped codepath

- Large requests skip bins and segment space.
- A dedicated mapping is created with `VirtualMemory::allocRegion`.
- The mapped chunk is linked into `directHead` and marked `direct-mapped`.
- On free, it is unlinked and returned directly to the OS.

### Top-chunk codepath

- `top` is the wilderness chunk at the end of one segment.
- If no bin candidate exists, allocation splits `top`.
- If `top` is too small, a new segment is mapped and installed as the new wilderness source.
