`VirtualMemory` (`ply-system.h`)
================================

The `VirtualMemory` class is a platform-independent wrapper for mapping virtual memory to physical memory.

In C++ applications, memory is represented as a 32-bit or 64-bit address space known as virtual memory, which is
divided into fixed-sized pages. Most pages are initially unusable and will cause an access violation or segmentation
fault if accessed. To make pages usable, they must be mapped to physical memory by the underlying operating system.

## System Information

{context class=VirtualMemory}

`static VirtualMemory::Properties getProperties()`
> Returns information about the system's virtual memory page size and allocation alignment. `VirtualMemory::Properties` has the following data members:
>
> | | |
> | --- | --- |
> | `uptr regionAlignment` | `reserveRegion` and `allocRegion` sizes must be a multiple of this. |
> | `uptr pageSize` | `commitPages` sizes must be a multiple of this. |

`static VirtualMemory::SystemStats getSystemStats()`
> Returns platform-specific statistics about the current process's virtual memory usage.
> `VirtualMemory::SystemStats` has the following data members on Windows:
>
> | | |
> | --- | --- |
> | `uptr privateUsage` | Private memory allocated by the process. |
> | `uptr workingSetSize` | Physical memory in the process working set. |
>
> On other platforms:
>
> | | |
> | --- | --- |
> | `uptr virtualSize` | Virtual address space used by the process. |
> | `uptr residentSize` | Physical memory resident for the process. |

## Managing Pages

`static void* reserveRegion(uptr numBytes)`
> Reserves a region of address space. Memory pages are initially uncommitted. Returns `nullptr` on failure. `numBytes` must be a multiple of `regionAlignment`.

`static void unreserveRegion(void* addr, uptr numReservedBytes, uptr numCommittedBytes)`
> Unreserves a region of address space. `numReservedBytes` must match the argument passed to `reserveRegion`. Caller is responsible for passing the correct `numCommittedBytes`, otherwise stats will get out of sync.

`static void commitPages(void* addr, uptr numBytes)`
> Commits a subregion of reserved address space, making it legal to read and write to the subregion. `addr` must be aligned to `pageSize` and `numBytes` must be a multiple of `pageSize`.

`static void decommitPages(void* addr, uptr numBytes)`
> Decommits a subregion of previously committed memory. `addr` must be aligned to `pageSize` and `numBytes` must be a multiple of `pageSize`.

## Allocating Large Blocks

`static void* allocRegion(uptr numBytes)`
> Reserves and commits a region of address space. Returns `nullptr` on failure. Free using `freeRegion`. Don't decommit any pages in the returned region, otherwise stats will get out of sync. `numBytes` must be a multiple of `regionAlignment`.

`static void freeRegion(void* addr, uptr numBytes)`
> Decommits and unreserves a region of address space. `numBytes` must match the argument passed to `allocRegion`.

## Usage Stats

| | |
| --- | --- |
| `static Atomic<uptr> totalReservedBytes` | The current total amount of address space that was reserved using `allocRegion` or `reserveRegion`. |
| `static Atomic<uptr> totalCommittedBytes` | The current total amount of memory that was committed using `allocRegion` or `commitPages`. |
