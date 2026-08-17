/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    Scalar, 2D and 3D Math                     │
│               Documentation: /docs/math.md               │
│                                                          │
└─────────────────────────────────────────────────────────*/

#include "ply-math.h"

namespace ply {

//   ▄▄▄▄               ▄▄▄                    ▄▄▄▄▄                      ▄▄   ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄   ██   ▄▄▄▄  ▄▄▄▄▄      ██    ▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄ ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//   ▀▀▀█▄ ██     ▄▄▄██  ██   ▄▄▄██ ██  ▀▀     ██▀▀  ██  ██ ██  ██ ██     ██   ██ ██  ██ ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██ ▄██▄ ▀█▄▄██ ██         ██    ▀█▄▄██ ██  ██ ▀█▄▄▄  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀
//

// Scalar math functions contain code adapted from musl libc and FreeBSD's msun library.
// See LICENSE for copyright and license notices.

static u32 getFloatBits(float value) {
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float makeFloat(u32 bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static u64 getDoubleBits(double value) {
    u64 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double makeDouble(u64 bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float scaleFloatByPowerOfTwo(float value, s32 exponent) {
    float result = value;
    if (exponent > 127) {
        result *= 1.7014118346046923e38f;
        exponent -= 127;
        if (exponent > 127) {
            result *= 1.7014118346046923e38f;
            exponent -= 127;
            if (exponent > 127) {
                exponent = 127;
            }
        }
    } else if (exponent < -126) {
        result *= 1.9721522630525295e-31f;
        exponent += 126 - 24;
        if (exponent < -126) {
            result *= 1.9721522630525295e-31f;
            exponent += 126 - 24;
            if (exponent < -126) {
                exponent = -126;
            }
        }
    }
    return result * makeFloat(static_cast<u32>(127 + exponent) << 23);
}

static double scaleByPowerOfTwo(double value, s32 exponent) {
    while (exponent > 0) {
        value *= 2;
        exponent--;
    }
    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }
    return value;
}

// Evaluates sin(x) on approximately [-pi/4, pi/4].
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/__sindf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n19
static float sinKernel(double x) {
    static constexpr double S1 = -0.166666666416265235595;
    static constexpr double S2 = 0.0083333293858894631756;
    static constexpr double S3 = -0.000198393348360966317347;
    static constexpr double S4 = 0.0000027183114939898219064;

    // Evaluate the polynomial in parallel chains.
    double z = x * x;
    double w = z * z;
    double r = S3 + z * S4;
    double s = z * x;
    return static_cast<float>((x + s * (S1 + z * S2)) + s * w * r);
}

// Evaluates cos(x) on approximately [-pi/4, pi/4].
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/__cosdf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n19
static float cosKernel(double x) {
    static constexpr double C0 = -0.499999997251031003120;
    static constexpr double C1 = 0.0416666233237390631894;
    static constexpr double C2 = -0.00138867637746099294692;
    static constexpr double C3 = 0.0000243904487962774090654;

    // Evaluate the polynomial in parallel chains.
    double z = x * x;
    double w = z * z;
    double r = C2 + z * C3;
    return static_cast<float>(((1 + z * C0) + w * C1) + (w * z) * r);
}

// Evaluates tan(x) or -1/tan(x) on approximately [-pi/4, pi/4].
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/__tandf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n18
static float tanKernel(double x, bool odd) {
    static constexpr double T[] = {
        0.333331395030791399758,  0.133392002712976742718,   0.0533812378445670393523,
        0.0245283181166547278873, 0.00297435743359967304927, 0.00946564784943673166728,
    };

    // Evaluate the polynomial in parallel chains.
    double z = x * x;
    double r = T[4] + z * T[5];
    double t = T[2] + z * T[3];
    double w = z * z;
    double s = z * x;
    double u = T[0] + z * T[1];
    r = (x + s * u) + (s * w) * (t + w * r);
    return static_cast<float>(odd ? -1 / r : r);
}

// Returns x modulo pi/2 for large single-precision inputs and the low three bits of
// the quadrant. The 24-bit chunks contain enough bits of 2/pi for every float exponent.
// Details:
// https://git.musl-libc.org/cgit/musl/tree/src/math/__rem_pio2_large.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n13
static s32 reduceLarge(float x, double* remainder) {
    static constexpr s32 twoOverPi[] = {
        0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7,
        0x246E3A, 0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C,
        0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11,
        0x8B5A0A, 0x6D1F6D, 0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7,
        0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E,
        0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
    };
    static constexpr double piOverTwo[] = {
        1.57079625129699707031,
        7.54978941586159635335e-08,
        5.39030252995776476554e-15,
        3.28200341580791294123e-22,
    };

    u32 bits = getFloatBits(x);
    u32 magnitude = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    s32 exponent = static_cast<s32>(magnitude >> 23) - (127 + 23);
    double input = makeFloat(magnitude - (static_cast<u32>(exponent) << 23));

    constexpr s32 numInitialTerms = 3;
    s32 quadrantChunks[20];
    double productTerms[20];
    double piProduct[20];
    s32 tableStart = max(0, (exponent - 3) / 24);
    s32 chunkExponent = exponent - 24 * (tableStart + 1);

    // Compute the initial product terms.
    for (s32 i = 0; i <= numInitialTerms; i++) {
        productTerms[i] = input * twoOverPi[tableStart + i];
    }

    s32 last = numInitialTerms;
recompute:
    // Distill the product terms into integer chunks in reverse order.
    double z = productTerms[last];
    for (s32 i = 0, j = last; j > 0; i++, j--) {
        double carry = static_cast<s32>(z * (1.0 / 16777216.0));
        quadrantChunks[i] = static_cast<s32>(z - 16777216.0 * carry);
        z = productTerms[j - 1] + carry;
    }

    // Extract the quadrant and the remaining fractional part.
    z = scaleByPowerOfTwo(z, chunkExponent);
    z -= 8 * static_cast<double>(static_cast<u64>(z * 0.125));
    s32 quadrant = static_cast<s32>(z);
    z -= quadrant;
    s32 complement = 0;
    if (chunkExponent > 0) {
        s32 extra = quadrantChunks[last - 1] >> (24 - chunkExponent);
        quadrant += extra;
        quadrantChunks[last - 1] -= extra << (24 - chunkExponent);
        complement = quadrantChunks[last - 1] >> (23 - chunkExponent);
    } else if (chunkExponent == 0) {
        complement = quadrantChunks[last - 1] >> 23;
    } else if (z >= 0.5) {
        complement = 2;
    }

    // Complement fractions above one half so the remainder stays small.
    if (complement > 0) {
        quadrant++;
        s32 carry = 0;
        for (s32 i = 0; i < last; i++) {
            s32 chunk = quadrantChunks[i];
            if (carry == 0) {
                if (chunk != 0) {
                    carry = 1;
                    quadrantChunks[i] = 0x1000000 - chunk;
                }
            } else {
                quadrantChunks[i] = 0xffffff - chunk;
            }
        }
        if (chunkExponent > 0) {
            quadrantChunks[last - 1] &= (0xffffff >> chunkExponent);
        }
        if (complement == 2) {
            z = 1 - z;
            if (carry != 0) {
                z -= scaleByPowerOfTwo(1, chunkExponent);
            }
        }
    }

    // Recompute with more terms if cancellation exhausted the known digits.
    if (z == 0) {
        s32 nonZero = 0;
        for (s32 i = last - 1; i >= numInitialTerms; i--) {
            nonZero |= quadrantChunks[i];
        }
        if (nonZero == 0) {
            s32 extraTerms = 1;
            while (quadrantChunks[numInitialTerms - extraTerms] == 0) {
                extraTerms++;
            }
            for (s32 i = last + 1; i <= last + extraTerms; i++) {
                productTerms[i] = input * twoOverPi[tableStart + i];
            }
            last += extraTerms;
            goto recompute;
        }
    }

    // Remove zero chunks or split the residual into 24-bit chunks.
    if (z == 0) {
        last--;
        chunkExponent -= 24;
        while (quadrantChunks[last] == 0) {
            last--;
            chunkExponent -= 24;
        }
    } else {
        z = scaleByPowerOfTwo(z, -chunkExponent);
        if (z >= 16777216.0) {
            double carry = static_cast<s32>(z * (1.0 / 16777216.0));
            quadrantChunks[last] = static_cast<s32>(z - 16777216.0 * carry);
            last++;
            chunkExponent += 24;
            quadrantChunks[last] = static_cast<s32>(carry);
        } else {
            quadrantChunks[last] = static_cast<s32>(z);
        }
    }

    // Convert the integer chunks back to floating-point values.
    double weight = scaleByPowerOfTwo(1, chunkExponent);
    for (s32 i = last; i >= 0; i--) {
        productTerms[i] = weight * quadrantChunks[i];
        weight *= 1.0 / 16777216.0;
    }

    // Convolve the chunks with the split representation of pi/2.
    for (s32 i = last; i >= 0; i--) {
        double term = 0;
        for (s32 j = 0; j <= numInitialTerms && j <= last - i; j++) {
            term += piOverTwo[j] * productTerms[i + j];
        }
        piProduct[last - i] = term;
    }

    // Compress the convolution into a single-precision remainder.
    double result = 0;
    for (s32 i = last; i >= 0; i--) {
        result += piProduct[i];
    }
    if (complement != 0) {
        result = -result;
    }
    if (negative) {
        *remainder = -result;
        return -(quadrant & 7);
    }
    *remainder = result;
    return quadrant & 7;
}

// Reduces a medium float angle modulo pi/2.
// Details:
// https://git.musl-libc.org/cgit/musl/tree/src/math/__rem_pio2f.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n16
static s32 reduce(float x, double* remainder) {
    static constexpr double inversePiOverTwo = 0.636619772367581382433;
    static constexpr double piOverTwoHigh = 1.57079631090164184570;
    static constexpr double piOverTwoTail = 1.58932547735281966916e-08;
#if FLT_EVAL_METHOD == 2
    static constexpr long double roundToInteger = 1.5L / LDBL_EPSILON;
    long double quadrant = static_cast<long double>(x) * inversePiOverTwo;
#else
    static constexpr double roundToInteger = 1.5 / DBL_EPSILON;
    double quadrant = static_cast<double>(x) * inversePiOverTwo;
#endif
    // Round x*(2/pi) to the nearest integer and subtract a split pi/2.
    quadrant = quadrant + roundToInteger - roundToInteger;
    s32 n = static_cast<s32>(quadrant);
    *remainder = x - quadrant * piOverTwoHigh - quadrant * piOverTwoTail;
    return n;
}

// Generates square-root significand bits one digit at a time for either binary precision.
static u64 generateSquareRootBits(u64 remainder, u64 movingBit, u64* residual) {
    u64 result = 0;
    u64 partial = 0;
    while (movingBit != 0) {
        u64 trial = partial + movingBit;
        if (trial <= remainder) {
            partial = trial + movingBit;
            remainder -= trial;
            result += movingBit;
        }
        remainder += remainder;
        movingBit >>= 1;
    }
    *residual = remainder;
    return result;
}

// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/sqrtf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n20
float sqrt(float value) {
    // Handle infinities, NaNs, signed zero and negative values.
    u32 bits = getFloatBits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u)
        return value * value + value;
    if (static_cast<s32>(bits) <= 0) {
        if ((bits & 0x7fffffffu) == 0)
            return value;
        return (value - value) / (value - value);
    }

    // Normalize the significand and make the exponent even.
    s32 exponent = static_cast<s32>(bits >> 23);
    if (exponent == 0) {
        s32 shifts = 0;
        while ((bits & 0x00800000u) == 0) {
            shifts++;
            bits <<= 1;
        }
        exponent -= shifts - 1;
    }
    exponent -= 127;
    bits = (bits & 0x007fffffu) | 0x00800000u;
    if (exponent & 1) {
        bits += bits;
    }
    exponent >>= 1;

    // Generate the significand and probe the active rounding direction.
    u64 residual;
    u32 resultBits = static_cast<u32>(generateSquareRootBits(bits + bits, 0x01000000u, &residual));
    if (residual != 0) {
        volatile float roundingProbe = 1 - 1e-30f;
        if (roundingProbe >= 1) {
            roundingProbe = 1 + 1e-30f;
            resultBits += roundingProbe > 1 ? 2 : resultBits & 1;
        }
    }
    // Assemble the rounded significand and halved exponent.
    resultBits = (resultBits >> 1) + 0x3f000000u;
    resultBits += static_cast<u32>(exponent) << 23;
    return makeFloat(resultBits);
}

// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/sqrt.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n12
double sqrt(double value) {
    // Handle infinities, NaNs, signed zero and negative values.
    u64 bits = getDoubleBits(value);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull)
        return value * value + value;
    if (static_cast<s64>(bits) <= 0) {
        if ((bits & 0x7fffffffffffffffull) == 0)
            return value;
        return (value - value) / (value - value);
    }

    // Normalize the significand and make the exponent even.
    s32 exponent = static_cast<s32>(bits >> 52);
    if (exponent == 0) {
        s32 shifts = 0;
        while ((bits & 0x0010000000000000ull) == 0) {
            shifts++;
            bits <<= 1;
        }
        exponent -= shifts - 1;
    }
    exponent -= 1023;
    bits = (bits & 0x000fffffffffffffull) | 0x0010000000000000ull;
    if (exponent & 1) {
        bits += bits;
    }
    exponent >>= 1;

    // Generate the significand and probe the active rounding direction.
    u64 residual;
    u64 resultBits = generateSquareRootBits(bits + bits, 0x0020000000000000ull, &residual);
    if (residual != 0) {
        volatile double roundingProbe = 1 - 1e-300;
        if (roundingProbe >= 1) {
            roundingProbe = 1 + 1e-300;
            resultBits += roundingProbe > 1 ? 2 : resultBits & 1;
        }
    }
    // Assemble the rounded significand and halved exponent.
    resultBits = (resultBits >> 1) + 0x3fe0000000000000ull;
    resultBits += static_cast<u64>(exponent) << 52;
    return makeDouble(resultBits);
}

// Computes the single-precision exponential using range reduction and a polynomial.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/expf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n23
float exp(float value) {
    static constexpr float half[] = {0.5f, -0.5f};
    static constexpr float ln2High = 6.9314575195e-1f;
    static constexpr float ln2Low = 1.4286067653e-6f;
    static constexpr float inverseLn2 = 1.4426950216f;
    static constexpr float P1 = 1.6666625440e-1f;
    static constexpr float P2 = -2.7667332906e-3f;

    // Handle values near overflow or underflow, infinities and NaNs.
    u32 bits = getFloatBits(value);
    u32 sign = bits >> 31;
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x42aeac50u) {
        if (magnitude > 0x7f800000u)
            return value;
        if (magnitude >= 0x42b17218u && sign == 0) {
            value *= 1.7014118346046923e38f;
            return value;
        }
        if (sign && magnitude >= 0x42cff1b5u)
            return 0;
    }

    // Reduce the argument to the primary interval around zero.
    s32 exponent;
    float high;
    float low;
    if (magnitude > 0x3eb17218u) {
        exponent = magnitude > 0x3f851592u ? static_cast<s32>(inverseLn2 * value + half[sign])
                                           : 1 - static_cast<s32>(sign) * 2;
        high = value - exponent * ln2High;
        low = exponent * ln2Low;
        value = high - low;
    } else if (magnitude > 0x39000000u) {
        exponent = 0;
        high = value;
        low = 0;
    } else {
        return 1 + value;
    }

    // Evaluate exp on the primary interval and restore the power of two.
    float square = value * value;
    float correction = value - square * (P1 + square * P2);
    float result = 1 + (value * correction / (2 - correction) - low + high);
    return exponent == 0 ? result : scaleFloatByPowerOfTwo(result, exponent);
}

// Computes the single-precision natural logarithm after normalizing around one.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/logf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n22
float log(float value) {
    static constexpr float ln2High = 6.9313812256e-01f;
    static constexpr float ln2Low = 9.0580006145e-06f;
    static constexpr float coefficients[] = {
        0.66666662693f,
        0.40000972152f,
        0.28498786688f,
        0.24279078841f,
    };

    // Handle zero, negative and nonfinite values, and normalize subnormals.
    u32 bits = getFloatBits(value);
    s32 exponent = 0;
    if (bits < 0x00800000u || bits >> 31) {
        if (bits << 1 == 0)
            return -1 / (value * value);
        if (bits >> 31)
            return (value - value) / 0.f;
        exponent -= 25;
        value *= 33554432.f;
        bits = getFloatBits(value);
    } else if (bits >= 0x7f800000u) {
        return value;
    } else if (bits == 0x3f800000u) {
        return 0;
    }

    // Reduce the significand to [sqrt(2)/2, sqrt(2)].
    bits += 0x3f800000u - 0x3f3504f3u;
    exponent += static_cast<s32>(bits >> 23) - 127;
    bits = (bits & 0x007fffffu) + 0x3f3504f3u;
    value = makeFloat(bits);

    // Approximate log1p and add the exponent times ln(2).
    float f = value - 1;
    float s = f / (2 + f);
    float z = s * s;
    float w = z * z;
    float t1 = w * (coefficients[1] + w * coefficients[3]);
    float t2 = z * (coefficients[0] + w * coefficients[2]);
    float polynomial = t1 + t2;
    float halfSquare = 0.5f * f * f;
    float exponentAsFloat = static_cast<float>(exponent);
    return s * (halfSquare + polynomial) + exponentAsFloat * ln2Low - halfSquare + f + exponentAsFloat * ln2High;
}

// Computes a single-precision power as 2^(exponent*log2(base)).
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/powf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n48
float pow(float base, float exponent) {
    static constexpr float bp[] = {1, 1.5f};
    static constexpr float dpHigh[] = {0, 5.84960938e-01f};
    static constexpr float dpLow[] = {0, 1.56322085e-06f};
    static constexpr float L1 = 6.0000002384e-01f;
    static constexpr float L2 = 4.2857143283e-01f;
    static constexpr float L3 = 3.3333334327e-01f;
    static constexpr float L4 = 2.7272811532e-01f;
    static constexpr float L5 = 2.3066075146e-01f;
    static constexpr float L6 = 2.0697501302e-01f;
    static constexpr float P1 = 1.6666667163e-01f;
    static constexpr float P2 = -2.7777778450e-03f;
    static constexpr float P3 = 6.6137559770e-05f;
    static constexpr float P4 = -1.6533901999e-06f;
    static constexpr float P5 = 4.1381369442e-08f;
    static constexpr float ln2 = 6.9314718246e-01f;
    static constexpr float ln2High = 6.93145752e-01f;
    static constexpr float ln2Low = 1.42860654e-06f;
    static constexpr float overflowTail = 4.2995665694e-08f;
    static constexpr float cp = 9.6179670095e-01f;
    static constexpr float cpHigh = 9.6191406250e-01f;
    static constexpr float cpLow = -1.1736857402e-04f;
    static constexpr float inverseLn2 = 1.4426950216e+00f;
    static constexpr float inverseLn2High = 1.4426879883e+00f;
    static constexpr float inverseLn2Low = 7.0526075433e-06f;

    // Handle zero exponents, unit bases and NaNs.
    s32 baseBits = static_cast<s32>(getFloatBits(base));
    s32 exponentBits = static_cast<s32>(getFloatBits(exponent));
    u32 baseMagnitude = static_cast<u32>(baseBits) & 0x7fffffffu;
    u32 exponentMagnitude = static_cast<u32>(exponentBits) & 0x7fffffffu;
    if (exponentMagnitude == 0 || baseBits == 0x3f800000)
        return 1;
    if (baseMagnitude > 0x7f800000u || exponentMagnitude > 0x7f800000u)
        return base + exponent;

    // Classify an exponent of a negative base as noninteger, odd or even.
    s32 exponentInteger = 0;
    if (baseBits < 0) {
        if (exponentMagnitude >= 0x4b800000u) {
            exponentInteger = 2;
        } else if (exponentMagnitude >= 0x3f800000u) {
            s32 shift = static_cast<s32>(exponentMagnitude >> 23) - 127;
            s32 integer = static_cast<s32>(exponentMagnitude >> (23 - shift));
            if ((static_cast<u32>(integer) << (23 - shift)) == exponentMagnitude)
                exponentInteger = 2 - (integer & 1);
        }
    }

    // Handle special exponent values.
    if (exponentMagnitude == 0x7f800000u) {
        if (baseMagnitude == 0x3f800000u)
            return 1;
        if (baseMagnitude > 0x3f800000u)
            return exponentBits >= 0 ? exponent : 0;
        return exponentBits >= 0 ? 0 : -exponent;
    }
    if (exponentMagnitude == 0x3f800000u)
        return exponentBits >= 0 ? base : 1 / base;
    if (exponentBits == 0x40000000)
        return base * base;
    if (exponentBits == 0x3f000000 && baseBits >= 0)
        return sqrt(base);

    // Handle zero, infinite and unit bases.
    float absoluteBase = baseBits < 0 ? -base : base;
    if (baseMagnitude == 0x7f800000u || baseMagnitude == 0 || baseMagnitude == 0x3f800000u) {
        float result = exponentBits < 0 ? 1 / absoluteBase : absoluteBase;
        if (baseBits < 0) {
            if (((baseMagnitude - 0x3f800000u) | static_cast<u32>(exponentInteger)) == 0)
                result = (result - result) / (result - result);
            else if (exponentInteger == 1)
                result = -result;
        }
        return result;
    }

    // Determine the result sign or reject a negative base with noninteger exponent.
    float sign = 1;
    if (baseBits < 0) {
        if (exponentInteger == 0)
            return (base - base) / (base - base);
        if (exponentInteger == 1)
            sign = -1;
    }

    // Compute log2(base) as a high- and low-part sum.
    float t1;
    float t2;
    if (exponentMagnitude > 0x4d000000u) {
        if (baseMagnitude < 0x3f7ffff8u)
            return exponentBits < 0 ? sign * 1e30f * 1e30f : sign * 1e-30f * 1e-30f;
        if (baseMagnitude > 0x3f800007u)
            return exponentBits > 0 ? sign * 1e30f * 1e30f : sign * 1e-30f * 1e-30f;

        // Use a short log1p series when a huge exponent forces the base near one.
        float t = absoluteBase - 1;
        float w = t * t * (0.5f - t * (0.333333333333f - t * 0.25f));
        float u = inverseLn2High * t;
        float v = t * inverseLn2Low - w * inverseLn2;
        t1 = makeFloat(getFloatBits(u + v) & 0xfffff000u);
        t2 = v - (t1 - u);
    } else {
        // Normalize the base and select an interval centered at 1 or 1.5.
        s32 normalizationExponent = 0;
        u32 normalizedBits = baseMagnitude;
        if (normalizedBits < 0x00800000u) {
            absoluteBase *= 16777216.f;
            normalizationExponent -= 24;
            normalizedBits = getFloatBits(absoluteBase);
        }
        normalizationExponent += static_cast<s32>(normalizedBits >> 23) - 127;
        u32 fraction = normalizedBits & 0x007fffffu;
        s32 interval;
        normalizedBits = fraction | 0x3f800000u;
        if (fraction <= 0x1cc471u) {
            interval = 0;
        } else if (fraction < 0x5db3d7u) {
            interval = 1;
        } else {
            interval = 0;
            normalizationExponent++;
            normalizedBits -= 0x00800000u;
        }

        // Approximate log2 of the normalized base with split arithmetic.
        absoluteBase = makeFloat(normalizedBits);
        float numerator = absoluteBase - bp[interval];
        float reciprocal = 1 / (absoluteBase + bp[interval]);
        float s = numerator * reciprocal;
        float sHigh = makeFloat(getFloatBits(s) & 0xfffff000u);
        u32 temporaryBits = ((normalizedBits >> 1) & 0xfffff000u) | 0x20000000u;
        float tHigh = makeFloat(temporaryBits + 0x00400000u + (static_cast<u32>(interval) << 21));
        float tLow = absoluteBase - (tHigh - bp[interval]);
        float sLow = reciprocal * ((numerator - sHigh * tHigh) - sHigh * tLow);
        float s2 = s * s;
        float r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += sLow * (sHigh + s);
        s2 = sHigh * sHigh;
        tHigh = 3 + s2 + r;
        tHigh = makeFloat(getFloatBits(tHigh) & 0xfffff000u);
        tLow = r - ((tHigh - 3) - s2);
        float u = sHigh * tHigh;
        float v = sLow * tHigh + tLow * s;
        float pHigh = makeFloat(getFloatBits(u + v) & 0xfffff000u);
        float pLow = v - (pHigh - u);
        float zHigh = cpHigh * pHigh;
        float zLow = cpLow * pHigh + pLow * cp + dpLow[interval];
        float normalizedExponent = static_cast<float>(normalizationExponent);
        t1 = ((zHigh + zLow) + dpHigh[interval]) + normalizedExponent;
        t1 = makeFloat(getFloatBits(t1) & 0xfffff000u);
        t2 = zLow - (((t1 - normalizedExponent) - dpHigh[interval]) - zHigh);
    }

    // Multiply the split logarithm by the split exponent.
    float exponentHigh = makeFloat(getFloatBits(exponent) & 0xfffff000u);
    float productLow = (exponent - exponentHigh) * t1 + exponent * t2;
    float productHigh = exponentHigh * t1;
    float product = productLow + productHigh;
    s32 productBits = static_cast<s32>(getFloatBits(product));
    if (productBits > 0x43000000)
        return sign * 1e30f * 1e30f;
    if (productBits == 0x43000000 && productLow + overflowTail > product - productHigh)
        return sign * 1e30f * 1e30f;
    if ((static_cast<u32>(productBits) & 0x7fffffffu) > 0x43160000u)
        return sign * 1e-30f * 1e-30f;
    if (static_cast<u32>(productBits) == 0xc3160000u && productLow <= product - productHigh)
        return sign * 1e-30f * 1e-30f;

    // Split off an integer exponent for the final power-of-two scaling.
    u32 magnitude = static_cast<u32>(productBits) & 0x7fffffffu;
    s32 magnitudeExponent = static_cast<s32>(magnitude >> 23) - 127;
    s32 integerExponent = 0;
    if (magnitude > 0x3f000000u) {
        u32 roundedBits = static_cast<u32>(productBits) + (0x00800000u >> (magnitudeExponent + 1));
        magnitudeExponent = static_cast<s32>((roundedBits & 0x7fffffffu) >> 23) - 127;
        float rounded = makeFloat(roundedBits & ~(0x007fffffu >> magnitudeExponent));
        integerExponent = static_cast<s32>(((roundedBits & 0x007fffffu) | 0x00800000u) >> (23 - magnitudeExponent));
        if (productBits < 0) {
            integerExponent = -integerExponent;
        }
        productHigh -= rounded;
    }

    // Evaluate 2^fraction and restore the integer exponent.
    float t = productLow + productHigh;
    t = makeFloat(getFloatBits(t) & 0xffff8000u);
    float u = t * ln2High;
    float v = (productLow - (t - productHigh)) * ln2 + t * ln2Low;
    float z = u + v;
    float w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    float r = (z * t1) / (t1 - 2) - (w + z * w);
    z = 1 - (r - z);
    s32 zBits = static_cast<s32>(getFloatBits(z));
    s32 resultExponent = static_cast<s32>(static_cast<u32>(zBits) >> 23) + integerExponent;
    if (resultExponent <= 0) {
        z = scaleFloatByPowerOfTwo(z, integerExponent);
    } else {
        z = makeFloat(static_cast<u32>(zBits) + (static_cast<u32>(integerExponent) << 23));
    }
    return sign * z;
}

// Computes single-precision sine with fast paths for small multiples of pi/2.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/sinf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n26
float sin(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    // Evaluate the primary interval directly.
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return rad;
        return sinKernel(rad);
    }
    // Reduce small multiples of pi/2 directly in double precision.
    if (magnitude <= 0x40e231d5u) {
        float absolute = negative ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = (quadrant & 1) ? cosKernel(reduced) : sinKernel(reduced);
        if (quadrant & 2)
            result = -result;
        return negative ? -result : result;
    }
    // Reject nonfinite inputs, then reduce general finite angles.
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    switch (quadrant & 3) {
        case 0:
            return sinKernel(reduced);
        case 1:
            return cosKernel(reduced);
        case 2:
            return sinKernel(-reduced);
        default:
            return -cosKernel(reduced);
    }
}

// Computes single-precision cosine with fast paths for small multiples of pi/2.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/cosf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n26
float cos(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    // Evaluate the primary interval directly.
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return 1;
        return cosKernel(rad);
    }
    // Reduce small multiples of pi/2 directly in double precision.
    if (magnitude <= 0x40e231d5u) {
        double absolute = (bits >> 31) ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = (quadrant & 1) ? -sinKernel(reduced) : cosKernel(reduced);
        return (quadrant & 2) ? -result : result;
    }
    // Reject nonfinite inputs, then reduce general finite angles.
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    switch (quadrant & 3) {
        case 0:
            return cosKernel(reduced);
        case 1:
            return sinKernel(-reduced);
        case 2:
            return -cosKernel(reduced);
        default:
            return sinKernel(reduced);
    }
}

// Computes single-precision tangent with fast paths for small multiples of pi/2.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/tanf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n26
float tan(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    // Evaluate the primary interval directly.
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return rad;
        return tanKernel(rad, false);
    }
    // Reduce small multiples of pi/2 directly in double precision.
    if (magnitude <= 0x40e231d5u) {
        double absolute = negative ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = tanKernel(reduced, (quadrant & 1) != 0);
        return negative ? -result : result;
    }
    // Reject nonfinite inputs, then reduce general finite angles.
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    return tanKernel(reduced, (quadrant & 1) != 0);
}

// Computes cosine and sine together so argument reduction is performed only once.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/sincosf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n27
Float2 cosAndSin(float rad) {
    u32 magnitude = getFloatBits(rad) & 0x7fffffffu;
    // Evaluate small inputs directly and reject nonfinite inputs.
    if (magnitude <= 0x3f490fdau)
        return {cosKernel(rad), magnitude < 0x39800000u ? rad : sinKernel(rad)};
    if (magnitude >= 0x7f800000u) {
        float nan = rad - rad;
        return {nan, nan};
    }
    // Reduce once, evaluate both kernels and map them to the original quadrant.
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    float sine = sinKernel(reduced);
    float cosine = cosKernel(reduced);
    switch (quadrant & 3) {
        case 0:
            return {cosine, sine};
        case 1:
            return {-sine, cosine};
        case 2:
            return {-cosine, -sine};
        default:
            return {sine, -cosine};
    }
}

// Reduces the argument to a short interval before evaluating an odd polynomial.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/atanf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n41
float arctan(float value) {
    static constexpr float high[] = {
        4.6364760399e-01f,
        7.8539812565e-01f,
        9.8279368877e-01f,
        1.5707962513e+00f,
    };
    static constexpr float low[] = {
        5.0121582440e-09f,
        3.7748947079e-08f,
        3.4473217170e-08f,
        7.5497894159e-08f,
    };
    static constexpr float coefficients[] = {
        3.3333328366e-01f, -1.9999158382e-01f, 1.4253635705e-01f, -1.0648017377e-01f, 6.1687607318e-02f,
    };

    // Handle very large values, infinities and NaNs.
    u32 bits = getFloatBits(value);
    bool negative = (bits >> 31) != 0;
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x4c800000u) {
        if (magnitude > 0x7f800000u)
            return value;
        float result = high[3] + 7.52316384526264e-37f;
        return negative ? -result : result;
    }

    // Reduce the magnitude to an interval near zero.
    s32 interval;
    if (magnitude < 0x3ee00000u) {
        if (magnitude < 0x39800000u)
            return value;
        interval = -1;
    } else {
        value = negative ? -value : value;
        if (magnitude < 0x3f980000u) {
            if (magnitude < 0x3f300000u) {
                interval = 0;
                value = (2 * value - 1) / (2 + value);
            } else {
                interval = 1;
                value = (value - 1) / (value + 1);
            }
        } else if (magnitude < 0x401c0000u) {
            interval = 2;
            value = (value - 1.5f) / (1 + 1.5f * value);
        } else {
            interval = 3;
            value = -1 / value;
        }
    }
    // Evaluate the odd and even polynomial chains and undo the reduction.
    float z = value * value;
    float w = z * z;
    float odd = z * (coefficients[0] + w * (coefficients[2] + w * coefficients[4]));
    float even = w * (coefficients[1] + w * coefficients[3]);
    if (interval < 0)
        return value - value * (odd + even);
    z = high[interval] - ((value * (odd + even) - low[interval]) - value);
    return negative ? -z : z;
}

// Computes atan2(y, x), including signed-zero and infinity cases.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/atan2f.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n22
float arctan(const Float2& pos) {
    static constexpr float pi = 3.1415927410e+00f;
    static constexpr float piLow = -8.7422776573e-08f;
    float x = pos.x;
    float y = pos.y;
    u32 xBits = getFloatBits(x);
    u32 yBits = getFloatBits(y);
    u32 xMagnitude = xBits & 0x7fffffffu;
    u32 yMagnitude = yBits & 0x7fffffffu;
    // Handle NaNs and the common x=1 case.
    if (xMagnitude > 0x7f800000u || yMagnitude > 0x7f800000u)
        return x + y;
    if (xBits == 0x3f800000u)
        return arctan(y);
    // Encode the signs as a quadrant and handle zero or infinite components.
    u32 quadrant = ((yBits >> 31) & 1) | ((xBits >> 30) & 2);
    if (yMagnitude == 0) {
        if (quadrant < 2)
            return y;
        return quadrant == 2 ? pi : -pi;
    }
    if (xMagnitude == 0)
        return quadrant & 1 ? -pi / 2 : pi / 2;
    if (xMagnitude == 0x7f800000u) {
        if (yMagnitude == 0x7f800000u) {
            static constexpr float results[] = {pi / 4, -pi / 4, 3 * pi / 4, -3 * pi / 4};
            return results[quadrant];
        }
        if (quadrant < 2)
            return makeFloat(quadrant << 31);
        return quadrant == 2 ? pi : -pi;
    }
    // Avoid a division when the magnitude ratio is extreme.
    if (xMagnitude + (26u << 23) < yMagnitude || yMagnitude == 0x7f800000u)
        return quadrant & 1 ? -pi / 2 : pi / 2;
    // Evaluate the positive ratio and map it to the encoded quadrant.
    float z = (quadrant & 2) && yMagnitude + (26u << 23) < xMagnitude ? 0 : arctan(abs(y / x));
    switch (quadrant) {
        case 0:
            return z;
        case 1:
            return -z;
        case 2:
            return pi - (z - piLow);
        default:
            return (z - piLow) - pi;
    }
}

static float inverseSinRatio(float z) {
    static constexpr float P0 = 1.6666586697e-01f;
    static constexpr float P1 = -4.2743422091e-02f;
    static constexpr float P2 = -8.6563630030e-03f;
    static constexpr float Q1 = -7.0662963390e-01f;
    return z * (P0 + z * (P1 + z * P2)) / (1 + z * Q1);
}

// Computes single-precision arcsine using a rational correction term.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/asinf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n35
float arcsin(float value) {
    static constexpr double piOverTwo = 1.570796326794896558;
    u32 bits = getFloatBits(value);
    u32 magnitude = bits & 0x7fffffffu;
    // Handle endpoints and values outside the domain.
    if (magnitude >= 0x3f800000u) {
        if (magnitude == 0x3f800000u)
            return static_cast<float>(value * piOverTwo + 7.52316384526264e-37f);
        return (value - value) / (value - value);
    }
    // Evaluate small magnitudes directly.
    if (magnitude < 0x3f000000u) {
        if (magnitude < 0x39800000u && magnitude >= 0x00800000u)
            return value;
        return value + value * inverseSinRatio(value * value);
    }
    // Transform larger magnitudes through sqrt((1-|x|)/2).
    float z = (1 - abs(value)) * 0.5f;
    double root = sqrt(z);
    float result = static_cast<float>(piOverTwo - 2 * (root + root * inverseSinRatio(z)));
    return bits >> 31 ? -result : result;
}

// Computes single-precision arccosine using a rational correction term.
// Details: https://git.musl-libc.org/cgit/musl/tree/src/math/acosf.c?id=8fe1f2d79b275b7f7fb0d41c99e379357df63cd9#n34
float arccos(float value) {
    static constexpr float piOverTwoHigh = 1.5707962513e+00f;
    static constexpr float piOverTwoLow = 7.5497894159e-08f;
    u32 bits = getFloatBits(value);
    u32 magnitude = bits & 0x7fffffffu;
    // Handle endpoints and values outside the domain.
    if (magnitude >= 0x3f800000u) {
        if (magnitude == 0x3f800000u)
            return bits >> 31 ? 2 * piOverTwoHigh + 7.52316384526264e-37f : 0;
        return (value - value) / (value - value);
    }
    // Evaluate the central interval around zero.
    if (magnitude < 0x3f000000u) {
        if (magnitude <= 0x32800000u)
            return piOverTwoHigh + 7.52316384526264e-37f;
        return piOverTwoHigh - (value - (piOverTwoLow - value * inverseSinRatio(value * value)));
    }
    // Transform the negative outer interval through sqrt((1+x)/2).
    if (bits >> 31) {
        float z = (1 + value) * 0.5f;
        float root = sqrt(z);
        float correction = inverseSinRatio(z) * root - piOverTwoLow;
        return 2 * (piOverTwoHigh - (root + correction));
    }
    // Transform the positive outer interval while retaining extra root precision.
    float z = (1 - value) * 0.5f;
    float root = sqrt(z);
    float truncatedRoot = makeFloat(getFloatBits(root) & 0xfffff000u);
    float correction = (z - truncatedRoot * truncatedRoot) / (root + truncatedRoot);
    float remainder = inverseSinRatio(z) * root + correction;
    return 2 * (truncatedRoot + remainder);
}

static double doubleAbsolute(double value) {
    return makeDouble(getDoubleBits(value) & 0x7fffffffffffffffull);
}

static bool doubleIsNan(double value) {
    return (getDoubleBits(value) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
}

static double scaleDoubleByPowerOfTwo(double value, s32 exponent) {
    double result = value;
    if (exponent > 1023) {
        result *= 8.98846567431158e307;
        exponent -= 1023;
        if (exponent > 1023) {
            result *= 8.98846567431158e307;
            exponent -= 1023;
            if (exponent > 1023) {
                exponent = 1023;
            }
        }
    } else if (exponent < -1022) {
        result *= 2.2250738585072014e-308 * 9007199254740992.0;
        exponent += 1022 - 53;
        if (exponent < -1022) {
            result *= 2.2250738585072014e-308 * 9007199254740992.0;
            exponent += 1022 - 53;
            if (exponent < -1022) {
                exponent = -1022;
            }
        }
    }
    return result * makeDouble(static_cast<u64>(1023 + exponent) << 52);
}

static void forceEval(double value) {
    volatile double result = value;
    (void) result;
}

static u32 getDoubleHighWord(double value) {
    return static_cast<u32>(getDoubleBits(value) >> 32);
}

static u32 getDoubleLowWord(double value) {
    return static_cast<u32>(getDoubleBits(value));
}

static double setDoubleHighWord(double value, u32 high) {
    return makeDouble((getDoubleBits(value) & 0xffffffffull) | (static_cast<u64>(high) << 32));
}

static double setDoubleLowWord(double value, u32 low) {
    return makeDouble((getDoubleBits(value) & 0xffffffff00000000ull) | low);
}

// Reduces huge angles using 24-bit chunks of 2/pi.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/k_rem_pio2.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
static s32 reduceDoubleLarge(double* x, double* y, s32 exponent, s32 numInputTerms) {
    static constexpr s32 twoOverPi[] = {
        0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7,
        0x246E3A, 0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C,
        0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11,
        0x8B5A0A, 0x6D1F6D, 0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7,
        0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E,
        0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
    };

    static constexpr double piOverTwoChunks[] = {1.57079625129699707031e+00, 7.54978941586159635335e-08,
                                                 5.39030252995776476554e-15, 3.28200341580791294123e-22,
                                                 1.27065575308067607349e-29};

    s32 jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
    double z, fw, f[20], fq[20], q[20];

    // Select the working precision and the needed window of 2/pi chunks.
    jk = jp = 4;

    jx = numInputTerms - 1;
    jv = (exponent - 3) / 24;
    if (jv < 0) {
        jv = 0;
    }
    q0 = exponent - 24 * (jv + 1);

    // Set up the aligned 2/pi chunks used by the initial convolution.
    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++) {
        f[i] = j < 0 ? 0.0 : static_cast<double>(twoOverPi[j]);
    }

    // Compute the initial product terms q[0] through q[jk].
    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0; j <= jx; j++) {
            fw += x[j] * f[jx + i - j];
        }
        q[i] = fw;
    }

    jz = jk;
recompute:
    // Distill q[] into iq[] in reverse order.
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw = static_cast<double>(static_cast<s32>(5.9604644775390625e-8 * z));
        iq[i] = static_cast<s32>(z - 16777216.0 * fw);
        z = q[j - 1] + fw;
    }

    // Extract the quadrant and the remaining fractional part.
    z = scaleDoubleByPowerOfTwo(z, q0);
    z -= 8.0 * roundDown(z * 0.125);
    n = static_cast<s32>(z);
    z -= static_cast<double>(n);
    ih = 0;
    if (q0 > 0) {
        i = iq[jz - 1] >> (24 - q0);
        n += i;
        iq[jz - 1] -= i << (24 - q0);
        ih = iq[jz - 1] >> (23 - q0);
    } else if (q0 == 0) {
        ih = iq[jz - 1] >> 23;
    } else if (z >= 0.5) {
        ih = 2;
    }

    // Complement fractions above one half so the remainder stays small.
    if (ih > 0) {
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) {
            j = iq[i];
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    iq[i] = 0x1000000 - j;
                }
            } else {
                iq[i] = 0xffffff - j;
            }
        }
        if (q0 > 0) {
            switch (q0) {
                case 1:
                    iq[jz - 1] &= 0x7fffff;
                    break;
                case 2:
                    iq[jz - 1] &= 0x3fffff;
                    break;
            }
        }
        if (ih == 2) {
            z = 1.0 - z;
            if (carry != 0) {
                z -= scaleDoubleByPowerOfTwo(1.0, q0);
            }
        }
    }

    // Recompute with more terms if cancellation exhausted the known digits.
    if (z == 0.0) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) {
            j |= iq[i];
        }
        if (j == 0) {
            k = 1;
            while (iq[jk - k] == 0) {
                k++;
            }

            for (i = jz + 1; i <= jz + k; i++) {
                f[jx + i] = static_cast<double>(twoOverPi[jv + i]);
                for (j = 0, fw = 0.0; j <= jx; j++) {
                    fw += x[j] * f[jx + i - j];
                }
                q[i] = fw;
            }
            jz += k;
            goto recompute;
        }
    }

    // Remove zero chunks or split the residual into 24-bit chunks.
    if (z == 0.0) {
        jz -= 1;
        q0 -= 24;
        while (iq[jz] == 0) {
            jz--;
            q0 -= 24;
        }
    } else {
        z = scaleDoubleByPowerOfTwo(z, -q0);
        if (z >= 16777216.0) {
            fw = static_cast<double>(static_cast<s32>(5.9604644775390625e-8 * z));
            iq[jz] = static_cast<s32>(z - 16777216.0 * fw);
            jz += 1;
            q0 += 24;
            iq[jz] = static_cast<s32>(fw);
        } else {
            iq[jz] = static_cast<s32>(z);
        }
    }

    // Convert the integer chunks back to floating-point values.
    fw = scaleDoubleByPowerOfTwo(1.0, q0);
    for (i = jz; i >= 0; i--) {
        q[i] = fw * static_cast<double>(iq[i]);
        fw *= 5.9604644775390625e-8;
    }

    // Convolve the chunks with the split representation of pi/2.
    for (i = jz; i >= 0; i--) {
        for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) {
            fw += piOverTwoChunks[k] * q[i + k];
        }
        fq[jz - i] = fw;
    }

    // Compress the convolution into a high- and low-part remainder.
    fw = 0.0;
    for (i = jz; i >= 0; i--) {
        fw += fq[i];
    }
    // Round the accumulated remainder to binary64 precision.
    fw = static_cast<double>(fw);
    y[0] = ih == 0 ? fw : -fw;
    fw = fq[0] - fw;
    for (i = 1; i <= jz; i++) {
        fw += fq[i];
    }
    y[1] = ih == 0 ? fw : -fw;
    return n & 7;
}

// Reduces a double angle to a high- and low-part remainder modulo pi/2.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_rem_pio2.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n15
static s32 reduceDouble(double x, double* y) {
#if FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1
    static constexpr double epsilon = DBL_EPSILON;
#elif FLT_EVAL_METHOD == 2
    static constexpr double epsilon = LDBL_EPSILON;
#endif

    static constexpr double roundToInteger = 1.5 / epsilon;
    static constexpr double inversePiOverTwo = 6.36619772367581382433e-01;
    static constexpr double piOverTwo1 = 1.57079632673412561417e+00;
    static constexpr double piOverTwo1Tail = 6.07710050650619224932e-11;
    static constexpr double piOverTwo2 = 6.07710050630396597660e-11;
    static constexpr double piOverTwo2Tail = 2.02226624879595063154e-21;
    static constexpr double piOverTwo3 = 2.02226624871116645580e-21;
    static constexpr double piOverTwo3Tail = 8.47842766036889956997e-32;

    u64 bits = getDoubleBits(x);
    double z, w, t, r, fn;
    double tx[3], ty[2];
    u32 ix;
    s32 sign, n, ex, ey, i;

    // Reduce values through 9pi/4 directly with one split of pi/2.
    sign = bits >> 63;
    ix = bits >> 32 & 0x7fffffff;
    if (ix <= 0x400f6a7a) {
        if ((ix & 0xfffff) == 0x921fb) {
            goto medium;
        }
        if (ix <= 0x4002d97c) {
            if (!sign) {
                z = x - piOverTwo1;
                y[0] = z - piOverTwo1Tail;
                y[1] = (z - y[0]) - piOverTwo1Tail;
                return 1;
            } else {
                z = x + piOverTwo1;
                y[0] = z + piOverTwo1Tail;
                y[1] = (z - y[0]) + piOverTwo1Tail;
                return -1;
            }
        } else {
            if (!sign) {
                z = x - 2 * piOverTwo1;
                y[0] = z - 2 * piOverTwo1Tail;
                y[1] = (z - y[0]) - 2 * piOverTwo1Tail;
                return 2;
            } else {
                z = x + 2 * piOverTwo1;
                y[0] = z + 2 * piOverTwo1Tail;
                y[1] = (z - y[0]) + 2 * piOverTwo1Tail;
                return -2;
            }
        }
    }
    if (ix <= 0x401c463b) {
        if (ix <= 0x4015fdbc) {
            if (ix == 0x4012d97c) {
                goto medium;
            }
            if (!sign) {
                z = x - 3 * piOverTwo1;
                y[0] = z - 3 * piOverTwo1Tail;
                y[1] = (z - y[0]) - 3 * piOverTwo1Tail;
                return 3;
            } else {
                z = x + 3 * piOverTwo1;
                y[0] = z + 3 * piOverTwo1Tail;
                y[1] = (z - y[0]) + 3 * piOverTwo1Tail;
                return -3;
            }
        } else {
            if (ix == 0x401921fb) {
                goto medium;
            }
            if (!sign) {
                z = x - 4 * piOverTwo1;
                y[0] = z - 4 * piOverTwo1Tail;
                y[1] = (z - y[0]) - 4 * piOverTwo1Tail;
                return 4;
            } else {
                z = x + 4 * piOverTwo1;
                y[0] = z + 4 * piOverTwo1Tail;
                y[1] = (z - y[0]) + 4 * piOverTwo1Tail;
                return -4;
            }
        }
    }
    if (ix < 0x413921fb) {
    medium:
        // Reduce medium values, adding more pi/2 tails after cancellation.
        fn = x * inversePiOverTwo + roundToInteger - roundToInteger;
        n = static_cast<s32>(fn);
        r = x - fn * piOverTwo1;
        w = fn * piOverTwo1Tail;
        y[0] = r - w;
        ey = getDoubleBits(y[0]) >> 52 & 0x7ff;
        ex = ix >> 20;
        if (ex - ey > 16) {
            t = r;
            w = fn * piOverTwo2;
            r = t - w;
            w = fn * piOverTwo2Tail - ((t - r) - w);
            y[0] = r - w;
            ey = getDoubleBits(y[0]) >> 52 & 0x7ff;
            if (ex - ey > 49) {
                t = r;
                w = fn * piOverTwo3;
                r = t - w;
                w = fn * piOverTwo3Tail - ((t - r) - w);
                y[0] = r - w;
            }
        }
        y[1] = (r - y[0]) - w;
        return n;
    }

    // Map infinities and NaNs to NaN remainders.
    if (ix >= 0x7ff00000) {
        y[0] = y[1] = x - x;
        return 0;
    }

    // Split a huge magnitude into 24-bit chunks for the large reducer.
    bits = (getDoubleBits(x) & (static_cast<u64>(-1) >> 12)) | (static_cast<u64>(0x3ff + 23) << 52);
    z = makeDouble(bits);
    for (i = 0; i < 2; i++) {
        tx[i] = static_cast<double>(static_cast<s32>(z));
        z = (z - tx[i]) * 16777216.0;
    }
    tx[i] = z;

    // Skip trailing zero chunks and restore the original sign afterward.
    while (tx[i] == 0.0) {
        i--;
    }
    n = reduceDoubleLarge(tx, ty, static_cast<s32>(ix >> 20) - (0x3ff + 23), i + 1);
    if (sign) {
        y[0] = -ty[0];
        y[1] = -ty[1];
        return -n;
    }
    y[0] = ty[0];
    y[1] = ty[1];
    return n;
}

// Approximates sine on the primary interval.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/k_sin.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
static double sinDoubleKernel(double x, double y, s32 iy) {
    static constexpr double S1 = -1.66666666666666324348e-01;
    static constexpr double S2 = 8.33333333332248946124e-03;
    static constexpr double S3 = -1.98412698298579493134e-04;
    static constexpr double S4 = 2.75573137070700676789e-06;
    static constexpr double S5 = -2.50507602534068634195e-08;
    static constexpr double S6 = 1.58969099521155010221e-10;

    double z, r, v, w;

    // Evaluate the odd sine polynomial and apply the optional tail correction.
    z = x * x;
    w = z * z;
    r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    v = z * x;
    if (iy == 0)
        return x + v * (S1 + z * r);
    else
        return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

// Approximates cosine on the primary interval.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/k_cos.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
static double cosDoubleKernel(double x, double y) {
    static constexpr double C1 = 4.16666666666666019037e-02;
    static constexpr double C2 = -1.38888888888741095749e-03;
    static constexpr double C3 = 2.48015872894767294178e-05;
    static constexpr double C4 = -2.75573143513906633035e-07;
    static constexpr double C5 = 2.08757232129817482790e-09;
    static constexpr double C6 = -1.13596475577881948265e-11;

    double hz, z, r, w;

    // Evaluate the cosine polynomial while preserving the leading subtraction.
    z = x * x;
    w = z * z;
    r = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    hz = 0.5 * z;
    w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

// Approximates tangent or its negated reciprocal on the primary interval.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/k_tan.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n11
static double tanDoubleKernel(double x, double y, s32 odd) {
    static constexpr double T[] = {
        3.33333333333334091986e-01, 1.33333333333201242699e-01, 5.39682539762260521377e-02, 2.18694882948595424599e-02,
        8.86323982359930005737e-03, 3.59207910759131235356e-03, 1.45620945432529025516e-03, 5.88041240820264096874e-04,
        2.46463134818469906812e-04, 7.81794442939557092300e-05, 7.14072491382608190305e-05, -1.85586374855275456654e-05,
        2.59073051863633712884e-05,
    };
    static constexpr double pio4 = 7.85398163397448278999e-01;
    static constexpr double pio4lo = 3.06161699786838301793e-17;

    double z, r, v, w, s, a;
    double w0, a0;
    u32 hx;
    s32 big, sign;

    // Reflect arguments near pi/4 to improve the polynomial's accuracy.
    hx = getDoubleHighWord(x);
    big = (hx & 0x7fffffff) >= 0x3FE59428;
    if (big) {
        sign = hx >> 31;
        if (sign) {
            x = -x;
            y = -y;
        }
        x = (pio4 - x) + (pio4lo - y);
        y = 0.0;
    }

    // Evaluate the tangent polynomial on the reduced argument.
    z = x * x;
    w = z * z;

    r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
    v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
    s = z * x;
    r = y + z * (s * (r + v) + y) + s * T[0];
    w = x + r;

    // Undo the pi/4 reflection when one was applied.
    if (big) {
        s = 1 - 2 * odd;
        v = s - 2.0 * (x + (r - w * w / (w + s)));
        return sign ? -v : v;
    }
    if (!odd)
        return w;

    // Compute the negated reciprocal with a split product correction.
    w0 = w;
    w0 = setDoubleLowWord(w0, 0);
    v = r - (w0 - x);
    a0 = a = -1.0 / w;
    a0 = setDoubleLowWord(a0, 0);
    return a0 + a * (1.0 + a0 * w0 + a0 * v);
}

// Computes logarithms using argument reduction and a Remez polynomial.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_log.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
double log(double x) {
    static constexpr double ln2High = 6.93147180369123816490e-01;
    static constexpr double ln2Low = 1.90821492927058770002e-10;
    static constexpr double Lg1 = 6.666666666666735130e-01;
    static constexpr double Lg2 = 3.999999999940941908e-01;
    static constexpr double Lg3 = 2.857142874366239149e-01;
    static constexpr double Lg4 = 2.222219843214978396e-01;
    static constexpr double Lg5 = 1.818357216161805012e-01;
    static constexpr double Lg6 = 1.531383769920937332e-01;
    static constexpr double Lg7 = 1.479819860511658591e-01;

    u64 bits = getDoubleBits(x);
    double hfsq, f, s, z, R, w, t1, t2, dk;
    u32 hx;
    s32 k;

    // Handle exceptional values and scale subnormal inputs into range.
    hx = bits >> 32;
    k = 0;
    if (hx < 0x00100000 || hx >> 31) {
        if (bits << 1 == 0)
            return -1 / (x * x);
        if (hx >> 31)
            return (x - x) / 0.0;

        k -= 54;
        x *= 18014398509481984.0;
        bits = getDoubleBits(x);
        hx = bits >> 32;
    } else if (hx >= 0x7ff00000) {
        return x;
    } else if (hx == 0x3ff00000 && bits << 32 == 0)
        return 0;

    // Normalize x to a compact interval around one and record its exponent.
    hx += 0x3ff00000 - 0x3fe6a09e;
    k += static_cast<s32>(hx >> 20) - 0x3ff;
    hx = (hx & 0x000fffff) + 0x3fe6a09e;
    bits = (static_cast<u64>(hx) << 32) | (bits & 0xffffffff);
    x = makeDouble(bits);

    // Approximate log(1 + f) and restore the exponent contribution.
    f = x - 1.0;
    hfsq = 0.5 * f * f;
    s = f / (2.0 + f);
    z = s * s;
    w = z * z;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    R = t2 + t1;
    dk = k;
    return s * (hfsq + R) + dk * ln2Low - hfsq + f + dk * ln2High;
}

// Computes exponentials using ln(2) reduction and a rational approximation.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_exp.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n12
double exp(double x) {
    static constexpr double half[2] = {0.5, -0.5};
    static constexpr double ln2High = 6.93147180369123816490e-01;
    static constexpr double ln2Low = 1.90821492927058770002e-10;
    static constexpr double inverseLn2 = 1.44269504088896338700e+00;
    static constexpr double P1 = 1.66666666666666019037e-01;
    static constexpr double P2 = -2.77777777770155933842e-03;
    static constexpr double P3 = 6.61375632143793436117e-05;
    static constexpr double P4 = -1.65339022054652515390e-06;
    static constexpr double P5 = 4.13813679705723846039e-08;

    double hi, lo, c, xx, y;
    s32 k, sign;
    u32 hx;

    // Filter NaNs and values that may overflow or underflow.
    hx = getDoubleHighWord(x);
    sign = hx >> 31;
    hx &= 0x7fffffff;

    if (hx >= 0x4086232b) {
        if (doubleIsNan(x))
            return x;
        if (x > 709.782712893383973096) {
            x *= 8.98846567431158e307;
            return x;
        }
        if (x < -708.39641853226410622) {
            forceEval(static_cast<float>(-1.401298464324817e-45 / x));
            if (x < -745.13321910194110842)
                return 0;
        }
    }

    // Reduce the argument to within half of ln(2).
    if (hx > 0x3fd62e42) {
        if (hx >= 0x3ff0a2b2) {
            k = static_cast<s32>(inverseLn2 * x + half[sign]);
        } else {
            k = 1 - sign - sign;
        }
        hi = x - k * ln2High;
        lo = k * ln2Low;
        x = hi - lo;
    } else if (hx > 0x3e300000) {
        k = 0;
        hi = x;
        lo = 0;
    } else {
        forceEval(8.98846567431158e307 + x);
        return 1 + x;
    }

    // Evaluate exp on the primary interval and restore the binary exponent.
    xx = x * x;
    c = x - xx * (P1 + xx * (P2 + xx * (P3 + xx * (P4 + xx * P5))));
    y = 1 + (x * c / (2 - c) - lo + hi);
    if (k == 0)
        return y;
    return scaleDoubleByPowerOfTwo(y, k);
}

// Computes powers from split logarithm and exponential approximations.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_pow.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n11
double pow(double x, double y) {
    static constexpr double bp[] = {1.0, 1.5};
    static constexpr double correctionHigh[] = {0.0, 5.84962487220764160156e-01};
    static constexpr double correctionLow[] = {0.0, 1.35003920212974897128e-08};
    static constexpr double two53 = 9007199254740992.0;
    static constexpr double huge = 1.0e300;
    static constexpr double tiny = 1.0e-300;
    static constexpr double L1 = 5.99999999999994648725e-01;
    static constexpr double L2 = 4.28571428578550184252e-01;
    static constexpr double L3 = 3.33333329818377432918e-01;
    static constexpr double L4 = 2.72728123808534006489e-01;
    static constexpr double L5 = 2.30660745775561754067e-01;
    static constexpr double L6 = 2.06975017800338417784e-01;
    static constexpr double P1 = 1.66666666666666019037e-01;
    static constexpr double P2 = -2.77777777770155933842e-03;
    static constexpr double P3 = 6.61375632143793436117e-05;
    static constexpr double P4 = -1.65339022054652515390e-06;
    static constexpr double P5 = 4.13813679705723846039e-08;
    static constexpr double lg2 = 6.93147180559945286227e-01;
    static constexpr double ln2High = 6.93147182464599609375e-01;
    static constexpr double ln2Low = -1.90465429995776804525e-09;
    static constexpr double ovt = 8.0085662595372944372e-017;
    static constexpr double cp = 9.61796693925975554329e-01;
    static constexpr double coefficientHigh = 9.61796700954437255859e-01;
    static constexpr double coefficientLow = -7.02846165095275826516e-09;
    static constexpr double ivln2 = 1.44269504088896338700e+00;
    static constexpr double inverseLn2High = 1.44269502162933349609e+00;
    static constexpr double inverseLn2Low = 1.92596299112661746887e-08;

    double z, ax, zHigh, zLow, productHigh, productLow;
    double y1, t1, t2, r, s, t, u, v, w;
    u32 i, j;
    s32 k, exponentIntegerType, n;
    s32 hx, hy, ix, iy;
    u32 lx, ly;

    // Extract the argument words used by the special-case tests.
    hx = getDoubleHighWord(x);
    lx = getDoubleLowWord(x);
    hy = getDoubleHighWord(y);
    ly = getDoubleLowWord(y);
    ix = hx & 0x7fffffff;
    iy = hy & 0x7fffffff;

    // Handle zero exponents, unit bases and NaN arguments.
    if ((iy | ly) == 0)
        return 1.0;

    if (hx == 0x3ff00000 && lx == 0)
        return 1.0;

    if (ix > 0x7ff00000 || (ix == 0x7ff00000 && lx != 0) || iy > 0x7ff00000 || (iy == 0x7ff00000 && ly != 0))
        return x + y;

    // Classify a negative base's exponent as non-integer, odd or even.
    exponentIntegerType = 0;
    if (hx < 0) {
        if (iy >= 0x43400000) {
            exponentIntegerType = 2;
        } else if (iy >= 0x3ff00000) {
            k = (iy >> 20) - 0x3ff;
            if (k > 20) {
                u32 j = ly >> (52 - k);
                if ((j << (52 - k)) == ly) {
                    exponentIntegerType = 2 - (j & 1);
                }
            } else if (ly == 0) {
                u32 j = iy >> (20 - k);
                if ((j << (20 - k)) == static_cast<u32>(iy)) {
                    exponentIntegerType = 2 - (j & 1);
                }
            }
        }
    }

    // Handle exact special values of the exponent.
    if (ly == 0) {
        if (iy == 0x7ff00000) {
            if (((ix - 0x3ff00000) | lx) == 0)
                return 1.0;
            else if (ix >= 0x3ff00000)
                return hy >= 0 ? y : 0.0;
            else
                return hy >= 0 ? 0.0 : -y;
        }
        if (iy == 0x3ff00000) {
            if (hy >= 0)
                return x;
            y = 1 / x;
#if FLT_EVAL_METHOD != 0
            {
                u64 i = getDoubleBits(y) & -1ULL / 2;
                if (i >> 52 == 0 && (i & (i - 1))) {
                    forceEval(static_cast<float>(y));
                }
            }
#endif
            return y;
        }
        if (hy == 0x40000000)
            return x * x;
        if (hy == 0x3fe00000) {
            if (hx >= 0)
                return sqrt(x);
        }
    }

    // Handle zero, infinity and unit-magnitude bases.
    ax = doubleAbsolute(x);

    if (lx == 0) {
        if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
            z = ax;
            if (hy < 0) {
                z = 1.0 / z;
            }
            if (hx < 0) {
                if (((ix - 0x3ff00000) | exponentIntegerType) == 0) {
                    z = (z - z) / (z - z);
                } else if (exponentIntegerType == 1) {
                    z = -z;
                }
            }
            return z;
        }
    }

    // Determine the result sign and reject negative bases to non-integer powers.
    s = 1.0;
    if (hx < 0) {
        if (exponentIntegerType == 0)
            return (x - x) / (x - x);
        if (exponentIntegerType == 1) {
            s = -1.0;
        }
    }

    // Approximate log2(x) directly when the exponent is huge and x is near one.
    if (iy > 0x41e00000) {
        if (iy > 0x43f00000) {
            if (ix <= 0x3fefffff)
                return hy < 0 ? huge * huge : tiny * tiny;
            if (ix >= 0x3ff00000)
                return hy > 0 ? huge * huge : tiny * tiny;
        }

        if (ix < 0x3fefffff)
            return hy < 0 ? s * huge * huge : s * tiny * tiny;
        if (ix > 0x3ff00000)
            return hy > 0 ? s * huge * huge : s * tiny * tiny;

        t = ax - 1.0;
        w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
        u = inverseLn2High * t;
        v = t * inverseLn2Low - w * ivln2;
        t1 = u + v;
        t1 = setDoubleLowWord(t1, 0);
        t2 = v - (t1 - u);
    } else {
        // Normalize x and compute a split log2(x) on the selected interval.
        double ss, s2, sHigh, sLow, tHigh, tLow;
        n = 0;

        // Scale subnormal inputs before extracting their exponent.
        if (ix < 0x00100000) {
            ax *= two53;
            n -= 53;
            ix = getDoubleHighWord(ax);
        }
        n += ((ix) >> 20) - 0x3ff;
        j = ix & 0x000fffff;

        // Select the interval centered at one or one and a half.
        ix = j | 0x3ff00000;
        if (j <= 0x3988E) {
            k = 0;
        } else if (j < 0xBB67A) {
            k = 1;
        } else {
            k = 0;
            n += 1;
            ix -= 0x00100000;
        }
        ax = setDoubleHighWord(ax, ix);

        // Split (x - bp[k]) / (x + bp[k]) into high and low parts.
        u = ax - bp[k];
        v = 1.0 / (ax + bp[k]);
        ss = u * v;
        sHigh = ss;
        sHigh = setDoubleLowWord(sHigh, 0);

        tHigh = 0.0;
        tHigh = setDoubleHighWord(tHigh, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));
        tLow = ax - (tHigh - bp[k]);
        sLow = v * ((u - sHigh * tHigh) - sHigh * tLow);

        // Evaluate the logarithm polynomial with compensated products.
        s2 = ss * ss;
        r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
        r += sLow * (sHigh + ss);
        s2 = sHigh * sHigh;
        tHigh = 3.0 + s2 + r;
        tHigh = setDoubleLowWord(tHigh, 0);
        tLow = r - ((tHigh - 3.0) - s2);

        u = sHigh * tHigh;
        v = sLow * tHigh + tLow * ss;

        // Convert the polynomial result into a split base-2 logarithm.
        productHigh = u + v;
        productHigh = setDoubleLowWord(productHigh, 0);
        productLow = v - (productHigh - u);
        zHigh = coefficientHigh * productHigh;
        zLow = coefficientLow * productHigh + productLow * cp + correctionLow[k];

        t = static_cast<double>(n);
        t1 = ((zHigh + zLow) + correctionHigh[k]) + t;
        t1 = setDoubleLowWord(t1, 0);
        t2 = zLow - (((t1 - t) - correctionHigh[k]) - zHigh);
    }

    // Multiply y by log2(x), preserving high and low product parts.
    y1 = y;
    y1 = setDoubleLowWord(y1, 0);
    productLow = (y - y1) * t1 + y * t2;
    productHigh = y1 * t1;
    z = productLow + productHigh;
    j = getDoubleHighWord(z);
    i = getDoubleLowWord(z);

    // Detect overflow or underflow before evaluating the exponential.
    if (j < 0x80000000 && j >= 0x40900000) {
        if (((j - 0x40900000) | i) != 0)
            return s * huge * huge;
        if (productLow + ovt > z - productHigh)
            return s * huge * huge;
    } else if ((j & 0x7fffffff) >= 0x4090cc00) {
        if (((j - 0xc090cc00) | i) != 0)
            return s * tiny * tiny;
        if (productLow <= z - productHigh)
            return s * tiny * tiny;
    }

    // Split the product into an integer exponent and a small remainder.
    i = j & 0x7fffffff;
    k = static_cast<s32>(i >> 20) - 0x3ff;
    n = 0;
    if (i > 0x3fe00000) {
        i = j + (0x00100000 >> (k + 1));
        k = static_cast<s32>((i & 0x7fffffff) >> 20) - 0x3ff;
        t = 0.0;
        t = setDoubleHighWord(t, i & ~(0x000fffff >> k));
        n = ((i & 0x000fffff) | 0x00100000) >> (20 - k);
        if (j >> 31) {
            n = -n;
        }
        productHigh -= t;
    }

    // Evaluate exp2 of the remainder and restore the integer exponent.
    t = productLow + productHigh;
    t = setDoubleLowWord(t, 0);
    u = t * ln2High;
    v = (productLow - (t - productHigh)) * lg2 + t * ln2Low;
    z = u + v;
    w = v - (z - u);
    t = z * z;
    t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    r = (z * t1) / (t1 - 2.0) - (w + z * w);
    z = 1.0 - (r - z);
    s64 adjustedHighWord = getDoubleHighWord(z) + static_cast<s64>(n) * 0x00100000;
    if (adjustedHighWord < 0x00100000) {
        z = scaleDoubleByPowerOfTwo(z, n);
    } else {
        z = setDoubleHighWord(z, static_cast<u32>(adjustedHighWord));
    }
    return s * z;
}

// Computes sine after quadrant reduction.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/s_sin.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n12
double sin(double x) {
    double y[2];
    u32 ix;
    unsigned n;

    // Evaluate directly on the primary interval.
    ix = getDoubleHighWord(x);
    ix &= 0x7fffffff;

    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e500000) {
            forceEval(ix < 0x00100000 ? x / 1.329227995784916e36f : x + 1.329227995784916e36f);
            return x;
        }
        return sinDoubleKernel(x, 0.0, 0);
    }

    // Map infinities and NaNs to NaN.
    if (ix >= 0x7ff00000)
        return x - x;

    // Reduce the argument and select the result for its quadrant.
    n = reduceDouble(x, y);
    switch (n & 3) {
        case 0:
            return sinDoubleKernel(y[0], y[1], 1);
        case 1:
            return cosDoubleKernel(y[0], y[1]);
        case 2:
            return -sinDoubleKernel(y[0], y[1], 1);
        default:
            return -cosDoubleKernel(y[0], y[1]);
    }
}

// Computes cosine after quadrant reduction.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/s_cos.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n12
double cos(double x) {
    double y[2];
    u32 ix;
    unsigned n;

    // Evaluate directly on the primary interval.
    ix = getDoubleHighWord(x);
    ix &= 0x7fffffff;

    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e46a09e) {
            forceEval(x + 1.329227995784916e36f);
            return 1.0;
        }
        return cosDoubleKernel(x, 0);
    }

    // Map infinities and NaNs to NaN.
    if (ix >= 0x7ff00000)
        return x - x;

    // Reduce the argument and select the result for its quadrant.
    n = reduceDouble(x, y);
    switch (n & 3) {
        case 0:
            return cosDoubleKernel(y[0], y[1]);
        case 1:
            return -sinDoubleKernel(y[0], y[1], 1);
        case 2:
            return -cosDoubleKernel(y[0], y[1]);
        default:
            return sinDoubleKernel(y[0], y[1], 1);
    }
}

// Computes tangent after quadrant reduction.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/s_tan.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n12
double tan(double x) {
    double y[2];
    u32 ix;
    unsigned n;

    // Evaluate directly on the primary interval.
    ix = getDoubleHighWord(x);
    ix &= 0x7fffffff;

    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e400000) {
            forceEval(ix < 0x00100000 ? x / 1.329227995784916e36f : x + 1.329227995784916e36f);
            return x;
        }
        return tanDoubleKernel(x, 0.0, 0);
    }

    // Map infinities and NaNs to NaN.
    if (ix >= 0x7ff00000)
        return x - x;

    // Reduce the argument and select tangent or its negated reciprocal.
    n = reduceDouble(x, y);
    return tanDoubleKernel(y[0], y[1], n & 1);
}

// Computes inverse sine with interval-specific rational approximations.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_asin.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
static double inverseSinRatio(double z) {
    static constexpr double P[] = {1.66666666666666657415e-01, -3.25565818622400915405e-01,
                                   2.01212532134862925881e-01, -4.00555345006794114027e-02,
                                   7.91534994289814532176e-04, 3.47933107596021167570e-05};
    static constexpr double Q[] = {-2.40339491173441421878e+00, 2.02094576023350569471e+00, -6.88283971605453293030e-01,
                                   7.70381505559019352791e-02};
    double p, q;
    p = z * (P[0] + z * (P[1] + z * (P[2] + z * (P[3] + z * (P[4] + z * P[5])))));
    q = 1.0 + z * (Q[0] + z * (Q[1] + z * (Q[2] + z * Q[3])));
    return p / q;
}

double arcsin(double x) {
    static constexpr double piOverTwoHigh = 1.57079632679489655800e+00;
    static constexpr double piOverTwoLow = 6.12323399573676603587e-17;

    double z, r, s;
    u32 hx, ix;

    // Handle endpoints and values outside the function's domain.
    hx = getDoubleHighWord(x);
    ix = hx & 0x7fffffff;

    if (ix >= 0x3ff00000) {
        u32 lx;
        lx = getDoubleLowWord(x);
        if (((ix - 0x3ff00000) | lx) == 0)
            return x * piOverTwoHigh + 7.52316384526264e-37f;
        return 0 / (x - x);
    }

    // Use the direct rational approximation below one half.
    if (ix < 0x3fe00000) {
        if (ix < 0x3e500000 && ix >= 0x00100000)
            return x;
        return x + x * inverseSinRatio(x * x);
    }

    // Transform larger magnitudes with asin(x) = pi/2 - 2*asin(sqrt((1-|x|)/2)).
    z = (1 - doubleAbsolute(x)) * 0.5;
    s = sqrt(z);
    r = inverseSinRatio(z);
    if (ix >= 0x3fef3333) {
        x = piOverTwoHigh - (2 * (s + s * r) - piOverTwoLow);
    } else {
        double f, c;

        f = s;
        f = setDoubleLowWord(f, 0);
        c = (z - f * f) / (s + f);
        x = 0.5 * piOverTwoHigh - (2 * s * r - (piOverTwoLow - 2 * c) - (0.5 * piOverTwoHigh - 2 * f));
    }
    if (hx >> 31)
        return -x;
    return x;
}

// Computes inverse cosine with interval-specific rational approximations.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/e_acos.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n13
double arccos(double x) {
    static constexpr double piOverTwoHigh = 1.57079632679489655800e+00;
    static constexpr double piOverTwoLow = 6.12323399573676603587e-17;

    double z, w, s, c, df;
    u32 hx, ix;

    // Handle endpoints and values outside the function's domain.
    hx = getDoubleHighWord(x);
    ix = hx & 0x7fffffff;

    if (ix >= 0x3ff00000) {
        u32 lx;

        lx = getDoubleLowWord(x);
        if (((ix - 0x3ff00000) | lx) == 0) {
            if (hx >> 31)
                return 2 * piOverTwoHigh + 7.52316384526264e-37f;
            return 0;
        }
        return 0 / (x - x);
    }

    // Use acos(x) = pi/2 - asin(x) below one half.
    if (ix < 0x3fe00000) {
        if (ix <= 0x3c600000)
            return piOverTwoHigh + 7.52316384526264e-37f;
        return piOverTwoHigh - (x - (piOverTwoLow - x * inverseSinRatio(x * x)));
    }

    // Transform negative values using acos(x) = pi - 2*asin(sqrt((1+x)/2)).
    if (hx >> 31) {
        z = (1.0 + x) * 0.5;
        s = sqrt(z);
        w = inverseSinRatio(z) * s - piOverTwoLow;
        return 2 * (piOverTwoHigh - (s + w));
    }

    // Transform positive values using acos(x) = 2*asin(sqrt((1-x)/2)).
    z = (1.0 - x) * 0.5;
    s = sqrt(z);
    df = s;
    df = setDoubleLowWord(df, 0);
    c = (z - df * df) / (s + df);
    w = inverseSinRatio(z) * s + c;
    return 2 * (df + w);
}

// Computes inverse tangent with interval reduction and a polynomial.
// Details: https://cgit.freebsd.org/src/tree/lib/msun/src/s_atan.c?id=0dd5a5603e7a33d976f8e6015620bbc79839c609#n12
double arctan(double x) {
    static constexpr double atanhi[] = {
        4.63647609000806093515e-01,
        7.85398163397448278999e-01,
        9.82793723247329054082e-01,
        1.57079632679489655800e+00,
    };

    static constexpr double atanlo[] = {
        2.26987774529616870924e-17,
        3.06161699786838301793e-17,
        1.39033110312309984516e-17,
        6.12323399573676603587e-17,
    };

    static constexpr double aT[] = {
        3.33333333333329318027e-01,  -1.99999999998764832476e-01, 1.42857142725034663711e-01,
        -1.11111104054623557880e-01, 9.09088713343650656196e-02,  -7.69187620504482999495e-02,
        6.66107313738753120669e-02,  -5.83357013379057348645e-02, 4.97687799461593236017e-02,
        -3.65315727442169155270e-02, 1.62858201153657823623e-02,
    };

    double w, s1, s2, z;
    u32 ix, sign;
    s32 id;

    // Handle huge arguments and reduce finite values to a primary interval.
    ix = getDoubleHighWord(x);
    sign = ix >> 31;
    ix &= 0x7fffffff;
    if (ix >= 0x44100000) {
        if (doubleIsNan(x))
            return x;
        z = atanhi[3] + 7.52316384526264e-37f;
        return sign ? -z : z;
    }
    if (ix < 0x3fdc0000) {
        if (ix < 0x3e400000) {
            if (ix < 0x00100000)
                forceEval(static_cast<float>(x));
            return x;
        }
        id = -1;
    } else {
        x = doubleAbsolute(x);
        if (ix < 0x3ff30000) {
            if (ix < 0x3fe60000) {
                id = 0;
                x = (2.0 * x - 1.0) / (2.0 + x);
            } else {
                id = 1;
                x = (x - 1.0) / (x + 1.0);
            }
        } else {
            if (ix < 0x40038000) {
                id = 2;
                x = (x - 1.5) / (1.0 + 1.5 * x);
            } else {
                id = 3;
                x = -1.0 / x;
            }
        }
    }

    // Split the polynomial into odd and even terms.
    z = x * x;
    w = z * z;

    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0)
        return x - x * (s1 + s2);
    z = atanhi[id] - (x * (s1 + s2) - atanlo[id] - x);
    return sign ? -z : z;
}

//  ▄▄▄▄▄ ▄▄▄                 ▄▄    ▄▄▄▄
//  ██     ██   ▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██▀▀   ██  ██  ██  ▄▄▄██  ██    ▄█▀▀
//  ██    ▄██▄ ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ██▄▄▄▄
//

Float2 Float2::normalized() const {
    return *this / length();
}

Float2 Float2::safeNormalized(const Float2& fallback, float epsilon) const {
    float L2 = this->lengthSquared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrt(L2);
}

Rect rectFromFov(float fovY, float aspect) {
    float halfTanY = tan(fovY / 2);
    return inflate(Rect{{0, 0}}, {halfTanY * aspect, halfTanY});
}

Float2 roundUp(const Float2& value) {
    return {roundUp(value.x), roundUp(value.y)};
}

Float2 roundDown(const Float2& value) {
    return {roundDown(value.x), roundDown(value.y)};
}

Float2 roundNearest(const Float2& value) {
    return {roundNearest(value.x), roundNearest(value.y)};
}

Float2 stepTowards(const Float2& start, const Float2& target, float amount) {
    Float2 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//  ▄▄▄▄▄ ▄▄▄                 ▄▄    ▄▄▄▄
//  ██     ██   ▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██▀▀   ██  ██  ██  ▄▄▄██  ██     ▀▀█▄
//  ██    ▄██▄ ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀
//

Float3 Float3::normalized() const {
    return *this / length();
}

Float3 Float3::safeNormalized(const Float3& fallback, float epsilon) const {
    float L2 = this->lengthSquared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrt(L2);
}

Float3 pow(const Float3& a, const Float3& b) {
    return {pow(a.x, b.x), pow(a.y, b.y), pow(a.z, b.z)};
}

Float3 roundUp(const Float3& value) {
    return {roundUp(value.x), roundUp(value.y), roundUp(value.z)};
}

Float3 roundDown(const Float3& value) {
    return {roundDown(value.x), roundDown(value.y), roundDown(value.z)};
}

Float3 roundNearest(const Float3& value) {
    return {roundNearest(value.x), roundNearest(value.y), roundNearest(value.z)};
}

Float3 stepTowards(const Float3& start, const Float3& target, float amount) {
    Float3 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//  ▄▄▄▄▄ ▄▄▄                 ▄▄      ▄▄▄
//  ██     ██   ▄▄▄▄   ▄▄▄▄  ▄██▄▄  ▄█▀██
//  ██▀▀   ██  ██  ██  ▄▄▄██  ██   ██▄▄██▄
//  ██    ▄██▄ ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄     ██
//

Float4 Float4::normalized() const {
    return *this / length();
}

Float4 Float4::safeNormalized(const Float4& fallback, float epsilon) const {
    float L2 = this->lengthSquared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrt(L2);
}

Float4 pow(const Float4& a, const Float4& b) {
    return {pow(a.x, b.x), pow(a.y, b.y), pow(a.z, b.z), pow(a.w, b.w)};
}

Float4 roundUp(const Float4& vec) {
    return {roundUp(vec.x), roundUp(vec.y), roundUp(vec.z), roundUp(vec.w)};
}

Float4 roundDown(const Float4& vec) {
    return {roundDown(vec.x), roundDown(vec.y), roundDown(vec.z), roundDown(vec.w)};
}

Float4 roundNearest(const Float4& vec) {
    return {roundNearest(vec.x), roundNearest(vec.y), roundNearest(vec.z), roundNearest(vec.w)};
}

Float4 stepTowards(const Float4& start, const Float4& target, float amount) {
    Float4 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//   ▄▄▄▄         ▄▄▄
//  ██  ▀▀  ▄▄▄▄   ██   ▄▄▄▄  ▄▄▄▄▄
//  ██     ██  ██  ██  ██  ██ ██  ▀▀
//  ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄ ▀█▄▄█▀ ██
//

Color::Color(StringView hex) {
    if ((hex.numBytes() != 6) && (hex.numBytes() != 8)) {
        PLY_ASSERT(0); // Invalid hex string
        return;
    }
    const char* s = hex.bytes();
    auto readHex = [&]() -> u32 {
        u32 c = 0;
        for (int j = 0; j < 2; j++) {
            c <<= 4;
            if (*s >= '0' && *s <= '9') {
                c += *s - '0';
            } else if (*s >= 'a' && *s <= 'f') {
                c += *s - 'a' + 10;
            } else if (*s >= 'A' && *s <= 'F') {
                c += *s - 'A' + 10;
            } else {
                PLY_ASSERT(0);
            }
            s++;
        }
        return (u8) c;
    };
    this->r = readHex();
    this->g = readHex();
    this->b = readHex();
    if (hex.numBytes() == 8) {
        this->a = readHex();
    } else {
        this->a = 255;
    }
}

float srgbToLinear(float s) {
    if (s < 0.0404482362771082f)
        return s / 12.92f;
    else
        return pow(((s + 0.055f) / 1.055f), 2.4f);
}

float linearToSrgb(float l) {
    if (l < 0.00313066844250063f)
        return l * 12.92f;
    else
        return 1.055f * pow(l, 1 / 2.4f) - 0.055f;
}

Float3 srgbToLinear(const Float3& vec) {
    return {srgbToLinear(vec.x), srgbToLinear(vec.y), srgbToLinear(vec.z)};
}

Float4 srgbToLinear(const Float4& vec) {
    return {srgbToLinear(vec.x), srgbToLinear(vec.y), srgbToLinear(vec.z), vec.w};
}

Float3 linearToSrgb(const Float3& vec) {
    return {linearToSrgb(vec.x), linearToSrgb(vec.y), linearToSrgb(vec.z)};
}

Float4 linearToSrgb(const Float4& vec) {
    return {linearToSrgb(vec.x), linearToSrgb(vec.y), linearToSrgb(vec.z), vec.w};
}

//  ▄▄   ▄▄         ▄▄    ▄▄▄▄          ▄▄▄▄
//  ███▄███  ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄ ▀▀  ██
//  ██▀█▀██  ▄▄▄██  ██    ▄█▀▀   ▀██▀   ▄█▀▀
//  ██   ██ ▀█▄▄██  ▀█▄▄ ██▄▄▄▄ ▄█▀▀█▄ ██▄▄▄▄
//

Mat2x2 Mat2x2::identity() {
    return {{1, 0}, {0, 1}};
}

Mat2x2 Mat2x2::scale(const Float2& scale) {
    return {{scale.x, 0}, {0, scale.y}};
}

Mat2x2 Mat2x2::rotate(float radians) {
    return fromComplex(Complex::fromAngle(radians));
}

Mat2x2 Mat2x2::fromComplex(const Float2& c) {
    return {{c.x, c.y}, {-c.y, c.x}};
}

Mat2x2 Mat2x2::transposed() const {
    return {{col[0].x, col[1].x}, {col[0].y, col[1].y}};
}

bool operator==(const Mat2x2& a, const Mat2x2& b) {
    return (a.col[0] == b.col[0]) && (a.col[1] == b.col[1]);
}

Float2 operator*(const Mat2x2& m_, const Float2& v_) {
    return {m_.col[0].x * v_.x + m_.col[1].x * v_.y, m_.col[0].y * v_.x + m_.col[1].y * v_.y};
}

Mat2x2 operator*(const Mat2x2& a, const Mat2x2& b) {
    Mat2x2 result;
    for (u32 c = 0; c < 2; c++) {
        result[c] = a * b.col[c];
    }
    return result;
}

//  ▄▄   ▄▄         ▄▄    ▄▄▄▄          ▄▄▄▄
//  ███▄███  ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄ ▀▀  ██
//  ██▀█▀██  ▄▄▄██  ██     ▀▀█▄  ▀██▀    ▀▀█▄
//  ██   ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ▄█▀▀█▄ ▀█▄▄█▀
//

Mat3x3::Mat3x3(const Mat3x4& m) : Mat3x3{m.col[0], m.col[1], m.col[2]} {
}

Mat3x3::Mat3x3(const Mat4x4& m) : Mat3x3{Float3{m.col[0]}, Float3{m.col[1]}, Float3{m.col[2]}} {
}

Mat3x3 Mat3x3::identity() {
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
}

Mat3x3 Mat3x3::scale(const Float3& arg) {
    return {{arg.x, 0, 0}, {0, arg.y, 0}, {0, 0, arg.z}};
}

Mat3x3 Mat3x3::rotate(const Float3& unitAxis, float radians) {
    return Mat3x3::fromQuaternion(Quaternion::fromAxisAngle(unitAxis, radians));
}

Mat3x3 Mat3x3::fromQuaternion(const Quaternion& q) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y}};
}

bool Mat3x3::hasScale() const {
    return !col[0].isUnitLength() || !col[1].isUnitLength() || !col[2].isUnitLength();
}

Mat3x3 Mat3x3::transposed() const {
    return {{col[0].x, col[1].x, col[2].x}, {col[0].y, col[1].y, col[2].y}, {col[0].z, col[1].z, col[2].z}};
}

bool operator==(const Mat3x3& a_, const Mat3x3& b_) {
    return a_.col[0] == b_.col[0] && a_.col[1] == b_.col[1] && a_.col[2] == b_.col[2];
}

Float3 operator*(const Mat3x3& m_, const Float3& v_) {
    return {m_.col[0].x * v_.x + m_.col[1].x * v_.y + m_.col[2].x * v_.z,
            m_.col[0].y * v_.x + m_.col[1].y * v_.y + m_.col[2].y * v_.z,
            m_.col[0].z * v_.x + m_.col[1].z * v_.y + m_.col[2].z * v_.z};
}

Mat3x3 operator*(const Mat3x3& a, const Mat3x3& b) {
    Mat3x3 result;
    for (u32 c = 0; c < 3; c++) {
        result.col[c] = a * b.col[c];
    }
    return result;
}

Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& dstUp, const Float3& srcUnitFwd, const Float3& srcUnitUp) {
    PLY_ASSERT(dstUnitFwd.isUnitLength());
    PLY_ASSERT(srcUnitFwd.isUnitLength());
    PLY_ASSERT(srcUnitUp.isUnitLength());

    Float3 dstRight = cross(dstUnitFwd, dstUp);
    float L2 = dstRight.lengthSquared();
    if (L2 < 1e-6f) {
        dstRight = cross(dstUnitFwd, getNoncollinear(dstUnitFwd));
        L2 = dstRight.lengthSquared();
    }
    dstRight /= sqrt(L2);
    return Mat3x3{dstRight, dstUnitFwd, cross(dstRight, dstUnitFwd)} *
           Mat3x3{cross(srcUnitFwd, srcUnitUp), srcUnitFwd, srcUnitUp}.transposed();
}

//  ▄▄   ▄▄         ▄▄    ▄▄▄▄            ▄▄▄
//  ███▄███  ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄  ▄█▀██
//  ██▀█▀██  ▄▄▄██  ██     ▀▀█▄  ▀██▀  ██▄▄██▄
//  ██   ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ▄█▀▀█▄     ██
//

Mat3x4::Mat3x4(const Mat3x3& m, const Float3& pos) {
    for (u32 i = 0; i < 3; i++) {
        col[i] = m.col[i];
    }
    col[3] = pos;
}

Mat3x4::Mat3x4(const Mat4x4& m) : Mat3x4{Float3{m.col[0]}, Float3{m.col[1]}, Float3{m.col[2]}, Float3{m.col[3]}} {
}

Mat3x4 Mat3x4::identity() {
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
}

Mat3x4 Mat3x4::scale(const Float3& arg) {
    return {{arg.x, 0, 0}, {0, arg.y, 0}, {0, 0, arg.z}, {0, 0, 0}};
}

Mat3x4 Mat3x4::rotate(const Float3& unitAxis, float radians) {
    return Mat3x4::fromQuaternion(Quaternion::fromAxisAngle(unitAxis, radians));
}

Mat3x4 Mat3x4::translate(const Float3& pos) {
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, pos};
}

Mat3x4 Mat3x4::fromQuaternion(const Quaternion& q, const Float3& pos) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y},
            pos};
}

Mat3x4 Mat3x4::invertedOrtho() const {
    Mat3x3 rotation = asMat3().transposed();
    return {rotation.col[0], rotation.col[1], rotation.col[2], rotation * -col[3]};
}

bool operator==(const Mat3x4& a_, const Mat3x4& b_) {
    return a_.col[0] == b_.col[0] && a_.col[1] == b_.col[1] && a_.col[2] == b_.col[2] && a_.col[3] == b_.col[3];
}

Float3 operator*(const Mat3x4& m_, const Float3& v_) {
    return {m_.col[0].x * v_.x + m_.col[1].x * v_.y + m_.col[2].x * v_.z + m_.col[3].x,
            m_.col[0].y * v_.x + m_.col[1].y * v_.y + m_.col[2].y * v_.z + m_.col[3].y,
            m_.col[0].z * v_.x + m_.col[1].z * v_.y + m_.col[2].z * v_.z + m_.col[3].z};
}

Float4 operator*(const Mat3x4& m_, const Float4& v_) {
    return {m_.col[0].x * v_.x + m_.col[1].x * v_.y + m_.col[2].x * v_.z + m_.col[3].x * v_.w,
            m_.col[0].y * v_.x + m_.col[1].y * v_.y + m_.col[2].y * v_.z + m_.col[3].y * v_.w,
            m_.col[0].z * v_.x + m_.col[1].z * v_.y + m_.col[2].z * v_.z + m_.col[3].z * v_.w, v_.w};
}

Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b) {
    Mat3x4 result;
    for (u32 c = 0; c < 3; c++) {
        result.col[c] = a.asMat3() * b.col[c];
    }
    result.col[3] = a * b.col[3];
    return result;
}

//  ▄▄   ▄▄         ▄▄      ▄▄▄            ▄▄▄
//  ███▄███  ▄▄▄▄  ▄██▄▄  ▄█▀██  ▄▄  ▄▄  ▄█▀██
//  ██▀█▀██  ▄▄▄██  ██   ██▄▄██▄  ▀██▀  ██▄▄██▄
//  ██   ██ ▀█▄▄██  ▀█▄▄     ██  ▄█▀▀█▄     ██
//

Mat4x4::Mat4x4(const Mat3x3& m, const Float3& pos) {
    *this = {
        {m.col[0], 0},
        {m.col[1], 0},
        {m.col[2], 0},
        {pos, 1},
    };
}

Mat4x4::Mat4x4(const Mat3x4& m) {
    *this = {
        {m.col[0], 0},
        {m.col[1], 0},
        {m.col[2], 0},
        {m.col[3], 1},
    };
}

Mat4x4 Mat4x4::identity() {
    return {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
}

Mat4x4 Mat4x4::scale(const Float3& arg) {
    return {{arg.x, 0, 0, 0}, {0, arg.y, 0, 0}, {0, 0, arg.z, 0}, {0, 0, 0, 1}};
}

Mat4x4 Mat4x4::rotate(const Float3& unitAxis, float radians) {
    return Mat4x4::fromQuaternion(Quaternion::fromAxisAngle(unitAxis, radians));
}

Mat4x4 Mat4x4::translate(const Float3& pos) {
    return {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {pos, 1}};
}

Mat4x4 Mat4x4::fromQuaternion(const Quaternion& q, const Float3& pos) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w, 0},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w, 0},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y, 0},
            {pos, 1}};
}

Mat4x4 Mat4x4::perspectiveProjection(const Rect& frustum, float zNear, float zFar, DeviceCoordType devCoordType) {
    PLY_ASSERT(zNear > 0 && zFar > 0);
    Mat4x4 result{0, 0, 0, 0};
    float ooXdenom = 1.f / frustum.width();
    float ooYdenom = 1.f / frustum.height();
    float ooZdenom = 1.f / (zNear - zFar);
    result.col[0].x = 2.f * ooXdenom;
    result.col[2].x = (frustum.mins.x + frustum.maxs.x) * ooXdenom;
    result.col[1].y = 2.f * ooYdenom;
    result.col[2].y = (frustum.mins.y + frustum.maxs.y) * ooYdenom;
    result.col[2].z = (zNear + zFar) * ooZdenom;
    result.col[2].w = -1.f;
    result.col[3].z = (2 * zNear * zFar) * ooZdenom;
    if (devCoordType == DeviceCoordType::Metal) {
        result.col[2].z = 0.5f * result.col[2].z - 0.5f;
        result.col[3].z *= 0.5f;
    }
    return result;
}

Mat4x4 Mat4x4::orthographicProjection(const Rect& rect, YCoordType yCoordType, float zNear, float zFar,
                                      DeviceCoordType devCoordType) {
    Mat4x4 result{0, 0, 0, 0};
    float tow = 2 / rect.width();
    float toh = 2 / rect.height();
    float ooZrange = 1 / (zNear - zFar);
    result.col[0].x = tow;
    result.col[3].x = -rect.mid().x * tow;
    if (yCoordType == YCoordType::Up) {
        result.col[1].y = toh;
        result.col[3].y = -rect.mid().y * toh;
    } else {
        result.col[1].y = -toh;
        result.col[3].y = rect.mid().y * toh;
    }
    result.col[2].z = 2 * ooZrange;
    result.col[3].z = (zNear + zFar) * ooZrange;
    result.col[3].w = 1.f;
    if (devCoordType == DeviceCoordType::Metal) {
        result.col[2].z *= 0.5f;
        result.col[3].z = 0.5f * result.col[3].z + 0.5f;
    }
    return result;
}

Mat4x4 Mat4x4::transposed() const {
    return {{col[0].x, col[1].x, col[2].x, col[3].x},
            {col[0].y, col[1].y, col[2].y, col[3].y},
            {col[0].z, col[1].z, col[2].z, col[3].z},
            {col[0].w, col[1].w, col[2].w, col[3].w}};
}

Mat4x4 Mat4x4::inverted() const {
    // Transpose the matrix so that it effectively becomes row-major.
    Mat4x4 left = this->transposed();
    Mat4x4 right = Mat4x4::identity();

    // Reduce the left side to identity using partial pivoting.
    for (u32 pivot = 0; pivot < 4; pivot++) {
        u32 pivotRow = pivot;
        float pivotAbs = abs(left[pivot][pivot]);
        for (u32 r = pivot + 1; r < 4; r++) {
            float candidateAbs = abs(left[r][pivot]);
            if (candidateAbs > pivotAbs) {
                pivotRow = r;
                pivotAbs = candidateAbs;
            }
        }
        PLY_ASSERT(pivotAbs != 0);

        if (pivotRow != pivot) {
            Float4 temp = left[pivot];
            left[pivot] = left[pivotRow];
            left[pivotRow] = temp;

            temp = right[pivot];
            right[pivot] = right[pivotRow];
            right[pivotRow] = temp;
        }

        float pivotValue = left[pivot][pivot];
        left[pivot] /= pivotValue;
        right[pivot] /= pivotValue;

        for (u32 r = 0; r < 4; r++) {
            if (r == pivot)
                continue;

            float factor = left[r][pivot];
            if (factor == 0)
                continue;

            left[r] -= left[pivot] * Float4{factor};
            right[r] -= right[pivot] * Float4{factor};
        }
    }

    return right.transposed();
}

Mat4x4 Mat4x4::invertedOrtho() const {
    Mat4x4 result = transposed();
    result.col[0].w = 0;
    result.col[1].w = 0;
    result.col[2].w = 0;
    result.col[3] = result * -col[3];
    result.col[3].w = 1;
    return result;
}

bool operator==(const Mat4x4& a_, const Mat4x4& b_) {
    return a_.col[0] == b_.col[0] && a_.col[1] == b_.col[1] && a_.col[2] == b_.col[2] && a_.col[3] == b_.col[3];
}

Float4 operator*(const Mat4x4& m_, const Float4& v_) {
    return {m_.col[0].x * v_.x + m_.col[1].x * v_.y + m_.col[2].x * v_.z + m_.col[3].x * v_.w,
            m_.col[0].y * v_.x + m_.col[1].y * v_.y + m_.col[2].y * v_.z + m_.col[3].y * v_.w,
            m_.col[0].z * v_.x + m_.col[1].z * v_.y + m_.col[2].z * v_.z + m_.col[3].z * v_.w,
            m_.col[0].w * v_.x + m_.col[1].w * v_.y + m_.col[2].w * v_.z + m_.col[3].w * v_.w};
}

Mat4x4 operator*(const Mat4x4& a, const Mat4x4& b) {
    Mat4x4 result;
    for (u32 c = 0; c < 4; c++) {
        result.col[c] = a * b.col[c];
    }
    return result;
}

Mat4x4 operator*(const Mat3x4& a, const Mat4x4& b) {
    Mat4x4 result;
    for (u32 c = 0; c < 4; c++) {
        result[c] = a * b.col[c];
    }
    return result;
}

Mat4x4 operator*(const Mat4x4& a, const Mat3x4& b) {
    Mat4x4 result;
    for (u32 c = 0; c < 3; c++) {
        result.col[c] = a * Float4{b.col[c], 0};
    }
    result[3] = a * Float4{b.col[3], 1};
    return result;
}

//   ▄▄▄▄                 ▄▄                        ▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██   ██▄▄██ ██  ▀▀ ██  ██ ██ ██  ██ ██  ██
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄  ██     ██  ██ ██ ▀█▄▄█▀ ██  ██
//      ▀▀

Quaternion Quaternion::fromAxisAngle(const Float3& unitAxis, float radians) {
    PLY_ASSERT(unitAxis.isUnitLength());
    Float2 cossin = cosAndSin(radians / 2);
    return {cossin.y * unitAxis.x, cossin.y * unitAxis.y, cossin.y * unitAxis.z, cossin.x};
}

Quaternion Quaternion::fromUnitVectors(const Float3& start, const Float3& end) {
    // Float4{cross(start, end), dot(start, end)} gives you double the desired rotation.
    // To get the desired rotation, "average" (really just sum) that with Float4{0, 0,
    // 0, 1}, then normalize.
    float w = dot(start, end) + 1;
    if (w < 1e-6f) {
        // Exceptional case: Vectors point in opposite directions.
        // Choose a perpendicular axis and make a 180 degree rotation.
        Float3 getNoncollinear = (abs(start.x) < 0.9f) ? Float3{1, 0, 0} : Float3{0, 1, 0};
        Float3 axis = cross(start, getNoncollinear);
        return (Quaternion) Float4{axis, 0}.normalized();
    }
    Float3 v = cross(start, end);
    return (Quaternion) Float4{v, w}.normalized();
}

template <typename M>
Quaternion quaternionFromOrtho(const M& m) {
    float t; // This will be set to 4*c*c for some quaternion component c.
    // At least one component's square must be >= 1/4. (Otherwise, it isn't a unit
    // quaternion.) Let's require t >= 1/2. This will accept any component whose square
    // is >= 1/8.
    if ((t = 1.f + m[0][0] + m[1][1] + m[2][2]) >= 0.5f) { // 4*w*w
        float w = sqrt(t) * 0.5f;
        float f = 0.25f / w;
        return {(m[1][2] - m[2][1]) * f, (m[2][0] - m[0][2]) * f, (m[0][1] - m[1][0]) * f, w};
    } else if ((t = 1.f + m[0][0] - m[1][1] - m[2][2]) >= 0.5f) { // 4*x*x
        // Prefer positive w component in result
        float wco = m[1][2] - m[2][1];
        float x = sqrt(t) * ((wco >= 0) - 0.5f); // Equivalent to sqrt(t) * 0.5f * sgn(wco).
        float f = 0.25f / x;
        return {x, (m[0][1] + m[1][0]) * f, (m[2][0] + m[0][2]) * f, wco * f};
    } else if ((t = 1.f - m[0][0] + m[1][1] - m[2][2]) >= 0.5f) { // 4*y*y
        float wco = m[2][0] - m[0][2];
        float y = sqrt(t) * ((wco >= 0) - 0.5f); // Equivalent to sqrt(t) * 0.5f * sgn(wco).
        float f = 0.25f / y;
        return {(m[0][1] + m[1][0]) * f, y, (m[1][2] + m[2][1]) * f, wco * f};
    } else if ((t = 1.f - m[0][0] - m[1][1] + m[2][2]) >= 0.5f) { // 4*z*z
        float wco = m[0][1] - m[1][0];
        float z = sqrt(t) * ((wco >= 0) - 0.5f); // Equivalent to sqrt(t) * 0.5f * sgn(wco).
        float f = 0.25f / z;
        return {(m[2][0] + m[0][2]) * f, (m[1][2] + m[2][1]) * f, z, wco * f};
    }
    PLY_ASSERT(0); // The matrix is not even close to being orthonormal
    return {0, 0, 0, 1};
}

Quaternion Quaternion::fromOrtho(const Mat3x3& m) {
    return quaternionFromOrtho(m);
}

Quaternion Quaternion::fromOrtho(const Mat4x4& m) {
    return quaternionFromOrtho(m);
}

Quaternion Quaternion::negatedIfCloserTo(const Quaternion& other) const {
    Float4 v0{*this};
    Float4 v1{other};
    return Quaternion{(v0 - v1).lengthSquared() < (-v0 - v1).lengthSquared() ? v0 : -v0};
}

Float3 operator*(const Quaternion& q, const Float3& v) {
    // From https://gist.github.com/rygorous/8da6651b597f3d825862
    Float3 t = cross((Float3) q, v) * 2.f;
    return v + t * q.w + cross((const Float3&) q, t);
}

Quaternion operator*(const Quaternion& a, const Quaternion& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Quaternion mix(const Quaternion& a, const Quaternion& b, float t) {
    Float4 linearMix = mix((Float4) a.negatedIfCloserTo(b), (Float4) b, t);
    return (Quaternion) linearMix.normalized();
}

} // namespace ply
