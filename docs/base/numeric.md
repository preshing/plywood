Numeric Functions and Types (`ply-base.h`)
==========================================

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

Plywood defines the following primitive numeric functions.

{apiSummary}
-- Limits
template <typename Type> Type getMinValue()
template <typename Type> Type getMaxValue()
-- Adjusting Values
template <typename Type> Type abs(Type value)
template <typename Type> Type min(Type value1, Type value2)
template <typename Type> Type max(Type value1, Type value2)
template <typename Type> Type clamp(Type value, Type lo, Type hi)
-- Byte Ordering
u16 reverseBytes(u16 value)
u32 reverseBytes(u32 value)
u64 reverseBytes(u64 value)
template <typename Type> Type convertLittleEndian(Type value)
template <typename Type> Type convertBigEndian(Type value)
-- Power-of-2 Alignment
u32 isPowerOf2(u32 value)
u64 isPowerOf2(u64 value)
u32 alignToPowerOf2(u32 value, u32 alignment)
u64 alignToPowerOf2(u64 value, u64 alignment)
bool isAlignedToPowerOf2(u32 value, u32 alignment)
bool isAlignedToPowerOf2(u64 value, u64 alignment)
u32 roundUpToNearestPowerOf2(u32 value)
u64 roundUpToNearestPowerOf2(u64 value)
-- Numeric Casts With Bounds Checking
template <typename DstType, typename SrcType> bool isRepresentable(SrcType value)
template <typename DstType, typename SrcType> DstType numericCast(SrcType value)
{/apiSummary}

### Limits

`template <typename Type> Type getMinValue()`
`template <typename Type> Type getMaxValue()`
> Returns the minimum or maximum representable value for a given type `Type`. Available for all integer or floating-point types. Equivalent to `INT32_MIN`, `FLT_MAX` and similar constants from the C standard library, but implemented as template functions.
> ```
> s32 value = getMaxValue<s32>();      // returns 0x7fffffff
> float value = getMinValue<float>();  // returns -3.402823466e+38f
> ```

### Adjusting Values

`template <typename Type> Type abs(Type value)`
> Returns the absolute value of any integer or floating-point value.
> ```
> s32 value = abs(-10);      // returns 10
> float value = abs(3.14f);  // returns 3.14f
> ```

`template <typename Type> Type min(Type value1, Type value2)`
> Returns the minimum of two integer or floating-point values.
> ```
> s32 value = min(10, 20);          // returns 10
> float value = min(3.14f, 2.71f);  // returns 2.71f
> ```

`template <typename Type> Type max(Type value1, Type value2)`
> Returns the maximum of two integer or floating-point values.
> ```
> s32 value = max(10, 20);          // returns 20
> float value = max(3.14f, 2.71f);  // returns 3.14f
> ```

`template <typename Type> Type clamp(Type value, Type lowerBound, Type upperBound)`
> Clamps an integer or floating-point value to lie between a lower and upper bound.
> ```
> s32 value = clamp(5, 0, 10);             // returns 5
> float value = clamp(3.14f, 0.0f, 1.0f);  // returns 1.0f
> ```

### Byte Ordering

`u16 reverseBytes(u16 value)`
`u32 reverseBytes(u32 value)`
`u64 reverseBytes(u64 value)`
> Reverses the byte order of a value. Used internally by `convertLittleEndian()` or `convertBigEndian()`.
> ```
> u16 value = reverseBytes(0x1234);  // returns 0x3412
> ```

`template <typename Type> Type convertLittleEndian(Type value)`
`template <typename Type> Type convertBigEndian(Type value)`
> Converts a native integer to little-endian or big-endian byte order. Also converts it back again. `Type` must be one of `u16`, `u32`, or `u64`.
>
> These days, nearly every platform is little-endian, so these functions aren't often needed. The main use for these functions today is to work with [networking APIs](/docs/networking.md), where certain arguments are expected in big-endian order.

### Power-of-2 Alignment

`u32 isPowerOf2(u32 value)`
`u64 isPowerOf2(u64 value)`
> Returns `true` if a `value` is a power of 2, `false` otherwise.

`u32 alignToPowerOf2(u32 value, u32 alignment)`
`u64 alignToPowerOf2(u64 value, u64 alignment)`
> Rounds `value` up to the nearest multiple of `alignment`, which must be a power of 2.

`bool isAlignedToPowerOf2(u32 value, u32 alignment)`
`bool isAlignedToPowerOf2(u64 value, u64 alignment)`
> Returns `true` if `value` is a multiple of `alignment`, which must be a power of 2.

`u32 roundUpToNearestPowerOf2(u32 value)`
`u64 roundUpToNearestPowerOf2(u64 value)`
> Rounds `value` up to the nearest power of 2.

### Numeric Casts With Bounds Checking

`template <typename DstType, typename SrcType> bool isRepresentable(SrcType value)`
> Returns `true` if `value` can be represented by the destination type `DstType`; `false` otherwise.
> ```
> isRepresentable<u16>(1234);   // returns true
> isRepresentable<u32>(-1234);  // returns false
> ```

`template <typename DstType, typename SrcType> DstType numericCast(SrcType value)`
> Casts `value` from one numeric type to another under the assumption that the value can be represented by the destination type. Will [assert](/docs/base/preprocessor-macros.md#assert) at runtime if the value can't be represented.
> ```
> s32 value = foo();
> if (value >= 0) {
>     u32 value2 = numericCast<u32>(value);  // OK
> }
> u32 value3 = numericCast<u32>(-1234);      // error: triggers runtime assertion
> ```
