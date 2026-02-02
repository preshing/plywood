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
    u32 headingLevel = 0;            // only used by Headings
    u32 relativeIndent = 0;          // only used by List_Items
    s32 listStartNumber = 0;         // only used by Lists. -1 means unordered
    bool isLooseIfContinued = false; // only used by Lists
    bool isLoose = false;            // only used by Lists
    char listPunc = '-';             // only used by Lists
    Array<Owned<Element>> children;
    Element* parent = nullptr;
    Array<String> rawLines; // only used by Leaf elements (Heading, Paragraph, Code_Block)
    String text;            // only used by Text, CodeSpan or Link (for the destination)
    String id;              // sets the id attribute for Headings

    Element(Element* parent, Type type) : type{type}, parent{parent} {
        if (parent) {
            parent->children.append(this);
        }
    }

    void addChildren(ArrayView<Owned<Element>> newChildren) {
        for (Element* newChild : newChildren) {
            PLY_ASSERT(!newChild->parent);
            newChild->parent = this;
        }
        this->children += std::move(newChildren);
    }

    bool isContainerBlock() const {
        return this->type < StartLeafElementType;
    }

    bool isLeafBlock() const {
        return (this->type >= StartLeafElementType) && (this->type < StartInlineElementType);
    }

    bool isInlineElement() const {
        return this->type >= StartInlineElementType;
    }

    bool isOrderedList() const {
        return (this->type == List) && (this->listStartNumber >= 0);
    }
};

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

struct Parser {
    Array<Element*> elementStack;
    Element* leafElement = nullptr;
    Element rootElement{nullptr, Element::Type::None};
};

// Creation and Destruction

Owned<Parser> createParser();
void destroy(Parser* parser);

// Parsing

Owned<Element> parseLine(Parser* parser, StringView line);
Owned<Element> flush(Parser* parser);
Array<Owned<Element>> parseWholeDocument(StringView markdown);

// Converting to HTML

struct HTML_Options {
    bool childAnchors = false;
};

String convertToHtml(StringView src);
void convertToHtml(Stream* outs, const Element* element, const HTML_Options& options);

// Debugging

void dump(Stream* outs, const Element* element, u32 level = 0);

} // namespace markdown
} // namespace ply
