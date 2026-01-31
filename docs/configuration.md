# Configuration Options

You can customize Plywood by defining the following preprocessor macros in your project's build settings.

{table caption="Configuration Options"}
`PLY_CONFIG_FILE` | The path to a file that will be automatically included by [`<ply-base.h>`](/docs/common). Additional configuration options can be put here.
`PLY_WITH_ASSERTS` | Enables [assertions](/docs/base/macros#assertions). Default is 1 in debug builds, 0 otherwise.
`PLY_WITH_DIRECTORY_WATCHER` | Enables the [`DirectoryWatcher`](/docs/base/filesystem#directory-watcher). Default is 0.
`PLY_OVERRIDE_NEW` | Overrides the C++ `new` and `delete` operators to allocate from the [Plywood heap](/docs/base/memory#heap). Default is 1.
`PLY_TRACK_VIRTUAL_MEMORY_USAGE` | Enables usage tracking in [`VirtualMemory`](/docs/base/memory#virtual-memory). Default is 0.
{/table}
