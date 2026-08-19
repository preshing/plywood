`ply-system.h`: System Interface
================================

This is where Plywood provides cross-platform access to basic operating system services such as memory management, timers, file systems, threads and processes. Whenever `ply-system.h` is included in a project, `ply-system.cpp` must also be compiled and linked in.

| | |
| --- | --- |
| `ply-system.h` | API declarations | ~4,900 lines |
| `ply-system.cpp` | Implementation | ~6,800 lines |

The System Interface is a single-header, monolithic API organized into the following categories:

- [Preprocessor Macros](/docs/system/preprocessor-macros.md): Identifies the target platform; defines assertions and other macros.
- [Numeric Functions and Types](/docs/system/numeric.md): Fixed-sized integers and primitive functions.
- [Memory Management](/docs/system/memory/index.md): Primitive data structures with a built-in heap.
- [Functors](/docs/system/functors.md): Class template for dynamic callback functions.
- [Generic Algorithms](/docs/system/algorithms.md): Searching and sorting.
- [Random Numbers](/docs/system/random-numbers.md): Fast, well-distributed pseudorandom number generation.
- [Time and Date](/docs/system/time-and-date.md): System clock and performance timer.
- [Input/Output](/docs/system/input-output.md): Pipes, streams and Unicode conversion.
- [File System](/docs/system/file-system.md): File and path manipulation with directory watcher.
- [Multithreading](/docs/system/multithreading.md): Thread-local variables, atomics, mutexes, condition variables, read-write locks, semaphores.
- [Processes](/docs/system/processes.md): Spawn subprocesses and read/write directly to their standard I/O ports.

## Configuration Options

You can customize this module by defining any of the following preprocessor macros in your project's build settings.

| Name               | Description        | Default |
| ------------------ | ------------------ | ------- |
| `PLY_CONFIG_FILE`  | The path to another file that will be included by [`ply-system.h`](/docs/common). Additional configuration options can be put here. | _None_ |
| `PLY_WITH_ASSERTS` | Controls whether [assertions](/docs/system/preprocessor-macros.md#assertions) are enabled. | 1 in Debug builds; 0 otherwise |
| `PLY_OVERRIDE_NEW` | Controls whether C++'s built-in `new` and `delete` operators should allocate from [Plywood's built-in heap](/docs/system/memory/heap.md). Projects already having their own `new`/`delete` overloads should set this to 0. | 1 |
| `PLY_WITH_DIRECTORY_WATCHER` | Enables the optional [`DirectoryWatcher`](/docs/system/file-system.md#directory-watcher) feature. | 0 |
