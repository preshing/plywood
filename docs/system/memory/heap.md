`Heap` (`ply-system.h`)
=======================

Plywood contains its own heap, which lets you allocate and free variable-sized blocks of memory.

The Plywood heap is separate from the Standard C Library's heap. Both heaps can coexist in the same program, but
memory allocated from a specific heap must always be freed using the same heap.

`Heap` is thread-safe. All member functions can be called concurrently from separate threads.

## Low-level Allocation

{context class=Heap}

`static void* alloc(uptr numBytes)`
> Allocates a block of memory from the Plywood heap. Equivalent to `malloc`. Always returns 16-byte aligned memory, suitable for SIMD vectors. Returns `nullptr` if allocation fails.

`static void* realloc(void* ptr, uptr numBytes)`
> Resizes a previously allocated block. The contents are preserved up to the smaller of the old and new sizes. May return a different pointer.

`static void* free(void* ptr)`
> Frees a previously allocated block, returning the memory to the heap.

`static void* allocAligned(uptr numBytes, u32 alignment)`
> Allocates memory with a specific alignment. Use for alignments greater than 16 bytes.

## Creating and Destroying Objects

By default, Plywood will override C++ `new` and `delete` to use the Plywood heap. If you don't want this behavior, perhaps because you're integrating Plywood into an existing application, define [`PLY_OVERRIDE_NEW=0`](/docs/configuration).

You can create and destroy C++ objects in the Plywood heap directly using `Heap::create` and `Heap::destroy`, which essentially work like `new` and `delete`:

`template <typename Type> static Type* create<Type>(Args&&... args)`
> Allocates heap memory for an object of type `Type` and calls the constructor. The provided arguments are passed to the constructor using [perfect forwarding](https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers).

`template <typename Type> static void destroy(Type* obj)`
> Invokes the destructor of an object and frees its memory back to the heap.

```
Owned<Foo> createFoo() {
    return Heap::create<Foo>();
}

void destroy(Foo* foo) {
    Heap::destroy(foo);
}
```

## Monitoring the Heap

`static void setOutOfMemoryHandler(Functor<void()> handler)`
> Sets an out-of-memory handler. `handler` will be called if an allocation fails due to insufficient system memory.
>
> Out-of-memory events are usually unrecoverable. There's really no ideal way to handle them, other than to collect a report when the event occurs so that the issue can be investigated. In general, developers should establish a memory budget and aim to stay within it.

`static Heap::Stats getStats()`
> Returns statistics about heap usage.
>
> | | |
> | --- | --- |
> | `uptr totalBytesConsumed` | The total number of bytes consumed by heap allocations, including chunk headers. |
> | `uptr totalSystemMemoryUsed` | The total number of bytes currently committed through the system's `VirtualMemory` API. |

`static void validate()`
> Validates the heap's internal consistency. Useful for debugging. Will force an immediate crash if the heap is
> corrupted, which is usually caused by a memory overrun or dangling pointer. Inserting calls to `validate` can help
> track down the cause of the corruption.
>
> `validate` performs checks only when `PLY_WITH_ASSERTS` is enabled.
