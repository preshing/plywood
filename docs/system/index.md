`ply-system.h`: System
======================

`ply-system.h` (located in the `src/` folder) declares Plywood's cross-platform API for memory management, timers, file systems, threads and processes. All other Plywood source and header files depend on this one. When this header file is used in a project, `ply-system.cpp` should be compiled and linked as well.

You can customize the System API by defining any of the following preprocessor macros in your project's build settings:

| | |
| --- | --- |
| `PLY_CONFIG_FILE` | The path to a file that will be automatically included by [`<ply-system.h>`](/docs/common). Additional configuration options can be put here. |
| `PLY_WITH_ASSERTS` | Controls whether [assertions](/docs/system/preprocessor-macros.md#assertions) are enabled. Default is 1 in Debug configurations, 0 otherwise. |
| `PLY_OVERRIDE_NEW` | Controls whether C++'s built-in `new` and `delete` operators should allocate from [Plywood's built-in heap](/docs/system/memory/heap.md). Default is 1. Projects that already use their own `new`/`delete` overloads should set this to 0. |
| `PLY_WITH_DIRECTORY_WATCHER` | When set to 1, enables the optional [`DirectoryWatcher`](/docs/system/file-system.md#directory-watcher) feature. Default is 0. |

The System API is organized into the following categories:

- [Preprocessor Macros](/docs/system/preprocessor-macros.md): Identifies the target platform; defines assertions and other macros.
- [Numeric Functions and Types](/docs/system/numeric.md): Fixed-sized integers and primitive functions.
- [Memory Management](/docs/system/memory/index.md): Common data structures with a built-in heap.
- [Functors](/docs/system/functors.md): Class template for dynamic callback functions.
- [Generic Algorithms](/docs/system/algorithms.md): Searching and sorting.
- [Random Numbers](/docs/system/random-numbers.md): Fast, well-distributed pseudorandom number generation.
- [Time and Date](/docs/system/time-and-date.md): System time and date with performance timer.
- [Input/Output](/docs/system/input-output.md): Pipes, streams and Unicode conversion.
- [File System](/docs/system/file-system.md): File and path manipulation with directory watcher.
- [Multithreading](/docs/system/multithreading.md): Thread-local variables, atomics, mutexes, condition variables, read-write locks, semaphores.
- [Processes](/docs/system/processes.md): Spawn subprocesses and read/write directly to their standard I/O ports.
