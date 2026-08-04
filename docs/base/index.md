{title text="The Base Library" include="ply-base.h" namespace="ply"}

This is where you'll find cross-platform operating system support, container types and commonly-used convenience functions. All other source files in Plywood include it.

Because of its compact size, it's easy to integrate into existing projects or use as the starting point of existing projects.

You can customize Plywood by defining the following preprocessor macros in your project's build settings.

{table caption="Configuration Options"}
`PLY_CONFIG_FILE` | The path to a file that will be automatically included by [`<ply-base.h>`](/docs/common). Additional configuration options can be put here.
`PLY_WITH_ASSERTS` | Enables [assertions](/docs/base/macros#assertions). Default is 1 in debug builds, 0 otherwise.
`PLY_WITH_DIRECTORY_WATCHER` | Enables the [`DirectoryWatcher`](/docs/base/filesystem#directory-watcher). Default is 0.
`PLY_OVERRIDE_NEW` | Overrides the C++ `new` and `delete` operators to allocate from the [Plywood heap](/docs/base/memory#heap). Default is 1.
`PLY_USE_NEW_ALLOCATOR` | Selects the heap backend. `1` uses Plywood's bespoke allocator, `0` uses legacy dlmalloc. Default is 1.
{/table}

### About the Container Types

The C++ language gives you primitive types, pointers and structs. Plywood containers extend that with resizable arrays, associative maps, variants and owned objects, letting you create and manipulate complex data structures using few lines of code.

These containers are flexible, but not the most memory-efficient. In particular, `Map`, `Array` and `Variant` come with plenty of bookkeeping overhead and unused bytes. That said, they're usually more memory-efficient than equivalent Python or JavaScript data structures.
