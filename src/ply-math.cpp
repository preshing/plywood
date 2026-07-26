/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-math.h"

namespace ply {

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
    return *this / sqrtf(L2);
}

Rect rectFromFov(float fovY, float aspect) {
    float halfTanY = tanf(fovY / 2);
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
    return *this / sqrtf(L2);
}

Float3 pow(const Float3& a, const Float3& b) {
    return {powf(a.x, b.x), powf(a.y, b.y), powf(a.z, b.z)};
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
    return *this / sqrtf(L2);
}

Float4 pow(const Float4& a, const Float4& b) {
    return {powf(a.x, b.x), powf(a.y, b.y), powf(a.z, b.z), powf(a.w, b.w)};
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
        return powf(((s + 0.055f) / 1.055f), 2.4f);
}

float linearToSrgb(float l) {
    if (l < 0.00313066844250063f)
        return l * 12.92f;
    else
        return 1.055f * powf(l, 1 / 2.4f) - 0.055f;
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
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float(*)[2]>(this);
    return {
        {m[0][0], m[1][0]},
        {m[0][1], m[1][1]},
    };
}

bool operator==(const Mat2x2& a, const Mat2x2& b) {
    return (a.col[0] == b.col[0]) && (a.col[1] == b.col[1]);
}

Float2 operator*(const Mat2x2& m_, const Float2& v_) {
    Float2 result;
    {
        PLY_PUN_GUARD;
        auto* res = reinterpret_cast<float*>(&result);
        auto* m = reinterpret_cast<const float(*)[2]>(&m_);
        auto* v = reinterpret_cast<const float*>(&v_);
        for (u32 r = 0; r < 2; r++) {
            res[r] = m[0][r] * v[0] + m[1][r] * v[1];
        }
    }
    return result;
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
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float(*)[3]>(this);
    return {
        {m[0][0], m[1][0], m[2][0]},
        {m[0][1], m[1][1], m[2][1]},
        {m[0][2], m[1][2], m[2][2]},
    };
}

bool operator==(const Mat3x3& a_, const Mat3x3& b_) {
    PLY_PUN_GUARD;
    auto* a = reinterpret_cast<const float*>(&a_);
    auto* b = reinterpret_cast<const float*>(&b_);
    for (u32 r = 0; r < 9; r++) {
        if (a[r] != b[r])
            return false;
    }
    return true;
}

Float3 operator*(const Mat3x3& m_, const Float3& v_) {
    Float3 result;
    {
        PLY_PUN_GUARD;
        auto* res = reinterpret_cast<float*>(&result);
        auto* m = reinterpret_cast<const float(*)[3]>(&m_);
        auto* v = reinterpret_cast<const float*>(&v_);
        for (u32 r = 0; r < 3; r++) {
            res[r] = m[0][r] * v[0] + m[1][r] * v[1] + m[2][r] * v[2];
        }
    }
    return result;
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
    dstRight /= sqrtf(L2);
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
    Mat3x4 result;
    reinterpret_cast<Mat3x3&>(result) = reinterpret_cast<const Mat3x3&>(*this).transposed();
    result.col[3] = reinterpret_cast<Mat3x3&>(result) * -col[3];
    return result;
}

bool operator==(const Mat3x4& a_, const Mat3x4& b_) {
    PLY_PUN_GUARD;
    auto* a = reinterpret_cast<const float*>(&a_);
    auto* b = reinterpret_cast<const float*>(&b_);
    for (u32 r = 0; r < 12; r++) {
        if (a[r] != b[r])
            return false;
    }
    return true;
}

Float3 operator*(const Mat3x4& m_, const Float3& v_) {
    Float3 result;
    {
        PLY_PUN_GUARD;
        auto* res = reinterpret_cast<float*>(&result);
        auto* m = reinterpret_cast<const float(*)[3]>(&m_);
        auto* v = reinterpret_cast<const float*>(&v_);
        for (u32 r = 0; r < 3; r++) {
            res[r] = m[0][r] * v[0] + m[1][r] * v[1] + m[2][r] * v[2] + m[3][r];
        }
    }
    return result;
}

Float4 operator*(const Mat3x4& m_, const Float4& v_) {
    Float4 result;
    {
        PLY_PUN_GUARD;
        auto* res = reinterpret_cast<float*>(&result);
        auto* m = reinterpret_cast<const float(*)[3]>(&m_);
        auto* v = reinterpret_cast<const float*>(&v_);
        for (u32 r = 0; r < 3; r++) {
            res[r] = m[0][r] * v[0] + m[1][r] * v[1] + m[2][r] * v[2] + m[3][r] * v[3];
        }
        res[3] = v[3];
    }
    return result;
}

Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b) {
    Mat3x4 result;
    for (u32 c = 0; c < 3; c++) {
        result.col[c] = a.as_mat3() * b.col[c];
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

Mat4x4 Mat4x4::orthographicProjection(const Rect& rect, YCoordType yCoordType, float zNear, float zFar, DeviceCoordType devCoordType) {
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
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float(*)[4]>(this);
    return {
        {m[0][0], m[1][0], m[2][0], m[3][0]},
        {m[0][1], m[1][1], m[2][1], m[3][1]},
        {m[0][2], m[1][2], m[2][2], m[3][2]},
        {m[0][3], m[1][3], m[2][3], m[3][3]},
    };
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
    PLY_PUN_GUARD;
    auto* a = reinterpret_cast<const float*>(&a_);
    auto* b = reinterpret_cast<const float*>(&b_);
    for (u32 r = 0; r < 16; r++) {
        if (a[r] != b[r])
            return false;
    }
    return true;
}

Float4 operator*(const Mat4x4& m_, const Float4& v_) {
    Float4 result;
    {
        PLY_PUN_GUARD;
        auto* res = reinterpret_cast<float*>(&result);
        auto* m = reinterpret_cast<const float(*)[4]>(&m_);
        auto* v = reinterpret_cast<const float*>(&v_);
        for (u32 r = 0; r < 4; r++) {
            res[r] = m[0][r] * v[0] + m[1][r] * v[1] + m[2][r] * v[2] + m[3][r] * v[3];
        }
    }
    return result;
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
    float c = cosf(radians / 2);
    float s = sinf(radians / 2);
    return {s * unitAxis.x, s * unitAxis.y, s * unitAxis.z, c};
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
Quaternion quaternionFromOrtho(M m) {
    float t; // This will be set to 4*c*c for some quaternion component c.
    // At least one component's square must be >= 1/4. (Otherwise, it isn't a unit
    // quaternion.) Let's require t >= 1/2. This will accept any component whose square
    // is >= 1/8.
    if ((t = 1.f + m[0][0] + m[1][1] + m[2][2]) >= 0.5f) { // 4*w*w
        float w = sqrtf(t) * 0.5f;
        float f = 0.25f / w;
        return {(m[1][2] - m[2][1]) * f, (m[2][0] - m[0][2]) * f, (m[0][1] - m[1][0]) * f, w};
    } else if ((t = 1.f + m[0][0] - m[1][1] - m[2][2]) >= 0.5f) { // 4*x*x
        // Prefer positive w component in result
        float wco = m[1][2] - m[2][1];
        float x = sqrtf(t) * ((wco >= 0) - 0.5f); // equivalent to sqrtf(t) * 0.5f * sgn(wco)
        float f = 0.25f / x;
        return {x, (m[0][1] + m[1][0]) * f, (m[2][0] + m[0][2]) * f, wco * f};
    } else if ((t = 1.f - m[0][0] + m[1][1] - m[2][2]) >= 0.5f) { // 4*y*y
        float wco = m[2][0] - m[0][2];
        float y = sqrtf(t) * ((wco >= 0) - 0.5f); // equivalent to sqrtf(t) * 0.5f * sgn(wco)
        float f = 0.25f / y;
        return {(m[0][1] + m[1][0]) * f, y, (m[1][2] + m[2][1]) * f, wco * f};
    } else if ((t = 1.f - m[0][0] - m[1][1] + m[2][2]) >= 0.5f) { // 4*z*z
        float wco = m[0][1] - m[1][0];
        float z = sqrtf(t) * ((wco >= 0) - 0.5f); // equivalent to sqrtf(t) * 0.5f * sgn(wco)
        float f = 0.25f / z;
        return {(m[2][0] + m[0][2]) * f, (m[1][2] + m[2][1]) * f, z, wco * f};
    }
    PLY_ASSERT(0); // The matrix is not even close to being orthonormal
    return {0, 0, 0, 1};
}

Quaternion Quaternion::fromOrtho(const Mat3x3& m) {
    PLY_PUN_GUARD;
    return quaternionFromOrtho(reinterpret_cast<const float(*)[3]>(&m));
}

Quaternion Quaternion::fromOrtho(const Mat4x4& m) {
    PLY_PUN_GUARD;
    return quaternionFromOrtho(reinterpret_cast<const float(*)[4]>(&m));
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
    return {Quaternion::fromOrtho(m.as_mat3()), m[3]};
}

QuatPos QuatPos::fromOrtho(const Mat4x4& m) {
    return {Quaternion::fromOrtho(m), Float3{m[3]}};
}

} // namespace ply
