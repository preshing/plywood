{title text="Generic Algorithms" include="ply-base.h" namespace="ply"}

Plywood provides generic algorithms that work with any array-like container. These are template functions that operate on `Array`, `ArrayView`, `FixedArray`, and other compatible types.

{api_summary}
s32 find(const AnyArray& arr, const Key& key)
s32 reverse_find(const AnyArray& arr, const Key& key)
void sort(AnyArray& arr)
u32 binary_search(const AnyArray& arr, const Key& key, FindType find_type)
{/api_summary}

{api_descriptions}
s32 find(const AnyArray& arr, const Key& key)
--
Performs a linear search from the beginning of the array. Returns the index of the first matching item, or `-1` if not found.

>>
s32 reverse_find(const AnyArray& arr, const Key& key)
--
Performs a linear search from the end of the array. Returns the index of the last matching item, or `-1` if not found.

>>
void sort(AnyArray& arr)
--
Sorts the array in ascending order. Items are compared using `operator<`.

>>
u32 binary_search(const AnyArray& arr, const Key& key, FindType find_type)
--
Performs a binary search on a sorted array. Returns the index of a matching item based on the `find_type` parameter. The array must already be sorted.
{/api_descriptions}

{table caption="`FindType` values"}
`FindGreaterThan` | Returns the first item strictly greater than `key`
`FindGreaterThanOrEqual` | Returns the first item greater than or equal to `key`
{/table}

{example}
Array<int> numbers = {5, 2, 8, 1, 9};
sort(numbers);  // numbers is now {1, 2, 5, 8, 9}

s32 idx = find(numbers, 5);  // idx is 2
u32 pos = binary_search(numbers, 6, FindGreaterThan);  // pos is 3 (points to 8)
{/example}
