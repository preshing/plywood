# ply-base.h - Base API

`ply-base.h` contains the base [application programming interface (API)](https://en.wikipedia.org/wiki/API) of the Plywood C++ runtime library. It provides access to clocks, filesystems, processes, threads, virtual memory and basic data containers. All other header files in the library include this one.

[diagram: plywood-include-graph]

Lives alongside existing system APIs. You don't need to convert the your app to Plywood. You can adopt it incrementally or use it to build middleware that can be shared easily between C++ projects.

You can customize Plywood by defining the following preprocessor macros in your project's build settings.

{table caption="Configuration Options"}
`PLY_CONFIG_FILE` | The path to a file that will be automatically included by [`<ply-base.h>`](/docs/common). Additional configuration options can be put here.
`PLY_WITH_ASSERTS` | Enables [assertions](/docs/base/preprocessor-macros.md#assertions). Default is 1 in debug builds, 0 otherwise.
`PLY_WITH_DIRECTORY_WATCHER` | Enables the [`DirectoryWatcher`](/docs/base/filesystem.md#directory-watcher). Default is 0.
`PLY_OVERRIDE_NEW` | Overrides the C++ `new` and `delete` operators to allocate from the [Plywood heap](/docs/base/memory-management.md#heap). Default is 1.
`PLY_USE_NEW_ALLOCATOR` | Selects the heap backend. `1` uses Plywood's bespoke allocator, `0` uses legacy dlmalloc. Default is 1.
{/table}

### About the Container Types

The C++ language gives you primitive types, pointers and structs. Plywood containers extend that with resizable arrays, associative maps, variants and owned objects, letting you create and manipulate complex data structures using few lines of code.

These containers are flexible, but not the most memory-efficient. In particular, `Map`, `Array` and `Variant` come with plenty of bookkeeping overhead and unused bytes. That said, they're usually more memory-efficient than equivalent Python or JavaScript data structures.
