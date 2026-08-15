/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Markdown Parser                                       │
│               Documentation: /docs/high-level/markdown-parser.md    │
│                                                                     │
└────────────────────────────────────────────────────────────────────*/

#pragma once
#include "ply-system.h"

#if !defined(PLY_WITH_MARKDOWN_DEBUGGING)
#define PLY_WITH_MARKDOWN_DEBUGGING 0
#endif

namespace ply {
namespace markdown {

//  ▄▄▄▄▄  ▄▄▄               ▄▄
//  ██  ██  ██   ▄▄▄▄   ▄▄▄▄ ██  ▄▄  ▄▄▄▄
//  ██▀▀█▄  ██  ██  ██ ██    ██▄█▀  ▀█▄▄▄
//  ██▄▄█▀ ▄██▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄  ▄▄▄█▀
//

//------------------------------------------------------
// A Span can be a Link, Image, Italic, Bold, Strikethrough, Text, Code, RawHTML, SoftBreak or HardBreak.
//------------------------------------------------------
struct Span {
    // Container spans can contain child spans.
    struct Container {
        Array<Owned<Span>> childSpans;
    };
    // A link contains parsed label spans and its resolved target.
    struct Link : Container {
        String destination;
        String title;
    };
    // An image contains parsed alt-text spans and its resolved target.
    struct Image : Container {
        String destination;
        String title;
    };
    struct Italic : Container {};
    struct Bold : Container {};
    struct Strikethrough : Container {};

    // Leaf span types.
    struct Text {
        String text;
    };
    struct Code {
        String text;
    };
    struct RawHTML {
        // Source text is retained verbatim; this records whether tag filtering was selected while parsing.
        String text;
        bool tagFilter = false;
    };
    struct SoftBreak {};
    struct HardBreak {};

    Variant<Link, Image, Italic, Bold, Strikethrough, Text, Code, RawHTML, SoftBreak, HardBreak> var;

    // Convenience functions:
    Container* asContainer() {
        if (auto* p = var.as<Link>())
            return p;
        if (auto* p = var.as<Image>())
            return p;
        if (auto* p = var.as<Italic>())
            return p;
        if (auto* p = var.as<Bold>())
            return p;
        if (auto* p = var.as<Strikethrough>())
            return p;
        return nullptr;
    }
    const Container* asContainer() const {
        return const_cast<Span*>(this)->asContainer();
    }
};

//------------------------------------------------------
// Alignment selected for one table column by its delimiter cell.
enum class TableAlignment : u8 {
    None,
    Left,
    Center,
    Right,
};

//------------------------------------------------------
// A Block can be a container block, table component, text leaf, HTML block or thematic break.
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
        // Task-list state is recorded when the first child is a paragraph beginning with a valid task marker.
        u32 relativeIndent = 0;
        bool isTask = false;
        bool isChecked = false;
    };
    struct BlockQuote : Inner {};
    struct Table : Inner {
        Array<TableAlignment> alignments;
    };
    struct TableRow : Inner {};

    // Leaf block types are leaves and can only contain text.
    struct Leaf {
        Array<Owned<Span>> spans;
    };
    struct Heading : Leaf {
        u32 level = 1;
        String id;
    };
    struct Paragraph : Leaf {};
    struct TableCell : Leaf {};
    struct IndentedCodeBlock : Leaf {};
    struct FencedCodeBlock : Leaf {
        String fenceMarker; // Indented code blocks leave this empty.
        String infoString;  // Optional info string for fenced code blocks.
        u32 relativeIndent = 0;
    };
    // An HTML block retains its source text; tagFilter records the rendering behavior selected while parsing.
    struct HTMLBlock {
        String text;
        bool tagFilter = false;
    };
    struct ThematicBreak {};

    Variant<List, ListItem, BlockQuote, Table, TableRow, Heading, Paragraph, TableCell, IndentedCodeBlock,
            FencedCodeBlock, HTMLBlock, ThematicBreak>
        var;
    Block* parent = nullptr;
    void* userData = nullptr;

    // Convenience functions:
    Inner* asInner() {
        if (auto* p = var.as<List>())
            return p;
        if (auto* p = var.as<ListItem>())
            return p;
        if (auto* p = var.as<BlockQuote>())
            return p;
        if (auto* p = var.as<Table>())
            return p;
        if (auto* p = var.as<TableRow>())
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
        if (auto* p = var.as<TableCell>())
            return p;
        if (auto* p = var.as<IndentedCodeBlock>())
            return p;
        if (auto* p = var.as<FencedCodeBlock>())
            return p;
        return nullptr;
    }
    const Leaf* asLeaf() const {
        return const_cast<Block*>(this)->asLeaf();
    }
};

//  ▄▄▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██
//

//------------------------------------------------------
// Controls recognition of Markdown syntax constructs independently.
//------------------------------------------------------
struct ParseOptions {
    // CommonMark inline elements.
    bool backslashEscapes = true;
    bool characterReferences = true;
    bool codeSpans = true;
    bool emphasis = true;
    bool strongEmphasis = true;
    bool inlineLinks = true;
    bool referenceLinks = true;
    bool inlineImages = true;
    bool referenceImages = true;
    bool autolinks = true;
    bool inlineHTML = true;
    bool softLineBreaks = true;
    bool hardLineBreaks = true;

    // CommonMark block elements.
    bool blockQuotes = true;
    bool orderedLists = true;
    bool unorderedLists = true;
    bool indentedCodeBlocks = true;
    bool fencedCodeBlocks = true;
    bool htmlBlocks = true;
    bool atxHeadings = true;
    bool setextHeadings = true;
    bool thematicBreaks = true;
    bool linkReferenceDefinitions = true;

    // GitHub Flavored Markdown extensions.
    bool tables = false;
    bool taskListItems = false;
    bool strikethrough = false;
    bool extendedAutolinks = false;
    bool tagFilter = false;

    static ParseOptions none();
    static ParseOptions githubFlavored();
};

struct Parser;

// Creation and destruction
Owned<Parser> createParser(const ParseOptions& options = {});

// Creates an independent deep copy that is automatically used when copying an Owned<Parser>.
Parser* duplicate(Parser* parser);
void destroy(Parser* parser);

// Parsing
Owned<Block> parseLine(Parser* parser, StringView line);
Owned<Block> flush(Parser* parser);
Array<Owned<Span>> parseInlineSpans(StringView markdown, const ParseOptions& options = {});
Array<Owned<Block>> parseWholeDocument(StringView markdown, const ParseOptions& options = {});

// Convert to HTML
struct HTMLOptions {
    bool childAnchors = false;
    Functor<String(StringView)> filterLinks;
};
String convertInlineToHtml(StringView src, const ParseOptions& parseOptions = {},
                           const HTMLOptions& htmlOptions = {});
String convertToHtml(StringView src, const ParseOptions& options = {});

// Converts one parsed inline span subtree to HTML.
void convertSpanToHtml(Stream* outs, const Span* span, const HTMLOptions& options);

// Converts one parsed block subtree to HTML.
void convertToHtml(Stream* outs, const Block* block, const HTMLOptions& options);

// Debugging
#if PLY_WITH_MARKDOWN_DEBUGGING
void dump(Stream* outs, const Block* block, u32 level = 0);
#endif

} // namespace markdown
} // namespace ply
