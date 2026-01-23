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

//  ▄▄▄▄▄ ▄▄▄                                 ▄▄
//  ██     ██   ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██▀▀   ██  ██▄▄██ ██ ██ ██ ██▄▄██ ██  ██  ██
//  ██▄▄▄ ▄██▄ ▀█▄▄▄  ██ ██ ██ ▀█▄▄▄  ██  ██  ▀█▄▄
//

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
    u32 heading_level = 0;              // only used by Headings
    u32 relative_indent = 0;            // only used by List_Items
    s32 list_start_number = 0;          // only used by Lists. -1 means unordered
    bool is_loose_if_continued = false; // only used by Lists
    bool is_loose = false;              // only used by Lists
    char list_punc = '-';               // only used by Lists
    Array<Owned<Element>> children;
    Element* parent = nullptr;
    Array<String> raw_lines; // only used by Leaf elements (Heading, Paragraph, Code_Block)
    String text;             // only used by Text, CodeSpan or Link (for the destination)
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

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

struct Parser {
    Array<Element*> element_stack;
    Element* leaf_element = nullptr;
    Element root_element{nullptr, Element::Type::None};
};

// Creation and Destruction

Owned<Parser> create_parser();
void destroy(Parser* parser);

// Parsing

Owned<Element> parse_line(Parser* parser, StringView line);
Owned<Element> flush(Parser* parser);
Array<Owned<Element>> parse_whole_document(StringView markdown);

// Converting to HTML

struct HTML_Options {
    bool child_anchors = false;
};

String convert_to_html(StringView src);
void convert_to_html(Stream* outs, const Element* element, const HTML_Options& options);

// Debugging

void dump(Stream* outs, const Element* element, u32 level = 0);

} // namespace markdown
} // namespace ply
