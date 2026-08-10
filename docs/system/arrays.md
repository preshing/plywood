Arrays (`ply-system.h`)
=====================

Arrays are sequences of items of the same type stored contiguously in memory. Plywood provides three class templates for working with arrays:

- `Array<Item>` is a dynamically resizable array that owns its contents. 
- `ArrayView<Item>` is a non-owning view into another array.
- `FixedArray<Item, NumItems>` is a fixed-size array that owns its contents.

All classes perform runtime bounds checking when [assertions](/docs/system/preprocessor-macros.md#assertions) are enabled.

These classes all use 32-bit integers for indexing, which means they can store a maximum of roughly 4 billion items.

When a reference to an array is `const`, all of its items effectively become `const`, making the array read-only.

These classes aren't thread-safe. Functions that read from the same array can be called concurrently from separate threads, but functions that modify the same array must not be called concurrently.

## Common Array Methods

The following member functions are implemented for all array classes:

### Accessing Items

{context class=Array}

`Item& operator[](u32 index) &`
`Item&& operator[](u32 index) &&`
`const Item& operator[](u32 index) const&`
> Subscript operator with runtime bounds checking. Returns a reference to the item at the specified `index`. Will assert if `index` is out of bounds.
> ```
> Array<u32> array = {4, 5, 6};
> array[0];   // Returns 4
> array[10];  // Asserts
> ```

`Item& back(s32 offset = -1) &`
`Item&& back(s32 offset = -1) &&`
`const Item& back(s32 offset = -1) const&`
> Returns a reference to the last item if no argument is provided; otherwise returns a reference to the item at the specified `offset` relative to the end of the array. An `offset` of `-1` returns the last item, `-2` returns the second-to-last item, and so on. Will assert if `offset` is out of bounds.
> ```
> Array<u32> array = {4, 5, 6};
> array.back();     // Returns 6
> array.back(-2);   // Returns 5
> array.back(-10);  // Asserts
> ```

`Item* items()`
`const Item* items() const`
> Returns a raw pointer to the first item in the array, or `nullptr` if the array is empty. Avoid calling this function since it bypasses the bounds checking normally performed by other methods. Mainly used to pass array contents to legacy functions that expect a raw pointer.

`u32 numItems() const`
> Returns the number of items in the array.

`bool isEmpty() const`
> Returns `true` if the array is empty.

`explicit operator bool() const`
> Returns `true` if the array is non-empty. Allows array objects to be used in `if` and `while` conditions.
> ```
> Array<u32> array = {4, 5, 6};
> if (array) {
>     // array is not empty
> }
> ```

`ArrayView<Item> subview(u32 start)`
`ArrayView<const Item> subview(u32 start) const`
`ArrayView<Item> subview(u32 start, u32 numItems)`
`ArrayView<const Item> subview(u32 start, u32 numItems) const`
> Returns a view of a portion of the array. If only one argument is provided, returns a view from `start` to the end of the array. If two arguments are provided, returns a view of at most `numItems`. These functions don't assert if their arguments are out of range; instead they return an empty or truncated view.
> ```
> Array<u32> array = {4, 5, 6};
> array.subview(1);     // Returns {5, 6}
> array.subview(0, 2);  // Returns {4, 5}
> array.subview(2, 3);  // Returns {6}
> array.subview(10);    // Returns an empty view
> ```

`Item* begin()`
`const Item* begin() const`
`Item* end()`
`const Item* end() const`
> Lets you use arrays in [range-based for loops](https://en.cppreference.com/w/cpp/language/range-for.html).
>
> ```
> Array<u32> array = {4, 5, 6};
> for (u32 item : array) {
>     getStdOut().format("{}\n", item);
> }
>
> // Output:
> // 4
> // 5
> // 6
> ```

### Casting to Other Types

`ArrayView<Item> view()`
`ArrayView<const Item> view() const`
> Explicitly creates an `ArrayView` of the entire array. If the array is `const`, a read-only view is created.

`operator ArrayView<Item>()`
`operator ArrayView<const Item>() const`
> Makes the array implicitly convertible to `ArrayView`. The second form allows arrays to be passed to functions that accept only read-only `ArrayView` arguments.
> ```
> // A function that accepts a read-only ArrayView.
> void useReadOnlyView(ArrayView<const u32> array);
>
> void test() {
>     Array<u32> array = {4, 5, 6};
>     useReadOnlyView(array);  // Implicit conversion to ArrayView<const u32>
> }
> ```

`StringView stringView() const`
> Interprets the array's bytes as a string and returns a `StringView`.

`MutStringView mutStringView()`
> Interprets the array's bytes as a mutable string.

`Item* release()`
> Releases ownership of the internal buffer and returns a pointer to it. The array becomes empty and the caller is responsible for freeing the memory.

## `Array`

An `Array` instance is a dynamically resizable array that owns all its items, similar to `std::vector` from the C++ Standard Library.

```
template <typename Item> class Array;
```

In addition to the [common array methods](#common) listed in the previous section, `Array` provides the following member functions:

Be careful not to resize the array while holding a pointer or reference to an item.

It automatically allocates memory to hold the elements from the heap.When an `Array` object is destroyed, all of its items are destructed and the memory containing them is freed. the , 
, similar to `std::vector`.

const.

It's movable and copyable. Be careful to avoid unwanted copies.

Internally, it's represented as:

| | |
| --- | --- |
| `Item*` | `items` |
| `u32` | `numItems` |
| `u32` | `population` |

The items are allocated from [the Plywood heap](/docs/system/memory-management.md#heap). There are some gotchas to watch out for. The allocattion strategy is simple. It allocates memory by powers of 2 but you can trim it by calling `compact()`.

You don't need to define the type before declaring an Array member variable. But you do need it if instantiating a variable.

```
struct Foo;  // Forward declaration

struct Bar {
    Array<Foo> array;  // OK to declare member variable before defining Foo
};

void test() {
    Array<Foo> array;  // error: Foo should be defined at this point
}
```

### Additional Constructors

The `Array` class template supports default and move constructors as well as move assignment. It supports copy construction and copy assignment as long as the underlying item type is copyable. Be careful to avoid unwanted copies such as when assigning to `auto` or passing by value. In addition, it also supports the following constructors and assignment operators:

`template <typename T> Array(T&& otherArray)`
> Constructs from any compatible array. `otherArray` can be an `Array`, `ArrayView`, `FixedArray` or a fixed-size C-style array of any type convertible to `Item`. If `otherArray` is an rvalue reference, the items are constructed using move semantics if possible.
> ```
> String temp[] = {"apple", "banana", "cherry"}; // Fixed-size C-style array of Strings.
> Array<String> array{std::move(temp)};          // String items are moved.
> ```

`Array(std::initializer_list<Item> initList)`
> Constructs an array directly from a C++11-style braced initializer list.
> ```
> Array<int> array = {3, 4, 5};
> ```

`static Array<Item> adopt(Item* items, u32 numItems)`
> Explicitly create an `Array` object from the provided arguments. No memory is allocated and no constructors are called; the returned array simply adopts the provided `items`, which must be allocated from [the Plywood heap](/docs/system/memory-management.md#heap). This memory will be freed when the `Array` is destructed.

### Additional Assignment Operators

`template <typename T> Array<Item>& operator=(T&& otherArray)`
> Assigns from any compatible array. `otherArray` can be an `Array`, `ArrayView`, `FixedArray` or a fixed-size C-style array of any type convertible to `Item`. If `otherArray` is an rvalue reference, the items are assigned using move semantics if possible.

`Array<Item>& operator=(std::initializer_list initList)`
> Assigns directly from a C++11-style braced initializer list.

### Modifying Array Contents

`void resize(u32 numItems)`
> Resizes the array.

`void clear()`
> Clears the array.

`Item& append(const Item& item)`
`Item& append(Item&& item)`
> Adds an item to the end of the array and returns a reference to it. The array is resized if necessary.

`template <typename... Args> Item& append(Args&&... args)`
> Constructs a new item in place at the end of the array using the provided arguments, and returns a reference to it.

`Array<Item>& operator+=(Array<Item>&& otherArray)`
> Move extend.

`template <typename T> Array<Item>& operator+=(T&& otherArray)`
> Extend from any compatible array.

`Array<Item>& operator+=(std::initializer_list initList)`
> Extend from initializer list.

`Item& insert(u32 pos, u32 count = 1)`
> Inserts `count` default-constructed items at position `pos`. Existing items at and after `pos` are shifted. Returns a reference to the first inserted item.

`void erase(u32 pos, u32 count = 1)`
> Removes `count` items starting at `pos`. Items after the erased range are shifted down to fill the gap.

`void eraseQuick(u32 pos, u32 count = 1)`
> Removes `count` items starting at `pos` by moving the last items of the array into the gap. This is faster than `erase` but does not preserve order.

`void pop(u32 count = 1)`
> Removes `count` items from the end of the array.

`void reserve(u32 numItems)`
> Ensures the array has capacity for at least `numItems` without reallocation. Does not change the current size.

`void compact()`
> Releases any excess capacity, shrinking the buffer to match the current number of items.

## `ArrayView`

An `ArrayView` is a non-owning view into another array.

Supports default, move and copy construction and move and copy assignment. Implements the following member functions in addition to the [common string methods](#common-methods):

By convention, Plywood usually passes `ArrayView` objects to functions by value instead of by reference, since copying an `ArrayView` doesn't copy the underlying items.

It's implicitly convertible to `ArrayView<const Item>`.

### Additional Constructors

{context class=ArrayView}

`ArrayView(Item* items, u32 numItems)`
> Constructs a view over an existing buffer of `numItems` items starting at `items`.

`ArrayView(Item (&s)[N])`
> Constructs a view from a static array. The size is automatically inferred.

`ArrayView(std::initializer_list<Item> init)`
> Constructs a view from an initializer list. Be careful: the initializer list's storage is temporary.

## `FixedArray`

A `FixedArray` is a fixed-size array that owns its contents.

```
template <typename Item, u32 Size> class FixedArray;
```

No additional heap allocations are performed. All the array items are stored directly in the `FixedArray` object. It's equivalent to a C-style array, but with runtime bounds checking.

The default constructor default-constructs all items.

### Additional Constructors

{context class=FixedArray}

`FixedArray(std::initializer_list<Item> args)`
> Constructs the array from an initializer list. The list must have exactly `Size` elements.

`FixedArray(Args&&... args)`
> Constructs the array from the provided arguments. Each argument is used to construct one item.
