/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Markdown Parser                                       │
│               Documentation: /docs/high-level/markdown-parser.md    │
│                                                                     │
└────────────────────────────────────────────────────────────────────*/

#include "ply-markdown.h"

namespace ply {
namespace markdown {

//------------------------------------------------------------------
// Parser implementation details not exposed in the public API.
//------------------------------------------------------------------
struct Parser {
    // A normalized link-reference definition collected before whole-document parsing.
    struct LinkReference {
        String label;
        String destination;
        String title;
    };

    // Optional syntax extensions selected when the parser was created.
    ParseOptions options;

    // The current stack of nested Markdown blocks based on the content of previous lines.
    // Consists of ListItems and BlockQuotes.
    Array<Block*> activeBlocks;

    // The current leaf block, if any; paragraphs, headings, code blocks and HTML blocks go here.
    Block* leafBlock = nullptr;

    // The table currently accepting body rows, if any.
    Block* tableBlock = nullptr;

    // Accumulates raw text to be added to the leaf block.
    // Inline delimiter spans are parsed when the leaf block is flushed.
    MemStream rawLeafText;

    // Link reference definitions are populated by the whole-document convenience functions.
    Array<LinkReference> linkReferences;

    // Root block of the document. Top-level blocks are popped from the front and returned to the caller as we go.
    Block rootBlock;

    // Only used if leafBlock is IndentedCodeBlock.
    u32 numBlankLinesInIndentedCodeBlock = 0;

    // Only used if leafBlock is HTMLBlock. Types 1-5 use htmlEndMarker; types 6-7 end at a blank line.
    u8 htmlBlockType = 0;
    String htmlEndMarker;

    // This flag indicates that some Lists on the stack have their isLooseIfContinued flag set: (Alternatively, we
    // *could* store the number of such Lists on the stack, and eliminate the isLooseIfContinued flag completely, but
    // it would complicate matchExistingIndentation a little bit. Sticking with this approach for now.)
    bool checkListContinuations = false;
};

//------------------------------------------------------------------
// ColumnTrackingReader keeps track of the column index while reading UTF-8 codepoints from an input string.
// Used to determine the indentation of markers and text so we know which block each line belongs to.
//------------------------------------------------------------------
struct ColumnTrackingReader {
    static constexpr u32 TabSize = 4;

    char* startByte = nullptr;
    char* curByte = nullptr;
    char* endByte = nullptr;
    u32 column = 0;
    s32 point = -1; // Next codepoint
    u32 nextAdvance = 0;

    void prefetch() {
        DecodeResult result = decodeUnicode({this->curByte, this->endByte}, UTF8);
        this->point = result.point;
        this->nextAdvance = result.numBytes;
    }
    ColumnTrackingReader(StringView line) {
        this->startByte = line.bytes();
        this->curByte = line.bytes();
        this->endByte = line.end();
        this->prefetch();
    }
    void advance() {
        if (this->point == '\t') {
            this->column = this->column + TabSize - (this->column % TabSize);
        } else if (this->point == '\n') {
            this->column = 0;
        } else if (this->point >= 32) {
            this->column++;
        }
        this->curByte += this->nextAdvance;
        PLY_ASSERT(this->curByte <= this->endByte);
        this->prefetch();
    }
    void skipPlainAscii(u32 numBytes) {
        PLY_ASSERT(this->endByte - this->curByte >= numBytes);
        this->curByte += numBytes;
        this->column += numBytes;
        this->prefetch();
    }
    bool atEnd() {
        return (this->point < 0);
    }
    StringView viewRemaining() const {
        return {this->curByte, this->endByte};
    }
};

//------------------------------------------------------------------
// LineParser contains all the internal state that's used while parsing a single line of input, but doesn't need to be
// persisted in the Parser itself.
//------------------------------------------------------------------
struct LineParser {
    // Parser
    Parser* parser = nullptr;

    // ColumnTrackingReader
    ColumnTrackingReader ctReader;

    // Keeps track of how many entries in Parser::activeBlocks were matched by current line's indentation and
    // blockquote > markers.
    u32 blockDepth = 0;

    // True when unmatched containers were retained so this line can lazily continue an open paragraph.
    bool isLazyContinuation = false;

    // If the last matching stack entry was a blockquote, this is the column number after the > marker and optional
    // following single space (if any). If the last matching stack entry was a list item, this is the column number
    // where sufficient indentation was reached for the rest of the line to be considered part of the list item. Note
    // that different lines can have different outerColumn numbers for the same stack entry, because blockquote >
    // markers can be preceded by a different number (from 0 to 3) of spaces on each line.
    u32 outerColumn = 0;

    // How much leading space was encountered on this line after outerColumn.
    u32 relativeIndent() const {
        return this->ctReader.column - this->outerColumn;
    }

    // Constructor.
    LineParser(Parser* parser, StringView line) : parser{parser}, ctReader{line} {
    }
};

//  ▄▄▄▄▄  ▄▄▄               ▄▄         ▄▄▄▄▄                       ▄▄
//  ██  ██  ██   ▄▄▄▄   ▄▄▄▄ ██  ▄▄     ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀█▄  ██  ██  ██ ██    ██▄█▀      ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██▄▄█▀ ▄██▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄     ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                                                             ▄▄▄█▀

// Helper to create a block, set its variant, attach it to a parent, and return it.
template <typename T>
Block* addBlock(Block* parent) {
    Block* block = Heap::create<Block>();
    block->parent = parent;
    block->var.switchTo<T>();
    parent->asInner()->childBlocks.append(block);
    return block;
}

// Helper to create a span with the given variant type.
template <typename T>
Owned<Span> makeSpan() {
    Owned<Span> s = Heap::create<Span>();
    s->var.switchTo<T>();
    return s;
}

// Forward declaration.
void finalizeLeafBlock(Parser* parser);

// Table helpers are implemented after inline parsing so each cell can be expanded independently.
bool tryConvertParagraphToTable(Parser* parser, StringView delimiterLine, u32 relativeIndent);
void appendTableBodyRow(Parser* parser, StringView line);

// Forward declaration for fenced-code info strings parsed before inline parsing helpers.
String normalizeFenceInfoString(StringView src, const ParseOptions& options);

// Helper function to extract a line from a code block without leading indentation.
String extractCodeLine(StringView line, u32 startColumn, u32 optionalSpace = 0) {
    u32 startColWithSpace = startColumn + optionalSpace;
    u32 indent = 0;
    for (u32 i = 0; i < line.numBytes(); i++) {
        if (indent == startColWithSpace)
            return line.substr(i);
        u8 c = line[i];
        if (c == ' ') {
            indent++;
        } else if (c == '\t') {
            u32 tabSize = 4;
            u32 newIndent = indent + tabSize - (indent % tabSize);
            if (newIndent > startColWithSpace)
                return StringView{" "} * (newIndent - startColWithSpace) + line.substr(i + 1);
            indent = newIndent;
        } else {
            if (indent >= startColumn)
                return line.substr(i);
            indent++;
        }
    }
    PLY_ASSERT(0);
    return {};
}

// Parsed pieces of a line that begins with a potential fenced code marker.
struct FenceLine {
    char marker = 0;
    u32 markerCount = 0;
    StringView suffix;
};

// Parses an initial run of ``` or ~~~ (length >= 3) and returns the remaining suffix.
bool parseFenceLineStart(StringView remainingLine, FenceLine* outFence) {
    if (!remainingLine)
        return false;
    char marker = remainingLine[0];
    if (marker != '`' && marker != '~')
        return false;

    u32 i = 0;
    while ((i < remainingLine.numBytes()) && (remainingLine[i] == marker)) {
        i++;
    }
    if (i < 3)
        return false;

    outFence->marker = marker;
    outFence->markerCount = i;
    outFence->suffix = remainingLine.substr(i);
    return true;
}

// Parses an opening fenced code line and fills marker/info metadata for the fenced block.
bool parseOpeningFence(StringView remainingLine, u32 relativeIndent, Block::FencedCodeBlock& outFenced,
                       const ParseOptions& options) {
    if (relativeIndent > 3)
        return false;

    FenceLine fence;
    if (!parseFenceLineStart(remainingLine, &fence))
        return false;

    StringView suffix = fence.suffix.trimRight([](char c) { return c == '\n' || c == '\r'; });
    if (fence.marker == '`' && suffix.find('`') >= 0)
        return false;

    StringView info = suffix.trim();
    s32 spacePos = info.find([](char c) { return c == ' ' || c == '\t'; });
    if (spacePos >= 0) {
        info = info.left(spacePos);
    }

    outFenced.fenceMarker = remainingLine.left(fence.markerCount);
    outFenced.infoString = normalizeFenceInfoString(info, options);
    outFenced.relativeIndent = relativeIndent;
    return true;
}

// Returns true if remainingLine is a valid closing fence for the given opening marker.
bool isClosingFence(StringView remainingLine, u32 relativeIndent, StringView openingFenceMarker) {
    if (relativeIndent > 3)
        return false;
    PLY_ASSERT(openingFenceMarker);

    FenceLine fence;
    if (!parseFenceLineStart(remainingLine, &fence))
        return false;
    if (fence.marker != openingFenceMarker[0])
        return false;
    if (fence.markerCount < openingFenceMarker.numBytes())
        return false;

    StringView suffix = fence.suffix.trimRight([](char c) { return c == '\n' || c == '\r'; });
    for (char c : suffix) {
        if (c != ' ' && c != '\t')
            return false;
    }
    return true;
}

// Converts one ASCII letter to lowercase without depending on the current locale.
char toLowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

// Returns true when two ASCII strings are equal without regard to letter case.
bool isEqualAsciiCaseInsensitive(StringView a, StringView b) {
    if (a.numBytes() != b.numBytes())
        return false;
    for (u32 i = 0; i < a.numBytes(); i++) {
        if (toLowerAscii(a[i]) != toLowerAscii(b[i]))
            return false;
    }
    return true;
}

// Finds an ASCII marker in text without regard to letter case.
bool containsAsciiCaseInsensitive(StringView text, StringView marker) {
    if (marker.numBytes() > text.numBytes())
        return false;
    for (u32 i = 0; i + marker.numBytes() <= text.numBytes(); i++) {
        if (isEqualAsciiCaseInsensitive(text.substr(i, marker.numBytes()), marker))
            return true;
    }
    return false;
}

// Returns true for whitespace permitted between HTML tag components.
bool isHTMLWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

// Consumes one CommonMark HTML tag name and leaves pos immediately after it.
bool consumeHTMLTagName(StringView text, u32* pos) {
    if (*pos >= text.numBytes() || !isAlpha(text[*pos]))
        return false;
    for ((*pos)++; *pos < text.numBytes(); (*pos)++) {
        char c = text[*pos];
        if (!isAlpha(c) && !isDigit(c) && c != '-')
            break;
    }
    return true;
}

// Consumes a complete CommonMark open or closing tag used by type 7 HTML blocks.
bool consumeCompleteHTMLTag(StringView text) {
    u32 pos = 0;
    if (text.numBytes() < 3 || text[pos++] != '<')
        return false;

    // Closing tags contain only a name and optional trailing whitespace.
    if (text[pos] == '/') {
        pos++;
        if (!consumeHTMLTagName(text, &pos))
            return false;
        while (pos < text.numBytes() && isHTMLWhitespace(text[pos]))
            pos++;
        return pos + 1 == text.numBytes() && text[pos] == '>';
    }

    // Opening tags can contain any number of well-formed attributes.
    if (!consumeHTMLTagName(text, &pos))
        return false;
    while (pos < text.numBytes()) {
        u32 whitespaceStart = pos;
        while (pos < text.numBytes() && isHTMLWhitespace(text[pos]))
            pos++;
        if (pos < text.numBytes() && text[pos] == '>')
            return pos + 1 == text.numBytes();
        if (pos + 1 < text.numBytes() && text[pos] == '/' && text[pos + 1] == '>')
            return pos + 2 == text.numBytes();
        if (pos == whitespaceStart || pos >= text.numBytes())
            return false;

        char first = text[pos];
        if (!isAlpha(first) && first != '_' && first != ':')
            return false;
        for (pos++; pos < text.numBytes(); pos++) {
            char c = text[pos];
            if (!isAlpha(c) && !isDigit(c) && c != '_' && c != '.' && c != ':' && c != '-')
                break;
        }
        u32 nameEnd = pos;
        while (pos < text.numBytes() && isHTMLWhitespace(text[pos]))
            pos++;
        if (pos >= text.numBytes() || text[pos] != '=') {
            pos = nameEnd;
            continue;
        }
        pos++;
        while (pos < text.numBytes() && isHTMLWhitespace(text[pos]))
            pos++;
        if (pos >= text.numBytes())
            return false;
        if (text[pos] == '\'' || text[pos] == '"') {
            char quote = text[pos++];
            while (pos < text.numBytes() && text[pos] != quote)
                pos++;
            if (pos >= text.numBytes())
                return false;
            pos++;
        } else {
            u32 valueStart = pos;
            while (pos < text.numBytes()) {
                char c = text[pos];
                if (isHTMLWhitespace(c) || c == '"' || c == '\'' || c == '=' || c == '<' || c == '>' ||
                    c == '`') {
                    break;
                }
                pos++;
            }
            if (pos == valueStart)
                return false;
        }
    }
    return false;
}

// Consumes one CommonMark raw HTML construct beginning at start and returns its exclusive end position.
bool consumeInlineHTML(StringView text, u32 start, u32* end) {
    PLY_ASSERT(start < text.numBytes() && text[start] == '<');
    StringView remaining = text.substr(start);

    // Comments include two abbreviated empty forms and otherwise close at the first "-->" marker.
    if (remaining.startsWith("<!--")) {
        if (remaining.startsWith("<!-->")) {
            *end = start + 5;
            return true;
        }
        if (remaining.startsWith("<!--->")) {
            *end = start + 6;
            return true;
        }
        s32 marker = remaining.substr(4).find("-->");
        if (marker < 0)
            return false;
        *end = start + 4 + numericCast<u32>(marker) + 3;
        return true;
    }

    // Processing instructions and CDATA sections preserve everything through their closing marker.
    if (remaining.startsWith("<?")) {
        s32 marker = remaining.substr(2).find("?>");
        if (marker < 0)
            return false;
        *end = start + 2 + numericCast<u32>(marker) + 2;
        return true;
    }
    if (remaining.startsWith("<![CDATA[")) {
        s32 marker = remaining.substr(9).find("]]>");
        if (marker < 0)
            return false;
        *end = start + 9 + numericCast<u32>(marker) + 3;
        return true;
    }

    // Declarations require an uppercase name followed by whitespace, then close at the next '>'.
    if (remaining.startsWith("<!") && remaining.numBytes() > 2 && remaining[2] >= 'A' &&
        remaining[2] <= 'Z') {
        u32 pos = 3;
        while (pos < remaining.numBytes() && remaining[pos] >= 'A' && remaining[pos] <= 'Z')
            pos++;
        if (pos >= remaining.numBytes() || !isHTMLWhitespace(remaining[pos]))
            return false;
        s32 marker = remaining.substr(pos + 1).find('>');
        if (marker < 0)
            return false;
        *end = start + pos + 1 + numericCast<u32>(marker) + 1;
        return true;
    }

    // A quoted attribute can contain '>', so test each possible terminator until a complete tag is found.
    u32 candidateEnd = 1;
    while (candidateEnd < remaining.numBytes()) {
        s32 marker = remaining.substr(candidateEnd).find('>');
        if (marker < 0)
            return false;
        candidateEnd += numericCast<u32>(marker) + 1;
        if (consumeCompleteHTMLTag(remaining.left(candidateEnd))) {
            *end = start + candidateEnd;
            return true;
        }
    }
    return false;
}

// Describes how a recognized HTML block terminates.
struct HTMLBlockStart {
    String endMarker;
    u8 type = 0;
};

// Recognizes the first line of one of CommonMark's seven HTML block types.
bool parseHTMLBlockStart(StringView remainingLine, u32 relativeIndent, bool hasParagraph, HTMLBlockStart* result) {
    if (relativeIndent > 3)
        return false;
    StringView text = remainingLine.trimRight([](char c) { return c == '\n' || c == '\r'; });
    if (!text.startsWith('<'))
        return false;

    // Types 1-5 terminate at a marker that can occur anywhere in a subsequent line.
    StringView rawTag = text.substr(1);
    u32 tagEnd = 0;
    if (consumeHTMLTagName(rawTag, &tagEnd)) {
        StringView tag = rawTag.left(tagEnd);
        bool hasBoundary = tagEnd == rawTag.numBytes() || isHTMLWhitespace(rawTag[tagEnd]) || rawTag[tagEnd] == '>';
        if (hasBoundary && (isEqualAsciiCaseInsensitive(tag, "script") ||
                            isEqualAsciiCaseInsensitive(tag, "pre") ||
                            isEqualAsciiCaseInsensitive(tag, "style") ||
                            isEqualAsciiCaseInsensitive(tag, "textarea"))) {
            result->type = 1;
            result->endMarker = "</" + tag.lower() + ">";
            return true;
        }
    }
    if (text.startsWith("<!--")) {
        result->type = 2;
        result->endMarker = "-->";
        return true;
    }
    if (text.startsWith("<?")) {
        result->type = 3;
        result->endMarker = "?>";
        return true;
    }
    if (text.numBytes() >= 3 && text.startsWith("<!") && text[2] >= 'A' && text[2] <= 'Z') {
        result->type = 4;
        result->endMarker = ">";
        return true;
    }
    if (text.startsWith("<![CDATA[")) {
        result->type = 5;
        result->endMarker = "]]>";
        return true;
    }

    // Type 6 recognizes block-level tag names even when the rest of the tag is malformed.
    static const StringView blockTags[] = {
        "address", "article", "aside", "base", "basefont", "blockquote", "body", "caption", "center", "col",
        "colgroup", "dd", "details", "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption", "figure",
        "footer", "form", "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header",
        "hr", "html", "iframe", "legend", "li", "link", "main", "menu", "menuitem", "nav", "noframes", "ol",
        "optgroup", "option", "p", "param", "search", "section", "summary", "table", "tbody", "td", "tfoot",
        "th", "thead", "title", "tr", "track", "ul",
    };
    u32 nameStart = 1;
    if (nameStart < text.numBytes() && text[nameStart] == '/')
        nameStart++;
    u32 nameEnd = nameStart;
    if (consumeHTMLTagName(text, &nameEnd)) {
        bool hasBoundary = nameEnd == text.numBytes() || isHTMLWhitespace(text[nameEnd]) || text[nameEnd] == '/' ||
                           text[nameEnd] == '>';
        if (hasBoundary) {
            StringView tag = text.substr(nameStart, nameEnd - nameStart);
            for (StringView blockTag : blockTags) {
                if (isEqualAsciiCaseInsensitive(tag, blockTag)) {
                    result->type = 6;
                    return true;
                }
            }
        }
    }

    // Type 7 must be a complete tag and cannot interrupt a paragraph.
    StringView completeTagText = text.trimRight(isHTMLWhitespace);
    if (!hasParagraph && consumeCompleteHTMLTag(completeTagText)) {
        result->type = 7;
        return true;
    }
    return false;
}

// Appends one source line verbatim and returns true when it closes an HTML block of type 1-5.
bool appendHTMLBlockLine(Parser* parser, LineParser& lp) {
    String line = extractCodeLine({lp.ctReader.startByte, lp.ctReader.endByte}, lp.outerColumn);
    parser->rawLeafText.write(line);
    return parser->htmlBlockType <= 5 && containsAsciiCaseInsensitive(line, parser->htmlEndMarker);
}

// Returns true if the remaining line is a thematic break, according to basic CommonMark rules:
// up to 3 columns of indentation, followed by at least 3 matching '-', '*' or '_' markers
// separated only by spaces/tabs.
bool isThematicBreak(StringView remainingLine, u32 relativeIndent) {
    if (relativeIndent > 3)
        return false;
    StringView text = remainingLine.trim();
    if (text.isEmpty())
        return false;

    char punctuator = 0;
    u32 numPunctuators = 0;
    for (char c : text) {
        if (c == ' ' || c == '\t') {
            continue;
        }
        if (!punctuator) {
            if (c != '-' && c != '*' && c != '_')
                return false;
            punctuator = numericCast<char>(c);
        } else if (c != punctuator)
            return false;
        numPunctuators++;
    }
    return numPunctuators >= 3;
}

// Returns true if the remaining line is a Setext underline (level 1 for '=', level 2 for '-').
bool isSetextUnderline(StringView remainingLine, u32 relativeIndent, u32* level) {
    if (relativeIndent > 3)
        return false;
    StringView text = remainingLine.trim();
    if (text.isEmpty())
        return false;
    char marker = text[0];
    if (marker != '=' && marker != '-')
        return false;
    for (char c : text) {
        if (c != marker)
            return false;
    }
    *level = (marker == '=') ? 1u : 2u;
    return true;
}

// Removes an optional closing sequence of ATX '#' markers and its surrounding whitespace.
StringView trimClosingATXMarkers(StringView text) {
    text = text.trimRight([](char c) { return c == ' ' || c == '\t'; });
    char* markerEnd = text.end();
    char* markerStart = markerEnd;
    while (markerStart > text.bytes() && markerStart[-1] == '#') {
        markerStart--;
    }
    if (markerStart == markerEnd || (markerStart > text.bytes() && markerStart[-1] != ' ' && markerStart[-1] != '\t'))
        return text;
    return StringView{text.bytes(), markerStart}.trimRight([](char c) { return c == ' ' || c == '\t'; });
}

// Removes whitespace between Setext heading content and its underline without changing earlier line endings.
void trimSetextHeadingContent(Parser* parser) {
    String rawText = parser->rawLeafText.moveToString();
    new (&parser->rawLeafText) MemStream;
    parser->rawLeafText.write(rawText.trimRight([](char c) { return c == ' ' || c == '\t'; }));
}

// This is called at the start of each line. It figures out which of the existing blocks we are still inside by
// consuming indentation and blockquote '>' markers that match activeBlocks.
void matchExistingIndentation(LineParser& lp) {
    Parser* parser = lp.parser;
    ColumnTrackingReader& ctReader = lp.ctReader;

    // Consume leading whitespace.
    while (ctReader.point == ' ' || ctReader.point == '\t') {
        ctReader.advance();
    }

    // Iterate over stack items, matching as much leading indentation and BlockQuote '>' markers as possible.
    PLY_ASSERT(lp.blockDepth == 0);
    while (lp.blockDepth < parser->activeBlocks.numItems()) {
        // Each nested container marker can have its own leading indentation.
        while (ctReader.point == ' ' || ctReader.point == '\t') {
            ctReader.advance();
        }
        Block* block = parser->activeBlocks[lp.blockDepth];
        if (block->var.is<Block::BlockQuote>()) {
            // If there is a '>' within 3 columns of outerColumn, match this BlockQuote.
            if ((ctReader.point == '>') && (lp.relativeIndent() <= 3)) {
                lp.blockDepth++;
                ctReader.advance();
                lp.outerColumn = ctReader.column;
                if (ctReader.point == ' ' || ctReader.point == '\t') {
                    // Read optional space after '>'.
                    ctReader.advance();
                    lp.outerColumn++;
                }
                continue;
            }
        } else if (auto* listItem = block->var.as<Block::ListItem>()) {
            // If the line's indentation surpasses the list item's indentation, match this ListItem.
            if (lp.relativeIndent() >= listItem->relativeIndent) {
                lp.blockDepth++;
                lp.outerColumn += listItem->relativeIndent;
                continue;
            }
        } else {
            // activeBlocks can only hold BlockQuote and ListItem blocks.
            PLY_ASSERT(0);
        }
        break;
    }
}

// Marks containing lists as potentially loose when a blank line is seen directly inside a list item.
void markContainingListsLooseIfContinued(Parser* parser) {
    if (!parser->activeBlocks || !parser->activeBlocks.back()->var.is<Block::ListItem>())
        return;

    for (Block* block : parser->activeBlocks) {
        if (block->var.is<Block::ListItem>()) {
            auto* parentList = block->parent->var.as<Block::List>();
            PLY_ASSERT(parentList);
            if (!parentList->isLoose) {
                parentList->isLooseIfContinued = true;
                parser->checkListContinuations = true;
            }
        }
    }
}

// This is called after matchExistingIndentation() if the remainder of the line is blank.
void handleBlankLine(LineParser& lp) {
    Parser* parser = lp.parser;
    ColumnTrackingReader& ctReader = lp.ctReader;

    // Terminate paragraph or blank-line-terminated HTML block if any.
    if (parser->leafBlock && (parser->leafBlock->var.is<Block::Paragraph>() ||
                              parser->leafBlock->var.is<Block::HTMLBlock>())) {
        finalizeLeafBlock(parser);
    }

    // Stay inside lists.
    while ((lp.blockDepth < parser->activeBlocks.numItems()) &&
           parser->activeBlocks[lp.blockDepth]->var.is<Block::ListItem>()) {
        lp.blockDepth++;
    }

    // If there's another entry in activeBlocks, it must be a BlockQuote. Terminate it.
    if (lp.blockDepth < parser->activeBlocks.numItems()) {
        PLY_ASSERT(parser->activeBlocks[lp.blockDepth]->var.is<Block::BlockQuote>());
        parser->activeBlocks.resize(lp.blockDepth);
        finalizeLeafBlock(parser);
    }

    if (parser->leafBlock) {
        // At this point, the leaf block must be a code block, because Paragraphs are terminated above, and
        // Headings don't persist across lines.
        if (parser->leafBlock->var.is<Block::IndentedCodeBlock>()) {
            // Count blank lines in IndentedCodeBlocks
            if (ctReader.column - lp.outerColumn > 4) {
                // Add intermediate blank lines.
                for (u32 i = 0; i < parser->numBlankLinesInIndentedCodeBlock; i++) {
                    parser->rawLeafText.write('\n');
                }
                parser->numBlankLinesInIndentedCodeBlock = 0;
                String codeLine = extractCodeLine({ctReader.startByte, ctReader.endByte}, lp.outerColumn + 4);
                parser->rawLeafText.write(codeLine);
            } else {
                parser->numBlankLinesInIndentedCodeBlock++;
            }
        } else {
            auto* fenced = parser->leafBlock->var.as<Block::FencedCodeBlock>();
            PLY_ASSERT(fenced);
            String codeLine =
                extractCodeLine({ctReader.startByte, ctReader.endByte}, lp.outerColumn, fenced->relativeIndent);
            parser->rawLeafText.write(codeLine);
        }
    } else {
        // There's no leaf block and the remainder of the line is blank.
        markContainingListsLooseIfContinued(parser);

        // A blank line closes an empty item. A later marker can still rejoin its list, but indented content cannot
        // retroactively become the empty item's first child.
        if (parser->activeBlocks && parser->activeBlocks.back()->var.is<Block::ListItem>() &&
            parser->activeBlocks.back()->asInner()->childBlocks.isEmpty()) {
            parser->activeBlocks.pop();
        }
    }
}

// Tries to start a list item after parsing its marker.
// On success, updates lp.outerColumn and appends the corresponding List/ListItem blocks.
bool tryStartListItem(Parser* parser, LineParser& lp, char punctuator, s32 startNumber) {
    ColumnTrackingReader& ctReader = lp.ctReader;
    u32 markerBaseOuter = lp.outerColumn;
    u32 markerEndColumn = ctReader.column;

    // A list marker can be followed by whitespace, or end the line to represent an empty item.
    if (!(ctReader.point == ' ' || ctReader.point == '\t' || ctReader.point == '\n' || ctReader.atEnd()))
        return false;

    if (ctReader.point == ' ' || ctReader.point == '\t') {
        // Consume all whitespace after the marker. Padding <= 4 defines item indentation.
        // Padding > 4 means we only count one column as list indentation, and the extra
        // indentation belongs to content (usually an indented code block).
        ctReader.advance();
        while (ctReader.point == ' ' || ctReader.point == '\t') {
            ctReader.advance();
        }
        u32 padding = ctReader.column - markerEndColumn;
        if (ctReader.point == '\n' || ctReader.atEnd()) {
            lp.outerColumn = markerEndColumn + 1;
        } else if (padding <= 4) {
            lp.outerColumn = ctReader.column;
        } else {
            lp.outerColumn = markerEndColumn + 1;
        }
    } else {
        lp.outerColumn = markerEndColumn + 1;
    }

    // If list item interrupts a paragraph, it can't be empty.
    if (parser->leafBlock && ctReader.viewRemaining().trim().isEmpty())
        return false;

    PLY_ASSERT(lp.outerColumn >= markerBaseOuter);
    u32 relativeIndent = lp.outerColumn - markerBaseOuter;

    finalizeLeafBlock(parser);
    Block* listBlock = nullptr;
    Block* parentCtr = &parser->rootBlock;
    if (parser->activeBlocks) {
        parentCtr = parser->activeBlocks.back();
    }
    Block::Inner* parentInner = parentCtr->asInner();
    PLY_ASSERT(parentInner);
    if (!parentInner->childBlocks.isEmpty()) {
        Block* potentialParent = parentInner->childBlocks.back();
        if (auto* potentialList = potentialParent->var.as<Block::List>()) {
            if (potentialList->punctuator == punctuator) {
                // Add item to existing list
                listBlock = potentialParent;
            }
        }
    }
    if (!listBlock) {
        // Begin new list
        listBlock = Heap::create<Block>();
        listBlock->parent = parentCtr;
        auto& list = listBlock->var.switchTo<Block::List>();
        list.punctuator = punctuator;
        list.startNumber = startNumber;
        parentInner->childBlocks.append(listBlock);
    }
    Block* listItemBlock = addBlock<Block::ListItem>(listBlock);
    listItemBlock->var.as<Block::ListItem>()->relativeIndent = relativeIndent;
    parser->activeBlocks.append(listItemBlock);
    return true;
}

// This is called after matchExistingIndentation() if the remainder of the line is not blank. It consumes new
// blockquote '>' markers and list item markers such as '*', creating new list blocks for each marker encountered.
void parseNewMarkers(LineParser& lp) {
    Parser* parser = lp.parser;
    ColumnTrackingReader& ctReader = lp.ctReader;

    // Line must not be blank.
    PLY_ASSERT(!ctReader.viewRemaining().trim().isEmpty());

    // Attempt to parse new Block markers
    while (!ctReader.atEnd()) {
        if (lp.relativeIndent() >= 4)
            break;
        if (ctReader.viewRemaining().trim().isEmpty())
            break;
        if (parser->options.thematicBreaks && isThematicBreak(ctReader.viewRemaining(), lp.relativeIndent()))
            break;

        ColumnTrackingReader savedPos = ctReader;

        if (ctReader.point == '>' && parser->options.blockQuotes) {
            // Begin a new blockquote
            finalizeLeafBlock(parser);
            Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
            Block* bqBlock = addBlock<Block::BlockQuote>(parent);
            parser->activeBlocks.append(bqBlock);
            ctReader.advance();
            lp.outerColumn = ctReader.column;

            // Consume optional space after '>'.
            if (ctReader.point == ' ' || ctReader.point == '\t') {
                ctReader.advance();
                lp.outerColumn++;
            }
        } else if (parser->options.unorderedLists &&
                   (ctReader.point == '*' || ctReader.point == '-' || ctReader.point == '+')) {
            char punctuator = numericCast<char>(ctReader.point);
            ctReader.advance();
            // It's an unordered list item.
            if (!tryStartListItem(parser, lp, punctuator, -1))
                goto notMarker;
        } else if (parser->options.orderedLists && ctReader.point >= '0' && ctReader.point <= '9') {
            // Read number.
            ViewStream in(ctReader.viewRemaining());
            u64 num = readU64FromText(in);
            if (parser->leafBlock && num != 1) {
                // If list item interrupts a paragraph, the start number must be 1.
                goto notMarker;
            }
            if (in.getSeekPos() > 9)
                goto notMarker; // marker too long
            ctReader.skipPlainAscii(numericCast<u32>(in.getSeekPos()));

            // Read '.' or ')' punctuator after number.
            if (ctReader.point != '.' && ctReader.point != ')')
                goto notMarker;
            char punctuator = numericCast<char>(ctReader.point);
            ctReader.advance();
            // It's an ordered list item.
            // 32-bit demotion is safe because we know the marker is 9 digits or less.
            if (!tryStartListItem(parser, lp, punctuator, numericCast<s32>(num)))
                goto notMarker;
        } else {
            goto notMarker;
        }

        // Consume whitespace
        while (ctReader.point == ' ' || ctReader.point == '\t') {
            ctReader.advance();
        }
        continue;

    notMarker:
        ctReader = savedPos;
        break;
    }
}

// Parse non-blank line content into leaf blocks. This can create or extend paragraphs, detect headings/thematic
// breaks, or append indented code, while also updating list looseness state when needed.
void parseParagraphText(LineParser& lp) {
    Parser* parser = lp.parser;

    StringView remainingText =
        lp.ctReader.viewRemaining().trimLeft().trimRight([](char c) { return c == '\n' || c == '\r'; });
    bool hasPara = parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>();
    if (remainingText && parser->checkListContinuations) {
        // The deepest continued list owns the blank-line separation; pending ancestors remain tight.
        Block::List* deepestContinuedList = nullptr;
        for (Block* block : parser->activeBlocks) {
            if (block->var.is<Block::ListItem>()) {
                auto* parentList = block->parent->var.as<Block::List>();
                PLY_ASSERT(parentList);
                if (parentList->isLooseIfContinued)
                    deepestContinuedList = parentList;
            }
        }
        if (deepestContinuedList)
            deepestContinuedList->isLoose = true;
        for (Block* block : parser->activeBlocks) {
            if (block->var.is<Block::ListItem>()) {
                auto* parentList = block->parent->var.as<Block::List>();
                PLY_ASSERT(parentList);
                parentList->isLooseIfContinued = false;
            }
        }
        parser->checkListContinuations = false;
    }
    if (parser->options.indentedCodeBlocks && !hasPara && lp.relativeIndent() >= 4) {
        // Potentially begin or append to code block
        if (remainingText && !parser->leafBlock) {
            Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
            parser->leafBlock = addBlock<Block::IndentedCodeBlock>(parent);
            PLY_ASSERT(parser->rawLeafText.getSeekPos() == 0);
            PLY_ASSERT(parser->numBlankLinesInIndentedCodeBlock == 0);
        }
        if (parser->leafBlock) {
            PLY_ASSERT(parser->leafBlock->var.is<Block::IndentedCodeBlock>());
            // Add intermediate blank lines
            for (u32 i = 0; i < parser->numBlankLinesInIndentedCodeBlock; i++) {
                parser->rawLeafText.write('\n');
            }
            parser->numBlankLinesInIndentedCodeBlock = 0;
            String codeLine = extractCodeLine({lp.ctReader.startByte, lp.ctReader.endByte}, lp.outerColumn + 4);
            parser->rawLeafText.write(codeLine);
        }
    } else {
        if (remainingText) {
            HTMLBlockStart newHTML;
            if (parser->options.htmlBlocks &&
                parseHTMLBlockStart(lp.ctReader.viewRemaining(), lp.relativeIndent(), hasPara, &newHTML)) {
                if (hasPara)
                    finalizeLeafBlock(parser);
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                Block* htmlBlock = addBlock<Block::HTMLBlock>(parent);
                parser->leafBlock = htmlBlock;
                parser->htmlBlockType = newHTML.type;
                parser->htmlEndMarker = std::move(newHTML.endMarker);
                PLY_ASSERT(parser->rawLeafText.getSeekPos() == 0);
                if (appendHTMLBlockLine(parser, lp))
                    finalizeLeafBlock(parser);
                return;
            }

            Block::FencedCodeBlock newFenced;
            if (parser->options.fencedCodeBlocks &&
                parseOpeningFence(lp.ctReader.viewRemaining(), lp.relativeIndent(), newFenced, parser->options)) {
                if (hasPara) {
                    finalizeLeafBlock(parser);
                }
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                Block* fencedBlock = addBlock<Block::FencedCodeBlock>(parent);
                auto* fenced = fencedBlock->var.as<Block::FencedCodeBlock>();
                PLY_ASSERT(fenced);
                *fenced = std::move(newFenced);
                parser->leafBlock = fencedBlock;
                PLY_ASSERT(parser->rawLeafText.getSeekPos() == 0);
                return;
            }

            u32 setextLevel = 0;
            if (parser->options.setextHeadings && hasPara && !lp.isLazyContinuation &&
                isSetextUnderline(lp.ctReader.viewRemaining(), lp.relativeIndent(), &setextLevel)) {
                // Convert current paragraph block to a Setext heading.
                PLY_ASSERT(parser->leafBlock->var.is<Block::Paragraph>());
                trimSetextHeadingContent(parser);
                auto& heading = parser->leafBlock->var.switchTo<Block::Heading>();
                heading.level = setextLevel;
                finalizeLeafBlock(parser);
                return;
            }

            if (parser->options.thematicBreaks &&
                isThematicBreak(lp.ctReader.viewRemaining(), lp.relativeIndent())) {
                // Thematic breaks terminate an open paragraph and become a standalone block.
                if (hasPara) {
                    finalizeLeafBlock(parser);
                }
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                addBlock<Block::ThematicBreak>(parent);
                return;
            }

            if (parser->options.atxHeadings && remainingText.startsWith('#') && (lp.relativeIndent() <= 3)) {
                // Attempt to parse a heading
                ViewStream in{remainingText};
                u32 poundCount = 0;
                while (in.peekByte() == '#') {
                    poundCount++;
                    in.readByte();
                }
                StringView space = readWhitespace(in);
                if ((poundCount <= 6) && (!space.isEmpty() || !in.hasRemainingBytes())) {
                    // Got a heading
                    if (hasPara)
                        finalizeLeafBlock(parser);
                    Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                    Block* headingBlock = addBlock<Block::Heading>(parent);
                    auto* heading = headingBlock->var.as<Block::Heading>();
                    heading->level = poundCount;
                    parser->leafBlock = headingBlock;
                    PLY_ASSERT(parser->rawLeafText.getSeekPos() == 0);
                    if (StringView headingText = trimClosingATXMarkers(in.viewRemainingBytes())) {
                        parser->rawLeafText.write(headingText);
                    }
                    finalizeLeafBlock(parser);
                    return;
                }
            }

            // If parser->leafBlock already exists, it's a lazy paragraph continuation
            if (!hasPara) {
                // Begin new paragraph
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                parser->leafBlock = addBlock<Block::Paragraph>(parent);
                PLY_ASSERT(parser->rawLeafText.getSeekPos() == 0);
            }
            if (parser->rawLeafText.getSeekPos() > 0)
                parser->rawLeafText.write('\n');
            parser->rawLeafText.write(remainingText);
        } else {
            PLY_ASSERT(!parser->leafBlock); // Should already be cleared by this point
        }
    }
}

//   ▄▄▄▄                           ▄▄▄▄▄                       ▄▄
//  ██  ▀▀ ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄      ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄ ██  ██  ▄▄▄██ ██  ██     ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ▀█▄▄█▀ ██▄▄█▀ ▀█▄▄██ ██  ██     ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//         ██                                                              ▄▄▄█▀

// Optimistically parses a backtick code span until a matching closing backtick run, advancing pos on success.
String getCodeSpan(StringView rawText, u32* pos, u32 endTickCount) {
    MemStream mout;
    u32 i = *pos;
    while (i < rawText.numBytes()) {
        char c = rawText[i];
        if (c == '\n') {
            mout.write(' ');
            i++;
            continue;
        }
        if (c == '`') {
            u32 tickCount = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '`'; i++) {
                tickCount++;
            }
            if (tickCount == endTickCount) {
                *pos = i;
                String result = mout.moveToString();
                PLY_ASSERT(result);
                if (result[0] == ' ' && result.back() == ' ' && result.find([](char c) { return c != ' '; }) >= 0) {
                    result = result.substr(1, result.numBytes() - 2);
                }
                return result;
            }
            mout.write(rawText.substr(i - tickCount, tickCount));
        } else {
            mout.write(c);
            i++;
        }
    }
    return {};
}

// Returns true if c is an ASCII punctuation character.
inline bool isAscPunc(char c) {
    return (c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e);
}

// Returns the UTF-8 codepoint that starts at bytePos, or the sentinel at either end of the input.
s32 getInlineCodepoint(StringView text, u32 bytePos) {
    if (bytePos >= text.numBytes())
        return -1;
    return decodeUnicode(text.substr(bytePos), UTF8).point;
}

// Returns the UTF-8 codepoint immediately before bytePos, or the sentinel at the start of the input.
s32 getPreviousInlineCodepoint(StringView text, u32 bytePos) {
    if (bytePos == 0)
        return -1;
    u32 codepointPos = bytePos - 1;
    while (codepointPos > 0 && (u8(text[codepointPos]) & 0xc0) == 0x80) {
        codepointPos--;
    }
    return decodeUnicode(text.substr(codepointPos, bytePos - codepointPos), UTF8).point;
}

// Returns true for the Unicode whitespace characters used by CommonMark delimiter classification.
bool isInlineWhitespace(s32 point) {
    return point < 0 || point == 0x09 || point == 0x0a || point == 0x0c || point == 0x0d || point == 0x20 ||
           point == 0xa0 || point == 0x1680 || (point >= 0x2000 && point <= 0x200a) || point == 0x202f ||
           point == 0x205f || point == 0x3000;
}

// Returns true for Unicode punctuation and symbol characters used by CommonMark delimiter classification.
bool isInlinePunctuation(s32 point) {
    if (point < 0x80)
        return point >= 0 && isAscPunc(numericCast<char>(point));

    struct Range {
        u32 first;
        u32 last;
    };
    static const Range ranges[] = {
        {0xa1, 0xa9}, {0xab, 0xac}, {0xae, 0xb1}, {0xb4, 0xb4},
        {0xb6, 0xb8}, {0xbb, 0xbb}, {0xbf, 0xbf}, {0xd7, 0xd7},
        {0xf7, 0xf7}, {0x2c2, 0x2c5}, {0x2d2, 0x2df}, {0x2e5, 0x2eb},
        {0x2ed, 0x2ed}, {0x2ef, 0x2ff}, {0x375, 0x375}, {0x37e, 0x37e},
        {0x384, 0x385}, {0x387, 0x387}, {0x3f6, 0x3f6}, {0x482, 0x482},
        {0x55a, 0x55f}, {0x589, 0x58a}, {0x58d, 0x58f}, {0x5be, 0x5be},
        {0x5c0, 0x5c0}, {0x5c3, 0x5c3}, {0x5c6, 0x5c6}, {0x5f3, 0x5f4},
        {0x606, 0x60f}, {0x61b, 0x61b}, {0x61d, 0x61f}, {0x66a, 0x66d},
        {0x6d4, 0x6d4}, {0x6de, 0x6de}, {0x6e9, 0x6e9}, {0x6fd, 0x6fe},
        {0x700, 0x70d}, {0x7f6, 0x7f9}, {0x7fe, 0x7ff}, {0x830, 0x83e},
        {0x85e, 0x85e}, {0x888, 0x888}, {0x964, 0x965}, {0x970, 0x970},
        {0x9f2, 0x9f3}, {0x9fa, 0x9fb}, {0x9fd, 0x9fd}, {0xa76, 0xa76},
        {0xaf0, 0xaf1}, {0xb70, 0xb70}, {0xbf3, 0xbfa}, {0xc77, 0xc77},
        {0xc7f, 0xc7f}, {0xc84, 0xc84}, {0xd4f, 0xd4f}, {0xd79, 0xd79},
        {0xdf4, 0xdf4}, {0xe3f, 0xe3f}, {0xe4f, 0xe4f}, {0xe5a, 0xe5b},
        {0xf01, 0xf17}, {0xf1a, 0xf1f}, {0xf34, 0xf34}, {0xf36, 0xf36},
        {0xf38, 0xf38}, {0xf3a, 0xf3d}, {0xf85, 0xf85}, {0xfbe, 0xfc5},
        {0xfc7, 0xfcc}, {0xfce, 0xfda}, {0x104a, 0x104f}, {0x109e, 0x109f},
        {0x10fb, 0x10fb}, {0x1360, 0x1368}, {0x1390, 0x1399}, {0x1400, 0x1400},
        {0x166d, 0x166e}, {0x169b, 0x169c}, {0x16eb, 0x16ed}, {0x1735, 0x1736},
        {0x17d4, 0x17d6}, {0x17d8, 0x17db}, {0x1800, 0x180a}, {0x1940, 0x1940},
        {0x1944, 0x1945}, {0x19de, 0x19ff}, {0x1a1e, 0x1a1f}, {0x1aa0, 0x1aa6},
        {0x1aa8, 0x1aad}, {0x1b5a, 0x1b6a}, {0x1b74, 0x1b7e}, {0x1bfc, 0x1bff},
        {0x1c3b, 0x1c3f}, {0x1c7e, 0x1c7f}, {0x1cc0, 0x1cc7}, {0x1cd3, 0x1cd3},
        {0x1fbd, 0x1fbd}, {0x1fbf, 0x1fc1}, {0x1fcd, 0x1fcf}, {0x1fdd, 0x1fdf},
        {0x1fed, 0x1fef}, {0x1ffd, 0x1ffe}, {0x2010, 0x2027}, {0x2030, 0x205e},
        {0x207a, 0x207e}, {0x208a, 0x208e}, {0x20a0, 0x20c0}, {0x2100, 0x2101},
        {0x2103, 0x2106}, {0x2108, 0x2109}, {0x2114, 0x2114}, {0x2116, 0x2118},
        {0x211e, 0x2123}, {0x2125, 0x2125}, {0x2127, 0x2127}, {0x2129, 0x2129},
        {0x212e, 0x212e}, {0x213a, 0x213b}, {0x2140, 0x2144}, {0x214a, 0x214d},
        {0x214f, 0x214f}, {0x218a, 0x218b}, {0x2190, 0x2426}, {0x2440, 0x244a},
        {0x249c, 0x24e9}, {0x2500, 0x2775}, {0x2794, 0x2b73}, {0x2b76, 0x2b95},
        {0x2b97, 0x2bff}, {0x2ce5, 0x2cea}, {0x2cf9, 0x2cfc}, {0x2cfe, 0x2cff},
        {0x2d70, 0x2d70}, {0x2e00, 0x2e2e}, {0x2e30, 0x2e5d}, {0x2e80, 0x2e99},
        {0x2e9b, 0x2ef3}, {0x2f00, 0x2fd5}, {0x2ff0, 0x2ffb}, {0x3001, 0x3004},
        {0x3008, 0x3020}, {0x3030, 0x3030}, {0x3036, 0x3037}, {0x303d, 0x303f},
        {0x309b, 0x309c}, {0x30a0, 0x30a0}, {0x30fb, 0x30fb}, {0x3190, 0x3191},
        {0x3196, 0x319f}, {0x31c0, 0x31e3}, {0x3200, 0x321e}, {0x322a, 0x3247},
        {0x3250, 0x3250}, {0x3260, 0x327f}, {0x328a, 0x32b0}, {0x32c0, 0x33ff},
        {0x4dc0, 0x4dff}, {0xa490, 0xa4c6}, {0xa4fe, 0xa4ff}, {0xa60d, 0xa60f},
        {0xa673, 0xa673}, {0xa67e, 0xa67e}, {0xa6f2, 0xa6f7}, {0xa700, 0xa716},
        {0xa720, 0xa721}, {0xa789, 0xa78a}, {0xa828, 0xa82b}, {0xa836, 0xa839},
        {0xa874, 0xa877}, {0xa8ce, 0xa8cf}, {0xa8f8, 0xa8fa}, {0xa8fc, 0xa8fc},
        {0xa92e, 0xa92f}, {0xa95f, 0xa95f}, {0xa9c1, 0xa9cd}, {0xa9de, 0xa9df},
        {0xaa5c, 0xaa5f}, {0xaa77, 0xaa79}, {0xaade, 0xaadf}, {0xaaf0, 0xaaf1},
        {0xab5b, 0xab5b}, {0xab6a, 0xab6b}, {0xabeb, 0xabeb}, {0xfb29, 0xfb29},
        {0xfbb2, 0xfbc2}, {0xfd3e, 0xfd4f}, {0xfdcf, 0xfdcf}, {0xfdfc, 0xfdff},
        {0xfe10, 0xfe19}, {0xfe30, 0xfe52}, {0xfe54, 0xfe66}, {0xfe68, 0xfe6b},
        {0xff01, 0xff0f}, {0xff1a, 0xff20}, {0xff3b, 0xff40}, {0xff5b, 0xff65},
        {0xffe0, 0xffe6}, {0xffe8, 0xffee}, {0xfffc, 0xfffd}, {0x10100, 0x10102},
        {0x10137, 0x1013f}, {0x10179, 0x10189}, {0x1018c, 0x1018e}, {0x10190, 0x1019c},
        {0x101a0, 0x101a0}, {0x101d0, 0x101fc}, {0x1039f, 0x1039f}, {0x103d0, 0x103d0},
        {0x1056f, 0x1056f}, {0x10857, 0x10857}, {0x10877, 0x10878}, {0x1091f, 0x1091f},
        {0x1093f, 0x1093f}, {0x10a50, 0x10a58}, {0x10a7f, 0x10a7f}, {0x10ac8, 0x10ac8},
        {0x10af0, 0x10af6}, {0x10b39, 0x10b3f}, {0x10b99, 0x10b9c}, {0x10ead, 0x10ead},
        {0x10f55, 0x10f59}, {0x10f86, 0x10f89}, {0x11047, 0x1104d}, {0x110bb, 0x110bc},
        {0x110be, 0x110c1}, {0x11140, 0x11143}, {0x11174, 0x11175}, {0x111c5, 0x111c8},
        {0x111cd, 0x111cd}, {0x111db, 0x111db}, {0x111dd, 0x111df}, {0x11238, 0x1123d},
        {0x112a9, 0x112a9}, {0x1144b, 0x1144f}, {0x1145a, 0x1145b}, {0x1145d, 0x1145d},
        {0x114c6, 0x114c6}, {0x115c1, 0x115d7}, {0x11641, 0x11643}, {0x11660, 0x1166c},
        {0x116b9, 0x116b9}, {0x1173c, 0x1173f}, {0x1183b, 0x1183b}, {0x11944, 0x11946},
        {0x119e2, 0x119e2}, {0x11a3f, 0x11a46}, {0x11a9a, 0x11a9c}, {0x11a9e, 0x11aa2},
        {0x11b00, 0x11b09}, {0x11c41, 0x11c45}, {0x11c70, 0x11c71}, {0x11ef7, 0x11ef8},
        {0x11f43, 0x11f4f}, {0x11fd5, 0x11ff1}, {0x11fff, 0x11fff}, {0x12470, 0x12474},
        {0x12ff1, 0x12ff2}, {0x16a6e, 0x16a6f}, {0x16af5, 0x16af5}, {0x16b37, 0x16b3f},
        {0x16b44, 0x16b45}, {0x16e97, 0x16e9a}, {0x16fe2, 0x16fe2}, {0x1bc9c, 0x1bc9c},
        {0x1bc9f, 0x1bc9f}, {0x1cf50, 0x1cfc3}, {0x1d000, 0x1d0f5}, {0x1d100, 0x1d126},
        {0x1d129, 0x1d164}, {0x1d16a, 0x1d16c}, {0x1d183, 0x1d184}, {0x1d18c, 0x1d1a9},
        {0x1d1ae, 0x1d1ea}, {0x1d200, 0x1d241}, {0x1d245, 0x1d245}, {0x1d300, 0x1d356},
        {0x1d6c1, 0x1d6c1}, {0x1d6db, 0x1d6db}, {0x1d6fb, 0x1d6fb}, {0x1d715, 0x1d715},
        {0x1d735, 0x1d735}, {0x1d74f, 0x1d74f}, {0x1d76f, 0x1d76f}, {0x1d789, 0x1d789},
        {0x1d7a9, 0x1d7a9}, {0x1d7c3, 0x1d7c3}, {0x1d800, 0x1d9ff}, {0x1da37, 0x1da3a},
        {0x1da6d, 0x1da74}, {0x1da76, 0x1da83}, {0x1da85, 0x1da8b}, {0x1e14f, 0x1e14f},
        {0x1e2ff, 0x1e2ff}, {0x1e95e, 0x1e95f}, {0x1ecac, 0x1ecac}, {0x1ecb0, 0x1ecb0},
        {0x1ed2e, 0x1ed2e}, {0x1eef0, 0x1eef1}, {0x1f000, 0x1f02b}, {0x1f030, 0x1f093},
        {0x1f0a0, 0x1f0ae}, {0x1f0b1, 0x1f0bf}, {0x1f0c1, 0x1f0cf}, {0x1f0d1, 0x1f0f5},
        {0x1f10d, 0x1f1ad}, {0x1f1e6, 0x1f202}, {0x1f210, 0x1f23b}, {0x1f240, 0x1f248},
        {0x1f250, 0x1f251}, {0x1f260, 0x1f265}, {0x1f300, 0x1f6d7}, {0x1f6dc, 0x1f6ec},
        {0x1f6f0, 0x1f6fc}, {0x1f700, 0x1f776}, {0x1f77b, 0x1f7d9}, {0x1f7e0, 0x1f7eb},
        {0x1f7f0, 0x1f7f0}, {0x1f800, 0x1f80b}, {0x1f810, 0x1f847}, {0x1f850, 0x1f859},
        {0x1f860, 0x1f887}, {0x1f890, 0x1f8ad}, {0x1f8b0, 0x1f8b1}, {0x1f900, 0x1fa53},
        {0x1fa60, 0x1fa6d}, {0x1fa70, 0x1fa7c}, {0x1fa80, 0x1fa88}, {0x1fa90, 0x1fabd},
        {0x1fabf, 0x1fac5}, {0x1face, 0x1fadb}, {0x1fae0, 0x1fae8}, {0x1faf0, 0x1faf8},
        {0x1fb00, 0x1fb92}, {0x1fb94, 0x1fbca},
    };

    // The sorted category table makes the Unicode general-category lookup compact and locale-independent.
    u32 lo = 0;
    u32 hi = PLY_STATIC_ARRAY_SIZE(ranges);
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        if (point < numericCast<s32>(ranges[mid].first)) {
            hi = mid;
        } else if (point > numericCast<s32>(ranges[mid].last)) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

// Token produced by inline scanning. It can represent raw text, emphasis runs, link markers, or a completed span.
struct Delimiter {
    enum Type {
        RawText,
        Stars,
        Underscores,
        Tildes,
        OpenLink,
        OpenImage,
        InlineElem,
    };

    Type type = RawText;
    bool canOpen = false;       // Stars, Underscores & Tildes only
    bool canClose = false;      // Stars, Underscores & Tildes only
    bool active = true;         // OpenLink only
    u32 sourcePos = 0;          // OpenLink & OpenImage only
    u32 originalLength = 0;     // Stars, Underscores & Tildes only
    String textStorage;
    StringView text;
    Owned<Span> span; // InlineElem only

    Delimiter() = default;
    Delimiter(Type type, StringView text) : type{type}, text{text} {
    }
    Delimiter(Type type, String&& text) : type{type}, textStorage{std::move(text)}, text{textStorage} {
    }
    Delimiter(Owned<Span>&& s) : type{InlineElem}, span{std::move(s)} {
    }
    static Delimiter makeRun(Type type, StringView rawLine, u32 start, u32 numBytes) {
        s32 before = getPreviousInlineCodepoint(rawLine, start);
        s32 after = getInlineCodepoint(rawLine, start + numBytes);
        bool precededByWhite = isInlineWhitespace(before);
        bool followedByWhite = isInlineWhitespace(after);
        bool precededByPunc = isInlinePunctuation(before);
        bool followedByPunc = isInlinePunctuation(after);
        bool leftFlanking =
            !followedByWhite && (!followedByPunc || (precededByWhite || precededByPunc));
        bool rightFlanking =
            !precededByWhite && (!precededByPunc || (followedByWhite || followedByPunc));

        Delimiter result{type, rawLine.substr(start, numBytes)};
        result.originalLength = numBytes;
        result.canOpen = leftFlanking && (type != Underscores || !rightFlanking || precededByPunc);
        result.canClose = rightFlanking && (type != Underscores || !leftFlanking || followedByPunc);
        return result;
    }
};

// Normalizes a reference label using CommonMark's whitespace and Unicode case-folding rules.
String normalizeReferenceLabel(StringView label) {
    MemStream out;
    bool pendingSpace = false;
    for (u32 i = 0; i < label.numBytes();) {
        DecodeResult decoded = decodeUnicode(label.substr(i), UTF8);
        s32 point = decoded.point;
        u32 numBytes = decoded.status == DS_OK ? decoded.numBytes : 1;

        // Unicode case folding maps both forms of German sharp S to "ss".
        if (point == 0xdf || point == 0x1e9e) {
            if (pendingSpace) {
                out.write(' ');
                pendingSpace = false;
            }
            out.write("ss");
            i += numBytes;
            continue;
        }
        if (point == ' ' || point == '\t' || point == '\n' || point == '\r') {
            pendingSpace = out.getSeekPos() > 0;
            i += numBytes;
            continue;
        }
        if (pendingSpace) {
            out.write(' ');
            pendingSpace = false;
        }

        // Cover the regular one-to-one mappings used by Latin, Greek and Cyrillic reference labels.
        if (point >= 'A' && point <= 'Z') {
            point += 'a' - 'A';
        } else if ((point >= 0xc0 && point <= 0xd6) || (point >= 0xd8 && point <= 0xde)) {
            point += 0x20;
        } else if ((point >= 0x391 && point <= 0x3a1) || (point >= 0x3a3 && point <= 0x3ab)) {
            point += 0x20;
        } else if (point >= 0x410 && point <= 0x42f) {
            point += 0x20;
        }
        encodeUnicode(out, UTF8, point);
        i += numBytes;
    }
    return out.moveToString();
}

// Finds a previously collected reference definition by normalized label.
const Parser::LinkReference* findLinkReference(const Parser* parser, StringView label) {
    String normalized = normalizeReferenceLabel(label);
    for (const Parser::LinkReference& reference : parser->linkReferences) {
        if (reference.label == normalized)
            return &reference;
    }
    return nullptr;
}

// Result of parsing a link destination after a closing ']'.
struct LinkDestination {
    bool success = false;
    String dest;
    String title;
};

// An HTML 5 named character reference and its UTF-8 replacement text.
struct NamedCharacterReference {
    StringView name;
    StringView value;
};

// Complete semicolon-terminated HTML 5 named character reference lookup table.
static const NamedCharacterReference NamedCharacterReferences[] = {
#include "ply-markdown-entities.inc"
};

// Applies the replacements required for invalid and legacy numeric character references.
u32 normalizeNumericCharacterReference(u32 point) {
    static const u16 Windows1252Replacements[] = {
        0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x2c6, 0x2030, 0x160, 0x2039, 0x152, 0x8d, 0x17d, 0x8f,
        0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d, 0x17e, 0x178,
    };
    if (point == 0 || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
        return 0xfffd;
    if (point >= 0x80 && point <= 0x9f)
        return Windows1252Replacements[point - 0x80];
    return point;
}

// Decodes one semicolon-terminated HTML character reference at the start of src.
bool decodeCharacterReference(MemStream& out, StringView src, u32* numBytes) {
    if (src.numBytes() < 3 || src[0] != '&')
        return false;

    // Numeric references contain at most seven decimal or six hexadecimal digits.
    if (src[1] == '#') {
        u32 pos = 2;
        u32 radix = 10;
        u32 maxDigits = 7;
        if (pos < src.numBytes() && (src[pos] == 'x' || src[pos] == 'X')) {
            pos++;
            radix = 16;
            maxDigits = 6;
        }
        u32 digits = 0;
        u32 point = 0;
        while (pos < src.numBytes() && digits < maxDigits) {
            u32 digit;
            char c = src[pos];
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (radix == 16 && c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (radix == 16 && c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            } else {
                break;
            }
            point = point * radix + digit;
            digits++;
            pos++;
        }
        if (digits == 0 || pos >= src.numBytes() || src[pos] != ';')
            return false;
        encodeUnicode(out, UTF8, normalizeNumericCharacterReference(point));
        *numBytes = pos + 1;
        return true;
    }

    // Named references use ASCII alphanumeric names and require their semicolon in Markdown.
    u32 end = 1;
    while (end < src.numBytes() && ((src[end] >= 'a' && src[end] <= 'z') ||
                                    (src[end] >= 'A' && src[end] <= 'Z') ||
                                    (src[end] >= '0' && src[end] <= '9'))) {
        end++;
    }
    if (end == 1 || end >= src.numBytes() || src[end] != ';')
        return false;
    StringView name = src.substr(1, end - 1);
    u32 lo = 0;
    u32 hi = PLY_STATIC_ARRAY_SIZE(NamedCharacterReferences);
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        s32 order = compare(name, NamedCharacterReferences[mid].name);
        if (order < 0) {
            hi = mid;
        } else if (order > 0) {
            lo = mid + 1;
        } else {
            out.write(NamedCharacterReferences[mid].value);
            *numBytes = end + 1;
            return true;
        }
    }
    return false;
}

// Decodes character references while normalizing link destinations, titles and fence info strings.
String decodeCharacterReferences(StringView src) {
    MemStream out;
    for (u32 i = 0; i < src.numBytes();) {
        u32 numBytes = 0;
        if (src[i] == '&' && decodeCharacterReference(out, src.substr(i), &numBytes)) {
            i += numBytes;
            continue;
        }
        out.write(src[i++]);
    }
    return out.moveToString();
}

// Decodes backslash escapes and character references in a fenced-code info string.
String normalizeFenceInfoString(StringView src, const ParseOptions& options) {
    MemStream out;
    for (u32 i = 0; i < src.numBytes(); i++) {
        if (options.backslashEscapes && src[i] == '\\' && i + 1 < src.numBytes() && isAscPunc(src[i + 1]))
            i++;
        out.write(src[i]);
    }
    String normalized = out.moveToString();
    return options.characterReferences ? decodeCharacterReferences(normalized) : normalized;
}

// Percent-encodes bytes that aren't permitted literally in an HTML link destination.
String normalizeLinkDestination(StringView src, bool decodeReferences) {
    String decoded = decodeReferences ? decodeCharacterReferences(src) : String{src};
    static const char Hex[] = "0123456789ABCDEF";
    MemStream out;
    for (u8 c : decoded) {
        if (c <= 0x20 || c >= 0x7f || c == '"' || c == '\\' || c == '<' || c == '>' || c == '[' ||
            c == ']' || c == '`') {
            out.write('%');
            out.write(Hex[c >> 4]);
            out.write(Hex[c & 15]);
        } else {
            out.write(c);
        }
    }
    return out.moveToString();
}

// Consumes a GFM extended URL or email autolink beginning at start.
bool consumeExtendedAutolink(StringView rawText, u32 start, u32* end, String* destination,
                             bool decodeReferences) {
    PLY_ASSERT(start < rawText.numBytes());

    // Extended autolinks only begin at a line boundary or after one of GFM's permitted delimiters.
    if (start > 0) {
        char previous = rawText[start - 1];
        if (!isWhite(previous) && previous != '*' && previous != '_' && previous != '~' && previous != '(')
            return false;
    }
    auto isAlphaNumeric = [](char c) { return isAlpha(c) || isDigit(c); };
    auto equalIgnoringCase = [](char a, char b) {
        return a == b || (a >= 'A' && a <= 'Z' && a - 'A' + 'a' == b);
    };

    // Recognize www and the three supported URL schemes, then validate their domain.
    u32 domainStart = 0;
    bool addHttpScheme = false;
    bool requireDomainPeriod = false;
    if (rawText.substr(start).startsWith("www.")) {
        domainStart = start;
        addHttpScheme = true;
        requireDomainPeriod = true;
    } else {
        static const StringView Schemes[] = {"http://", "https://", "ftp://"};
        for (StringView scheme : Schemes) {
            if (start + scheme.numBytes() > rawText.numBytes())
                continue;
            bool matches = true;
            for (u32 i = 0; i < scheme.numBytes(); i++) {
                if (!equalIgnoringCase(rawText[start + i], scheme[i])) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                domainStart = start + scheme.numBytes();
                break;
            }
        }
    }
    if (domainStart || addHttpScheme) {
        // The domain is the initial run of domain characters; sentence-ending periods belong to the path.
        u32 domainEnd = domainStart;
        while (domainEnd < rawText.numBytes()) {
            char c = rawText[domainEnd];
            if (!isAlphaNumeric(c) && c != '_' && c != '-' && c != '.')
                break;
            domainEnd++;
        }
        while (domainEnd > domainStart && rawText[domainEnd - 1] == '.')
            domainEnd--;
        if (domainEnd == domainStart)
            return false;

        // Domain segments are nonempty, and underscores are forbidden in the final two segments.
        u32 numPeriods = 0;
        u32 lastPeriod = domainStart;
        u32 secondLastPeriod = domainStart;
        for (u32 i = domainStart; i < domainEnd; i++) {
            if (rawText[i] == '.') {
                if (i == domainStart || rawText[i - 1] == '.')
                    return false;
                secondLastPeriod = lastPeriod;
                lastPeriod = i + 1;
                numPeriods++;
            }
        }
        u32 protectedStart = numPeriods > 0 ? secondLastPeriod : domainStart;
        for (u32 i = protectedStart; i < domainEnd; i++) {
            if (rawText[i] == '_')
                return false;
        }
        if (requireDomainPeriod && numPeriods == 0)
            return false;

        // Paths continue through every non-space byte except '<', then GFM endpoint validation trims them.
        u32 linkEnd = domainEnd;
        while (linkEnd < rawText.numBytes() && !isWhite(rawText[linkEnd]) && rawText[linkEnd] != '<')
            linkEnd++;
        for (;;) {
            char trailing = rawText[linkEnd - 1];
            if (trailing == '?' || trailing == '!' || trailing == '.' || trailing == ',' || trailing == ':' ||
                trailing == '*' || trailing == '_' || trailing == '~') {
                linkEnd--;
                continue;
            }
            if (trailing == ')') {
                u32 numOpen = 0;
                u32 numClose = 0;
                for (u32 i = start; i < linkEnd; i++) {
                    numOpen += rawText[i] == '(';
                    numClose += rawText[i] == ')';
                }
                if (numClose > numOpen) {
                    linkEnd--;
                    continue;
                }
            } else if (trailing == ';') {
                u32 entityStart = linkEnd - 1;
                while (entityStart > domainEnd && isAlphaNumeric(rawText[entityStart - 1]))
                    entityStart--;
                if (entityStart > domainEnd && rawText[entityStart - 1] == '&') {
                    linkEnd = entityStart - 1;
                    continue;
                }
            }
            break;
        }
        StringView label = rawText.substr(start, linkEnd - start);
        *destination = normalizeLinkDestination(addHttpScheme ? "http://" + label : String{label},
                                                decodeReferences);
        *end = linkEnd;
        return true;
    }

    // GFM email local parts accept only alphanumerics and four punctuation characters.
    u32 at = start;
    while (at < rawText.numBytes()) {
        char c = rawText[at];
        if (!isAlphaNumeric(c) && c != '.' && c != '-' && c != '_' && c != '+')
            break;
        at++;
    }
    if (at == start || at >= rawText.numBytes() || rawText[at] != '@')
        return false;

    // Email domains require at least one period, and cannot end in '-' or '_'.
    u32 emailEnd = at + 1;
    if (emailEnd >= rawText.numBytes() ||
        (!isAlphaNumeric(rawText[emailEnd]) && rawText[emailEnd] != '-' && rawText[emailEnd] != '_'))
        return false;
    u32 numPeriods = 0;
    while (emailEnd < rawText.numBytes()) {
        char c = rawText[emailEnd];
        if (isAlphaNumeric(c) || c == '-' || c == '_') {
            emailEnd++;
        } else if (c == '.' && emailEnd + 1 < rawText.numBytes() &&
                   (isAlphaNumeric(rawText[emailEnd + 1]) || rawText[emailEnd + 1] == '-' ||
                    rawText[emailEnd + 1] == '_')) {
            numPeriods++;
            emailEnd++;
        } else {
            break;
        }
    }
    if (numPeriods == 0 || (emailEnd < rawText.numBytes() && rawText[emailEnd] == '@') ||
        rawText[emailEnd - 1] == '-' || rawText[emailEnd - 1] == '_')
        return false;
    StringView label = rawText.substr(start, emailEnd - start);
    *destination = normalizeLinkDestination("mailto:" + label, decodeReferences);
    *end = emailEnd;
    return true;
}

// Consumes a CommonMark URI or email autolink beginning at start and returns its exclusive end position.
bool consumeAutolink(StringView rawText, u32 start, u32* end, bool* isEmail) {
    PLY_ASSERT(start < rawText.numBytes() && rawText[start] == '<');

    // Locate the closing angle bracket. Autolink contents cannot contain whitespace, controls or another '<'.
    u32 close = start + 1;
    while (close < rawText.numBytes() && rawText[close] != '>') {
        u8 c = rawText[close];
        if (c <= 0x20 || c == 0x7f || c == '<')
            return false;
        close++;
    }
    if (close >= rawText.numBytes())
        return false;
    StringView contents = rawText.substr(start + 1, close - start - 1);

    // A URI scheme is an ASCII letter followed by 1-31 letters, digits, '+', '-' or '.', then ':'.
    u32 pos = 0;
    if (contents && ((contents[0] >= 'a' && contents[0] <= 'z') ||
                     (contents[0] >= 'A' && contents[0] <= 'Z'))) {
        for (pos = 1; pos < contents.numBytes() && pos < 32; pos++) {
            char c = contents[pos];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '+' || c == '-' || c == '.'))
                break;
        }
        if (pos >= 2 && pos < contents.numBytes() && contents[pos] == ':') {
            *end = close + 1;
            *isEmail = false;
            return true;
        }
    }

    // Email local parts use RFC 5322 atext plus '.', followed by one or more valid DNS-style labels.
    pos = 0;
    while (pos < contents.numBytes() && contents[pos] != '@') {
        char c = contents[pos];
        bool isAlphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9');
        bool isAtextPunctuation = c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' ||
                                  c == '*' || c == '+' || c == '-' || c == '/' || c == '=' || c == '?' ||
                                  c == '^' || c == '_' || c == '`' || c == '{' || c == '|' || c == '}' ||
                                  c == '~' || c == '.';
        if (!isAlphaNumeric && !isAtextPunctuation)
            return false;
        pos++;
    }
    if (pos == 0 || pos >= contents.numBytes() || contents[pos] != '@')
        return false;
    pos++;
    if (pos >= contents.numBytes())
        return false;

    // Domain labels are 1-63 characters, begin and end alphanumeric, and may contain interior hyphens.
    while (pos < contents.numBytes()) {
        u32 labelStart = pos;
        while (pos < contents.numBytes() && contents[pos] != '.') {
            char c = contents[pos];
            bool isAlphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9');
            if (!isAlphaNumeric && c != '-')
                return false;
            pos++;
        }
        u32 labelLength = pos - labelStart;
        if (labelLength == 0 || labelLength > 63 || contents[labelStart] == '-' || contents[pos - 1] == '-')
            return false;
        if (pos < contents.numBytes())
            pos++;
    }
    if (pos == 0 || contents.back() == '.')
        return false;
    *end = close + 1;
    *isEmail = true;
    return true;
}

// Parses an autolink or raw HTML at start as one atomic inline, with autolinks taking precedence.
Owned<Span> parseAtomicAngleSpan(StringView rawText, u32 start, u32* end, const ParseOptions& options) {
    PLY_ASSERT(start < rawText.numBytes() && rawText[start] == '<');

    // Consume the entire autolink so later inline parsing can't interpret backticks in its label.
    bool isEmail = false;
    if (options.autolinks && consumeAutolink(rawText, start, end, &isEmail)) {
        StringView label = rawText.substr(start + 1, *end - start - 2);
        Owned<Span> linkSpan = makeSpan<Span::Link>();
        String destination = isEmail ? "mailto:" + label : String{label};
        linkSpan->var.as<Span::Link>()->destination =
            normalizeLinkDestination(destination, options.characterReferences);
        Owned<Span> textSpan = makeSpan<Span::Text>();
        textSpan->var.as<Span::Text>()->text = label;
        linkSpan->var.as<Span::Link>()->childSpans.append(std::move(textSpan));
        return linkSpan;
    }

    // Raw HTML has the same atomicity: Markdown delimiters within the construct remain literal.
    if (options.inlineHTML && consumeInlineHTML(rawText, start, end)) {
        Owned<Span> htmlSpan = makeSpan<Span::RawHTML>();
        htmlSpan->var.as<Span::RawHTML>()->text = rawText.substr(start, *end - start);
        htmlSpan->var.as<Span::RawHTML>()->tagFilter = options.tagFilter;
        return htmlSpan;
    }
    return nullptr;
}

// Reads backslash escapes until the specified closing title delimiter.
bool parseLinkTitle(StringView rawText, u32* pos, char closing, String* title, const ParseOptions& options) {
    MemStream out;
    for (u32 i = *pos; i < rawText.numBytes(); i++) {
        char c = rawText[i];
        if (c == closing) {
            *pos = i + 1;
            String rawTitle = out.moveToString();
            *title = options.characterReferences ? decodeCharacterReferences(rawTitle) : rawTitle;
            return true;
        }
        if (options.backslashEscapes && c == '\\' && i + 1 < rawText.numBytes() && isAscPunc(rawText[i + 1])) {
            c = rawText[++i];
        }
        out.write(c);
    }
    return false;
}

// Parses a link destination from rawText starting after '(' and advances pos past ')' on success.
LinkDestination parseLinkDestination(StringView rawText, u32* pos, const ParseOptions& options) {
    u32 i = *pos;

    // Skip initial whitespace, then parse either an angle-bracketed or bare destination.
    while (i < rawText.numBytes() && isWhite(rawText[i])) {
        i++;
    }
    MemStream mout;
    if (i < rawText.numBytes() && rawText[i] == '<') {
        bool closed = false;
        for (i++; i < rawText.numBytes(); i++) {
            char c = rawText[i];
            if (c == '\n' || c == '<')
                return {};
            if (c == '>') {
                closed = true;
                i++;
                break;
            }
            if (options.backslashEscapes && c == '\\' && i + 1 < rawText.numBytes() &&
                isAscPunc(rawText[i + 1])) {
                c = rawText[++i];
            }
            mout.write(c);
        }
        if (!closed)
            return {};
    } else {
        u32 parenNestLevel = 0;
        for (; i < rawText.numBytes(); i++) {
            char c = rawText[i];
            if (c == '\n' || isWhite(c))
                break;
            if (options.backslashEscapes && c == '\\' && i + 1 < rawText.numBytes() &&
                isAscPunc(rawText[i + 1])) {
                mout.write(rawText[++i]);
            } else if (c == '(') {
                mout.write(c);
                if (++parenNestLevel > 32)
                    return {};
            } else if (c == ')') {
                if (parenNestLevel == 0)
                    break;
                mout.write(c);
                parenNestLevel--;
            } else {
                mout.write(c);
            }
        }
        if (parenNestLevel != 0)
            return {};
    }

    String destination = normalizeLinkDestination(mout.moveToString(), options.characterReferences);

    // A title is allowed only when separated from the destination by whitespace.
    bool hadWhitespace = false;
    while (i < rawText.numBytes() && isWhite(rawText[i])) {
        hadWhitespace = true;
        i++;
    }
    String title;
    if (hadWhitespace && i < rawText.numBytes() && (rawText[i] == '"' || rawText[i] == '\'' || rawText[i] == '(')) {
        char opening = rawText[i++];
        char closing = opening == '(' ? ')' : opening;
        if (!parseLinkTitle(rawText, &i, closing, &title, options))
            return {};
        while (i < rawText.numBytes() && isWhite(rawText[i])) {
            i++;
        }
    }
    if (i >= rawText.numBytes() || rawText[i] != ')')
        return {};

    *pos = i + 1;
    return {true, std::move(destination), std::move(title)};
}

// Returns the byte after the current physical line, including its line ending when present.
u32 getNextMarkdownLine(StringView src, u32 lineStart) {
    while (lineStart < src.numBytes() && src[lineStart] != '\n')
        lineStart++;
    return min(lineStart + 1, src.numBytes());
}

// Strips indentation and one block quote marker to locate reference-definition text on a physical line.
u32 getReferenceContentStart(StringView src, u32 lineStart, u32 lineEnd, bool allowBlockQuote,
                             bool* isBlockQuote) {
    u32 pos = lineStart;
    while (pos < lineEnd && pos - lineStart < 3 && src[pos] == ' ')
        pos++;
    *isBlockQuote = allowBlockQuote && pos < lineEnd && src[pos] == '>';
    if (*isBlockQuote) {
        pos++;
        if (pos < lineEnd && (src[pos] == ' ' || src[pos] == '\t'))
            pos++;
    }
    return pos;
}

// Collects reference definitions and removes their source lines before block parsing.
String collectLinkReferences(StringView src, Parser* parser) {
    MemStream cleaned;
    u32 lineStart = 0;
    bool inFence = false;
    char fenceChar = 0;
    u32 fenceLength = 0;
    bool hasParagraph = false;
    while (lineStart < src.numBytes()) {
        u32 nextLine = getNextMarkdownLine(src, lineStart);
        u32 lineEnd = nextLine - (nextLine > lineStart && src[nextLine - 1] == '\n');
        bool isBlockQuote = false;
        u32 start = getReferenceContentStart(src, lineStart, lineEnd, parser->options.blockQuotes, &isBlockQuote);

        // Track fenced code blocks so apparent definitions inside them remain literal code.
        u32 markerPos = start;
        while (markerPos < lineEnd && markerPos - start < 3 && src[markerPos] == ' ')
            markerPos++;
        u32 markerEnd = markerPos;
        while (markerEnd < lineEnd && (src[markerEnd] == '`' || src[markerEnd] == '~') &&
               src[markerEnd] == src[markerPos]) {
            markerEnd++;
        }
        u32 markerLength = markerEnd - markerPos;
        if (parser->options.fencedCodeBlocks && markerLength >= 3 &&
            (!inFence || (src[markerPos] == fenceChar && markerLength >= fenceLength))) {
            inFence = !inFence;
            fenceChar = src[markerPos];
            fenceLength = markerLength;
            cleaned.write(src.substr(lineStart, nextLine - lineStart));
            hasParagraph = false;
            lineStart = nextLine;
            continue;
        }

        bool consumed = false;
        if (!inFence && !hasParagraph && start < lineEnd && src[start] == '[') {
            u32 close = start + 1;
            bool invalid = false;
            while (close < src.numBytes()) {
                if (src[close] == '\\' && close + 1 < src.numBytes()) {
                    close += 2;
                    continue;
                }
                if (src[close] == '[' || close - start - 1 > 999) {
                    invalid = true;
                    break;
                }
                if (src[close] == ']')
                    break;
                if (src[close] == '\n') {
                    u32 followingEnd = getNextMarkdownLine(src, close + 1);
                    u32 followingLineEnd = followingEnd - (src[followingEnd - 1] == '\n');
                    if (src.substr(close + 1, followingLineEnd - close - 1).trim().isEmpty()) {
                        invalid = true;
                        break;
                    }
                }
                close++;
            }
            if (!invalid && close < src.numBytes() && close > start + 1 && close + 1 < src.numBytes() &&
                src[close + 1] == ':') {
                String label = normalizeReferenceLabel(src.substr(start + 1, close - start - 1));
                u32 candidateStart = close + 2;
                u32 candidateLineEnd = getNextMarkdownLine(src, candidateStart);
                MemStream targetText;
                targetText.write(src.substr(candidateStart, candidateLineEnd - candidateStart).trimRight());
                LinkDestination bestDestination;
                u32 bestEnd = 0;

                // Extend through nonblank continuation lines, keeping the longest complete definition.
                for (;;) {
                    String candidate = targetText.moveToString();
                    if (candidate.trim()) {
                        String wrapped = StringView{"("} + candidate + ')';
                        u32 targetPos = 1;
                        LinkDestination destination = parseLinkDestination(wrapped, &targetPos, parser->options);
                        if (destination.success) {
                            bestDestination = std::move(destination);
                            bestEnd = candidateLineEnd;
                        }
                    }
                    new (&targetText) MemStream;
                    targetText.write(candidate);
                    if (candidateLineEnd >= src.numBytes())
                        break;

                    u32 continuationEnd = getNextMarkdownLine(src, candidateLineEnd);
                    u32 continuationLineEnd = continuationEnd - (src[continuationEnd - 1] == '\n');
                    bool continuationQuote = false;
                    u32 continuationStart = getReferenceContentStart(src, candidateLineEnd, continuationLineEnd,
                                                                      parser->options.blockQuotes,
                                                                      &continuationQuote);
                    StringView continuation = src.substr(continuationStart,
                                                         continuationLineEnd - continuationStart).trim();
                    if (!continuation || continuationQuote != isBlockQuote)
                        break;
                    targetText.write('\n');
                    targetText.write(continuation);
                    candidateLineEnd = continuationEnd;
                }

                if (bestEnd && label) {
                    if (!findLinkReference(parser, label)) {
                        Parser::LinkReference& reference = parser->linkReferences.append();
                        reference.label = std::move(label);
                        reference.destination = std::move(bestDestination.dest);
                        reference.title = std::move(bestDestination.title);
                    }
                    nextLine = bestEnd;
                    if (isBlockQuote)
                        cleaned.write(src.substr(lineStart, start - lineStart));
                    consumed = true;
                }
            }
        }

        if (!consumed) {
            cleaned.write(src.substr(lineStart, nextLine - lineStart));
            if (src.substr(start, lineEnd - start).trim().isEmpty()) {
                hasParagraph = false;
            } else if (parser->options.atxHeadings && src[start] == '#' && start + 1 < lineEnd &&
                       (src[start + 1] == ' ' || src[start + 1] == '\t')) {
                hasParagraph = false;
            } else if (!inFence) {
                hasParagraph = true;
            }
        }
        lineStart = nextLine;
    }
    return cleaned.moveToString();
}

// Converts delimiters to spans, merging plain-text delimiters into adjacent Span::Text nodes.
Array<Owned<Span>> convertToInlineElems(ArrayView<Delimiter> delimiters) {
    Array<Owned<Span>> spans;
    for (Delimiter& delimiter : delimiters) {
        if (delimiter.type == Delimiter::InlineElem) {
            spans.append(std::move(delimiter.span));
        } else {
            if (!(spans.numItems() > 0 && spans.back()->var.is<Span::Text>())) {
                spans.append(makeSpan<Span::Text>());
            }
            spans.back()->var.as<Span::Text>()->text += delimiter.text;
        }
    }
    return spans;
}

// Resolves emphasis and strikethrough delimiter runs in-place, then returns inline elems from bottomPos.
Array<Owned<Span>> processInlineDelimiters(Array<Delimiter>& delimiters, u32 bottomPos,
                                           const ParseOptions& options) {
    for (u32 pos = bottomPos; pos < delimiters.numItems(); pos++) {
        Delimiter::Type closerType = delimiters[pos].type;
        if ((closerType != Delimiter::Stars && closerType != Delimiter::Underscores &&
             closerType != Delimiter::Tildes) ||
            !delimiters[pos].canClose)
            continue;

        // A closer can consume its run in more than one match, so retry it after each successful match.
        while (pos < delimiters.numItems() && delimiters[pos].type == closerType && delimiters[pos].canClose) {
            s32 openerPos = -1;
            for (u32 j = pos; j > bottomPos;) {
                --j;
                Delimiter& opener = delimiters[j];
                if (opener.type != closerType || !opener.canOpen)
                    continue;

                // Ambidextrous emphasis runs cannot match when their sum is a multiple of three unless both are.
                Delimiter& closer = delimiters[pos];
                bool oddMatch = closerType != Delimiter::Tildes && (opener.canClose || closer.canOpen) &&
                                (opener.originalLength + closer.originalLength) % 3 == 0 &&
                                (opener.originalLength % 3 != 0 || closer.originalLength % 3 != 0);
                if (!oddMatch) {
                    openerPos = numericCast<s32>(j);
                    break;
                }
            }
            if (openerPos < 0)
                break;

            // Consume one or two markers while preserving unused markers on their original sides of the span.
            u32 opener = numericCast<u32>(openerPos);
            bool isStrikethrough = closerType == Delimiter::Tildes;
            u32 numMarkers = 0;
            if (isStrikethrough) {
                numMarkers = 2;
            } else if (options.strongEmphasis && delimiters[opener].text.numBytes() >= 2 &&
                       delimiters[pos].text.numBytes() >= 2) {
                numMarkers = 2;
            } else if (options.emphasis) {
                numMarkers = 1;
            } else {
                break;
            }
            Owned<Span> span = isStrikethrough ? makeSpan<Span::Strikethrough>() :
                               numMarkers == 2 ? makeSpan<Span::Bold>() : makeSpan<Span::Italic>();
            span->asContainer()->childSpans = convertToInlineElems(delimiters.subview(opener + 1, pos - opener - 1));
            delimiters[opener].text = delimiters[opener].text.shortenedBy(numMarkers);
            delimiters[pos].text = delimiters[pos].text.shortenedBy(numMarkers);

            u32 eraseStart = delimiters[opener].text ? opener + 1 : opener;
            u32 eraseEnd = delimiters[pos].text ? pos : pos + 1;
            delimiters.erase(eraseStart, eraseEnd - eraseStart);
            delimiters.insert(eraseStart) = std::move(span);
            pos = delimiters[opener].text ? eraseStart + 1 : eraseStart;
        }
    }
    Array<Owned<Span>> result = convertToInlineElems(delimiters.subview(bottomPos));
    delimiters.resize(bottomPos);
    return result;
}

// Parses inline Markdown spans within rawText and returns the expanded span sequence.
Array<Owned<Span>> expandInlineSpans(const Parser* parser, StringView rawText) {
    // A line ending is required to turn trailing spaces into a hard break; final trailing spaces are discarded.
    if (parser->options.hardLineBreaks)
        rawText = rawText.trimRight([](char c) { return c == ' '; });
    Array<Delimiter> delimiters;
    u32 i = 0;
    u32 flushedIndex = 0;
    auto flushText = [&]() {
        if (i > flushedIndex) {
            delimiters.append({Delimiter::RawText, rawText.substr(flushedIndex, i - flushedIndex)});
            flushedIndex = i;
        }
    };
    while (i < rawText.numBytes()) {
        char c = rawText[i];

        // Extended autolinks are atomic and disabled while an ordinary link or image label is open.
        if (parser->options.extendedAutolinks) {
            bool inBracket = find(delimiters, [](const Delimiter& delimiter) {
                return delimiter.type == Delimiter::OpenLink || delimiter.type == Delimiter::OpenImage;
            }) >= 0;
            u32 linkEnd = 0;
            String destination;
            if (!inBracket && consumeExtendedAutolink(rawText, i, &linkEnd, &destination,
                                                      parser->options.characterReferences)) {
                flushText();
                Owned<Span> linkSpan = makeSpan<Span::Link>();
                linkSpan->var.as<Span::Link>()->destination = std::move(destination);
                Owned<Span> textSpan = makeSpan<Span::Text>();
                textSpan->var.as<Span::Text>()->text = rawText.substr(i, linkEnd - i);
                linkSpan->var.as<Span::Link>()->childSpans.append(std::move(textSpan));
                delimiters.append(std::move(linkSpan));
                i = linkEnd;
                flushedIndex = i;
                continue;
            }
        }
        if (c == '\n') {
            // At line boundaries, trailing spaces are trimmed and can convert to hard breaks.
            u32 savedPos = i;
            while (i > flushedIndex && rawText[i - 1] == ' ') {
                i--;
            }
            bool hardBreakFromSpaces = parser->options.hardLineBreaks && (savedPos - i >= 2);
            if (!parser->options.hardLineBreaks)
                i = savedPos;
            flushText();
            i = savedPos + 1;
            flushedIndex = i;
            if (i < rawText.numBytes()) {
                if (hardBreakFromSpaces) {
                    delimiters.append(makeSpan<Span::HardBreak>());
                } else if (parser->options.softLineBreaks) {
                    delimiters.append(makeSpan<Span::SoftBreak>());
                } else {
                    delimiters.append({Delimiter::RawText, StringView{"\n"}});
                }
            }
            continue;
        }

        if (c == '`' && parser->options.codeSpans) {
            flushText();
            u32 tickCount = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '`'; i++) {
                tickCount++;
            }
            // Try consuming code span
            u32 backup = i;
            String codeStr = getCodeSpan(rawText, &i, tickCount);
            if (codeStr) {
                Owned<Span> codeSpan = makeSpan<Span::Code>();
                codeSpan->var.as<Span::Code>()->text = std::move(codeStr);
                delimiters.append(std::move(codeSpan));
                flushedIndex = i;
            } else {
                i = backup;
                flushText();
            }
        } else if (c == '<') {
            // Angle constructs are consumed before scanning any backticks they contain.
            u32 angleEnd = 0;
            if (Owned<Span> angleSpan = parseAtomicAngleSpan(rawText, i, &angleEnd, parser->options)) {
                flushText();
                delimiters.append(std::move(angleSpan));
                i = angleEnd;
                flushedIndex = i;
                continue;
            }
            i++;
        } else if (c == '&' && parser->options.characterReferences) {
            // Decoded punctuation is text, not Markdown syntax, so keep each reference in an atomic delimiter.
            MemStream decoded;
            u32 numBytes = 0;
            if (decodeCharacterReference(decoded, rawText.substr(i), &numBytes)) {
                flushText();
                delimiters.append({Delimiter::RawText, decoded.moveToString()});
                i += numBytes;
                flushedIndex = i;
            } else {
                i++;
            }
        } else if (c == '*' && (parser->options.emphasis || parser->options.strongEmphasis)) {
            flushText();
            u32 runLength = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '*'; i++) {
                runLength++;
            }
            if ((runLength == 1 && parser->options.emphasis) ||
                (runLength >= 2 && parser->options.strongEmphasis)) {
                delimiters.append(Delimiter::makeRun(Delimiter::Stars, rawText, i - runLength, runLength));
                flushedIndex = i;
            }
        } else if (c == '_' && (parser->options.emphasis || parser->options.strongEmphasis)) {
            flushText();
            u32 runLength = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '_'; i++) {
                runLength++;
            }
            if ((runLength == 1 && parser->options.emphasis) ||
                (runLength >= 2 && parser->options.strongEmphasis)) {
                delimiters.append(Delimiter::makeRun(Delimiter::Underscores, rawText, i - runLength, runLength));
                flushedIndex = i;
            }
        } else if (c == '~' && parser->options.strikethrough) {
            // Only an exact pair of tildes forms a GFM strikethrough delimiter.
            flushText();
            u32 runLength = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '~'; i++) {
                runLength++;
            }
            if (runLength == 2) {
                delimiters.append(Delimiter::makeRun(Delimiter::Tildes, rawText, i - runLength, runLength));
                flushedIndex = i;
            }
        } else if (c == '\\' && (parser->options.backslashEscapes || parser->options.hardLineBreaks)) {
            flushText();
            i++;
            if (i >= rawText.numBytes()) {
                delimiters.append({Delimiter::RawText, StringView{"\\"}});
                flushedIndex = i;
            } else if (rawText[i] == '\n' && parser->options.hardLineBreaks) {
                delimiters.append(makeSpan<Span::HardBreak>());
                i++;
                flushedIndex = i;
            } else if (parser->options.backslashEscapes && isAscPunc(rawText[i])) {
                delimiters.append({Delimiter::RawText, rawText.substr(i, 1)});
                i++;
                flushedIndex = i;
            } else {
                delimiters.append({Delimiter::RawText, StringView{"\\"}});
                flushedIndex = i;
            }
        } else if (c == '!' && i + 1 < rawText.numBytes() && rawText[i + 1] == '[') {
            if (!parser->options.inlineImages && !parser->options.referenceImages) {
                i += 2;
                continue;
            }
            flushText();
            delimiters.append({Delimiter::OpenImage, rawText.substr(i, 2)});
            delimiters.back().sourcePos = i + 1;
            i += 2;
            flushedIndex = i;
        } else if (c == '[' && (parser->options.inlineLinks || parser->options.referenceLinks)) {
            flushText();
            delimiters.append({Delimiter::OpenLink, rawText.substr(i, 1)});
            delimiters.back().sourcePos = i;
            i++;
            flushedIndex = i;
        } else if (c == ']') {
            // Locate the nearest bracket opener before examining its inline or reference suffix.
            flushText();
            i++;
            s32 openLink = reverseFind(delimiters, [](const Delimiter& delim) {
                return delim.type == Delimiter::OpenLink || delim.type == Delimiter::OpenImage;
            });
            if (openLink < 0)
                continue;

            // An inactive link opener cannot form a nested link, but still takes precedence over outer openers.
            if (delimiters[openLink].type == Delimiter::OpenLink && !delimiters[openLink].active) {
                delimiters[openLink].type = Delimiter::RawText;
                continue;
            }

            // Inline destinations take precedence over all reference-link forms.
            LinkDestination linkDest;
            u32 suffixEnd = i;
            bool isImage = delimiters[openLink].type == Delimiter::OpenImage;
            bool allowInline = isImage ? parser->options.inlineImages : parser->options.inlineLinks;
            bool allowReference = isImage ? parser->options.referenceImages : parser->options.referenceLinks;
            bool hasInlineMarker = i < rawText.numBytes() && rawText[i] == '(';
            bool hasInlineSuffix = allowInline && hasInlineMarker;
            bool preventReferenceFallback = hasInlineMarker && !allowInline;
            if (hasInlineSuffix) {
                suffixEnd++;
                linkDest = parseLinkDestination(rawText, &suffixEnd, parser->options);
            }

            // If the inline suffix is invalid, try full, collapsed, then shortcut references.
            const Parser::LinkReference* reference = nullptr;
            bool hasReferenceSuffix = allowReference && !preventReferenceFallback && !linkDest.success &&
                                      i < rawText.numBytes() && rawText[i] == '[';
            if (hasReferenceSuffix) {
                u32 labelEnd = i + 1;
                while (labelEnd < rawText.numBytes() && rawText[labelEnd] != ']' && rawText[labelEnd] != '\n') {
                    if (rawText[labelEnd] == '\\' && labelEnd + 1 < rawText.numBytes())
                        labelEnd++;
                    labelEnd++;
                }
                if (labelEnd < rawText.numBytes() && rawText[labelEnd] == ']') {
                    StringView explicitLabel = rawText.substr(i + 1, labelEnd - i - 1);
                    StringView linkLabel = rawText.substr(delimiters[openLink].sourcePos + 1,
                                                          i - delimiters[openLink].sourcePos - 2);
                    reference = findLinkReference(parser, explicitLabel ? explicitLabel : linkLabel);
                    if (reference)
                        suffixEnd = labelEnd + 1;
                }
            } else if (allowReference && !preventReferenceFallback && !linkDest.success) {
                StringView linkLabel = rawText.substr(delimiters[openLink].sourcePos + 1,
                                                      i - delimiters[openLink].sourcePos - 2);
                reference = findLinkReference(parser, linkLabel);
                if (reference)
                    suffixEnd = i;
            }

            // An unmatched closer makes this opener unavailable to subsequent closing brackets.
            if (!linkDest.success && !reference) {
                delimiters[openLink].type = Delimiter::RawText;
                continue;
            }

            // Build the resolved link or image, retaining parsed label spans for rendering.
            Owned<Span> linkSpan = isImage ? makeSpan<Span::Image>() : makeSpan<Span::Link>();
            String* destination = isImage ? &linkSpan->var.as<Span::Image>()->destination :
                                            &linkSpan->var.as<Span::Link>()->destination;
            String* title = isImage ? &linkSpan->var.as<Span::Image>()->title :
                                      &linkSpan->var.as<Span::Link>()->title;
            if (reference) {
                *destination = reference->destination;
                *title = reference->title;
            } else {
                *destination = std::move(linkDest.dest);
                *title = std::move(linkDest.title);
            }
            linkSpan->asContainer()->childSpans = processInlineDelimiters(delimiters, openLink + 1,
                                                                          parser->options);
            delimiters.resize(openLink);
            if (!isImage) {
                for (Delimiter& delimiter : delimiters) {
                    if (delimiter.type == Delimiter::OpenLink)
                        delimiter.active = false;
                }
            }
            delimiters.append(std::move(linkSpan));
            i = suffixEnd;
            flushedIndex = suffixEnd;
        } else {
            i++;
        }
    }

    flushText();
    return processInlineDelimiters(delimiters, 0, parser->options);
}

//--------------------------------------------------------------------
// Tables
//--------------------------------------------------------------------

// Splits a table row at unescaped pipes that aren't inside valid code spans.
Array<StringView> splitTableRow(StringView source, u32* numSeparators = nullptr) {
    StringView text = source.trim();
    Array<u32> separators;
    for (u32 pos = 0; pos < text.numBytes();) {
        if (text[pos] == '\\') {
            pos += min(2u, text.numBytes() - pos);
            continue;
        }
        if (text[pos] == '`') {
            u32 runEnd = pos + 1;
            while (runEnd < text.numBytes() && text[runEnd] == '`')
                runEnd++;
            u32 runLength = runEnd - pos;

            // Only a matched backtick run forms code; unmatched backticks remain ordinary cell text.
            u32 closePos = runEnd;
            while (closePos < text.numBytes()) {
                if (text[closePos] != '`') {
                    closePos++;
                    continue;
                }
                u32 closeEnd = closePos + 1;
                while (closeEnd < text.numBytes() && text[closeEnd] == '`')
                    closeEnd++;
                if (closeEnd - closePos == runLength)
                    break;
                closePos = closeEnd;
            }
            if (closePos < text.numBytes()) {
                pos = closePos + runLength;
                continue;
            }
            pos = runEnd;
            continue;
        }
        if (text[pos] == '|')
            separators.append(pos);
        pos++;
    }
    if (numSeparators)
        *numSeparators = separators.numItems();

    // Boundary pipes decorate the row and don't create empty cells.
    Array<StringView> cells;
    u32 start = separators && separators[0] == 0 ? 1 : 0;
    for (u32 separator : separators) {
        if (separator < start)
            continue;
        cells.append(text.substr(start, separator - start).trim());
        start = separator + 1;
    }
    if (start < text.numBytes() || separators.isEmpty() || separators.back() != text.numBytes() - 1)
        cells.append(text.substr(start).trim());
    return cells;
}

// Parses a table delimiter row and records one alignment value per cell.
bool parseTableDelimiterRow(StringView line, Array<TableAlignment>* alignments) {
    u32 numSeparators = 0;
    Array<StringView> cells = splitTableRow(line, &numSeparators);
    if (numSeparators == 0 || cells.isEmpty())
        return false;

    for (StringView cell : cells) {
        if (!cell)
            return false;
        bool left = cell[0] == ':';
        bool right = cell.back() == ':';
        u32 start = left ? 1 : 0;
        u32 end = cell.numBytes() - (right ? 1 : 0);
        if (start >= end)
            return false;
        for (u32 pos = start; pos < end; pos++) {
            if (cell[pos] != '-')
                return false;
        }
        alignments->append(left ? (right ? TableAlignment::Center : TableAlignment::Left) :
                                  (right ? TableAlignment::Right : TableAlignment::None));
    }
    return true;
}

// Appends a row with exactly the table's column count, parsing each retained cell as inline Markdown.
void appendTableRow(Parser* parser, Block* tableBlock, Array<StringView> sourceCells) {
    auto* table = tableBlock->var.as<Block::Table>();
    PLY_ASSERT(table);
    Block* rowBlock = addBlock<Block::TableRow>(tableBlock);
    for (u32 column = 0; column < table->alignments.numItems(); column++) {
        Block* cellBlock = addBlock<Block::TableCell>(rowBlock);
        if (column < sourceCells.numItems()) {
            // Pipe escapes are table syntax even inside code spans, so remove only that escape before inline parsing.
            StringView source = sourceCells[column];
            MemStream normalized;
            u32 flushedPos = 0;
            for (u32 pos = 0; pos + 1 < source.numBytes(); pos++) {
                if (source[pos] == '\\' && source[pos + 1] == '|') {
                    normalized.write(source.substr(flushedPos, pos - flushedPos));
                    flushedPos = pos + 1;
                    pos++;
                }
            }
            normalized.write(source.substr(flushedPos));
            cellBlock->var.as<Block::TableCell>()->spans = expandInlineSpans(parser, normalized.moveToString());
        }
    }
}

// Converts an open one-line paragraph into a table when the current line is a matching delimiter row.
bool tryConvertParagraphToTable(Parser* parser, StringView delimiterLine, u32 relativeIndent) {
    if (relativeIndent > 3)
        return false;
    String headerText = parser->rawLeafText.moveToString();
    new (&parser->rawLeafText) MemStream;
    if (headerText.find('\n') >= 0) {
        parser->rawLeafText.write(headerText);
        return false;
    }

    u32 numHeaderSeparators = 0;
    Array<StringView> headerCells = splitTableRow(headerText, &numHeaderSeparators);
    Array<TableAlignment> alignments;
    if (numHeaderSeparators == 0 || !parseTableDelimiterRow(delimiterLine, &alignments) ||
        headerCells.numItems() != alignments.numItems()) {
        parser->rawLeafText.write(headerText);
        return false;
    }

    Block* tableBlock = parser->leafBlock;
    auto& table = tableBlock->var.switchTo<Block::Table>();
    table.alignments = std::move(alignments);
    parser->leafBlock = nullptr;
    parser->tableBlock = tableBlock;
    appendTableRow(parser, tableBlock, std::move(headerCells));
    return true;
}

// Adds an ordinary body row, filling omitted cells and discarding excess cells.
void appendTableBodyRow(Parser* parser, StringView line) {
    PLY_ASSERT(parser->tableBlock);
    appendTableRow(parser, parser->tableBlock, splitTableRow(line));
}

// Finalizes parser->leafBlock by moving raw text into spans, then clears leaf parsing state.
void finalizeLeafBlock(Parser* parser) {
    if (!parser->leafBlock)
        return;

    String rawText = parser->rawLeafText.moveToString();
    new (&parser->rawLeafText) MemStream;
    if (auto* html = parser->leafBlock->var.as<Block::HTMLBlock>()) {
        html->text = std::move(rawText);
        html->tagFilter = parser->options.tagFilter;
    } else {
        Block::Leaf* leaf = parser->leafBlock->asLeaf();
        PLY_ASSERT(leaf);
        PLY_ASSERT(leaf->spans.isEmpty());

        // Recognize a task marker only in the first paragraph of a list item, before expanding inline spans.
        if (parser->options.taskListItems && parser->leafBlock->var.is<Block::Paragraph>() &&
            parser->leafBlock->parent) {
            auto* listItem = parser->leafBlock->parent->var.as<Block::ListItem>();
            if (listItem && listItem->childBlocks && listItem->childBlocks[0] == parser->leafBlock) {
                u32 markerStart = 0;
                while (markerStart < rawText.numBytes() && rawText[markerStart] == ' ')
                    markerStart++;
                if (markerStart + 3 < rawText.numBytes() && rawText[markerStart] == '[' &&
                    (rawText[markerStart + 1] == ' ' || rawText[markerStart + 1] == 'x' ||
                     rawText[markerStart + 1] == 'X') &&
                    rawText[markerStart + 2] == ']' &&
                    isInlineWhitespace(getInlineCodepoint(rawText, markerStart + 3))) {
                    listItem->isTask = true;
                    listItem->isChecked = rawText[markerStart + 1] != ' ';
                    rawText = String{rawText.substr(markerStart + 3)};
                }
            }
        }

        if (parser->leafBlock->var.is<Block::IndentedCodeBlock>() ||
            parser->leafBlock->var.is<Block::FencedCodeBlock>()) {
            if (rawText) {
                Owned<Span> textSpan = makeSpan<Span::Text>();
                textSpan->var.as<Span::Text>()->text = std::move(rawText);
                leaf->spans.append(std::move(textSpan));
            }
        } else {
            leaf->spans = expandInlineSpans(parser, rawText);
        }
    }
    parser->leafBlock = nullptr;
    parser->numBlankLinesInIndentedCodeBlock = 0;
    parser->htmlBlockType = 0;
    parser->htmlEndMarker.clear();
}

// Returns true if the remaining line starts a container marker that must end the open paragraph.
bool listMarkerInterruptsParagraph(LineParser& lp) {
    const ParseOptions& options = lp.parser->options;
    ColumnTrackingReader reader = lp.ctReader;
    if (lp.relativeIndent() > 3)
        return false;

    // Parse an unordered marker or an ordered marker with at most nine digits.
    u64 startNumber = 0;
    if (options.unorderedLists && (reader.point == '*' || reader.point == '-' || reader.point == '+')) {
        reader.advance();
    } else if (options.orderedLists && reader.point >= '0' && reader.point <= '9') {
        u32 numDigits = 0;
        while (reader.point >= '0' && reader.point <= '9' && numDigits < 10) {
            startNumber = startNumber * 10 + reader.point - '0';
            reader.advance();
            numDigits++;
        }
        if (numDigits > 9 || (reader.point != '.' && reader.point != ')'))
            return false;
        reader.advance();
    } else {
        return options.blockQuotes && reader.point == '>';
    }

    if (!(reader.point == ' ' || reader.point == '\t' || reader.point == '\n' || reader.atEnd()))
        return false;

    // A marker for the currently open list starts its next item even when empty or numbered other than one.
    if (lp.blockDepth < lp.parser->activeBlocks.numItems()) {
        Block* unmatched = lp.parser->activeBlocks[lp.blockDepth];
        if (unmatched->var.is<Block::ListItem>()) {
            PLY_ASSERT(unmatched->parent->var.is<Block::List>());
            return true;
        }
    }

    StringView suffix = reader.viewRemaining().trim();
    if (!suffix)
        return false;
    return startNumber <= 1;
}

// Closes unmatched containers unless the current line can lazily continue their paragraph.
void closeBlocksIfNotLazyContinuation(LineParser& lp) {
    Parser* parser = lp.parser;
    if (lp.blockDepth >= parser->activeBlocks.numItems())
        return;

    bool canLazyContinueParagraph = parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>();
    if (canLazyContinueParagraph) {
        Block::FencedCodeBlock maybeFence;
        HTMLBlockStart maybeHTML;
        if (parser->options.fencedCodeBlocks && canLazyContinueParagraph &&
            parseOpeningFence(lp.ctReader.viewRemaining(), lp.relativeIndent(), maybeFence, parser->options)) {
            canLazyContinueParagraph = false;
        }
        if (parser->options.htmlBlocks && canLazyContinueParagraph &&
            parseHTMLBlockStart(lp.ctReader.viewRemaining(), lp.relativeIndent(), true, &maybeHTML)) {
            canLazyContinueParagraph = false;
        }
        if (parser->options.thematicBreaks && canLazyContinueParagraph &&
            isThematicBreak(lp.ctReader.viewRemaining(), lp.relativeIndent())) {
            canLazyContinueParagraph = false;
        }
        if (canLazyContinueParagraph && listMarkerInterruptsParagraph(lp)) {
            canLazyContinueParagraph = false;
        }
    }

    if (canLazyContinueParagraph) {
        lp.blockDepth = parser->activeBlocks.numItems();
        lp.isLazyContinuation = true;
    } else {
        finalizeLeafBlock(parser);
        parser->activeBlocks.resize(lp.blockDepth);
    }
}

// Returns true when a nonblank line starts a block that terminates a table before being parsed normally.
bool lineStartsBlockAfterTable(LineParser& lp) {
    const ParseOptions& options = lp.parser->options;
    StringView remaining = lp.ctReader.viewRemaining();
    if ((options.indentedCodeBlocks && lp.relativeIndent() >= 4) ||
        (options.thematicBreaks && isThematicBreak(remaining, lp.relativeIndent())))
        return true;

    Block::FencedCodeBlock maybeFence;
    HTMLBlockStart maybeHTML;
    if ((options.fencedCodeBlocks && parseOpeningFence(remaining, lp.relativeIndent(), maybeFence, options)) ||
        (options.htmlBlocks && parseHTMLBlockStart(remaining, lp.relativeIndent(), false, &maybeHTML))) {
        return true;
    }
    if (lp.relativeIndent() > 3)
        return false;

    ColumnTrackingReader reader = lp.ctReader;
    if (options.blockQuotes && reader.point == '>')
        return true;
    if (options.atxHeadings && reader.point == '#') {
        u32 count = 0;
        while (reader.point == '#') {
            count++;
            reader.advance();
        }
        if (count <= 6 && (reader.point == ' ' || reader.point == '\t' || reader.point == '\n' || reader.atEnd()))
            return true;
    }

    // Any syntactically valid list marker starts a new block after a table.
    if (options.unorderedLists && (reader.point == '*' || reader.point == '-' || reader.point == '+')) {
        reader.advance();
    } else if (options.orderedLists && reader.point >= '0' && reader.point <= '9') {
        u32 numDigits = 0;
        while (reader.point >= '0' && reader.point <= '9' && numDigits < 10) {
            reader.advance();
            numDigits++;
        }
        if (numDigits > 9 || (reader.point != '.' && reader.point != ')'))
            return false;
        reader.advance();
    } else {
        return false;
    }
    return reader.point == ' ' || reader.point == '\t' || reader.point == '\n' || reader.atEnd();
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

// Repairs non-owning block pointers after a parser's block tree has been copied.
void repairDuplicatedBlocks(Parser* dstParser, Parser* srcParser, Block* dstBlock, Block* srcBlock,
                                   Block* dstParent) {
    dstBlock->parent = dstParent;
    if (srcParser->leafBlock == srcBlock) {
        dstParser->leafBlock = dstBlock;
    }
    if (srcParser->tableBlock == srcBlock) {
        dstParser->tableBlock = dstBlock;
    }
    for (u32 i = 0; i < srcParser->activeBlocks.numItems(); i++) {
        if (srcParser->activeBlocks[i] == srcBlock) {
            dstParser->activeBlocks[i] = dstBlock;
        }
    }

    // Descend through the matching source and destination child trees.
    Block::Inner* srcInner = srcBlock->asInner();
    Block::Inner* dstInner = dstBlock->asInner();
    if (!srcInner) {
        PLY_ASSERT(!dstInner);
        return;
    }
    PLY_ASSERT(dstInner);
    PLY_ASSERT(srcInner->childBlocks.numItems() == dstInner->childBlocks.numItems());
    for (u32 i = 0; i < srcInner->childBlocks.numItems(); i++) {
        repairDuplicatedBlocks(dstParser, srcParser, dstInner->childBlocks[i], srcInner->childBlocks[i],
                                      dstBlock);
    }
}

// Returns parsing options with recognition of every Markdown construct disabled.
ParseOptions ParseOptions::none() {
    ParseOptions options;
    options.backslashEscapes = false;
    options.characterReferences = false;
    options.codeSpans = false;
    options.emphasis = false;
    options.strongEmphasis = false;
    options.inlineLinks = false;
    options.referenceLinks = false;
    options.inlineImages = false;
    options.referenceImages = false;
    options.autolinks = false;
    options.inlineHTML = false;
    options.softLineBreaks = false;
    options.hardLineBreaks = false;
    options.blockQuotes = false;
    options.orderedLists = false;
    options.unorderedLists = false;
    options.indentedCodeBlocks = false;
    options.fencedCodeBlocks = false;
    options.htmlBlocks = false;
    options.atxHeadings = false;
    options.setextHeadings = false;
    options.thematicBreaks = false;
    options.linkReferenceDefinitions = false;
    options.tables = false;
    options.taskListItems = false;
    options.strikethrough = false;
    options.extendedAutolinks = false;
    options.tagFilter = false;
    return options;
}

// Returns parsing options with every GitHub Flavored Markdown extension enabled.
ParseOptions ParseOptions::githubFlavored() {
    ParseOptions options;
    options.tables = true;
    options.taskListItems = true;
    options.strikethrough = true;
    options.extendedAutolinks = true;
    options.tagFilter = true;
    return options;
}

// Creates a parser with an initialized root container block and the selected syntax extensions.
Owned<Parser> createParser(const ParseOptions& options) {
    Owned<Parser> parser = Heap::create<Parser>();
    parser->options = options;
    parser->rootBlock.var.switchTo<Block::BlockQuote>();
    return parser;
}

// Creates an independent deep copy of a parser, including any unfinished block.
Parser* duplicate(Parser* parser) {
    Parser* result = Heap::create<Parser>();

    // Duplicate the owned tree and scalar parsing state.
    result->options = parser->options;
    result->rootBlock = parser->rootBlock;
    result->linkReferences = parser->linkReferences;
    result->numBlankLinesInIndentedCodeBlock = parser->numBlankLinesInIndentedCodeBlock;
    result->htmlBlockType = parser->htmlBlockType;
    result->htmlEndMarker = parser->htmlEndMarker;
    result->checkListContinuations = parser->checkListContinuations;
    result->activeBlocks.resize(parser->activeBlocks.numItems());
    for (Block*& activeBlock : result->activeBlocks) {
        activeBlock = nullptr;
    }

    // Rebuild all pointers that refer to nodes owned by the copied tree.
    repairDuplicatedBlocks(result, parser, &result->rootBlock, &parser->rootBlock, nullptr);
    for (Block* activeBlock : result->activeBlocks) {
        PLY_ASSERT(activeBlock);
    }
    PLY_ASSERT(!parser->leafBlock || result->leafBlock);
    PLY_ASSERT(!parser->tableBlock || result->tableBlock);

    // Give the duplicate its own copy of the unfinished leaf text.
    result->rawLeafText = parser->rawLeafText.duplicate();
    return result;
}

// Parses one source line and returns the next completed top-level block, if one becomes available.
Owned<Block> parseLine(Parser* parser, StringView line) {
    LineParser lp{parser, line};

    // Match existing indentation and blockquote '>' markers.
    matchExistingIndentation(lp);

    // A table consumes ordinary nonblank lines as rows, but yields block starts back to the normal parser.
    bool handledTableLine = false;
    if (parser->tableBlock) {
        bool isInsideSameContainers = lp.blockDepth == parser->activeBlocks.numItems();
        bool isBlank = lp.ctReader.viewRemaining().trim().isEmpty();
        if (isInsideSameContainers && !isBlank && !lineStartsBlockAfterTable(lp)) {
            appendTableBodyRow(parser, lp.ctReader.viewRemaining());
            handledTableLine = true;
        } else {
            parser->tableBlock = nullptr;
        }
    }

    // Continue an HTML block before interpreting blank lines or Markdown markers. Types 6 and 7 end before the
    // first blank line; types 1-5 include blank lines and end only when their closing marker is found.
    bool handledHTMLLine = false;
    if (!handledTableLine && parser->leafBlock && parser->leafBlock->var.is<Block::HTMLBlock>()) {
        if (lp.blockDepth == parser->activeBlocks.numItems()) {
            bool isBlank = lp.ctReader.viewRemaining().trim().isEmpty();
            if (parser->htmlBlockType >= 6 && isBlank) {
                finalizeLeafBlock(parser);
            } else {
                if (appendHTMLBlockLine(parser, lp))
                    finalizeLeafBlock(parser);
                handledHTMLLine = true;
            }
        }
    }

    bool handledFencedLine = false;
    if (auto* fenced = !handledTableLine && parser->leafBlock ?
                           parser->leafBlock->var.as<Block::FencedCodeBlock>() : nullptr) {
        if (lp.blockDepth == parser->activeBlocks.numItems()) {
            if (isClosingFence(lp.ctReader.viewRemaining(), lp.relativeIndent(), fenced->fenceMarker)) {
                finalizeLeafBlock(parser);
            } else {
                String codeLine = extractCodeLine(
                    {lp.ctReader.startByte, lp.ctReader.endByte}, lp.outerColumn, fenced->relativeIndent);
                parser->rawLeafText.write(codeLine);
            }
            handledFencedLine = true;
        }
    }

    if (!handledTableLine && !handledHTMLLine && !handledFencedLine) {
        if (lp.ctReader.viewRemaining().trim().isEmpty()) {
            // The rest of the line is blank.
            handleBlankLine(lp);
        } else {
            // There's more text on the current line.
            // Close blockquotes and list items that we are no longer inside.
            closeBlocksIfNotLazyContinuation(lp);
            // Close indented code blocks that we are no longer inside.
            if (parser->leafBlock && parser->leafBlock->var.is<Block::IndentedCodeBlock>() && lp.relativeIndent() < 4) {
                if (parser->numBlankLinesInIndentedCodeBlock > 0) {
                    markContainingListsLooseIfContinued(parser);
                }
                finalizeLeafBlock(parser);
            }
            // A valid table delimiter takes precedence over list markers, Setext underlines and thematic breaks.
            bool convertedTable = parser->options.tables && parser->leafBlock &&
                                  parser->leafBlock->var.is<Block::Paragraph>() && !lp.isLazyContinuation &&
                                  tryConvertParagraphToTable(parser, lp.ctReader.viewRemaining(), lp.relativeIndent());
            if (!convertedTable) {
                // Parse new markers.
                parseNewMarkers(lp);
                // Handle remaining paragraph text.
                parseParagraphText(lp);
            }
        }
    }

    auto& rootChildren = parser->rootBlock.asInner()->childBlocks;
    if (rootChildren.numItems() > 1) {
        // parseParagraphText can only add one child block, so rootBlock can only have
        // exactly 2 blocks at this point. Pop the first one and return it.
        PLY_ASSERT(rootChildren.numItems() == 2);
        Owned<Block> out = std::move(rootChildren[0]);
        rootChildren.erase(0);
        return out;
    }
    return {};
}

// Finishes parsing at end-of-input and returns the final remaining top-level block, if any.
Owned<Block> flush(Parser* parser) {
    // Terminate all existing blocks.
    finalizeLeafBlock(parser);
    parser->tableBlock = nullptr;
    parser->activeBlocks.clear();

    auto& rootChildren = parser->rootBlock.asInner()->childBlocks;
    if (rootChildren) {
        // There cannot be more than one child block at this point.
        PLY_ASSERT(rootChildren.numItems() == 1);
        Owned<Block> block = std::move(rootChildren[0]);
        rootChildren.erase(0);
        block->parent = nullptr;
        return block;
    }
    return {};
}

// Destroys a parser created with createParser().
void destroy(Parser* parser) {
    Heap::destroy(parser);
}

// Parses raw paragraph content directly into inline spans without recognizing block constructs.
Array<Owned<Span>> parseInlineElements(StringView markdown, const ParseOptions& options) {
    Owned<Parser> parser = createParser(options);
    return expandInlineSpans(parser, markdown);
}

// Convenience helper that parses inline Markdown and returns rendered HTML without block markup.
String convertInlineToHtml(StringView src, const ParseOptions& parseOptions, const HTMLOptions& htmlOptions) {
    Array<Owned<Span>> spans = parseInlineElements(src, parseOptions);
    MemStream out;
    for (const Span* span : spans) {
        convertSpanToHtml(&out, span, htmlOptions);
    }
    return out.moveToString();
}

// Convenience helper that parses an entire Markdown string into a list of top-level blocks.
Array<Owned<Block>> parseWholeDocument(StringView markdown, const ParseOptions& options) {
    Array<Owned<Block>> blocks;
    Owned<Parser> parser = createParser(options);
    String cleaned = options.linkReferenceDefinitions ? collectLinkReferences(markdown, parser) : String{markdown};
    ViewStream in{cleaned};

    while (StringView line = readLine(in)) {
        if (Owned<Block> block = parseLine(parser, line)) {
            blocks.append(std::move(block));
        }
    }
    if (Owned<Block> block = flush(parser)) {
        blocks.append(std::move(block));
    }

    return blocks;
}

// Convenience helper that parses Markdown source and returns rendered HTML.
String convertToHtml(StringView src, const ParseOptions& parseOptions) {
    MemStream out;
    markdown::HTMLOptions options;
    Owned<Parser> parser = createParser(parseOptions);
    String cleaned = parseOptions.linkReferenceDefinitions ? collectLinkReferences(src, parser) : String{src};
    ViewStream in{cleaned};

    while (StringView line = readLine(in)) {
        if (Owned<Block> block = parseLine(parser, line)) {
            convertToHtml(&out, block, options);
        }
    }
    if (Owned<Block> block = flush(parser)) {
        convertToHtml(&out, block, options);
    }

    // Preserve one line boundary when definitions are the only source blocks in the document.
    if (out.getSeekPos() == 0 && parser->linkReferences)
        return "\n";
    return out.moveToString();
}

//  ▄▄▄▄▄         ▄▄                          ▄▄
//  ██  ██  ▄▄▄▄  ██▄▄▄  ▄▄  ▄▄  ▄▄▄▄▄  ▄▄▄▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██  ██ ██▄▄██ ██  ██ ██  ██ ██  ██ ██  ██ ██ ██  ██ ██  ██
//  ██▄▄█▀ ▀█▄▄▄  ██▄▄█▀ ▀█▄▄██ ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██
//                               ▄▄▄█▀  ▄▄▄█▀            ▄▄▄█▀

#if PLY_WITH_MARKDOWN_DEBUGGING

// Debug printer for a span subtree.
void dumpSpan(Stream* outs, const Span* span, u32 level) {
    String indent = StringView{"  "} * level;
    outs->write(indent);
    if (auto* text = span->var.as<Span::Text>()) {
        outs->write("text \"");
        printEscapedString(*outs, text->text);
        outs->write('"');
    } else if (auto* link = span->var.as<Span::Link>()) {
        outs->write("link destination=\"");
        printEscapedString(*outs, link->destination);
        outs->write('"');
    } else if (auto* image = span->var.as<Span::Image>()) {
        outs->write("image destination=\"");
        printEscapedString(*outs, image->destination);
        outs->write('"');
    } else if (auto* code = span->var.as<Span::Code>()) {
        outs->write("code \"");
        printEscapedString(*outs, code->text);
        outs->write('"');
    } else if (auto* html = span->var.as<Span::RawHTML>()) {
        outs->write("rawhtml \"");
        printEscapedString(*outs, html->text);
        outs->write('"');
        if (html->tagFilter)
            outs->write(" (tag_filter)");
    } else if (span->var.is<Span::SoftBreak>()) {
        outs->write("softbreak");
    } else if (span->var.is<Span::HardBreak>()) {
        outs->write("hardbreak");
    } else if (span->var.is<Span::Italic>()) {
        outs->write("italic");
    } else if (span->var.is<Span::Bold>()) {
        outs->write("bold");
    } else if (span->var.is<Span::Strikethrough>()) {
        outs->write("strikethrough");
    } else {
        PLY_ASSERT(0);
        outs->write("???");
    }
    outs->write("\n");
    if (const Span::Container* container = span->asContainer()) {
        for (const Span* child : container->childSpans) {
            dumpSpan(outs, child, level + 1);
        }
    }
}

// Debug printer for a block subtree, including nested spans for leaf blocks.
void dump(Stream* outs, const Block* block, u32 level) {
    String indent = StringView{"  "} * level;
    outs->write(indent);
    if (auto* list = block->var.as<Block::List>()) {
        outs->write("list");
        if (list->isLoose) {
            outs->write(" (loose");
        } else {
            outs->write(" (tight");
        }
        if (list->startNumber >= 0) {
            outs->format(", ordered, start={})", list->startNumber);
        } else {
            outs->write(", unordered)");
        }
    } else if (auto* listItem = block->var.as<Block::ListItem>()) {
        outs->write("item");
        if (listItem->isTask)
            outs->write(listItem->isChecked ? " (task, checked)" : " (task, unchecked)");
    } else if (block->var.is<Block::BlockQuote>()) {
        outs->write("block_quote");
    } else if (auto* table = block->var.as<Block::Table>()) {
        outs->format("table columns={}", table->alignments.numItems());
    } else if (block->var.is<Block::TableRow>()) {
        outs->write("table_row");
    } else if (auto* heading = block->var.as<Block::Heading>()) {
        outs->format("heading level={}", heading->level);
    } else if (block->var.is<Block::Paragraph>()) {
        outs->write("paragraph");
    } else if (block->var.is<Block::TableCell>()) {
        outs->write("table_cell");
    } else if (block->var.is<Block::IndentedCodeBlock>()) {
        outs->write("indented_code_block");
    } else if (block->var.is<Block::FencedCodeBlock>()) {
        outs->write("fenced_code_block");
    } else if (auto* html = block->var.as<Block::HTMLBlock>()) {
        outs->write("html_block");
        if (html->tagFilter)
            outs->write(" (tag_filter)");
    } else if (block->var.is<Block::ThematicBreak>()) {
        outs->write("thematic_break");
    } else {
        PLY_ASSERT(0);
        outs->write("???");
    }
    outs->write("\n");
    if (const Block::Leaf* leaf = block->asLeaf()) {
        for (const Span* span : leaf->spans) {
            dumpSpan(outs, span, level + 1);
        }
    }
    if (const Block::Inner* inner = block->asInner()) {
        for (const Block* child : inner->childBlocks) {
            PLY_ASSERT(child->parent == block);
            dump(outs, child, level + 1);
        }
    }
}

#endif // PLY_WITH_MARKDOWN_DEBUGGING

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄   ▄▄ ▄▄
//  ██  ██   ██   ███▄███ ██
//  ██▀▀██   ██   ██▀█▀██ ██
//  ██  ██   ██   ██   ██ ██▄▄▄
//

// Writes raw HTML while escaping the opening delimiter of tags disallowed by GFM's tagfilter extension.
void writeFilteredRawHTML(Stream* outs, StringView text) {
    static const StringView disallowedTags[] = {
        "title", "textarea", "style", "xmp", "iframe", "noembed", "noframes", "script", "plaintext",
    };
    u32 flushedPos = 0;
    for (u32 pos = 0; pos < text.numBytes(); pos++) {
        if (text[pos] != '<')
            continue;

        // Match an optional closing slash, then a disallowed name and a valid tag-name boundary.
        u32 nameStart = pos + 1;
        if (nameStart < text.numBytes() && text[nameStart] == '/')
            nameStart++;
        for (StringView tag : disallowedTags) {
            u32 nameEnd = nameStart + tag.numBytes();
            if (nameEnd > text.numBytes() ||
                !isEqualAsciiCaseInsensitive(text.substr(nameStart, tag.numBytes()), tag)) {
                continue;
            }
            bool hasBoundary = nameEnd < text.numBytes() &&
                               (isHTMLWhitespace(text[nameEnd]) || text[nameEnd] == '>' ||
                                (text[nameEnd] == '/' && nameEnd + 1 < text.numBytes() && text[nameEnd + 1] == '>'));
            if (!hasBoundary)
                continue;
            outs->write(text.substr(flushedPos, pos - flushedPos));
            outs->write("&lt;");
            flushedPos = pos + 1;
            break;
        }
    }
    outs->write(text.substr(flushedPos));
}

// Renders the plain-text contribution of one parsed image-label span, escaping it for an HTML attribute.
void convertImageAltToHtml(Stream* outs, const Span* span) {
    if (auto* text = span->var.as<Span::Text>()) {
        printXmlEscapedString(*outs, text->text);
    } else if (auto* code = span->var.as<Span::Code>()) {
        printXmlEscapedString(*outs, code->text);
    } else if (auto* html = span->var.as<Span::RawHTML>()) {
        printXmlEscapedString(*outs, html->text);
    } else if (span->var.is<Span::SoftBreak>() || span->var.is<Span::HardBreak>()) {
        outs->write('\n');
    } else if (const Span::Container* container = span->asContainer()) {
        for (const Span* child : container->childSpans) {
            convertImageAltToHtml(outs, child);
        }
    }
}

// Renders one inline span subtree to HTML.
void convertSpanToHtml(Stream* outs, const Span* span, const HTMLOptions& options) {
    if (auto* text = span->var.as<Span::Text>()) {
        printXmlEscapedString(*outs, text->text);
    } else if (auto* link = span->var.as<Span::Link>()) {
        String destination = link->destination;
        if (options.filterLinks) {
            destination = options.filterLinks(destination);
        }
        outs->format("<a href=\"{:&}\"", destination);
        if (link->title)
            outs->format(" title=\"{:&}\"", link->title);
        outs->write('>');
        for (const Span* child : link->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</a>");
    } else if (auto* image = span->var.as<Span::Image>()) {
        String destination = image->destination;
        if (options.filterLinks) {
            destination = options.filterLinks(destination);
        }
        outs->format("<img src=\"{:&}\" alt=\"", destination);
        for (const Span* child : image->childSpans) {
            convertImageAltToHtml(outs, child);
        }
        outs->write('"');
        if (image->title)
            outs->format(" title=\"{:&}\"", image->title);
        outs->write(" />");
    } else if (auto* code = span->var.as<Span::Code>()) {
        outs->format("<code>{:&}</code>", code->text);
    } else if (auto* html = span->var.as<Span::RawHTML>()) {
        if (html->tagFilter) {
            writeFilteredRawHTML(outs, html->text);
        } else {
            outs->write(html->text);
        }
    } else if (span->var.is<Span::SoftBreak>()) {
        outs->write("\n");
    } else if (span->var.is<Span::HardBreak>()) {
        outs->write("<br />\n");
    } else if (auto* emph = span->var.as<Span::Italic>()) {
        outs->write("<em>");
        for (const Span* child : emph->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</em>");
    } else if (auto* strong = span->var.as<Span::Bold>()) {
        outs->write("<strong>");
        for (const Span* child : strong->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</strong>");
    } else if (auto* strikethrough = span->var.as<Span::Strikethrough>()) {
        outs->write("<del>");
        for (const Span* child : strikethrough->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</del>");
    } else {
        PLY_ASSERT(0);
    }
}

// Renders one block subtree to HTML.
void convertToHtml(Stream* outs, const Block* block, const HTMLOptions& options) {
    if (auto* list = block->var.as<Block::List>()) {
        if (list->startNumber >= 0) {
            if (list->startNumber != 1) {
                outs->format("<ol start=\"{}\">\n", list->startNumber);
            } else {
                outs->write("<ol>\n");
            }
        } else {
            outs->write("<ul>\n");
        }
        for (const Block* child : list->childBlocks) {
            convertToHtml(outs, child, options);
        }
        if (list->startNumber >= 0) {
            outs->write("</ol>\n");
        } else {
            outs->write("</ul>\n");
        }
    } else if (auto* listItem = block->var.as<Block::ListItem>()) {
        auto* parentList = block->parent->var.as<Block::List>();
        outs->write("<li>");
        if (listItem->childBlocks.isEmpty()) {
            // Empty items have no line break between their tags.
        } else if (!parentList->isLoose && listItem->childBlocks[0]->var.is<Block::Paragraph>()) {
            // Don't output a newline before the paragraph in a tight list.
        } else {
            outs->write("\n");
        }
        for (u32 i = 0; i < listItem->childBlocks.numItems(); i++) {
            convertToHtml(outs, listItem->childBlocks[i], options);
            if (!parentList->isLoose && listItem->childBlocks[i]->var.is<Block::Paragraph>() &&
                i + 1 < listItem->childBlocks.numItems()) {
                // This paragraph had no <p> tag and didn't end in a newline, but
                // there are more children following it, so add a newline here.
                outs->write("\n");
            }
        }
        outs->write("</li>\n");
    } else if (auto* bq = block->var.as<Block::BlockQuote>()) {
        outs->write("<blockquote>\n");
        for (const Block* child : bq->childBlocks) {
            convertToHtml(outs, child, options);
        }
        outs->write("</blockquote>\n");
    } else if (auto* table = block->var.as<Block::Table>()) {
        PLY_ASSERT(table->childBlocks);
        outs->write("<table>\n<thead>\n");
        convertToHtml(outs, table->childBlocks[0], options);
        outs->write("</thead>\n");
        if (table->childBlocks.numItems() > 1) {
            outs->write("<tbody>\n");
            for (u32 row = 1; row < table->childBlocks.numItems(); row++) {
                convertToHtml(outs, table->childBlocks[row], options);
            }
            outs->write("</tbody>\n");
        }
        outs->write("</table>\n");
    } else if (auto* row = block->var.as<Block::TableRow>()) {
        outs->write("<tr>\n");
        for (const Block* cell : row->childBlocks) {
            convertToHtml(outs, cell, options);
        }
        outs->write("</tr>\n");
    } else if (auto* cell = block->var.as<Block::TableCell>()) {
        const Block* rowBlock = block->parent;
        const Block* tableBlock = rowBlock->parent;
        auto* table = tableBlock->var.as<Block::Table>();
        PLY_ASSERT(table && table->childBlocks && table->childBlocks[0]->var.is<Block::TableRow>());
        bool isHeader = table->childBlocks[0] == rowBlock;
        StringView tag = isHeader ? "th" : "td";
        outs->format("<{}", tag);

        u32 column = 0;
        auto* row = rowBlock->var.as<Block::TableRow>();
        while (column < row->childBlocks.numItems() && row->childBlocks[column] != block)
            column++;
        PLY_ASSERT(column < table->alignments.numItems());
        switch (table->alignments[column]) {
            case TableAlignment::Left:
                outs->write(" align=\"left\"");
                break;
            case TableAlignment::Center:
                outs->write(" align=\"center\"");
                break;
            case TableAlignment::Right:
                outs->write(" align=\"right\"");
                break;
            case TableAlignment::None:
                break;
        }
        outs->write('>');
        for (const Span* span : cell->spans) {
            convertSpanToHtml(outs, span, options);
        }
        outs->format("</{}>\n", tag);
    } else if (auto* heading = block->var.as<Block::Heading>()) {
        outs->format("<h{}", heading->level);
        if (heading->id) {
            if (options.childAnchors) {
                outs->format(" class=\"anchored\"><span class=\"anchor\" id=\"{:&}\">&nbsp;</span>", heading->id);
            } else {
                outs->format(" id=\"{:&}\">", heading->id);
            }
        } else {
            outs->write('>');
        }
        for (const Span* span : heading->spans) {
            convertSpanToHtml(outs, span, options);
        }
        outs->format("</h{}>\n", heading->level);
    } else if (auto* para = block->var.as<Block::Paragraph>()) {
        bool isInsideTight = false;
        if (block->parent && block->parent->var.is<Block::ListItem>()) {
            auto* grandparentList = block->parent->parent->var.as<Block::List>();
            isInsideTight = grandparentList && !grandparentList->isLoose;
        }
        if (!isInsideTight) {
            outs->write("<p>");
        }
        // A task checkbox replaces the marker at the start of the list item's first paragraph.
        auto* listItem = block->parent ? block->parent->var.as<Block::ListItem>() : nullptr;
        bool isFirstTaskParagraph = listItem && listItem->isTask && listItem->childBlocks &&
                                    listItem->childBlocks[0] == block;
        if (isFirstTaskParagraph) {
            if (listItem->isChecked) {
                outs->write("<input checked=\"\" disabled=\"\" type=\"checkbox\">");
            } else {
                outs->write("<input disabled=\"\" type=\"checkbox\">");
            }
        }
        for (const Span* span : para->spans) {
            convertSpanToHtml(outs, span, options);
        }
        if (!isInsideTight) {
            outs->write("</p>\n");
        }
    } else if (auto* indented = block->var.as<Block::IndentedCodeBlock>()) {
        outs->write("<pre><code>");
        for (const Span* span : indented->spans) {
            auto* text = span->var.as<Span::Text>();
            PLY_ASSERT(text);
            printXmlEscapedString(*outs, text->text);
        }
        outs->write("</code></pre>\n");
    } else if (auto* fenced = block->var.as<Block::FencedCodeBlock>()) {
        outs->write("<pre><code");
        if (fenced->infoString) {
            outs->format(" class=\"language-{:&}\"", fenced->infoString);
        }
        outs->write(">");
        for (const Span* span : fenced->spans) {
            auto* text = span->var.as<Span::Text>();
            PLY_ASSERT(text);
            printXmlEscapedString(*outs, text->text);
        }
        outs->write("</code></pre>\n");
    } else if (auto* html = block->var.as<Block::HTMLBlock>()) {
        if (html->tagFilter) {
            writeFilteredRawHTML(outs, html->text);
        } else {
            outs->write(html->text);
        }
    } else if (block->var.is<Block::ThematicBreak>()) {
        outs->write("<hr />\n");
    } else {
        PLY_ASSERT(0);
    }
}

} // namespace markdown
} // namespace ply
