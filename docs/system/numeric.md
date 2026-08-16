Numeric Functions and Types (`ply-system.h`)
==========================================

Plywood defines the following explicily-sized integer types, similar to `<stdint.h>` from the C standard library. These types are used internally throughout Plywood.

### Integer Types

| | |
| --- | --- |
| `s8` | Signed 8-bit integer |
| `s16` | Signed 16-bit integer |
| `s32` | Signed 32-bit integer |
| `s64` | Signed 64-bit integer |
| `u8` | Unsigned 8-bit integer |
| `u16` | Unsigned 16-bit integer |
| `u32` | Unsigned 32-bit integer |
| `u64` | Unsigned 64-bit integer |
| `sptr` | Signed pointer-sized integer |
| `uptr` | Unsigned pointer-sized integer |

### Primitive Functions

`template <typename Type> Type abs(Type value)`
> Returns the absolute value of any integer or floating-point value.

`template <typename Type> Type min(Type value1, Type value2)`
> Returns the minimum of two integer or floating-point values.

`template <typename Type> Type max(Type value1, Type value2)`
> Returns the maximum of two integer or floating-point values.

`template <typename Type> Type clamp(Type value, Type lowerBound, Type upperBound)`
> Clamps an integer or floating-point value to lie between a lower and upper bound.
> ```
> clamp(-1, 0, 10);  // returns 0
> clamp(5, 0, 10);   // returns 5
> clamp(11, 0, 10);  // returns 10
> ```

### Alignment

`u32 isPowerOf2(u32 value)`
`u64 isPowerOf2(u64 value)`
> Returns `true` if a `value` is a power of 2, `false` otherwise.

`u32 alignToPowerOf2(u32 value, u32 alignment)`
`u64 alignToPowerOf2(u64 value, u64 alignment)`
> Rounds `value` up to the nearest multiple of `alignment`, which must be a power of 2.

`bool isAlignedToPowerOf2(u32 value, u32 alignment)`
`bool isAlignedToPowerOf2(u64 value, u64 alignment)`
> Returns `true` if `value` is a multiple of `alignment`, which must be a power of 2.

`u32 roundUpToPowerOf2(u32 value)`
`u64 roundUpToPowerOf2(u64 value)`
> Rounds `value` up to the nearest power of 2.

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

### Numeric Casts and Limits

`template <typename DstType, typename SrcType> bool isRepresentable(SrcType value)`
> Returns `true` if `value` can be represented by the destination type `DstType`; `false` otherwise.
> ```
> isRepresentable<u16>(1234);   // returns true
> isRepresentable<u32>(-1234);  // returns false
> ```

`template <typename DstType, typename SrcType> DstType numericCast(SrcType value)`
> Casts `value` from one numeric type to another under the assumption that the value can be represented by the destination type. Will [assert](/docs/system/preprocessor-macros.md#assert) at runtime if the value can't be represented.
> ```
> s32 value = foo();
> if (value >= 0) {
>     u32 value2 = numericCast<u32>(value);  // OK
> }
> u32 value3 = numericCast<u32>(-1234);      // error: triggers runtime assertion
> ```

`template <typename Type> Type minRepresentableValue()`
`template <typename Type> Type maxRepresentableValue()`
> Returns the minimum or maximum representable value for a given type `Type`. Works with all integer and floating-point types.
> ```
> s32 value = maxRepresentableValue<s32>();      // returns 0x7fffffff
> float value = minRepresentableValue<float>();  // returns -3.402823466e+38f
> ```
