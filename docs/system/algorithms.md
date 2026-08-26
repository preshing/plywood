Generic Algorithms (ply-system.h)
===============================

Plywood provides generic algorithms that work with any array-like container. These are template functions that operate on `Array`, `ArrayView`, `FixedArray`, and other compatible types.

`s32 find(const AnyArray& arr, const Key& key)`
> Performs a linear search from the beginning of the array. Returns the index of the first matching item, or `-1` if not found.

`s32 reverseFind(const AnyArray& arr, const Key& key)`
> Performs a linear search from the end of the array. Returns the index of the last matching item, or `-1` if not found.

`void sort(AnyArray& arr)`
> Sorts the array in ascending order. Items are compared using `operator<`.

`u32 binarySearch(const AnyArray& arr, const Key& key, FindType findType)`
> Performs a binary search on a sorted array. Returns the index of a matching item based on the `findType` parameter. The array must already be sorted.

| | |
| --- | --- |
| `FindType::GreaterThan` | Returns the first item strictly greater than `key` |
| `FindType::GreaterThanOrEqual` | Returns the first item greater than or equal to `key` |

```
Array<int> numbers = {8, 6, 4, 2};
sort(numbers);  // The array is now {2, 4, 6, 8}.

find(numbers, 2);  // Returns 0.
find(numbers, 1);  // Returns -1.
binarySearch(numbers, 5, FindType::GreaterThan);  // Returns 2.
```
