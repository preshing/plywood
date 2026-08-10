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
| `FindGreaterThan` | Returns the first item strictly greater than `key` |
| `FindGreaterThanOrEqual` | Returns the first item greater than or equal to `key` |

```
Array<int> numbers = {5, 2, 8, 1, 9};
sort(numbers);  // numbers is now {1, 2, 5, 8, 9}

s32 idx = find(numbers, 5);  // idx is 2
u32 pos = binarySearch(numbers, 6, FindGreaterThan);  // pos is 3 (points to 8)
```
