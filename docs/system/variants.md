Variants (`ply-system.h`)
=======================

A `Variant` can hold a value of one of several predefined types at runtime. It's similar to a [tagged union](https://en.wikipedia.org/wiki/Tagged_union).

    template <typename... Types> class Variant;

{context class=Variant}

`template <typename T> Variant(T&& value)`
> Constructs a variant containing the given value. `T` must be one of the variant's allowed types.

`template <typename T> Variant& operator=(T&& value)`
> Assigns a new value to the variant. The previous value is destroyed first.

`u32 getSubtypeIndex() const`
> Returns the zero-based index of the currently held type within the variant's type list.

`bool isEmpty() const`
> Returns `true` if the variant holds no value.

`template <typename T> bool is() const`
> Returns `true` if the variant currently holds a value of type `T`.

`template <typename T> T* as()`
`template <typename T> const T* as() const`
> Returns a pointer to the contained value if it's of type `T`, or `nullptr` otherwise.

`template <typename T, typename... Args> T& switchTo(Args&&... args)`
> Destroys the current value (if any), constructs a new value of type `T` using the provided arguments, and returns a reference to it.

{example}
Variant<int, String, float> value;
value = 42;
if (value.is<int>()) {
    int* p = value.as<int>();  // p points to the int
}
value.switchTo<String>("hello");
{/example}
