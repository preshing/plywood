{title text="Heap Design" include="ply-base.h" namespace="ply"}

This page describes the internal design of Plywood's bespoke heap allocator (`PLY_USE_NEW_ALLOCATOR=1`).
It is an implementation note for maintainers and contributors.

## Internal Heap State

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

## Notes for Junior Programmers

- Boundary tags are what make constant-time coalescing possible.
- Immediate coalescing avoids long-lived adjacent free chunks, reducing fragmentation pressure.
- Small bins are fast for common tiny allocations; tree bins control fragmentation for larger sizes.
- The designated-victim chunk is a hot free chunk cache that avoids unnecessary bin traffic.
- `Heap::validate()` is your first debugging tool for corruption: call it frequently while narrowing bugs.
