`ply-system.h`: System
======================

`ply-system.h` (located in the `src/` folder) declares Plywood's cross-platform API for memory management, timers, file systems, threads and processes. All other Plywood source and header files depend on this one. When this header file is used in a project, `ply-system.cpp` should be compiled and linked as well.

You can customize the System API by defining any of the following preprocessor macros in your project's build settings:

| | |
| --- | --- |
| `PLY_CONFIG_FILE` | The path to a file that will be automatically included by [`<ply-system.h>`](/docs/common). Additional configuration options can be put here. |
| `PLY_WITH_ASSERTS` | Controls whether [assertions](/docs/system/preprocessor-macros.md#assertions) are enabled. Default is 1 in Debug configurations, 0 otherwise. |
| `PLY_OVERRIDE_NEW` | Controls whether C++'s built-in `new` and `delete` operators should allocate from [Plywood's built-in heap](/docs/system/memory/heap.md). Default is 1. Projects that already use own `new`/`delete` overloads should set this to 0. |
| `PLY_WITH_DIRECTORY_WATCHER` | When set to 1, enables the optional [`DirectoryWatcher`](/docs/system/file-system.md#directory-watcher) feature. Default is 0. |

The System API is organized into the following categories:

- [Preprocessor Macros](/docs/system/preprocessor-macros.md): Identifies the target platform, defines assertions and other convenient macros.
- [Numeric Functions and Types](/docs/system/numeric.md): Fixed-sized integers and primitive functions.
- [Memory Management](/docs/system/memory/index.md): Common data structures with a built-in heap.
    - [Virtual Memory](/docs/system/memory/virtual-memory.md): Map virtual address space to physical memory pages.
    - [Heap](/docs/system/memory/heap.md): Plywood's built-in heap. All dynamic allocations in Plywood go through here.
    - [Strings](/docs/system/strings.md): String classes suitable for UTF-8 or arbitrary binary data.
    - [Arrays](/docs/system/arrays.md): Class templates providing resizable arrays.
    - [Associative Maps](/docs/system/associative-maps.md): Resizable collections supporting fast hash lookup.
    - [Variants](/docs/system/variants.md): Class template providing variant types.
    - [Object Ownership](/docs/system/object-ownership.md): Reference-counting and owning pointers.
- [Functors](/docs/system/functors.md): Class template for dynamic callback functions.
- [Generic Algorithms](/docs/system/algorithms.md): Searching and sorting.
- [Random Numbers](/docs/system/random-numbers.md): Fast, well-distributed pseudorandom number generation.
- [Time and Date](/docs/system/time-and-date.md): System time and date with performance timer.
- [Input/Output](/docs/system/input-output.md): Pipes, streams and Unicode conversion.
- [File System](/docs/system/file-system.md): File and path manipulation with directory watcher.
- [Multithreading](/docs/system/multithreading.md): Thread-local variables, atomics, mutexes, condition variables, read-write locks and semaphores.
- [Processes](/docs/system/processes.md): Spawn subprocess and read/write from their standard ports.
