/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#pragma once
#include "ply-base.h"

namespace ply {
namespace markdown {

// The parser generates a sequence of `ply::markdown::Node` objects as output.
// Each `Node` describes a top-level Markdown block (such as a paragraph, code
// block or list) and all its child elements.

struct Node {
    enum Type {
        // These types of nodes can have child blocks:
        None = 0,
        List,
        ListItem,
        BlockQuote,

        // These types of nodes are leaves and can only contain text:
        StartLeafNodeType,
        Heading = StartLeafNodeType,
        Paragraph,
        CodeBlock,

        // These types of nodes are inline markers used inside text:
        StartInlineNodeType,
        Text = StartInlineNodeType,
        Link,
        CodeSpan,
        SoftBreak,
        Emphasis,
        Strong,
    };

    Type type = None;
    u32 indent_or_level = 0;            // only used by List_Items & Headings
    s32 list_start_number = 0;          // only used by Lists. -1 means unordered
    bool is_loose_if_continued = false; // only used by Lists
    bool is_loose = false;              // only used by Lists
    char list_punc = '-';               // only used by Lists
    Array<Owned<Node>> children;
    Node* parent = nullptr;
    Array<String> raw_lines; // only used by Leaf nodes (Heading, Paragraph, Code_Block)
    String text;             // only used by Text, Code or Link (for the destination)
    String id;               // sets the id attribute for Headings

    Node(Node* parent, Type type) : type{type}, parent{parent} {
        if (parent) {
            parent->children.append(this);
        }
    }

    void add_children(ArrayView<Owned<Node>> new_children) {
        for (Node* new_child : new_children) {
            PLY_ASSERT(!new_child->parent);
            new_child->parent = this;
        }
        this->children += std::move(new_children);
    }

    Array<Owned<Node>> remove_children() {
        for (Node* child : this->children) {
            PLY_ASSERT(child->parent == this);
            child->parent = nullptr;
        }
        return std::move(this->children);
    }

    bool is_container_block() const {
        return this->type < StartLeafNodeType;
    }

    bool is_leaf_block() const {
        return (this->type >= StartLeafNodeType) && (this->type < StartInlineNodeType);
    }

    bool is_inline_element() const {
        return this->type >= StartInlineNodeType;
    }

    bool is_ordered_list() const {
        return (this->type == List) && (this->list_start_number >= 0);
    }
};

// `ply::markdown::Parser` is an opaque data type you instantiate using
// `create_parser()`. Its API consists of two main funtions: `parse_line()`,
// which consumes one line of input text at a time, and `flush()`, which
// terminates the current top-level block.
//
// These functions return a `Node` when a top-level block (such as a
// paragraph or list) has ended, or `nullptr` if the current top-level
// block hasn't ended yet.
//
// Basically, you feed this class a sequence of input lines, and it gives
// you back a sequence of top-level `Node`s that make up the Markdown document.

struct Parser;

Owned<Parser> create_parser();
void destroy(Parser* parser);

Owned<Node> parse_line(Parser* parser, StringView line);
Owned<Node> flush(Parser* parser);

// Alternatively, call `ply::markdown::convert_to_html()` to parse an entire Markdown document
// at once and get an array of `Node`s back.

String convert_to_html(StringView src);

// `convert_to_html()` converts a `Node` (and all its children) to HTML.

struct HTML_Options {
    bool child_anchors = false;
};

void convert_to_html(Stream* outs, const Node* node, const HTML_Options& options);

// dump() is for debugging purposes.

void dump(Stream* outs, const Node* node, u32 level = 0);

} // namespace markdown
} // namespace ply
