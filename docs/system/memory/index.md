Memory Management (`ply-system.h`)
================================

At the lowest level, there's [virtual memory](/docs/system/memory/virtual-memory.md), which can be mapped to
physical memory using the underlying operating system.

The [heap](/docs/system/memory/heap.md) sits directly above that, dividing memory into variable-sized blocks that
can be dynamically allocated and freed by the application as needed. Higher-level containers for managing memory,
like `String`, `Array`, `Map`, `Owned` and `Reference`, are described in later sections.
