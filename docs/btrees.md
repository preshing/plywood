{title text="B-Trees" include="ply-btree.h" namespace="ply"}

A `BTree` is a collection of items that supports fast lookup using a key type that's automatically determined from the item type. It's similar to [`Set`](/docs/hash-maps#Set), except that the items are kept in sorted order, and the key type doesn't have to be hashable, only sortable.

    template <typename Item> class BTree;

`BTree` objects are movable, copyable and construct to an empty collection by default. They provide the following member functions:

{api_summary class=BTree}
-- Additional Constructors
BTree(std::initializer_list<Item> items)
-- Accessing Items
bool find(const Key& desired_key) const
ConstIterator find_earliest(const Key& desired_key, FindType find_type) const
ConstIterator begin() const
ConstIterator end() const
-- Modifying the B-Tree
void clear()
void insert(Arg_ item_to_insert)
void insert(Iterator* insert_pos, Arg_  item_to_insert)
bool erase(const Key& key_to_erase)
void erase(Iterator erase_pos)
{/api_summary}

A type is *sortable* if it can be compared using the `<` operator. Sortable item types can be used directly as the item type.

    BTree<u32> tree = {4, 5, 6};
    PLY_ASSERT(tree.find(4));  // OK

Otherwise, the item type must implement a `get_lookup_key` member function. The return type of `get_lookup_key` determines the key type.

    struct CustomItem {
        String key;
        u32 value;

        StringView get_lookup_key() const {
            return this->key;
        }
    };

    BTree<CustomItem> tree = {
        {"apple", 1},
        {"banana", 2},
        {"cherry", 3},
    };
    PLY_ASSERT(tree.find("banana"));  // OK

The items in a `BTree` are always kept in sorted order.

    BTree<u32> tree = {7, 5, 6, 4};
    for (u32 item : tree) {
        get_stdout().format("{}\n", item);
    }

{output}
4
5
6
7
{/output}

### Additional Constructors

{api_descriptions class=BTree}
BTree(std::initializer_list<Item> items)
--
Constructs a B-tree from a braced initializer list.
{/api_descriptions}

### Accessing Items

{api_descriptions class=BTree}
bool find(const Key& desired_key) const
--
Returns `true` if an item with the given key exists in the tree.

>>
ConstIterator find_earliest(const Key& desired_key, FindType find_type) const
--
Finds the first item matching the given criteria. Use `FindGreaterThan` or `FindGreaterThanOrEqual` to specify the comparison.

>>
ConstIterator begin() const
ConstIterator end() const
--
Returns iterators for range-based for loops. Items are yielded in sorted order.
{/api_descriptions}

### Modifying the B-Tree

{api_descriptions class=BTree}
void clear()
--
Removes all items from the tree.

>>
void insert(Arg_ item_to_insert)
--
Inserts an item into the tree. The tree remains sorted after insertion.

>>
void insert(Iterator* insert_pos, Arg_ item_to_insert)
--
Inserts an item at a specific position. The caller must ensure the position maintains sorted order.

>>
bool erase(const Key& key_to_erase)
--
Removes the item with the given key. Returns `true` if an item was removed.

>>
void erase(Iterator erase_pos)
--
Removes the item at the given iterator position.
{/api_descriptions}
