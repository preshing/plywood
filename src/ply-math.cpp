/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "ply-math.h"

namespace ply {

//                        ▄▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄ ▀▀  ██
//  ▀█▄ ▄█▀ ██▄▄██ ██     ▄█▀▀
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄ ██▄▄▄▄
//

Float2 Float2::normalized() const {
    return *this / length();
}

Float2 Float2::safe_normalized(const Float2& fallback, float epsilon) const {
    float L2 = this->length_squared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrtf(L2);
}

Rect rect_from_fov(float fov_y, float aspect) {
    float half_tan_y = tanf(fov_y / 2);
    return inflate(Rect{{0, 0}}, {half_tan_y * aspect, half_tan_y});
}

Float2 round_up(const Float2& value) {
    return {round_up(value.x), round_up(value.y)};
}

Float2 round_down(const Float2& value) {
    return {round_down(value.x), round_down(value.y)};
}

Float2 round_nearest(const Float2& value) {
    return {round_nearest(value.x), round_nearest(value.y)};
}

Float2 step_towards(const Float2& start, const Float2& target, float amount) {
    Float2 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//                        ▄▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄ ▀▀  ██
//  ▀█▄ ▄█▀ ██▄▄██ ██      ▀▀█▄
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄ ▀█▄▄█▀
//

Float3 Float3::normalized() const {
    return *this / length();
}

Float3 Float3::safe_normalized(const Float3& fallback, float epsilon) const {
    float L2 = this->length_squared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrtf(L2);
}

Float3 pow(const Float3& a, const Float3& b) {
    return {powf(a.x, b.x), powf(a.y, b.y), powf(a.z, b.z)};
}

Float3 round_up(const Float3& value) {
    return {round_up(value.x), round_up(value.y), round_up(value.z)};
}

Float3 round_down(const Float3& value) {
    return {round_down(value.x), round_down(value.y), round_down(value.z)};
}

Float3 round_nearest(const Float3& value) {
    return {round_nearest(value.x), round_nearest(value.y), round_nearest(value.z)};
}

Float3 step_towards(const Float3& start, const Float3& target, float amount) {
    Float3 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//                          ▄▄▄
//  ▄▄   ▄▄  ▄▄▄▄   ▄▄▄▄  ▄█▀██
//  ▀█▄ ▄█▀ ██▄▄██ ██    ██▄▄██▄
//    ▀█▀   ▀█▄▄▄  ▀█▄▄▄     ██
//

Float4 Float4::normalized() const {
    return *this / length();
}

Float4 Float4::safe_normalized(const Float4& fallback, float epsilon) const {
    float L2 = this->length_squared();
    if (L2 < epsilon * epsilon)
        return fallback;
    return *this / sqrtf(L2);
}

Float4 pow(const Float4& a, const Float4& b) {
    return {powf(a.x, b.x), powf(a.y, b.y), powf(a.z, b.z), powf(a.w, b.w)};
}

Float4 round_up(const Float4& vec) {
    return {round_up(vec.x), round_up(vec.y), round_up(vec.z), round_up(vec.w)};
}

Float4 round_down(const Float4& vec) {
    return {round_down(vec.x), round_down(vec.y), round_down(vec.z), round_down(vec.w)};
}

Float4 round_nearest(const Float4& vec) {
    return {round_nearest(vec.x), round_nearest(vec.y), round_nearest(vec.z), round_nearest(vec.w)};
}

Float4 step_towards(const Float4& start, const Float4& target, float amount) {
    Float4 delta = target - start;
    float length = delta.length();
    return (length < amount) ? target : start + delta * (amount / length);
}

//   ▄▄▄▄         ▄▄▄
//  ██  ▀▀  ▄▄▄▄   ██   ▄▄▄▄  ▄▄▄▄▄
//  ██     ██  ██  ██  ██  ██ ██  ▀▀
//  ▀█▄▄█▀ ▀█▄▄█▀ ▄██▄ ▀█▄▄█▀ ██
//

void convert_from_hex(float* values, size_t num_values, const char* hex) {
    for (size_t i = 0; i < num_values; i++) {
        int c = 0;
        for (int j = 0; j < 2; j++) {
            c <<= 4;
            if (*hex >= '0' && *hex <= '9') {
                c += *hex - '0';
            } else if (*hex >= 'a' && *hex <= 'f') {
                c += *hex - 'a' + 10;
            } else if (*hex >= 'A' && *hex <= 'F') {
                c += *hex - 'A' + 10;
            } else {
                PLY_ASSERT(0);
            }
            hex++;
        }
        values[i] = c * (1 / 255.f);
    }
    PLY_ASSERT(*hex == 0);
}

float srgb_to_linear(float s) {
    if (s < 0.0404482362771082f)
        return s / 12.92f;
    else
        return powf(((s + 0.055f) / 1.055f), 2.4f);
}

float linear_to_srgb(float l) {
    if (l < 0.00313066844250063f)
        return l * 12.92f;
    else
        return 1.055f * powf(l, 1 / 2.4f) - 0.055f;
}

Float3 srgb_to_linear(const Float3& vec) {
    return {srgb_to_linear(vec.x), srgb_to_linear(vec.y), srgb_to_linear(vec.z)};
}

Float4 srgb_to_linear(const Float4& vec) {
    return {srgb_to_linear(vec.x), srgb_to_linear(vec.y), srgb_to_linear(vec.z), vec.w};
}

Float3 linear_to_srgb(const Float3& vec) {
    return {linear_to_srgb(vec.x), linear_to_srgb(vec.y), linear_to_srgb(vec.z)};
}

Float4 linear_to_srgb(const Float4& vec) {
    return {linear_to_srgb(vec.x), linear_to_srgb(vec.y), linear_to_srgb(vec.z), vec.w};
}

//                   ▄▄    ▄▄▄▄          ▄▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄ ▀▀  ██
//  ██ ██ ██  ▄▄▄██  ██    ▄█▀▀   ▀██▀   ▄█▀▀
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ██▄▄▄▄ ▄█▀▀█▄ ██▄▄▄▄
//

Mat2x2 Mat2x2::identity() {
    return {{1, 0}, {0, 1}};
}

Mat2x2 Mat2x2::scale(const Float2& scale) {
    return {{scale.x, 0}, {0, scale.y}};
}

Mat2x2 Mat2x2::rotate(float radians) {
    return from_complex(Complex::from_angle(radians));
}

Mat2x2 Mat2x2::from_complex(const Float2& c) {
    return {{c.x, c.y}, {-c.y, c.x}};
}

Mat2x2 Mat2x2::transposed() const {
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float (*)[2]>(this);
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
        auto* m = reinterpret_cast<const float (*)[2]>(&m_);
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

//                   ▄▄    ▄▄▄▄          ▄▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄ ▀▀  ██
//  ██ ██ ██  ▄▄▄██  ██     ▀▀█▄  ▀██▀    ▀▀█▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ▄█▀▀█▄ ▀█▄▄█▀
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

Mat3x3 Mat3x3::rotate(const Float3& unit_axis, float radians) {
    return Mat3x3::from_quaternion(Quaternion::from_axis_angle(unit_axis, radians));
}

Mat3x3 Mat3x3::from_quaternion(const Quaternion& q) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y}};
}

bool Mat3x3::has_scale() const {
    return !col[0].is_unit_length() || !col[1].is_unit_length() || !col[2].is_unit_length();
}

Mat3x3 Mat3x3::transposed() const {
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float (*)[3]>(this);
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
        auto* m = reinterpret_cast<const float (*)[3]>(&m_);
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

Mat3x3 make_basis(const Float3& dst_unit_fwd, const Float3& dst_up, const Float3& src_unit_fwd,
                  const Float3& src_unit_up) {
    PLY_ASSERT(dst_unit_fwd.is_unit_length());
    PLY_ASSERT(src_unit_fwd.is_unit_length());
    PLY_ASSERT(src_unit_up.is_unit_length());

    Float3 dst_right = cross(dst_unit_fwd, dst_up);
    float L2 = dst_right.length_squared();
    if (L2 < 1e-6f) {
        dst_right = cross(dst_unit_fwd, get_noncollinear(dst_unit_fwd));
        L2 = dst_right.length_squared();
    }
    dst_right /= sqrtf(L2);
    return Mat3x3{dst_right, dst_unit_fwd, cross(dst_right, dst_unit_fwd)} *
           Mat3x3{cross(src_unit_fwd, src_unit_up), src_unit_fwd, src_unit_up}.transposed();
}

//                   ▄▄    ▄▄▄▄            ▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▀▀  ██ ▄▄  ▄▄  ▄█▀██
//  ██ ██ ██  ▄▄▄██  ██     ▀▀█▄  ▀██▀  ██▄▄██▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ▄█▀▀█▄     ██
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

Mat3x4 Mat3x4::rotate(const Float3& unit_axis, float radians) {
    return Mat3x4::from_quaternion(Quaternion::from_axis_angle(unit_axis, radians));
}

Mat3x4 Mat3x4::translate(const Float3& pos) {
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, pos};
}

Mat3x4 Mat3x4::from_quaternion(const Quaternion& q, const Float3& pos) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y},
            pos};
}

Mat3x4 Mat3x4::inverted_ortho() const {
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
        auto* m = reinterpret_cast<const float (*)[3]>(&m_);
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
        auto* m = reinterpret_cast<const float (*)[3]>(&m_);
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

//                   ▄▄      ▄▄▄            ▄▄▄
//  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄  ▄█▀██  ▄▄  ▄▄  ▄█▀██
//  ██ ██ ██  ▄▄▄██  ██   ██▄▄██▄  ▀██▀  ██▄▄██▄
//  ██ ██ ██ ▀█▄▄██  ▀█▄▄     ██  ▄█▀▀█▄     ██
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

Mat4x4 Mat4x4::rotate(const Float3& unit_axis, float radians) {
    return Mat4x4::from_quaternion(Quaternion::from_axis_angle(unit_axis, radians));
}

Mat4x4 Mat4x4::translate(const Float3& pos) {
    return {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {pos, 1}};
}

Mat4x4 Mat4x4::from_quaternion(const Quaternion& q, const Float3& pos) {
    return {{1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y + 2 * q.z * q.w, 2 * q.x * q.z - 2 * q.y * q.w, 0},
            {2 * q.x * q.y - 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z + 2 * q.x * q.w, 0},
            {2 * q.x * q.z + 2 * q.y * q.w, 2 * q.y * q.z - 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y, 0},
            {pos, 1}};
}

Mat4x4 Mat4x4::perspective_projection(const Rect& frustum, float z_near, float z_far, ClipNearType clip_near) {
    PLY_ASSERT(z_near > 0 && z_far > 0);
    Mat4x4 result{0, 0, 0, 0};
    float oo_xdenom = 1.f / frustum.width();
    float oo_ydenom = 1.f / frustum.height();
    float oo_zdenom = 1.f / (z_near - z_far);
    result.col[0].x = 2.f * oo_xdenom;
    result.col[2].x = (frustum.mins.x + frustum.maxs.x) * oo_xdenom;
    result.col[1].y = 2.f * oo_ydenom;
    result.col[2].y = (frustum.mins.y + frustum.maxs.y) * oo_xdenom;
    result.col[2].z = (z_near + z_far) * oo_zdenom;
    result.col[2].w = -1.f;
    result.col[3].z = (2 * z_near * z_far) * oo_zdenom;
    if (clip_near == CLIP_NEAR_TO_0) {
        result.col[2].z = 0.5f * result.col[2].z - 0.5f;
        result.col[3].z *= 0.5f;
    }
    return result;
}

Mat4x4 Mat4x4::orthographic_projection(const Rect& rect, float z_near, float z_far, ClipNearType clip_near) {
    Mat4x4 result{0, 0, 0, 0};
    float tow = 2 / rect.width();
    float toh = 2 / rect.height();
    float oo_zrange = 1 / (z_near - z_far);
    result.col[0].x = tow;
    result.col[3].x = -rect.mid().x * tow;
    result.col[1].y = toh;
    result.col[3].y = -rect.mid().y * toh;
    result.col[2].z = 2 * oo_zrange;
    result.col[3].z = (z_near + z_far) * oo_zrange;
    result.col[3].w = 1.f;
    if (clip_near == CLIP_NEAR_TO_0) {
        result.col[2].z *= 0.5f;
        result.col[3].z = 0.5f * result.col[3].z + 0.5f;
    }
    return result;
}

Mat4x4 Mat4x4::transposed() const {
    PLY_PUN_GUARD;
    auto* m = reinterpret_cast<const float (*)[4]>(this);
    return {
        {m[0][0], m[1][0], m[2][0], m[3][0]},
        {m[0][1], m[1][1], m[2][1], m[3][1]},
        {m[0][2], m[1][2], m[2][2], m[3][2]},
        {m[0][3], m[1][3], m[2][3], m[3][3]},
    };
}

Mat4x4 Mat4x4::inverted_ortho() const {
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
        auto* m = reinterpret_cast<const float (*)[4]>(&m_);
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

Quaternion Quaternion::from_axis_angle(const Float3& unit_axis, float radians) {
    PLY_ASSERT(unit_axis.is_unit_length());
    float c = cosf(radians / 2);
    float s = sinf(radians / 2);
    return {s * unit_axis.x, s * unit_axis.y, s * unit_axis.z, c};
}

Quaternion Quaternion::from_unit_vectors(const Float3& start, const Float3& end) {
    // Float4{cross(start, end), dot(start, end)} gives you double the desired rotation.
    // To get the desired rotation, "average" (really just sum) that with Float4{0, 0,
    // 0, 1}, then normalize.
    float w = dot(start, end) + 1;
    if (w < 1e-6f) {
        // Exceptional case: Vectors point in opposite directions.
        // Choose a perpendicular axis and make a 180 degree rotation.
        Float3 get_noncollinear = (abs(start.x) < 0.9f) ? Float3{1, 0, 0} : Float3{0, 1, 0};
        Float3 axis = cross(start, get_noncollinear);
        return (Quaternion) Float4{axis, 0}.normalized();
    }
    Float3 v = cross(start, end);
    return (Quaternion) Float4{v, w}.normalized();
}

template <typename M>
Quaternion quaternion_from_ortho(M m) {
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

Quaternion Quaternion::from_ortho(const Mat3x3& m) {
    PLY_PUN_GUARD;
    return quaternion_from_ortho(reinterpret_cast<const float (*)[3]>(&m));
}

Quaternion Quaternion::from_ortho(const Mat4x4& m) {
    PLY_PUN_GUARD;
    return quaternion_from_ortho(reinterpret_cast<const float (*)[4]>(&m));
}

Quaternion Quaternion::negated_if_closer_to(const Quaternion& other) const {
    Float4 v0{*this};
    Float4 v1{other};
    return Quaternion{(v0 - v1).length_squared() < (-v0 - v1).length_squared() ? v0 : -v0};
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
    Float4 linear_mix = mix((Float4) a.negated_if_closer_to(b), (Float4) b, t);
    return (Quaternion) linear_mix.normalized();
}

//   ▄▄▄▄                 ▄▄         ▄▄▄▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄  ▄██▄▄       ██  ██  ▄▄▄▄   ▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██         ██▀▀▀  ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██  ▀█▄▄ ▄▄▄▄▄ ██     ▀█▄▄█▀  ▄▄▄█▀
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

QuatPos QuatPos::rotate(const Float3& unit_axis, float radians) {
    return {Quaternion::from_axis_angle(unit_axis, radians), {0, 0, 0}};
}

QuatPos QuatPos::from_ortho(const Mat3x4& m) {
    return {Quaternion::from_ortho(m.as_mat3()), m[3]};
}

QuatPos QuatPos::from_ortho(const Mat4x4& m) {
    return {Quaternion::from_ortho(m), Float3{m[3]}};
}

} // namespace ply
