# `<ply-base.h>` — The Base Library

Most of the base library is exposed through the single header file `<ply-base.h>`. This is where you'll find cross-platform operating system support, container types and commonly-used convenience functions.

### About the Container Types

The C++ language gives you primitive types, pointers and structs. Plywood containers extend that with resizable arrays, associative maps, variants and owned objects, letting you create and manipulate complex data structures using few lines of code.

These containers are flexible, but not the most memory-efficient. In particular, `Map`, `Array` and `Variant` come with plenty of bookkeeping overhead and unused bytes. That said, they're usually more memory-efficient than equivalent Python or JavaScript data structures.
