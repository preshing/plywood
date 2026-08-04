{title text="2D and 3D Math" include="ply-math.h" namespace="ply"}

The math library provides small POD-style types for vectors, rectangles, matrices, colors and rotations.
Most types expose public fields, support inline arithmetic operators and perform runtime bounds checks in
`operator[]` when [assertions](/docs/base/preprocessor-macros.md#assertions) are enabled.

## Scalar Helpers

The header also defines several scalar constants and free functions that are used throughout the rest of the math API.

{apiSummary title="Scalar helpers"}
-- Constants
static constexpr float Pi
static constexpr double DPi
-- Functions
float square(float v)
float roundNearest(float x)
float roundUp(float value)
float roundDown(float value)
float wrap(float value, float range)
u16 floatToHalf(const char* srcFloat)
float mix(float a, float b, float t)
float unmix(float a, float b, float mixed)
float stepTowards(float start, float target, float amount)
float fastSin(float rad)
float fastCos(float rad)
Float2 fastCosSin(float rad)
float easeInAndOut(float t)
{/apiSummary}

### Common Helpers

`float square(float v)`
> Returns `v * v`.

`float mix(float a, float b, float t)`
> Performs linear interpolation between `a` and `b`.

`float unmix(float a, float b, float mixed)`
> Returns the interpolation parameter that would produce `mixed` between `a` and `b`.

`float stepTowards(float start, float target, float amount)`
> Moves `start` toward `target` by at most `amount` without overshooting.

`float wrap(float value, float range)`
> Wraps `value` into the half-open interval `[0, range)`. `range` must be positive.

### Conversion And Approximation

`u16 floatToHalf(const char* srcFloat)`
> Converts the bit pattern of a 32-bit float to a 16-bit half-float encoding.

`float fastSin(float rad)`
> Fast sine approximation. Use it when speed matters more than exact precision.

`float fastCos(float rad)`
> Fast cosine approximation. Use it when speed matters more than exact precision.

`Float2 fastCosSin(float rad)`
> Returns approximate cosine and sine together as `{cos(rad), sin(rad)}`.

`float easeInAndOut(float t)`
> Smoothstep-style easing curve over the range `[0, 1]`.

## `Bool2`

`Bool2` stores the result of a per-component 2D comparison.

{context class=Bool2}

{apiSummary class="Bool2"}
Bool2(bool x, bool y)
{/apiSummary}

**Related Helpers**

{apiSummary title="`Bool2` helpers"}
bool all(const Bool2& v)
bool any(const Bool2& v)
{/apiSummary}

`Bool2(bool x, bool y)`
> Constructs a two-component boolean vector.

`bool all(const Bool2& v)`
> Returns `true` only if every component is `true`.

`bool any(const Bool2& v)`
> Returns `true` if at least one component is `true`.

## `Bool3`

`Bool3` stores the result of a per-component 3D comparison.

{context class=Bool3}

{apiSummary class="Bool3"}
Bool3(bool x, bool y, bool z)
{/apiSummary}

**Related Helpers**

{apiSummary title="`Bool3` helpers"}
bool all(const Bool3& v)
bool any(const Bool3& v)
{/apiSummary}

`Bool3(bool x, bool y, bool z)`
> Constructs a three-component boolean vector.

`bool all(const Bool3& v)`
> Returns `true` only if every component is `true`.

`bool any(const Bool3& v)`
> Returns `true` if at least one component is `true`.

## `Bool4`

`Bool4` stores the result of a per-component 4D comparison.

{context class=Bool4}

{apiSummary class="Bool4"}
Bool4(bool x, bool y, bool z, bool w)
{/apiSummary}

**Related Helpers**

{apiSummary title="`Bool4` helpers"}
bool all(const Bool4& v)
bool any(const Bool4& v)
{/apiSummary}

`Bool4(bool x, bool y, bool z, bool w)`
> Constructs a four-component boolean vector.

`bool all(const Bool4& v)`
> Returns `true` only if every component is `true`.

`bool any(const Bool4& v)`
> Returns `true` if at least one component is `true`.

## `Float2`

`Float2` represents a 2D vector, point or size.

{context class=Float2}

{apiSummary class="Float2"}
-- Constructors and Conversion
Float2(float t = 0.f)
Float2(float x, float y)
explicit operator Int2() const
-- Accessors
float& operator[](u32 index)
float operator[](u32 index) const
float& r()
float r() const
float& g()
float g() const
-- Magnitude
float lengthSquared() const
float length() const
bool isUnitLength() const
Float2 normalized() const
Float2 safeNormalized(const Float2& fallback = {1, 0}, float epsilon = 1e-6f) const
-- Swizzle
Float2 swizzle(u32 i0, u32 i1) const
Float3 swizzle(u32 i0, u32 i1, u32 i2) const
Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Float2` helpers"}
float dot(const Float2& a, const Float2& b)
float cross(const Float2& a, const Float2& b)
Float2 clamp(const Float2& v, const Float2& mins, const Float2& maxs)
Float2 abs(const Float2& a)
Float2 pow(const Float2& a, const Float2& b)
Float2 min(const Float2& a, const Float2& b)
Float2 max(const Float2& a, const Float2& b)
Float2 roundUp(const Float2& value)
Float2 roundDown(const Float2& value)
Float2 roundNearest(const Float2& value)
Float2 mix(const Float2& a, const Float2& b, const Float2& t)
Float2 unmix(const Float2& a, const Float2& b, const Float2& mixed)
Float2 stepTowards(const Float2& start, const Float2& target, float amount)
{/apiSummary}

### Construction And Access

`Float2(float x, float y)`
> Constructs a vector from explicit `x` and `y` components.

`float& operator[](u32 index)`
`float operator[](u32 index) const`
> Returns the component at the specified index with runtime bounds checking.

### Length And Direction

`Float2 normalized() const`
> Returns a unit-length copy.

`Float2 safeNormalized(const Float2& fallback = {1, 0}, float epsilon = 1e-6f) const`
> Returns `fallback` when the vector is too close to zero length.

### Related Helpers

`float dot(const Float2& a, const Float2& b)`
> Measures directional similarity.

`Float2 clamp(const Float2& v, const Float2& mins, const Float2& maxs)`
> Performs per-component clamping.

`Float2 min(const Float2& a, const Float2& b)`
> Returns the per-component minimum.

`Float2 max(const Float2& a, const Float2& b)`
> Returns the per-component maximum.

`Float2 abs(const Float2& a)`
> Returns the per-component absolute value.

`Float2 pow(const Float2& a, const Float2& b)`
> Raises each component of `a` to the corresponding power in `b`.

`Float2 roundUp(const Float2& value)`
`Float2 roundDown(const Float2& value)`
`Float2 roundNearest(const Float2& value)`
> Rounds each component upward, downward or to the nearest integer value.

`float cross(const Float2& a, const Float2& b)`
> Returns the scalar 2D cross product `a.x * b.y - a.y * b.x`.

`Float2 mix(const Float2& a, const Float2& b, const Float2& t)`
> Performs per-component interpolation.

`Float2 unmix(const Float2& a, const Float2& b, const Float2& mixed)`
> Returns the per-component interpolation parameters that produce `mixed`.

`Float2 stepTowards(const Float2& start, const Float2& target, float amount)`
> Moves each component toward the target by at most `amount`.

`Float2 swizzle(u32 i0, u32 i1) const`
`Float3 swizzle(u32 i0, u32 i1, u32 i2) const`
`Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const`
> Reorders components by index and returns a new vector of the requested size.

## `Float3`

`Float3` represents a 3D vector, point, direction or RGB triple.

{context class=Float3}

{apiSummary class="Float3"}
-- Constructors and Conversion
Float3()
Float3(float t)
Float3(float x, float y, float z)
Float3(const Float2& v, float z)
Float3(const Color& c)
explicit operator Float2() const
explicit operator Int2() const
explicit operator Int3() const
-- Accessors
float& operator[](u32 index)
float operator[](u32 index) const
float& r()
float r() const
float& g()
float g() const
float& b()
float b() const
-- Magnitude
float lengthSquared() const
float length() const
bool isUnitLength() const
Float3 normalized() const
Float3 safeNormalized(const Float3& fallback = {1, 0, 0}, float epsilon = 1e-9f) const
-- Swizzle
Float2 swizzle(u32 i0, u32 i1) const
Float3 swizzle(u32 i0, u32 i1, u32 i2) const
Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Float3` helpers"}
float dot(const Float3& a, const Float3& b)
Float3 cross(const Float3& a, const Float3& b)
Float3 clamp(const Float3& v, const Float3& mins, const Float3& maxs)
Float3 abs(const Float3& a)
Float3 pow(const Float3& a, const Float3& b)
Float3 min(const Float3& a, const Float3& b)
Float3 max(const Float3& a, const Float3& b)
Float3 roundUp(const Float3& value)
Float3 roundDown(const Float3& value)
Float3 roundNearest(const Float3& value)
Float3 mix(const Float3& a, const Float3& b, const Float3& t)
Float3 unmix(const Float3& a, const Float3& b, const Float3& mixed)
Float3 stepTowards(const Float3& start, const Float3& target, float amount)
Float3 getNoncollinear(const Float3& unitVec)
{/apiSummary}

### Construction And Access

`Float3(float x, float y, float z)`
`Float3(const Float2& v, float z)`
> Constructs a 3D vector either from three components or by extending a `Float2`.

`float& operator[](u32 index)`
`float operator[](u32 index) const`
> Returns the component at the specified index with runtime bounds checking.

### Length And Orientation

`Float3 normalized() const`
> Returns a unit-length copy.

`Float3 safeNormalized(const Float3& fallback = {1, 0, 0}, float epsilon = 1e-9f) const`
> Returns `fallback` if the vector is too close to zero length.

### Related Helpers

`float dot(const Float3& a, const Float3& b)`
> Computes projection similarity.

`Float3 cross(const Float3& a, const Float3& b)`
> Returns the perpendicular 3D vector.

`Float3 getNoncollinear(const Float3& unitVec)`
> Returns a convenient axis that is not too closely aligned with `unitVec`.

`Float3 clamp(const Float3& v, const Float3& mins, const Float3& maxs)`
> Performs per-component clamping.

`Float3 abs(const Float3& a)`
> Returns the per-component absolute value.

`Float3 pow(const Float3& a, const Float3& b)`
> Raises each component of `a` to the corresponding power in `b`.

`Float3 min(const Float3& a, const Float3& b)`
> Returns the per-component minimum.

`Float3 max(const Float3& a, const Float3& b)`
> Returns the per-component maximum.

`Float3 roundUp(const Float3& value)`
`Float3 roundDown(const Float3& value)`
`Float3 roundNearest(const Float3& value)`
> Rounds each component upward, downward or to the nearest integer value.

`Float3 mix(const Float3& a, const Float3& b, const Float3& t)`
> Performs per-component interpolation.

`Float3 unmix(const Float3& a, const Float3& b, const Float3& mixed)`
> Returns the per-component interpolation parameters that produce `mixed`.

`Float3 stepTowards(const Float3& start, const Float3& target, float amount)`
> Moves each component toward the target by at most `amount`.

## `Float4`

`Float4` is commonly used for homogeneous coordinates, RGBA values and quaternion storage.

{context class=Float4}

{apiSummary class="Float4"}
-- Constructors and Conversion
Float4()
Float4(float t)
Float4(float x, float y, float z, float w)
Float4(const Float3& v, float w)
Float4(const Float2& v, float z, float w)
Float4(const Color& color)
explicit operator Float2() const
explicit operator Float3() const
explicit operator Int2() const
explicit operator Int3() const
explicit operator Int4() const
explicit operator Quaternion() const
explicit operator Color() const
-- Accessors
float& operator[](u32 index)
float operator[](u32 index) const
float& r()
float r() const
float& g()
float g() const
float& b()
float b() const
float& a()
float a() const
-- Magnitude
float lengthSquared() const
float length() const
bool isUnitLength() const
Float4 normalized() const
Float4 safeNormalized(const Float4& fallback = {1, 0, 0, 0}, float epsilon = 1e-9f) const
-- Swizzle
Float2 swizzle(u32 i0, u32 i1) const
Float3 swizzle(u32 i0, u32 i1, u32 i2) const
Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Float4` helpers"}
float dot(const Float4& a, const Float4& b)
Float4 clamp(const Float4& v, const Float4& mins, const Float4& maxs)
Float4 abs(const Float4& a)
Float4 pow(const Float4& a, const Float4& b)
Float4 min(const Float4& a, const Float4& b)
Float4 max(const Float4& a, const Float4& b)
Float4 roundUp(const Float4& value)
Float4 roundDown(const Float4& value)
Float4 roundNearest(const Float4& value)
Float4 mix(const Float4& a, const Float4& b, const Float4& t)
Float4 unmix(const Float4& a, const Float4& b, const Float4& mixed)
Float4 stepTowards(const Float4& start, const Float4& target, float amount)
{/apiSummary}

### Construction And Conversion

`Float4(float x, float y, float z, float w)`
> Constructs a four-component vector directly.

`Float4(const Color& color)`
> Constructs a vector from normalized RGBA values derived from `Color`.

`explicit operator Quaternion() const`
> Converts `Float4` to a quaternion with the same four components.

`explicit operator Color() const`
> Converts to a packed color. This asserts that all components lie in `[0, 1]`.

### Length And Component Operations

`Float4 normalized() const`
> Returns a unit-length copy.

### Related Helpers

`float dot(const Float4& a, const Float4& b)`
> Computes the four-dimensional dot product.

`Float4 clamp(const Float4& v, const Float4& mins, const Float4& maxs)`
> Performs per-component clamping.

`Float4 abs(const Float4& a)`
> Returns the per-component absolute value.

`Float4 pow(const Float4& a, const Float4& b)`
> Raises each component of `a` to the corresponding power in `b`.

`Float4 min(const Float4& a, const Float4& b)`
> Returns the per-component minimum.

`Float4 max(const Float4& a, const Float4& b)`
> Returns the per-component maximum.

`Float4 roundUp(const Float4& value)`
`Float4 roundDown(const Float4& value)`
`Float4 roundNearest(const Float4& value)`
> Rounds each component upward, downward or to the nearest integer value.

`Float4 mix(const Float4& a, const Float4& b, const Float4& t)`
> Performs per-component interpolation.

`Float4 unmix(const Float4& a, const Float4& b, const Float4& mixed)`
> Returns the per-component interpolation parameters that produce `mixed`.

`Float4 stepTowards(const Float4& start, const Float4& target, float amount)`
> Moves each component toward the target by at most `amount`.

`Float4 swizzle(u32 i0, u32 i1, u32 i2, u32 i3) const`
> Reorders components by index and returns a new four-component vector.

## `Rect`

`Rect` represents a 2D axis-aligned rectangle using `Float2 mins` and `Float2 maxs`.

{context class=Rect}

{apiSummary class="Rect"}
-- Constructors and Conversion
Rect()
Rect(float v)
Rect(const Float2& v)
Rect(const Float2& mins, const Float2& maxs)
Rect(float minX, float minY, float maxX, float maxY)
static Rect fromSize(const Float2& mins, const Float2& size)
static Rect empty()
static Rect full()
explicit operator IntRect() const
-- Geometry
Float2 size() const
bool isEmpty() const
float width() const
float height() const
Float2 mix(const Float2& arg) const
Float2 unmix(const Float2& arg) const
Float2 mid() const
Rect mix(const Rect& arg) const
Rect unmix(const Rect& arg) const
Float2 clamp(const Float2& arg) const
Float2 xminYmax() const
Float2 xmaxYmin() const
bool contains(const Float2& arg) const
bool contains(const Rect& arg) const
bool intersects(const Rect& arg) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Rect` helpers"}
Rect makeUnion(const Rect& a, const Rect& b)
Rect intersect(const Rect& a, const Rect& b)
Rect inflate(const Rect& a, const Float2& b)
Rect roundNearest(const Rect& a)
Rect rectFromFov(float fovY, float aspect)
{/apiSummary}

### Construction And Queries

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

### Related Helpers

`Rect makeUnion(const Rect& a, const Rect& b)`
> Returns the bounding rectangle of both inputs.

`Rect intersect(const Rect& a, const Rect& b)`
> Returns the overlapping region of the two rectangles.

`Rect rectFromFov(float fovY, float aspect)`
> Computes the view-frustum rectangle on the `z = -1` plane for a perspective projection.

`Rect inflate(const Rect& a, const Float2& b)`
> Expands the rectangle by the specified amount in all directions.

`Rect roundNearest(const Rect& a)`
> Rounds both rectangle corners to the nearest integer values.

## `AABB`

`AABB` is the 3D equivalent of `Rect`, using `Float3 mins` and `Float3 maxs`.

{context class=AABB}

{apiSummary class="AABB"}
-- Constructors
AABB()
AABB(const Float3& v)
AABB(const Float3& mins, const Float3& maxs)
static AABB empty()
static AABB full()
static AABB fromSize(const Float3& mins, const Float3& size)
-- Geometry
Float3 size() const
bool isEmpty() const
float width() const
float height() const
float depth() const
Float3 mix(const Float3& arg) const
Float3 unmix(const Float3& arg) const
Float3 mid() const
AABB mix(const AABB& arg) const
AABB unmix(const AABB& arg) const
Float3 clamp(const Float3& arg) const
Float3 xminYmax() const
Float3 xmaxYmin() const
bool contains(const Float3& arg) const
bool contains(const AABB& arg) const
bool intersects(const AABB& arg) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`AABB` helpers"}
AABB makeUnion(const AABB& a, const AABB& b)
AABB intersect(const AABB& a, const AABB& b)
AABB inflate(const AABB& a, const Float3& b)
AABB roundNearest(const AABB& a)
{/apiSummary}

### Construction And Queries

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

### Related Helpers

`AABB makeUnion(const AABB& a, const AABB& b)`
> Returns the bounding box of both inputs.

`AABB intersect(const AABB& a, const AABB& b)`
> Returns the overlapping volume of the two boxes.

`AABB inflate(const AABB& a, const Float3& b)`
> Expands the box by the specified amount in all directions.

`AABB roundNearest(const AABB& a)`
> Rounds both box corners to the nearest integer values.

## `Int2`

`Int2` is the integer counterpart to `Float2`.

{context class=Int2}

{apiSummary class="Int2"}
Int2()
Int2(int x)
Int2(int x, int y)
explicit operator Float2() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Int2` helpers"}
Int2 clamp(const Int2& v, const Int2& mins, const Int2& maxs)
Int2 abs(const Int2& a)
Int2 min(const Int2& a, const Int2& b)
Int2 max(const Int2& a, const Int2& b)
{/apiSummary}

`Int2(int x, int y)`
> Constructs a two-component integer vector.

### Related Helpers

`Int2 clamp(const Int2& v, const Int2& mins, const Int2& maxs)`
> Performs per-component integer clamping.

`Int2 abs(const Int2& a)`
> Returns the per-component absolute value.

`Int2 min(const Int2& a, const Int2& b)`
> Returns the per-component minimum.

`Int2 max(const Int2& a, const Int2& b)`
> Returns the per-component maximum.

## `Int3`

`Int3` is the integer counterpart to `Float3`.

{context class=Int3}

{apiSummary class="Int3"}
Int3()
Int3(int x)
Int3(int x, int y, int z)
explicit operator Int2() const
explicit operator Float2() const
explicit operator Float3() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Int3` helpers"}
Int3 clamp(const Int3& v, const Int3& mins, const Int3& maxs)
Int3 abs(const Int3& a)
Int3 min(const Int3& a, const Int3& b)
Int3 max(const Int3& a, const Int3& b)
{/apiSummary}

`Int3(int x, int y, int z)`
> Constructs a three-component integer vector.

`explicit operator Float3() const`
> Converts the vector to floating-point component values.

### Related Helpers

`Int3 clamp(const Int3& v, const Int3& mins, const Int3& maxs)`
> Performs per-component integer clamping.

`Int3 abs(const Int3& a)`
> Returns the per-component absolute value.

`Int3 min(const Int3& a, const Int3& b)`
> Returns the per-component minimum.

`Int3 max(const Int3& a, const Int3& b)`
> Returns the per-component maximum.

## `Int4`

`Int4` is the integer counterpart to `Float4`.

{context class=Int4}

{apiSummary class="Int4"}
Int4()
Int4(int x, int y, int z, int w)
explicit operator Int2() const
explicit operator Int3() const
explicit operator Float2() const
explicit operator Float3() const
explicit operator Float4() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Int4` helpers"}
Int4 clamp(const Int4& v, const Int4& mins, const Int4& maxs)
Int4 abs(const Int4& a)
Int4 min(const Int4& a, const Int4& b)
Int4 max(const Int4& a, const Int4& b)
{/apiSummary}

`Int4(int x, int y, int z, int w)`
> Constructs a four-component integer vector.

`explicit operator Float4() const`
> Converts the vector to floating-point component values.

### Related Helpers

`Int4 clamp(const Int4& v, const Int4& mins, const Int4& maxs)`
> Performs per-component integer clamping.

`Int4 abs(const Int4& a)`
> Returns the per-component absolute value.

`Int4 min(const Int4& a, const Int4& b)`
> Returns the per-component minimum.

`Int4 max(const Int4& a, const Int4& b)`
> Returns the per-component maximum.

## `IntRect`

`IntRect` is the integer counterpart to `Rect`.

{context class=IntRect}

{apiSummary class="IntRect"}
-- Constructors and Conversion
IntRect()
IntRect(const Int2& v)
IntRect(const Int2& mins, const Int2& maxs)
IntRect(int minX, int minY, int maxX, int maxY)
static IntRect fromSize(const Int2& mins, const Int2& size)
static IntRect empty()
static IntRect full()
explicit operator Rect() const
-- Geometry
Int2 size() const
bool isEmpty() const
int width() const
int height() const
Int2 mid() const
Int2 clamp(const Int2& arg) const
Int2 xminYmax() const
Int2 xmaxYmin() const
bool contains(const Int2& arg) const
bool contains(const IntRect& arg) const
bool intersects(const IntRect& arg) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`IntRect` helpers"}
IntRect makeUnion(const IntRect& a, const IntRect& b)
IntRect intersect(const IntRect& a, const IntRect& b)
IntRect inflate(const IntRect& a, const Int2& b)
{/apiSummary}

### Construction And Queries

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

### Related Helpers

`IntRect makeUnion(const IntRect& a, const IntRect& b)`
> Returns the bounding rectangle of both inputs.

`IntRect intersect(const IntRect& a, const IntRect& b)`
> Returns the overlapping region of the two rectangles.

`IntRect inflate(const IntRect& a, const Int2& b)`
> Expands the rectangle by the specified amount in all directions.

## `Color`

`Color` stores 8-bit RGBA components.

{context class=Color}

{apiSummary class="Color"}
Color()
Color(StringView hex)
Color(u8 r, u8 g, u8 b, u8 a)
{/apiSummary}

**Related Helpers**

{apiSummary title="Color helpers"}
float srgbToLinear(float s)
float linearToSrgb(float l)
Float3 srgbToLinear(const Float3& vec)
Float4 srgbToLinear(const Float4& vec)
Float3 linearToSrgb(const Float3& vec)
Float4 linearToSrgb(const Float4& vec)
{/apiSummary}

### Construction

`Color(StringView hex)`
> Parses a color from a hexadecimal string representation.

`Color(u8 r, u8 g, u8 b, u8 a)`
> Constructs a packed RGBA color from 8-bit channels.

### Related Helpers

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

## `Mat2x2`

`Mat2x2` represents a 2D linear transform stored in column-major form.

{context class=Mat2x2}

{apiSummary class="Mat2x2"}
Mat2x2()
Mat2x2(const Float2& col0, const Float2& col1)
Float2& operator[](u32 i)
const Float2& operator[](u32 i) const
static Mat2x2 identity()
static Mat2x2 scale(const Float2& scale)
static Mat2x2 rotate(float radians)
static Mat2x2 fromComplex(const Float2& c)
Mat2x2 transposed() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Mat2x2` helpers"}
bool operator==(const Mat2x2& a, const Mat2x2& b)
Float2 operator*(const Mat2x2& m, const Float2& v)
Mat2x2 operator*(const Mat2x2& a, const Mat2x2& b)
{/apiSummary}

### Factory Functions

`static Mat2x2 identity()`
> Constructs the identity matrix.

`static Mat2x2 scale(const Float2& scale)`
> Constructs a 2D scale matrix.

`static Mat2x2 rotate(float radians)`
> Constructs a 2D rotation matrix.

`static Mat2x2 fromComplex(const Float2& c)`
> Constructs a rotation matrix from a complex-number representation.

### Related Helpers

`Float2 operator*(const Mat2x2& m, const Float2& v)`
> Applies the transform to a vector.

`Mat2x2 operator*(const Mat2x2& a, const Mat2x2& b)`
> Composes two transforms.

`bool operator==(const Mat2x2& a, const Mat2x2& b)`
> Returns `true` if all matrix elements are equal.

## `Mat3x3`

`Mat3x3` represents a 3D linear transform with no translation.

{context class=Mat3x3}

{apiSummary class="Mat3x3"}
Mat3x3()
Mat3x3(const Float3& col0, const Float3& col1, const Float3& col2)
explicit Mat3x3(const Mat3x4& m)
explicit Mat3x3(const Mat4x4& m)
static Mat3x3 identity()
static Mat3x3 scale(const Float3& arg)
static Mat3x3 rotate(const Float3& unitAxis, float radians)
static Mat3x3 fromQuaternion(const Quaternion& q)
Float3& operator[](u32 i)
const Float3& operator[](u32 i) const
bool hasScale() const
Mat3x3 transposed() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Mat3x3` helpers"}
bool operator==(const Mat3x3& a, const Mat3x3& b)
Float3 operator*(const Mat3x3& m, const Float3& v)
Mat3x3 operator*(const Mat3x3& a, const Mat3x3& b)
Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& dstUp, const Float3& srcUnitFwd, const Float3& srcUnitUp)
Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& srcFwd)
{/apiSummary}

### Factory Functions

`static Mat3x3 rotate(const Float3& unitAxis, float radians)`
> Constructs a rotation matrix from an axis-angle pair.

`static Mat3x3 fromQuaternion(const Quaternion& q)`
> Constructs a rotation matrix from a quaternion.

`bool hasScale() const`
> Returns `true` when the matrix contains scale or shear instead of only an orthonormal basis.

### Related Helpers

`Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& dstUp, const Float3& srcUnitFwd, const Float3& srcUnitUp)`
> Builds an orientation matrix that maps one forward/up basis to another.

`Mat3x3 makeBasis(const Float3& dstUnitFwd, const Float3& srcFwd)`
> Builds an orientation matrix from forward vectors using automatically chosen up vectors.

`Float3 operator*(const Mat3x3& m, const Float3& v)`
> Applies the linear transform to a vector.

`Mat3x3 operator*(const Mat3x3& a, const Mat3x3& b)`
> Composes two linear transforms.

`bool operator==(const Mat3x3& a, const Mat3x3& b)`
> Returns `true` if all matrix elements are equal.

## `Mat3x4`

`Mat3x4` stores a 3D affine transform: a 3x3 basis plus a translation column.

{context class=Mat3x4}

{apiSummary class="Mat3x4"}
Mat3x4()
Mat3x4(const Float3& col0, const Float3& col1, const Float3& col2, const Float3& col3)
explicit Mat3x4(const Mat3x3& m, const Float3& pos = 0)
explicit Mat3x4(const Mat4x4& m)
static Mat3x4 identity()
static Mat3x4 scale(const Float3& arg)
static Mat3x4 rotate(const Float3& unitAxis, float radians)
static Mat3x4 translate(const Float3& pos)
static Mat3x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)
static Mat3x4 fromQuatPos(const QuatPos& qp)
Float3& operator[](u32 i)
const Float3& operator[](u32 i) const
const Mat3x3& as_mat3() const
bool hasScale() const
Mat3x4 invertedOrtho() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Mat3x4` helpers"}
bool operator==(const Mat3x4& a, const Mat3x4& b)
Float3 operator*(const Mat3x4& m, const Float3& v)
Float4 operator*(const Mat3x4& m, const Float4& v)
Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b)
{/apiSummary}

### Factory Functions

`static Mat3x4 translate(const Float3& pos)`
> Constructs an affine transform representing translation only.

`static Mat3x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)`
> Constructs an affine transform from quaternion rotation plus translation.

`static Mat3x4 fromQuatPos(const QuatPos& qp)`
> Constructs an affine transform from a `QuatPos`.

### Related Helpers

`const Mat3x3& as_mat3() const`
> Returns the linear 3x3 portion of the transform.

`Mat3x4 invertedOrtho() const`
> Computes a fast inverse for orthonormal affine transforms.

`Float3 operator*(const Mat3x4& m, const Float3& v)`
> Applies the affine transform to a 3D point or vector.

`Float4 operator*(const Mat3x4& m, const Float4& v)`
> Applies the affine transform to a homogeneous vector.

`Mat3x4 operator*(const Mat3x4& a, const Mat3x4& b)`
> Composes two affine transforms.

`bool operator==(const Mat3x4& a, const Mat3x4& b)`
> Returns `true` if all matrix elements are equal.

## `Mat4x4`

`Mat4x4` stores a full 4D matrix for homogeneous transforms and projection matrices.

{context class=Mat4x4}

{apiSummary class="Mat4x4"}
-- Constructors
Mat4x4()
Mat4x4(float t)
Mat4x4(const Float4& col0, const Float4& col1, const Float4& col2, const Float4& col3)
explicit Mat4x4(const Mat3x3& m, const Float3& pos = 0)
explicit Mat4x4(const Mat3x4& m)
-- Factory Functions
static Mat4x4 identity()
static Mat4x4 scale(const Float3& arg)
static Mat4x4 rotate(const Float3& unitAxis, float radians)
static Mat4x4 translate(const Float3& pos)
static Mat4x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)
static Mat4x4 fromQuatPos(const QuatPos& qp)
static Mat4x4 perspectiveProjection(const Rect& frustum, float zNear, float zFar, ClipNearType clipNear)
static Mat4x4 orthographicProjection(const Rect& rect, float zNear, float zFar, ClipNearType clipNear)
-- Access and Operations
Float4& operator[](u32 i)
const Float4& operator[](u32 i) const
Mat4x4 transposed() const
Mat4x4 inverted() const
Mat4x4 invertedOrtho() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Mat4x4` helpers"}
bool operator==(const Mat4x4& a, const Mat4x4& b)
Float4 operator*(const Mat4x4& m, const Float4& v)
Mat4x4 operator*(const Mat4x4& a, const Mat4x4& b)
Mat4x4 operator*(const Mat3x4& a, const Mat4x4& b)
Mat4x4 operator*(const Mat4x4& a, const Mat3x4& b)
{/apiSummary}

### Transform Factories

`static Mat4x4 translate(const Float3& pos)`
> Constructs a 4x4 translation transform.

`static Mat4x4 fromQuaternion(const Quaternion& q, const Float3& pos = 0)`
> Constructs a 4x4 transform from quaternion rotation plus translation.

### Projection Factories

`static Mat4x4 perspectiveProjection(const Rect& frustum, float zNear, float zFar, ClipNearType clipNear)`
> Constructs a perspective projection matrix. `clipNear` selects whether the near clip plane maps to `0` or `-1`.

`static Mat4x4 orthographicProjection(const Rect& rect, float zNear, float zFar, ClipNearType clipNear)`
> Constructs an orthographic projection matrix. `clipNear` selects whether the near clip plane maps to `0` or `-1`.

### Related Helpers

`Mat4x4 operator*(const Mat4x4& a, const Mat4x4& b)`
> Composes two matrices.

`Float4 operator*(const Mat4x4& m, const Float4& v)`
> Transforms a homogeneous vector.

`Mat4x4 operator*(const Mat3x4& a, const Mat4x4& b)`
`Mat4x4 operator*(const Mat4x4& a, const Mat3x4& b)`
> Multiplies affine and full 4x4 transforms in either order.

`bool operator==(const Mat4x4& a, const Mat4x4& b)`
> Returns `true` if all matrix elements are equal.

## `Complex`

`Complex` provides static helpers for 2D rotation using a `Float2` complex-number representation.

{context class=Complex}

{apiSummary class="Complex"}
static Float2 identity()
static Float2 fromAngle(float radians)
static float getAngle(const Float2& v)
static Float2 mul(const Float2& a, const Float2& b)
{/apiSummary}

`static Float2 identity()`
> Returns the unit complex value representing no rotation.

`static Float2 fromAngle(float radians)`
> Converts an angle in radians to the equivalent complex-number representation.

`static float getAngle(const Float2& v)`
> Converts a complex-number representation back to an angle in radians.

`static Float2 mul(const Float2& a, const Float2& b)`
> Multiplies two complex values, which composes their rotations.

## `Quaternion`

`Quaternion` stores a 3D rotation using `x`, `y`, `z` and `w`.

{context class=Quaternion}

{apiSummary class="Quaternion"}
Quaternion()
Quaternion(float x, float y, float z, float w)
Quaternion(const Float3& v, float w)
static Quaternion identity()
static Quaternion fromAxisAngle(const Float3& unitAxis, float radians)
static Quaternion fromUnitVectors(const Float3& start, const Float3& end)
static Quaternion fromOrtho(const Mat3x3& m)
static Quaternion fromOrtho(const Mat4x4& m)
explicit operator Float3() const
explicit operator Float4() const
Quaternion inverted() const
Quaternion normalized() const
bool isUnitLength() const
Quaternion negatedIfCloserTo(const Quaternion& other) const
{/apiSummary}

**Related Helpers**

{apiSummary title="`Quaternion` helpers"}
Float3 operator*(const Quaternion& q, const Float3& v)
Quaternion operator*(const Quaternion& a, const Quaternion& b)
Quaternion mix(const Quaternion& a, const Quaternion& b, float f)
{/apiSummary}

### Factory Functions

`static Quaternion identity()`
`static Quaternion fromAxisAngle(const Float3& unitAxis, float radians)`
> Constructs the identity rotation.

`static Quaternion fromAxisAngle(const Float3& unitAxis, float radians)`
> Constructs an axis-angle rotation.

`static Quaternion fromUnitVectors(const Float3& start, const Float3& end)`
> Constructs the rotation that maps one unit vector onto another.

### Normalization And Sign

`Quaternion normalized() const`
> Returns a normalized quaternion.

`Quaternion negatedIfCloserTo(const Quaternion& other) const`
> Flips to the equivalent sign that is closer to `other`, which is useful before interpolation.

### Related Helpers

`Float3 operator*(const Quaternion& q, const Float3& v)`
> Rotates a vector.

`Quaternion operator*(const Quaternion& a, const Quaternion& b)`
> Composes two rotations.

`Quaternion mix(const Quaternion& a, const Quaternion& b, float f)`
> Linearly blends between two quaternion values.

## `QuatPos`

`QuatPos` combines a quaternion rotation with a translation vector.

{context class=QuatPos}

{apiSummary class="QuatPos"}
QuatPos()
QuatPos(const Quaternion& quat, const Float3& pos)
static QuatPos identity()
static QuatPos translate(const Float3& pos)
static QuatPos rotate(const Float3& unitAxis, float radians)
static QuatPos fromOrtho(const Mat3x4& m)
static QuatPos fromOrtho(const Mat4x4& m)
QuatPos inverted() const
{/apiSummary}

**Related Helpers**

{apiSummary title="`QuatPos` helpers"}
Float3 operator*(const QuatPos& qp, const Float3& v)
QuatPos operator*(const QuatPos& a, const QuatPos& b)
QuatPos operator*(const QuatPos& a, const Quaternion& b)
QuatPos operator*(const Quaternion& a, const QuatPos& b)
{/apiSummary}

### Factory Functions

`static QuatPos identity()`
> Constructs the identity rigid transform.

`static QuatPos translate(const Float3& pos)`
> Constructs a rigid transform representing pure translation.

`static QuatPos rotate(const Float3& unitAxis, float radians)`
> Constructs a rigid transform representing pure rotation.

### Related Helpers

`QuatPos inverted() const`
> Returns the inverse rigid transform.

`Float3 operator*(const QuatPos& qp, const Float3& v)`
> Applies the transform to a point.

`QuatPos operator*(const QuatPos& a, const QuatPos& b)`
> Composes two rigid transforms.

`QuatPos operator*(const QuatPos& a, const Quaternion& b)`
`QuatPos operator*(const Quaternion& a, const QuatPos& b)`
> Applies an additional rotation on the right or left side of a rigid transform.

## Cubic Bezier Helpers

The header ends with generic helpers for sampling cubic Bezier curves.

{apiSummary title="Bezier helpers"}
template <typename T> T sampleCubicBezier(const T& p0, const T& p1, const T& p2, const T& p3, float t)
template <typename T> T sampleCubicBezierDerivative(const T& p0, const T& p1, const T& p2, const T& p3, float t)
{/apiSummary}

`template <typename T> T sampleCubicBezier(const T& p0, const T& p1, const T& p2, const T& p3, float t)`
> Evaluates the cubic Bezier curve at parameter `t`.

`template <typename T> T sampleCubicBezierDerivative(const T& p0, const T& p1, const T& p2, const T& p3, float t)`
> Evaluates the derivative curve at parameter `t`, which is useful for tangent calculations.
