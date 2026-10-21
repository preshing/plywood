/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include "run-system-tests.h"
#include <ply-math.h>
#include <math.h>

//  ▄▄  ▄▄                               ▄▄
//  ███ ██ ▄▄  ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄
//  ██▀███ ██  ██ ██ ██ ██ ██▄▄██ ██  ▀▀ ██ ██
//  ██  ██ ▀█▄▄██ ██ ██ ██ ▀█▄▄▄  ██     ██ ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Numeric_

TEST_CASE("isRepresentable") {
    // Integer ranges
    check(isRepresentable<u32>(0));
    check(isRepresentable<u32>(123));
    check(!isRepresentable<u32>(-5));
    check(isRepresentable<s32>(-5));
    check(!isRepresentable<s8>(200));
    check(isRepresentable<u8>(200));
    check(!isRepresentable<s8>(u8(200)));
    check(isRepresentable<u16>(maxRepresentableValue<s16>()));
    check(!isRepresentable<s16>(maxRepresentableValue<u16>()));
    check(!isRepresentable<s32>(minRepresentableValue<s64>()));
    check(!isRepresentable<s32>(maxRepresentableValue<s64>()));
    check(!isRepresentable<s32>(maxRepresentableValue<u64>()));
    check(!isRepresentable<u32>(minRepresentableValue<s64>()));
    check(!isRepresentable<u32>(maxRepresentableValue<s64>()));
    check(!isRepresentable<u32>(maxRepresentableValue<u64>()));
    check(isRepresentable<s64>(minRepresentableValue<s32>()));
    check(isRepresentable<s64>(maxRepresentableValue<s32>()));
    check(isRepresentable<s64>(maxRepresentableValue<u32>()));
    check(!isRepresentable<s64>(maxRepresentableValue<u64>()));
    check(!isRepresentable<u64>(minRepresentableValue<s32>()));
    check(isRepresentable<u64>(maxRepresentableValue<s32>()));
    check(isRepresentable<u64>(maxRepresentableValue<u32>()));
    check(isRepresentable<u64>(maxRepresentableValue<s64>()));

    // float to int
    check(isRepresentable<u32>(123.0f));
    check(!isRepresentable<u32>(123.25f));
    check(isRepresentable<s32>(-2147483648.0f));

    // int to float
    check(isRepresentable<float>(16777216));
    check(!isRepresentable<float>(16777217));

    // float to float
    check(isRepresentable<double>(16777216.f));
    check(!isRepresentable<float>(16777217.0));
}

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Math_

static bool isClose(const Mat4x4& a, const Mat4x4& b, float epsilon = 0.0001f) {
    for (u32 c = 0; c < 4; c++) {
        for (u32 r = 0; r < 4; r++) {
            if (abs(a.col[c][r] - b.col[c][r]) > epsilon)
                return false;
        }
    }
    return true;
}

static u32 mathFloatBits(float value) {
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float mathFloatFromBits(u32 bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static u32 mathUlpDistance(float a, float b) {
    if (a == b)
        return 0;
    u32 aBits = mathFloatBits(a);
    u32 bBits = mathFloatBits(b);
    u32 aOrdered = (aBits & 0x80000000u) ? ~aBits : aBits | 0x80000000u;
    u32 bOrdered = (bBits & 0x80000000u) ? ~bBits : bBits | 0x80000000u;
    return aOrdered > bOrdered ? aOrdered - bOrdered : bOrdered - aOrdered;
}

static u64 mathDoubleBits(double value) {
    u64 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double mathDoubleFromBits(u64 bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static u64 mathUlpDistance(double a, double b) {
    if (a == b)
        return 0;
    u64 aBits = mathDoubleBits(a);
    u64 bBits = mathDoubleBits(b);
    u64 aOrdered = (aBits & 0x8000000000000000ull) ? ~aBits : aBits | 0x8000000000000000ull;
    u64 bOrdered = (bBits & 0x8000000000000000ull) ? ~bBits : bBits | 0x8000000000000000ull;
    return aOrdered > bOrdered ? aOrdered - bOrdered : bOrdered - aOrdered;
}

TEST_CASE("Trigonometric special values") {
    float negativeZero = mathFloatFromBits(0x80000000u);
    float infinity = mathFloatFromBits(0x7f800000u);
    float negativeInfinity = mathFloatFromBits(0xff800000u);
    float nan = mathFloatFromBits(0x7fc00000u);

    check(mathFloatBits(ply::sin(0.f)) == 0);
    check(mathFloatBits(ply::sin(negativeZero)) == 0x80000000u);
    check(mathFloatBits(ply::tan(0.f)) == 0);
    check(mathFloatBits(ply::tan(negativeZero)) == 0x80000000u);
    check(ply::cos(0.f) == 1.f);
    check(ply::cos(negativeZero) == 1.f);
    check(ply::cosAndSin(0.f) == Float2{1, 0});
    check(ply::cosAndSin(negativeZero) == Float2{1, negativeZero});
    check(ply::sin(infinity) != ply::sin(infinity));
    check(ply::cos(negativeInfinity) != ply::cos(negativeInfinity));
    check(ply::tan(infinity) != ply::tan(infinity));
    Float2 invalidCosSin = ply::cosAndSin(infinity);
    check(invalidCosSin.x != invalidCosSin.x);
    check(invalidCosSin.y != invalidCosSin.y);
    check(ply::sin(nan) != ply::sin(nan));
    check(ply::cos(nan) != ply::cos(nan));
    check(ply::tan(nan) != ply::tan(nan));
}

TEST_CASE("Trigonometric accuracy") {
    static constexpr float inputs[] = {
        -1000000.f, -1000.f, -2 * Pi, -Pi,        -Pi * 0.5f, -Pi * 0.25f, -1.f,   -0.5f,  -0.0001f,  0.f,
        0.0001f,    0.5f,    1.f,     Pi * 0.25f, Pi * 0.5f,  Pi,          2 * Pi, 1000.f, 1000000.f,
    };
    u32 maxSinDistance = 0;
    u32 maxCosDistance = 0;
    u32 maxTanDistance = 0;
    u32 maxCosSinDistance = 0;
    float worstSinInput = 0;
    float worstCosInput = 0;
    float worstTanInput = 0;
    auto measure = [&](float input) {
        u32 sinDistance = mathUlpDistance(ply::sin(input), ::sinf(input));
        u32 cosDistance = mathUlpDistance(ply::cos(input), ::cosf(input));
        u32 tanDistance = mathUlpDistance(ply::tan(input), ::tanf(input));
        Float2 cosAndSin = ply::cosAndSin(input);
        maxCosSinDistance = max(maxCosSinDistance, mathUlpDistance(cosAndSin.x, ::cosf(input)));
        maxCosSinDistance = max(maxCosSinDistance, mathUlpDistance(cosAndSin.y, ::sinf(input)));
        if (sinDistance > maxSinDistance) {
            maxSinDistance = sinDistance;
            worstSinInput = input;
        }
        if (cosDistance > maxCosDistance) {
            maxCosDistance = cosDistance;
            worstCosInput = input;
        }
        if (tanDistance > maxTanDistance) {
            maxTanDistance = tanDistance;
            worstTanInput = input;
        }
    };
    for (float input : inputs)
        measure(input);
    measure(mathFloatFromBits(0x7f7fffffu));
    measure(mathFloatFromBits(0xff7fffffu));

    // Sample all finite exponent ranges, including the large-input reduction path.
    u32 randomBits = 0x12345678u;
    for (u32 i = 0; i < 200000; i++) {
        randomBits = randomBits * 1664525u + 1013904223u;
        if ((randomBits & 0x7f800000u) == 0x7f800000u)
            continue;
        measure(mathFloatFromBits(randomBits));
    }
    if (maxSinDistance > 1 || maxCosDistance > 1 || maxTanDistance > 2 || maxCosSinDistance > 1) {
        getStdOut().format("\n  worst ULP distances: sin {} at {}, cos {} at {}, tan {} at {}\n", maxSinDistance,
                           worstSinInput, maxCosDistance, worstCosInput, maxTanDistance, worstTanInput);
        getStdOut().format("  results/reference: sin {}/{}, cos {}/{}, tan {}/{}\n", ply::sin(worstSinInput),
                           ::sinf(worstSinInput), ply::cos(worstCosInput), ::cosf(worstCosInput),
                           ply::tan(worstTanInput), ::tanf(worstTanInput));
        getStdOut().format("  worst cosAndSin ULP distance: {}\n", maxCosSinDistance);
    }
    check(maxSinDistance <= 1);
    check(maxCosDistance <= 1);
    check(maxTanDistance <= 2);
    check(maxCosSinDistance <= 1);
}

TEST_CASE("Scalar math special values") {
    float negativeZero = mathFloatFromBits(0x80000000u);
    float infinity = mathFloatFromBits(0x7f800000u);
    float negativeInfinity = mathFloatFromBits(0xff800000u);
    float nan = mathFloatFromBits(0x7fc00000u);

    check(mathFloatBits(roundDown(negativeZero)) == 0x80000000u);
    check(mathFloatBits(roundUp(negativeZero)) == 0x80000000u);
    check(mathFloatBits(roundNearest(negativeZero)) == 0x80000000u);
    check(roundDown(1.75f) == 1);
    check(roundDown(-1.25f) == -2);
    check(roundUp(1.25f) == 2);
    check(roundUp(-1.75f) == -1);
    check(roundNearest(1.5f) == 2);
    check(roundNearest(-1.5f) == -2);

    check(mathFloatBits(ply::sqrt(negativeZero)) == 0x80000000u);
    check(ply::sqrt(infinity) == infinity);
    check(ply::sqrt(-1.f) != ply::sqrt(-1.f));
    check(mathFloatBits(ply::fastSqrt(negativeZero)) == 0x80000000u);
    check(ply::fastSqrt(infinity) == infinity);
    check(ply::fastSqrt(-1) != ply::fastSqrt(-1));
    check(ply::fastInvSqrt(0) == infinity);
    check(ply::fastInvSqrt(negativeZero) == negativeInfinity);
    check(ply::fastInvSqrt(infinity) == 0);
    check(ply::fastInvSqrt(-1) != ply::fastInvSqrt(-1));
    check(ply::log(1.f) == 0);
    check(ply::log(0.f) == negativeInfinity);
    check(ply::log(-1.f) != ply::log(-1.f));
    check(ply::exp(negativeInfinity) == 0);
    check(ply::exp(infinity) == infinity);
    check(ply::exp(nan) != ply::exp(nan));

    check(mathFloatBits(ply::arctan(Float2{1, negativeZero})) == 0x80000000u);
    check(ply::arcsin(1.f) == Pi * 0.5f);
    check(ply::arcsin(-1.f) == -Pi * 0.5f);
    check(ply::arccos(1.f) == 0);
    check(mathUlpDistance(ply::arccos(-1.f), Pi) <= 1);
    check(ply::arcsin(2.f) != ply::arcsin(2.f));
    check(ply::arccos(2.f) != ply::arccos(2.f));

    check(ply::pow(-2.f, 3.f) == -8);
    check(ply::pow(-2.f, 4.f) == 16);
    check(ply::pow(-2.f, -3.f) == -0.125f);
    check(ply::pow(-2.f, 0.5f) != ply::pow(-2.f, 0.5f));
    check(ply::pow(0.f, -2.f) == infinity);
    check(ply::pow(-1.f, infinity) == 1);
    check(ply::pow(nan, 0.f) == 1);
    check(ply::pow(1.f, nan) == 1);
}

TEST_CASE("Scalar math accuracy") {
    u32 maxSqrtDistance = 0;
    u32 maxLogDistance = 0;
    u32 maxExpDistance = 0;
    u32 maxPowDistance = 0;
    u32 maxArcsinDistance = 0;
    u32 maxArccosDistance = 0;
    u32 maxArctanDistance = 0;
    u32 maxVectorArctanDistance = 0;
    bool roundingMatches = true;
    u32 randomBits = 0x8badf00du;
    for (u32 i = 0; i < 200000; i++) {
        randomBits = randomBits * 1664525u + 1013904223u;
        u32 positiveBits = randomBits & 0x7fffffffu;
        if (positiveBits < 0x7f800000u) {
            float positive = mathFloatFromBits(positiveBits);
            maxSqrtDistance = max(maxSqrtDistance, mathUlpDistance(ply::sqrt(positive), ::sqrtf(positive)));
            if (positive != 0)
                maxLogDistance = max(maxLogDistance, mathUlpDistance(ply::log(positive), ::logf(positive)));
        }

        float signedValue = mathFloatFromBits(randomBits);
        bool signedValueIsFinite = (randomBits & 0x7f800000u) != 0x7f800000u;
        if (signedValueIsFinite) {
            maxArctanDistance = max(maxArctanDistance, mathUlpDistance(ply::arctan(signedValue), ::atanf(signedValue)));
            roundingMatches &= mathFloatBits(roundDown(signedValue)) == mathFloatBits(::floorf(signedValue));
            roundingMatches &= mathFloatBits(roundUp(signedValue)) == mathFloatBits(::ceilf(signedValue));
            roundingMatches &= mathFloatBits(roundNearest(signedValue)) == mathFloatBits(::roundf(signedValue));
        }

        float unitValue = static_cast<s32>(randomBits) * (1.f / 2147483648.f);
        maxArcsinDistance = max(maxArcsinDistance, mathUlpDistance(ply::arcsin(unitValue), ::asinf(unitValue)));
        maxArccosDistance = max(maxArccosDistance, mathUlpDistance(ply::arccos(unitValue), ::acosf(unitValue)));

        float expInput = static_cast<s32>(randomBits) * (100.f / 2147483648.f);
        maxExpDistance = max(maxExpDistance, mathUlpDistance(ply::exp(expInput), ::expf(expInput)));

        randomBits = randomBits * 1664525u + 1013904223u;
        float other = mathFloatFromBits(randomBits);
        if ((randomBits & 0x7f800000u) != 0x7f800000u && signedValueIsFinite) {
            maxVectorArctanDistance =
                max(maxVectorArctanDistance,
                    mathUlpDistance(ply::arctan(Float2{other, signedValue}), ::atan2f(signedValue, other)));
        }

        float base = 0.01f + static_cast<float>((randomBits >> 8) & 0xffffu) * (16.f / 65535.f);
        randomBits = randomBits * 1664525u + 1013904223u;
        float exponent = static_cast<s32>(randomBits) * (8.f / 2147483648.f);
        maxPowDistance = max(maxPowDistance, mathUlpDistance(ply::pow(base, exponent), ::powf(base, exponent)));
    }
    if (maxSqrtDistance > 0 || maxLogDistance > 1 || maxExpDistance > 1 || maxPowDistance > 1 ||
        maxArcsinDistance > 1 || maxArccosDistance > 1 || maxArctanDistance > 1 || maxVectorArctanDistance > 1) {
        getStdOut().format("\n  worst ULP distances: sqrt {}, log {}, exp {}, pow {}, arcsin {}, arccos {}, arctan {}, "
                           "vector arctan {}\n",
                           maxSqrtDistance, maxLogDistance, maxExpDistance, maxPowDistance, maxArcsinDistance,
                           maxArccosDistance, maxArctanDistance, maxVectorArctanDistance);
    }
    check(maxSqrtDistance == 0);
    check(maxLogDistance <= 1);
    check(maxExpDistance <= 1);
    check(maxPowDistance <= 1);
    check(maxArcsinDistance <= 1);
    check(maxArccosDistance <= 1);
    check(maxArctanDistance <= 1);
    check(maxVectorArctanDistance <= 1);
    check(roundingMatches);
}

TEST_CASE("Binary64 scalar math signatures and special values") {
    double (*sqrtFunction)(double) = &ply::sqrt;
    double (*logFunction)(double) = &ply::log;
    double (*expFunction)(double) = &ply::exp;
    double (*powFunction)(double, double) = &ply::pow;
    double (*sinFunction)(double) = &ply::sin;
    double (*cosFunction)(double) = &ply::cos;
    double (*tanFunction)(double) = &ply::tan;
    double (*arcsinFunction)(double) = &ply::arcsin;
    double (*arccosFunction)(double) = &ply::arccos;
    double (*arctanFunction)(double) = &ply::arctan;
    float (*floatSqrtFunction)(float) = &ply::sqrt;
    float (*floatLogFunction)(float) = &ply::log;
    float (*floatExpFunction)(float) = &ply::exp;
    float (*floatPowFunction)(float, float) = &ply::pow;
    float (*floatSinFunction)(float) = &ply::sin;
    float (*floatCosFunction)(float) = &ply::cos;
    float (*floatTanFunction)(float) = &ply::tan;
    float (*floatArcsinFunction)(float) = &ply::arcsin;
    float (*floatArccosFunction)(float) = &ply::arccos;
    float (*floatArctanFunction)(float) = &ply::arctan;
    check(sqrtFunction && logFunction && expFunction && powFunction && sinFunction && cosFunction && tanFunction &&
          arcsinFunction && arccosFunction && arctanFunction && floatSqrtFunction && floatLogFunction &&
          floatExpFunction && floatPowFunction && floatSinFunction && floatCosFunction && floatTanFunction &&
          floatArcsinFunction && floatArccosFunction && floatArctanFunction);

    double negativeZero = mathDoubleFromBits(0x8000000000000000ull);
    double infinity = mathDoubleFromBits(0x7ff0000000000000ull);
    double negativeInfinity = mathDoubleFromBits(0xfff0000000000000ull);
    double nan = mathDoubleFromBits(0x7ff8000000000000ull);
    check(mathDoubleBits(ply::sqrt(negativeZero)) == 0x8000000000000000ull);
    check(ply::sqrt(infinity) == infinity);
    check(ply::sqrt(-1.0) != ply::sqrt(-1.0));
    check(mathDoubleBits(ply::sin(negativeZero)) == 0x8000000000000000ull);
    check(mathDoubleBits(ply::tan(negativeZero)) == 0x8000000000000000ull);
    check(ply::cos(negativeZero) == 1);
    check(ply::sin(infinity) != ply::sin(infinity));
    check(ply::log(0.0) == negativeInfinity);
    check(ply::log(-1.0) != ply::log(-1.0));
    check(ply::exp(negativeInfinity) == 0);
    check(ply::exp(infinity) == infinity);
    check(ply::exp(nan) != ply::exp(nan));
    check(ply::arcsin(1.0) == DPi * 0.5);
    check(ply::arcsin(-1.0) == -DPi * 0.5);
    check(ply::arccos(1.0) == 0);
    check(ply::arccos(-1.0) == DPi);
    check(ply::arcsin(2.0) != ply::arcsin(2.0));
    check(ply::arccos(2.0) != ply::arccos(2.0));
    check(ply::pow(-2.0, 3.0) == -8);
    check(ply::pow(-2.0, 4.0) == 16);
    check(ply::pow(-2.0, -3.0) == -0.125);
    check(ply::pow(-2, 0.5) != ply::pow(-2, 0.5));
    check(ply::pow(0.0, -2.0) == infinity);
    check(ply::pow(-1, infinity) == 1);
    check(ply::pow(nan, 0) == 1);
    check(ply::pow(1, nan) == 1);

    check(mathDoubleBits(roundDown(negativeZero)) == 0x8000000000000000ull);
    check(mathDoubleBits(roundUp(negativeZero)) == 0x8000000000000000ull);
    check(mathDoubleBits(roundNearest(negativeZero)) == 0x8000000000000000ull);
    check(roundDown(-1.25) == -2);
    check(roundUp(1.25) == 2);
    check(roundNearest(-1.5) == -2);
    check(square(3.0) == 9);
    check(wrap(-1.0, 4.0) == 3);
    check(mix(2.0, 6.0, 0.25) == 3);
    check(unmix(2.0, 6.0, 3.0) == 0.25);
    check(stepTowards(1.0, 3.0, 0.5) == 1.5);
}

TEST_CASE("Binary64 scalar math accuracy") {
    u64 maxSqrtDistance = 0;
    u64 maxLogDistance = 0;
    u64 maxExpDistance = 0;
    u64 maxPowDistance = 0;
    u64 maxSinDistance = 0;
    u64 maxCosDistance = 0;
    u64 maxTanDistance = 0;
    u64 maxArcsinDistance = 0;
    u64 maxArccosDistance = 0;
    u64 maxArctanDistance = 0;
    bool roundingMatches = true;
    u64 randomBits = 0x8badf00d12345678ull;
    for (u32 i = 0; i < 200000; i++) {
        randomBits = randomBits * 6364136223846793005ull + 1442695040888963407ull;
        u64 magnitude = randomBits & 0x7fffffffffffffffull;
        if (magnitude < 0x7ff0000000000000ull) {
            double positive = mathDoubleFromBits(magnitude);
            maxSqrtDistance = max(maxSqrtDistance, mathUlpDistance(ply::sqrt(positive), ::sqrt(positive)));
            if (positive != 0)
                maxLogDistance = max(maxLogDistance, mathUlpDistance(ply::log(positive), ::log(positive)));
        }
        if (magnitude >= 0x7ff0000000000000ull)
            continue;

        double signedValue = mathDoubleFromBits(randomBits);
        maxSinDistance = max(maxSinDistance, mathUlpDistance(ply::sin(signedValue), ::sin(signedValue)));
        maxCosDistance = max(maxCosDistance, mathUlpDistance(ply::cos(signedValue), ::cos(signedValue)));
        maxTanDistance = max(maxTanDistance, mathUlpDistance(ply::tan(signedValue), ::tan(signedValue)));
        maxArctanDistance = max(maxArctanDistance, mathUlpDistance(ply::arctan(signedValue), ::atan(signedValue)));
        roundingMatches &= mathDoubleBits(roundDown(signedValue)) == mathDoubleBits(::floor(signedValue));
        roundingMatches &= mathDoubleBits(roundUp(signedValue)) == mathDoubleBits(::ceil(signedValue));
        roundingMatches &= mathDoubleBits(roundNearest(signedValue)) == mathDoubleBits(::round(signedValue));

        double unitValue = static_cast<s64>(randomBits) * (1.0 / 9223372036854775808.0);
        maxArcsinDistance = max(maxArcsinDistance, mathUlpDistance(ply::arcsin(unitValue), ::asin(unitValue)));
        maxArccosDistance = max(maxArccosDistance, mathUlpDistance(ply::arccos(unitValue), ::acos(unitValue)));
        double expInput = static_cast<s64>(randomBits) * (750.0 / 9223372036854775808.0);
        maxExpDistance = max(maxExpDistance, mathUlpDistance(ply::exp(expInput), ::exp(expInput)));

        randomBits = randomBits * 6364136223846793005ull + 1442695040888963407ull;
        double base = 0.01 + static_cast<double>((randomBits >> 24) & 0xffffffu) * (16.0 / 16777215.0);
        double exponent = static_cast<s64>(randomBits) * (8.0 / 9223372036854775808.0);
        maxPowDistance = max(maxPowDistance, mathUlpDistance(ply::pow(base, exponent), ::pow(base, exponent)));
    }
    check(maxSqrtDistance == 0);
    check(maxLogDistance <= 1);
    check(maxExpDistance <= 1);
    check(maxPowDistance <= 2);
    check(maxSinDistance <= 1);
    check(maxCosDistance <= 1);
    check(maxTanDistance <= 2);
    check(maxArcsinDistance <= 1);
    check(maxArccosDistance <= 1);
    check(maxArctanDistance <= 1);
    check(roundingMatches);
}

TEST_CASE("Fast square root and vector normalization") {
    float maxSqrtRelativeError = 0;
    float maxInvSqrtRelativeError = 0;
    u32 randomBits = 0x51f15e5du;
    for (u32 i = 0; i < 200000; i++) {
        randomBits = randomBits * 1664525u + 1013904223u;
        u32 positiveBits = randomBits & 0x7fffffffu;
        if (positiveBits == 0 || positiveBits >= 0x7f800000u)
            continue;
        float value = mathFloatFromBits(positiveBits);
        float referenceSqrt = ::sqrtf(value);
        float referenceInvSqrt = 1.f / referenceSqrt;
        maxSqrtRelativeError = max(maxSqrtRelativeError, abs(ply::fastSqrt(value) / referenceSqrt - 1));
        maxInvSqrtRelativeError = max(maxInvSqrtRelativeError, abs(ply::fastInvSqrt(value) / referenceInvSqrt - 1));
    }
    check(maxSqrtRelativeError < 0.00001f);
    check(maxInvSqrtRelativeError < 0.00001f);

    Float2 v2 = {3, 4};
    Float3 v3 = {2, -3, 6};
    Float4 v4 = {1, -2, 2, 4};
    check(abs(v2.fastLength() / v2.length() - 1) < 0.00001f);
    check(abs(v3.fastLength() / v3.length() - 1) < 0.00001f);
    check(abs(v4.fastLength() / v4.length() - 1) < 0.00001f);
    check(abs(v2.fastNormalized().length() - 1) < 0.00001f);
    check(abs(v3.fastNormalized().length() - 1) < 0.00001f);
    check(abs(v4.fastNormalized().length() - 1) < 0.00001f);
}

TEST_CASE("Mat4x4::inverted affine") {
    Mat4x4 m = Mat4x4::translate({2, -3, 5}) * Mat4x4::rotate(Float3{1, 2, 3}.normalized(), 0.7f) *
               Mat4x4::scale({2, -4, 0.5f});
    Mat4x4 inv = m.inverted();

    check(isClose(inv * m, Mat4x4::identity()));
    check(isClose(m * inv, Mat4x4::identity()));
}

TEST_CASE("Mat4x4::inverted projective") {
    Mat4x4 m = {
        {2, -1, 0.5f, 0.25f},
        {1, 3, -2, -0.5f},
        {0, 4, 1.5f, 0.75f},
        {-3, 2, 5, 1},
    };
    Mat4x4 inv = m.inverted();

    check(isClose(inv * m, Mat4x4::identity()));
    check(isClose(m * inv, Mat4x4::identity()));
}
#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX String_

//  ▄▄▄▄▄▄ ▄▄                      ▄▄▄        ▄▄▄▄▄          ▄▄
//    ██   ▄▄ ▄▄▄▄▄▄▄   ▄▄▄▄      ██ ▀▀       ██  ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄
//    ██   ██ ██ ██ ██ ██▄▄██     ▄█▀█▄▀▀     ██  ██  ▄▄▄██  ██   ██▄▄██
//    ██   ██ ██ ██ ██ ▀█▄▄▄      ▀█▄▄▀█▄     ██▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄
//

TEST_CASE("String::fromDateTime") {
    DateTime dateTime;
    dateTime.year = 2025;
    dateTime.month = 12;
    dateTime.day = 1;
    dateTime.weekday = 1; // Monday
    dateTime.hour = 19;
    dateTime.minute = 0;
    dateTime.second = 1;
    dateTime.timeZoneOffsetInMinutes = -300;
    dateTime.microsecond = 234000;

    check(String::fromDateTime("%Y-%m-%d", dateTime) == "2025-12-01");
    check(String::fromDateTime("%H:%M:%S", dateTime) == "19:00:01");
    check(String::fromDateTime("%A, %B %e, %Y", dateTime) == "Monday, December 1, 2025");
    check(String::fromDateTime("%l:%M %p (UTC%Z)", dateTime) == "7:00 PM (UTC-05:00)");
    check(String::fromDateTime("[%Y:%m:%d %H:%M:%S.%L]", dateTime) == "[2025:12:01 19:00:01.234]");
}

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Thread_

TEST_CASE("Thread join") {
    int value = 0;
    Thread thread([&]() { value = 42; });
    thread.join();
    check(value == 42);
}

TEST_CASE("Atomic 8-bit operations") {
    Atomic<u8> value = 7;
    check(value.load(MemoryOrder::Relaxed) == 7);

    value.store(9, MemoryOrder::Release);
    check(value.load(MemoryOrder::Acquire) == 9);

    check(value.compareExchange(9, 12, MemoryOrder::AcqRel) == 9);
    check(value.load(MemoryOrder::Relaxed) == 12);
    check(value.compareExchange(9, 1, MemoryOrder::AcqRel) == 12);
    check(value.load(MemoryOrder::Relaxed) == 12);

    check(value.exchange(3, MemoryOrder::AcqRel) == 12);
    check(value.load(MemoryOrder::Relaxed) == 3);

    check(value.fetchAdd(4, MemoryOrder::AcqRel) == 3);
    check(value.load(MemoryOrder::Relaxed) == 7);
    check(value.fetchSub(2, MemoryOrder::AcqRel) == 7);
    check(value.load(MemoryOrder::Relaxed) == 5);

    check(value.fetchAnd(6, MemoryOrder::AcqRel) == 5);
    check(value.load(MemoryOrder::Relaxed) == 4);
    check(value.fetchOr(3, MemoryOrder::AcqRel) == 4);
    check(value.load(MemoryOrder::Relaxed) == 7);
}

TEST_CASE("Atomic 16-bit operations") {
    Atomic<u16> value = 0x1200;
    check(value.load(MemoryOrder::Relaxed) == 0x1200);

    value.store(0x1234, MemoryOrder::Release);
    check(value.load(MemoryOrder::Acquire) == 0x1234);

    check(value.compareExchange(0x1234, 0x4321, MemoryOrder::AcqRel) == 0x1234);
    check(value.load(MemoryOrder::Relaxed) == 0x4321);
    check(value.compareExchange(0x1234, 0xFFFF, MemoryOrder::AcqRel) == 0x4321);
    check(value.load(MemoryOrder::Relaxed) == 0x4321);

    check(value.exchange(0x0102, MemoryOrder::AcqRel) == 0x4321);
    check(value.load(MemoryOrder::Relaxed) == 0x0102);

    check(value.fetchAdd(0x0010, MemoryOrder::AcqRel) == 0x0102);
    check(value.load(MemoryOrder::Relaxed) == 0x0112);
    check(value.fetchSub(0x0003, MemoryOrder::AcqRel) == 0x0112);
    check(value.load(MemoryOrder::Relaxed) == 0x010F);

    check(value.fetchAnd(0x00FF, MemoryOrder::AcqRel) == 0x010F);
    check(value.load(MemoryOrder::Relaxed) == 0x000F);
    check(value.fetchOr(0x0F00, MemoryOrder::AcqRel) == 0x000F);
    check(value.load(MemoryOrder::Relaxed) == 0x0F0F);
}

TEST_CASE("Atomic 32-bit operations") {
    Atomic<u32> value = 0x12000000;
    check(value.load(MemoryOrder::Relaxed) == 0x12000000);

    value.store(0x12345678, MemoryOrder::Release);
    check(value.load(MemoryOrder::Acquire) == 0x12345678);

    check(value.compareExchange(0x12345678, 0x87654321, MemoryOrder::AcqRel) == 0x12345678);
    check(value.load(MemoryOrder::Relaxed) == 0x87654321);
    check(value.compareExchange(0x12345678, 0xFFFFFFFF, MemoryOrder::AcqRel) == 0x87654321);
    check(value.load(MemoryOrder::Relaxed) == 0x87654321);

    check(value.exchange(0x01020304, MemoryOrder::AcqRel) == 0x87654321);
    check(value.load(MemoryOrder::Relaxed) == 0x01020304);

    check(value.fetchAdd(0x00010010, MemoryOrder::AcqRel) == 0x01020304);
    check(value.load(MemoryOrder::Relaxed) == 0x01030314);
    check(value.fetchSub(0x00000024, MemoryOrder::AcqRel) == 0x01030314);
    check(value.load(MemoryOrder::Relaxed) == 0x010302F0);

    check(value.fetchAnd(0x00FF00FF, MemoryOrder::AcqRel) == 0x010302F0);
    check(value.load(MemoryOrder::Relaxed) == 0x000300F0);
    check(value.fetchOr(0x0F00000F, MemoryOrder::AcqRel) == 0x000300F0);
    check(value.load(MemoryOrder::Relaxed) == 0x0F0300FF);
}

TEST_CASE("Atomic 64-bit operations") {
    Atomic<u64> value = u64(0x1200000000000000ull);
    check(value.load(MemoryOrder::Relaxed) == u64(0x1200000000000000ull));

    value.store(u64(0x123456789ABCDEF0ull), MemoryOrder::Release);
    check(value.load(MemoryOrder::Acquire) == u64(0x123456789ABCDEF0ull));

    check(value.compareExchange(u64(0x123456789ABCDEF0ull), u64(0x0FEDCBA987654321ull), MemoryOrder::AcqRel) ==
          u64(0x123456789ABCDEF0ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x0FEDCBA987654321ull));
    check(value.compareExchange(u64(0x123456789ABCDEF0ull), u64(0xFFFFFFFFFFFFFFFFull), MemoryOrder::AcqRel) ==
          u64(0x0FEDCBA987654321ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x0FEDCBA987654321ull));

    check(value.exchange(u64(0x0102030405060708ull), MemoryOrder::AcqRel) == u64(0x0FEDCBA987654321ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x0102030405060708ull));

    check(value.fetchAdd(u64(0x0001001000010010ull), MemoryOrder::AcqRel) == u64(0x0102030405060708ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x0103031405070718ull));
    check(value.fetchSub(u64(0x0000000000000028ull), MemoryOrder::AcqRel) == u64(0x0103031405070718ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x01030314050706F0ull));

    check(value.fetchAnd(u64(0x00FF00FF00FF00FFull), MemoryOrder::AcqRel) == u64(0x01030314050706F0ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x00030014000700F0ull));
    check(value.fetchOr(u64(0x0F0000000000000Full), MemoryOrder::AcqRel) == u64(0x00030014000700F0ull));
    check(value.load(MemoryOrder::Relaxed) == u64(0x0F030014000700FFull));
}

//  ▄▄  ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀██ ██▄▄██  ▄▄▄██ ██  ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄██ ██▄▄█▀
//                       ██

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Heap_

TEST_CASE("Heap::getStats") {
    Heap::Stats beginStats = Heap::getStats();

    void* a = Heap::alloc(64);
    void* b = Heap::alloc(4096);
    check(a != nullptr);
    check(b != nullptr);

    Heap::Stats duringStats = Heap::getStats();
    check(duringStats.totalBytesConsumed >= beginStats.totalBytesConsumed + 64 + 4096);
    check(duringStats.totalSystemMemoryUsed >= duringStats.totalBytesConsumed);

    Heap::free(a);
    Heap::free(b);

    Heap::Stats endStats = Heap::getStats();
    check(endStats.totalBytesConsumed <= duringStats.totalBytesConsumed);
    check(endStats.totalSystemMemoryUsed >= endStats.totalBytesConsumed);
}

TEST_CASE("Heap::setOutOfMemoryHandler") {
    u32 outOfMemoryHandlerCallCount = 0;
    Heap::setOutOfMemoryHandler([&]() { outOfMemoryHandlerCallCount++; });

    void* ptr = Heap::alloc(maxRepresentableValue<uptr>());
    check(ptr == nullptr);
    check(outOfMemoryHandlerCallCount == 1);

    Heap::setOutOfMemoryHandler({});
}

TEST_CASE("Heap::allocAligned") {
    const u32 alignments[] = {16, 32, 64, 128, 256};
    for (u32 alignment : alignments) {
        void* ptr = Heap::allocAligned(113, alignment);
        check(ptr != nullptr);
        check(isAlignedToPowerOf2((u64) (uptr) ptr, (u64) alignment));
        if (ptr) {
            memset(ptr, 0x5A, 113);
            Heap::free(ptr);
        }
    }
    Heap::validate();
}

TEST_CASE("Heap::realloc grow/shrink") {
    u32 initialBytes = 96;
    u8* ptr = (u8*) Heap::alloc(initialBytes);
    check(ptr != nullptr);
    if (!ptr) {
        return;
    }

    for (u32 i = 0; i < initialBytes; i++) {
        ptr[i] = (u8) (i ^ 0xA5);
    }

    u32 grownBytes = 2048;
    u8* grown = (u8*) Heap::realloc(ptr, grownBytes);
    check(grown != nullptr);
    if (!grown) {
        Heap::free(ptr);
        return;
    }
    for (u32 i = 0; i < initialBytes; i++) {
        check(grown[i] == (u8) (i ^ 0xA5));
    }

    u32 shrunkBytes = 64;
    u8* shrunk = (u8*) Heap::realloc(grown, shrunkBytes);
    check(shrunk != nullptr);
    if (!shrunk) {
        Heap::free(grown);
        return;
    }
    for (u32 i = 0; i < shrunkBytes; i++) {
        check(shrunk[i] == (u8) (i ^ 0xA5));
    }

    Heap::free(shrunk);
    Heap::validate();
}

TEST_CASE("Heap coalescing") {
    void* a = Heap::alloc(120);
    void* b = Heap::alloc(120);
    void* c = Heap::alloc(120);
    check(a != nullptr);
    check(b != nullptr);
    check(c != nullptr);
    if (!(a && b && c)) {
        Heap::free(a);
        Heap::free(b);
        Heap::free(c);
        return;
    }

    Heap::free(a);
    Heap::free(b);
    void* d = Heap::alloc(220);
    check(d != nullptr);
    check(d == a);

    Heap::free(d);
    Heap::free(c);
    Heap::validate();
}

TEST_CASE("Heap stress") {
    struct Block {
        void* ptr = nullptr;
        u32 numBytes = 0;
    };

    Random rand{0x12345678};
    Array<Block> blocks;
    for (u32 i = 0; i < 4000; i++) {
        u32 action = rand.generateU32() % 3;
        if (blocks.isEmpty() || action == 0) {
            u32 numBytes = (rand.generateU32() % 4096) + 1;
            if ((rand.generateU32() % 64) == 0) {
                numBytes += 256 * 1024;
            }
            void* ptr = Heap::alloc(numBytes);
            check(ptr != nullptr);
            if (ptr) {
                blocks.append({ptr, numBytes});
            }
        } else if (action == 1) {
            u32 index = rand.generateU32() % blocks.numItems();
            u32 newBytes = (rand.generateU32() % 4096) + 1;
            if ((rand.generateU32() % 64) == 0) {
                newBytes += 256 * 1024;
            }
            void* ptr = Heap::realloc(blocks[index].ptr, newBytes);
            check(ptr != nullptr);
            if (ptr) {
                blocks[index].ptr = ptr;
                blocks[index].numBytes = newBytes;
            }
        } else {
            u32 index = rand.generateU32() % blocks.numItems();
            Heap::free(blocks[index].ptr);
            blocks.eraseQuick(index);
        }

        if ((i % 128) == 0) {
            Heap::validate();
        }
    }

    for (const Block& block : blocks) {
        Heap::free(block.ptr);
    }
    Heap::validate();
}

TEST_CASE("Heap multithread smoke") {
    Atomic<u32> failures = 0;
    static constexpr u32 NumThreads = 4;
    static constexpr u32 Iterations = 1200;
    Thread threads[NumThreads];

    for (u32 t = 0; t < NumThreads; t++) {
        threads[t].run([&failures, t]() {
            Random rand{t + 1};
            Array<void*> ptrs;
            for (u32 i = 0; i < Iterations; i++) {
                if (ptrs.isEmpty() || (rand.generateU32() % 2) == 0) {
                    u32 numBytes = (rand.generateU32() % 2048) + 1;
                    void* ptr = Heap::alloc(numBytes);
                    if (ptr) {
                        ptrs.append(ptr);
                    } else {
                        failures.fetchAdd(1, MemoryOrder::AcqRel);
                    }
                } else {
                    u32 index = rand.generateU32() % ptrs.numItems();
                    Heap::free(ptrs[index]);
                    ptrs.eraseQuick(index);
                }
            }
            for (void* ptr : ptrs) {
                Heap::free(ptr);
            }
        });
    }
    for (u32 t = 0; t < NumThreads; t++) {
        threads[t].join();
    }

    check(failures.load(MemoryOrder::Acquire) == 0);
    Heap::validate();
}

TEST_CASE("Heap::validate") {
    Heap::validate();
    void* ptr = Heap::alloc(256);
    check(ptr != nullptr);
    Heap::validate();
    Heap::free(ptr);
    Heap::validate();
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Hashing_

TEST_CASE("shuffle_bits() 32") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u32 value = rand.generateU32();
        u32 shuffled = shuffleBits(value);
        u32 unshuffled = unshuffleBits(shuffled);
        check(value == unshuffled);
    }
}

TEST_CASE("shuffle_bits() 64") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u64 value = rand.generateU32();
        u64 shuffled = shuffleBits(value);
        u64 unshuffled = unshuffleBits(shuffled);
        check(value == unshuffled);
    }
}

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX String_

TEST_CASE("String self-assignment") {
    String str = "How now brown cow?";
    str = str.shortenedBy(1);
    check(str == "How now brown cow");
}

TEST_CASE("String find") {
    String str = "abcdefgh";
    check(str.find([](char x) { return x == 'c'; }) == 2);
    check(str.find([](char x) { return x == 'z'; }) < 0);
    check(str.find('c') == 2);
    check(str.find('z') < 0);
}

TEST_CASE("String reverseFind") {
    String str = "abcdefgh";
    check(str.reverseFind([](char x) { return x == 'c'; }) == 2);
    check(str.reverseFind([](char x) { return x == 'z'; }) < 0);
    check(str.reverseFind('c') == 2);
    check(str.reverseFind('z') < 0);
}

TEST_CASE("String split") {
    // Single char separator
    {
        String str = "apple,banana,cherry";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
        check(parts[2] == "cherry");
    }
    // Multi char separator
    {
        String str = "apple::banana::cherry";
        Array<StringView> parts = str.split("::");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
        check(parts[2] == "cherry");
    }
    // Multi char separator with partial match in content
    {
        String str = "apple::banana:cherry::date";
        Array<StringView> parts = str.split("::");
        check(parts.numItems() == 3);
        check(parts[0] == "apple");
        check(parts[1] == "banana:cherry");
        check(parts[2] == "date");
    }
    // Consecutive separators (empty parts are skipped)
    {
        String str = "apple,,banana";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 2);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
    }
    // No separator found
    {
        String str = "hello world";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 1);
        check(parts[0] == "hello world");
    }
    // Empty string
    {
        String str = "";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 1);
        check(parts[0] == "");
    }
    // Separator at ends
    {
        String str = ",apple,banana,";
        Array<StringView> parts = str.split(",");
        check(parts.numItems() == 2);
        check(parts[0] == "apple");
        check(parts[1] == "banana");
    }
}

TEST_CASE("String match identifier") {
    String str = "(hello)";
    StringView identifier;
    check(str.match("'(%i')$", &identifier));
    check(identifier == "hello");
}

TEST_CASE("String match integer") {
    StringView str = "count: 42";
    s32 value = 0;
    check(str.match("count: %d", &value));
    check(value == 42);
}

TEST_CASE("String match negative integer") {
    StringView str = "offset=-123";
    s32 value = 0;
    check(str.match("offset=%d", &value));
    check(value == -123);
}

TEST_CASE("String match float") {
    StringView str = "pi=3.14159";
    double value = 0;
    check(str.match("pi=%f", &value));
    check(value > 3.14 && value < 3.15);
}

TEST_CASE("readDoubleFromText subnormal exponent") {
    // Ensure negative exponent scaling preserves representable subnormal values.
    ViewStream in{"1e-309"};
    ReadNumber<double> result = readDoubleFromText(in);
    check(result.success);
    check(in.curByte == in.endByte);
    check(result.value > 0);
}

TEST_CASE("readS64FromText signed limits") {
    ViewStream minIn{"-9223372036854775808"};
    ReadNumber<s64> minResult = readS64FromText(minIn);
    check(minResult.success);
    check(minResult.value == minRepresentableValue<s64>());

    ViewStream maxIn{"9223372036854775807"};
    ReadNumber<s64> maxResult = readS64FromText(maxIn);
    check(maxResult.success);
    check(maxResult.value == maxRepresentableValue<s64>());

    ViewStream underflowIn{"-9223372036854775809"};
    check(!readS64FromText(underflowIn).success);
    ViewStream overflowIn{"9223372036854775808"};
    check(!readS64FromText(overflowIn).success);
}

TEST_CASE("String match quoted string") {
    StringView str = "name=\"hello world\"";
    String value;
    check(str.match("name=%q", &value));
    check(value == "hello world");
}

TEST_CASE("String match quoted string escapes") {
    StringView str = "name=\"hello \\\"world\\\"\"";
    String value;
    check(str.match("name=%q", &value));
    check(value == "hello \"world\"");
}

TEST_CASE("readQuotedString C strict hex and unicode escapes") {
    ViewStream in{"\"A\\x42\\u03c0\\U0001f600\""};
    String value = readQuotedString(in, QuotedStringType::C, true);
    check(in.curByte == in.endByte);
    check(value == "AB\xcf\x80\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString JavaScript strict unicode escapes") {
    ViewStream in{"\"A\\x42\\u03c0\\u{1f600}\""};
    String value = readQuotedString(in, QuotedStringType::JavaScript, true);
    check(in.curByte == in.endByte);
    check(value == "AB\xcf\x80\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString JSON strict surrogate pairs") {
    ViewStream in{"\"\\uD83D\\uDE00\""};
    String value = readQuotedString(in, QuotedStringType::JSON, true);
    check(in.curByte == in.endByte);
    check(value == "\xf0\x9f\x98\x80");
}

TEST_CASE("readQuotedString Python strict triple quoted multiline") {
    ViewStream in{"'''hello\n\"there\"'''"};
    String value = readQuotedString(in, QuotedStringType::Python, true);
    check(in.curByte == in.endByte);
    check(value == "hello\n\"there\"");
}

TEST_CASE("readQuotedString JSON strict rejects x escape") {
    ViewStream in{"\"\\x41\""};
    bool hadError = false;
    String value = readQuotedString(in, QuotedStringType::JSON, true,
                                    [&](QuotedStringError) { hadError = true; });
    PLY_UNUSED(value);
    check(hadError);
}

TEST_CASE("readQuotedString JavaScript strict rejects big U escape") {
    ViewStream in{"\"\\U0001F600\""};
    bool hadError = false;
    String value = readQuotedString(in, QuotedStringType::JavaScript, true,
                                    [&](QuotedStringError) { hadError = true; });
    PLY_UNUSED(value);
    check(hadError);
}

TEST_CASE("readQuotedString JSON strict rejects lone low surrogate") {
    ViewStream in{"\"\\uDE00\""};
    bool hadError = false;
    String value = readQuotedString(in, QuotedStringType::JSON, true,
                                    [&](QuotedStringError) { hadError = true; });
    PLY_UNUSED(value);
    check(hadError);
}

TEST_CASE("readQuotedString JSON permissive keeps invalid single-character escapes") {
    ViewStream in{"\"\\#\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(in.curByte == in.endByte);
    check(value == "#");
}

TEST_CASE("readQuotedString JSON permissive preserves malformed unicode escape text") {
    ViewStream in{"\"\\u12Z4\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(in.curByte == in.endByte);
    check(value == "\\u12Z4");
}

TEST_CASE("readQuotedString C permissive preserves malformed big U escape text") {
    ViewStream in{"\"\\U0000G600\""};
    String value = readQuotedString(in, QuotedStringType::C, false);
    check(in.curByte == in.endByte);
    check(value == "\\U0000G600");
}

TEST_CASE("readQuotedString JSON permissive preserves malformed surrogate text") {
    ViewStream in{"\"\\uD83Dx\""};
    String value = readQuotedString(in, QuotedStringType::JSON, false);
    check(in.curByte == in.endByte);
    check(value == "\\uD83Dx");
}

TEST_CASE("String match whitespace") {
    StringView str = "hello   world";
    check(str.match("hello *world"));
}

TEST_CASE("String match whitespace with tab") {
    StringView str = "hello\tworld";
    check(str.match("hello world"));
}

TEST_CASE("String match optional whitespace") {
    StringView str1 = "hello world";
    StringView str2 = "helloworld";
    check(str1.match("hello ?world"));
    check(str2.match("hello ?world"));
}

TEST_CASE("String match escape character") {
    StringView str = "%test%";
    StringView id;
    check(str.match("'%%i'%", &id));
    check(id == "test");
}

TEST_CASE("String match group alternation") {
    StringView str1 = "color: red";
    StringView str2 = "color: blue";
    StringView str3 = "color: green";
    check(str1.match("color: (red|blue|green)"));
    check(str2.match("color: (red|blue|green)"));
    check(str3.match("color: (red|blue|green)"));
}

TEST_CASE("String match group alternation fail") {
    StringView str = "color: yellow";
    check(!str.match("color: (red|blue|green)"));
}

TEST_CASE("String match zero or more") {
    StringView str1 = "ab";
    StringView str2 = "aab";
    StringView str3 = "aaab";
    check(str1.match("a*b"));
    check(str2.match("a*b"));
    check(str3.match("a*b"));
}

TEST_CASE("String match zero or more empty") {
    StringView str = "b";
    check(str.match("a*b"));
}

TEST_CASE("String match optional character") {
    StringView str1 = "color";
    StringView str2 = "colour";
    check(str1.match("colou?r"));
    check(str2.match("colou?r"));
}

TEST_CASE("String match group zero or more") {
    StringView str1 = "start end";
    StringView str2 = "start foo end";
    StringView str3 = "start foo foo foo end";
    check(str1.match("start (foo )*end"));
    check(str2.match("start (foo )*end"));
    check(str3.match("start (foo )*end"));
}

TEST_CASE("String match nested groups") {
    check(StringView{""}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple,banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana, apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(!StringView{"orange"}.match("((apple|banana)(, *)?)*$"));
}

TEST_CASE("String match end anchor") {
    StringView str = "hello";
    check(str.match("hello$"));
    check(!str.match("hell$"));
}

TEST_CASE("String match multiple captures") {
    StringView str = "point(10, 20)";
    s32 x = 0, y = 0;
    check(str.match("point'(%d, %d')", &x, &y));
    check(x == 10);
    check(y == 20);
}

TEST_CASE("String match optional format specifier") {
    s32 value = -1;
    check(StringView{"item"}.match("item%d?$", &value));
    check(value == -1); // Unchanged since no number present
    check(StringView{"item42"}.match("item%d?$", &value));
    check(value == 42);
    check(!StringView{"item42extra"}.match("item%d?$", &value));
}

TEST_CASE("String match optional group with format specifier") {
    s32 num = -1;
    String text;
    // Test optional group with alternation containing format specifiers
    check(StringView{"start end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == -1); // Unchanged
    check(text.isEmpty());
    num = -1;
    check(StringView{"start num=42 end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == 42);
    num = -1;
    text = {};
    check(StringView{"start text=\"hello\" end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(text == "hello");
}

TEST_CASE("String match literal characters") {
    StringView str = "abc123xyz";
    check(str.match("abc123xyz"));
    check(!str.match("abc456xyz"));
}

TEST_CASE("String::format") {
    // Escaped braces.
    check(String::format("{} {} {{}}", "hello", 7) == "hello 7 {}");

    // XML escaping.
    check(String::format("{:&}", "<tag attr=\"x\">") == "&lt;tag attr=&quot;x&quot;&gt;");
    check(String::format("{:.4&}", "<abcd>") == "&lt;abc");

    // Width, fill and alignment.
    check(String::format("{:>6}", "abc") == "   abc");
    check(String::format("{:<6}", 42) == "42    ");
    check(String::format("{:_^7}", "abc") == "__abc__");
    check(String::format("{:06}", -42) == "-00042");

    // Sign and integer type specifiers.
    check(String::format("{:+d} {:+d} {: d}", 42, -42, 42) == "+42 -42  42");
    check(String::format("{:x} {:X} {:o} {:b}", 255, 255, 255, 5) == "ff FF 377 101");

    // Precision and floating-point type specifiers.
    check(String::format("{:.4s}", "abcdef") == "abcd");
    check(String::format("{:.2f} {:.2e}", 12.345, 12.345) == "12.35 1.23e+01");
    check(String::format("{:+.1F} {: .0E}", 12.34, 12.34) == "+12.3  1E+01");
}

TEST_CASE("printNumber") {
    {
        MemStream mem;

        // Signed integers can be printed directly through the public overload.
        printNumber(mem, -1);
        mem.write(' ');
        printNumber(mem, (s64) 1, [] {
            NumberFormat format;
            format.signMode = NumberFormat::ShowPlus;
            return format;
        }());
        mem.write(' ');
        printNumber(mem, (s64) -1, [] {
            NumberFormat format;
            format.signMode = NumberFormat::ShowPlus;
            return format;
        }());
        mem.write(' ');
        printNumber(mem, (u64) 1, [] {
            NumberFormat format;
            format.signMode = NumberFormat::ShowPlus;
            return format;
        }());
        mem.write(' ');
        printNumber(mem, (u64) 1, [] {
            NumberFormat format;
            format.signMode = NumberFormat::LeaveSpaceForSign;
            return format;
        }());
        check(mem.moveToString() == "-1 +1 -1 +1  1");
    }
    {
        MemStream mem;

        // Auto float mode switches to scientific notation before fixed output would round to zero.
        printNumber(mem, 0.0004);
        mem.write(' ');
        printNumber(mem, 0.0005);
        mem.write(' ');
        printNumber(mem, 1e20);
        check(mem.moveToString() == "4.000e-04 0.001 1.000e+20");
    }
}

//   ▄▄▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄
//  ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██
//  ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██
//                               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Array_

//--------------------------------
// Constructors
//--------------------------------
TEST_CASE("Array default constructor") {
    Array<u32> a;
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array construct from braced initializer list") {
    Array<u32> a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Copy construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array construct from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = std::move(a);
    check(a.isEmpty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

//--------------------------------
// Assignment Operators
//--------------------------------
TEST_CASE("Array assign from braced initializer list") {
    Array<u32> a;
    a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array assign, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array assign from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move assign Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a.isEmpty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array move assign from FixedArray") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array self-assignment") {
    Array<u32> a = {1, 1, 2, 3, 5, 8};
    a = a.subview(1);
    check(a == ArrayView<const u32>{1, 2, 3, 5, 8});
}

//--------------------------------
// Element Access
//--------------------------------
TEST_CASE("Array subscript lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a[0] == 4);
    check(a[1] == 5);
    check(a[2] == 6);
}

TEST_CASE("Array subscript modification") {
    Array<u32> a = {4, 5, 6};
    a[1] = 7;
    check(a == ArrayView<const u32>{4, 7, 6});
}

TEST_CASE("Array back lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a.back() == 6);
    check(a.back(-2) == 5);
}

TEST_CASE("Array back modification") {
    Array<u32> a = {4, 5, 6};
    a.back() = 7;
    check(a == ArrayView<const u32>{4, 5, 7});
}

TEST_CASE("Array iteration") {
    Array<u32> a = {4, 5, 6};
    u32 prev = 3;
    for (u32 i : a) {
        check(i == prev + 1);
        prev = i;
    }
}

TEST_CASE("Array iteration 2") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    for (u32 i : a) {
        b.append(i);
    }
    check(b == ArrayView<const u32>{4, 5, 6});
}

//--------------------------------
// Capacity
//--------------------------------
TEST_CASE("Array operator bool") {
    Array<u32> a;
    check(!(bool) a);
    a = {4, 5, 6};
    check((bool) a);
}

TEST_CASE("Array isEmpty") {
    Array<u32> a;
    check(a.isEmpty());
    a = {4, 5, 6};
    check(!a.isEmpty());
}

TEST_CASE("Array numItems") {
    Array<u32> a;
    check(a.numItems() == 0);
    a = {4, 5, 6};
    check(a.numItems() == 3);
}

//--------------------------------
// Modifers
//--------------------------------
TEST_CASE("Array clear") {
    Array<u32> a = {4, 5, 6};
    a.clear();
    check(a == ArrayView<const u32>{});
}

// FIXME: Add reserve() test?
// Ideally it would measure the number of allocations performed under the hood.

TEST_CASE("Array resize") {
    Array<u32> a;
    a.resize(3);
    check(a.numItems() == 3);
}

TEST_CASE("Array resize 2") {
    Array<String> a;
    a.resize(3);
    check(a == ArrayView<const StringView>{{}, {}, {}});
}

TEST_CASE("Array append, no move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(s0);
    a.append(s1);
    check(s0 == "hello");
    check(s1 == "there");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append with move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(std::move(s0));
    a.append(std::move(s1));
    check(s0 == "");
    check(s1 == "");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append, no move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(s);
    check(s == "hello");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array append String with move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(std::move(s));
    check(s == "");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array extend from braced initializer list") {
    Array<String> a;
    a += {"hello", "there"};
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray, no move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray with move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> without move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array pop") {
    Array<u32> a = {4, 5, 6};
    a.pop();
    check(a == ArrayView<const u32>{4, 5});
    a.pop(2);
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array insert") {
    Array<u32> a = {4, 5, 6};
    a.insert(2) = 7;
    check(a == ArrayView<const u32>{4, 5, 7, 6});
}

TEST_CASE("Array insert 2") {
    Array<String> a = {"hello", "there"};
    a.insert(1, 2);
    check(a == ArrayView<const StringView>{"hello", {}, {}, "there"});
}

TEST_CASE("Array erase") {
    Array<u32> a = {4, 5, 6};
    a.erase(0);
    check(a == ArrayView<const u32>{5, 6});

    Array<u32> b = {4, 5, 6, 7};
    b.erase(1, 2);
    check(b == ArrayView<const u32>{4, 7});
}

TEST_CASE("Array erase_quick") {
    Array<u32> a = {4, 5, 6};
    a.eraseQuick(0);
    check(a == ArrayView<const u32>{6, 5});

    Array<u32> b = {4, 5, 6, 7, 8, 9, 10};
    b.eraseQuick(1, 2);
    check(b == ArrayView<const u32>{4, 9, 10, 7, 8});
}

//   ▄▄▄▄          ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄
//   ▀▀▀█▄ ██▄▄██  ██
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Set_

struct TestHistogramBucket {
    u32 population = 0;
    u32 numTimesOccurred = 0;
};

TEST_CASE("Set stress test u32") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {{0, 0},  {1, 0},  {2, 0},  {4, 0},   {8, 0},
                                            {16, 0}, {32, 0}, {64, 0}, {128, 0}, {256, 0}};

    // Test setup.
    Set<u32> set;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 2500; iters++) {
        // Ensure the set and mirror array have the same number of items.
        PLY_ASSERT(set.items().numItems() == arr.numItems());

        // Decide what population size the set should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generateFloat();
        PLY_ASSERT(exp > 0);                          // Guaranteed because generateFloat returns numbers < 1.
        float randomPopulation = -ply::log(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desiredPopulation = (u32) clamp(randomPopulation - 4.f, 0.f, 512.f);

        // Add items to the set if needed.
        while (desiredPopulation > set.items().numItems()) {
            u32 valueToInsert = r.generateU32() % 1000;
            if (set.insert(valueToInsert).wasFound) {
                check(find(arr, valueToInsert) >= 0);
            } else {
                arr.append(valueToInsert);
            }
        }

        // Remove items from the set if needed.
        while (desiredPopulation < arr.numItems()) {
            u32 indexToRemove = r.generateU32() % arr.numItems();
            u32 valueToRemove = arr[indexToRemove];
            bool wasFound = set.erase(valueToRemove);
            check(wasFound);
            arr.eraseQuick(indexToRemove);
        }

        // Check its population.
        check(desiredPopulation == set.items().numItems());
        check(desiredPopulation == arr.numItems());
        for (s32 i = histogram.numItems() - 1; i >= 0; i--) {
            if (desiredPopulation >= histogram[i].population) {
                histogram[i].numTimesOccurred++;
                break;
            }
        }

        // Test find.
        sort(arr);
        for (u32 i = 0; i < arr.numItems(); i++) {
            check(set.find(arr[i]));
            if (i > 0) {
                check(arr[i] > arr[i - 1]); // No duplicates.
                u32 delta = arr[i] - arr[i - 1];
                if (delta > 1) {
                    u32 absentKey = arr[i - 1] + 1 + (r.generateU32() % (delta - 1));
                    check(!set.find(absentKey));
                }
            }
        }
    }
}

//  ▄▄   ▄▄
//  ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██▀█▀██  ▄▄▄██ ██  ██
//  ██   ██ ▀█▄▄██ ██▄▄█▀
//                 ██

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Map_

TEST_CASE("Map with String keys") {
    Map<String, u32> map;

    auto result1 = map.insert("apple");
    check(!result1.wasFound);
    *result1.value = 1;

    auto result2 = map.insert("banana");
    check(!result2.wasFound);
    *result2.value = 2;

    auto result3 = map.insert("cherry");
    check(!result3.wasFound);
    *result3.value = 3;

    // Find by string
    check(map.find("apple") != nullptr);
    check(*map.find("apple") == 1);
    check(map.find("banana") != nullptr);
    check(*map.find("banana") == 2);
    check(map.find("cherry") != nullptr);
    check(*map.find("cherry") == 3);

    // Find non-existing
    check(map.find("durian") == nullptr);
}

TEST_CASE("Map stress test") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {
        {0, 0}, {1, 0}, {2, 0}, {4, 0}, {8, 0}, {16, 0}, {32, 0}, {64, 0}, {128, 0}, {256, 0},
    };

    // Test setup.
    Map<u32, String> map;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 500; iters++) {
        // Ensure the map and mirror array have the same number of items.
        PLY_ASSERT(map.items().numItems() == arr.numItems());

        // Decide what population size the map should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generateFloat();
        PLY_ASSERT(exp > 0);                          // Guaranteed because generateFloat returns numbers < 1.
        float randomPopulation = -ply::log(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desiredPopulation = (u32) clamp(randomPopulation - 4.f, 0.f, 512.f);

        // Add items to the map if needed.
        while (desiredPopulation > map.items().numItems()) {
            u32 keyToInsert = r.generateU32() % 1000;
            auto result = map.insert(keyToInsert);
            if (result.wasFound) {
                check(find(arr, keyToInsert) >= 0);
            } else {
                *result.value = String::format("{}", keyToInsert);
                arr.append(keyToInsert);
            }
        }

        // Remove items from the map if needed.
        while (desiredPopulation < arr.numItems()) {
            u32 indexToRemove = r.generateU32() % arr.numItems();
            u32 keyToRemove = arr[indexToRemove];
            map.erase(keyToRemove);
            arr.eraseQuick(indexToRemove);
        }

        // Check its population.
        check(desiredPopulation == map.items().numItems());
        check(desiredPopulation == arr.numItems());
        for (s32 i = histogram.numItems() - 1; i >= 0; i--) {
            if (desiredPopulation >= histogram[i].population) {
                histogram[i].numTimesOccurred++;
                break;
            }
        }

        // Test find.
        sort(arr);
        for (u32 i = 0; i < arr.numItems(); i++) {
            String* found = map.find(arr[i]);
            check(found);
            check(*found == String::format("{}", arr[i]));
            if (i > 0) {
                check(arr[i] > arr[i - 1]); // No duplicates.
                u32 delta = arr[i] - arr[i - 1];
                if (delta > 1) {
                    u32 absentKey = arr[i - 1] + 1 + (r.generateU32() % (delta - 1));
                    check(!map.find(absentKey));
                }
            }
        }
    }
}

//  ▄▄   ▄▄               ▄▄                ▄▄
//  ██   ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//   ██ ██   ▄▄▄██ ██  ▀▀ ██  ▄▄▄██ ██  ██  ██
//    ▀█▀   ▀█▄▄██ ██     ██ ▀█▄▄██ ██  ██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Variant_

struct FruitBowl {
    struct Apple {
        String type;
    };
    struct Banana {
        bool peeled = false;
    };
    struct Cherry {
        u32 numOnStem = 1;
    };
    struct Date {};

    Variant<Apple, Banana, Cherry, Date> fruit;
};

TEST_CASE("Variant template") {
    FruitBowl bowl;
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());

    bowl.fruit = FruitBowl::Apple{"Cortland"};
    check(bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());
    check(bowl.fruit.as<FruitBowl::Apple>()->type == "Cortland");

    bowl.fruit = FruitBowl::Date{};
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(bowl.fruit.is<FruitBowl::Date>());

    FruitBowl bowl2{FruitBowl::Banana{}};
    check(bowl2.fruit.is<FruitBowl::Banana>());
    check(!bowl2.fruit.as<FruitBowl::Banana>()->peeled);

    FruitBowl bowl3 = {FruitBowl::Cherry{}};
    check(bowl3.fruit.is<FruitBowl::Cherry>());
    check(bowl3.fruit.as<FruitBowl::Cherry>()->numOnStem == 1);
}

//   ▄▄▄▄  ▄▄▄                       ▄▄  ▄▄   ▄▄
//  ██  ██  ██   ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄██▄▄ ██▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄
//  ██▀▀██  ██  ██  ██ ██  ██ ██  ▀▀ ██  ██   ██  ██ ██ ██ ██ ▀█▄▄▄
//  ██  ██ ▄██▄ ▀█▄▄██ ▀█▄▄█▀ ██     ██  ▀█▄▄ ██  ██ ██ ██ ██  ▄▄▄█▀
//               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Algorithm_

TEST_CASE("binarySearch() basic functionality") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 5, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, 7, FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, 1, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 15, FindType::GreaterThanOrEqual) == 7);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 4, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, 6, FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, 0, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 20, FindType::GreaterThanOrEqual) == 8);
}

TEST_CASE("binarySearch() with FindType::GreaterThan condition") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, 5, FindType::GreaterThan) == 3);
    check(binarySearch(arr, 7, FindType::GreaterThan) == 4);
    check(binarySearch(arr, 1, FindType::GreaterThan) == 1);
    check(binarySearch(arr, 15, FindType::GreaterThan) == 8);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, 4, FindType::GreaterThan) == 2);
    check(binarySearch(arr, 6, FindType::GreaterThan) == 3);
    check(binarySearch(arr, 0, FindType::GreaterThan) == 0);
    check(binarySearch(arr, 20, FindType::GreaterThan) == 8);
}

TEST_CASE("binarySearch() empty array") {
    Array<u32> emptyArr;

    // Empty array should always return 0 for any search
    check(binarySearch(emptyArr, 5, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(emptyArr, 5, FindType::GreaterThan) == 0);
    check(binarySearch(emptyArr, 0, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(emptyArr, 100, FindType::GreaterThan) == 0);
}

TEST_CASE("binarySearch() single element") {
    Array<u32> singleArr = {42};

    // Test with single element array
    check(binarySearch(singleArr, 42, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(singleArr, 42, FindType::GreaterThan) == 1);
    check(binarySearch(singleArr, 40, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(singleArr, 40, FindType::GreaterThan) == 0);
    check(binarySearch(singleArr, 50, FindType::GreaterThanOrEqual) == 1);
    check(binarySearch(singleArr, 50, FindType::GreaterThan) == 1);
}

TEST_CASE("binarySearch() with duplicates") {
    Array<u32> arr = {1, 3, 3, 3, 5, 7, 7, 9};

    // Test finding duplicates with Find_Greater_Than_Or_Equal (should find first occurrence)
    check(binarySearch(arr, 3, FindType::GreaterThanOrEqual) == 1);
    check(binarySearch(arr, 7, FindType::GreaterThanOrEqual) == 5);

    // Test finding duplicates with Find_Greater_Than (should find first element after duplicates)
    check(binarySearch(arr, 3, FindType::GreaterThan) == 4);
    check(binarySearch(arr, 7, FindType::GreaterThan) == 7);

    // Test finding elements between duplicates
    check(binarySearch(arr, 4, FindType::GreaterThanOrEqual) == 4);
    check(binarySearch(arr, 4, FindType::GreaterThan) == 4);
    check(binarySearch(arr, 6, FindType::GreaterThanOrEqual) == 5);
    check(binarySearch(arr, 6, FindType::GreaterThan) == 5);
}

TEST_CASE("binarySearch() all same elements") {
    Array<u32> arr = {5, 5, 5, 5, 5};

    // Test with all same elements
    check(binarySearch(arr, 5, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 5, FindType::GreaterThan) == 5);
    check(binarySearch(arr, 3, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 3, FindType::GreaterThan) == 0);
    check(binarySearch(arr, 7, FindType::GreaterThanOrEqual) == 5);
    check(binarySearch(arr, 7, FindType::GreaterThan) == 5);
}

TEST_CASE("binarySearch() with custom type") {
    struct TestItem {
        u32 value;
        String name;

        u32 getLookupKey() const {
            return value;
        }
    };

    Array<TestItem> arr = {{10, "ten"}, {20, "twenty"}, {30, "thirty"}, {40, "forty"}, {50, "fifty"}};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 30, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, 40, FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, 10, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 50, FindType::GreaterThanOrEqual) == 4);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, 25, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, 35, FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, 5, FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, 60, FindType::GreaterThanOrEqual) == 5);

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, 30, FindType::GreaterThan) == 3);
    check(binarySearch(arr, 40, FindType::GreaterThan) == 4);
    check(binarySearch(arr, 10, FindType::GreaterThan) == 1);
    check(binarySearch(arr, 50, FindType::GreaterThan) == 5);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, 25, FindType::GreaterThan) == 2);
    check(binarySearch(arr, 35, FindType::GreaterThan) == 3);
    check(binarySearch(arr, 5, FindType::GreaterThan) == 0);
    check(binarySearch(arr, 60, FindType::GreaterThan) == 5);
}

TEST_CASE("binarySearch() with String type") {
    Array<String> arr = {"apple", "banana", "cherry", "date", "elderberry"};

    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, "cherry", FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, "date", FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, "apple", FindType::GreaterThanOrEqual) == 0);
    check(binarySearch(arr, "elderberry", FindType::GreaterThanOrEqual) == 4);

    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binarySearch(arr, "blueberry", FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(arr, "coconut", FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(arr, "apricot", FindType::GreaterThanOrEqual) == 1);
    check(binarySearch(arr, "fig", FindType::GreaterThanOrEqual) == 5);

    // Test finding existing elements with Find_Greater_Than
    check(binarySearch(arr, "cherry", FindType::GreaterThan) == 3);
    check(binarySearch(arr, "date", FindType::GreaterThan) == 4);
    check(binarySearch(arr, "apple", FindType::GreaterThan) == 1);
    check(binarySearch(arr, "elderberry", FindType::GreaterThan) == 5);

    // Test finding non-existing elements with Find_Greater_Than
    check(binarySearch(arr, "blueberry", FindType::GreaterThan) == 2);
    check(binarySearch(arr, "coconut", FindType::GreaterThan) == 3);
    check(binarySearch(arr, "apricot", FindType::GreaterThan) == 1);
    check(binarySearch(arr, "fig", FindType::GreaterThan) == 5);
}

TEST_CASE("binarySearch() with different numeric types") {
    // Test with float array
    Array<float> floatArr = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    check(binarySearch(floatArr, 3.3f, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(floatArr, 3.0f, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(floatArr, 3.3f, FindType::GreaterThan) == 3);
    check(binarySearch(floatArr, 6.0f, FindType::GreaterThanOrEqual) == 5);

    // Test with double array
    Array<double> doubleArr = {1.1, 2.2, 3.3, 4.4, 5.5};
    check(binarySearch(doubleArr, 3.3, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(doubleArr, 3.0, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(doubleArr, 3.3, FindType::GreaterThan) == 3);
    check(binarySearch(doubleArr, 6.0, FindType::GreaterThanOrEqual) == 5);

    // Test with s32 array
    Array<s32> s32_arr = {-5, -3, -1, 1, 3, 5};
    check(binarySearch(s32_arr, -1, FindType::GreaterThanOrEqual) == 2);
    check(binarySearch(s32_arr, 0, FindType::GreaterThanOrEqual) == 3);
    check(binarySearch(s32_arr, -1, FindType::GreaterThan) == 3);
    check(binarySearch(s32_arr, 10, FindType::GreaterThanOrEqual) == 6);
}

//  ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Unicode_

// MockPipe is an in-memory implementation of Pipe used to test Stream behavior without OS pipes.
struct MockPipe : Pipe {
    String bytes;
    u32 pos = 0;
    u32 limit = 0xffffffff;
    u32 writeLimit = 0;
    IOResult::Code code = IOResult::OK;
    u32 readCalls = 0;
    u32 writeCalls = 0;
    u32 seekCalls = 0;
    u32 flushCalls = 0;
    explicit MockPipe(StringView input = {}) : bytes{input} {
        this->capabilities = CanRead | CanWrite | CanSeek;
    }
    IOResult read(MutStringView dst) override {
        this->readCalls++;
        if (this->code != IOResult::OK)
            return {this->code, 0};
        u32 count =
            min(this->limit, min(dst.numBytes, this->bytes.numBytes() - min(this->pos, this->bytes.numBytes())));
        if (count) {
            memcpy(dst.bytes, this->bytes.bytes() + this->pos, count);
        }
        this->pos += count;
        return {count ? IOResult::OK : IOResult::EndOfFile, count};
    }
    IOResult tryRead(MutStringView dst) override {
        return this->read(dst);
    }
    IOResult write(StringView src) override {
        this->writeCalls++;
        u32 count = this->code == IOResult::OK ? src.numBytes() : min(this->writeLimit, src.numBytes());
        this->bytes.resize(max(this->bytes.numBytes(), this->pos + count));
        if (count) {
            memcpy(this->bytes.bytes() + this->pos, src.bytes(), count);
        }
        this->pos += count;
        return {this->code, count};
    }
    void seekTo(s64 offset) override {
        this->seekCalls++;
        this->pos = (u32) offset;
    }
    void flush(bool) override {
        this->flushCalls++;
    }
};

TEST_CASE("Encode Unicode codepoints") {
    // Verify the direct single-byte path and every variable-length UTF-8 form.
    FixedArray<char, 4> encoded;
    check(encodeUnicode(encoded, UnicodeType::None, 'A') == 1);
    check(encoded[0] == 'A');
    check(encodeUnicode(encoded, UnicodeType::UTF8, 0xa2) == 2);
    check(StringView{encoded.items(), 2} == StringView{"\xc2\xa2", 2});
    check(encodeUnicode(encoded, UnicodeType::UTF8, 0x3002) == 3);
    check(StringView{encoded.items(), 3} == StringView{"\xe3\x80\x82", 3});
    check(encodeUnicode(encoded, UnicodeType::UTF8, 0x1f642) == 4);
    check(StringView{encoded.items(), 4} == StringView{"\xf0\x9f\x99\x82", 4});
}

// Unicode adapters: fragmentation, output capacity and incomplete terminal input.
TEST_CASE("Unicode output conversion preserves every UTF-8 split") {
    StringView input = "PATH\xe3\x80\x82xyz\xf0\x9f\x99\x82";
    StringView expected{"P\0A\0T\0H\0\x02\x30x\0y\0z\0\x3d\xd8\x42\xde", 20};
    for (u32 split = 0; split <= input.numBytes(); split++) {
        OutPipeConvertUnicode conv{MemStream{}, UnicodeType::UTF16LE};
        IOResult first = conv.write(input.left(split));
        IOResult second = conv.write(input.substr(split));
        check(first.code == IOResult::OK && first.numBytes == split);
        check(second.code == IOResult::OK && second.numBytes == input.numBytes() - split);
        conv.flush(false);
        check(static_cast<MemStream&>(conv.childOut).moveToString() == expected);
    }
}

TEST_CASE("Decode truncated UTF-8") {
    // e3 80 82 is the valid UTF-8 encoding of U+3002
    // e3 80 is the truncated version of it
    // As such, it should be decoded as two 8-bit characters
    OutPipeConvertUnicode conv{MemStream{}, UnicodeType::UTF16LE};
    conv.write("\xe3\x80");
    conv.flush(false);
    String result = static_cast<MemStream&>(conv.childOut).moveToString();
    check(result == StringView{"\xe3\x00\x80\x00", 4});
}

TEST_CASE("Unicode input conversion handles input and output boundaries") {
    struct Example {
        UnicodeType type;
        StringView input;
        StringView expected;
    };
    Example examples[] = {
        {UnicodeType::UTF8, "", ""},
        {UnicodeType::UTF8, "ASCII text", "ASCII text"},
        {UnicodeType::UTF8, "\xe3\x80\x82X\xc2\xa2", "\xe3\x80\x82X\xc2\xa2"},
        {UnicodeType::UTF8, "\xf0\x9f\x99\x82Z", "\xf0\x9f\x99\x82Z"},
        {UnicodeType::UTF8, "\xe3XY", "\xc3\xa3XY"},
        {UnicodeType::UTF8, "\xe3\x80", "\xc3\xa3\xc2\x80"},
        {UnicodeType::UTF16LE, StringView{"A\0\x3d\xd8\x42\xdeZ\0", 8}, "A\xf0\x9f\x99\x82Z"},
        {UnicodeType::UTF16BE, StringView{"\0A\xd8\x3d\xde\x42\0Z", 8}, "A\xf0\x9f\x99\x82Z"},
        {UnicodeType::UTF16LE, StringView{"\x3d\xd8Z\0", 4}, "\xed\xa0\xbdZ"},
        {UnicodeType::UTF16LE, StringView{"A\0Z", 3}, "A"},
        {UnicodeType::None, "A\xffZ", "A\xc3\xbfZ"},
    };
    for (const Example& example : examples) {
        for (bool blocking : {false, true}) {
            for (u32 chunkSize : {1u, 2u, 3u, 4u, 16u}) {
                for (u32 outputSize : {1u, 2u, 3u, 4u, 16u}) {
                    MockPipe pipe{example.input};
                    pipe.limit = chunkSize;
                    InPipeConvertUnicode conv{Stream{&pipe, false}, example.type};
                    MemStream output;
                    char bytes[16];
                    for (u32 attempt = 0; attempt < 32; attempt++) {
                        IOResult result = blocking ? conv.read({bytes, outputSize}) : conv.tryRead({bytes, outputSize});
                        if (result.code != IOResult::OK) {
                            check(result.code == IOResult::EndOfFile && result.numBytes == 0);
                            break;
                        }
                        check(result.numBytes > 0 && result.numBytes <= outputSize);
                        output.write({bytes, result.numBytes});
                        check(attempt < 31);
                    }
                    check(output.copyToString() == example.expected);
                }
            }
        }
    }
}

TEST_CASE("Nonblocking Unicode conversion handles buffered truncated input at EOF") {
    MemFile input;
    input.write("\xe3\x80");
    input.seekTo(0);
    check(!input.makeReadable(3));
    check(!input.atEof());
    InPipeConvertUnicode conv{std::move(input), UnicodeType::UTF8};
    char dst[8];
    IOResult result = conv.tryRead({dst, sizeof(dst)});
    check(result.code == IOResult::OK);
    check(StringView{dst, result.numBytes} == StringView{"\xc3\xa3\xc2\x80", 4});
    check(conv.tryRead({dst, sizeof(dst)}).code == IOResult::EndOfFile);
}

#if defined(PLY_POSIX)
TEST_CASE("Nonblocking Unicode conversion preserves fragmented input") {
    int fds[2];
    check(pipe(fds) == 0);
    InPipeConvertUnicode conv{Stream{Heap::create<PipeFD>(fds[0], Pipe::CanRead), true}, UnicodeType::UTF8};
    char dst[8];

    IOResult result = conv.tryRead({dst, sizeof(dst)});
    check(result.code == IOResult::WouldBlock);
    check(::write(fds[1], "\xe3", 1) == 1);
    result = conv.tryRead({dst, sizeof(dst)});
    check(result.code == IOResult::WouldBlock);
    check(::write(fds[1], "\x80\x82", 2) == 2);
    result = conv.tryRead({dst, sizeof(dst)});
    check(result.code == IOResult::OK);
    check(result.numBytes == 3);
    check(StringView{dst, result.numBytes} == StringView{"\xe3\x80\x82", 3});

    check(::close(fds[1]) == 0);
    result = conv.tryRead({dst, sizeof(dst)});
    check(result.code == IOResult::EndOfFile);
}

#endif

//  ▄▄▄▄▄  ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀▀  ██ ██  ██ ██▄▄██
//  ██     ██ ██▄▄█▀ ▀█▄▄▄
//            ██

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Pipe_

#if defined(PLY_WINDOWS)

TEST_CASE("blocking read drains data before reporting EOF") {
    // Closing the writer must preserve queued bytes and then report EOF on every subsequent read.
    HANDLE readHandle;
    HANDLE writeHandle;
    BOOL created = CreatePipe(&readHandle, &writeHandle, nullptr, 0);
    check(created != 0);
    if (!created)
        return;
    PipeHandle reader{readHandle, Pipe::CanRead};
    {
        PipeHandle writer{writeHandle, Pipe::CanWrite};
        IOResult result = writer.write("abc");
        check(result.code == IOResult::OK && result.numBytes == 3);
    }
    char bytes[8];
    IOResult result = reader.read({bytes, sizeof(bytes)});
    check(result.code == IOResult::OK && result.numBytes == 3);
    check(StringView{bytes, result.numBytes} == "abc");
    result = reader.read({bytes, sizeof(bytes)});
    check(result.code == IOResult::EndOfFile && result.numBytes == 0);
    check(reader.read({bytes, sizeof(bytes)}).code == IOResult::EndOfFile);
}

#endif

#if defined(PLY_POSIX)

TEST_CASE("tryRead reports data, would-block and EOF") {
    int fds[2];
    check(pipe(fds) == 0);
    PipeFD reader{fds[0], Pipe::CanRead};
    char bytes[8];

    IOResult result = reader.tryRead({bytes, sizeof(bytes)});
    check(result.code == IOResult::WouldBlock);
    check(::write(fds[1], "abc", 3) == 3);
    result = reader.tryRead({bytes, sizeof(bytes)});
    check(result.code == IOResult::OK);
    check(result.numBytes == 3);
    check(StringView{bytes, result.numBytes} == "abc");
    check(reader.tryRead({bytes, sizeof(bytes)}).code == IOResult::WouldBlock);

    check(::close(fds[1]) == 0);
    check(reader.tryRead({bytes, sizeof(bytes)}).code == IOResult::EndOfFile);
}

TEST_CASE("disable releases a blocking read and is terminal") {
    int fds[2];
    check(pipe(fds) == 0);
    PipeFD reader{fds[0], Pipe::CanRead};
    Atomic<bool> started = false;
    IOResult readResult{IOResult::OK, 1};
    Thread thread{[&]() {
        started.store(true, MemoryOrder::Release);
        char byte;
        readResult = reader.read({&byte, 1});
    }};
    while (!started.load(MemoryOrder::Acquire)) {
    }
    sleepMillis(20);
    reader.disable();
    reader.disable();
    thread.join();

    char byte;
    check(readResult.code == IOResult::Disabled);
    check(reader.read({&byte, 1}).code == IOResult::Disabled);
    check(reader.tryRead({&byte, 1}).code == IOResult::Disabled);
    Stream in{&reader, false};
    check(!in.makeReadable());
    check(in.getStatus() == IOResult::Disabled);
    check(in.atEof());
    check(::close(fds[1]) == 0);
}

TEST_CASE("disable releases a blocking write") {
    int fds[2];
    check(pipe(fds) == 0);
    PipeFD writer{fds[1], Pipe::CanWrite};
    String payload = String::allocate(4 * 1024 * 1024);
    memset(payload.bytes(), 'x', payload.numBytes());
    Atomic<bool> started = false;
    IOResult writeStatus{IOResult::OK, 0};
    Thread thread{[&]() {
        started.store(true, MemoryOrder::Release);
        writeStatus = writer.write(payload);
    }};
    while (!started.load(MemoryOrder::Acquire)) {
    }
    sleepMillis(20);
    writer.disable();
    thread.join();

    check(writeStatus.code == IOResult::Disabled);
    check(writer.write("x").code == IOResult::Disabled);
    check(::close(fds[0]) == 0);
}

#endif

//  ▄▄▄▄▄▄                ▄▄   ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄ ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██   ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Text_Format_

struct ExtractedFormat {
    bool isValid = false;
    TextFormat format;
};

ExtractedFormat extractFormatFromName(StringView name) {
    TextFormat tf;

    Array<StringView> components = name.split(".");
    if (components.numItems() != 4)
        return {false, {}};

    if (components[1] == "utf8") {
        tf.unicodeType = UnicodeType::UTF8;
    } else if (components[1] == "utf16le") {
        tf.unicodeType = UnicodeType::UTF16LE;
    } else if (components[1] == "utf16be") {
        tf.unicodeType = UnicodeType::UTF16BE;
    } else if (components[1] == "win1252") {
        tf.unicodeType = UnicodeType::None;
    } else {
        return {false, {}};
    }

    if (components[2] == "lf") {
        tf.newLine = TextFormat::LF;
    } else if (components[2] == "crlf") {
        tf.newLine = TextFormat::CRLF;
    } else {
        return {false, {}};
    }

    if (components[3] == "bom") {
        tf.bom = true;
    } else if (components[3] == "nobom") {
        tf.bom = false;
    } else {
        return {false, {}};
    }

    return {true, tf};
}

// Loads each Unicode fixture as an independently counted test.
TestResult runUnicodeLoadingTests() {
    String testsFolder = joinPath(TEST_SUITE_PATH, "unicode-test-files");
    Array<DirectoryEntry> entries = FileSystem::listDir(testsFolder);
    u32 numFiles = 0;
    for (const DirectoryEntry& entry : entries) {
        if (!entry.isDir && entry.name.endsWith(".txt")) {
            numFiles++;
        }
    }

    TestResult result;
    u32 entryCount = 0;
    for (const DirectoryEntry& entry : entries) {
        if (!entry.isDir && entry.name.endsWith(".txt")) {
            Heap::Stats beginStats = Heap::getStats();
            bool success = true;
            {
                ExtractedFormat expectedFormat = extractFormatFromName(entry.name.shortenedBy(4));
                success = expectedFormat.isValid;

                TextFormat detectedFormat;
                String contents = FileSystem::loadTextAutodetect(joinPath(testsFolder, entry.name), &detectedFormat);
                success &= detectedFormat.unicodeType == expectedFormat.format.unicodeType;
                success &= detectedFormat.newLine == expectedFormat.format.newLine;
                success &= detectedFormat.bom == expectedFormat.format.bom;

                auto compareTo =
                    FileSystem::loadBinary(joinPath(testsFolder, entry.name.split(".")[0] + ".utf8.lf.nobom.txt"));
                success &= contents == compareTo;
            }
            success &= beginStats.totalBytesConsumed == Heap::getStats().totalBytesConsumed;
            entryCount++;
            result.add(success);
            if (options.verbose) {
                getStdOut().format("[{}/{}] {}... {}\n", entryCount, numFiles, entry.name,
                                   success ? "success" : "***FAIL***");
            } else if (!success) {
                getStdOut().format("***FAIL*** [{}/{}] {}\n", entryCount, numFiles, entry.name);
            }
        }
    }
    if (entryCount != 50) {
        getStdOut().format("***FAIL*** Expected 50 Unicode loading fixtures, found {}\n", entryCount);
        result.add(false);
    }
    return result;
}

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Stream_

String streamPayload(u32 size) {
    String result = String::allocate(size);
    for (u32 i = 0; i < size; i++) {
        result[i] = (char) shuffleBits(i);
    }
    return result;
}

// Common contracts: the same read sequence runs against every readable backend.
static void checkStreamReads(Stream& in, StringView expected, u32 method) {
    check(in.isOpen() && (in.getCapabilities() & Stream::CanRead));
    u32 pos = 0;
    while (pos < expected.numBytes()) {
        check(in.peekByte() == expected[pos]);
        check(in.getSeekPos() == pos); // Peeking never consumes data.
        u32 count = min(37u, expected.numBytes() - pos);
        char dst[37];
        if (method == 0) {
            check(in.readByte() == expected[pos]);
            count = 1;
        } else if (method == 1) {
            check(in.read({dst, count}) == count);
            check(StringView{dst, count} == expected.substr(pos, count));
        } else if (method == 2) {
            count = in.tryRead({dst, count});
            if (!check(count > 0))
                return;
            check(StringView{dst, count} == expected.substr(pos, count));
        } else {
            if (!check(in.makeReadable(count)))
                return;
            check(in.viewRemainingBytes().left(count) == expected.substr(pos, count));
            check(in.skip(count) == count);
        }
        pos += count;
        check(in.getMode() == Stream::Mode::Reading && in.getSeekPos() == pos);
    }
    check(in.peekByte() == 0 && in.atEof());
    check(in.readByte() == 0 && in.skip(1) == 0);
    char tail[2] = {'-', '-'};
    check(in.tryRead({tail, 2}) == 0 && StringView{tail, 2} == "--");
    check(in.read({tail, 2}) == 0 && StringView{tail, 2} == StringView{"\0\0", 2});
    check(!in.tryMakeReadable() && in.getStatus() == IOResult::EndOfFile);
    in.close();
    check(!in.isOpen() && in.getMode() == Stream::Mode::Closed);
}

TEST_CASE("Stream read contracts across backends and boundaries") {
    for (u32 size : {0u, 1u, Stream::MemChunkSize - 1, Stream::MemChunkSize, Stream::MemChunkSize + 1,
                     Stream::MemFilePageSize - 1, Stream::MemFilePageSize, Stream::MemFilePageSize * 2 + 17}) {
        String expected = streamPayload(size);
        for (u32 method = 0; method < 4; method++) {
            ViewStream view{StringView{expected}};
            checkStreamReads(view, expected, method);
            ViewStream mutableView{MutStringView{expected.bytes(), size}};
            checkStreamReads(mutableView, expected, method);
            MemFile file;
            file.write(expected);
            file.seekTo(0);
            checkStreamReads(file, expected, method);
            MemPipe pipe{false};
            pipe.write(expected);
            pipe.close();
            MemReader reader{&pipe};
            checkStreamReads(reader, expected, method);
            MockPipe source{expected};
            source.limit = 19;
            Stream input{&source, false};
            checkStreamReads(input, expected, method);
        }
    }
}

// Content checks read the object itself, never a copy; restore its public cursor afterwards.
static bool hasMemFileContents(MemFile& file, StringView expected) {
    u64 pos = file.getSeekPos();
    file.seekTo(0);
    String actual = String::allocate(expected.numBytes() + 1);
    u32 count = file.read({actual.bytes(), actual.numBytes()});
    bool result = count == expected.numBytes() && actual.left(count) == expected && file.atEof();
    file.seekTo(pos);
    return result;
}

// Exercise partially produced buffers, short EOF views and read/write transitions around page boundaries.
TEST_CASE("Stream seekable storage transitions and partial boundary writes") {
    for (u32 base : {0u, Stream::MemFilePageSize - 4, Stream::MemFilePageSize, Stream::MemFilePageSize + 1}) {
        for (u32 requested : {16u, Stream::MaxConsecutiveBytes}) {
            for (u32 produced : {0u, 1u, 4u, 8u}) {
                String expected = streamPayload(base);
                MemFile file;
                check(file.getCapabilities() == (Stream::CanRead | Stream::CanWrite | Stream::CanSeek));
                check(file.peekByte() == 0 && file.atEof());
                file.write(expected);
                check(file.makeWritable(requested));
                StringView payload = StringView{"12345678"}.left(produced);
                memcpy(file.curByte, payload.bytes(), produced);
                file.curByte += produced;
                expected += payload;
                // Seeking must commit only bytes produced, including an untouched writable shim.
                check(hasMemFileContents(file, expected));
                check(file.getSeekPos() == expected.numBytes());
                file.write('!');
                expected += "!";
                file.flush();
                check(file.readByte() == 0); // Spare write capacity is never readable data.
                file.seekTo(base);
                check(!file.makeReadable(requested) && !file.atEof());
                check(file.viewRemainingBytes() == expected.substr(base));
                check(!file.makeReadable(requested)); // Retrying preserves a short EOF view.
                check(file.skip(min(2u, produced)) == min(2u, produced));
                u32 pos = base + min(2u, produced);
                file.write("XY");
                expected.resize(max(expected.numBytes(), pos + 2));
                memcpy(expected.bytes() + pos, "XY", 2);
                check(hasMemFileContents(file, expected));
                file.close();
                check(!file.isOpen());
            }
        }
    }
    char bytes[] = "abcdef";
    ViewStream view{MutStringView{bytes, 6}};
    view.seekTo(bytes + 2);
    check(view.write("XY") == 2);
    view.seekTo(bytes);
    check(view.makeReadable(6) && view.viewRemainingBytes() == "abXYef");
    view.seekTo(bytes + 6);
    check(!view.makeWritable());
}

// Deterministic model checks arbitrary operation sequences against a byte array and cursor.
TEST_CASE("MemFile mixed operations") {
    for (u32 seed : {1u, 7u, 3000u, 32640u}) {
        Random random{seed};
        MemFile file;
        String expected = String::allocate(Stream::MemFilePageSize + 32);
        for (u32 i = 0; i < expected.numBytes(); i++) {
            expected[i] = (char) shuffleBits(i);
        }
        file.write(expected);
        u32 pos = expected.numBytes();
        for (u32 i = 0; i < 256; i++) {
            switch (random.generateU32() % 6) {
                case 0: {
                    const u32 boundaries[] = {0, Stream::MemFilePageSize - 1, Stream::MemFilePageSize,
                                              Stream::MemFilePageSize + 1, expected.numBytes()};
                    pos = (i & 1) ? boundaries[random.generateU32() % 5]
                                  : random.generateU32() % (expected.numBytes() + 1);
                    file.seekTo(pos);
                    break;
                }
                case 1: {
                    // Write part or all of a requested contiguous buffer.
                    u32 requested = random.generateU32() % Stream::MaxConsecutiveBytes + 1;
                    u32 produced = random.generateU32() % (requested + 1);
                    check(file.makeWritable(requested));
                    expected.resize(max(expected.numBytes(), pos + produced));
                    for (u32 j = 0; j < produced; j++) {
                        char byte = (char) random.generateU32();
                        expected[pos++] = byte;
                        *file.curByte++ = byte;
                    }
                    break;
                }
                case 2: {
                    // Partially consume a contiguous read, including short EOF views.
                    u32 requested = random.generateU32() % Stream::MaxConsecutiveBytes + 1;
                    u32 available = expected.numBytes() - pos;
                    bool readable = file.makeReadable(requested);
                    check(readable == (available >= requested));
                    if (!readable) {
                        check(file.getStatus() == IOResult::EndOfFile);
                        check(file.atEof() == !file.hasRemainingBytes());
                    }
                    u32 exposed = file.numRemainingBytes();
                    if (!check(exposed <= available))
                        return;
                    check(file.viewRemainingBytes() == expected.substr(pos, exposed));
                    u32 consumed = random.generateU32() % (exposed + 1);
                    file.curByte += consumed;
                    pos += consumed;
                    break;
                }
                case 3: {
                    // Verify wrapper reads, including their different short-read padding behavior.
                    char bytes[64];
                    u32 requested = random.generateU32() % sizeof(bytes) + 1;
                    u32 available = min(requested, expected.numBytes() - pos);
                    u32 count = (i & 1) ? file.read({bytes, requested}) : file.tryRead({bytes, requested});
                    check(count == available);
                    check(StringView{bytes, available} == expected.substr(pos, available));
                    if (i & 1) {
                        for (u32 j = available; j < requested; j++) {
                            check(bytes[j] == 0);
                        }
                    }
                    pos += available;
                    break;
                }
                case 4:
                    file.flush();
                    break;
                case 5: {
                    MemFile copy = file;
                    check(copy.getSeekPos() == pos);
                    check(copy.getMode() == file.getMode());
                    check(hasMemFileContents(copy, expected));
                    break;
                }
            }
            check(file.getSeekPos() == pos);
            if (i % 32 == 31) {
                check(hasMemFileContents(file, expected));
            }
        }
        check(hasMemFileContents(file, expected));
    }
}

// Copies retain independent storage; moves preserve active views and replace existing destinations.
TEST_CASE("Stream memory ownership in ordinary and cross-page states") {
    for (u32 base : {0u, Stream::MemFilePageSize - 4, Stream::MemFilePageSize + 1}) {
        for (bool reading : {false, true}) {
            String expected = streamPayload(base) + (reading ? "abcdefghijklmnop" : "ab");
            MemFile source;
            source.write(expected);
            source.seekTo(base);
            if (reading) {
                check(source.makeReadable(8));
                source.skip(2);
            } else {
                check(source.makeWritable(8));
                source.write("12345678");
                expected.resize(base + 8);
                memcpy(expected.bytes() + base, "12345678", 8);
            }
            u32 pos = base + (reading ? 2 : 8);
            char* cur = source.curByte;
            char* end = source.endByte;
            MemFile copy = source;
            MemFile assigned;
            assigned.write("discarded");
            assigned = source;
            check(source.curByte == cur && source.endByte == end);
            for (MemFile* file : {&copy, &assigned}) {
                check(file->getSeekPos() == pos && file->getMode() == source.getMode());
                check(file->curByte != source.curByte);
            }
            source.write("S"); // Mutating the source cannot affect either copy.
            source.close();
            MemFile moved = std::move(copy);
            check(!copy.isOpen());
            MemFile destination;
            destination.write("discarded");
            destination = std::move(moved);
            check(!moved.isOpen());
            check(destination.getSeekPos() == pos);
            check(destination.getMode() == (reading ? Stream::Mode::Reading : Stream::Mode::Writing));
            check(hasMemFileContents(destination, expected));
            assigned.write("Z");
            expected.resize(max(expected.numBytes(), pos + 1));
            expected[pos] = 'Z';
            check(hasMemFileContents(assigned, expected));
        }
    }
    MemFile empty;
    MemFile emptyCopy = empty;
    MemFile emptyAssigned;
    emptyAssigned.write("discarded");
    emptyAssigned = empty;
    check(hasMemFileContents(emptyCopy, "") && hasMemFileContents(emptyAssigned, ""));
    MemFile emptyMoved = std::move(emptyCopy);
    emptyAssigned = std::move(emptyMoved);
    check(!emptyCopy.isOpen() && !emptyMoved.isOpen() && hasMemFileContents(emptyAssigned, ""));
    for (u32 size : {0u, 1u, Stream::MemChunkSize - 1, Stream::MemChunkSize, Stream::MemChunkSize + 1,
                     Stream::MemChunkSize * 2 + 17}) {
        String expected = streamPayload(size);
        MemStream source;
        check(source.getCapabilities() == Stream::CanWrite);
        source.write(expected);
        MemStream copy = source;
        MemStream assigned;
        assigned.write("discarded");
        assigned = source;
        check(source.copyToString() == expected && source.getSeekPos() == size);
        source.write('S');
        copy.write('C');
        assigned.write('A');
        MemStream moved = std::move(copy);
        check(!copy.isOpen());
        copy = MemStream{};
        copy.write("discarded");
        copy = std::move(assigned);
        check(!assigned.isOpen());
        check(moved.moveToString() == expected + "C" && !moved.isOpen());
        check(copy.moveToString() == expected + "A" && !copy.isOpen());
        check(source.moveToString() == expected + "S" && !source.isOpen());
        MemStream emptyOrBoundary;
        emptyOrBoundary.write(expected);
        check(emptyOrBoundary.moveToString() == expected && !emptyOrBoundary.isOpen());
    }
}

// Readers retain independent cursors and cross-chunk views after their writer and source reader disappear.
TEST_CASE("Stream MemPipe readers retain independent positions and final views") {
    for (bool threadSafe : {false, true}) {
        for (u32 size : {0u, 1u, Stream::MemChunkSize - 1, Stream::MemChunkSize, Stream::MemChunkSize + 17,
                         Stream::MemChunkSize * 2 + 17}) {
            String expected = streamPayload(size);
            Owned<MemPipe> pipe = Heap::create<MemPipe>(threadSafe);
            MemReader early{pipe};
            check(!early.tryMakeReadable() && early.getStatus() == IOResult::WouldBlock && !early.atEof());
            // Buffered writer flush and destructor both deliver their pending bytes.
            {
                Stream writer{pipe, false};
                writer.write(expected.left(size / 2));
                writer.flush(true);
                writer.write(expected.substr(size / 2));
            }
            u32 offset = size > Stream::MemChunkSize ? Stream::MemChunkSize - 3 : size / 2;
            check(early.skip(offset) == offset);
            if (size > Stream::MemChunkSize) {
                check(early.makeReadable(8));
                check(early.readByte() == expected[offset++]);
            }
            MemReader late{pipe};
            MemReader copy = early;
            MemReader assigned{pipe};
            assigned = early;
            MemReader moved = std::move(copy);
            check(!copy.isOpen());
            copy = early;
            assigned = std::move(copy);
            check(!copy.isOpen());
            pipe.clear();
            early.close();
            for (MemReader* reader : {&moved, &assigned}) {
                check(reader->getSeekPos() == offset);
                // Repeated oversized requests preserve the cross-chunk final view.
                if (size > Stream::MemChunkSize && size < Stream::MemChunkSize * 2) {
                    for (u32 retry = 0; retry < 2; retry++) {
                        check(!reader->makeReadable(Stream::MaxConsecutiveBytes));
                        check(reader->viewRemainingBytes() == expected.substr(offset));
                        check(!reader->atEof());
                    }
                    check(reader->skip(5) == 5);
                    check(!reader->makeReadable(Stream::MaxConsecutiveBytes));
                    check(reader->viewRemainingBytes() == expected.substr(offset + 5));
                }
                u32 pos = (u32) reader->getSeekPos();
                String bytes = String::allocate(size + 2);
                check(reader->read({bytes.bytes(), bytes.numBytes()}) == size - pos);
                check(bytes.left(size - pos) == expected.substr(pos));
                for (u32 i = size - pos; i < bytes.numBytes(); i++) {
                    check(bytes[i] == 0);
                }
                check(!reader->makeReadable() && !reader->tryMakeReadable() && reader->atEof());
            }
            String bytes = String::allocate(size + 2);
            memset(bytes.bytes(), '-', bytes.numBytes());
            check(late.tryRead({bytes.bytes(), bytes.numBytes()}) == size);
            check(bytes.left(size) == expected && bytes[size] == '-' && bytes[size + 1] == '-');
        }
    }
}

// Every terminal action wakes all readers while leaving their final buffered bytes available.
TEST_CASE("Stream MemPipe termination wakes all readers and preserves tails") {
    for (u32 terminal = 0; terminal < 3; terminal++) {
        for (u32 size : {0u, Stream::MemChunkSize + 7}) {
            Owned<MemPipe> pipe = Heap::create<MemPipe>();
            String expected = streamPayload(size);
            pipe->write(expected);
            MemReader readers[] = {MemReader{pipe}, MemReader{pipe}, MemReader{pipe}};
            u32 offset = size ? Stream::MemChunkSize - 2 : 0;
            Semaphore started;
            Thread threads[3];
            bool results[3] = {};
            for (u32 i = 0; i < 3; i++) {
                check(readers[i].skip(offset) == offset);
                threads[i].run([&, i]() {
                    started.signal();
                    results[i] = readers[i].makeReadable(64);
                });
            }
            for (u32 i = 0; i < 3; i++) {
                started.wait();
            }
            sleepMillis(20);
            if (terminal == 0) {
                pipe->close();
            } else if (terminal == 1) {
                pipe->disable();
            } else {
                pipe.clear();
            }
            IOResult::Code status = terminal == 1 ? IOResult::Disabled : IOResult::EndOfFile;
            for (u32 i = 0; i < 3; i++) {
                threads[i].join();
                check(!results[i] && readers[i].getStatus() == status);
                check(readers[i].atEof() == (size == offset));
                check(readers[i].viewRemainingBytes() == expected.substr(offset));
                check(readers[i].skip(size - offset) == size - offset);
                check(!readers[i].tryMakeReadable() && readers[i].atEof());
            }
            if (pipe) {
                IOResult rejected = pipe->write("ignored");
                check(rejected.code == (terminal == 1 ? IOResult::Disabled : IOResult::Error));
                check(rejected.numBytes == 0);
                if (terminal == 1) {
                    pipe->disable();
                }
                pipe->close();
                pipe->close();
                check(!readers[0].tryMakeReadable() && readers[0].getStatus() == status);
            }
        }
    }
}

// Concurrency: delivery and close/write serialization exercise distinct races.
TEST_CASE("Stream MemPipe concurrent writes and destruction") {
    String expected = streamPayload(Stream::MemChunkSize * 3 + 17);
    for (u32 iteration = 0; iteration < 8; iteration++) {
        Owned<MemPipe> pipe = Heap::create<MemPipe>();
        MemReader reader{pipe};
        bool writesSucceeded = true;
        Thread producer{[&]() {
            for (u32 offset = 0; offset < expected.numBytes();) {
                StringView part = expected.substr(offset, min(701u, expected.numBytes() - offset));
                IOResult written = pipe->write(part);
                writesSucceeded &= written.code == IOResult::OK && written.numBytes == part.numBytes();
                offset += part.numBytes();
            }
            pipe.clear();
        }};
        String bytes = String::allocate(expected.numBytes() + 4);
        u32 count = reader.read({bytes.bytes(), bytes.numBytes()});
        producer.join();
        check(writesSucceeded);
        check(count == expected.numBytes());
        check(bytes.left(count) == expected);
        check(reader.getStatus() == IOResult::EndOfFile);
        check(!reader.makeReadable());
    }
}

// Explicit close is permanent and can race a write without accepting a partial append.
TEST_CASE("Stream MemPipe close is idempotent and serialized with writes") {
    String payload = String::allocate(Stream::MemChunkSize + 17);
    memset(payload.bytes(), 'x', payload.numBytes());
    for (u32 iteration = 0; iteration < 16; iteration++) {
        MemPipe pipe;
        MemReader early{&pipe};
        Semaphore started;
        IOResult written;
        Thread producer{[&]() {
            started.signal();
            written = pipe.write(payload);
        }};
        started.wait();
        pipe.close();
        pipe.close();
        producer.join();
        check((written.code == IOResult::OK && written.numBytes == payload.numBytes()) ||
              (written.code == IOResult::Error && written.numBytes == 0));
        IOResult rejected = pipe.write("ignored");
        check(rejected.code == IOResult::Error && rejected.numBytes == 0);
        MemReader late{&pipe};
        for (MemReader* reader : {&early, &late}) {
            String bytes = String::allocate(payload.numBytes() + 1);
            check(reader->read({bytes.bytes(), bytes.numBytes()}) == written.numBytes);
            check(bytes.left(written.numBytes) == payload.left(written.numBytes));
            check(reader->getStatus() == IOResult::EndOfFile);
        }
    }
}

// WouldBlock permits retry; partial failed writes advance only by the number of bytes accepted.
TEST_CASE("Stream Pipe nonblocking recovery and partial write status") {
    MockPipe pipe{"abc"};
    Stream in{&pipe, false};
    char bytes[8];
    memset(bytes, '-', sizeof(bytes));
    pipe.code = IOResult::WouldBlock;
    check(!in.tryMakeReadable() && !in.atEof());
    check(in.tryRead({bytes, 8}) == 0 && StringView{bytes, 8} == "--------");
    pipe.code = IOResult::OK;
    check(in.tryMakeReadable() && in.viewRemainingBytes() == "abc");
    check(in.tryRead({bytes, 8}) == 3 && StringView{bytes, 8} == "abc-----");
    check(in.atEof());
    pipe.bytes += "defghijkl";
    check(in.tryRead({bytes, 8}) == 8 && StringView{bytes, 8} == "defghijk");
    check(in.tryRead({bytes, 8}) == 1 && bytes[0] == 'l');
    check(in.getSeekPos() == 12);
    // Switching from exhausted input to output delivers the pending write on the next read.
    check(in.write("MN") == 2 && in.getMode() == Stream::Mode::Writing);
    check(in.readByte() == 0 && pipe.bytes == "abcdefghijklMN");
    MockPipe sink;
    Stream out{&sink, false};
    out.write("abcdef");
    sink.code = IOResult::Error;
    sink.writeLimit = 2;
    out.flush();
    check(out.getStatus() == IOResult::Error && out.getSeekPos() == 2 && sink.bytes == "ab");
    out.clearError();
    sink.code = IOResult::OK;
    out.write("XY");
    out.flush();
    check(sink.bytes == "abXY" && out.getSeekPos() == 4);
}

// Pipe seeking, sticky errors and recovery.
TEST_CASE("Stream tracks pipe positions after seeking outside the buffer") {
    MockPipe pipe{streamPayload(256)};
    pipe.limit = 1;
    pipe.code = IOResult::Error;
    pipe.code = IOResult::OK;
    Stream stream{&pipe, false};

    stream.seekTo(100);
    check(pipe.seekCalls == 1);
    check(stream.getSeekPos() == 100);
    check(stream.readByte() == (char) shuffleBits(100u));
    check(stream.getSeekPos() == 101);

    // Rewinding within the refilled buffer must not seek the underlying pipe.
    stream.seekTo(100);
    check(pipe.seekCalls == 1);
    check(stream.getSeekPos() == 100);
    check(stream.readByte() == (char) shuffleBits(100u));

    // A backward seek outside the buffer establishes a new buffer origin too.
    stream.seekTo(10);
    check(pipe.seekCalls == 2);
    check(stream.getSeekPos() == 10);
    check(stream.readByte() == (char) shuffleBits(10u));
    check(stream.getSeekPos() == 11);
}

TEST_CASE("Stream remembers pipe read errors until reset") {
    MockPipe pipe{streamPayload(256)};
    pipe.limit = 1;
    pipe.code = IOResult::Error;
    Stream stream{&pipe, false};
    check(stream.getStatus() != IOResult::Error);
    check(!stream.makeReadable());
    check(stream.getStatus() == IOResult::Error);
    check(stream.atEof());
    pipe.code = IOResult::OK;
    check(!stream.makeReadable());
    check(!stream.tryMakeReadable());
    check(!stream.makeWritable());
    stream.seekTo(100);
    stream.flush();
    check(pipe.readCalls == 1 && pipe.writeCalls == 0);
    check(pipe.seekCalls == 0 && pipe.flushCalls == 0);
    check(stream.getStatus() == IOResult::Error);
    stream.clearError();
    check(stream.getStatus() == IOResult::OK);
    check(stream.tryMakeReadable());
    check(stream.getStatus() == IOResult::OK);
    stream.curByte++;
    stream.clearError();
    pipe.code = IOResult::WouldBlock;
    check(!stream.tryMakeReadable());
    check(stream.getStatus() != IOResult::Error);
    pipe.code = IOResult::Error;
    check(!stream.tryMakeReadable());
    check(stream.getStatus() == IOResult::Error);
    Stream moved{std::move(stream)};
    check(moved.getStatus() == IOResult::Error);
    check(stream.getStatus() != IOResult::Error);
    moved.close();
    check(moved.getStatus() != IOResult::Error);
}

TEST_CASE("Stream remembers buffered pipe write errors until reset") {
    MockPipe pipe{streamPayload(256)};
    pipe.limit = 1;
    pipe.code = IOResult::Error;
    Stream stream{&pipe, false};
    stream.write("x");
    check(stream.getStatus() != IOResult::Error);
    stream.flush();
    check(stream.getStatus() == IOResult::Error);
    check(stream.atEof());
    pipe.code = IOResult::OK;
    check(stream.write("x") == 0);
    stream.flush();
    check(pipe.writeCalls == 1 && pipe.flushCalls == 0);
    check(stream.getStatus() == IOResult::Error);
    stream.clearError();
    check(stream.write("x") == 1);
    stream.flush();
    check(stream.getStatus() == IOResult::OK);
    check(pipe.flushCalls == 1);
    pipe.code = IOResult::Disabled;
    stream.write("x");
    stream.flush();
    check(stream.getStatus() != IOResult::Error);
    pipe.code = IOResult::Error;
    for (u32 i = 0; i <= Stream::PipeBufferSize; i++) {
        stream.write("x");
    }
    check(stream.getStatus() == IOResult::Error);
    stream.clearError();
    // Switching to read mode also flushes pending writes.
    stream.makeReadable();
    check(stream.getStatus() == IOResult::Error);
}

TEST_CASE("Stream error latch permits buffered access but blocks buffer transitions") {
    MockPipe pipe{streamPayload(256)};
    pipe.limit = 1;
    pipe.code = IOResult::Error;
    Stream stream{&pipe, false};
    pipe.code = IOResult::OK;
    check(stream.makeReadable());
    pipe.code = IOResult::Error;
    check(!stream.makeReadable(2));
    check(stream.getStatus() == IOResult::Error);
    check(!stream.atEof()); // One buffered byte remains after the failed larger request.
    u32 readCalls = pipe.readCalls;
    check(stream.makeReadable());
    check(stream.tryMakeReadable());
    check(stream.getStatus() == IOResult::Error);
    check(stream.readByte() == (char) shuffleBits(0u));
    check(stream.atEof());
    check(!stream.makeReadable());
    check(!stream.tryMakeReadable());
    check(pipe.readCalls == readCalls);
    stream.clearError();

    check(!stream.atEof());
    pipe.code = IOResult::OK;
    check(stream.makeReadable());
    check(stream.getStatus() == IOResult::OK);
}

// Clearing an error must leave unrelated I/O statuses unchanged.
TEST_CASE("Stream clearError preserves non-error statuses") {
    MockPipe pipe{streamPayload(256)};
    pipe.limit = 1;
    pipe.code = IOResult::Error;
    Stream stream{&pipe, false};
    for (IOResult::Code code : {IOResult::OK, IOResult::WouldBlock, IOResult::EndOfFile, IOResult::Disabled}) {
        pipe.code = code;
        check(stream.tryMakeReadable() == (code == IOResult::OK));
        check(stream.getStatus() == code);
        stream.clearError();
        check(stream.getStatus() == code);
        stream.curByte = stream.endByte;
    }
}

//  ▄▄   ▄▄ ▄▄         ▄▄                 ▄▄▄  ▄▄   ▄▄
//  ██   ██ ▄▄ ▄▄▄▄▄  ▄██▄▄ ▄▄  ▄▄  ▄▄▄▄   ██  ███▄███  ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄
//   ██ ██  ██ ██  ▀▀  ██   ██  ██  ▄▄▄██  ██  ██▀█▀██ ██▄▄██ ██ ██ ██ ██  ██ ██  ▀▀ ██  ██
//    ▀█▀   ██ ██      ▀█▄▄ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██   ██ ▀█▄▄▄  ██ ██ ██ ▀█▄▄█▀ ██     ▀█▄▄██
//                                                                                    ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX VirtualMemory_

TEST_CASE("Usage stats with reserve/commit/decommit/unreserve") {
    VirtualMemory::Properties props = VirtualMemory::getProperties();
    uptr regionSize = max(props.pageSize * 4, props.regionAlignment);

    uptr initialReserved = VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed);
    uptr initialCommitted = VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed);

    // Reserve region
    void* addr = VirtualMemory::reserveRegion(regionSize);
    check(addr != nullptr);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted);

    // Commit 3 pages
    VirtualMemory::commitPages(addr, props.pageSize * 3);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted + props.pageSize * 3);

    // Decommit 1 page
    VirtualMemory::decommitPages(addr, props.pageSize);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted + props.pageSize * 2);

    // Unreserve region (with 2 pages still committed)
    VirtualMemory::unreserveRegion(addr, regionSize, props.pageSize * 2);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted);
}

TEST_CASE("Usage stats with alloc/free") {
    VirtualMemory::Properties props = VirtualMemory::getProperties();
    uptr regionSize = props.regionAlignment * 2;

    uptr initialReserved = VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed);
    uptr initialCommitted = VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed);

    // Alloc region (reserves and commits)
    void* addr = VirtualMemory::allocRegion(regionSize);
    check(addr != nullptr);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved + regionSize);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted + regionSize);

    // Free region (decommits and unreserves)
    VirtualMemory::freeRegion(addr, regionSize);
    check(VirtualMemory::totalReservedBytes.load(MemoryOrder::Relaxed) == initialReserved);
    check(VirtualMemory::totalCommittedBytes.load(MemoryOrder::Relaxed) == initialCommitted);
}

//   ▄▄▄▄         ▄▄
//  ██  ▀▀ ▄▄  ▄▄ ██▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██  ██ ██  ██ ██  ██ ██  ▀▀ ██  ██ ██    ██▄▄██ ▀█▄▄▄  ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ██▄▄█▀ ██▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀
//                       ██

#if PLY_WITH_SUBPROCESS

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Subprocess_

TEST_CASE("execShellCommand() with merged output") {
    // Select a platform command with a nonzero exit status.
#if defined(PLY_WINDOWS)
    StringView command = "echo stdout&echo stderr>&2&exit /b 7";
    StringView expectedOutput = "stdout\r\nstderr\r\n";
#else
    StringView command = "printf stdout; printf stderr >&2; exit 7";
    StringView expectedOutput = "stdoutstderr";
#endif

    // Capture the merged stream through Plywood's process pipes.
    Owned<Subprocess> process =
        Subprocess::execShellCommand(command, {}, Subprocess::Output::openMerged(), Subprocess::Input::ignore());
    check(process);
    if (!process)
        return;
    String output = readAllRemainingData(process->getStdOutReader());
    check(output == expectedOutput);
    check(process->join() == 7);
}

TEST_CASE("execShellCommand() preserves quotes and backslashes") {
    // Exercise shell quoting and a trailing backslash that CRT argument escaping would double on Windows.
#if defined(PLY_WINDOWS)
    StringView command = "echo \"two words\" C:\\";
    StringView expectedOutput = "\"two words\" C:\\\r\n";
#else
    StringView command = "printf '%s' \"two words\" ' C:\\'";
    StringView expectedOutput = "two words C:\\";
#endif
    Owned<Subprocess> process =
        Subprocess::execShellCommand(command, {}, Subprocess::Output::openMerged(), Subprocess::Input::ignore());
    check(process);
    if (!process)
        return;

    // Drain the pipe and compare the shell's output exactly.
    check(readAllRemainingData(process->getStdOutReader()) == expectedOutput);
    check(process->join() == 0);
}

TEST_CASE("execShellCommand() runs a quoted executable path") {
    // A command starting with a quoted executable must retain those quotes when cmd.exe strips its outer pair.
    String command = String::format("\"{}\" --help", getCurrentExecutablePath());
    Owned<Subprocess> process =
        Subprocess::execShellCommand(command, {}, Subprocess::Output::ignore(), Subprocess::Input::ignore());
    check(process);
    if (!process)
        return;
    check(process->join() == 0);
}

TEST_CASE("exec() reports child setup errors") {
    // Select a path that is guaranteed not to exist on the current platform.
#if defined(PLY_WINDOWS)
    StringView nonexistentPath = "?:/plywood-nonexistent";
#else
    StringView nonexistentPath = "/dev/null/nonexistent";
#endif

    // A successful spawn returns the running subprocess.
    Owned<Subprocess> success = Subprocess::exec(getCurrentExecutablePath(), {"--help"}, {},
                                                 Subprocess::Output::ignore(), Subprocess::Input::ignore());
    check(success && success->join() == 0);

    // A working directory error is reported without running the executable in the parent directory.
    Owned<Subprocess> badDir = Subprocess::exec(getCurrentExecutablePath(), {"--help"}, nonexistentPath,
                                                Subprocess::Output::ignore(), Subprocess::Input::ignore());
    check(!badDir);

    // An executable lookup error is reported to the parent as a failed spawn.
    Owned<Subprocess> badExe = Subprocess::exec(nonexistentPath, {}, {}, Subprocess::Output::ignore(),
                                                Subprocess::Input::ignore());
    check(!badExe);
}

TEST_CASE("joinWithTimeout() supports timeouts and retries") {
    // Keep the subprocess alive until the parent closes its input, avoiding startup timing assumptions.
#if defined(PLY_WINDOWS)
    StringView command = "set /p value= >nul & exit /b -2";
    s32 expectedExitCode = -2;
#else
    StringView command = "read value; exit 7";
    s32 expectedExitCode = 7;
#endif

    // A timeout must leave the subprocess available for another wait or for termination.
    for (u32 mode = 0; mode < 3; mode++) {
        Owned<Subprocess> process = Subprocess::execShellCommand(command, {}, Subprocess::Output::openMerged());
        check(process);
        if (!process)
            return;
        u64 startTicks = getCpuTicks();
        check(!process->joinWithTimeout(0).joined);
        double elapsedMillis = (getCpuTicks() - startTicks) * 1000.0 / getCpuTicksPerSecond();
        check(elapsedMillis < 1000.0);
        startTicks = getCpuTicks();
        check(!process->joinWithTimeout(30).joined);
        elapsedMillis = (getCpuTicks() - startTicks) * 1000.0 / getCpuTicksPerSecond();
        check(elapsedMillis >= 20.0 && elapsedMillis < 1000.0);

        // Exercise bounded completion, polling an exited child, and an indefinite wait after termination.
        if (mode == 0) {
            process->closeStdIn();
            process->closeStdIn(); // Closing an already closed input endpoint is harmless.
            Subprocess::JoinResult result = process->joinWithTimeout(5000);
            check(result.joined && result.exitCode == expectedExitCode);
        } else if (mode == 1) {
            process->closeStdIn();
            char byte;
            check(process->getStdOutReader()->read({&byte, 1}).code == IOResult::EndOfFile);
            startTicks = getCpuTicks();
            Subprocess::JoinResult result;
            do {
                result = process->joinWithTimeout(0);
            } while (!result.joined && (getCpuTicks() - startTicks) / getCpuTicksPerSecond() < 5.0);
            check(result.joined && result.exitCode == expectedExitCode);
        } else {
            check(process->terminate());
            Subprocess::JoinResult result = process->joinWithTimeout(-2);
            check(result.joined);
#if defined(PLY_POSIX)
            check(result.exitCode == -1);
#endif
        }
    }
}

TEST_CASE("terminate() stops an isolated process tree") {
    // Start a shell whose child would otherwise keep the captured output pipe open for several seconds.
#if defined(PLY_WINDOWS)
    StringView command = "ping -n 4 127.0.0.1 >nul";
#else
    StringView command = "sleep 3 & wait";
#endif
    Subprocess::Options processOptions;
    processOptions.terminateProcessTree = true;
    Owned<Subprocess> process = Subprocess::execShellCommand(command, {}, Subprocess::Output::openMerged(),
                                                             Subprocess::Input::ignore(), processOptions);
    check(process);
    if (!process)
        return;

    // Join on another thread so terminate() must safely release its blocking process wait.
    Atomic<bool> joinReturned = false;
    Thread joinThread{[&]() {
        process->join();
        joinReturned.store(true, MemoryOrder::Release);
    }};
    sleepMillis(50);
    check(!joinReturned.load(MemoryOrder::Acquire));
    u64 startTicks = getCpuTicks();
    check(process->terminate());
    char byte;
    check(process->getStdOutReader()->read({&byte, 1}).code == IOResult::EndOfFile);
    joinThread.join();
    check(joinReturned.load(MemoryOrder::Acquire));
    double elapsedMillis = (getCpuTicks() - startTicks) * 1000.0 / getCpuTicksPerSecond();
    check(elapsedMillis < 1500.0);
}

#endif // PLY_WITH_SUBPROCESS

//  ▄▄▄▄▄          ▄▄   ▄▄
//  ██  ██  ▄▄▄▄  ▄██▄▄ ██▄▄▄
//  ██▀▀▀   ▄▄▄██  ██   ██  ██
//  ██     ▀█▄▄██  ▀█▄▄ ██  ██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Path_

TEST_CASE("matchGlobPattern()") {
    check(matchGlobPattern("file.cpp", "*.cpp"));
    check(matchGlobPattern("file1.cpp", "file?.cpp"));
    check(!matchGlobPattern("file12.cpp", "file?.cpp"));
    check(matchGlobPattern("ab", "a[b-d]"));
    check(matchGlobPattern("ad", "a[!b-c]"));
    check(!matchGlobPattern("ab", "a[!b-c]"));
    check(matchGlobPattern("a7", "a[[:digit:]]"));
    check(matchGlobPattern("a[b]", "a\\[b\\]"));
    check(matchGlobPattern("ab", "a\\b"));
}

TEST_CASE("matchGitIgnorePattern() with gitignore-style patterns") {
    // Basename-only patterns match at any depth and ignored directories ignore their descendants.
    check(matchGitIgnorePattern("foo", false, "foo"));
    check(matchGitIgnorePattern("a/foo", false, "foo"));
    check(matchGitIgnorePattern("a/foo/file.txt", false, "foo"));
    check(!matchGitIgnorePattern("a/foo.txt", false, "foo"));

    // Leading and middle slashes anchor patterns to the supplied path root.
    check(matchGitIgnorePattern("foo", false, "/foo"));
    check(!matchGitIgnorePattern("a/foo", false, "/foo"));
    check(matchGitIgnorePattern("doc/frotz", false, "doc/frotz"));
    check(matchGitIgnorePattern("doc/frotz/file.txt", false, "doc/frotz"));
    check(!matchGitIgnorePattern("c/doc/frotz", false, "doc/frotz"));

    // A trailing slash restricts the match to directories, including matched ancestors.
    check(matchGitIgnorePattern("foo", true, "foo/"));
    check(!matchGitIgnorePattern("foo", false, "foo/"));
    check(matchGitIgnorePattern("a/foo/file.txt", false, "foo/"));
    check(!matchGitIgnorePattern("a/foo.txt", false, "foo/"));

    // Single-component wildcards do not cross path separators.
    check(matchGitIgnorePattern("logs/file.log", false, "*.log"));
    check(!matchGitIgnorePattern("logs/file.txt", false, "*.log"));
    check(matchGitIgnorePattern("Documentation/git.html", false, "Documentation/*.html"));
    check(!matchGitIgnorePattern("Documentation/ppc/ppc.html", false, "Documentation/*.html"));

    // Double-star forms match zero or more directories.
    check(matchGitIgnorePattern("foo", false, "**/foo"));
    check(matchGitIgnorePattern("a/b/foo", false, "**/foo"));
    check(matchGitIgnorePattern("a/foo", false, "a/**/foo"));
    check(matchGitIgnorePattern("a/b/c/foo", false, "a/**/foo"));
    check(!matchGitIgnorePattern("b/a/foo", false, "a/**/foo"));
    check(matchGitIgnorePattern("build", true, "build/**"));
    check(!matchGitIgnorePattern("build", false, "build/**"));
    check(matchGitIgnorePattern("build/sub/file.txt", false, "build/**"));

    // Bracket expressions and escapes use Git's wildcard syntax.
    check(matchGitIgnorePattern("ab", false, "a[b]"));
    check(!matchGitIgnorePattern("a[b]", false, "a[b]"));
    check(matchGitIgnorePattern("a[b]", false, "a\\[b\\]"));
    check(matchGitIgnorePattern("ax", false, "a[!b]"));
    check(!matchGitIgnorePattern("ab", false, "a[!b]"));
    check(matchGitIgnorePattern("a7", false, "a[[:digit:]]"));
    check(!matchGitIgnorePattern("aa", false, "a[[:digit:]]"));

    // `#` and `!` are literal.
    check(matchGitIgnorePattern("#literal", false, "#literal"));
    check(matchGitIgnorePattern("!literal", false, "!literal"));
    check(!matchGitIgnorePattern("foo", false, "foo "));
    check(matchGitIgnorePattern("foo ", false, "foo "));
    check(matchGitIgnorePattern("foo ", false, "foo\\ "));
}

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if 0 // Disabled test
#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Directory_Watcher_

template <typename Item>
class MessageQueue {
    Mutex mutex;
    ConditionVariable cv;
    Array<Item> items;

public:
    void push(Item&& item) {
        LockGuard<Mutex> lock{mutex};
        items.append(std::move(item));
        cv.wakeOne();
    }
    bool pop(Item& item, u32 timeoutMs = 2000) {
        u64 timeLimit = getCpuTicks() + (u64) (timeoutMs * getCpuTicksPerSecond() / 1000.f);
        LockGuard<Mutex> lock{mutex};
        while (items.isEmpty()) {
            u64 now = getCpuTicks();
            s32 remainingMs = (s32) ((timeLimit - now) * (1000.f / getCpuTicksPerSecond()));
            if (remainingMs <= 0)
                return false;
            cv.timedWait(lock, remainingMs);
        }
        item = std::move(items[0]);
        items.erase(0);
        return true;
    }
};

TEST_CASE("DirectoryWatcher") {
    struct Event {
        String path;
        bool mustRecurse;
    };
    MessageQueue<Event> messageQueue;

    auto waitForEvent = [&](const Event& expectedEvent) -> bool {
        for (;;) {
            Event event;
            if (!messageQueue.pop(event))
                return false;
            if (event.path == expectedEvent.path && event.mustRecurse == expectedEvent.mustRecurse)
                return true;
        }
    };

    // Set up temp directory.
    String tempDir = joinPath(BUILD_DIR, "temp-dir-watcher");
    FileSystem::removeDirTree(tempDir); // Clean up from any previous run
    FileSystem::makeDir(tempDir);

    // Start the watcher.
    DirectoryWatcher watcher;
    watcher.start(tempDir, [&](StringView path, bool mustRecurse) {
        messageQueue.push({path, mustRecurse});
    });

    // Create a file in the temp directory.
    FileSystem::saveText(joinPath(tempDir, "first_file.txt"), "Hello, world!\n");
    check(waitForEvent({"first_file.txt", false}));

    // Create a subdirectory.
    FileSystem::makeDir(joinPath(tempDir, "subdir"));
    check(waitForEvent({"subdir", true}));

    // Modify the first file.
    FileSystem::saveText(joinPath(tempDir, "first_file.txt"), "Modified content!\n");
    check(waitForEvent({"first_file.txt", false}));

    // Create a file in the subdirectory.
    FileSystem::saveText(joinPath(tempDir, "subdir", "second_file.txt"), "Another file\n");
    check(waitForEvent({joinPath("subdir", "second_file.txt"), false}));

    // Delete the first file.
    FileSystem::deleteFile(joinPath(tempDir, "first_file.txt"));
    check(waitForEvent({"first_file.txt", false}));

    watcher.stop();
    FileSystem::removeDirTree(tempDir);
}
#endif

//   ▄▄▄▄                                             ▄▄     ▄▄▄  ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄██      ██  ▄▄ ▄▄▄▄▄   ▄▄▄▄
//  ██     ██  ██ ██ ██ ██ ██ ██ ██  ▄▄▄██ ██  ██ ██  ██      ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ▀█▄▄█▀ ██ ██ ██ ██ ██ ██ ▀█▄▄██ ██  ██ ▀█▄▄██     ▄██▄ ██ ██  ██ ▀█▄▄▄
//

// Stores values populated by focused CommandLineParser tests.
struct CommandLineParserTestOptions {
    bool useProxy = false;
    String proxyPort;
    String prompt;
    PLY_DECLARE_TYPE_INFO(CommandLineParserTestOptions)
};

PLY_STRUCT_BEGIN(CommandLineParserTestOptions)
PLY_STRUCT_MEMBER(useProxy)
PLY_STRUCT_MEMBER(proxyPort)
PLY_STRUCT_MEMBER(prompt)
PLY_STRUCT_END()

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX CommandLine_

TEST_CASE("Optional inline value") {
    // A bare option sets its boolean while leaving the following prompt untouched.
    {
        CommandLineParserTestOptions parsed;
        CommandLineParser parser({
            {"-x", "--proxy", PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, useProxy), "Use proxy",
             PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, proxyPort)},
        });
        parser.defaultHandler = [&](ArrayView<const char*> args, u32 index) { parsed.prompt = args[index]; };
        const char* argv[] = {"test", "-x", "hello"};
        check(parser.apply(3, argv, &parsed));
        check(parsed.useProxy);
        check(!parsed.proxyPort);
        check(parsed.prompt == "hello");
    }

    // Both option spellings accept an inline value after '='.
    {
        CommandLineParserTestOptions parsed;
        CommandLineParser parser({
            {"-x", "--proxy", PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, useProxy), "Use proxy",
             PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, proxyPort)},
        });
        const char* argv[] = {"test", "-x=8088"};
        check(parser.apply(2, argv, &parsed));
        check(parsed.useProxy);
        check(parsed.proxyPort == "8088");
    }
    {
        CommandLineParserTestOptions parsed;
        CommandLineParser parser({
            {"-x", "--proxy", PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, useProxy), "Use proxy",
             PLY_LOOKUP_MEMBER(CommandLineParserTestOptions, proxyPort)},
        });
        const char* argv[] = {"test", "--proxy=9090"};
        check(parser.apply(2, argv, &parsed));
        check(parsed.useProxy);
        check(parsed.proxyPort == "9090");
    }
}
