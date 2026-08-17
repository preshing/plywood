`ply-math.h`: Math
==================

Scalar and vector math suitable for games and UI layouts.

Basic Functions
---------------

`float roundNearest(float value)`
`double roundNearest(double value)`
> Rounds `value` to the nearest integer, with halfway cases rounded away from zero.

`float roundUp(float value)`
`double roundUp(double value)`
> Rounds `value` toward positive infinity.

`float roundDown(float value)`
`double roundDown(double value)`
> Rounds `value` toward negative infinity.

`float mix(float a, float b, float t)`
`double mix(double a, double b, double t)`
> Performs linear interpolation between `a` and `b`.

`float unmix(float a, float b, float mixed)`
`double unmix(double a, double b, double mixed)`
> Returns the interpolation parameter that would produce `mixed` between `a` and `b`.

`float stepTowards(float start, float target, float amount)`
`double stepTowards(double start, double target, double amount)`
> Moves `start` toward `target` by at most `amount` without overshooting.

`float wrap(float value, float range)`
`double wrap(double value, double range)`
> Wraps `value` into the half-open interval `[0, range)`. `range` must be positive.

`float square(float v)`
`double square(double v)`
> Returns `v * v`.

Transcendental Functions
------------------------

`float sqrt(float value)`
`double sqrt(double value)`
> Returns the square root of `value`.

`float fastSqrt(float value)`
> Fast approximate square root.

`float fastInvSqrt(float value)`
> Fast approximate reciprocal square root.

`float log(float value)`
`double log(double value)`
> Returns the natural logarithm of `value`.

`float exp(float value)`
`double exp(double value)`
> Returns Euler's number raised to `value`.

`float pow(float base, float exponent)`
`double pow(double base, double exponent)`
> Returns `base` raised to `exponent`.

`float sin(float rad)`
`double sin(double rad)`
> Returns the sine of `rad`.

`float cos(float rad)`
`double cos(double rad)`
> Returns the cosine of `rad`.

`float tan(float rad)`
`double tan(double rad)`
> Returns the tangent of `rad`.

`Float2 cosAndSin(float rad)`
> Returns the cosine and sine together as `{cos(rad), sin(rad)}`. Faster than calling `sin` and `cos` separately.

`float fastSin(float rad)`
> Fast sine approximation.

`float fastCos(float rad)`
> Fast cosine approximation.

`float arcsin(float value)`
`double arcsin(double value)`
> Returns the arcsine of `value` in radians.

`float arccos(float value)`
`double arccos(double value)`
> Returns the arccosine of `value` in radians.

`float arctan(float value)`
`double arctan(double value)`
> Returns the arctangent of `value` in radians.

`float arctan(const Float2& pos)`
> Returns the angle of `pos` in radians.

Boolean Vectors
---------------

### `Bool2`

`Bool2` stores the result of a per-component 2D comparison.

{context class=Bool2}

`Bool2(bool x, bool y)`
> Constructs a two-component boolean vector.

### `Bool3`

`Bool3` stores the result of a per-component 3D comparison.

{context class=Bool3}

`Bool3(bool x, bool y, bool z)`
> Constructs a three-component boolean vector.

### `Bool4`

`Bool4` stores the result of a per-component 4D comparison.

{context class=Bool4}

`Bool4(bool x, bool y, bool z, bool w)`
> Constructs a four-component boolean vector.

### Namespace-Level Functions

`bool any(const Bool2& v)`
`bool any(const Bool3& v)`
`bool any(const Bool4& v)`
> Returns `true` if at least one component is `true`.

`bool all(const Bool2& v)`
`bool all(const Bool3& v)`
`bool all(const Bool4& v)`
> Returns `true` only if every component is `true`.

Floating-Point Vectors
----------------------

### `Float2`

{context class=Float2}

`Float2(float x, float y)`
> Constructs a vector from explicit `x` and `y` components.

`float& operator[](u32 index)`
`float operator[](u32 index) const`
> Returns the component at the specified index with runtime bounds checking.

`Float2 normalized() const`
> Returns a unit-length copy.

`float fastLength() const`
> Returns an approximate vector length using `fastSqrt`.

`Float2 fastNormalized() const`
> Returns an approximately unit-length copy using `fastInvSqrt`.

`Float2 safeNormalized(const Float2& fallback = {1, 0}, float epsilon = 1e-6f) const`
> Returns `fallback` when the vector is too close to zero length.

`Float2 swizzle(u32 i0, u32 i1) const`
`Float3 swizzle(u32 i0, u32 i1, u32 i2) const`
`Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const`
> Reorders components by index and returns a new vector of the requested size.

### `Float3`

{context class=Float3}

`Float3(float x, float y, float z)`
`Float3(const Float2& v, float z)`
> Constructs a 3D vector either from three components or by extending a `Float2`.

`float& operator[](u32 index)`
`float operator[](u32 index) const`
> Returns the component at the specified index with runtime bounds checking.

`Float3 normalized() const`
> Returns a unit-length copy.

`float fastLength() const`
> Returns an approximate vector length using `fastSqrt`.

`Float3 fastNormalized() const`
> Returns an approximately unit-length copy using `fastInvSqrt`.

`Float3 safeNormalized(const Float3& fallback = {1, 0, 0}, float epsilon = 1e-9f) const`
> Returns `fallback` if the vector is too close to zero length.

### `Float4`

{context class=Float4}

`Float4(float x, float y, float z, float w)`
> Constructs a four-component vector directly.

`Float4(const Color& color)`
> Constructs a vector from normalized RGBA values derived from `Color`.

`explicit operator Quaternion() const`
> Converts `Float4` to a quaternion with the same four components.

`explicit operator Color() const`
> Converts to a packed color. This asserts that all components lie in `[0, 1]`.

`Float4 normalized() const`
> Returns a unit-length copy.

`float fastLength() const`
> Returns an approximate vector length using `fastSqrt`.

`Float4 fastNormalized() const`
> Returns an approximately unit-length copy using `fastInvSqrt`.

`Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const`
> Reorders components by index and returns a new four-component vector.

### Namespace-Level Functions

`float dot(const Float2& a, const Float2& b)`
`float dot(const Float3& a, const Float3& b)`
`float dot(const Float4& a, const Float4& b)`
> Returns the dot product of two vectors.

`float cross(const Float2& a, const Float2& b)`
`Float3 cross(const Float3& a, const Float3& b)`
> Returns the cross product of two vectors.

`Float2 clamp(const Float2& v, const Float2& mins, const Float2& maxs)`
`Float3 clamp(const Float3& v, const Float3& mins, const Float3& maxs)`
`Float4 clamp(const Float4& v, const Float4& mins, const Float4& maxs)`
> Performs per-component clamping.

`Float2 min(const Float2& a, const Float2& b)`
`Float3 min(const Float3& a, const Float3& b)`
`Float4 min(const Float4& a, const Float4& b)`
> Returns the per-component minimum.

`Float2 max(const Float2& a, const Float2& b)`
`Float3 max(const Float3& a, const Float3& b)`
`Float4 max(const Float4& a, const Float4& b)`
> Returns the per-component maximum.

`Float2 abs(const Float2& a)`
`Float3 abs(const Float3& a)`
`Float4 abs(const Float4& a)`
> Returns the per-component absolute value.

`Float2 pow(const Float2& a, const Float2& b)`
`Float3 pow(const Float3& a, const Float3& b)`
`Float4 pow(const Float4& a, const Float4& b)`
> Raises each component of `a` to the corresponding power in `b`.

`Float2 roundNearest(const Float2& value)`
`Float3 roundNearest(const Float3& value)`
`Float4 roundNearest(const Float4& value)`
> Rounds each component individually.

`Float2 roundUp(const Float2& value)`
`Float3 roundUp(const Float3& value)`
`Float4 roundUp(const Float4& value)`
> Rounds each component upward individually.

`Float2 roundDown(const Float2& value)`
`Float3 roundDown(const Float3& value)`
`Float4 roundDown(const Float4& value)`
> Rounds each component upward individually.

`Float2 mix(const Float2& a, const Float2& b, const Float2& t)`
`Float3 mix(const Float3& a, const Float3& b, const Float3& t)`
`Float4 mix(const Float4& a, const Float4& b, const Float4& t)`
> Performs per-component interpolation.

`Float2 unmix(const Float2& a, const Float2& b, const Float2& mixed)`
`Float3 unmix(const Float3& a, const Float3& b, const Float3& mixed)`
`Float4 unmix(const Float4& a, const Float4& b, const Float4& mixed)`
> Returns the per-component interpolation parameters that produce `mixed`.

`Float2 stepTowards(const Float2& start, const Float2& target, float amount)`
`Float3 stepTowards(const Float3& start, const Float3& target, float amount)`
`Float4 stepTowards(const Float4& start, const Float4& target, float amount)`
> Moves each vector toward the target by at most `amount` without overshooting.

`Float3 getNoncollinear(const Float3& unitVec)`
> Returns a convenient axis that is not too closely aligned with `unitVec`.

Integer Vectors
---------------

### `Int2`

`Int2` is the integer counterpart to `Float2`.

{context class=Int2}

`Int2(int x, int y)`
> Constructs a two-component integer vector.

### `Int3`

`Int3` is the integer counterpart to `Float3`.

{context class=Int3}

`Int3(int x, int y, int z)`
> Constructs a three-component integer vector.

`explicit operator Float3() const`
> Converts the vector to floating-point component values.

### `Int4`

`Int4` is the integer counterpart to `Float4`.

{context class=Int4}

`Int4(int x, int y, int z, int w)`
> Constructs a four-component integer vector.

`explicit operator Float4() const`
> Converts the vector to floating-point component values.

### Namespace-Level Functions

`Int2 clamp(const Int2& v, const Int2& mins, const Int2& maxs)`
`Int3 clamp(const Int3& v, const Int3& mins, const Int3& maxs)`
`Int4 clamp(const Int4& v, const Int4& mins, const Int4& maxs)`
> Performs per-component integer clamping.

`Int2 abs(const Int2& a)`
`Int3 abs(const Int3& a)`
`Int4 abs(const Int4& a)`
> Returns the per-component absolute value.

`Int2 min(const Int2& a, const Int2& b)`
`Int3 min(const Int3& a, const Int3& b)`
`Int4 min(const Int4& a, const Int4& b)`
> Returns the per-component minimum.

`Int2 max(const Int2& a, const Int2& b)`
`Int3 max(const Int3& a, const Int3& b)`
`Int4 max(const Int4& a, const Int4& b)`
> Returns the per-component maximum.

Colors
------

### `Color`

`Color` stores 8-bit RGBA components.

{context class=Color}

### Construction

`Color(StringView hex)`
> Parses a color from a hexadecimal string representation.

`Color(u8 r, u8 g, u8 b, u8 a)`
> Constructs a packed RGBA color from 8-bit channels.

### Namespace-Level Functions

`float srgbToLinear(float s)`
> Converts a single-channel sRGB value to linear space.

`float linearToSrgb(float l)`
> Converts a single-channel linear value to sRGB space.

`Float3 srgbToLinear(const Float3& vec)`
`Float4 srgbToLinear(const Float4& vec)`
> Applies sRGB-to-linear conversion per component to RGB or RGBA vectors.

`Float3 linearToSrgb(const Float3& vec)`
`Float4 linearToSrgb(const Float4& vec)`
> Applies linear-to-sRGB conversion per component to RGB or RGBA vectors.

Bounding Boxes
--------------

### `Rect`

`Rect` represents a 2D axis-aligned rectangle using `Float2 mins` and `Float2 maxs`.

{context class=Rect}

`static Rect fromSize(const Float2& mins, const Float2& size)`
> Constructs a rectangle from a minimum corner and a size vector.

`Float2 size() const`
> Returns the rectangle size.

`Float2 mid() const`
> Returns the midpoint of the rectangle.

`bool contains(const Float2& arg) const`
> Tests whether a point lies inside the rectangle using half-open bounds: `mins <= p < maxs`.

`bool intersects(const Rect& arg) const`
> Tests whether two rectangles overlap.

### `AABB`

`AABB` is the 3D equivalent of `Rect`, using `Float3 mins` and `Float3 maxs`.

{context class=AABB}

`static AABB fromSize(const Float3& mins, const Float3& size)`
> Constructs an axis-aligned bounding box from a minimum corner and a size vector.

`Float3 size() const`
> Returns the box dimensions.

`Float3 mid() const`
> Returns the box center point.

`bool contains(const Float3& arg) const`
> Tests point containment using half-open bounds.

`bool intersects(const AABB& arg) const`
> Tests whether two boxes overlap.

### `IntRect`

`IntRect` is the integer counterpart to `Rect`.

{context class=IntRect}

`static IntRect fromSize(const Int2& mins, const Int2& size)`
> Constructs a rectangle from an integer minimum corner and size.

`Int2 size() const`
> Returns the integer size of the rectangle.

`Int2 mid() const`
> Returns the integer midpoint of the rectangle.

`bool contains(const Int2& arg) const`
> Tests point containment using half-open bounds.

`bool intersects(const IntRect& arg) const`
> Tests rectangle overlap using half-open bounds.

### Namespace-Level Functions

`Rect makeUnion(const Rect& a, const Rect& b)`
`AABB makeUnion(const AABB& a, const AABB& b)`
`IntRect makeUnion(const IntRect& a, const IntRect& b)`
> Returns the bounding rectangle of both inputs.

`Rect intersect(const Rect& a, const Rect& b)`
`AABB intersect(const AABB& a, const AABB& b)`
`IntRect intersect(const IntRect& a, const IntRect& b)`
> Returns the overlapping region of the two rectangles.

`Rect rectFromFov(float fovY, float aspect)`
> Computes the view-frustum rectangle on the `z = -1` plane for a perspective projection.

`Rect inflate(const Rect& a, const Float2& b)`
`AABB inflate(const AABB& a, const Float3& b)`
`IntRect inflate(const IntRect& a, const Int2& b)`
> Expands the rectangle by the specified amount in all directions.

`Rect roundNearest(const Rect& a)`
`AABB roundNearest(const AABB& a)`
> Rounds both rectangle corners to the nearest integer values.

Matrices
--------

### `Mat2x2`

`Mat2x2` represents a 2D linear transform stored in column-major form.

{context class=Mat2x2}

`static Mat2x2 identity()`
> Constructs the identity matrix.

`static Mat2x2 scale(const Float2& scale)`
> Constructs a 2D scale matrix.

`static Mat2x2 rotate(float radians)`
> Constructs a 2D rotation matrix.

`static Mat2x2 fromComplex(const Float2& c)`
> Constructs a rotation matrix from a complex-number representation.

### `Mat3x3`

`Mat3x3` represents a 3D linear transform with no translation.

{context class=Mat3x3}

`static Mat3x3 rotate(const Float3& unitAxis, float radians)`
> Constructs a rotation matrix from an axis-angle pair.

`static Mat3x3 fromQuaternion(const Quaternion& q)`
> Constructs a rotation matrix from a quaternion.

`bool hasScale() const`
> Returns `true` when the matrix contains scale or shear instead of only an orthonormal basis.

### `Mat3x4`

`Mat3x4` stores a 3D affine transform: a 3x3 basis plus a translation column.

{context class=Mat3x4}

`static Mat3x4 translate(const Float3& pos)`
> Constructs an affine transform representing translation only.

`static Mat3x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)`
> Constructs an affine transform from quaternion rotation plus translation.

`Mat3x3 asMat3() const`
> Returns the linear 3x3 portion of the transform.

`Mat3x4 invertedOrtho() const`
> Computes a fast inverse for orthonormal affine transforms.

### `Mat4x4`

`Mat4x4` stores a full 4D matrix for homogeneous transforms and projection matrices.

{context class=Mat4x4}

`static Mat4x4 translate(const Float3& pos)`
> Constructs a 4x4 translation transform.

`static Mat4x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)`
> Constructs a 4x4 transform from quaternion rotation plus translation.

`static Mat4x4 perspectiveProjection(const Rect& frustum, float zNear, float zFar, ClipNearType clipNear)`
> Constructs a perspective projection matrix. `clipNear` selects whether the near clip plane maps to `0` or `-1`.

`static Mat4x4 orthographicProjection(const Rect& rect, float zNear, float zFar, ClipNearType clipNear)`
> Constructs an orthographic projection matrix. `clipNear` selects whether the near clip plane maps to `0` or `-1`.

### Namespace-Level Functions

`Float2 operator*(const Mat2x2& m, const Float2& v)`
`Float3 operator*(const Mat3x3& m, const Float3& v)`
`Float3 operator*(const Mat3x4& m, const Float3& v)`
`Float4 operator*(const Mat3x4& m, const Float4& v)`
`Float4 operator*(const Mat4x4& m, const Float4& v)`
> Applies the transform to a vector.

`Mat2x2 operator*(const Mat2x2& a, const Mat2x2& b)`
`Mat3x3 operator*(const Mat3x3& a, const Mat3x3& b)`
`Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b)`
`Mat4x4 operator*(const Mat3x4& a, const Mat4x4& b)`
`Mat4x4 operator*(const Mat4x4& a, const Mat3x4& b)`
`Mat4x4 operator*(const Mat4x4& a, const Mat4x4& b)`
> Composes two transforms.

`bool operator==(const Mat2x2& a, const Mat2x2& b)`
`bool operator==(const Mat3x3& a, const Mat3x3& b)`
`bool operator==(const Mat3x4& a, const Mat3x4& b)`
`bool operator==(const Mat4x4& a, const Mat4x4& b)`
> Returns `true` if all matrix elements are equal.

`Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& srcFwd)`
`Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& dstUp, const Float3& srcUnitFwd, const Float3& srcUnitUp)`
> Builds an orientation matrix that maps one forward/up basis to another.

Complex Numbers
---------------

### `Complex`

`Complex` provides static helpers for 2D rotation using a `Float2` complex-number representation.

{context class=Complex}

`static Float2 identity()`
> Returns the unit complex value representing no rotation.

`static Float2 fromAngle(float radians)`
> Converts an angle in radians to the equivalent complex-number representation.

`static float getAngle(const Float2& v)`
> Converts a complex-number representation back to an angle in radians.

`static Float2 mul(const Float2& a, const Float2& b)`
> Multiplies two complex values, which composes their rotations.

Quaternions
-----------

### `Quaternion`

`Quaternion` stores a 3D rotation using `x`, `y`, `z` and `w`.

{context class=Quaternion}

`static Quaternion identity()`
`static Quaternion fromAxisAngle(const Float3& unitAxis, float radians)`
> Constructs the identity rotation.

`static Quaternion fromAxisAngle(const Float3& unitAxis, float radians)`
> Constructs an axis-angle rotation.

`static Quaternion fromUnitVectors(const Float3& start, const Float3& end)`
> Constructs the rotation that maps one unit vector onto another.

`Quaternion normalized() const`
> Returns a normalized quaternion.

`Quaternion negatedIfCloserTo(const Quaternion& other) const`
> Flips to the equivalent sign that is closer to `other`, which is useful before interpolation.

### Namespace-Level Functions

`Float3 operator*(const Quaternion& q, const Float3& v)`
> Rotates a vector.

`Quaternion operator*(const Quaternion& a, const Quaternion& b)`
> Composes two rotations.

`Quaternion mix(const Quaternion& a, const Quaternion& b, float f)`
> Linearly blends between two quaternion values.
