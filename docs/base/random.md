{title text="Random Numbers" include="ply-base.h" namespace="ply"}

The `Random` class generates pseudorandom integers and floating-point values.

{api_summary class=Random title="Random member functions"}
Random()
Random(u64 seed)
--
u32 generate_u32()
u64 generate_u64()
double generate_double()
{/api_summary}

This class uses the well-known [xoroshiro128**](http://xorshift.di.unimi.it/) algorithm by Sebastiano Vigna and David Blackman internally. To use it, construct a `Random` object (optionally passing an explicit seed), then call any of its member functions as many times as needed.

This class isn't thread-safe. Its member functions must not be called concurrently from separate threads.

{api_descriptions class=Random}
Random()
--
Constructs a pseudorandom number generator seeded from the system clock.

>>
Random(u64 seed)
--
Constructs a pseudorandom number generator using an explicit seed. The same `seed` always generates the same sequence of pseudorandom numbers.

>>
u32 generate_u32()
--
Returns an unsigned integer uniformly distibuted over the entire range of representable 32-bit values. Use the modulo `%` operator to restrict the value to a smaller range.

>>
u64 generate_u64()
--
Returns an unsigned integer uniformly distibuted over the entire range of representable 64-bit values. Use the modulo `%` operator to restrict the value to a smaller range.

>>
double generate_double()
--
Returns a uniformly-distributed floating-point number that is greater than or equal to 0.0 and less than 1.0.
{/api_descriptions}

    // Generate some 4-digit random numbers
    Random r;
    Stream out = get_stdout();
    for (u32 i = 0; i < 5; i++) {
        out.format("{}\n", r.generate_u32() % 9000 + 1000);
    }

{output}
2910
7364
5994
8732
1678
{/output}
