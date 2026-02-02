{title text="B-Trees" include="ply-btree.h" namespace="ply"}

A `BTree` is a collection of items that supports fast lookup using a key type that's automatically determined from the item type. It's similar to [`Set`](/docs/hash-maps#Set), except that the items are kept in sorted order, and the key type doesn't have to be hashable, only sortable.

    template <typename Item> class BTree;

`BTree` objects are movable, copyable and construct to an empty collection by default. They provide the following member functions:

{apiSummary class=BTree}
-- Additional Constructors
BTree(std::initializer_list<Item> items)
-- Accessing Items
bool find(const Key& desiredKey) const
ConstIterator findEarliest(const Key& desiredKey, FindType findType) const
ConstIterator begin() const
ConstIterator end() const
-- Modifying the B-Tree
void clear()
void insert(Arg_ itemToInsert)
void insert(Iterator* insertPos, Arg_  itemToInsert)
bool erase(const Key& keyToErase)
void erase(Iterator erasePos)
{/apiSummary}

A type is *sortable* if it can be compared using the `<` operator. Sortable item types can be used directly as the item type.

    BTree<u32> tree = {4, 5, 6};
    PLY_ASSERT(tree.find(4));  // OK

Otherwise, the item type must implement a `getLookupKey` member function. The return type of `getLookupKey` determines the key type.

    struct CustomItem {
        String key;
        u32 value;

        StringView getLookupKey() const {
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
        getStdOut().format("{}\n", item);
    }

{output}
4
5
6
7
{/output}

### Additional Constructors

{apiDescriptions class=BTree}
BTree(std::initializer_list<Item> items)
--
Constructs a B-tree from a braced initializer list.
{/apiDescriptions}

### Accessing Items

{apiDescriptions class=BTree}
bool find(const Key& desiredKey) const
--
Returns `true` if an item with the given key exists in the tree.

>>
ConstIterator findEarliest(const Key& desiredKey, FindType findType) const
--
Finds the first item matching the given criteria. Use `FindGreaterThan` or `FindGreaterThanOrEqual` to specify the comparison.

>>
ConstIterator begin() const
ConstIterator end() const
--
Returns iterators for range-based for loops. Items are yielded in sorted order.
{/apiDescriptions}

### Modifying the B-Tree

{apiDescriptions class=BTree}
void clear()
--
Removes all items from the tree.

>>
void insert(Arg_ itemToInsert)
--
Inserts an item into the tree. The tree remains sorted after insertion.

>>
void insert(Iterator* insertPos, Arg_ itemToInsert)
--
Inserts an item at a specific position. The caller must ensure the position maintains sorted order.

>>
bool erase(const Key& keyToErase)
--
Removes the item with the given key. Returns `true` if an item was removed.

>>
void erase(Iterator erasePos)
--
Removes the item at the given iterator position.
{/apiDescriptions}
