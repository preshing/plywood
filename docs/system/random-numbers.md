Random Numbers (`ply-system.h`)
=========================================

The `Random` class generates pseudorandom integers and floating-point values.

This class uses the well-known [xoroshiro128**](http://xorshift.di.unimi.it/) algorithm by Sebastiano Vigna and David Blackman internally. To use it, construct a `Random` object (optionally passing an explicit seed), then call any of its member functions as many times as needed.

This class isn't thread-safe. Its member functions must not be called concurrently from separate threads.

{context class=Random}

`Random()`
> Constructs a pseudorandom number generator seeded from the system clock.

`Random(u64 seed)`
> Constructs a pseudorandom number generator using an explicit seed. The same `seed` always generates the same sequence of pseudorandom numbers.

`u32 generateU32()`
> Returns an unsigned integer uniformly distibuted over the entire range of representable 32-bit values. Use the modulo `%` operator to restrict the value to a smaller range.

`u64 generateU64()`
> Returns an unsigned integer uniformly distibuted over the entire range of representable 64-bit values. Use the modulo `%` operator to restrict the value to a smaller range.

`double generateDouble()`
> Returns a uniformly-distributed floating-point number that is greater than or equal to 0.0 and less than 1.0.

```
// Generate some 4-digit random numbers.
Random r;
Stream out = getStdOut();
for (u32 i = 0; i < 5; i++) {
    out.format("{}\n", r.generateU32() % 9000 + 1000);
}

// Output:
// 2910
// 7364
// 5994
// 8732
// 1678
```
