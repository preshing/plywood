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

// The parser generates a sequence of `ply::markdown::Element` objects as output.
// Each `Element` describes a top-level Markdown block (such as a paragraph, code
// block or list) and all its child elements.

struct Element {
    enum Type {
        // These types of elements can have child blocks:
        None = 0,
        List,
        ListItem,
        BlockQuote,

        // These types of elements are leaves and can only contain text:
        StartLeafElementType,
        Heading = StartLeafElementType,
        Paragraph,
        CodeBlock,

        // These types of elements are inline markers used inside text:
        StartInlineElementType,
        Text = StartInlineElementType,
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
    Array<Owned<Element>> children;
    Element* parent = nullptr;
    Array<String> raw_lines; // only used by Leaf elements (Heading, Paragraph, Code_Block)
    String text;             // only used by Text, Code or Link (for the destination)
    String id;               // sets the id attribute for Headings

    Element(Element* parent, Type type) : type{type}, parent{parent} {
        if (parent) {
            parent->children.append(this);
        }
    }

    void add_children(ArrayView<Owned<Element>> new_children) {
        for (Element* new_child : new_children) {
            PLY_ASSERT(!new_child->parent);
            new_child->parent = this;
        }
        this->children += std::move(new_children);
    }

    bool is_container_block() const {
        return this->type < StartLeafElementType;
    }

    bool is_leaf_block() const {
        return (this->type >= StartLeafElementType) && (this->type < StartInlineElementType);
    }

    bool is_inline_element() const {
        return this->type >= StartInlineElementType;
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
// These functions return an `Element` when a top-level block (such as a
// paragraph or list) has ended, or `nullptr` if the current top-level
// block hasn't ended yet.
//
// Basically, you feed this class a sequence of input lines, and it gives
// you back a sequence of top-level `Element`s that make up the Markdown document.

struct Parser {
    // element_stack and leaf_element, together, represent the stack of elements leading up to the
    // current location in the document being parsed.
    // element_stack can only hold BlockQuote and ListItem elements.
    // leaf_element represents the top of the stack where text goes. It can either hold a
    // Element::Paragraph or Element::CodeBlock.
    // All elements are ultimately owned by root_element.children or its descendents.
    // Calls to parse_line and flush may complete the root element, causing it to be returned from the
    // function and Parser::root_element reset to an empty state.
    Array<Element*> element_stack;
    Element* leaf_element = nullptr;
    Element root_element{nullptr, Element::Type::None};
};

Owned<Parser> create_parser();
void destroy(Parser* parser);

Owned<Element> parse_line(Parser* parser, StringView line);
Owned<Element> flush(Parser* parser);

// Alternatively, call `ply::markdown::convert_to_html()` to parse an entire Markdown document
// at once and get an array of `Element`s back.

String convert_to_html(StringView src);

// `convert_to_html()` converts an `Element` (and all its children) to HTML.

struct HTML_Options {
    bool child_anchors = false;
};

void convert_to_html(Stream* outs, const Element* element, const HTML_Options& options);

// dump() is for debugging purposes.

void dump(Stream* outs, const Element* element, u32 level = 0);

} // namespace markdown
} // namespace ply
