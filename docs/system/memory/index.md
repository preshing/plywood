Memory Management (`ply-system.h`)
================================

At the lowest level, there's [virtual memory](/docs/system/memory/virtual-memory.md), which can be mapped to
physical memory using the underlying operating system.

The [heap](/docs/system/memory/heap.md) sits directly above that, dividing memory into variable-sized blocks that
can be dynamically allocated and freed by the application as needed. Higher-level containers for managing memory,
like `String`, `Array`, `Map`, `Owned` and `Reference`, are described in later sections.

- [Virtual Memory](/docs/system/memory/virtual-memory.md): Map virtual address space to physical memory pages.
- [Heap](/docs/system/memory/heap.md): Plywood's built-in heap. All dynamic allocations in Plywood go through here.
- [Strings](/docs/system/memory/strings.md): String classes suitable for UTF-8 or arbitrary binary data.
- [Arrays](/docs/system/memory/arrays.md): Class templates providing resizable arrays.
- [Associative Maps](/docs/system/memory/associative-maps.md): Resizable collections supporting fast hash lookup.
- [Variants](/docs/system/memory/variants.md): Variant types similar to tagged unions with safety checks.
- [Object Ownership](/docs/system/memory/object-ownership.md): Reference-counting and owning pointers.
