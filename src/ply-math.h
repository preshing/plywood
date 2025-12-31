/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#pragma once

#include "ply-base.h"
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <type_traits>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if PLY_CPU_X86 || PLY_CPU_X64
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

namespace ply {

static constexpr float Pi = 3.14159265358979323846f;
static constexpr double DPi = 3.14159265358979323846;

inline float square(float v) {
    return v * v;
}
inline float round_nearest(float x) {
#if PLY_CPU_ARM64
    return roundf(x);
#elif PLY_CPU_X86 || PLY_CPU_X64
    // Intrinsic version
    __m128 sign_bit = _mm_and_ps(_mm_set_ss(x), _mm_castsi128_ps(_mm_cvtsi32_si128(0x80000000u)));
    __m128 added = _mm_add_ss(_mm_set_ss(x), _mm_or_ps(sign_bit, _mm_castsi128_ps(_mm_cvtsi32_si128(0x3f000000u))));
    return (float) _mm_cvtt_ss2si(added);
#else
    // Non-intrinsic version
    PLY_PUN_GUARD;
    float frac = 0.5f;
    u32 s = (*(u32*) &x) & 0x80000000u;
    u32 c = (*(u32*) &frac) | s;
    return (float) (s32) (x + *(float*) &c);
#endif
}
inline float round_up(float value) {
    return ceilf(value);
}
inline float round_down(float value) {
    return floorf(value);
}
inline float wrap(float value, float range) {
    PLY_ASSERT(range > 0);
    float t = floorf(value / range);
    return value - t * range;
}
inline u16 float_to_half(const char* src_float) {
    u32 single = *(const u32*) src_float;
    // If exponent is less than -14, this will force the result to zero.
    u16 zero_mask = -(single + single >= 0x71000000);
    // Exponent and mantissa. Just assume exponent is small enough to avoid wrap around.
    u16 half = ((single >> 16) & 0x8000) | (((single >> 13) - 0x1c000) & 0x7fff);
    return half & zero_mask;
}
inline float mix(float a, float b, float t) {
    return a * (1 - t) + b * t;
}
inline float unmix(float a, float b, float mixed) {
    return (mixed - a) / (b - a);
}
inline float step_towards(float start, float target, float amount) {
    return start < target ? min(start + amount, target) : max(start - amount, target);
}

//  ▄▄                   ▄▄▄   ▄▄▄▄
//  ██▄▄▄   ▄▄▄▄   ▄▄▄▄   ██  ▀▀  ██
//  ██  ██ ██  ██ ██  ██  ██   ▄█▀▀
//  ██▄▄█▀ ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄ ██▄▄▄▄
//

struct Bool2 {
    bool x;
    bool y;

    Bool2(bool x, bool y) : x{x}, y{y} {
    }
};

inline bool all(const Bool2& v) {
    return v.x && v.y;
}
inline bool any(const Bool2& v) {
    return v.x || v.y;
}

//  ▄▄                   ▄▄▄   ▄▄▄▄
//  ██▄▄▄   ▄▄▄▄   ▄▄▄▄   ██  ▀▀  ██
//  ██  ██ ██  ██ ██  ██  ██    ▀▀█▄
//  ██▄▄█▀ ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄ ▀█▄▄█▀
//

struct Bool3 {
    bool x;
    bool y;
    bool z;

    Bool3(bool x, bool y, bool z) : x{x}, y{y}, z{z} {
    }
};

inline bool all(const Bool3& v) {
    return v.x && v.y && v.z;
}
inline bool any(const Bool3& v) {
    return v.x || v.y || v.z;
}

//  ▄▄                   ▄▄▄     ▄▄▄
//  ██▄▄▄   ▄▄▄▄   ▄▄▄▄   ██   ▄█▀██
//  ██  ██ ██  ██ ██  ██  ██  ██▄▄██▄
//  ██▄▄█▀ ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄     ██
//

struct Bool4 {
    bool x;
    bool y;
    bool z;
    bool w;

    Bool4(bool x, bool y, bool z, bool w) : x{x}, y{y}, z{z}, w{w} {
    }
};

inline bool all(const Bool4& v) {
    return v.x && v.y && v.z && v.w;
}
inline bool any(const Bool4& v) {
    return v.x || v.y || v.z || v.w;
}

//                        ▄▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄ ▀▀  ██
//  ▀█▄ ▄█▀ ██▄▄██ ██     ▄█▀▀
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄ ██▄▄▄▄
//

struct Float3;
struct Float4;
struct Int2;
struct Int3;
struct Int4;
struct Quaternion;
struct Color;

struct Float2 {
    float x;
    float y;

    Float2(float t = 0.f) : x{t}, y{t} {
    }
    Float2(float x, float y) : x{x}, y{y} {
    }

    explicit operator Int2() const;
    float& operator[](u32 index) {
        PLY_ASSERT(index < 2);
        PLY_PUN_GUARD;
        return ((float*) this)[index];
    }
    float operator[](u32 index) const {
        PLY_ASSERT(index < 2);
        PLY_PUN_GUARD;
        return ((const float*) this)[index];
    }
    float length_squared() const {
        return x * x + y * y;
    }
    float length() const {
        return sqrtf(length_squared());
    }
    bool is_unit_length() const {
        return ply::abs(length_squared() - 1.f) < 0.001f;
    }
    PLY_NO_DISCARD Float2 normalized() const;
    PLY_NO_DISCARD Float2 safe_normalized(const Float2& fallback = {1, 0}, float epsilon = 1e-6f) const;
    float& r() {
        return x;
    }
    float r() const {
        return x;
    }
    float& g() {
        return y;
    }
    float g() const {
        return y;
    }
    PLY_NO_DISCARD Float2 swizzle(u32 i0, u32 i1) const;
    PLY_NO_DISCARD Float3 swizzle(u32 i0, u32 i1, u32 i2) const;
    PLY_NO_DISCARD Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const;
};

inline bool operator==(const Float2& a, const Float2& b) {
    return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const Float2& a, const Float2& b) {
    return !(a == b);
}
inline Bool2 operator<(const Float2& a, const Float2& b) {
    return {a.x < b.x, a.y < b.y};
}
inline Bool2 operator<=(const Float2& a, const Float2& b) {
    return {a.x <= b.x, a.y <= b.y};
}
inline Bool2 operator>(const Float2& a, const Float2& b) {
    return {a.x > b.x, a.y > b.y};
}
inline Bool2 operator>=(const Float2& a, const Float2& b) {
    return {a.x >= b.x, a.y >= b.y};
}
inline Float2 operator-(const Float2& a) {
    return {-a.x, -a.y};
}
inline Float2 operator+(const Float2& a, const Float2& b) {
    return {a.x + b.x, a.y + b.y};
}
inline Float2 operator-(const Float2& a, const Float2& b) {
    return {a.x - b.x, a.y - b.y};
}
inline Float2 operator*(const Float2& a, const Float2& b) {
    return {a.x * b.x, a.y * b.y};
}
inline Float2 operator/(const Float2& a, const Float2& b) {
    return {a.x / b.x, a.y / b.y};
}
inline Float2 operator/(const Float2& a, float b) {
    float oob = 1.f / b;
    return {a.x * oob, a.y * oob};
}
inline void operator+=(Float2& a, const Float2& b) {
    a.x += b.x;
    a.y += b.y;
}
inline void operator-=(Float2& a, const Float2& b) {
    a.x -= b.x;
    a.y -= b.y;
}
inline void operator*=(Float2& a, const Float2& b) {
    a.x *= b.x;
    a.y *= b.y;
}
inline void operator/=(Float2& a, const Float2& b) {
    a.x /= b.x;
    a.y /= b.y;
}
inline void operator/=(Float2& a, float b) {
    float oob = 1.f / b;
    a.x *= oob;
    a.y *= oob;
}
inline float dot(const Float2& a, const Float2& b) {
    return a.x * b.x + a.y * b.y;
}
inline float cross(const Float2& a, const Float2& b) {
    return a.x * b.y - a.y * b.x;
}
inline Float2 clamp(const Float2& v, const Float2& mins, const Float2& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y)};
}
inline Float2 abs(const Float2& a) {
    return {abs(a.x), abs(a.y)};
}
inline Float2 pow(const Float2& a, const Float2& b) {
    return {powf(a.x, b.x), powf(a.y, b.y)};
}
inline Float2 min(const Float2& a, const Float2& b) {
    return {min(a.x, b.x), min(a.y, b.y)};
}
inline Float2 max(const Float2& a, const Float2& b) {
    return {max(a.x, b.x), max(a.y, b.y)};
}
Float2 round_up(const Float2& value);
Float2 round_down(const Float2& value);
Float2 round_nearest(const Float2& value);
inline Float2 mix(const Float2& a, const Float2& b, const Float2& t) {
    return a * (1 - t) + b * t;
}
inline Float2 unmix(const Float2& a, const Float2& b, const Float2& mixed) {
    return (mixed - a) / (b - a);
}
Float2 step_towards(const Float2& start, const Float2& target, float amount);

//                        ▄▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄ ▀▀  ██
//  ▀█▄ ▄█▀ ██▄▄██ ██      ▀▀█▄
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄ ▀█▄▄█▀
//

struct Float3 {
    float x;
    float y;
    float z;

    Float3() = default;
    Float3(float t) : x{t}, y{t}, z{t} {
    }
    Float3(float x, float y, float z) : x{x}, y{y}, z{z} {
    }
    Float3(const Float2& v, float z) : x{v.x}, y{v.y}, z{z} {
    }
    Float3(const Color& c);

    explicit operator Float2() const {
        return {x, y};
    }
    explicit operator Int2() const;
    explicit operator Int3() const;
    float& operator[](u32 index) {
        PLY_ASSERT(index < 3);
        PLY_PUN_GUARD;
        return ((float*) this)[index];
    }
    float operator[](u32 index) const {
        PLY_ASSERT(index < 3);
        PLY_PUN_GUARD;
        return ((const float*) this)[index];
    }
    float length_squared() const {
        return x * x + y * y + z * z;
    }
    float length() const {
        return sqrtf(length_squared());
    }
    bool is_unit_length() const {
        return abs(length_squared() - 1.f) < 0.001f;
    }
    PLY_NO_DISCARD Float3 normalized() const;
    PLY_NO_DISCARD Float3 safe_normalized(const Float3& fallback = {1, 0, 0}, float epsilon = 1e-9f) const;
    float& r() {
        return x;
    }
    float r() const {
        return x;
    }
    float& g() {
        return y;
    }
    float g() const {
        return y;
    }
    float& b() {
        return z;
    }
    float b() const {
        return z;
    }
    PLY_NO_DISCARD Float2 swizzle(u32 i0, u32 i1) const;
    PLY_NO_DISCARD Float3 swizzle(u32 i0, u32 i1, u32 i2) const;
    PLY_NO_DISCARD Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const;
};

inline bool operator==(const Float3& a, const Float3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
inline bool operator!=(const Float3& a, const Float3& b) {
    return !(a == b);
}
inline Bool3 operator<(const Float3& a, const Float3& b) {
    return {a.x < b.x, a.y < b.y, a.z < b.z};
}
inline Bool3 operator<=(const Float3& a, const Float3& b) {
    return {a.x <= b.x, a.y <= b.y, a.z <= b.z};
}
inline Bool3 operator>(const Float3& a, const Float3& b) {
    return {a.x > b.x, a.y > b.y, a.z > b.z};
}
inline Bool3 operator>=(const Float3& a, const Float3& b) {
    return {a.x >= b.x, a.y >= b.y, a.z >= b.z};
}
inline Float3 operator-(const Float3& a) {
    return {-a.x, -a.y, -a.z};
}
inline Float3 operator+(const Float3& a, const Float3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Float3 operator-(const Float3& a, const Float3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Float3 operator*(const Float3& a, const Float3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}
inline Float3 operator/(const Float3& a, const Float3& b) {
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}
inline Float3 operator/(const Float3& a, float b) {
    float oob = 1.f / b;
    return {a.x * oob, a.y * oob, a.z * oob};
}
inline void operator+=(Float3& a, const Float3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}
inline void operator-=(Float3& a, const Float3& b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
}
inline void operator*=(Float3& a, const Float3& b) {
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
}
inline void operator/=(Float3& a, const Float3& b) {
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
}
inline void operator/=(Float3& a, float b) {
    float oob = 1.f / b;
    a.x *= oob;
    a.y *= oob;
    a.z *= oob;
}
inline float dot(const Float3& a, const Float3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Float3 cross(const Float3& a, const Float3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Float3 clamp(const Float3& v, const Float3& mins, const Float3& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y), clamp(v.z, mins.z, maxs.z)};
}
inline Float3 abs(const Float3& a) {
    return {abs(a.x), abs(a.y), abs(a.z)};
}
Float3 pow(const Float3& a, const Float3& b);
inline Float3 min(const Float3& a, const Float3& b) {
    return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
inline Float3 max(const Float3& a, const Float3& b) {
    return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}
Float3 round_up(const Float3& value);
Float3 round_down(const Float3& value);
Float3 round_nearest(const Float3& value);
inline Float3 mix(const Float3& a, const Float3& b, const Float3& t) {
    return a * (1 - t) + b * t;
}
inline Float3 unmix(const Float3& a, const Float3& b, const Float3& mixed) {
    return (mixed - a) / (b - a);
}
Float3 step_towards(const Float3& start, const Float3& target, float amount);
inline Float3 get_noncollinear(const Float3& unit_vec) {
    return square(unit_vec.z) < 0.9f ? Float3{0, 0, 1} : Float3{0, -1, 0};
}

//                          ▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄  ▄█▀██
//  ▀█▄ ▄█▀ ██▄▄██ ██    ██▄▄██▄
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄     ██
//

struct Float4 {
    float x;
    float y;
    float z;
    float w;

    Float4() = default;
    Float4(float t) : x{t}, y{t}, z{t}, w{t} {
    }
    Float4(float x, float y, float z, float w) : x{x}, y{y}, z{z}, w{w} {
    }
    Float4(const Float3& v, float w) : x{v.x}, y{v.y}, z{v.z}, w{w} {
    }
    Float4(const Float2& v, float z, float w) : x{v.x}, y{v.y}, z{z}, w{w} {
    }

    explicit operator Float2() const {
        return {x, y};
    }
    explicit operator Float3() const {
        return {x, y, z};
    }
    explicit operator Int2() const;
    explicit operator Int3() const;
    explicit operator Int4() const;
    explicit operator Quaternion() const;
    explicit operator Color() const;
    float& operator[](u32 index) {
        PLY_ASSERT(index < 4);
        PLY_PUN_GUARD;
        return ((float*) this)[index];
    }
    float operator[](u32 index) const {
        PLY_ASSERT(index < 4);
        PLY_PUN_GUARD;
        return ((const float*) this)[index];
    }
    float length_squared() const {
        return x * x + y * y + z * z + w * w;
    }
    float length() const {
        return sqrtf(length_squared());
    }
    bool is_unit_length() const {
        return abs(length_squared() - 1.f) < 0.001f;
    }
    PLY_NO_DISCARD Float4 normalized() const;
    PLY_NO_DISCARD Float4 safe_normalized(const Float4& fallback = {1, 0, 0, 0}, float epsilon = 1e-9f) const;
    float& r() {
        return x;
    }
    float r() const {
        return x;
    }
    float& g() {
        return y;
    }
    float g() const {
        return y;
    }
    float& b() {
        return z;
    }
    float b() const {
        return z;
    }
    float& a() {
        return w;
    }
    float a() const {
        return w;
    }
    PLY_NO_DISCARD Float2 swizzle(u32 i0, u32 i1) const;
    PLY_NO_DISCARD Float3 swizzle(u32 i0, u32 i1, u32 i2) const;
    PLY_NO_DISCARD Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const;
};

inline bool operator==(const Float4& a, const Float4& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const Float4& a, const Float4& b) {
    return !(a == b);
}
inline Bool4 operator<(const Float4& a, const Float4& b) {
    return {a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w};
}
inline Bool4 operator<=(const Float4& a, const Float4& b) {
    return {a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w};
}
inline Bool4 operator>(const Float4& a, const Float4& b) {
    return {a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w};
}
inline Bool4 operator>=(const Float4& a, const Float4& b) {
    return {a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w};
}
inline Float4 operator-(const Float4& a) {
    return {-a.x, -a.y, -a.z, -a.w};
}
inline Float4 operator+(const Float4& a, const Float4& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline Float4 operator-(const Float4& a, const Float4& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
inline Float4 operator*(const Float4& a, const Float4& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}
inline Float4 operator/(const Float4& a, const Float4& b) {
    return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
}
inline Float4 operator/(const Float4& a, float b) {
    float oob = 1.f / b;
    return {a.x * oob, a.y * oob, a.z * oob, a.w * oob};
}
inline void operator+=(Float4& a, const Float4& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
}
inline void operator-=(Float4& a, const Float4& b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    a.w -= b.w;
}
inline void operator*=(Float4& a, const Float4& b) {
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    a.w *= b.w;
}
inline void operator/=(Float4& a, const Float4& b) {
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    a.w /= b.w;
}
inline void operator/=(Float4& a, float b) {
    float oob = 1.f / b;
    a.x *= oob;
    a.y *= oob;
    a.z *= oob;
    a.w *= oob;
}
inline float dot(const Float4& a, const Float4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
inline Float4 clamp(const Float4& v, const Float4& mins, const Float4& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y), clamp(v.z, mins.z, maxs.z),
            clamp(v.w, mins.w, maxs.w)};
}
inline Float4 abs(const Float4& a) {
    return {abs(a.x), abs(a.y), abs(a.z), abs(a.w)};
}
Float4 pow(const Float4& a, const Float4& b);
inline Float4 min(const Float4& a, const Float4& b) {
    return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
}
inline Float4 max(const Float4& a, const Float4& b) {
    return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)};
}
Float4 round_up(const Float4& vec);
Float4 round_down(const Float4& vec);
Float4 round_nearest(const Float4& vec);
inline Float4 mix(const Float4& a, const Float4& b, const Float4& t) {
    return a * (1 - t) + b * t;
}
inline Float4 unmix(const Float4& a, const Float4& b, const Float4& mixed) {
    return (mixed - a) / (b - a);
}
Float4 step_towards(const Float4& start, const Float4& target, float amount);

// Swizzle functions

inline PLY_NO_DISCARD Float2 Float2::swizzle(u32 i0, u32 i1) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 2 && i1 < 2);
    return {v[i0], v[i1]};
}
inline PLY_NO_DISCARD Float3 Float2::swizzle(u32 i0, u32 i1, u32 i2) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 2 && i1 < 2 && i2 < 2);
    return {v[i0], v[i1], v[i2]};
}
inline PLY_NO_DISCARD Float4 Float2::swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 2 && i1 < 2 && i2 < 2 && i3 < 2);
    return {v[i0], v[i1], v[i2], v[i3]};
}
inline PLY_NO_DISCARD Float2 Float3::swizzle(u32 i0, u32 i1) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 3 && i1 < 3);
    return {v[i0], v[i1]};
}
inline PLY_NO_DISCARD Float3 Float3::swizzle(u32 i0, u32 i1, u32 i2) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 3 && i1 < 3 && i2 < 3);
    return {v[i0], v[i1], v[i2]};
}
inline PLY_NO_DISCARD Float4 Float3::swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 3 && i1 < 3 && i2 < 3 && i2 < 3);
    return {v[i0], v[i1], v[i2], v[i3]};
}
inline PLY_NO_DISCARD Float2 Float4::swizzle(u32 i0, u32 i1) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 4 && i1 < 4);
    return {v[i0], v[i1]};
}
inline PLY_NO_DISCARD Float3 Float4::swizzle(u32 i0, u32 i1, u32 i2) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 4 && i1 < 4 && i2 < 4);
    return {v[i0], v[i1], v[i2]};
}
inline PLY_NO_DISCARD Float4 Float4::swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const {
    PLY_PUN_GUARD;
    auto* v = (const float*) this;
    PLY_ASSERT(i0 < 4 && i1 < 4 && i2 < 4 && i2 < 4);
    return {v[i0], v[i1], v[i2], v[i3]};
}

//   ▄▄          ▄▄
//  ▄██▄▄ ▄▄▄▄▄  ▄▄  ▄▄▄▄▄
//   ██   ██  ▀▀ ██ ██  ██
//   ▀█▄▄ ██     ██ ▀█▄▄██
//                   ▄▄▄█▀

inline float fast_sin_part(float x) {
    float val = 4 * x * (abs(x) - 1);
    return val * (0.225f * abs(val) + 0.775f);
}
inline float fast_sin(float rad) {
    float frac = rad * (0.5f / Pi);
    return fast_sin_part((frac - floorf(frac)) * 2 - 1);
}
inline float fast_cos(float rad) {
    return fast_sin(rad + (Pi * 0.5f));
}
inline Float2 fast_cos_sin(float rad) {
    return {fast_cos(rad), fast_sin(rad)};
}

//  ▄▄▄▄▄                ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄ ▄██▄▄
//  ██▀▀█▄ ██▄▄██ ██     ██
//  ██  ██ ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄
//

struct IntRect;

struct Rect {
    Float2 mins;
    Float2 maxs;

    Rect() = default;
    Rect(float v) : mins{v}, maxs{v} {
    }
    Rect(const Float2& v) : mins{v}, maxs{v} {
    }
    Rect(const Float2& mins, const Float2& maxs) : mins{mins}, maxs{maxs} {
    }
    Rect(float min_x, float min_y, float max_x, float max_y) : mins{min_x, min_y}, maxs{max_x, max_y} {
    }
    static Rect from_size(const Float2& mins, const Float2& size) {
        return {mins, mins + size};
    }
    static Rect empty() {
        return {get_max_value<float>(), get_min_value<float>()};
    }
    static Rect full() {
        return {get_min_value<float>(), get_max_value<float>()};
    }

    explicit operator IntRect() const;
    Float2 size() const {
        return maxs - mins;
    }
    bool is_empty() const {
        return any(maxs <= mins);
    }
    float width() const {
        return maxs.x - mins.x;
    }
    float height() const {
        return maxs.y - mins.y;
    }
    Float2 mix(const Float2& arg) const {
        return ply::mix(mins, maxs, arg);
    }
    Float2 unmix(const Float2& arg) const {
        return ply::unmix(mins, maxs, arg);
    }
    Float2 mid() const {
        return (mins + maxs) * 0.5f;
    }
    Rect mix(const Rect& arg) const {
        return {mix(arg.mins), mix(arg.maxs)};
    }
    Rect unmix(const Rect& arg) const {
        return {unmix(arg.mins), unmix(arg.maxs)};
    }
    Float2 clamp(const Float2& arg) const {
        return ply::clamp(arg, mins, maxs);
    }
    Float2 top_left() const {
        return Float2{mins.x, maxs.y};
    }
    Float2 bottom_right() const {
        return Float2{maxs.x, mins.y};
    }
    bool contains(const Float2& arg) const {
        return all(mins <= arg) && all(arg < maxs);
    }
    bool contains(const Rect& arg) const {
        return all(mins <= arg.mins) && all(arg.maxs <= maxs);
    }
    bool intersects(const Rect& arg) const;
};

inline bool operator==(const Rect& a, const Rect& b) {
    return (a.mins == b.mins) && (a.maxs == b.maxs);
}
inline bool operator!=(const Rect& a, const Rect& b) {
    return !(a == b);
}
inline Rect operator+(const Rect& a, const Rect& b) {
    return {a.mins + b.mins, a.maxs + b.maxs};
}
inline void operator+=(Rect& a, const Rect& b) {
    a.mins += b.mins;
    a.maxs += b.maxs;
}
inline Rect operator-(const Rect& a, const Rect& b) {
    return {a.mins - b.mins, a.maxs - b.maxs};
}
inline void operator-=(Rect& a, const Rect& b) {
    a.mins -= b.mins;
    a.maxs -= b.maxs;
}
inline Rect operator*(const Rect& a, const Rect& b) {
    return {a.mins * b.mins, a.maxs * b.maxs};
}
inline void operator*=(Rect& a, const Rect& b) {
    a.mins *= b.mins;
    a.maxs *= b.maxs;
}
inline Rect operator/(const Rect& a, const Rect& b) {
    return {a.mins / b.mins, a.maxs / b.maxs};
}
inline void operator/=(Rect& a, const Rect& b) {
    a.mins /= b.mins;
    a.maxs /= b.maxs;
}
inline Rect make_union(const Rect& a, const Rect& b) {
    return {min(a.mins, b.mins), max(a.maxs, b.maxs)};
}
inline Rect intersect(const Rect& a, const Rect& b) {
    return {max(a.mins, b.mins), min(a.maxs, b.maxs)};
}
inline bool Rect::intersects(const Rect& arg) const {
    return !intersect(*this, arg).is_empty();
}
inline Rect inflate(const Rect& a, const Float2& b) {
    return {a.mins - b, a.maxs + b};
}
inline Rect round_nearest(const Rect& a) {
    return {round_nearest(a.mins), round_nearest(a.maxs)};
}
Rect rect_from_fov(float fov_y, float aspect);

//  ▄▄▄▄▄                 ▄▄▄▄  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄  ▄▄ ▀▀  ██ ██  ██
//  ██▀▀█▄ ██  ██  ▀██▀    ▀▀█▄ ██  ██
//  ██▄▄█▀ ▀█▄▄█▀ ▄█▀▀█▄ ▀█▄▄█▀ ██▄▄█▀
//

struct AABB {
    Float3 mins;
    Float3 maxs;

    AABB() = default;
    AABB(const Float3& v) : mins{v}, maxs{v} {
    }
    AABB(const Float3& mins, const Float3& maxs) : mins{mins}, maxs{maxs} {
    }
    AABB(float min_x, float min_y, float max_x, float max_y) : mins{min_x, min_y}, maxs{max_x, max_y} {
    }
    static AABB empty() {
        return {get_max_value<float>(), get_min_value<float>()};
    }
    static AABB full() {
        return {get_min_value<float>(), get_max_value<float>()};
    }
    static AABB from_size(const Float3& mins, const Float3& size) {
        return {mins, mins + size};
    }

    Float3 size() const {
        return maxs - mins;
    }
    bool is_empty() const {
        return any(maxs <= mins);
    }
    float width() const {
        return maxs.x - mins.x;
    }
    float height() const {
        return maxs.y - mins.y;
    }
    float depth() const {
        return maxs.z - mins.z;
    }
    Float3 mix(const Float3& arg) const {
        return ply::mix(mins, maxs, arg);
    }
    Float3 unmix(const Float3& arg) const {
        return ply::unmix(mins, maxs, arg);
    }
    Float3 mid() const {
        return (mins + maxs) * 0.5f;
    }
    AABB mix(const AABB& arg) const {
        return {mix(arg.mins), mix(arg.maxs)};
    }
    AABB unmix(const AABB& arg) const {
        return {unmix(arg.mins), unmix(arg.maxs)};
    }
    Float3 clamp(const Float3& arg) const {
        return ply::clamp(arg, mins, maxs);
    }
    Float3 top_left() const {
        return Float3{mins.x, maxs.y};
    }
    Float3 bottom_right() const {
        return Float3{maxs.x, mins.y};
    }
    bool contains(const Float3& arg) const {
        return all(mins <= arg) && all(arg < maxs);
    }
    bool contains(const AABB& arg) const {
        return all(mins <= arg.mins) && all(arg.maxs <= maxs);
    }
    bool intersects(const AABB& arg) const;
};

inline bool operator==(const AABB& a, const AABB& b) {
    return (a.mins == b.mins) && (a.maxs == b.maxs);
}
inline bool operator!=(const AABB& a, const AABB& b) {
    return !(a == b);
}
inline AABB operator+(const AABB& a, const AABB& b) {
    return {a.mins + b.mins, a.maxs + b.maxs};
}
inline void operator+=(AABB& a, const AABB& b) {
    a.mins += b.mins;
    a.maxs += b.maxs;
}
inline AABB operator-(const AABB& a, const AABB& b) {
    return {a.mins - b.mins, a.maxs - b.maxs};
}
inline void operator-=(AABB& a, const AABB& b) {
    a.mins -= b.mins;
    a.maxs -= b.maxs;
}
inline AABB operator*(const AABB& a, const AABB& b) {
    return {a.mins * b.mins, a.maxs * b.maxs};
}
inline void operator*=(AABB& a, const AABB& b) {
    a.mins *= b.mins;
    a.maxs *= b.maxs;
}
inline AABB operator/(const AABB& a, const AABB& b) {
    return {a.mins / b.mins, a.maxs / b.maxs};
}
inline void operator/=(AABB& a, const AABB& b) {
    a.mins /= b.mins;
    a.maxs /= b.maxs;
}
inline AABB make_union(const AABB& a, const AABB& b) {
    return {min(a.mins, b.mins), max(a.maxs, b.maxs)};
}
inline AABB intersect(const AABB& a, const AABB& b) {
    return {max(a.mins, b.mins), min(a.maxs, b.maxs)};
}
inline bool AABB::intersects(const AABB& arg) const {
    return !intersect(*this, arg).is_empty();
}
inline AABB inflate(const AABB& a, const Float3& b) {
    return {a.mins - b, a.maxs + b};
}
inline AABB round_nearest(const AABB& a) {
    return {round_nearest(a.mins), round_nearest(a.maxs)};
}

//  ▄▄         ▄▄    ▄▄▄▄
//  ▄▄ ▄▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██ ██  ██  ██    ▄█▀▀
//  ██ ██  ██  ▀█▄▄ ██▄▄▄▄
//

struct Int2 {
    int x;
    int y;

    Int2() = default;
    Int2(int x) : x{x}, y{x} {
    }
    Int2(int x, int y) : x{x}, y{y} {
    }

    explicit operator Float2() const {
        return {(float) x, (float) y};
    }
};

inline Float2::operator Int2() const {
    return {(int) x, (int) y};
}
inline Float3::operator Int2() const {
    return {(int) x, (int) y};
}
inline Float4::operator Int2() const {
    return {(int) x, (int) y};
}
inline bool operator==(const Int2& a, const Int2& b) {
    return (a.x == b.x) && (a.y == b.y);
}
inline bool operator!=(const Int2& a, const Int2& b) {
    return !(a == b);
}
inline Bool2 operator<(const Int2& a, const Int2& b) {
    return {a.x < b.x, a.y < b.y};
}
inline Bool2 operator<=(const Int2& a, const Int2& b) {
    return {a.x <= b.x, a.y <= b.y};
}
inline Bool2 operator>(const Int2& a, const Int2& b) {
    return {a.x > b.x, a.y > b.y};
}
inline Bool2 operator>=(const Int2& a, const Int2& b) {
    return {a.x >= b.x, a.y >= b.y};
}
inline Int2 operator-(const Int2& a) {
    return {-a.x, -a.y};
}
inline Int2 operator+(const Int2& a, const Int2& b) {
    return {a.x + b.x, a.y + b.y};
}
inline void operator+=(Int2& a, const Int2& b) {
    a.x += b.x;
    a.y += b.y;
}
inline Int2 operator-(const Int2& a, const Int2& b) {
    return {a.x - b.x, a.y - b.y};
}
inline void operator-=(Int2& a, const Int2& b) {
    a.x -= b.x;
    a.y -= b.y;
}
inline Int2 operator*(const Int2& a, const Int2& b) {
    return {a.x * b.x, a.y * b.y};
}
inline void operator*=(Int2& a, const Int2& b) {
    a.x *= b.x;
    a.y *= b.y;
}
inline Int2 operator/(const Int2& a, const Int2& b) {
    return {a.x / b.x, a.y / b.y};
}
inline void operator/=(Int2& a, const Int2& b) {
    a.x /= b.x;
    a.y /= b.y;
}
inline Int2 clamp(const Int2& v, const Int2& mins, const Int2& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y)};
}
inline Int2 abs(const Int2& a) {
    return {abs(a.x), abs(a.y)};
}
inline Int2 min(const Int2& a, const Int2& b) {
    return Int2{min(a.x, b.x), min(a.y, b.y)};
}
inline Int2 max(const Int2& a, const Int2& b) {
    return Int2{max(a.x, b.x), max(a.y, b.y)};
}

//  ▄▄         ▄▄    ▄▄▄▄
//  ▄▄ ▄▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██ ██  ██  ██     ▀▀█▄
//  ██ ██  ██  ▀█▄▄ ▀█▄▄█▀
//

struct Int3 {
    int x;
    int y;
    int z;

    Int3() = default;
    Int3(int x) : x{x}, y{x}, z{x} {
    }
    Int3(int x, int y, int z) : x{x}, y{y}, z{z} {
    }

    explicit operator Int2() const {
        return {x, y};
    }
    explicit operator Float2() const {
        return {(float) x, (float) y};
    }
    explicit operator Float3() const {
        return {(float) x, (float) y, (float) z};
    }
};

inline Float3::operator Int3() const {
    return {(int) x, (int) y, (int) z};
}
inline Float4::operator Int3() const {
    return {(int) x, (int) y, (int) z};
}
inline bool operator==(const Int3& a, const Int3& b) {
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}
inline bool operator!=(const Int3& a, const Int3& b) {
    return !(a == b);
}
inline Bool3 operator<(const Int3& a, const Int3& b) {
    return {a.x < b.x, a.y < b.y, a.z < b.z};
}
inline Bool3 operator<=(const Int3& a, const Int3& b) {
    return {a.x <= b.x, a.y <= b.y, a.z <= b.z};
}
inline Bool3 operator>(const Int3& a, const Int3& b) {
    return {a.x > b.x, a.y > b.y, a.z > b.z};
}
inline Bool3 operator>=(const Int3& a, const Int3& b) {
    return {a.x >= b.x, a.y >= b.y, a.z >= b.z};
}
inline Int3 operator-(const Int3& a) {
    return {-a.x, -a.y, -a.z};
}
inline Int3 operator+(const Int3& a, const Int3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline void operator+=(Int3& a, const Int3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}
inline Int3 operator-(const Int3& a, const Int3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline void operator-=(Int3& a, const Int3& b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
}
inline Int3 operator*(const Int3& a, const Int3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}
inline void operator*=(Int3& a, const Int3& b) {
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
}
inline Int3 operator/(const Int3& a, const Int3& b) {
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}
inline void operator/=(Int3& a, const Int3& b) {
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
}
inline Int3 clamp(const Int3& v, const Int3& mins, const Int3& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y), clamp(v.z, mins.z, maxs.z)};
}
inline Int3 abs(const Int3& a) {
    return {abs(a.x), abs(a.y), abs(a.z)};
}
inline Int3 min(const Int3& a, const Int3& b) {
    return Int3{min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
inline Int3 max(const Int3& a, const Int3& b) {
    return Int3{max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}

//  ▄▄         ▄▄      ▄▄▄
//  ▄▄ ▄▄▄▄▄  ▄██▄▄  ▄█▀██
//  ██ ██  ██  ██   ██▄▄██▄
//  ██ ██  ██  ▀█▄▄     ██
//

struct Int4 {
    int x;
    int y;
    int z;
    int w;

    Int4() = default;
    Int4(int x, int y, int z, int w) : x{x}, y{y}, z{z}, w{w} {
    }

    explicit operator Int2() const {
        return {x, y};
    }
    explicit operator Int3() const {
        return {x, y, z};
    }
    explicit operator Float2() const {
        return {(float) x, (float) y};
    }
    explicit operator Float3() const {
        return {(float) x, (float) y, (float) z};
    }
    explicit operator Float4() const {
        return {(float) x, (float) y, (float) z, (float) w};
    }
};

inline Float4::operator Int4() const {
    return {(int) x, (int) y, (int) z, (int) w};
}
inline bool operator==(const Int4& a, const Int4& b) {
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w);
}
inline bool operator!=(const Int4& a, const Int4& b) {
    return !(a == b);
}
inline Bool4 operator<(const Int4& a, const Int4& b) {
    return {a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w};
}
inline Bool4 operator<=(const Int4& a, const Int4& b) {
    return {a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w};
}
inline Bool4 operator>(const Int4& a, const Int4& b) {
    return {a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w};
}
inline Bool4 operator>=(const Int4& a, const Int4& b) {
    return {a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w};
}
inline Int4 operator-(const Int4& a) {
    return {-a.x, -a.y, -a.z, -a.w};
}
inline Int4 operator+(const Int4& a, const Int4& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
inline void operator+=(Int4& a, const Int4& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
}
inline Int4 operator-(const Int4& a, const Int4& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
inline void operator-=(Int4& a, const Int4& b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    a.w -= b.w;
}
inline Int4 operator*(const Int4& a, const Int4& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}
inline void operator*=(Int4& a, const Int4& b) {
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    a.w *= b.w;
}
inline Int4 operator/(const Int4& a, const Int4& b) {
    return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
}
inline void operator/=(Int4& a, const Int4& b) {
    a.x /= b.x;
    a.y /= b.y;
    a.z /= b.z;
    a.w /= b.w;
}
inline Int4 clamp(const Int4& v, const Int4& mins, const Int4& maxs) {
    return {clamp(v.x, mins.x, maxs.x), clamp(v.y, mins.y, maxs.y), clamp(v.z, mins.z, maxs.z),
            clamp(v.w, mins.w, maxs.w)};
}
inline Int4 abs(const Int4& a) {
    return {abs(a.x), abs(a.y), abs(a.z), abs(a.w)};
}
inline Int4 min(const Int4& a, const Int4& b) {
    return Int4{min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
}
inline Int4 max(const Int4& a, const Int4& b) {
    return Int4{max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)};
}

//  ▄▄▄▄         ▄▄         ▄▄▄▄▄                ▄▄
//   ██  ▄▄▄▄▄  ▄██▄▄       ██  ██  ▄▄▄▄   ▄▄▄▄ ▄██▄▄
//   ██  ██  ██  ██         ██▀▀█▄ ██▄▄██ ██     ██
//  ▄██▄ ██  ██  ▀█▄▄ ▄▄▄▄▄ ██  ██ ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄
//

struct IntRect {
    Int2 mins;
    Int2 maxs;

    IntRect() = default;
    IntRect(const Int2& v) : mins{v}, maxs{v} {
    }
    IntRect(const Int2& mins, const Int2& maxs) : mins{mins}, maxs{maxs} {
    }
    IntRect(int min_x, int min_y, int max_x, int max_y) : mins{min_x, min_y}, maxs{max_x, max_y} {
    }
    static IntRect from_size(const Int2& mins, const Int2& size) {
        return {mins, mins + size};
    }
    static IntRect empty() {
        return {get_max_value<int>(), get_min_value<int>()};
    }
    static IntRect full() {
        return {get_min_value<int>(), get_max_value<int>()};
    }

    explicit operator Rect() const {
        return {Float2{mins}, Float2{maxs}};
    }
    Int2 size() const {
        return maxs - mins;
    }
    bool is_empty() const {
        return any(maxs <= mins);
    }
    int width() const {
        return maxs.x - mins.x;
    }
    int height() const {
        return maxs.y - mins.y;
    }
    Int2 mid() const {
        return (mins + maxs) / 2;
    }
    Int2 clamp(const Int2& arg) const {
        return ply::clamp(arg, mins, maxs);
    }
    Int2 top_left() const {
        return Int2{mins.x, maxs.y};
    }
    Int2 bottom_right() const {
        return Int2{maxs.x, mins.y};
    }
    bool contains(const Int2& arg) const {
        return all(mins <= arg) && all(arg < maxs);
    }
    bool contains(const IntRect& arg) const {
        return all(mins <= arg.mins) && all(arg.maxs <= maxs);
    }
    bool intersects(const IntRect& arg) const;
};

inline Rect::operator IntRect() const {
    return {Int2{mins}, Int2{maxs}};
}
inline bool operator==(const IntRect& a, const IntRect& b) {
    return (a.mins == b.mins) && (a.maxs == b.maxs);
}
inline bool operator!=(const IntRect& a, const IntRect& b) {
    return !(a == b);
}
inline IntRect operator+(const IntRect& a, const IntRect& b) {
    return {a.mins + b.mins, a.maxs + b.maxs};
}
inline void operator+=(IntRect& a, const IntRect& b) {
    a.mins += b.mins;
    a.maxs += b.maxs;
}
inline IntRect operator-(const IntRect& a, const IntRect& b) {
    return {a.mins - b.mins, a.maxs - b.maxs};
}
inline void operator-=(IntRect& a, const IntRect& b) {
    a.mins -= b.mins;
    a.maxs -= b.maxs;
}
inline IntRect operator*(const IntRect& a, const IntRect& b) {
    return {a.mins * b.mins, a.maxs * b.maxs};
}
inline void operator*=(IntRect& a, const IntRect& b) {
    a.mins *= b.mins;
    a.maxs *= b.maxs;
}
inline IntRect operator/(const IntRect& a, const IntRect& b) {
    return {a.mins / b.mins, a.maxs / b.maxs};
}
inline void operator/=(IntRect& a, const IntRect& b) {
    a.mins /= b.mins;
    a.maxs /= b.maxs;
}
inline IntRect make_union(const IntRect& a, const IntRect& b) {
    return {min(a.mins, b.mins), max(a.maxs, b.maxs)};
}
inline IntRect intersect(const IntRect& a, const IntRect& b) {
    return {max(a.mins, b.mins), min(a.maxs, b.maxs)};
}
inline bool IntRect::intersects(const IntRect& arg) const {
    return !intersect(*this, arg).is_empty();
}
inline IntRect inflate(const IntRect& a, const Int2& b) {
    return {a.mins - b, a.maxs + b};
}

//   ▄▄▄▄         ▄▄▄
//  ██  ▀▀  ▄▄▄▄   ██   ▄▄▄▄  ▄▄▄▄▄
//  ██     ██  ██  ██  ██  ██ ██  ▀▀
//  ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄ ▀█▄▄█▀ ██
//

struct StringView;

struct Color {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 0;

    Color() = default;
    Color(u8 r, u8 g, u8 b, u8 a) : r{r}, g{g}, b{b}, a{a} {
    }

    explicit operator Float4() const {
        return {r / 255.f, g / 255.f, b / 255.f, a / 255.f};
    }
    static Color from_hex(const char* hex, s32 num_bytes = -1);
    static Color from_hex(StringView hex);
};

inline Float4::operator Color() const {
    PLY_ASSERT(all(*this >= 0) && all(*this <= 1));
    return {u8(x * 255.99f), u8(y * 255.99f), u8(z * 255.99f), u8(w * 255.99f)};
}

#if defined(PLY_RUNTIME_INCLUDED)
inline Color Color::from_hex(StringView hex) {
    return from_hex(hex.bytes, hex.num_bytes);
}
#endif

float srgb_to_linear(float s);
float linear_to_srgb(float l);
Float3 srgb_to_linear(const Float3& vec);
Float4 srgb_to_linear(const Float4& vec);
Float3 linear_to_srgb(const Float3& vec);
Float4 linear_to_srgb(const Float4& vec);

//                   ▄▄    ▄▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██ ██ ██  ▄▄▄██  ██    ▄█▀▀
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ██▄▄▄▄
//

struct Mat2x2 {
    Float2 col[2];

    Mat2x2() = default;
    Mat2x2(const Float2& col0, const Float2& col1) : col{col0, col1} {
    }
    Float2& operator[](u32 i) {
        PLY_ASSERT(i < 2);
        return col[i];
    }
    const Float2& operator[](u32 i) const {
        PLY_ASSERT(i < 2);
        return col[i];
    }

    static Mat2x2 identity();
    static Mat2x2 scale(const Float2& scale);
    static Mat2x2 rotate(float radians);
    static Mat2x2 from_complex(const Float2& c);
    Mat2x2 transposed() const;
};

bool operator==(const Mat2x2& a, const Mat2x2& b);
inline bool operator!=(const Mat2x2& a, const Mat2x2& b) {
    return !(a == b);
}
Float2 operator*(const Mat2x2& m, const Float2& v);
Mat2x2 operator*(const Mat2x2& a, const Mat2x2& b);

//                   ▄▄    ▄▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██
//  ██ ██ ██  ▄▄▄██  ██     ▀▀█▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀
//

struct Mat3x4;
struct Mat4x4;

struct Mat3x3 {
    Float3 col[3];

    Mat3x3() = default;
    Mat3x3(const Float3& col0, const Float3& col1, const Float3& col2) : col{col0, col1, col2} {
    }
    explicit Mat3x3(const Mat3x4& m);
    explicit Mat3x3(const Mat4x4& m);
    static Mat3x3 identity();
    static Mat3x3 scale(const Float3& arg);
    static Mat3x3 rotate(const Float3& unit_axis, float radians);
    static Mat3x3 from_quaternion(const Quaternion& q);

    Float3& operator[](u32 i) {
        PLY_ASSERT(i < 3);
        return col[i];
    }
    const Float3& operator[](u32 i) const {
        PLY_ASSERT(i < 3);
        return col[i];
    }
    bool has_scale() const;
    Mat3x3 transposed() const;
};

bool operator==(const Mat3x3& a, const Mat3x3& b);
inline bool operator!=(const Mat3x3& a, const Mat3x3& b) {
    return !(a == b);
}
Float3 operator*(const Mat3x3& m, const Float3& v);
Mat3x3 operator*(const Mat3x3& a, const Mat3x3& b);
Mat3x3 make_basis(const Float3& dst_unit_fwd, const Float3& dst_up, const Float3& src_unit_fwd,
                  const Float3& src_unit_up);
inline Mat3x3 make_basis(const Float3& dst_unit_fwd, const Float3& src_fwd) {
    return make_basis(dst_unit_fwd, get_noncollinear(dst_unit_fwd), src_fwd, get_noncollinear(src_fwd));
}

//                   ▄▄    ▄▄▄▄            ▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄  ▄█▀██
//  ██ ██ ██  ▄▄▄██  ██     ▀▀█▄  ▀██▀  ██▄▄██▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ▄█▀▀█▄     ██
//

struct Mat4x4;
struct QuatPos;

struct Mat3x4 {
    Float3 col[4];

    Mat3x4() = default;
    Mat3x4(const Float3& col0, const Float3& col1, const Float3& col2, const Float3& col3)
        : col{col0, col1, col2, col3} {
    }
    explicit Mat3x4(const Mat3x3& m, const Float3& pos = 0);
    explicit Mat3x4(const Mat4x4& m);
    static Mat3x4 identity();
    static Mat3x4 scale(const Float3& arg);
    static Mat3x4 rotate(const Float3& unit_axis, float radians);
    static Mat3x4 translate(const Float3& pos);
    static Mat3x4 from_quaternion(const Quaternion& q, const Float3& pos = 0);
    static Mat3x4 from_quat_pos(const QuatPos& qp);

    Float3& operator[](u32 i) {
        PLY_ASSERT(i < 4);
        return col[i];
    }
    const Float3& operator[](u32 i) const {
        PLY_ASSERT(i < 4);
        return col[i];
    }
    const Mat3x3& as_mat3() const {
        PLY_PUN_GUARD;
        return (const Mat3x3&) *this;
    }
    bool has_scale() const {
        return ((Mat3x3*) this)->has_scale();
    }
    Mat3x4 inverted_ortho() const;
};

bool operator==(const Mat3x4& a, const Mat3x4& b);
inline bool operator!=(const Mat3x4& a, const Mat3x4& b) {
    return !(a == b);
}
Float3 operator*(const Mat3x4& m, const Float3& v);
Float4 operator*(const Mat3x4& m, const Float4& v);
Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b);

//                   ▄▄      ▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄  ▄█▀██
//  ██ ██ ██  ▄▄▄██  ██   ██▄▄██▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄     ██
//

enum ClipNearType { CLIP_NEAR_TO_0, CLIP_NEAR_TO_NEG_1 };

struct Mat4x4 {
    Float4 col[4];

    Mat4x4() = default;
    Mat4x4(float t) : col{t, t, t, t} {
    }
    Mat4x4(const Float4& col0, const Float4& col1, const Float4& col2, const Float4& col3)
        : col{col0, col1, col2, col3} {
    }
    explicit Mat4x4(const Mat3x3& m, const Float3& pos = 0);
    explicit Mat4x4(const Mat3x4& m);
    static Mat4x4 identity();
    static Mat4x4 scale(const Float3& arg);
    static Mat4x4 rotate(const Float3& unit_axis, float radians);
    static Mat4x4 translate(const Float3& pos);
    static Mat4x4 from_quaternion(const Quaternion& q, const Float3& pos = 0);
    static Mat4x4 from_quat_pos(const QuatPos& qp);
    static Mat4x4 perspective_projection(const Rect& frustum, float z_near, float z_far, ClipNearType clip_near);
    static Mat4x4 orthographic_projection(const Rect& rect, float z_near, float z_far, ClipNearType clip_near);

    Float4& operator[](u32 i) {
        PLY_ASSERT(i < 4);
        return col[i];
    }
    const Float4& operator[](u32 i) const {
        PLY_ASSERT(i < 4);
        return col[i];
    }
    Mat4x4 transposed() const;
    Mat4x4 inverted_ortho() const;
};

bool operator==(const Mat4x4& a, const Mat4x4& b);
inline bool operator!=(const Mat4x4& a, const Mat4x4& b) {
    return !(a == b);
}
Float4 operator*(const Mat4x4& m, const Float4& v);
Mat4x4 operator*(const Mat4x4& a, const Mat4x4& b);
Mat4x4 operator*(const Mat3x4& a, const Mat4x4& b);
Mat4x4 operator*(const Mat4x4& a, const Mat3x4& b);

//   ▄▄▄▄                         ▄▄▄
//  ██  ▀▀  ▄▄▄▄  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██   ▄▄▄▄  ▄▄  ▄▄
//  ██     ██  ██ ██ ██ ██ ██  ██  ██  ██▄▄██  ▀██▀
//  ▀█▄▄█▀ ▀█▄▄█▀ ██ ██ ██ ██▄▄█▀ ▄██▄ ▀█▄▄▄  ▄█▀▀█▄
//                         ██

struct Complex {
    static Float2 identity() {
        return Float2{1, 0};
    }
    static Float2 from_angle(float radians) {
        float c = cosf(radians);
        float s = sinf(radians);
        return Float2{c, s};
    }
    static float get_angle(const Float2& v) {
        return atan2f(v.y, v.x);
    }
    static Float2 mul(const Float2& a, const Float2& b) {
        return {a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x};
    }
};

//   ▄▄▄▄                 ▄▄                        ▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██   ██▄▄██ ██  ▀▀ ██  ██ ██ ██  ██ ██  ██
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██  ▀█▄▄ ▀█▄▄▄  ██     ██  ██ ██ ▀█▄▄█▀ ██  ██
//      ▀▀

struct Quaternion {
    float x;
    float y;
    float z;
    float w;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w) : x{x}, y{y}, z{z}, w{w} {
    }
    Quaternion(const Float3& v, float w) : x{v.x}, y{v.y}, z{v.z}, w{w} {
    }
    static Quaternion identity() {
        return {0, 0, 0, 1};
    }
    static Quaternion from_axis_angle(const Float3& unit_axis, float radians);
    static Quaternion from_unit_vectors(const Float3& start, const Float3& end);
    static Quaternion from_ortho(const Mat3x3& m);
    static Quaternion from_ortho(const Mat4x4& m);

    explicit operator Float3() const {
        return {x, y, z};
    }
    explicit operator Float4() const {
        return {x, y, z, w};
    }
    PLY_NO_DISCARD Quaternion inverted() const {
        // Small rotations have large w component, so prefer to keep the same sign of w.
        // Better for interpolation.
        return {-x, -y, -z, w};
    }
    Quaternion normalized() const {
        Float4 value = ((Float4*) this)->normalized();
        PLY_PUN_GUARD;
        return (const Quaternion&) value;
    }
    bool is_unit_length() const {
        PLY_PUN_GUARD;
        return abs(((Float4*) this)->length_squared() - 1.f) < 0.001f;
    }
    Quaternion negated_if_closer_to(const Quaternion& other) const;
};

inline Float4::operator Quaternion() const {
    return {x, y, z, w};
}
inline Quaternion operator-(const Quaternion& q) {
    return {-q.x, -q.y, -q.z, -q.w};
}
Float3 operator*(const Quaternion& q, const Float3& v);
Quaternion operator*(const Quaternion& a, const Quaternion& b);
Quaternion mix(const Quaternion& a, const Quaternion& b, float f);

//   ▄▄▄▄                 ▄▄         ▄▄▄▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄       ██  ██  ▄▄▄▄   ▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██         ██▀▀▀  ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██  ▀█▄▄ ▄▄▄▄▄ ██     ▀█▄▄█▀  ▄▄▄█▀
//      ▀▀

struct QuatPos {
    Quaternion quat;
    Float3 pos;

    QuatPos() = default;
    QuatPos(const Quaternion& quat, const Float3& pos) : quat(quat), pos(pos) {
    }
    static QuatPos identity();
    static QuatPos translate(const Float3& pos);
    static QuatPos rotate(const Float3& unit_axis, float radians);
    static QuatPos from_ortho(const Mat3x4& m);
    static QuatPos from_ortho(const Mat4x4& m);

    QuatPos inverted() const;
};

inline Float3 operator*(const QuatPos& qp, const Float3& v) {
    return (qp.quat * v) + qp.pos;
}
inline QuatPos operator*(const QuatPos& a, const QuatPos& b) {
    return {a.quat * b.quat, (a.quat * b.pos) + a.pos};
}
inline QuatPos operator*(const QuatPos& a, const Quaternion& b) {
    return {a.quat * b, a.pos};
}
inline QuatPos operator*(const Quaternion& a, const QuatPos& b) {
    return {a * b.quat, a * b.pos};
}
inline Mat3x4 Mat3x4::from_quat_pos(const QuatPos& qp) {
    return from_quaternion(qp.quat, qp.pos);
}
inline Mat4x4 Mat4x4::from_quat_pos(const QuatPos& qp) {
    return from_quaternion(qp.quat, qp.pos);
}

// Cubic Bezier curves

template <typename T>
T sample_cubic_bezier(const T& p0, const T& p1, const T& p2, const T& p3, float t) {
    float omt = 1.f - t;
    return p0 * (omt * omt * omt) + p1 * (3 * omt * omt * t) + p2 * (3 * omt * t * t) + p3 * (t * t * t);
}
template <typename T>
T sample_cubic_bezier_derivative(const T& p0, const T& p1, const T& p2, const T& p3, float t) {
    T q0 = p1 - p0;
    T q1 = p2 - p1;
    T q2 = p3 - p2;
    T r0 = mix(q0, q1, t);
    T r1 = mix(q1, q2, t);
    T p = mix(r0, r1, t);
    return p;
}
inline float ease_in_and_out(float t) {
    return (3.f - 2.f * t) * t * t;
}

} // namespace ply
