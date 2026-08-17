/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    2D and 3D Math                             │
│               Documentation: /docs/math.md               │
│                                                          │
└─────────────────────────────────────────────────────────*/

#include "ply-math.h"

namespace ply {

// The scalar math functions below are adapted from musl libc's single-precision
// implementations, many of which originated in FreeBSD's msun library.
//
// ====================================================
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunPro, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this software is freely granted,
// provided that this notice is preserved.
// ====================================================
// Copyright 2004 Sun Microsystems, Inc. All Rights Reserved.
//
// Permission to use, copy, modify, and distribute this software is freely granted,
// provided that this notice is preserved.
// ====================================================

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

static float scaleFloatByPowerOfTwo(float value, s32 exponent) {
    float result = value;
    if (exponent > 127) {
        result *= 1.7014118346046923e38f;
        exponent -= 127;
        if (exponent > 127) {
            result *= 1.7014118346046923e38f;
            exponent -= 127;
            if (exponent > 127)
                exponent = 127;
        }
    } else if (exponent < -126) {
        result *= 1.9721522630525295e-31f;
        exponent += 126 - 24;
        if (exponent < -126) {
            result *= 1.9721522630525295e-31f;
            exponent += 126 - 24;
            if (exponent < -126)
                exponent = -126;
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

static float sinKernel(double x) {
    static constexpr double S1 = -0.166666666416265235595;
    static constexpr double S2 = 0.0083333293858894631756;
    static constexpr double S3 = -0.000198393348360966317347;
    static constexpr double S4 = 0.0000027183114939898219064;
    double z = x * x;
    double w = z * z;
    double r = S3 + z * S4;
    double s = z * x;
    return static_cast<float>((x + s * (S1 + z * S2)) + s * w * r);
}

static float cosKernel(double x) {
    static constexpr double C0 = -0.499999997251031003120;
    static constexpr double C1 = 0.0416666233237390631894;
    static constexpr double C2 = -0.00138867637746099294692;
    static constexpr double C3 = 0.0000243904487962774090654;
    double z = x * x;
    double w = z * z;
    double r = C2 + z * C3;
    return static_cast<float>(((1 + z * C0) + w * C1) + (w * z) * r);
}

static float tanKernel(double x, bool odd) {
    static constexpr double T[] = {
        0.333331395030791399758,
        0.133392002712976742718,
        0.0533812378445670393523,
        0.0245283181166547278873,
        0.00297435743359967304927,
        0.00946564784943673166728,
    };
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
static s32 reduceLarge(float x, double* remainder) {
    static constexpr s32 twoOverPi[] = {
        0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
        0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
        0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
        0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
        0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
        0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
        0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
        0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
        0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
        0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
        0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
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

    for (s32 i = 0; i <= numInitialTerms; i++)
        productTerms[i] = input * twoOverPi[tableStart + i];

    s32 last = numInitialTerms;
recompute:
    double z = productTerms[last];
    for (s32 i = 0, j = last; j > 0; i++, j--) {
        double carry = static_cast<s32>(z * (1.0 / 16777216.0));
        quadrantChunks[i] = static_cast<s32>(z - 16777216.0 * carry);
        z = productTerms[j - 1] + carry;
    }

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
        if (chunkExponent > 0)
            quadrantChunks[last - 1] &= (0xffffff >> chunkExponent);
        if (complement == 2) {
            z = 1 - z;
            if (carry != 0)
                z -= scaleByPowerOfTwo(1, chunkExponent);
        }
    }

    if (z == 0) {
        s32 nonZero = 0;
        for (s32 i = last - 1; i >= numInitialTerms; i--)
            nonZero |= quadrantChunks[i];
        if (nonZero == 0) {
            s32 extraTerms = 1;
            while (quadrantChunks[numInitialTerms - extraTerms] == 0)
                extraTerms++;
            for (s32 i = last + 1; i <= last + extraTerms; i++)
                productTerms[i] = input * twoOverPi[tableStart + i];
            last += extraTerms;
            goto recompute;
        }
    }

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

    double weight = scaleByPowerOfTwo(1, chunkExponent);
    for (s32 i = last; i >= 0; i--) {
        productTerms[i] = weight * quadrantChunks[i];
        weight *= 1.0 / 16777216.0;
    }
    for (s32 i = last; i >= 0; i--) {
        double term = 0;
        for (s32 j = 0; j <= numInitialTerms && j <= last - i; j++)
            term += piOverTwo[j] * productTerms[i + j];
        piProduct[last - i] = term;
    }

    double result = 0;
    for (s32 i = last; i >= 0; i--)
        result += piProduct[i];
    if (complement != 0)
        result = -result;
    if (negative) {
        *remainder = -result;
        return -(quadrant & 7);
    }
    *remainder = result;
    return quadrant & 7;
}

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
    quadrant = quadrant + roundToInteger - roundToInteger;
    s32 n = static_cast<s32>(quadrant);
    *remainder = x - quadrant * piOverTwoHigh - quadrant * piOverTwoTail;
    return n;
}

float sqrt(float value) {
    u32 bits = getFloatBits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u)
        return value * value + value;
    if (static_cast<s32>(bits) <= 0) {
        if ((bits & 0x7fffffffu) == 0)
            return value;
        return (value - value) / (value - value);
    }

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
    if (exponent & 1)
        bits += bits;
    exponent >>= 1;

    bits += bits;
    u32 resultBits = 0;
    u32 partial = 0;
    for (u32 movingBit = 0x01000000u; movingBit != 0; movingBit >>= 1) {
        u32 trial = partial + movingBit;
        if (trial <= bits) {
            partial = trial + movingBit;
            bits -= trial;
            resultBits += movingBit;
        }
        bits += bits;
    }
    if (bits != 0) {
        volatile float roundingProbe = 1 - 1e-30f;
        if (roundingProbe >= 1) {
            roundingProbe = 1 + 1e-30f;
            resultBits += roundingProbe > 1 ? 2 : resultBits & 1;
        }
    }
    resultBits = (resultBits >> 1) + 0x3f000000u;
    resultBits += static_cast<u32>(exponent) << 23;
    return makeFloat(resultBits);
}

float exp(float value) {
    static constexpr float half[] = {0.5f, -0.5f};
    static constexpr float ln2High = 6.9314575195e-1f;
    static constexpr float ln2Low = 1.4286067653e-6f;
    static constexpr float inverseLn2 = 1.4426950216f;
    static constexpr float P1 = 1.6666625440e-1f;
    static constexpr float P2 = -2.7667332906e-3f;

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

    float square = value * value;
    float correction = value - square * (P1 + square * P2);
    float result = 1 + (value * correction / (2 - correction) - low + high);
    return exponent == 0 ? result : scaleFloatByPowerOfTwo(result, exponent);
}

float log(float value) {
    static constexpr float ln2High = 6.9313812256e-01f;
    static constexpr float ln2Low = 9.0580006145e-06f;
    static constexpr float coefficients[] = {
        0.66666662693f, 0.40000972152f, 0.28498786688f, 0.24279078841f,
    };

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

    bits += 0x3f800000u - 0x3f3504f3u;
    exponent += static_cast<s32>(bits >> 23) - 127;
    bits = (bits & 0x007fffffu) + 0x3f3504f3u;
    value = makeFloat(bits);
    float f = value - 1;
    float s = f / (2 + f);
    float z = s * s;
    float w = z * z;
    float t1 = w * (coefficients[1] + w * coefficients[3]);
    float t2 = z * (coefficients[0] + w * coefficients[2]);
    float polynomial = t1 + t2;
    float halfSquare = 0.5f * f * f;
    float exponentAsFloat = static_cast<float>(exponent);
    return s * (halfSquare + polynomial) + exponentAsFloat * ln2Low - halfSquare + f +
           exponentAsFloat * ln2High;
}

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

    s32 baseBits = static_cast<s32>(getFloatBits(base));
    s32 exponentBits = static_cast<s32>(getFloatBits(exponent));
    u32 baseMagnitude = static_cast<u32>(baseBits) & 0x7fffffffu;
    u32 exponentMagnitude = static_cast<u32>(exponentBits) & 0x7fffffffu;
    if (exponentMagnitude == 0 || baseBits == 0x3f800000)
        return 1;
    if (baseMagnitude > 0x7f800000u || exponentMagnitude > 0x7f800000u)
        return base + exponent;

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

    float sign = 1;
    if (baseBits < 0) {
        if (exponentInteger == 0)
            return (base - base) / (base - base);
        if (exponentInteger == 1)
            sign = -1;
    }

    float t1;
    float t2;
    if (exponentMagnitude > 0x4d000000u) {
        if (baseMagnitude < 0x3f7ffff8u)
            return exponentBits < 0 ? sign * 1e30f * 1e30f : sign * 1e-30f * 1e-30f;
        if (baseMagnitude > 0x3f800007u)
            return exponentBits > 0 ? sign * 1e30f * 1e30f : sign * 1e-30f * 1e-30f;
        float t = absoluteBase - 1;
        float w = t * t * (0.5f - t * (0.333333333333f - t * 0.25f));
        float u = inverseLn2High * t;
        float v = t * inverseLn2Low - w * inverseLn2;
        t1 = makeFloat(getFloatBits(u + v) & 0xfffff000u);
        t2 = v - (t1 - u);
    } else {
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

    u32 magnitude = static_cast<u32>(productBits) & 0x7fffffffu;
    s32 magnitudeExponent = static_cast<s32>(magnitude >> 23) - 127;
    s32 integerExponent = 0;
    if (magnitude > 0x3f000000u) {
        u32 roundedBits = static_cast<u32>(productBits) + (0x00800000u >> (magnitudeExponent + 1));
        magnitudeExponent = static_cast<s32>((roundedBits & 0x7fffffffu) >> 23) - 127;
        float rounded = makeFloat(roundedBits & ~(0x007fffffu >> magnitudeExponent));
        integerExponent = static_cast<s32>(((roundedBits & 0x007fffffu) | 0x00800000u) >>
                                           (23 - magnitudeExponent));
        if (productBits < 0)
            integerExponent = -integerExponent;
        productHigh -= rounded;
    }
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
    if (resultExponent <= 0)
        z = scaleFloatByPowerOfTwo(z, integerExponent);
    else
        z = makeFloat(static_cast<u32>(zBits) + (static_cast<u32>(integerExponent) << 23));
    return sign * z;
}

float sin(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return rad;
        return sinKernel(rad);
    }
    if (magnitude <= 0x40e231d5u) {
        float absolute = negative ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = (quadrant & 1) ? cosKernel(reduced) : sinKernel(reduced);
        if (quadrant & 2)
            result = -result;
        return negative ? -result : result;
    }
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    switch (quadrant & 3) {
        case 0: return sinKernel(reduced);
        case 1: return cosKernel(reduced);
        case 2: return sinKernel(-reduced);
        default: return -cosKernel(reduced);
    }
}

float cos(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return 1;
        return cosKernel(rad);
    }
    if (magnitude <= 0x40e231d5u) {
        double absolute = (bits >> 31) ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = (quadrant & 1) ? -sinKernel(reduced) : cosKernel(reduced);
        return (quadrant & 2) ? -result : result;
    }
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    switch (quadrant & 3) {
        case 0: return cosKernel(reduced);
        case 1: return sinKernel(-reduced);
        case 2: return -cosKernel(reduced);
        default: return sinKernel(reduced);
    }
}

float tan(float rad) {
    static constexpr double piOverTwo = 1.57079632679489661923;
    u32 bits = getFloatBits(rad);
    u32 magnitude = bits & 0x7fffffffu;
    bool negative = (bits >> 31) != 0;
    if (magnitude <= 0x3f490fdau) {
        if (magnitude < 0x39800000u)
            return rad;
        return tanKernel(rad, false);
    }
    if (magnitude <= 0x40e231d5u) {
        double absolute = negative ? -rad : rad;
        s32 quadrant = static_cast<s32>(absolute / piOverTwo + 0.5);
        double reduced = absolute - quadrant * piOverTwo;
        float result = tanKernel(reduced, (quadrant & 1) != 0);
        return negative ? -result : result;
    }
    if (magnitude >= 0x7f800000u)
        return rad - rad;
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    return tanKernel(reduced, (quadrant & 1) != 0);
}

Float2 cosAndSin(float rad) {
    u32 magnitude = getFloatBits(rad) & 0x7fffffffu;
    if (magnitude <= 0x3f490fdau)
        return {cosKernel(rad), magnitude < 0x39800000u ? rad : sinKernel(rad)};
    if (magnitude >= 0x7f800000u) {
        float nan = rad - rad;
        return {nan, nan};
    }
    double reduced;
    s32 quadrant = magnitude < 0x4dc90fdbu ? reduce(rad, &reduced) : reduceLarge(rad, &reduced);
    float sine = sinKernel(reduced);
    float cosine = cosKernel(reduced);
    switch (quadrant & 3) {
        case 0: return {cosine, sine};
        case 1: return {-sine, cosine};
        case 2: return {-cosine, -sine};
        default: return {sine, -cosine};
    }
}

float arctan(float value) {
    static constexpr float high[] = {
        4.6364760399e-01f, 7.8539812565e-01f, 9.8279368877e-01f, 1.5707962513e+00f,
    };
    static constexpr float low[] = {
        5.0121582440e-09f, 3.7748947079e-08f, 3.4473217170e-08f, 7.5497894159e-08f,
    };
    static constexpr float coefficients[] = {
        3.3333328366e-01f, -1.9999158382e-01f, 1.4253635705e-01f,
        -1.0648017377e-01f, 6.1687607318e-02f,
    };

    u32 bits = getFloatBits(value);
    bool negative = (bits >> 31) != 0;
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x4c800000u) {
        if (magnitude > 0x7f800000u)
            return value;
        float result = high[3] + 7.52316384526264e-37f;
        return negative ? -result : result;
    }

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
    float z = value * value;
    float w = z * z;
    float odd = z * (coefficients[0] + w * (coefficients[2] + w * coefficients[4]));
    float even = w * (coefficients[1] + w * coefficients[3]);
    if (interval < 0)
        return value - value * (odd + even);
    z = high[interval] - ((value * (odd + even) - low[interval]) - value);
    return negative ? -z : z;
}

float arctan(const Float2& pos) {
    static constexpr float pi = 3.1415927410e+00f;
    static constexpr float piLow = -8.7422776573e-08f;
    float x = pos.x;
    float y = pos.y;
    u32 xBits = getFloatBits(x);
    u32 yBits = getFloatBits(y);
    u32 xMagnitude = xBits & 0x7fffffffu;
    u32 yMagnitude = yBits & 0x7fffffffu;
    if (xMagnitude > 0x7f800000u || yMagnitude > 0x7f800000u)
        return x + y;
    if (xBits == 0x3f800000u)
        return arctan(y);
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
    if (xMagnitude + (26u << 23) < yMagnitude || yMagnitude == 0x7f800000u)
        return quadrant & 1 ? -pi / 2 : pi / 2;
    float z = (quadrant & 2) && yMagnitude + (26u << 23) < xMagnitude
                  ? 0
                  : arctan(abs(y / x));
    switch (quadrant) {
        case 0: return z;
        case 1: return -z;
        case 2: return pi - (z - piLow);
        default: return (z - piLow) - pi;
    }
}

static float inverseSinRatio(float z) {
    static constexpr float P0 = 1.6666586697e-01f;
    static constexpr float P1 = -4.2743422091e-02f;
    static constexpr float P2 = -8.6563630030e-03f;
    static constexpr float Q1 = -7.0662963390e-01f;
    return z * (P0 + z * (P1 + z * P2)) / (1 + z * Q1);
}

float arcsin(float value) {
    static constexpr double piOverTwo = 1.570796326794896558;
    u32 bits = getFloatBits(value);
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x3f800000u) {
        if (magnitude == 0x3f800000u)
            return static_cast<float>(value * piOverTwo + 7.52316384526264e-37f);
        return (value - value) / (value - value);
    }
    if (magnitude < 0x3f000000u) {
        if (magnitude < 0x39800000u && magnitude >= 0x00800000u)
            return value;
        return value + value * inverseSinRatio(value * value);
    }
    float z = (1 - abs(value)) * 0.5f;
    double root = sqrt(z);
    float result = static_cast<float>(piOverTwo - 2 * (root + root * inverseSinRatio(z)));
    return bits >> 31 ? -result : result;
}

float arccos(float value) {
    static constexpr float piOverTwoHigh = 1.5707962513e+00f;
    static constexpr float piOverTwoLow = 7.5497894159e-08f;
    u32 bits = getFloatBits(value);
    u32 magnitude = bits & 0x7fffffffu;
    if (magnitude >= 0x3f800000u) {
        if (magnitude == 0x3f800000u)
            return bits >> 31 ? 2 * piOverTwoHigh + 7.52316384526264e-37f : 0;
        return (value - value) / (value - value);
    }
    if (magnitude < 0x3f000000u) {
        if (magnitude <= 0x32800000u)
            return piOverTwoHigh + 7.52316384526264e-37f;
        return piOverTwoHigh - (value - (piOverTwoLow - value * inverseSinRatio(value * value)));
    }
    if (bits >> 31) {
        float z = (1 + value) * 0.5f;
        float root = sqrt(z);
        float correction = inverseSinRatio(z) * root - piOverTwoLow;
        return 2 * (piOverTwoHigh - (root + correction));
    }
    float z = (1 - value) * 0.5f;
    float root = sqrt(z);
    float truncatedRoot = makeFloat(getFloatBits(root) & 0xfffff000u);
    float correction = (z - truncatedRoot * truncatedRoot) / (root + truncatedRoot);
    float remainder = inverseSinRatio(z) * root + correction;
    return 2 * (truncatedRoot + remainder);
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

//   ▄▄▄▄                 ▄▄   ▄▄▄▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄ ██  ██  ▄▄▄▄   ▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██   ██▀▀▀  ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██  ▀█▄▄ ██     ▀█▄▄█▀  ▄▄▄█▀
//      ▀▀

QuatPos QuatPos::inverted() const {
    Quaternion qi = quat.inverted();
    return {qi, qi * -pos};
}

QuatPos QuatPos::identity() {
    return {{0, 0, 0, 1}, {0, 0, 0}};
}

QuatPos QuatPos::translate(const Float3& pos) {
    return {{0, 0, 0, 1}, pos};
}

QuatPos QuatPos::rotate(const Float3& unitAxis, float radians) {
    return {Quaternion::fromAxisAngle(unitAxis, radians), {0, 0, 0}};
}

QuatPos QuatPos::fromOrtho(const Mat3x4& m) {
    return {Quaternion::fromOrtho(m.asMat3()), m[3]};
}

QuatPos QuatPos::fromOrtho(const Mat4x4& m) {
    return {Quaternion::fromOrtho(m), Float3{m[3]}};
}

} // namespace ply
