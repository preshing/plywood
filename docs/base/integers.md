{title text="Integer Types" include="ply-base.h" namespace="ply"}

Plywood defines the following explicily-sized integer types, similar to `<stdint.h>` from the C standard library. These types are used internally throughout Plywood.

### Fixed-Sized Integers

{table}
`s8` | Signed 8-bit integer
`s16` | Signed 16-bit integer
`s32` | Signed 32-bit integer
`s64` | Signed 64-bit integer
`u8` | Unsigned 8-bit integer
`u16` | Unsigned 16-bit integer
`u32` | Unsigned 32-bit integer
`u64` | Unsigned 64-bit integer
{/table}

### Pointer-Sized Integers

These are equivalent to `ptrdiff_t`/`size_t` from the C standard library. They're defined as `s64`/`u64` in 64-bit environments and `s32`/`u32` in 32-bit environments such as WebAssembly.

{table}
`sptr` | Signed pointer-sized integer
`uptr` | Unsigned pointer-sized integer
{/table}
