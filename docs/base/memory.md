{title text="Memory" include="ply-base.h" namespace="ply"}

At the lowest level, there's **virtual memory**, which can be mapped to physical memory using the underlying operating system.

The **heap** sits directly above that, dividing memory into variable-sized blocks that can be dynamically allocated and freed by the application as needed. Higher-level containers for managing memory, like `String`, `Array`, `Map`, `Owned` and `Reference`, are described in later sections.

## `Heap`

Plywood contains its own heap, which lets you allocate and free variable-sized blocks of memory.

{api_summary class=Heap}
-- Low-level allocation
static void* alloc(uptr num_bytes)
static void* realloc(void* ptr, uptr num_bytes)
static void* free(void* ptr)
static void* alloc_aligned(uptr num_bytes, u32 alignment)
-- Creating and destroying objects
static T* create<T>(Args&&... args)
static void destroy(T* obj)
-- Monitoring the heap
static void set_out_of_memory_handler(Functor<void()> handler)
static Heap::Stats get_stats()
static void validate()
{/api_summary}

The Plywood heap is separate from the C Standard Library's heap. Both heaps can coexist in the same program, but memory allocated from a specific heap must always be freed using the same heap. Plywood's heap implementation uses [dlmalloc](https://gee.cs.oswego.edu/dl/html/malloc.html) under the hood.

`Heap` is thread-safe. All member functions can be called concurrently from separate threads.

### Low-level Allocation

{api_descriptions class=Heap}
static void* alloc(uptr num_bytes)
--
Allocates a block of memory from the Plywood heap. Equivalent to `malloc`. Always returns 16-byte aligned memory, suitable for SIMD vectors. Returns `nullptr` if allocation fails.

>>
static void* realloc(void* ptr, uptr num_bytes)
--
Resizes a previously allocated block. The contents are preserved up to the smaller of the old and new sizes. May return a different pointer.

>>
static void* free(void* ptr)
--
Frees a previously allocated block, returning the memory to the heap.

>>
static void* alloc_aligned(uptr num_bytes, u32 alignment)
--
Allocates memory with a specific alignment. Use for alignments greater than 16 bytes.
{/api_descriptions}

### Creating and Destroying Objects

By default, Plywood will override C++ `new` and `delete` to use the Plywood heap. If you don't want this behavior, perhaps because you're integrating Plywood into an existing application, define [`PLY_OVERRIDE_NEW=0`](/docs/configuration).

You can create and destroy C++ objects in the Plywood heap directly using `Heap::create` and `Heap::destroy`, which essentially work like `new` and `delete`:

{api_descriptions class=Heap}
template <typename Type> static Type* create<Type>(Args&&... args)
--
Allocates heap memory for an object of type `Type` and calls the constructor. The provided arguments are passed to the constructor using [perfect forwarding](https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers).

>>
template <typename Type> static void destroy(Type* obj)
--
Invokes the destructor of an object and frees its memory back to the heap.
{/api_descriptions}

{example}
Owned<Foo> create_foo() {
    return Heap::create<Foo>();
}

void destroy(Foo* foo) {
    Heap::destroy(foo);
}
{/example}

### Monitoring the Heap

{api_descriptions class=Heap}
static void set_out_of_memory_handler(Functor<void()> handler)
--
Sets an out-of-memory handler. `handler` will be called if an allocation fails due to insufficient system memory.

Out-of-memory events are usually unrecoverable. There's really no ideal way to handle them, other than to collect a report when the event occurs so that the issue can be investigated. In general, developers should establish a memory budget and aim to stay within it.

>>
static Heap::Stats get_stats()
--
Returns statistics about heap usage. `num_bytes_allocated` is the sum of the sizes of all allocated blocks. `virtual_memory_size`, a larger number, is the total amount of system memory used to store those blocks, including bookkeeping overhead and unused space.

{table caption="`Heap::Stats` members"}
`uptr`|num_bytes_allocated
`uptr`|virtual_memory_size
{/table}
>>
static void validate()
--
Validates the heap's internal consistency. Useful for debugging. Will force an immediate crash if the heap is corrupted, which is usually caused by a memory overrun or dangling pointer. Inserting calls to `validate` can help track down the cause of the corruption.
{/api_descriptions}

## `VirtualMemory`

The `VirtualMemory` class is a platform-independent wrapper for mapping virtual memory to physical memory.

{api_summary class=VirtualMemory title="VirtualMemory member functions"}
-- System Information
static Properties get_properties()
static SystemStats get_system_stats()
-- Managing Pages
static void* reserve_region(uptr num_bytes)
static void unreserve_region(void* addr, uptr num_reserved_bytes, uptr num_committed_bytes)
static void commit_pages(void* addr, uptr num_bytes)
static void decommit_pages(void* addr, uptr num_bytes)
-- Allocating Large Blocks
static void* alloc_region(uptr num_bytes)
static void free_region(void* addr, uptr num_bytes)
-- Usage Stats
static Atomic<uptr> total_reserved_bytes
static Atomic<uptr> total_committed_bytes
{/api_summary}

In C++ applications, memory is represented as a 32-bit or 64-bit address space known as virtual memory, which is divided into fixed-sized pages. Most pages are initially unusable and will cause an access violation or segmentation fault if accessed. To make pages usable, they must be mapped to physical memory by the underlying operating system.

### System Information

{api_descriptions class=VirtualMemory}
static VirtualMemory::Properties get_properties()
--
Returns information about the system's virtual memory page size and allocation alignment. `VirtualMemory::Properties` has these members:

{table caption="`VirtualMemory::Properties` members"}
`uptr`|region_alignment|`reserve_region` and `alloc_region` sizes must be a multiple of this
`uptr`|page_size|`commit_pages` sizes must be a multiple of this
{/table}

>>
static VirtualMemory::SystemStats get_system_stats()
--
Returns statistics about the current process's virtual memory usage. `VirtualMemory::SystemStats` has platform-specific members:

On Windows:

{table caption="`VirtualMemory::SystemStats` members (Windows)"}
`uptr`|private_usage
`uptr`|working_set_size
{/table}

On other platforms:

{table caption="`VirtualMemory::SystemStats` members (POSIX)"}
`uptr`|virtual_size
`uptr`|resident_size
{/table}
{/api_descriptions}

### Managing Pages

{api_descriptions class=VirtualMemory}
static void* reserve_region(uptr num_bytes)
--
Reserves a region of address space. Memory pages are initially uncommitted. Returns `nullptr` on failure. `num_bytes` must be a multiple of `region_alignment`.

>>
static void unreserve_region(void* addr, uptr num_reserved_bytes, uptr num_committed_bytes)
--
Unreserves a region of address space. `num_reserved_bytes` must match the argument passed to `reserve_region`. Caller is responsible for passing the correct `num_committed_bytes`, otherwise stats will get out of sync.

>>
static void commit_pages(void* addr, uptr num_bytes)
--
Commits a subregion of reserved address space, making it legal to read and write to the subregion. `addr` must be aligned to `page_size` and `num_bytes` must be a multiple of `page_size`.

>>
static void decommit_pages(void* addr, uptr num_bytes)
--
Decommits a subregion of previously committed memory. `addr` must be aligned to `page_size` and `num_bytes` must be a multiple of `page_size`.
{/api_descriptions}

### Allocating Large Blocks

{api_descriptions class=VirtualMemory}
static void* alloc_region(uptr num_bytes)
--
Reserves and commits a region of address space. Returns `nullptr` on failure. Free using `free_region`. Don't decommit any pages in the returned region, otherwise stats will get out of sync. `num_bytes` must be a multiple of `region_alignment`.

>>
static void free_region(void* addr, uptr num_bytes)
--
Decommits and unreserves a region of address space. `num_bytes` must match the argument passed to `alloc_region`.
{/api_descriptions}

### Usage Stats

{api_descriptions class=VirtualMemory}
static Atomic<uptr> total_reserved_bytes
--
The current total amount of address space that was reserved using `alloc_region` or `reserve_region`.

>>
static Atomic<uptr> total_committed_bytes
--
The current total amount of memory that was committed using `alloc_region` or `commit_pages`.
{/api_descriptions}

