Associative Maps (`ply-system.h`)
===============================

Hash maps are collections of items that support fast key lookup. Plywood provides two class templates for hash maps:

* `Set<Item>` uses a key type that's automatically determined from the item type. The items are kept in insertion order and the key type must be hashable.
* `Map<Key, Value>` uses a key type that's determined by a separate template argument. The items are kept in insertion order and the key type must be hashable.

These classes aren't thread-safe. Functions that modify the same collection must not be called concurrently.

## `Set`

A `Set` is a collection of items that supports fast lookup using a key type that's automatically determined from the item type. The key type must be hashable.

```
template <typename Item> class Set;
```

Hashable item types can be used directly as the key type.

```
Set<u32> set = {4, 5, 6};
PLY_ASSERT(set.find(4));  // OK
```

Otherwise, the item type must implement a `getLookupKey` member function. The return type of `getLookupKey` determines the key type.

```
struct CustomItem {
    String key;
    u32 value;

    StringView getLookupKey() const {
        return this->key;
    }
};

Set<CustomItem> set = {
    {"apple", 1},
    {"banana", 2},
    {"cherry", 3},
};
PLY_ASSERT(set.find("banana"));  // OK
```

The items in a `Set` are maintained in insertion order unless `eraseQuick` is called.

```
Set<u32> set = {4, 5, 6, 7};
ArrayView<u32> items = set.items();  // Returns {4, 5, 6, 7}.

// Calling erase maintains insertion order.
set.erase(5);
items = set.items();  // Returns {4, 6, 7}.

// Calling eraseQuick can change the order of remaining items.
set.eraseQuick(4);
items = set.items();  // Returns {7, 6}.
```

`Set` objects are movable, copyable and construct to an empty collection by default.

{context class=Set}

`Set(std::initializer_list<Item> items)`
> Constructs a set from a braced initializer list. The items are inserted in the order they appear in the list.

`const Item* find(const Key& key) const`
> Looks for an item in the collection that matches the given key. Returns a pointer to the item if found, or `nullptr` if not found.
>
> The returned pointer is temporary. Any subsequent change to the `Set` can invalidate this pointer, so don't store it beyond the next call to `find` or `erase`, even if those calls involve different keys.

`ArrayView<Item> items()`
`ArrayView<const Item> items() const`
> Returns a view of all items in the set. The items are in insertion order unless `eraseQuick` was called.

`void clear()`
> Calls the destructor of all existing items and resets to an empty set.

`InsertResult insert(const Key& key)`
> Inserts a new item in the set using the given key if it doesn't already exist. The `Item` type must be constructible from `Key`. Returns an `InsertResult` with the following members:
>
> [TBD]
>
> This function is actually a function template that uses SFINAE to delete itself if the `Key` type is not constructible from the `Item` type. In particular, this means you can't call this function on a `Set<Owned<T>>`; you can only call `insertItem` on such sets.

`InsertResult insertItem(Item&& item)`
> Inserts a fully constructed item into the set using move semantics. Any existing item with the same key is replaced.

`bool erase(const Key& key)`
> Removes the item with the given key. The remaining items are kept in insertion order. If an existing item was found in the set, its destructor is called and `true` is returned. Otherwise, returns `false`. This function is slower than `eraseQuick`.

`bool eraseQuick(const Key& key)`
> Removes the item with the given key without keeping the remaining items in insertion order. If an existing item was found in the set, its destructor is called and `true` is returned. Otherwise, returns `false`.

## `Map`

A `Map` is a collection of key-value pairs whose types are determined by template arguments.

```
template <typename Key, typename Value> class Map;
```

`Key` can either be a hashable type or a type that maps to a hashable type using `getLookupKey`, such as `String`. `KeyView` is a type alias for the return type of `getLookupKey`.

`Item` is a member type that represents a key-value pair. It has the following members:

| | |
| --- | --- |
| `Key key` | The key in the pair. |
| `Value value` | The value in the pair. |

The items in a `Map` are kept in insertion order unless `eraseQuick` is called.

```
Map<u32, String> map = {
    {4, "apple"},
    {5, "banana"},
    {6, "cherry"},
    {7, "date"},
};

// Calling erase maintains insertion order.
map.erase(5);
auto items = map.items();  // Returns {{4, "apple"}, {6, "cherry"}, {7, "date"}}.

// Calling eraseQuick can change the order of remaining items.
map.eraseQuick(4);
items = map.items();  // Returns {{7, "date"}, {6, "cherry"}}.
```

`Map` objects are movable, copyable and construct to an empty collection by default.

{context class=Map}

`Map(std::initializer_list<Item> items)`
> Constructs a map from a braced initializer list. The key-value pairs are inserted in the order they appear in the list.

`const Value* find(const KeyView& key) const`
> Looks up a value by key. Returns a pointer to the value if found, or `nullptr` if not present.

`ArrayView<Item> items()`
`ArrayView<const Item> items() const`
> Returns a view of all key-value pairs in the map. The pairs are in insertion order unless `eraseQuick` was called.

`void clear()`
> Calls the destructor of all existing items and resets to an empty map.

`InsertResult insert(const KeyView& key)`
> Inserts a new key-value pair with the given key if it doesn't already exist. The value is default-constructed. Returns an `InsertResult` with the following members:
>
> [TBD]

`bool erase(const KeyView& key)`
> Removes the key-value pair with the given key. The remaining pairs are kept in insertion order. If an existing pair was found in the map, its destructor is called and `true` is returned. Otherwise, returns `false`. This function is slower than `eraseQuick`.

`bool eraseQuick(const KeyView& key)`
> Removes the key-value pair with the given key without keeping the remaining pairs in insertion order. If an existing pair was found in the map, its destructor is called and `true` is returned. Otherwise, returns `false`.

## Custom Key Types

In the `Set` and `Map` class templates, [hashing](https://en.wikipedia.org/wiki/Hash_function) is used to speed up key lookups. Any hashable type can be used as a lookup key. In Plywood, the following types are hashable by default:

| |
| --- |
| `s8` |
| `s16` |
| `s32` |
| `s64` |
| `u8` |
| `u16` |
| `u32` |
| `u64` |
| `float` |
| `double` |
| `T*` |
| `StringView` |

* When hashing a pointer, only the address is used, not the contents of the pointed-to object. [TBD: Change this.]
* You can hash a `String` by implicitly converting it to a `StringView`.

To make additional types hashable, overload the `addToHash` function for the desired type, as demonstrated below.

```
struct CustomType {
    u32 x;
    String str;
};

// User-defined addToHash overload.
void addToHash(HashBuilder& builder, const CustomType& item) {
    addToHash(builder, item.x);
    addToHash(builder, item.str);
}
```

`addToHash` is called internally by `Set` and `Map`. It's called using [argument-dependent lookup](https://en.cppreference.com/w/cpp/language/adl.html), so you can define it in the same namespace as the type itself.
