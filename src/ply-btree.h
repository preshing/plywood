/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#pragma once

#include "ply-base.h"

namespace ply {

//  ▄▄▄▄▄  ▄▄▄▄▄▄
//  ██  ██   ██   ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀█▄   ██   ██  ▀▀ ██▄▄██ ██▄▄██
//  ██▄▄█▀   ██   ██     ▀█▄▄▄  ▀█▄▄▄
//

template <typename Item>
struct BTree {
    using Key = LookupKey<Item>;

    constexpr static u32 MaxItemsPerNode = 16;

    struct InnerNode;

    struct Node {
        InnerNode* parent = nullptr;
        Node* left_sibling = nullptr;
        Node* right_sibling = nullptr;
        Key max_key;
        bool is_leaf = true;
    };

    struct InnerNode : Node {
        u16 num_children;
        Key child_keys[MaxItemsPerNode];
        Node* children[MaxItemsPerNode];

        const Key& get_min_key() const {
            PLY_ASSERT(this->num_children > 0 && this->num_children <= MaxItemsPerNode);
            return this->child_keys[0];
        }
        const Key& get_internal_max_key() const {
            PLY_ASSERT(this->num_children > 0 && this->num_children <= MaxItemsPerNode);
            return this->children[this->num_children - 1]->max_key;
        }
    };

    struct LeafNode : Node {
        u16 num_items;
        Item items[MaxItemsPerNode];

        auto get_min_key() const {
            PLY_ASSERT(this->num_items > 0 && this->num_items <= MaxItemsPerNode);
            return get_lookup_key(this->items[0]);
        }
        auto get_internal_max_key() const {
            PLY_ASSERT(this->num_items > 0 && this->num_items <= MaxItemsPerNode);
            return get_lookup_key(this->items[this->num_items - 1]);
        }
    };

    Node* root = nullptr;
    u32 num_items = 0;

    struct Iterator {
        BTree* btree = nullptr;
        LeafNode* leaf_node = nullptr;
        u32 item_index = 0;

        operator bool() const {
            return this->leaf_node;
        }
        Item& operator*() const {
            PLY_ASSERT(this->leaf_node);
            return this->leaf_node->items[this->item_index];
        }
        Item* operator->() const {
            PLY_ASSERT(this->leaf_node);
            return &this->leaf_node->items[this->item_index];
        }
        void operator++(int) {
            this->item_index++;
            if (this->item_index >= this->leaf_node->num_items) {
                this->leaf_node = static_cast<LeafNode*>(this->leaf_node->right_sibling);
                PLY_ASSERT(!this->leaf_node || this->leaf_node->is_leaf);
                this->item_index = 0;
            }
        }
        void operator++() {
            (*this)++;
        }
        void operator--(int) {
            this->item_index--;
            // Test for wrap-around.
            if (this->item_index > MaxItemsPerNode) {
                if (!this->leaf_node) {
                    *this = this->btree->get_last_item();
                } else {
                    this->leaf_node = static_cast<LeafNode*>(this->leaf_node->left_sibling);
                    if (this->leaf_node) {
                        PLY_ASSERT(leaf_node->is_leaf);
                        PLY_ASSERT((leaf_node->num_items >= MaxItemsPerNode / 2) &&
                                   (leaf_node->num_items <= MaxItemsPerNode));
                        this->item_index = this->leaf_node->num_items - 1;
                    } else {
                        this->item_index = 0;
                    }
                }
            }
        }
        void operator--() {
            (*this)--;
        }
    };

    struct ConstIterator : Iterator {
        ConstIterator() = default;
        ConstIterator(const Iterator& iter) : Iterator(iter) {
        }

        const Item& operator*() const {
            PLY_ASSERT(this->leaf_node);
            return this->leaf_node->items[this->item_index];
        }
        const Item* operator->() const {
            PLY_ASSERT(this->leaf_node);
            return &this->leaf_node->items[this->item_index];
        }
    };

private:
    //------------------------------------------------
    PLY_NO_INLINE static void on_min_key_changed(Node* node) {
        PLY_ASSERT(node);
        if (node->parent) {
            // Find the index of this node within its parent.
            PLY_ASSERT(node->parent->num_children > 0);
            u32 index_in_parent = 0;
            for (; index_in_parent < node->parent->num_children; index_in_parent++) {
                if (node->parent->children[index_in_parent] == node)
                    break;
            }
            PLY_ASSERT(index_in_parent < node->parent->num_children); // Must be found.

            // Update the corresponding child key.
            if (node->is_leaf) {
                node->parent->child_keys[index_in_parent] = static_cast<LeafNode*>(node)->get_min_key();
            } else {
                node->parent->child_keys[index_in_parent] = static_cast<InnerNode*>(node)->get_min_key();
            }

            if (index_in_parent == 0) {
                on_min_key_changed(node->parent);
            }
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE static void on_max_key_changed(Node* node) {
        PLY_ASSERT(node);
        if (node->is_leaf) {
            node->max_key = static_cast<LeafNode*>(node)->get_internal_max_key();
        } else {
            node->max_key = static_cast<InnerNode*>(node)->get_internal_max_key();
        }
        if (node->parent) {
            PLY_ASSERT(node->parent->num_children > 0);
            if (node->parent->children[node->parent->num_children - 1] == node) {
                on_max_key_changed(node->parent);
            }
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE void insert_right_sibling(Node* existing_node, Node* node_to_insert) {
        // Add node_to_insert to the linked list of siblings, just to the right of existing_node.
        node_to_insert->right_sibling = existing_node->right_sibling;
        if (node_to_insert->right_sibling) {
            node_to_insert->right_sibling->left_sibling = node_to_insert;
        }
        node_to_insert->left_sibling = existing_node;
        existing_node->right_sibling = node_to_insert;

        // Locate existing_node inside its parent.
        InnerNode* existing_parent = existing_node->parent;
        u32 insert_index = 0;
        if (existing_parent) {
            for (; insert_index < existing_parent->num_children; insert_index++) {
                if (existing_parent->children[insert_index] == existing_node)
                    break;
            }
            // existing_node must have been found.
            PLY_ASSERT(insert_index < existing_parent->num_children);
            // Increment because we are inserting to the right.
            insert_index++;
        } else {
            // There's no parent for this node, which means it's currently the root.
            // Create new root node and make this node its only child. node_to_insert will
            // be inserted to its right.
            PLY_ASSERT(this->root == existing_node);
            InnerNode* new_root = (InnerNode*) Heap::alloc(sizeof(InnerNode));
            new (new_root) Node; // Construct base class members only
            new_root->is_leaf = false;
            new_root->num_children = 1;
            if (existing_node->is_leaf) {
                new (&new_root->child_keys[0]) Key{get_lookup_key(static_cast<LeafNode*>(existing_node)->items[0])};
            } else {
                new (&new_root->child_keys[0]) Key{static_cast<InnerNode*>(existing_node)->child_keys[0]};
            }
            new_root->children[0] = existing_node;
            new_root->max_key = new_root->child_keys[0];
            existing_node->parent = new_root;
            this->root = new_root;
            existing_parent = new_root;
            insert_index = 1;
        }

        // Set node_to_insert's parent optimistically.
        node_to_insert->parent = existing_parent;

        // If the parent is full, split it in two before inserting.
        InnerNode* split_parent = nullptr;
        if (existing_parent->num_children == MaxItemsPerNode) {
            // Split parent into two nodes. split_parent will be the new sibling to its right.
            split_parent = (InnerNode*) Heap::alloc(sizeof(InnerNode));
            new (split_parent) Node; // Construct base class members only.
            split_parent->is_leaf = false;
            // Move half of parent's items to split_parent.
            split_parent->num_children = existing_parent->num_children / 2;
            existing_parent->num_children -= split_parent->num_children;
            for (u32 i = 0; i < split_parent->num_children; i++) {
                Key& key_to_move = existing_parent->child_keys[existing_parent->num_children + i];
                new (&split_parent->child_keys[i]) Key{std::move(key_to_move)};
                key_to_move.~Key();
                split_parent->children[i] = existing_parent->children[existing_parent->num_children + i];
                split_parent->children[i]->parent = split_parent;
            }

            // If out of range...
            if (insert_index > existing_parent->num_children) {
                node_to_insert->parent = split_parent;
                insert_index -= existing_parent->num_children;
            }

            // Update max keys.
            split_parent->max_key = std::move(existing_parent->max_key);
            on_max_key_changed(existing_parent);
        }

        // Insert node_to_insert into its parent at the prescribed index.
        if (insert_index == node_to_insert->parent->num_children) {
            if (node_to_insert->is_leaf) {
                new (&node_to_insert->parent->child_keys[insert_index])
                    Key{static_cast<LeafNode*>(node_to_insert)->get_min_key()};
            } else {
                new (&node_to_insert->parent->child_keys[insert_index])
                    Key{static_cast<InnerNode*>(node_to_insert)->get_min_key()};
            }
        } else {
            u32 i = node_to_insert->parent->num_children;
            new (&node_to_insert->parent->child_keys[i]) Key{std::move(node_to_insert->parent->child_keys[i - 1])};
            node_to_insert->parent->children[i] = node_to_insert->parent->children[i - 1];
            for (; i > insert_index; i--) {
                node_to_insert->parent->child_keys[i] = std::move(node_to_insert->parent->child_keys[i - 1]);
                node_to_insert->parent->children[i] = node_to_insert->parent->children[i - 1];
            }
            if (node_to_insert->is_leaf) {
                node_to_insert->parent->child_keys[insert_index] =
                    static_cast<LeafNode*>(node_to_insert)->get_min_key();
            } else {
                node_to_insert->parent->child_keys[insert_index] =
                    static_cast<InnerNode*>(node_to_insert)->get_min_key();
            }
        }
        node_to_insert->parent->children[insert_index] = node_to_insert;
        node_to_insert->parent->num_children++;
        PLY_ASSERT(node_to_insert->parent->num_children <= MaxItemsPerNode);
        if (insert_index == 0) {
            on_min_key_changed(node_to_insert->parent);
        }
        if (insert_index == (u32) node_to_insert->parent->num_children - 1) {
            on_max_key_changed(node_to_insert->parent);
        }

        // If the parent was split, insert split_parent into parent's parent.
        if (split_parent) {
            this->insert_right_sibling(existing_parent, split_parent);
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE void insert_internal(Iterator* insert_pos, Item& item_to_insert, bool with_move_semantics) {
        if (!insert_pos->leaf_node) {
            // A null iterator means insert at the end of the list.
            if (!this->root) {
                // It's an empty tree. Create a new Leaf_Node and set it as root.
                insert_pos->leaf_node = (LeafNode*) Heap::alloc(sizeof(LeafNode));
                insert_pos->item_index = 0;
                // Construct base class members only (no Items are constructed).
                new (insert_pos->leaf_node) Node;
                insert_pos->leaf_node->num_items = 0;
                this->root = insert_pos->leaf_node;
            } else {
                // Find the actual insert position.
                *insert_pos = this->get_last_item();
                insert_pos->item_index++;
            }
        }

        // If the leaf node is full, split it in two before inserting.
        if (insert_pos->leaf_node->num_items == MaxItemsPerNode) {
            LeafNode* leaf_node = insert_pos->leaf_node;
            u32 N = leaf_node->num_items;

            // Split this leaf node in two. split_node will be the new sibling to its right.
            LeafNode* split_node = (LeafNode*) Heap::alloc(sizeof(LeafNode));
            // Construct base class members only (no Items are constructed).
            new (split_node) Node;
            split_node->parent = leaf_node->parent;
            // Move half leaf_node's items to split_node.
            split_node->num_items = N / 2;
            N -= split_node->num_items;
            for (u32 i = 0; i < split_node->num_items; i++) {
                Item* src_item = &leaf_node->items[N + i];
                new (&split_node->items[i]) Item{std::move(*src_item)}; // Move to destination.
                src_item->~Item();                                      // Destruct source item.
            }
            insert_pos->leaf_node->num_items = N;

            // If the input Iterator no longer refers to a valid Item, modify it to point to the correct Item in
            // split_node.
            if (insert_pos->item_index >= N) {
                insert_pos->leaf_node = split_node;
                insert_pos->item_index -= N;
            }

            // Update max keys.
            split_node->max_key = leaf_node->max_key;
            on_max_key_changed(leaf_node);

            this->insert_right_sibling(leaf_node, split_node);
        }

        // Insert into the leaf node.
        LeafNode* leaf_node = insert_pos->leaf_node;
        u32 N = leaf_node->num_items;
        PLY_ASSERT(N < MaxItemsPerNode);
        PLY_ASSERT(insert_pos->item_index <= N);
        leaf_node->num_items++;
        if (insert_pos->item_index == N) {
            // It's the last item in the leaf node. Move-construct it directly here.
            if (with_move_semantics) {
                new (&leaf_node->items[N]) Item{std::move(item_to_insert)};
            } else {
                new (&leaf_node->items[N]) Item{static_cast<const Item&>(item_to_insert)};
            }
            on_max_key_changed(leaf_node);
        } else {
            // It's not the last item in the leaf node. Move-construct the last item to the right of the insert
            // position.
            new (&leaf_node->items[N]) Item{std::move(leaf_node->items[N - 1])};
            // Move the items on the right of the insert position to the right.
            for (u32 i = N - 1; i > insert_pos->item_index; i--) {
                leaf_node->items[i] = std::move(leaf_node->items[i - 1]);
            }
            // Move the new item to the insert position.
            if (with_move_semantics) {
                leaf_node->items[insert_pos->item_index] = std::move(item_to_insert);
            } else {
                leaf_node->items[insert_pos->item_index] = static_cast<const Item&>(item_to_insert);
            }
        }

        if (insert_pos->item_index == 0) {
            on_min_key_changed(leaf_node);
        }

        // Increment the number of items in the tree.
        this->num_items++;

#if defined(PLY_WITH_ASSERTS)
        // Validate non-decreasing order to the left of the insert position.
        if (insert_pos->item_index > 0) {
            PLY_ASSERT(get_lookup_key(leaf_node->items[insert_pos->item_index - 1]) <=
                       get_lookup_key(leaf_node->items[insert_pos->item_index]));
        } else if (leaf_node->left_sibling) {
            LeafNode* left_sibling = static_cast<LeafNode*>(leaf_node->left_sibling);
            PLY_ASSERT(left_sibling->is_leaf);
            PLY_ASSERT(get_lookup_key(left_sibling->items[left_sibling->num_items - 1]) <=
                       get_lookup_key(leaf_node->items[0]));
        }
        // Validate non-decreasing order to the right of the insert position.
        if (insert_pos->item_index + 1 < leaf_node->num_items) {
            PLY_ASSERT(get_lookup_key(leaf_node->items[insert_pos->item_index]) <=
                       get_lookup_key(leaf_node->items[insert_pos->item_index + 1]));
        } else if (leaf_node->right_sibling) {
            LeafNode* right_sibling = static_cast<LeafNode*>(leaf_node->right_sibling);
            PLY_ASSERT(right_sibling->is_leaf);
            PLY_ASSERT(get_lookup_key(leaf_node->items[insert_pos->item_index]) >=
                       get_lookup_key(right_sibling->items[0]));
        }
#endif
    }

    //------------------------------------------------
    PLY_NO_INLINE void merge_with_right_sibling(Node* node) {
        PLY_ASSERT(node);
        PLY_ASSERT(node->right_sibling);

        if (node->is_leaf) {
            LeafNode* leaf_node = static_cast<LeafNode*>(node);
            LeafNode* right_sibling = static_cast<LeafNode*>(node->right_sibling);
            u32 N = leaf_node->num_items;
            for (u32 i = 0; i < right_sibling->num_items; i++) {
                new (&leaf_node->items[N + i]) Item{std::move(right_sibling->items[i])};
                right_sibling->items[i].~Item();
            }
            leaf_node->num_items += right_sibling->num_items;
        } else {
            InnerNode* inner_node = static_cast<InnerNode*>(node);
            InnerNode* right_sibling = static_cast<InnerNode*>(node->right_sibling);
            u32 N = inner_node->num_children;
            for (u32 i = 0; i < right_sibling->num_children; i++) {
                Key& key_to_move = right_sibling->child_keys[i];
                inner_node->child_keys[N + i] = std::move(key_to_move);
                key_to_move.~Key();
                inner_node->children[N + i] = right_sibling->children[i];
                inner_node->children[N + i]->parent = inner_node;
            }
            inner_node->num_children += right_sibling->num_children;
        }
        on_max_key_changed(node);

        // We want to erase the right sibling from its parent.
        InnerNode* parent = node->right_sibling->parent;
        InnerNode* parent_left_sibling = static_cast<InnerNode*>(parent->left_sibling);
        InnerNode* parent_right_sibling = static_cast<InnerNode*>(parent->right_sibling);

        // But first, if the parent is only half full, select a strategy to avoid making it any smaller.
        bool steal_from_left = false;
        bool steal_from_right = false;
        if (parent->num_children == MaxItemsPerNode / 2) {
            if (parent_left_sibling && (parent_left_sibling->num_children > MaxItemsPerNode / 2)) {
                // Steal a node from the left sibling.
                steal_from_left = true;
            } else if (parent_right_sibling && (parent_right_sibling->num_children > MaxItemsPerNode / 2)) {
                // Steal a node from the right sibling.
                steal_from_right = true;
            } else if (parent_left_sibling) {
                // Merge the left sibling before deleting.
                parent = parent_left_sibling;
                merge_with_right_sibling(parent);
            } else if (parent_right_sibling) {
                // Merge with the right sibling before deleting.
                merge_with_right_sibling(parent);
            }
        }

        // Locate the right sibling within its parent.
        Node* right_sibling = node->right_sibling;
        u32 erase_index = 0;
        for (; erase_index < parent->num_children; erase_index++) {
            if (parent->children[erase_index] == right_sibling)
                break;
        }
        // The right sibling must have been found.
        PLY_ASSERT(erase_index < parent->num_children);

        // Erase the right sibling from its parent.
        if (steal_from_left) {
            // Move the child nodes left of the erase position to the right.
            for (u32 i = erase_index; i > 0; i--) {
                parent->child_keys[i] = std::move(parent->child_keys[i - 1]);
                parent->children[i] = parent->children[i - 1];
            }
            // Move the child node from the left sibling to the first position.
            Key& key_to_move = parent_left_sibling->child_keys[parent_left_sibling->num_children - 1];
            parent->child_keys[0] = std::move(key_to_move);
            key_to_move.~Key();
            parent->children[0] = parent_left_sibling->children[parent_left_sibling->num_children - 1];
            parent->children[0]->parent = parent;
            // Decrement the number of items in the left sibling.
            parent_left_sibling->num_children--;
            on_max_key_changed(parent_left_sibling);
            on_min_key_changed(parent);
            if (erase_index == (u32) parent->num_children - 1) {
                on_max_key_changed(parent);
            }
        } else {
            // Move the child nodes on the right of the erase position to the left.
            for (u32 i = erase_index; i < (u32) parent->num_children - 1; i++) {
                parent->child_keys[i] = std::move(parent->child_keys[i + 1]);
                parent->children[i] = parent->children[i + 1];
            }
            if (steal_from_right) {
                // Move the child node from the right sibling to the last position.
                Key& key_to_move = parent_right_sibling->child_keys[0];
                parent->child_keys[parent->num_children - 1] = std::move(key_to_move);
                key_to_move.~Key();
                parent->children[parent->num_children - 1] = parent_right_sibling->children[0];
                parent->children[parent->num_children - 1]->parent = parent;
                // Move all child nodes in the right sibling to the left.
                for (u32 i = 0; i < (u32) parent_right_sibling->num_children - 1; i++) {
                    parent_right_sibling->child_keys[i] = std::move(parent_right_sibling->child_keys[i + 1]);
                    parent_right_sibling->children[i] = parent_right_sibling->children[i + 1];
                }
                // Decrement the number of items in the right sibling.
                parent_right_sibling->num_children--;
                parent_right_sibling->child_keys[parent_right_sibling->num_children].~Key();
                on_max_key_changed(parent);
                on_min_key_changed(parent_right_sibling);
            } else {
                // Decrement the number of child nodes in the parent.
                parent->num_children--;
                parent->child_keys[parent->num_children].~Key();
                if (erase_index == parent->num_children) {
                    on_max_key_changed(parent);
                }
            }
            if (erase_index == 0) {
                on_min_key_changed(parent);
            }

            if (parent->num_children == 1) {
                // This is the root node, and it has only one child. Promote the child to be the new root.
                PLY_ASSERT(this->root == parent);
                PLY_ASSERT(!parent->parent);
                parent->children[0]->parent = nullptr;
                this->root = parent->children[0];
                Heap::free(parent);
            }
        }

        // Unlink right sibling from its siblings.
        node->right_sibling = right_sibling->right_sibling;
        if (node->right_sibling) {
            node->right_sibling->left_sibling = node;
        }

        // Delete right sibling.
        Heap::free(right_sibling);
    }

public:
    //------------------------------------------------
    Iterator get_first_item() {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        while (!node->is_leaf) {
            InnerNode* inner_node = static_cast<InnerNode*>(node);
            PLY_ASSERT(inner_node->num_children > 0 && inner_node->num_children <= MaxItemsPerNode);
            node = inner_node->children[0];
        }
        LeafNode* leaf_node = static_cast<LeafNode*>(node);
        PLY_ASSERT(leaf_node->num_items > 0 && leaf_node->num_items <= MaxItemsPerNode);
        return {this, leaf_node, 0};
    }

    ConstIterator get_first_item() const {
        return (ConstIterator) const_cast<BTree*>(this)->get_first_item();
    }

    //------------------------------------------------
    Iterator get_last_item() {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        while (!node->is_leaf) {
            InnerNode* inner_node = static_cast<InnerNode*>(node);
            PLY_ASSERT(inner_node->num_children > 0 && inner_node->num_children <= MaxItemsPerNode);
            node = inner_node->children[inner_node->num_children - 1];
        }
        LeafNode* leaf_node = static_cast<LeafNode*>(node);
        PLY_ASSERT(leaf_node->num_items > 0 && leaf_node->num_items <= MaxItemsPerNode);
        return {this, leaf_node, leaf_node->num_items - 1u};
    }

    ConstIterator get_last_item() const {
        return (ConstIterator) const_cast<BTree*>(this)->get_last_item();
    }

    //------------------------------------------------
    // Returns the first item whose key is > the target key.
    // If the BTree items fall on range boundaries, this lets you find the range containing a particular key.
    PLY_NO_INLINE Iterator find_earliest(const Key& desired_key, FindType find_type) {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        if (!meets_condition(node->max_key, desired_key, find_type))
            return {this, nullptr, 0};

        // Iterate from the top to the bottom of the tree.
        while (!node->is_leaf) {
            InnerNode* inner_node = static_cast<InnerNode*>(node);
            PLY_ASSERT((inner_node->num_children > 0) && (inner_node->num_children <= MaxItemsPerNode));

            // Binary search this inner node.
            u32 found_item =
                binary_search(ArrayView<Key>{inner_node->child_keys, inner_node->num_children}, desired_key, find_type);

            // found_item identifies the first child node whose descendent items *all* meet the specified search
            // condition, which may not necessarily be the node we'll descend into. If the node preceding that one has a
            // max_key that also meets the search condition, that means *some* of its descendent items meet the search
            // condition, so we descend into that node instead. In both cases, we descend into the node containing the
            // first item that meets the condition.
            if (found_item == inner_node->num_children) {
                node = inner_node->children[found_item - 1];
            } else if ((found_item > 0) &&
                       meets_condition(inner_node->children[found_item - 1]->max_key, desired_key, find_type)) {
                node = inner_node->children[found_item - 1];
            } else {
                node = inner_node->children[found_item];
            }
        }

        // Binary search the items in this leaf node.
        // Items are stored with their keys in increasing order (with possible duplicate keys).
        LeafNode* leaf_node = static_cast<LeafNode*>(node);
        PLY_ASSERT((leaf_node->num_items > 0) && (leaf_node->num_items <= MaxItemsPerNode));
        u32 found_item = binary_search(ArrayView<Item>{leaf_node->items, leaf_node->num_items}, desired_key, find_type);
        // Item must have been found, because this->root->max_key promised it would be.
        PLY_ASSERT(found_item < leaf_node->num_items);
        PLY_ASSERT(meets_condition(get_lookup_key(leaf_node->items[found_item]), desired_key, find_type));
        return {this, leaf_node, found_item};
    }

    ConstIterator find_earliest(const Key& desired_key, FindType find_type) const {
        return const_cast<BTree*>(this)->find_earliest(desired_key, find_type);
    }

    bool find(const Key& desired_key) const {
        ConstIterator iter = this->find_earliest(desired_key, FindGreaterThanOrEqual);
        return (iter && get_lookup_key(*iter) == desired_key);
    }

    //------------------------------------------------
    void insert(const Item& item_to_insert) {
        Iterator insert_pos = this->find_earliest(get_lookup_key(item_to_insert), FindGreaterThan);
        this->insert_internal(&insert_pos, const_cast<Item&>(item_to_insert), false);
    }

    void insert(Item&& item_to_insert) {
        Iterator insert_pos = this->find_earliest(get_lookup_key(item_to_insert), FindGreaterThan);
        this->insert_internal(&insert_pos, item_to_insert, true);
    }

    void insert(Iterator* insert_pos, const Item& item_to_insert) {
        this->insert_internal(insert_pos, const_cast<Item&>(item_to_insert), false);
    }

    void insert(Iterator* insert_pos, Item&& item_to_insert) {
        this->insert_internal(insert_pos, item_to_insert, true);
    }

    //------------------------------------------------
    PLY_NO_INLINE void erase(Iterator erase_pos) {
        LeafNode* leaf_node = erase_pos.leaf_node;
        LeafNode* left_sibling = static_cast<LeafNode*>(leaf_node->left_sibling);
        LeafNode* right_sibling = static_cast<LeafNode*>(leaf_node->right_sibling);

        // If the leaf_node is only half full, select a strategy to avoid making it any smaller.
        bool steal_from_left = false;
        bool steal_from_right = false;
        if (leaf_node->num_items == MaxItemsPerNode / 2) {
            if (left_sibling && (left_sibling->num_items > MaxItemsPerNode / 2)) {
                // Steal an item from the left sibling.
                steal_from_left = true;
            } else if (right_sibling && (right_sibling->num_items > MaxItemsPerNode / 2)) {
                // Steal an item from the right sibling.
                steal_from_right = true;
            } else if (left_sibling) {
                // Update the erase position to point to the left sibling.
                erase_pos.item_index += left_sibling->num_items;
                erase_pos.leaf_node = left_sibling;
                leaf_node = left_sibling;
                // Merge the left sibling.
                merge_with_right_sibling(left_sibling);
            } else if (right_sibling) {
                // Merge with the right sibling. The erase position remains unchanged.
                merge_with_right_sibling(leaf_node);
            }
        }

        // Erase the desired item from the leaf node.
        if (steal_from_left) {
            // Move the items left of the erase position to the right.
            for (u32 i = erase_pos.item_index; i > 0; i--) {
                leaf_node->items[i] = std::move(leaf_node->items[i - 1]);
            }
            // Move the item from the left sibling to the first position.
            leaf_node->items[0] = std::move(left_sibling->items[left_sibling->num_items - 1]);
            // Destruct the item we stole.
            left_sibling->items[left_sibling->num_items - 1].~Item();
            // Decrement the number of items in the left sibling.
            left_sibling->num_items--;
            on_max_key_changed(left_sibling);
            on_min_key_changed(leaf_node);
            if (erase_pos.item_index == (u32) leaf_node->num_items - 1) {
                on_max_key_changed(leaf_node);
            }
        } else {
            // Move the items on the right of the erase position to the left.
            for (u32 i = erase_pos.item_index; i < (u32) leaf_node->num_items - 1; i++) {
                leaf_node->items[i] = std::move(leaf_node->items[i + 1]);
            }
            if (steal_from_right) {
                // Move the item from the right sibling to the last position.
                leaf_node->items[leaf_node->num_items - 1] = std::move(right_sibling->items[0]);
                // Move all items in the right sibling to the left.
                for (u32 i = 0; i < (u32) right_sibling->num_items - 1; i++) {
                    right_sibling->items[i] = std::move(right_sibling->items[i + 1]);
                }
                // Destruct the last item in the right sibling.
                right_sibling->items[right_sibling->num_items - 1].~Item();
                // Decrement the number of items in the right sibling.
                right_sibling->num_items--;
                on_max_key_changed(leaf_node);
                on_min_key_changed(right_sibling);
                if (erase_pos.item_index == 0) {
                    on_min_key_changed(leaf_node);
                }
            } else {
                // Destruct the item in the last position.
                leaf_node->items[leaf_node->num_items - 1].~Item();
                // Decrement the number of items in the leaf node.
                leaf_node->num_items--;

                if (leaf_node->num_items == 0) {
                    PLY_ASSERT(this->root == leaf_node);
                    PLY_ASSERT(leaf_node->parent == nullptr);
                    this->root = nullptr;
                    Heap::free(leaf_node);
                } else {
                    if (erase_pos.item_index == 0) {
                        on_min_key_changed(leaf_node);
                    }
                    if (erase_pos.item_index == leaf_node->num_items) {
                        on_max_key_changed(leaf_node);
                    }
                }
            }
        }
        // Decrement the number of items in the tree.
        this->num_items--;
    }

    bool erase(const Key& key_to_erase) {
        Iterator iter = this->find_earliest(key_to_erase, FindGreaterThanOrEqual);
        if (get_lookup_key(*iter) == key_to_erase) {
            this->erase(iter);
            return true;
        }
        return false;
    }

    //------------------------------------------------
    PLY_NO_INLINE void clear() {
        Node* first_node_in_row = this->root;
        while (!first_node_in_row->is_leaf) {
            InnerNode* inner_node = static_cast<InnerNode*>(first_node_in_row);
            first_node_in_row = inner_node->children[0];

            // Iterate over the inner nodes of this row from left to right.
            while (inner_node) {
                PLY_ASSERT((inner_node->num_children > 0) && (inner_node->num_children <= MaxItemsPerNode));
                for (u32 i = 0; i < inner_node->num_children; i++) {
                    inner_node->child_keys[i].~Key();
                }
                InnerNode* next = static_cast<InnerNode*>(inner_node->right_sibling);
                Heap::free(inner_node);
                inner_node = next;
            }
        }

        // Iterate over leaf nodes.
        LeafNode* leaf_node = static_cast<LeafNode*>(first_node_in_row);
        while (leaf_node) {
            PLY_ASSERT((leaf_node->num_items > 0) && (leaf_node->num_items <= MaxItemsPerNode));
            for (u32 i = 0; i < leaf_node->num_items; i++) {
                leaf_node->items[i].~Item();
            }
            LeafNode* next = static_cast<LeafNode*>(leaf_node->right_sibling);
            Heap::free(leaf_node);
            leaf_node = next;
        }

        this->root = nullptr;
        this->num_items = 0;
    }

    ~BTree() {
        this->clear();
    }

#if defined(PLY_WITH_ASSERTS)
    //------------------------------------------------
    PLY_NO_INLINE void validate() const {
        if (!this->root)
            return;

        // Iterate over the rows of the tree from top to bottom.
        Node* first_node_in_row = this->root;
        while (!first_node_in_row->is_leaf) {
            InnerNode* inner_node = static_cast<InnerNode*>(first_node_in_row);
            PLY_ASSERT(!inner_node->left_sibling);

            // Iterate over the inner nodes of this row from left to right.
            while (inner_node) {
                PLY_ASSERT(!inner_node->is_leaf);

                // Validate the number of children.
                PLY_ASSERT(inner_node->num_children > 0 && inner_node->num_children <= MaxItemsPerNode);
                if (inner_node->parent) {
                    // All nodes must be at least half full unless it's the root node.
                    PLY_ASSERT(inner_node->num_children >= MaxItemsPerNode / 2);
                }

                // Iterate over this node's children.
                for (u32 i = 0; i < inner_node->num_children; i++) {
                    // Validate the parent pointer.
                    PLY_ASSERT(inner_node->children[i]->parent == inner_node);
                    // Validate that the child keys are non-decreasing.
                    if (i > 0) {
                        PLY_ASSERT(inner_node->child_keys[i] >= inner_node->child_keys[i - 1]);
                        // Validate sibling links.
                        PLY_ASSERT(inner_node->children[i]->left_sibling == inner_node->children[i - 1]);
                        PLY_ASSERT(inner_node->children[i]->left_sibling->right_sibling == inner_node->children[i]);
                    } else if (inner_node->left_sibling) {
                        InnerNode* left_sibling = static_cast<InnerNode*>(inner_node->left_sibling);
                        PLY_ASSERT(!left_sibling->is_leaf);
                        PLY_ASSERT(inner_node->child_keys[0] >=
                                   left_sibling->child_keys[left_sibling->num_children - 1]);
                        // Validate sibling links.
                        PLY_ASSERT(inner_node->children[i]->left_sibling ==
                                   left_sibling->children[left_sibling->num_children - 1]);
                        PLY_ASSERT(inner_node->children[i]->left_sibling->right_sibling == inner_node->children[i]);
                    }
                    // Validate the child's max key.
                    if (i + 1 < inner_node->num_children) {
                        PLY_ASSERT(inner_node->children[i]->max_key <= inner_node->child_keys[i + 1]);
                    } else {
                        PLY_ASSERT(inner_node->children[i]->max_key <= inner_node->max_key);
                    }
                }

                inner_node = static_cast<InnerNode*>(inner_node->right_sibling);
            }

            first_node_in_row = static_cast<InnerNode*>(first_node_in_row)->children[0];
        }

        LeafNode* leaf_node = static_cast<LeafNode*>(first_node_in_row);
        PLY_ASSERT(!leaf_node->left_sibling);

        // Iterate over the leaf nodes of this row from left to right.
        while (leaf_node) {
            PLY_ASSERT(leaf_node->is_leaf);

            // Validate the number of items in the leaf node.
            PLY_ASSERT(leaf_node->num_items > 0 && leaf_node->num_items <= MaxItemsPerNode);
            if (leaf_node->parent) {
                // All nodes must be at least half full unless it's the root node.
                PLY_ASSERT(leaf_node->num_items >= MaxItemsPerNode / 2);
            }

            // Iterate over the items in the leaf node.
            for (u32 i = 0; i < leaf_node->num_items; i++) {
                // Validate that the items are in non-decreasing order.
                if (i > 0) {
                    PLY_ASSERT(get_lookup_key(leaf_node->items[i]) >= get_lookup_key(leaf_node->items[i - 1]));
                } else {
                    if (leaf_node->left_sibling) {
                        LeafNode* left_sibling = static_cast<LeafNode*>(leaf_node->left_sibling);
                        PLY_ASSERT(left_sibling->is_leaf);
                        PLY_ASSERT(get_lookup_key(leaf_node->items[i]) >=
                                   get_lookup_key(left_sibling->items[left_sibling->num_items - 1]));
                    }
                }
            }

            // Validate the leaf node's max key.
            PLY_ASSERT(leaf_node->max_key >= get_lookup_key(leaf_node->items[leaf_node->num_items - 1]));

            leaf_node = static_cast<LeafNode*>(leaf_node->right_sibling);
        }
    }
#endif
};

} // namespace ply
