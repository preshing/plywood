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

//  ▄▄▄▄▄  ▄▄▄               ▄▄                 ▄▄▄         ▄▄▄▄
//  ██  ██  ██   ▄▄▄▄   ▄▄▄▄ ██  ▄▄  ▄▄▄▄      ██ ▀▀       ██  ▀▀ ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀█▄  ██  ██  ██ ██    ██▄█▀  ▀█▄▄▄      ▄█▀█▄▀▀      ▀▀▀█▄ ██  ██  ▄▄▄██ ██  ██ ▀█▄▄▄
//  ██▄▄█▀ ▄██▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄  ▄▄▄█▀     ▀█▄▄▀█▄     ▀█▄▄█▀ ██▄▄█▀ ▀█▄▄██ ██  ██  ▄▄▄█▀
//                                                                ██

struct Span;

//------------------------------------------------------
// A Block can be a List, ListItem, BlockQuote, Heading, Paragraph or CodeBlock.
//------------------------------------------------------

struct Block {
    // Inner block types can have child blocks.
    struct Inner {
        Array<Owned<Block>> childBlocks;
    };
    struct List : Inner {
        // If startNumber < 0, it's an unordered list and punctuator can be '-', '*' or '+'.
        // Otherwise, it's an ordered list and punctuator can be '.' or ')'.
        char punctuator = '-';
        s32 startNumber = -1;
        bool isLooseIfContinued = false;
        bool isLoose = false;
    };
    struct ListItem : Inner {
        u32 relativeIndent = 0;
    };
    struct BlockQuote : Inner {};

    // Leaf block types are leaves and can only contain text.
    struct Leaf {
        Array<String> rawLines;
        Array<Owned<Span>> spans;
    };
    struct Heading : Leaf {
        u32 level = 1;
        String id;
    };
    struct Paragraph : Leaf {};
    struct CodeBlock : Leaf {};

    Variant<List, ListItem, BlockQuote, Heading, Paragraph, CodeBlock> var;
    Block* parent = nullptr;

    // Convenience functions:
    Inner* asInner() {
        if (auto* p = var.as<List>())
            return p;
        if (auto* p = var.as<ListItem>())
            return p;
        if (auto* p = var.as<BlockQuote>())
            return p;
        return nullptr;
    }
    const Inner* asInner() const {
        return const_cast<Block*>(this)->asInner();
    }
    Leaf* asLeaf() {
        if (auto* p = var.as<Heading>())
            return p;
        if (auto* p = var.as<Paragraph>())
            return p;
        if (auto* p = var.as<CodeBlock>())
            return p;
        return nullptr;
    }
    const Leaf* asLeaf() const {
        return const_cast<Block*>(this)->asLeaf();
    }
};

//------------------------------------------------------
// A Span can be a Link, Italic, Bold, Code or SoftBreak.
//------------------------------------------------------

struct Span {
    // Container spans can contain child spans.
    struct Container {
        Array<Owned<Span>> childSpans;
    };
    struct Link : Container {
        String destination;
    };
    struct Italic : Container {};
    struct Bold : Container {};

    // Leaf span types.
    struct Text {
        String text;
    };
    struct Code {
        String text;
    };
    struct SoftBreak {};

    Variant<Link, Italic, Bold, Text, Code, SoftBreak> var;

    // Convenience functions:
    Container* asContainer() {
        if (auto* p = var.as<Link>())
            return p;
        if (auto* p = var.as<Italic>())
            return p;
        if (auto* p = var.as<Bold>())
            return p;
        return nullptr;
    }
    const Container* asContainer() const {
        return const_cast<Span*>(this)->asContainer();
    }
};

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

struct Parser;

// Creation and Destruction

Owned<Parser> createParser();
void destroy(Parser* parser);

// Parsing

Owned<Block> parseLine(Parser* parser, StringView line);
Owned<Block> flush(Parser* parser);
Array<Owned<Block>> parseWholeDocument(StringView markdown);

// Converting to HTML

struct HTML_Options {
    bool childAnchors = false;
};

String convertToHtml(StringView src);
void convertToHtml(Stream* outs, const Block* block, const HTML_Options& options);

// Debugging

void dump(Stream* outs, const Block* block, u32 level = 0);

} // namespace markdown
} // namespace ply
