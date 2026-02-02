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
        Node* leftSibling = nullptr;
        Node* rightSibling = nullptr;
        Key maxKey;
        bool isLeaf = true;
    };

    struct InnerNode : Node {
        u16 numChildren;
        Key childKeys[MaxItemsPerNode];
        Node* children[MaxItemsPerNode];

        const Key& getMinKey() const {
            PLY_ASSERT(this->numChildren > 0 && this->numChildren <= MaxItemsPerNode);
            return this->childKeys[0];
        }
        const Key& getInternalMaxKey() const {
            PLY_ASSERT(this->numChildren > 0 && this->numChildren <= MaxItemsPerNode);
            return this->children[this->numChildren - 1]->maxKey;
        }
    };

    struct LeafNode : Node {
        u16 numItems;
        Item items[MaxItemsPerNode];

        auto getMinKey() const {
            PLY_ASSERT(this->numItems > 0 && this->numItems <= MaxItemsPerNode);
            return getAnyLookupKey(this->items[0]);
        }
        auto getInternalMaxKey() const {
            PLY_ASSERT(this->numItems > 0 && this->numItems <= MaxItemsPerNode);
            return getAnyLookupKey(this->items[this->numItems - 1]);
        }
    };

    Node* root = nullptr;
    u32 numItems = 0;

    struct Iterator {
        BTree* btree = nullptr;
        LeafNode* leafNode = nullptr;
        u32 itemIndex = 0;

        operator bool() const {
            return this->leafNode;
        }
        Item& operator*() const {
            PLY_ASSERT(this->leafNode);
            return this->leafNode->items[this->itemIndex];
        }
        Item* operator->() const {
            PLY_ASSERT(this->leafNode);
            return &this->leafNode->items[this->itemIndex];
        }
        void operator++(int) {
            this->itemIndex++;
            if (this->itemIndex >= this->leafNode->numItems) {
                this->leafNode = static_cast<LeafNode*>(this->leafNode->rightSibling);
                PLY_ASSERT(!this->leafNode || this->leafNode->isLeaf);
                this->itemIndex = 0;
            }
        }
        void operator++() {
            (*this)++;
        }
        void operator--(int) {
            this->itemIndex--;
            // Test for wrap-around.
            if (this->itemIndex > MaxItemsPerNode) {
                if (!this->leafNode) {
                    *this = this->btree->getLastItem();
                } else {
                    this->leafNode = static_cast<LeafNode*>(this->leafNode->leftSibling);
                    if (this->leafNode) {
                        PLY_ASSERT(leafNode->isLeaf);
                        PLY_ASSERT((leafNode->numItems >= MaxItemsPerNode / 2) &&
                                   (leafNode->numItems <= MaxItemsPerNode));
                        this->itemIndex = this->leafNode->numItems - 1;
                    } else {
                        this->itemIndex = 0;
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
            PLY_ASSERT(this->leafNode);
            return this->leafNode->items[this->itemIndex];
        }
        const Item* operator->() const {
            PLY_ASSERT(this->leafNode);
            return &this->leafNode->items[this->itemIndex];
        }
    };

private:
    //------------------------------------------------
    PLY_NO_INLINE static void onMinKeyChanged(Node* node) {
        PLY_ASSERT(node);
        if (node->parent) {
            // Find the index of this node within its parent.
            PLY_ASSERT(node->parent->numChildren > 0);
            u32 indexInParent = 0;
            for (; indexInParent < node->parent->numChildren; indexInParent++) {
                if (node->parent->children[indexInParent] == node)
                    break;
            }
            PLY_ASSERT(indexInParent < node->parent->numChildren); // Must be found.

            // Update the corresponding child key.
            if (node->isLeaf) {
                node->parent->childKeys[indexInParent] = static_cast<LeafNode*>(node)->getMinKey();
            } else {
                node->parent->childKeys[indexInParent] = static_cast<InnerNode*>(node)->getMinKey();
            }

            if (indexInParent == 0) {
                onMinKeyChanged(node->parent);
            }
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE static void onMaxKeyChanged(Node* node) {
        PLY_ASSERT(node);
        if (node->isLeaf) {
            node->maxKey = static_cast<LeafNode*>(node)->getInternalMaxKey();
        } else {
            node->maxKey = static_cast<InnerNode*>(node)->getInternalMaxKey();
        }
        if (node->parent) {
            PLY_ASSERT(node->parent->numChildren > 0);
            if (node->parent->children[node->parent->numChildren - 1] == node) {
                onMaxKeyChanged(node->parent);
            }
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE void insertRightSibling(Node* existingNode, Node* nodeToInsert) {
        // Add nodeToInsert to the linked list of siblings, just to the right of existingNode.
        nodeToInsert->rightSibling = existingNode->rightSibling;
        if (nodeToInsert->rightSibling) {
            nodeToInsert->rightSibling->leftSibling = nodeToInsert;
        }
        nodeToInsert->leftSibling = existingNode;
        existingNode->rightSibling = nodeToInsert;

        // Locate existingNode inside its parent.
        InnerNode* existingParent = existingNode->parent;
        u32 insertIndex = 0;
        if (existingParent) {
            for (; insertIndex < existingParent->numChildren; insertIndex++) {
                if (existingParent->children[insertIndex] == existingNode)
                    break;
            }
            // existingNode must have been found.
            PLY_ASSERT(insertIndex < existingParent->numChildren);
            // Increment because we are inserting to the right.
            insertIndex++;
        } else {
            // There's no parent for this node, which means it's currently the root.
            // Create new root node and make this node its only child. nodeToInsert will
            // be inserted to its right.
            PLY_ASSERT(this->root == existingNode);
            InnerNode* newRoot = (InnerNode*) Heap::alloc(sizeof(InnerNode));
            new (newRoot) Node; // Construct base class members only
            newRoot->isLeaf = false;
            newRoot->numChildren = 1;
            if (existingNode->isLeaf) {
                new (&newRoot->childKeys[0]) Key{getAnyLookupKey(static_cast<LeafNode*>(existingNode)->items[0])};
            } else {
                new (&newRoot->childKeys[0]) Key{static_cast<InnerNode*>(existingNode)->childKeys[0]};
            }
            newRoot->children[0] = existingNode;
            newRoot->maxKey = newRoot->childKeys[0];
            existingNode->parent = newRoot;
            this->root = newRoot;
            existingParent = newRoot;
            insertIndex = 1;
        }

        // Set nodeToInsert's parent optimistically.
        nodeToInsert->parent = existingParent;

        // If the parent is full, split it in two before inserting.
        InnerNode* splitParent = nullptr;
        if (existingParent->numChildren == MaxItemsPerNode) {
            // Split parent into two nodes. splitParent will be the new sibling to its right.
            splitParent = (InnerNode*) Heap::alloc(sizeof(InnerNode));
            new (splitParent) Node; // Construct base class members only.
            splitParent->isLeaf = false;
            // Move half of parent's items to splitParent.
            splitParent->numChildren = existingParent->numChildren / 2;
            existingParent->numChildren -= splitParent->numChildren;
            for (u32 i = 0; i < splitParent->numChildren; i++) {
                Key& keyToMove = existingParent->childKeys[existingParent->numChildren + i];
                new (&splitParent->childKeys[i]) Key{std::move(keyToMove)};
                keyToMove.~Key();
                splitParent->children[i] = existingParent->children[existingParent->numChildren + i];
                splitParent->children[i]->parent = splitParent;
            }

            // If out of range...
            if (insertIndex > existingParent->numChildren) {
                nodeToInsert->parent = splitParent;
                insertIndex -= existingParent->numChildren;
            }

            // Update max keys.
            splitParent->maxKey = std::move(existingParent->maxKey);
            onMaxKeyChanged(existingParent);
        }

        // Insert nodeToInsert into its parent at the prescribed index.
        if (insertIndex == nodeToInsert->parent->numChildren) {
            if (nodeToInsert->isLeaf) {
                new (&nodeToInsert->parent->childKeys[insertIndex])
                    Key{static_cast<LeafNode*>(nodeToInsert)->getMinKey()};
            } else {
                new (&nodeToInsert->parent->childKeys[insertIndex])
                    Key{static_cast<InnerNode*>(nodeToInsert)->getMinKey()};
            }
        } else {
            u32 i = nodeToInsert->parent->numChildren;
            new (&nodeToInsert->parent->childKeys[i]) Key{std::move(nodeToInsert->parent->childKeys[i - 1])};
            nodeToInsert->parent->children[i] = nodeToInsert->parent->children[i - 1];
            for (; i > insertIndex; i--) {
                nodeToInsert->parent->childKeys[i] = std::move(nodeToInsert->parent->childKeys[i - 1]);
                nodeToInsert->parent->children[i] = nodeToInsert->parent->children[i - 1];
            }
            if (nodeToInsert->isLeaf) {
                nodeToInsert->parent->childKeys[insertIndex] = static_cast<LeafNode*>(nodeToInsert)->getMinKey();
            } else {
                nodeToInsert->parent->childKeys[insertIndex] = static_cast<InnerNode*>(nodeToInsert)->getMinKey();
            }
        }
        nodeToInsert->parent->children[insertIndex] = nodeToInsert;
        nodeToInsert->parent->numChildren++;
        PLY_ASSERT(nodeToInsert->parent->numChildren <= MaxItemsPerNode);
        if (insertIndex == 0) {
            onMinKeyChanged(nodeToInsert->parent);
        }
        if (insertIndex == (u32) nodeToInsert->parent->numChildren - 1) {
            onMaxKeyChanged(nodeToInsert->parent);
        }

        // If the parent was split, insert splitParent into parent's parent.
        if (splitParent) {
            this->insertRightSibling(existingParent, splitParent);
        }
    }

    //------------------------------------------------
    PLY_NO_INLINE void insertInternal(Iterator* insertPos, Item& itemToInsert, bool withMoveSemantics) {
        if (!insertPos->leafNode) {
            // A null iterator means insert at the end of the list.
            if (!this->root) {
                // It's an empty tree. Create a new Leaf_Node and set it as root.
                insertPos->leafNode = (LeafNode*) Heap::alloc(sizeof(LeafNode));
                insertPos->itemIndex = 0;
                // Construct base class members only (no Items are constructed).
                new (insertPos->leafNode) Node;
                insertPos->leafNode->numItems = 0;
                this->root = insertPos->leafNode;
            } else {
                // Find the actual insert position.
                *insertPos = this->getLastItem();
                insertPos->itemIndex++;
            }
        }

        // If the leaf node is full, split it in two before inserting.
        if (insertPos->leafNode->numItems == MaxItemsPerNode) {
            LeafNode* leafNode = insertPos->leafNode;
            u32 N = leafNode->numItems;

            // Split this leaf node in two. splitNode will be the new sibling to its right.
            LeafNode* splitNode = (LeafNode*) Heap::alloc(sizeof(LeafNode));
            // Construct base class members only (no Items are constructed).
            new (splitNode) Node;
            splitNode->parent = leafNode->parent;
            // Move half leafNode's items to splitNode.
            splitNode->numItems = N / 2;
            N -= splitNode->numItems;
            for (u32 i = 0; i < splitNode->numItems; i++) {
                Item* srcItem = &leafNode->items[N + i];
                new (&splitNode->items[i]) Item{std::move(*srcItem)}; // Move to destination.
                srcItem->~Item();                                     // Destruct source item.
            }
            insertPos->leafNode->numItems = N;

            // If the input Iterator no longer refers to a valid Item, modify it to point to the correct Item in
            // splitNode.
            if (insertPos->itemIndex >= N) {
                insertPos->leafNode = splitNode;
                insertPos->itemIndex -= N;
            }

            // Update max keys.
            splitNode->maxKey = leafNode->maxKey;
            onMaxKeyChanged(leafNode);

            this->insertRightSibling(leafNode, splitNode);
        }

        // Insert into the leaf node.
        LeafNode* leafNode = insertPos->leafNode;
        u32 N = leafNode->numItems;
        PLY_ASSERT(N < MaxItemsPerNode);
        PLY_ASSERT(insertPos->itemIndex <= N);
        leafNode->numItems++;
        if (insertPos->itemIndex == N) {
            // It's the last item in the leaf node. Move-construct it directly here.
            if (withMoveSemantics) {
                new (&leafNode->items[N]) Item{std::move(itemToInsert)};
            } else {
                new (&leafNode->items[N]) Item{static_cast<const Item&>(itemToInsert)};
            }
            onMaxKeyChanged(leafNode);
        } else {
            // It's not the last item in the leaf node. Move-construct the last item to the right of the insert
            // position.
            new (&leafNode->items[N]) Item{std::move(leafNode->items[N - 1])};
            // Move the items on the right of the insert position to the right.
            for (u32 i = N - 1; i > insertPos->itemIndex; i--) {
                leafNode->items[i] = std::move(leafNode->items[i - 1]);
            }
            // Move the new item to the insert position.
            if (withMoveSemantics) {
                leafNode->items[insertPos->itemIndex] = std::move(itemToInsert);
            } else {
                leafNode->items[insertPos->itemIndex] = static_cast<const Item&>(itemToInsert);
            }
        }

        if (insertPos->itemIndex == 0) {
            onMinKeyChanged(leafNode);
        }

        // Increment the number of items in the tree.
        this->numItems++;

#if defined(PLY_WITH_ASSERTS)
        // Validate non-decreasing order to the left of the insert position.
        if (insertPos->itemIndex > 0) {
            PLY_ASSERT(getAnyLookupKey(leafNode->items[insertPos->itemIndex - 1]) <=
                       getAnyLookupKey(leafNode->items[insertPos->itemIndex]));
        } else if (leafNode->leftSibling) {
            LeafNode* leftSibling = static_cast<LeafNode*>(leafNode->leftSibling);
            PLY_ASSERT(leftSibling->isLeaf);
            PLY_ASSERT(getAnyLookupKey(leftSibling->items[leftSibling->numItems - 1]) <=
                       getAnyLookupKey(leafNode->items[0]));
        }
        // Validate non-decreasing order to the right of the insert position.
        if (insertPos->itemIndex + 1 < leafNode->numItems) {
            PLY_ASSERT(getAnyLookupKey(leafNode->items[insertPos->itemIndex]) <=
                       getAnyLookupKey(leafNode->items[insertPos->itemIndex + 1]));
        } else if (leafNode->rightSibling) {
            LeafNode* rightSibling = static_cast<LeafNode*>(leafNode->rightSibling);
            PLY_ASSERT(rightSibling->isLeaf);
            PLY_ASSERT(getAnyLookupKey(leafNode->items[insertPos->itemIndex]) >=
                       getAnyLookupKey(rightSibling->items[0]));
        }
#endif
    }

    //------------------------------------------------
    PLY_NO_INLINE void mergeWithRightSibling(Node* node) {
        PLY_ASSERT(node);
        PLY_ASSERT(node->rightSibling);

        if (node->isLeaf) {
            LeafNode* leafNode = static_cast<LeafNode*>(node);
            LeafNode* rightSibling = static_cast<LeafNode*>(node->rightSibling);
            u32 N = leafNode->numItems;
            for (u32 i = 0; i < rightSibling->numItems; i++) {
                new (&leafNode->items[N + i]) Item{std::move(rightSibling->items[i])};
                rightSibling->items[i].~Item();
            }
            leafNode->numItems += rightSibling->numItems;
        } else {
            InnerNode* innerNode = static_cast<InnerNode*>(node);
            InnerNode* rightSibling = static_cast<InnerNode*>(node->rightSibling);
            u32 N = innerNode->numChildren;
            for (u32 i = 0; i < rightSibling->numChildren; i++) {
                Key& keyToMove = rightSibling->childKeys[i];
                innerNode->childKeys[N + i] = std::move(keyToMove);
                keyToMove.~Key();
                innerNode->children[N + i] = rightSibling->children[i];
                innerNode->children[N + i]->parent = innerNode;
            }
            innerNode->numChildren += rightSibling->numChildren;
        }
        onMaxKeyChanged(node);

        // We want to erase the right sibling from its parent.
        InnerNode* parent = node->rightSibling->parent;
        InnerNode* parentLeftSibling = static_cast<InnerNode*>(parent->leftSibling);
        InnerNode* parentRightSibling = static_cast<InnerNode*>(parent->rightSibling);

        // But first, if the parent is only half full, select a strategy to avoid making it any smaller.
        bool stealFromLeft = false;
        bool stealFromRight = false;
        if (parent->numChildren == MaxItemsPerNode / 2) {
            if (parentLeftSibling && (parentLeftSibling->numChildren > MaxItemsPerNode / 2)) {
                // Steal a node from the left sibling.
                stealFromLeft = true;
            } else if (parentRightSibling && (parentRightSibling->numChildren > MaxItemsPerNode / 2)) {
                // Steal a node from the right sibling.
                stealFromRight = true;
            } else if (parentLeftSibling) {
                // Merge the left sibling before deleting.
                parent = parentLeftSibling;
                mergeWithRightSibling(parent);
            } else if (parentRightSibling) {
                // Merge with the right sibling before deleting.
                mergeWithRightSibling(parent);
            }
        }

        // Locate the right sibling within its parent.
        Node* rightSibling = node->rightSibling;
        u32 eraseIndex = 0;
        for (; eraseIndex < parent->numChildren; eraseIndex++) {
            if (parent->children[eraseIndex] == rightSibling)
                break;
        }
        // The right sibling must have been found.
        PLY_ASSERT(eraseIndex < parent->numChildren);

        // Erase the right sibling from its parent.
        if (stealFromLeft) {
            // Move the child nodes left of the erase position to the right.
            for (u32 i = eraseIndex; i > 0; i--) {
                parent->childKeys[i] = std::move(parent->childKeys[i - 1]);
                parent->children[i] = parent->children[i - 1];
            }
            // Move the child node from the left sibling to the first position.
            Key& keyToMove = parentLeftSibling->childKeys[parentLeftSibling->numChildren - 1];
            parent->childKeys[0] = std::move(keyToMove);
            keyToMove.~Key();
            parent->children[0] = parentLeftSibling->children[parentLeftSibling->numChildren - 1];
            parent->children[0]->parent = parent;
            // Decrement the number of items in the left sibling.
            parentLeftSibling->numChildren--;
            onMaxKeyChanged(parentLeftSibling);
            onMinKeyChanged(parent);
            if (eraseIndex == (u32) parent->numChildren - 1) {
                onMaxKeyChanged(parent);
            }
        } else {
            // Move the child nodes on the right of the erase position to the left.
            for (u32 i = eraseIndex; i < (u32) parent->numChildren - 1; i++) {
                parent->childKeys[i] = std::move(parent->childKeys[i + 1]);
                parent->children[i] = parent->children[i + 1];
            }
            if (stealFromRight) {
                // Move the child node from the right sibling to the last position.
                Key& keyToMove = parentRightSibling->childKeys[0];
                parent->childKeys[parent->numChildren - 1] = std::move(keyToMove);
                keyToMove.~Key();
                parent->children[parent->numChildren - 1] = parentRightSibling->children[0];
                parent->children[parent->numChildren - 1]->parent = parent;
                // Move all child nodes in the right sibling to the left.
                for (u32 i = 0; i < (u32) parentRightSibling->numChildren - 1; i++) {
                    parentRightSibling->childKeys[i] = std::move(parentRightSibling->childKeys[i + 1]);
                    parentRightSibling->children[i] = parentRightSibling->children[i + 1];
                }
                // Decrement the number of items in the right sibling.
                parentRightSibling->numChildren--;
                parentRightSibling->childKeys[parentRightSibling->numChildren].~Key();
                onMaxKeyChanged(parent);
                onMinKeyChanged(parentRightSibling);
            } else {
                // Decrement the number of child nodes in the parent.
                parent->numChildren--;
                parent->childKeys[parent->numChildren].~Key();
                if (eraseIndex == parent->numChildren) {
                    onMaxKeyChanged(parent);
                }
            }
            if (eraseIndex == 0) {
                onMinKeyChanged(parent);
            }

            if (parent->numChildren == 1) {
                // This is the root node, and it has only one child. Promote the child to be the new root.
                PLY_ASSERT(this->root == parent);
                PLY_ASSERT(!parent->parent);
                parent->children[0]->parent = nullptr;
                this->root = parent->children[0];
                Heap::free(parent);
            }
        }

        // Unlink right sibling from its siblings.
        node->rightSibling = rightSibling->rightSibling;
        if (node->rightSibling) {
            node->rightSibling->leftSibling = node;
        }

        // Delete right sibling.
        Heap::free(rightSibling);
    }

public:
    //------------------------------------------------
    Iterator getFirstItem() {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        while (!node->isLeaf) {
            InnerNode* innerNode = static_cast<InnerNode*>(node);
            PLY_ASSERT(innerNode->numChildren > 0 && innerNode->numChildren <= MaxItemsPerNode);
            node = innerNode->children[0];
        }
        LeafNode* leafNode = static_cast<LeafNode*>(node);
        PLY_ASSERT(leafNode->numItems > 0 && leafNode->numItems <= MaxItemsPerNode);
        return {this, leafNode, 0};
    }

    ConstIterator getFirstItem() const {
        return (ConstIterator) const_cast<BTree*>(this)->getFirstItem();
    }

    //------------------------------------------------
    Iterator getLastItem() {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        while (!node->isLeaf) {
            InnerNode* innerNode = static_cast<InnerNode*>(node);
            PLY_ASSERT(innerNode->numChildren > 0 && innerNode->numChildren <= MaxItemsPerNode);
            node = innerNode->children[innerNode->numChildren - 1];
        }
        LeafNode* leafNode = static_cast<LeafNode*>(node);
        PLY_ASSERT(leafNode->numItems > 0 && leafNode->numItems <= MaxItemsPerNode);
        return {this, leafNode, leafNode->numItems - 1u};
    }

    ConstIterator getLastItem() const {
        return (ConstIterator) const_cast<BTree*>(this)->getLastItem();
    }

    //------------------------------------------------
    // Returns the first item whose key is > the target key.
    // If the BTree items fall on range boundaries, this lets you find the range containing a particular key.
    PLY_NO_INLINE Iterator findEarliest(const Key& desiredKey, FindType findType) {
        Node* node = this->root;
        if (!node)
            return {this, nullptr, 0};
        if (!meetsCondition(node->maxKey, desiredKey, findType))
            return {this, nullptr, 0};

        // Iterate from the top to the bottom of the tree.
        while (!node->isLeaf) {
            InnerNode* innerNode = static_cast<InnerNode*>(node);
            PLY_ASSERT((innerNode->numChildren > 0) && (innerNode->numChildren <= MaxItemsPerNode));

            // Binary search this inner node.
            u32 foundItem =
                binarySearch(ArrayView<Key>{innerNode->childKeys, innerNode->numChildren}, desiredKey, findType);

            // foundItem identifies the first child node whose descendent items *all* meet the specified search
            // condition, which may not necessarily be the node we'll descend into. If the node preceding that one has a
            // maxKey that also meets the search condition, that means *some* of its descendent items meet the search
            // condition, so we descend into that node instead. In both cases, we descend into the node containing the
            // first item that meets the condition.
            if (foundItem == innerNode->numChildren) {
                node = innerNode->children[foundItem - 1];
            } else if ((foundItem > 0) &&
                       meetsCondition(innerNode->children[foundItem - 1]->maxKey, desiredKey, findType)) {
                node = innerNode->children[foundItem - 1];
            } else {
                node = innerNode->children[foundItem];
            }
        }

        // Binary search the items in this leaf node.
        // Items are stored with their keys in increasing order (with possible duplicate keys).
        LeafNode* leafNode = static_cast<LeafNode*>(node);
        PLY_ASSERT((leafNode->numItems > 0) && (leafNode->numItems <= MaxItemsPerNode));
        u32 foundItem = binarySearch(ArrayView<Item>{leafNode->items, leafNode->numItems}, desiredKey, findType);
        // Item must have been found, because this->root->maxKey promised it would be.
        PLY_ASSERT(foundItem < leafNode->numItems);
        PLY_ASSERT(meetsCondition(getAnyLookupKey(leafNode->items[foundItem]), desiredKey, findType));
        return {this, leafNode, foundItem};
    }

    ConstIterator findEarliest(const Key& desiredKey, FindType findType) const {
        return const_cast<BTree*>(this)->findEarliest(desiredKey, findType);
    }

    bool find(const Key& desiredKey) const {
        ConstIterator iter = this->findEarliest(desiredKey, FindGreaterThanOrEqual);
        return (iter && getAnyLookupKey(*iter) == desiredKey);
    }

    //------------------------------------------------
    void insert(const Item& itemToInsert) {
        Iterator insertPos = this->findEarliest(getAnyLookupKey(itemToInsert), FindGreaterThan);
        this->insertInternal(&insertPos, const_cast<Item&>(itemToInsert), false);
    }

    void insert(Item&& itemToInsert) {
        Iterator insertPos = this->findEarliest(getAnyLookupKey(itemToInsert), FindGreaterThan);
        this->insertInternal(&insertPos, itemToInsert, true);
    }

    void insert(Iterator* insertPos, const Item& itemToInsert) {
        this->insertInternal(insertPos, const_cast<Item&>(itemToInsert), false);
    }

    void insert(Iterator* insertPos, Item&& itemToInsert) {
        this->insertInternal(insertPos, itemToInsert, true);
    }

    //------------------------------------------------
    PLY_NO_INLINE void erase(Iterator erasePos) {
        LeafNode* leafNode = erasePos.leafNode;
        LeafNode* leftSibling = static_cast<LeafNode*>(leafNode->leftSibling);
        LeafNode* rightSibling = static_cast<LeafNode*>(leafNode->rightSibling);

        // If the leafNode is only half full, select a strategy to avoid making it any smaller.
        bool stealFromLeft = false;
        bool stealFromRight = false;
        if (leafNode->numItems == MaxItemsPerNode / 2) {
            if (leftSibling && (leftSibling->numItems > MaxItemsPerNode / 2)) {
                // Steal an item from the left sibling.
                stealFromLeft = true;
            } else if (rightSibling && (rightSibling->numItems > MaxItemsPerNode / 2)) {
                // Steal an item from the right sibling.
                stealFromRight = true;
            } else if (leftSibling) {
                // Update the erase position to point to the left sibling.
                erasePos.itemIndex += leftSibling->numItems;
                erasePos.leafNode = leftSibling;
                leafNode = leftSibling;
                // Merge the left sibling.
                mergeWithRightSibling(leftSibling);
            } else if (rightSibling) {
                // Merge with the right sibling. The erase position remains unchanged.
                mergeWithRightSibling(leafNode);
            }
        }

        // Erase the desired item from the leaf node.
        if (stealFromLeft) {
            // Move the items left of the erase position to the right.
            for (u32 i = erasePos.itemIndex; i > 0; i--) {
                leafNode->items[i] = std::move(leafNode->items[i - 1]);
            }
            // Move the item from the left sibling to the first position.
            leafNode->items[0] = std::move(leftSibling->items[leftSibling->numItems - 1]);
            // Destruct the item we stole.
            leftSibling->items[leftSibling->numItems - 1].~Item();
            // Decrement the number of items in the left sibling.
            leftSibling->numItems--;
            onMaxKeyChanged(leftSibling);
            onMinKeyChanged(leafNode);
            if (erasePos.itemIndex == (u32) leafNode->numItems - 1) {
                onMaxKeyChanged(leafNode);
            }
        } else {
            // Move the items on the right of the erase position to the left.
            for (u32 i = erasePos.itemIndex; i < (u32) leafNode->numItems - 1; i++) {
                leafNode->items[i] = std::move(leafNode->items[i + 1]);
            }
            if (stealFromRight) {
                // Move the item from the right sibling to the last position.
                leafNode->items[leafNode->numItems - 1] = std::move(rightSibling->items[0]);
                // Move all items in the right sibling to the left.
                for (u32 i = 0; i < (u32) rightSibling->numItems - 1; i++) {
                    rightSibling->items[i] = std::move(rightSibling->items[i + 1]);
                }
                // Destruct the last item in the right sibling.
                rightSibling->items[rightSibling->numItems - 1].~Item();
                // Decrement the number of items in the right sibling.
                rightSibling->numItems--;
                onMaxKeyChanged(leafNode);
                onMinKeyChanged(rightSibling);
                if (erasePos.itemIndex == 0) {
                    onMinKeyChanged(leafNode);
                }
            } else {
                // Destruct the item in the last position.
                leafNode->items[leafNode->numItems - 1].~Item();
                // Decrement the number of items in the leaf node.
                leafNode->numItems--;

                if (leafNode->numItems == 0) {
                    PLY_ASSERT(this->root == leafNode);
                    PLY_ASSERT(leafNode->parent == nullptr);
                    this->root = nullptr;
                    Heap::free(leafNode);
                } else {
                    if (erasePos.itemIndex == 0) {
                        onMinKeyChanged(leafNode);
                    }
                    if (erasePos.itemIndex == leafNode->numItems) {
                        onMaxKeyChanged(leafNode);
                    }
                }
            }
        }
        // Decrement the number of items in the tree.
        this->numItems--;
    }

    bool erase(const Key& keyToErase) {
        Iterator iter = this->findEarliest(keyToErase, FindGreaterThanOrEqual);
        if (getAnyLookupKey(*iter) == keyToErase) {
            this->erase(iter);
            return true;
        }
        return false;
    }

    //------------------------------------------------
    PLY_NO_INLINE void clear() {
        Node* firstNodeInRow = this->root;
        while (!firstNodeInRow->isLeaf) {
            InnerNode* innerNode = static_cast<InnerNode*>(firstNodeInRow);
            firstNodeInRow = innerNode->children[0];

            // Iterate over the inner nodes of this row from left to right.
            while (innerNode) {
                PLY_ASSERT((innerNode->numChildren > 0) && (innerNode->numChildren <= MaxItemsPerNode));
                for (u32 i = 0; i < innerNode->numChildren; i++) {
                    innerNode->childKeys[i].~Key();
                }
                InnerNode* next = static_cast<InnerNode*>(innerNode->rightSibling);
                Heap::free(innerNode);
                innerNode = next;
            }
        }

        // Iterate over leaf nodes.
        LeafNode* leafNode = static_cast<LeafNode*>(firstNodeInRow);
        while (leafNode) {
            PLY_ASSERT((leafNode->numItems > 0) && (leafNode->numItems <= MaxItemsPerNode));
            for (u32 i = 0; i < leafNode->numItems; i++) {
                leafNode->items[i].~Item();
            }
            LeafNode* next = static_cast<LeafNode*>(leafNode->rightSibling);
            Heap::free(leafNode);
            leafNode = next;
        }

        this->root = nullptr;
        this->numItems = 0;
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
        Node* firstNodeInRow = this->root;
        while (!firstNodeInRow->isLeaf) {
            InnerNode* innerNode = static_cast<InnerNode*>(firstNodeInRow);
            PLY_ASSERT(!innerNode->leftSibling);

            // Iterate over the inner nodes of this row from left to right.
            while (innerNode) {
                PLY_ASSERT(!innerNode->isLeaf);

                // Validate the number of children.
                PLY_ASSERT(innerNode->numChildren > 0 && innerNode->numChildren <= MaxItemsPerNode);
                if (innerNode->parent) {
                    // All nodes must be at least half full unless it's the root node.
                    PLY_ASSERT(innerNode->numChildren >= MaxItemsPerNode / 2);
                }

                // Iterate over this node's children.
                for (u32 i = 0; i < innerNode->numChildren; i++) {
                    // Validate the parent pointer.
                    PLY_ASSERT(innerNode->children[i]->parent == innerNode);
                    // Validate that the child keys are non-decreasing.
                    if (i > 0) {
                        PLY_ASSERT(innerNode->childKeys[i] >= innerNode->childKeys[i - 1]);
                        // Validate sibling links.
                        PLY_ASSERT(innerNode->children[i]->leftSibling == innerNode->children[i - 1]);
                        PLY_ASSERT(innerNode->children[i]->leftSibling->rightSibling == innerNode->children[i]);
                    } else if (innerNode->leftSibling) {
                        InnerNode* leftSibling = static_cast<InnerNode*>(innerNode->leftSibling);
                        PLY_ASSERT(!leftSibling->isLeaf);
                        PLY_ASSERT(innerNode->childKeys[0] >= leftSibling->childKeys[leftSibling->numChildren - 1]);
                        // Validate sibling links.
                        PLY_ASSERT(innerNode->children[i]->leftSibling ==
                                   leftSibling->children[leftSibling->numChildren - 1]);
                        PLY_ASSERT(innerNode->children[i]->leftSibling->rightSibling == innerNode->children[i]);
                    }
                    // Validate the child's max key.
                    if (i + 1 < innerNode->numChildren) {
                        PLY_ASSERT(innerNode->children[i]->maxKey <= innerNode->childKeys[i + 1]);
                    } else {
                        PLY_ASSERT(innerNode->children[i]->maxKey <= innerNode->maxKey);
                    }
                }

                innerNode = static_cast<InnerNode*>(innerNode->rightSibling);
            }

            firstNodeInRow = static_cast<InnerNode*>(firstNodeInRow)->children[0];
        }

        LeafNode* leafNode = static_cast<LeafNode*>(firstNodeInRow);
        PLY_ASSERT(!leafNode->leftSibling);

        // Iterate over the leaf nodes of this row from left to right.
        while (leafNode) {
            PLY_ASSERT(leafNode->isLeaf);

            // Validate the number of items in the leaf node.
            PLY_ASSERT(leafNode->numItems > 0 && leafNode->numItems <= MaxItemsPerNode);
            if (leafNode->parent) {
                // All nodes must be at least half full unless it's the root node.
                PLY_ASSERT(leafNode->numItems >= MaxItemsPerNode / 2);
            }

            // Iterate over the items in the leaf node.
            for (u32 i = 0; i < leafNode->numItems; i++) {
                // Validate that the items are in non-decreasing order.
                if (i > 0) {
                    PLY_ASSERT(getAnyLookupKey(leafNode->items[i]) >= getAnyLookupKey(leafNode->items[i - 1]));
                } else {
                    if (leafNode->leftSibling) {
                        LeafNode* leftSibling = static_cast<LeafNode*>(leafNode->leftSibling);
                        PLY_ASSERT(leftSibling->isLeaf);
                        PLY_ASSERT(getAnyLookupKey(leafNode->items[i]) >=
                                   getAnyLookupKey(leftSibling->items[leftSibling->numItems - 1]));
                    }
                }
            }

            // Validate the leaf node's max key.
            PLY_ASSERT(leafNode->maxKey >= getAnyLookupKey(leafNode->items[leafNode->numItems - 1]));

            leafNode = static_cast<LeafNode*>(leafNode->rightSibling);
        }
    }
#endif
};

} // namespace ply
