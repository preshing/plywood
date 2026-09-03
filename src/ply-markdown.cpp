/*────────────────────────────────────────────────────────────────────┐
│                                                                     │
│     ____      Plywood C++ Runtime Library                           │
│    ╱   ╱╲     https://plywood.dev/                                  │
│   ╱___╱╭╮╲                                                          │
│    └──┴┴┴┘    Markdown Parser                                       │
│               Documentation: docs/high-level/markdown-parser.md     │
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
        DecodeResult result = decodeUnicode({this->curByte, this->endByte}, UnicodeType::UTF8);
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
    return decodeUnicode(text.substr(bytePos), UnicodeType::UTF8).point;
}

// Returns the UTF-8 codepoint immediately before bytePos, or the sentinel at the start of the input.
s32 getPreviousInlineCodepoint(StringView text, u32 bytePos) {
    if (bytePos == 0)
        return -1;
    u32 codepointPos = bytePos - 1;
    while (codepointPos > 0 && (u8(text[codepointPos]) & 0xc0) == 0x80) {
        codepointPos--;
    }
    return decodeUnicode(text.substr(codepointPos, bytePos - codepointPos), UnicodeType::UTF8).point;
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
        DecodeResult decoded = decodeUnicode(label.substr(i), UnicodeType::UTF8);
        s32 point = decoded.point;
        u32 numBytes = decoded.status == DecodeStatus::OK ? decoded.numBytes : 1;

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
        encodeUnicode(out, UnicodeType::UTF8, point);
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
// Generated from the HTML 5 named character reference table.
// Each name excludes the required trailing semicolon.
    {"AElig", "\xC3\x86"},
    {"AMP", "\x26"},
    {"Aacute", "\xC3\x81"},
    {"Abreve", "\xC4\x82"},
    {"Acirc", "\xC3\x82"},
    {"Acy", "\xD0\x90"},
    {"Afr", "\xF0\x9D\x94\x84"},
    {"Agrave", "\xC3\x80"},
    {"Alpha", "\xCE\x91"},
    {"Amacr", "\xC4\x80"},
    {"And", "\xE2\xA9\x93"},
    {"Aogon", "\xC4\x84"},
    {"Aopf", "\xF0\x9D\x94\xB8"},
    {"ApplyFunction", "\xE2\x81\xA1"},
    {"Aring", "\xC3\x85"},
    {"Ascr", "\xF0\x9D\x92\x9C"},
    {"Assign", "\xE2\x89\x94"},
    {"Atilde", "\xC3\x83"},
    {"Auml", "\xC3\x84"},
    {"Backslash", "\xE2\x88\x96"},
    {"Barv", "\xE2\xAB\xA7"},
    {"Barwed", "\xE2\x8C\x86"},
    {"Bcy", "\xD0\x91"},
    {"Because", "\xE2\x88\xB5"},
    {"Bernoullis", "\xE2\x84\xAC"},
    {"Beta", "\xCE\x92"},
    {"Bfr", "\xF0\x9D\x94\x85"},
    {"Bopf", "\xF0\x9D\x94\xB9"},
    {"Breve", "\xCB\x98"},
    {"Bscr", "\xE2\x84\xAC"},
    {"Bumpeq", "\xE2\x89\x8E"},
    {"CHcy", "\xD0\xA7"},
    {"COPY", "\xC2\xA9"},
    {"Cacute", "\xC4\x86"},
    {"Cap", "\xE2\x8B\x92"},
    {"CapitalDifferentialD", "\xE2\x85\x85"},
    {"Cayleys", "\xE2\x84\xAD"},
    {"Ccaron", "\xC4\x8C"},
    {"Ccedil", "\xC3\x87"},
    {"Ccirc", "\xC4\x88"},
    {"Cconint", "\xE2\x88\xB0"},
    {"Cdot", "\xC4\x8A"},
    {"Cedilla", "\xC2\xB8"},
    {"CenterDot", "\xC2\xB7"},
    {"Cfr", "\xE2\x84\xAD"},
    {"Chi", "\xCE\xA7"},
    {"CircleDot", "\xE2\x8A\x99"},
    {"CircleMinus", "\xE2\x8A\x96"},
    {"CirclePlus", "\xE2\x8A\x95"},
    {"CircleTimes", "\xE2\x8A\x97"},
    {"ClockwiseContourIntegral", "\xE2\x88\xB2"},
    {"CloseCurlyDoubleQuote", "\xE2\x80\x9D"},
    {"CloseCurlyQuote", "\xE2\x80\x99"},
    {"Colon", "\xE2\x88\xB7"},
    {"Colone", "\xE2\xA9\xB4"},
    {"Congruent", "\xE2\x89\xA1"},
    {"Conint", "\xE2\x88\xAF"},
    {"ContourIntegral", "\xE2\x88\xAE"},
    {"Copf", "\xE2\x84\x82"},
    {"Coproduct", "\xE2\x88\x90"},
    {"CounterClockwiseContourIntegral", "\xE2\x88\xB3"},
    {"Cross", "\xE2\xA8\xAF"},
    {"Cscr", "\xF0\x9D\x92\x9E"},
    {"Cup", "\xE2\x8B\x93"},
    {"CupCap", "\xE2\x89\x8D"},
    {"DD", "\xE2\x85\x85"},
    {"DDotrahd", "\xE2\xA4\x91"},
    {"DJcy", "\xD0\x82"},
    {"DScy", "\xD0\x85"},
    {"DZcy", "\xD0\x8F"},
    {"Dagger", "\xE2\x80\xA1"},
    {"Darr", "\xE2\x86\xA1"},
    {"Dashv", "\xE2\xAB\xA4"},
    {"Dcaron", "\xC4\x8E"},
    {"Dcy", "\xD0\x94"},
    {"Del", "\xE2\x88\x87"},
    {"Delta", "\xCE\x94"},
    {"Dfr", "\xF0\x9D\x94\x87"},
    {"DiacriticalAcute", "\xC2\xB4"},
    {"DiacriticalDot", "\xCB\x99"},
    {"DiacriticalDoubleAcute", "\xCB\x9D"},
    {"DiacriticalGrave", "\x60"},
    {"DiacriticalTilde", "\xCB\x9C"},
    {"Diamond", "\xE2\x8B\x84"},
    {"DifferentialD", "\xE2\x85\x86"},
    {"Dopf", "\xF0\x9D\x94\xBB"},
    {"Dot", "\xC2\xA8"},
    {"DotDot", "\xE2\x83\x9C"},
    {"DotEqual", "\xE2\x89\x90"},
    {"DoubleContourIntegral", "\xE2\x88\xAF"},
    {"DoubleDot", "\xC2\xA8"},
    {"DoubleDownArrow", "\xE2\x87\x93"},
    {"DoubleLeftArrow", "\xE2\x87\x90"},
    {"DoubleLeftRightArrow", "\xE2\x87\x94"},
    {"DoubleLeftTee", "\xE2\xAB\xA4"},
    {"DoubleLongLeftArrow", "\xE2\x9F\xB8"},
    {"DoubleLongLeftRightArrow", "\xE2\x9F\xBA"},
    {"DoubleLongRightArrow", "\xE2\x9F\xB9"},
    {"DoubleRightArrow", "\xE2\x87\x92"},
    {"DoubleRightTee", "\xE2\x8A\xA8"},
    {"DoubleUpArrow", "\xE2\x87\x91"},
    {"DoubleUpDownArrow", "\xE2\x87\x95"},
    {"DoubleVerticalBar", "\xE2\x88\xA5"},
    {"DownArrow", "\xE2\x86\x93"},
    {"DownArrowBar", "\xE2\xA4\x93"},
    {"DownArrowUpArrow", "\xE2\x87\xB5"},
    {"DownBreve", "\xCC\x91"},
    {"DownLeftRightVector", "\xE2\xA5\x90"},
    {"DownLeftTeeVector", "\xE2\xA5\x9E"},
    {"DownLeftVector", "\xE2\x86\xBD"},
    {"DownLeftVectorBar", "\xE2\xA5\x96"},
    {"DownRightTeeVector", "\xE2\xA5\x9F"},
    {"DownRightVector", "\xE2\x87\x81"},
    {"DownRightVectorBar", "\xE2\xA5\x97"},
    {"DownTee", "\xE2\x8A\xA4"},
    {"DownTeeArrow", "\xE2\x86\xA7"},
    {"Downarrow", "\xE2\x87\x93"},
    {"Dscr", "\xF0\x9D\x92\x9F"},
    {"Dstrok", "\xC4\x90"},
    {"ENG", "\xC5\x8A"},
    {"ETH", "\xC3\x90"},
    {"Eacute", "\xC3\x89"},
    {"Ecaron", "\xC4\x9A"},
    {"Ecirc", "\xC3\x8A"},
    {"Ecy", "\xD0\xAD"},
    {"Edot", "\xC4\x96"},
    {"Efr", "\xF0\x9D\x94\x88"},
    {"Egrave", "\xC3\x88"},
    {"Element", "\xE2\x88\x88"},
    {"Emacr", "\xC4\x92"},
    {"EmptySmallSquare", "\xE2\x97\xBB"},
    {"EmptyVerySmallSquare", "\xE2\x96\xAB"},
    {"Eogon", "\xC4\x98"},
    {"Eopf", "\xF0\x9D\x94\xBC"},
    {"Epsilon", "\xCE\x95"},
    {"Equal", "\xE2\xA9\xB5"},
    {"EqualTilde", "\xE2\x89\x82"},
    {"Equilibrium", "\xE2\x87\x8C"},
    {"Escr", "\xE2\x84\xB0"},
    {"Esim", "\xE2\xA9\xB3"},
    {"Eta", "\xCE\x97"},
    {"Euml", "\xC3\x8B"},
    {"Exists", "\xE2\x88\x83"},
    {"ExponentialE", "\xE2\x85\x87"},
    {"Fcy", "\xD0\xA4"},
    {"Ffr", "\xF0\x9D\x94\x89"},
    {"FilledSmallSquare", "\xE2\x97\xBC"},
    {"FilledVerySmallSquare", "\xE2\x96\xAA"},
    {"Fopf", "\xF0\x9D\x94\xBD"},
    {"ForAll", "\xE2\x88\x80"},
    {"Fouriertrf", "\xE2\x84\xB1"},
    {"Fscr", "\xE2\x84\xB1"},
    {"GJcy", "\xD0\x83"},
    {"GT", "\x3E"},
    {"Gamma", "\xCE\x93"},
    {"Gammad", "\xCF\x9C"},
    {"Gbreve", "\xC4\x9E"},
    {"Gcedil", "\xC4\xA2"},
    {"Gcirc", "\xC4\x9C"},
    {"Gcy", "\xD0\x93"},
    {"Gdot", "\xC4\xA0"},
    {"Gfr", "\xF0\x9D\x94\x8A"},
    {"Gg", "\xE2\x8B\x99"},
    {"Gopf", "\xF0\x9D\x94\xBE"},
    {"GreaterEqual", "\xE2\x89\xA5"},
    {"GreaterEqualLess", "\xE2\x8B\x9B"},
    {"GreaterFullEqual", "\xE2\x89\xA7"},
    {"GreaterGreater", "\xE2\xAA\xA2"},
    {"GreaterLess", "\xE2\x89\xB7"},
    {"GreaterSlantEqual", "\xE2\xA9\xBE"},
    {"GreaterTilde", "\xE2\x89\xB3"},
    {"Gscr", "\xF0\x9D\x92\xA2"},
    {"Gt", "\xE2\x89\xAB"},
    {"HARDcy", "\xD0\xAA"},
    {"Hacek", "\xCB\x87"},
    {"Hat", "\x5E"},
    {"Hcirc", "\xC4\xA4"},
    {"Hfr", "\xE2\x84\x8C"},
    {"HilbertSpace", "\xE2\x84\x8B"},
    {"Hopf", "\xE2\x84\x8D"},
    {"HorizontalLine", "\xE2\x94\x80"},
    {"Hscr", "\xE2\x84\x8B"},
    {"Hstrok", "\xC4\xA6"},
    {"HumpDownHump", "\xE2\x89\x8E"},
    {"HumpEqual", "\xE2\x89\x8F"},
    {"IEcy", "\xD0\x95"},
    {"IJlig", "\xC4\xB2"},
    {"IOcy", "\xD0\x81"},
    {"Iacute", "\xC3\x8D"},
    {"Icirc", "\xC3\x8E"},
    {"Icy", "\xD0\x98"},
    {"Idot", "\xC4\xB0"},
    {"Ifr", "\xE2\x84\x91"},
    {"Igrave", "\xC3\x8C"},
    {"Im", "\xE2\x84\x91"},
    {"Imacr", "\xC4\xAA"},
    {"ImaginaryI", "\xE2\x85\x88"},
    {"Implies", "\xE2\x87\x92"},
    {"Int", "\xE2\x88\xAC"},
    {"Integral", "\xE2\x88\xAB"},
    {"Intersection", "\xE2\x8B\x82"},
    {"InvisibleComma", "\xE2\x81\xA3"},
    {"InvisibleTimes", "\xE2\x81\xA2"},
    {"Iogon", "\xC4\xAE"},
    {"Iopf", "\xF0\x9D\x95\x80"},
    {"Iota", "\xCE\x99"},
    {"Iscr", "\xE2\x84\x90"},
    {"Itilde", "\xC4\xA8"},
    {"Iukcy", "\xD0\x86"},
    {"Iuml", "\xC3\x8F"},
    {"Jcirc", "\xC4\xB4"},
    {"Jcy", "\xD0\x99"},
    {"Jfr", "\xF0\x9D\x94\x8D"},
    {"Jopf", "\xF0\x9D\x95\x81"},
    {"Jscr", "\xF0\x9D\x92\xA5"},
    {"Jsercy", "\xD0\x88"},
    {"Jukcy", "\xD0\x84"},
    {"KHcy", "\xD0\xA5"},
    {"KJcy", "\xD0\x8C"},
    {"Kappa", "\xCE\x9A"},
    {"Kcedil", "\xC4\xB6"},
    {"Kcy", "\xD0\x9A"},
    {"Kfr", "\xF0\x9D\x94\x8E"},
    {"Kopf", "\xF0\x9D\x95\x82"},
    {"Kscr", "\xF0\x9D\x92\xA6"},
    {"LJcy", "\xD0\x89"},
    {"LT", "\x3C"},
    {"Lacute", "\xC4\xB9"},
    {"Lambda", "\xCE\x9B"},
    {"Lang", "\xE2\x9F\xAA"},
    {"Laplacetrf", "\xE2\x84\x92"},
    {"Larr", "\xE2\x86\x9E"},
    {"Lcaron", "\xC4\xBD"},
    {"Lcedil", "\xC4\xBB"},
    {"Lcy", "\xD0\x9B"},
    {"LeftAngleBracket", "\xE2\x9F\xA8"},
    {"LeftArrow", "\xE2\x86\x90"},
    {"LeftArrowBar", "\xE2\x87\xA4"},
    {"LeftArrowRightArrow", "\xE2\x87\x86"},
    {"LeftCeiling", "\xE2\x8C\x88"},
    {"LeftDoubleBracket", "\xE2\x9F\xA6"},
    {"LeftDownTeeVector", "\xE2\xA5\xA1"},
    {"LeftDownVector", "\xE2\x87\x83"},
    {"LeftDownVectorBar", "\xE2\xA5\x99"},
    {"LeftFloor", "\xE2\x8C\x8A"},
    {"LeftRightArrow", "\xE2\x86\x94"},
    {"LeftRightVector", "\xE2\xA5\x8E"},
    {"LeftTee", "\xE2\x8A\xA3"},
    {"LeftTeeArrow", "\xE2\x86\xA4"},
    {"LeftTeeVector", "\xE2\xA5\x9A"},
    {"LeftTriangle", "\xE2\x8A\xB2"},
    {"LeftTriangleBar", "\xE2\xA7\x8F"},
    {"LeftTriangleEqual", "\xE2\x8A\xB4"},
    {"LeftUpDownVector", "\xE2\xA5\x91"},
    {"LeftUpTeeVector", "\xE2\xA5\xA0"},
    {"LeftUpVector", "\xE2\x86\xBF"},
    {"LeftUpVectorBar", "\xE2\xA5\x98"},
    {"LeftVector", "\xE2\x86\xBC"},
    {"LeftVectorBar", "\xE2\xA5\x92"},
    {"Leftarrow", "\xE2\x87\x90"},
    {"Leftrightarrow", "\xE2\x87\x94"},
    {"LessEqualGreater", "\xE2\x8B\x9A"},
    {"LessFullEqual", "\xE2\x89\xA6"},
    {"LessGreater", "\xE2\x89\xB6"},
    {"LessLess", "\xE2\xAA\xA1"},
    {"LessSlantEqual", "\xE2\xA9\xBD"},
    {"LessTilde", "\xE2\x89\xB2"},
    {"Lfr", "\xF0\x9D\x94\x8F"},
    {"Ll", "\xE2\x8B\x98"},
    {"Lleftarrow", "\xE2\x87\x9A"},
    {"Lmidot", "\xC4\xBF"},
    {"LongLeftArrow", "\xE2\x9F\xB5"},
    {"LongLeftRightArrow", "\xE2\x9F\xB7"},
    {"LongRightArrow", "\xE2\x9F\xB6"},
    {"Longleftarrow", "\xE2\x9F\xB8"},
    {"Longleftrightarrow", "\xE2\x9F\xBA"},
    {"Longrightarrow", "\xE2\x9F\xB9"},
    {"Lopf", "\xF0\x9D\x95\x83"},
    {"LowerLeftArrow", "\xE2\x86\x99"},
    {"LowerRightArrow", "\xE2\x86\x98"},
    {"Lscr", "\xE2\x84\x92"},
    {"Lsh", "\xE2\x86\xB0"},
    {"Lstrok", "\xC5\x81"},
    {"Lt", "\xE2\x89\xAA"},
    {"Map", "\xE2\xA4\x85"},
    {"Mcy", "\xD0\x9C"},
    {"MediumSpace", "\xE2\x81\x9F"},
    {"Mellintrf", "\xE2\x84\xB3"},
    {"Mfr", "\xF0\x9D\x94\x90"},
    {"MinusPlus", "\xE2\x88\x93"},
    {"Mopf", "\xF0\x9D\x95\x84"},
    {"Mscr", "\xE2\x84\xB3"},
    {"Mu", "\xCE\x9C"},
    {"NJcy", "\xD0\x8A"},
    {"Nacute", "\xC5\x83"},
    {"Ncaron", "\xC5\x87"},
    {"Ncedil", "\xC5\x85"},
    {"Ncy", "\xD0\x9D"},
    {"NegativeMediumSpace", "\xE2\x80\x8B"},
    {"NegativeThickSpace", "\xE2\x80\x8B"},
    {"NegativeThinSpace", "\xE2\x80\x8B"},
    {"NegativeVeryThinSpace", "\xE2\x80\x8B"},
    {"NestedGreaterGreater", "\xE2\x89\xAB"},
    {"NestedLessLess", "\xE2\x89\xAA"},
    {"NewLine", "\x0A"},
    {"Nfr", "\xF0\x9D\x94\x91"},
    {"NoBreak", "\xE2\x81\xA0"},
    {"NonBreakingSpace", "\xC2\xA0"},
    {"Nopf", "\xE2\x84\x95"},
    {"Not", "\xE2\xAB\xAC"},
    {"NotCongruent", "\xE2\x89\xA2"},
    {"NotCupCap", "\xE2\x89\xAD"},
    {"NotDoubleVerticalBar", "\xE2\x88\xA6"},
    {"NotElement", "\xE2\x88\x89"},
    {"NotEqual", "\xE2\x89\xA0"},
    {"NotEqualTilde", "\xE2\x89\x82\xCC\xB8"},
    {"NotExists", "\xE2\x88\x84"},
    {"NotGreater", "\xE2\x89\xAF"},
    {"NotGreaterEqual", "\xE2\x89\xB1"},
    {"NotGreaterFullEqual", "\xE2\x89\xA7\xCC\xB8"},
    {"NotGreaterGreater", "\xE2\x89\xAB\xCC\xB8"},
    {"NotGreaterLess", "\xE2\x89\xB9"},
    {"NotGreaterSlantEqual", "\xE2\xA9\xBE\xCC\xB8"},
    {"NotGreaterTilde", "\xE2\x89\xB5"},
    {"NotHumpDownHump", "\xE2\x89\x8E\xCC\xB8"},
    {"NotHumpEqual", "\xE2\x89\x8F\xCC\xB8"},
    {"NotLeftTriangle", "\xE2\x8B\xAA"},
    {"NotLeftTriangleBar", "\xE2\xA7\x8F\xCC\xB8"},
    {"NotLeftTriangleEqual", "\xE2\x8B\xAC"},
    {"NotLess", "\xE2\x89\xAE"},
    {"NotLessEqual", "\xE2\x89\xB0"},
    {"NotLessGreater", "\xE2\x89\xB8"},
    {"NotLessLess", "\xE2\x89\xAA\xCC\xB8"},
    {"NotLessSlantEqual", "\xE2\xA9\xBD\xCC\xB8"},
    {"NotLessTilde", "\xE2\x89\xB4"},
    {"NotNestedGreaterGreater", "\xE2\xAA\xA2\xCC\xB8"},
    {"NotNestedLessLess", "\xE2\xAA\xA1\xCC\xB8"},
    {"NotPrecedes", "\xE2\x8A\x80"},
    {"NotPrecedesEqual", "\xE2\xAA\xAF\xCC\xB8"},
    {"NotPrecedesSlantEqual", "\xE2\x8B\xA0"},
    {"NotReverseElement", "\xE2\x88\x8C"},
    {"NotRightTriangle", "\xE2\x8B\xAB"},
    {"NotRightTriangleBar", "\xE2\xA7\x90\xCC\xB8"},
    {"NotRightTriangleEqual", "\xE2\x8B\xAD"},
    {"NotSquareSubset", "\xE2\x8A\x8F\xCC\xB8"},
    {"NotSquareSubsetEqual", "\xE2\x8B\xA2"},
    {"NotSquareSuperset", "\xE2\x8A\x90\xCC\xB8"},
    {"NotSquareSupersetEqual", "\xE2\x8B\xA3"},
    {"NotSubset", "\xE2\x8A\x82\xE2\x83\x92"},
    {"NotSubsetEqual", "\xE2\x8A\x88"},
    {"NotSucceeds", "\xE2\x8A\x81"},
    {"NotSucceedsEqual", "\xE2\xAA\xB0\xCC\xB8"},
    {"NotSucceedsSlantEqual", "\xE2\x8B\xA1"},
    {"NotSucceedsTilde", "\xE2\x89\xBF\xCC\xB8"},
    {"NotSuperset", "\xE2\x8A\x83\xE2\x83\x92"},
    {"NotSupersetEqual", "\xE2\x8A\x89"},
    {"NotTilde", "\xE2\x89\x81"},
    {"NotTildeEqual", "\xE2\x89\x84"},
    {"NotTildeFullEqual", "\xE2\x89\x87"},
    {"NotTildeTilde", "\xE2\x89\x89"},
    {"NotVerticalBar", "\xE2\x88\xA4"},
    {"Nscr", "\xF0\x9D\x92\xA9"},
    {"Ntilde", "\xC3\x91"},
    {"Nu", "\xCE\x9D"},
    {"OElig", "\xC5\x92"},
    {"Oacute", "\xC3\x93"},
    {"Ocirc", "\xC3\x94"},
    {"Ocy", "\xD0\x9E"},
    {"Odblac", "\xC5\x90"},
    {"Ofr", "\xF0\x9D\x94\x92"},
    {"Ograve", "\xC3\x92"},
    {"Omacr", "\xC5\x8C"},
    {"Omega", "\xCE\xA9"},
    {"Omicron", "\xCE\x9F"},
    {"Oopf", "\xF0\x9D\x95\x86"},
    {"OpenCurlyDoubleQuote", "\xE2\x80\x9C"},
    {"OpenCurlyQuote", "\xE2\x80\x98"},
    {"Or", "\xE2\xA9\x94"},
    {"Oscr", "\xF0\x9D\x92\xAA"},
    {"Oslash", "\xC3\x98"},
    {"Otilde", "\xC3\x95"},
    {"Otimes", "\xE2\xA8\xB7"},
    {"Ouml", "\xC3\x96"},
    {"OverBar", "\xE2\x80\xBE"},
    {"OverBrace", "\xE2\x8F\x9E"},
    {"OverBracket", "\xE2\x8E\xB4"},
    {"OverParenthesis", "\xE2\x8F\x9C"},
    {"PartialD", "\xE2\x88\x82"},
    {"Pcy", "\xD0\x9F"},
    {"Pfr", "\xF0\x9D\x94\x93"},
    {"Phi", "\xCE\xA6"},
    {"Pi", "\xCE\xA0"},
    {"PlusMinus", "\xC2\xB1"},
    {"Poincareplane", "\xE2\x84\x8C"},
    {"Popf", "\xE2\x84\x99"},
    {"Pr", "\xE2\xAA\xBB"},
    {"Precedes", "\xE2\x89\xBA"},
    {"PrecedesEqual", "\xE2\xAA\xAF"},
    {"PrecedesSlantEqual", "\xE2\x89\xBC"},
    {"PrecedesTilde", "\xE2\x89\xBE"},
    {"Prime", "\xE2\x80\xB3"},
    {"Product", "\xE2\x88\x8F"},
    {"Proportion", "\xE2\x88\xB7"},
    {"Proportional", "\xE2\x88\x9D"},
    {"Pscr", "\xF0\x9D\x92\xAB"},
    {"Psi", "\xCE\xA8"},
    {"QUOT", "\x22"},
    {"Qfr", "\xF0\x9D\x94\x94"},
    {"Qopf", "\xE2\x84\x9A"},
    {"Qscr", "\xF0\x9D\x92\xAC"},
    {"RBarr", "\xE2\xA4\x90"},
    {"REG", "\xC2\xAE"},
    {"Racute", "\xC5\x94"},
    {"Rang", "\xE2\x9F\xAB"},
    {"Rarr", "\xE2\x86\xA0"},
    {"Rarrtl", "\xE2\xA4\x96"},
    {"Rcaron", "\xC5\x98"},
    {"Rcedil", "\xC5\x96"},
    {"Rcy", "\xD0\xA0"},
    {"Re", "\xE2\x84\x9C"},
    {"ReverseElement", "\xE2\x88\x8B"},
    {"ReverseEquilibrium", "\xE2\x87\x8B"},
    {"ReverseUpEquilibrium", "\xE2\xA5\xAF"},
    {"Rfr", "\xE2\x84\x9C"},
    {"Rho", "\xCE\xA1"},
    {"RightAngleBracket", "\xE2\x9F\xA9"},
    {"RightArrow", "\xE2\x86\x92"},
    {"RightArrowBar", "\xE2\x87\xA5"},
    {"RightArrowLeftArrow", "\xE2\x87\x84"},
    {"RightCeiling", "\xE2\x8C\x89"},
    {"RightDoubleBracket", "\xE2\x9F\xA7"},
    {"RightDownTeeVector", "\xE2\xA5\x9D"},
    {"RightDownVector", "\xE2\x87\x82"},
    {"RightDownVectorBar", "\xE2\xA5\x95"},
    {"RightFloor", "\xE2\x8C\x8B"},
    {"RightTee", "\xE2\x8A\xA2"},
    {"RightTeeArrow", "\xE2\x86\xA6"},
    {"RightTeeVector", "\xE2\xA5\x9B"},
    {"RightTriangle", "\xE2\x8A\xB3"},
    {"RightTriangleBar", "\xE2\xA7\x90"},
    {"RightTriangleEqual", "\xE2\x8A\xB5"},
    {"RightUpDownVector", "\xE2\xA5\x8F"},
    {"RightUpTeeVector", "\xE2\xA5\x9C"},
    {"RightUpVector", "\xE2\x86\xBE"},
    {"RightUpVectorBar", "\xE2\xA5\x94"},
    {"RightVector", "\xE2\x87\x80"},
    {"RightVectorBar", "\xE2\xA5\x93"},
    {"Rightarrow", "\xE2\x87\x92"},
    {"Ropf", "\xE2\x84\x9D"},
    {"RoundImplies", "\xE2\xA5\xB0"},
    {"Rrightarrow", "\xE2\x87\x9B"},
    {"Rscr", "\xE2\x84\x9B"},
    {"Rsh", "\xE2\x86\xB1"},
    {"RuleDelayed", "\xE2\xA7\xB4"},
    {"SHCHcy", "\xD0\xA9"},
    {"SHcy", "\xD0\xA8"},
    {"SOFTcy", "\xD0\xAC"},
    {"Sacute", "\xC5\x9A"},
    {"Sc", "\xE2\xAA\xBC"},
    {"Scaron", "\xC5\xA0"},
    {"Scedil", "\xC5\x9E"},
    {"Scirc", "\xC5\x9C"},
    {"Scy", "\xD0\xA1"},
    {"Sfr", "\xF0\x9D\x94\x96"},
    {"ShortDownArrow", "\xE2\x86\x93"},
    {"ShortLeftArrow", "\xE2\x86\x90"},
    {"ShortRightArrow", "\xE2\x86\x92"},
    {"ShortUpArrow", "\xE2\x86\x91"},
    {"Sigma", "\xCE\xA3"},
    {"SmallCircle", "\xE2\x88\x98"},
    {"Sopf", "\xF0\x9D\x95\x8A"},
    {"Sqrt", "\xE2\x88\x9A"},
    {"Square", "\xE2\x96\xA1"},
    {"SquareIntersection", "\xE2\x8A\x93"},
    {"SquareSubset", "\xE2\x8A\x8F"},
    {"SquareSubsetEqual", "\xE2\x8A\x91"},
    {"SquareSuperset", "\xE2\x8A\x90"},
    {"SquareSupersetEqual", "\xE2\x8A\x92"},
    {"SquareUnion", "\xE2\x8A\x94"},
    {"Sscr", "\xF0\x9D\x92\xAE"},
    {"Star", "\xE2\x8B\x86"},
    {"Sub", "\xE2\x8B\x90"},
    {"Subset", "\xE2\x8B\x90"},
    {"SubsetEqual", "\xE2\x8A\x86"},
    {"Succeeds", "\xE2\x89\xBB"},
    {"SucceedsEqual", "\xE2\xAA\xB0"},
    {"SucceedsSlantEqual", "\xE2\x89\xBD"},
    {"SucceedsTilde", "\xE2\x89\xBF"},
    {"SuchThat", "\xE2\x88\x8B"},
    {"Sum", "\xE2\x88\x91"},
    {"Sup", "\xE2\x8B\x91"},
    {"Superset", "\xE2\x8A\x83"},
    {"SupersetEqual", "\xE2\x8A\x87"},
    {"Supset", "\xE2\x8B\x91"},
    {"THORN", "\xC3\x9E"},
    {"TRADE", "\xE2\x84\xA2"},
    {"TSHcy", "\xD0\x8B"},
    {"TScy", "\xD0\xA6"},
    {"Tab", "\x09"},
    {"Tau", "\xCE\xA4"},
    {"Tcaron", "\xC5\xA4"},
    {"Tcedil", "\xC5\xA2"},
    {"Tcy", "\xD0\xA2"},
    {"Tfr", "\xF0\x9D\x94\x97"},
    {"Therefore", "\xE2\x88\xB4"},
    {"Theta", "\xCE\x98"},
    {"ThickSpace", "\xE2\x81\x9F\xE2\x80\x8A"},
    {"ThinSpace", "\xE2\x80\x89"},
    {"Tilde", "\xE2\x88\xBC"},
    {"TildeEqual", "\xE2\x89\x83"},
    {"TildeFullEqual", "\xE2\x89\x85"},
    {"TildeTilde", "\xE2\x89\x88"},
    {"Topf", "\xF0\x9D\x95\x8B"},
    {"TripleDot", "\xE2\x83\x9B"},
    {"Tscr", "\xF0\x9D\x92\xAF"},
    {"Tstrok", "\xC5\xA6"},
    {"Uacute", "\xC3\x9A"},
    {"Uarr", "\xE2\x86\x9F"},
    {"Uarrocir", "\xE2\xA5\x89"},
    {"Ubrcy", "\xD0\x8E"},
    {"Ubreve", "\xC5\xAC"},
    {"Ucirc", "\xC3\x9B"},
    {"Ucy", "\xD0\xA3"},
    {"Udblac", "\xC5\xB0"},
    {"Ufr", "\xF0\x9D\x94\x98"},
    {"Ugrave", "\xC3\x99"},
    {"Umacr", "\xC5\xAA"},
    {"UnderBar", "\x5F"},
    {"UnderBrace", "\xE2\x8F\x9F"},
    {"UnderBracket", "\xE2\x8E\xB5"},
    {"UnderParenthesis", "\xE2\x8F\x9D"},
    {"Union", "\xE2\x8B\x83"},
    {"UnionPlus", "\xE2\x8A\x8E"},
    {"Uogon", "\xC5\xB2"},
    {"Uopf", "\xF0\x9D\x95\x8C"},
    {"UpArrow", "\xE2\x86\x91"},
    {"UpArrowBar", "\xE2\xA4\x92"},
    {"UpArrowDownArrow", "\xE2\x87\x85"},
    {"UpDownArrow", "\xE2\x86\x95"},
    {"UpEquilibrium", "\xE2\xA5\xAE"},
    {"UpTee", "\xE2\x8A\xA5"},
    {"UpTeeArrow", "\xE2\x86\xA5"},
    {"Uparrow", "\xE2\x87\x91"},
    {"Updownarrow", "\xE2\x87\x95"},
    {"UpperLeftArrow", "\xE2\x86\x96"},
    {"UpperRightArrow", "\xE2\x86\x97"},
    {"Upsi", "\xCF\x92"},
    {"Upsilon", "\xCE\xA5"},
    {"Uring", "\xC5\xAE"},
    {"Uscr", "\xF0\x9D\x92\xB0"},
    {"Utilde", "\xC5\xA8"},
    {"Uuml", "\xC3\x9C"},
    {"VDash", "\xE2\x8A\xAB"},
    {"Vbar", "\xE2\xAB\xAB"},
    {"Vcy", "\xD0\x92"},
    {"Vdash", "\xE2\x8A\xA9"},
    {"Vdashl", "\xE2\xAB\xA6"},
    {"Vee", "\xE2\x8B\x81"},
    {"Verbar", "\xE2\x80\x96"},
    {"Vert", "\xE2\x80\x96"},
    {"VerticalBar", "\xE2\x88\xA3"},
    {"VerticalLine", "\x7C"},
    {"VerticalSeparator", "\xE2\x9D\x98"},
    {"VerticalTilde", "\xE2\x89\x80"},
    {"VeryThinSpace", "\xE2\x80\x8A"},
    {"Vfr", "\xF0\x9D\x94\x99"},
    {"Vopf", "\xF0\x9D\x95\x8D"},
    {"Vscr", "\xF0\x9D\x92\xB1"},
    {"Vvdash", "\xE2\x8A\xAA"},
    {"Wcirc", "\xC5\xB4"},
    {"Wedge", "\xE2\x8B\x80"},
    {"Wfr", "\xF0\x9D\x94\x9A"},
    {"Wopf", "\xF0\x9D\x95\x8E"},
    {"Wscr", "\xF0\x9D\x92\xB2"},
    {"Xfr", "\xF0\x9D\x94\x9B"},
    {"Xi", "\xCE\x9E"},
    {"Xopf", "\xF0\x9D\x95\x8F"},
    {"Xscr", "\xF0\x9D\x92\xB3"},
    {"YAcy", "\xD0\xAF"},
    {"YIcy", "\xD0\x87"},
    {"YUcy", "\xD0\xAE"},
    {"Yacute", "\xC3\x9D"},
    {"Ycirc", "\xC5\xB6"},
    {"Ycy", "\xD0\xAB"},
    {"Yfr", "\xF0\x9D\x94\x9C"},
    {"Yopf", "\xF0\x9D\x95\x90"},
    {"Yscr", "\xF0\x9D\x92\xB4"},
    {"Yuml", "\xC5\xB8"},
    {"ZHcy", "\xD0\x96"},
    {"Zacute", "\xC5\xB9"},
    {"Zcaron", "\xC5\xBD"},
    {"Zcy", "\xD0\x97"},
    {"Zdot", "\xC5\xBB"},
    {"ZeroWidthSpace", "\xE2\x80\x8B"},
    {"Zeta", "\xCE\x96"},
    {"Zfr", "\xE2\x84\xA8"},
    {"Zopf", "\xE2\x84\xA4"},
    {"Zscr", "\xF0\x9D\x92\xB5"},
    {"aacute", "\xC3\xA1"},
    {"abreve", "\xC4\x83"},
    {"ac", "\xE2\x88\xBE"},
    {"acE", "\xE2\x88\xBE\xCC\xB3"},
    {"acd", "\xE2\x88\xBF"},
    {"acirc", "\xC3\xA2"},
    {"acute", "\xC2\xB4"},
    {"acy", "\xD0\xB0"},
    {"aelig", "\xC3\xA6"},
    {"af", "\xE2\x81\xA1"},
    {"afr", "\xF0\x9D\x94\x9E"},
    {"agrave", "\xC3\xA0"},
    {"alefsym", "\xE2\x84\xB5"},
    {"aleph", "\xE2\x84\xB5"},
    {"alpha", "\xCE\xB1"},
    {"amacr", "\xC4\x81"},
    {"amalg", "\xE2\xA8\xBF"},
    {"amp", "\x26"},
    {"and", "\xE2\x88\xA7"},
    {"andand", "\xE2\xA9\x95"},
    {"andd", "\xE2\xA9\x9C"},
    {"andslope", "\xE2\xA9\x98"},
    {"andv", "\xE2\xA9\x9A"},
    {"ang", "\xE2\x88\xA0"},
    {"ange", "\xE2\xA6\xA4"},
    {"angle", "\xE2\x88\xA0"},
    {"angmsd", "\xE2\x88\xA1"},
    {"angmsdaa", "\xE2\xA6\xA8"},
    {"angmsdab", "\xE2\xA6\xA9"},
    {"angmsdac", "\xE2\xA6\xAA"},
    {"angmsdad", "\xE2\xA6\xAB"},
    {"angmsdae", "\xE2\xA6\xAC"},
    {"angmsdaf", "\xE2\xA6\xAD"},
    {"angmsdag", "\xE2\xA6\xAE"},
    {"angmsdah", "\xE2\xA6\xAF"},
    {"angrt", "\xE2\x88\x9F"},
    {"angrtvb", "\xE2\x8A\xBE"},
    {"angrtvbd", "\xE2\xA6\x9D"},
    {"angsph", "\xE2\x88\xA2"},
    {"angst", "\xC3\x85"},
    {"angzarr", "\xE2\x8D\xBC"},
    {"aogon", "\xC4\x85"},
    {"aopf", "\xF0\x9D\x95\x92"},
    {"ap", "\xE2\x89\x88"},
    {"apE", "\xE2\xA9\xB0"},
    {"apacir", "\xE2\xA9\xAF"},
    {"ape", "\xE2\x89\x8A"},
    {"apid", "\xE2\x89\x8B"},
    {"apos", "\x27"},
    {"approx", "\xE2\x89\x88"},
    {"approxeq", "\xE2\x89\x8A"},
    {"aring", "\xC3\xA5"},
    {"ascr", "\xF0\x9D\x92\xB6"},
    {"ast", "\x2A"},
    {"asymp", "\xE2\x89\x88"},
    {"asympeq", "\xE2\x89\x8D"},
    {"atilde", "\xC3\xA3"},
    {"auml", "\xC3\xA4"},
    {"awconint", "\xE2\x88\xB3"},
    {"awint", "\xE2\xA8\x91"},
    {"bNot", "\xE2\xAB\xAD"},
    {"backcong", "\xE2\x89\x8C"},
    {"backepsilon", "\xCF\xB6"},
    {"backprime", "\xE2\x80\xB5"},
    {"backsim", "\xE2\x88\xBD"},
    {"backsimeq", "\xE2\x8B\x8D"},
    {"barvee", "\xE2\x8A\xBD"},
    {"barwed", "\xE2\x8C\x85"},
    {"barwedge", "\xE2\x8C\x85"},
    {"bbrk", "\xE2\x8E\xB5"},
    {"bbrktbrk", "\xE2\x8E\xB6"},
    {"bcong", "\xE2\x89\x8C"},
    {"bcy", "\xD0\xB1"},
    {"bdquo", "\xE2\x80\x9E"},
    {"becaus", "\xE2\x88\xB5"},
    {"because", "\xE2\x88\xB5"},
    {"bemptyv", "\xE2\xA6\xB0"},
    {"bepsi", "\xCF\xB6"},
    {"bernou", "\xE2\x84\xAC"},
    {"beta", "\xCE\xB2"},
    {"beth", "\xE2\x84\xB6"},
    {"between", "\xE2\x89\xAC"},
    {"bfr", "\xF0\x9D\x94\x9F"},
    {"bigcap", "\xE2\x8B\x82"},
    {"bigcirc", "\xE2\x97\xAF"},
    {"bigcup", "\xE2\x8B\x83"},
    {"bigodot", "\xE2\xA8\x80"},
    {"bigoplus", "\xE2\xA8\x81"},
    {"bigotimes", "\xE2\xA8\x82"},
    {"bigsqcup", "\xE2\xA8\x86"},
    {"bigstar", "\xE2\x98\x85"},
    {"bigtriangledown", "\xE2\x96\xBD"},
    {"bigtriangleup", "\xE2\x96\xB3"},
    {"biguplus", "\xE2\xA8\x84"},
    {"bigvee", "\xE2\x8B\x81"},
    {"bigwedge", "\xE2\x8B\x80"},
    {"bkarow", "\xE2\xA4\x8D"},
    {"blacklozenge", "\xE2\xA7\xAB"},
    {"blacksquare", "\xE2\x96\xAA"},
    {"blacktriangle", "\xE2\x96\xB4"},
    {"blacktriangledown", "\xE2\x96\xBE"},
    {"blacktriangleleft", "\xE2\x97\x82"},
    {"blacktriangleright", "\xE2\x96\xB8"},
    {"blank", "\xE2\x90\xA3"},
    {"blk12", "\xE2\x96\x92"},
    {"blk14", "\xE2\x96\x91"},
    {"blk34", "\xE2\x96\x93"},
    {"block", "\xE2\x96\x88"},
    {"bne", "\x3D\xE2\x83\xA5"},
    {"bnequiv", "\xE2\x89\xA1\xE2\x83\xA5"},
    {"bnot", "\xE2\x8C\x90"},
    {"bopf", "\xF0\x9D\x95\x93"},
    {"bot", "\xE2\x8A\xA5"},
    {"bottom", "\xE2\x8A\xA5"},
    {"bowtie", "\xE2\x8B\x88"},
    {"boxDL", "\xE2\x95\x97"},
    {"boxDR", "\xE2\x95\x94"},
    {"boxDl", "\xE2\x95\x96"},
    {"boxDr", "\xE2\x95\x93"},
    {"boxH", "\xE2\x95\x90"},
    {"boxHD", "\xE2\x95\xA6"},
    {"boxHU", "\xE2\x95\xA9"},
    {"boxHd", "\xE2\x95\xA4"},
    {"boxHu", "\xE2\x95\xA7"},
    {"boxUL", "\xE2\x95\x9D"},
    {"boxUR", "\xE2\x95\x9A"},
    {"boxUl", "\xE2\x95\x9C"},
    {"boxUr", "\xE2\x95\x99"},
    {"boxV", "\xE2\x95\x91"},
    {"boxVH", "\xE2\x95\xAC"},
    {"boxVL", "\xE2\x95\xA3"},
    {"boxVR", "\xE2\x95\xA0"},
    {"boxVh", "\xE2\x95\xAB"},
    {"boxVl", "\xE2\x95\xA2"},
    {"boxVr", "\xE2\x95\x9F"},
    {"boxbox", "\xE2\xA7\x89"},
    {"boxdL", "\xE2\x95\x95"},
    {"boxdR", "\xE2\x95\x92"},
    {"boxdl", "\xE2\x94\x90"},
    {"boxdr", "\xE2\x94\x8C"},
    {"boxh", "\xE2\x94\x80"},
    {"boxhD", "\xE2\x95\xA5"},
    {"boxhU", "\xE2\x95\xA8"},
    {"boxhd", "\xE2\x94\xAC"},
    {"boxhu", "\xE2\x94\xB4"},
    {"boxminus", "\xE2\x8A\x9F"},
    {"boxplus", "\xE2\x8A\x9E"},
    {"boxtimes", "\xE2\x8A\xA0"},
    {"boxuL", "\xE2\x95\x9B"},
    {"boxuR", "\xE2\x95\x98"},
    {"boxul", "\xE2\x94\x98"},
    {"boxur", "\xE2\x94\x94"},
    {"boxv", "\xE2\x94\x82"},
    {"boxvH", "\xE2\x95\xAA"},
    {"boxvL", "\xE2\x95\xA1"},
    {"boxvR", "\xE2\x95\x9E"},
    {"boxvh", "\xE2\x94\xBC"},
    {"boxvl", "\xE2\x94\xA4"},
    {"boxvr", "\xE2\x94\x9C"},
    {"bprime", "\xE2\x80\xB5"},
    {"breve", "\xCB\x98"},
    {"brvbar", "\xC2\xA6"},
    {"bscr", "\xF0\x9D\x92\xB7"},
    {"bsemi", "\xE2\x81\x8F"},
    {"bsim", "\xE2\x88\xBD"},
    {"bsime", "\xE2\x8B\x8D"},
    {"bsol", "\x5C"},
    {"bsolb", "\xE2\xA7\x85"},
    {"bsolhsub", "\xE2\x9F\x88"},
    {"bull", "\xE2\x80\xA2"},
    {"bullet", "\xE2\x80\xA2"},
    {"bump", "\xE2\x89\x8E"},
    {"bumpE", "\xE2\xAA\xAE"},
    {"bumpe", "\xE2\x89\x8F"},
    {"bumpeq", "\xE2\x89\x8F"},
    {"cacute", "\xC4\x87"},
    {"cap", "\xE2\x88\xA9"},
    {"capand", "\xE2\xA9\x84"},
    {"capbrcup", "\xE2\xA9\x89"},
    {"capcap", "\xE2\xA9\x8B"},
    {"capcup", "\xE2\xA9\x87"},
    {"capdot", "\xE2\xA9\x80"},
    {"caps", "\xE2\x88\xA9\xEF\xB8\x80"},
    {"caret", "\xE2\x81\x81"},
    {"caron", "\xCB\x87"},
    {"ccaps", "\xE2\xA9\x8D"},
    {"ccaron", "\xC4\x8D"},
    {"ccedil", "\xC3\xA7"},
    {"ccirc", "\xC4\x89"},
    {"ccups", "\xE2\xA9\x8C"},
    {"ccupssm", "\xE2\xA9\x90"},
    {"cdot", "\xC4\x8B"},
    {"cedil", "\xC2\xB8"},
    {"cemptyv", "\xE2\xA6\xB2"},
    {"cent", "\xC2\xA2"},
    {"centerdot", "\xC2\xB7"},
    {"cfr", "\xF0\x9D\x94\xA0"},
    {"chcy", "\xD1\x87"},
    {"check", "\xE2\x9C\x93"},
    {"checkmark", "\xE2\x9C\x93"},
    {"chi", "\xCF\x87"},
    {"cir", "\xE2\x97\x8B"},
    {"cirE", "\xE2\xA7\x83"},
    {"circ", "\xCB\x86"},
    {"circeq", "\xE2\x89\x97"},
    {"circlearrowleft", "\xE2\x86\xBA"},
    {"circlearrowright", "\xE2\x86\xBB"},
    {"circledR", "\xC2\xAE"},
    {"circledS", "\xE2\x93\x88"},
    {"circledast", "\xE2\x8A\x9B"},
    {"circledcirc", "\xE2\x8A\x9A"},
    {"circleddash", "\xE2\x8A\x9D"},
    {"cire", "\xE2\x89\x97"},
    {"cirfnint", "\xE2\xA8\x90"},
    {"cirmid", "\xE2\xAB\xAF"},
    {"cirscir", "\xE2\xA7\x82"},
    {"clubs", "\xE2\x99\xA3"},
    {"clubsuit", "\xE2\x99\xA3"},
    {"colon", "\x3A"},
    {"colone", "\xE2\x89\x94"},
    {"coloneq", "\xE2\x89\x94"},
    {"comma", "\x2C"},
    {"commat", "\x40"},
    {"comp", "\xE2\x88\x81"},
    {"compfn", "\xE2\x88\x98"},
    {"complement", "\xE2\x88\x81"},
    {"complexes", "\xE2\x84\x82"},
    {"cong", "\xE2\x89\x85"},
    {"congdot", "\xE2\xA9\xAD"},
    {"conint", "\xE2\x88\xAE"},
    {"copf", "\xF0\x9D\x95\x94"},
    {"coprod", "\xE2\x88\x90"},
    {"copy", "\xC2\xA9"},
    {"copysr", "\xE2\x84\x97"},
    {"crarr", "\xE2\x86\xB5"},
    {"cross", "\xE2\x9C\x97"},
    {"cscr", "\xF0\x9D\x92\xB8"},
    {"csub", "\xE2\xAB\x8F"},
    {"csube", "\xE2\xAB\x91"},
    {"csup", "\xE2\xAB\x90"},
    {"csupe", "\xE2\xAB\x92"},
    {"ctdot", "\xE2\x8B\xAF"},
    {"cudarrl", "\xE2\xA4\xB8"},
    {"cudarrr", "\xE2\xA4\xB5"},
    {"cuepr", "\xE2\x8B\x9E"},
    {"cuesc", "\xE2\x8B\x9F"},
    {"cularr", "\xE2\x86\xB6"},
    {"cularrp", "\xE2\xA4\xBD"},
    {"cup", "\xE2\x88\xAA"},
    {"cupbrcap", "\xE2\xA9\x88"},
    {"cupcap", "\xE2\xA9\x86"},
    {"cupcup", "\xE2\xA9\x8A"},
    {"cupdot", "\xE2\x8A\x8D"},
    {"cupor", "\xE2\xA9\x85"},
    {"cups", "\xE2\x88\xAA\xEF\xB8\x80"},
    {"curarr", "\xE2\x86\xB7"},
    {"curarrm", "\xE2\xA4\xBC"},
    {"curlyeqprec", "\xE2\x8B\x9E"},
    {"curlyeqsucc", "\xE2\x8B\x9F"},
    {"curlyvee", "\xE2\x8B\x8E"},
    {"curlywedge", "\xE2\x8B\x8F"},
    {"curren", "\xC2\xA4"},
    {"curvearrowleft", "\xE2\x86\xB6"},
    {"curvearrowright", "\xE2\x86\xB7"},
    {"cuvee", "\xE2\x8B\x8E"},
    {"cuwed", "\xE2\x8B\x8F"},
    {"cwconint", "\xE2\x88\xB2"},
    {"cwint", "\xE2\x88\xB1"},
    {"cylcty", "\xE2\x8C\xAD"},
    {"dArr", "\xE2\x87\x93"},
    {"dHar", "\xE2\xA5\xA5"},
    {"dagger", "\xE2\x80\xA0"},
    {"daleth", "\xE2\x84\xB8"},
    {"darr", "\xE2\x86\x93"},
    {"dash", "\xE2\x80\x90"},
    {"dashv", "\xE2\x8A\xA3"},
    {"dbkarow", "\xE2\xA4\x8F"},
    {"dblac", "\xCB\x9D"},
    {"dcaron", "\xC4\x8F"},
    {"dcy", "\xD0\xB4"},
    {"dd", "\xE2\x85\x86"},
    {"ddagger", "\xE2\x80\xA1"},
    {"ddarr", "\xE2\x87\x8A"},
    {"ddotseq", "\xE2\xA9\xB7"},
    {"deg", "\xC2\xB0"},
    {"delta", "\xCE\xB4"},
    {"demptyv", "\xE2\xA6\xB1"},
    {"dfisht", "\xE2\xA5\xBF"},
    {"dfr", "\xF0\x9D\x94\xA1"},
    {"dharl", "\xE2\x87\x83"},
    {"dharr", "\xE2\x87\x82"},
    {"diam", "\xE2\x8B\x84"},
    {"diamond", "\xE2\x8B\x84"},
    {"diamondsuit", "\xE2\x99\xA6"},
    {"diams", "\xE2\x99\xA6"},
    {"die", "\xC2\xA8"},
    {"digamma", "\xCF\x9D"},
    {"disin", "\xE2\x8B\xB2"},
    {"div", "\xC3\xB7"},
    {"divide", "\xC3\xB7"},
    {"divideontimes", "\xE2\x8B\x87"},
    {"divonx", "\xE2\x8B\x87"},
    {"djcy", "\xD1\x92"},
    {"dlcorn", "\xE2\x8C\x9E"},
    {"dlcrop", "\xE2\x8C\x8D"},
    {"dollar", "\x24"},
    {"dopf", "\xF0\x9D\x95\x95"},
    {"dot", "\xCB\x99"},
    {"doteq", "\xE2\x89\x90"},
    {"doteqdot", "\xE2\x89\x91"},
    {"dotminus", "\xE2\x88\xB8"},
    {"dotplus", "\xE2\x88\x94"},
    {"dotsquare", "\xE2\x8A\xA1"},
    {"doublebarwedge", "\xE2\x8C\x86"},
    {"downarrow", "\xE2\x86\x93"},
    {"downdownarrows", "\xE2\x87\x8A"},
    {"downharpoonleft", "\xE2\x87\x83"},
    {"downharpoonright", "\xE2\x87\x82"},
    {"drbkarow", "\xE2\xA4\x90"},
    {"drcorn", "\xE2\x8C\x9F"},
    {"drcrop", "\xE2\x8C\x8C"},
    {"dscr", "\xF0\x9D\x92\xB9"},
    {"dscy", "\xD1\x95"},
    {"dsol", "\xE2\xA7\xB6"},
    {"dstrok", "\xC4\x91"},
    {"dtdot", "\xE2\x8B\xB1"},
    {"dtri", "\xE2\x96\xBF"},
    {"dtrif", "\xE2\x96\xBE"},
    {"duarr", "\xE2\x87\xB5"},
    {"duhar", "\xE2\xA5\xAF"},
    {"dwangle", "\xE2\xA6\xA6"},
    {"dzcy", "\xD1\x9F"},
    {"dzigrarr", "\xE2\x9F\xBF"},
    {"eDDot", "\xE2\xA9\xB7"},
    {"eDot", "\xE2\x89\x91"},
    {"eacute", "\xC3\xA9"},
    {"easter", "\xE2\xA9\xAE"},
    {"ecaron", "\xC4\x9B"},
    {"ecir", "\xE2\x89\x96"},
    {"ecirc", "\xC3\xAA"},
    {"ecolon", "\xE2\x89\x95"},
    {"ecy", "\xD1\x8D"},
    {"edot", "\xC4\x97"},
    {"ee", "\xE2\x85\x87"},
    {"efDot", "\xE2\x89\x92"},
    {"efr", "\xF0\x9D\x94\xA2"},
    {"eg", "\xE2\xAA\x9A"},
    {"egrave", "\xC3\xA8"},
    {"egs", "\xE2\xAA\x96"},
    {"egsdot", "\xE2\xAA\x98"},
    {"el", "\xE2\xAA\x99"},
    {"elinters", "\xE2\x8F\xA7"},
    {"ell", "\xE2\x84\x93"},
    {"els", "\xE2\xAA\x95"},
    {"elsdot", "\xE2\xAA\x97"},
    {"emacr", "\xC4\x93"},
    {"empty", "\xE2\x88\x85"},
    {"emptyset", "\xE2\x88\x85"},
    {"emptyv", "\xE2\x88\x85"},
    {"emsp", "\xE2\x80\x83"},
    {"emsp13", "\xE2\x80\x84"},
    {"emsp14", "\xE2\x80\x85"},
    {"eng", "\xC5\x8B"},
    {"ensp", "\xE2\x80\x82"},
    {"eogon", "\xC4\x99"},
    {"eopf", "\xF0\x9D\x95\x96"},
    {"epar", "\xE2\x8B\x95"},
    {"eparsl", "\xE2\xA7\xA3"},
    {"eplus", "\xE2\xA9\xB1"},
    {"epsi", "\xCE\xB5"},
    {"epsilon", "\xCE\xB5"},
    {"epsiv", "\xCF\xB5"},
    {"eqcirc", "\xE2\x89\x96"},
    {"eqcolon", "\xE2\x89\x95"},
    {"eqsim", "\xE2\x89\x82"},
    {"eqslantgtr", "\xE2\xAA\x96"},
    {"eqslantless", "\xE2\xAA\x95"},
    {"equals", "\x3D"},
    {"equest", "\xE2\x89\x9F"},
    {"equiv", "\xE2\x89\xA1"},
    {"equivDD", "\xE2\xA9\xB8"},
    {"eqvparsl", "\xE2\xA7\xA5"},
    {"erDot", "\xE2\x89\x93"},
    {"erarr", "\xE2\xA5\xB1"},
    {"escr", "\xE2\x84\xAF"},
    {"esdot", "\xE2\x89\x90"},
    {"esim", "\xE2\x89\x82"},
    {"eta", "\xCE\xB7"},
    {"eth", "\xC3\xB0"},
    {"euml", "\xC3\xAB"},
    {"euro", "\xE2\x82\xAC"},
    {"excl", "\x21"},
    {"exist", "\xE2\x88\x83"},
    {"expectation", "\xE2\x84\xB0"},
    {"exponentiale", "\xE2\x85\x87"},
    {"fallingdotseq", "\xE2\x89\x92"},
    {"fcy", "\xD1\x84"},
    {"female", "\xE2\x99\x80"},
    {"ffilig", "\xEF\xAC\x83"},
    {"fflig", "\xEF\xAC\x80"},
    {"ffllig", "\xEF\xAC\x84"},
    {"ffr", "\xF0\x9D\x94\xA3"},
    {"filig", "\xEF\xAC\x81"},
    {"fjlig", "\x66\x6A"},
    {"flat", "\xE2\x99\xAD"},
    {"fllig", "\xEF\xAC\x82"},
    {"fltns", "\xE2\x96\xB1"},
    {"fnof", "\xC6\x92"},
    {"fopf", "\xF0\x9D\x95\x97"},
    {"forall", "\xE2\x88\x80"},
    {"fork", "\xE2\x8B\x94"},
    {"forkv", "\xE2\xAB\x99"},
    {"fpartint", "\xE2\xA8\x8D"},
    {"frac12", "\xC2\xBD"},
    {"frac13", "\xE2\x85\x93"},
    {"frac14", "\xC2\xBC"},
    {"frac15", "\xE2\x85\x95"},
    {"frac16", "\xE2\x85\x99"},
    {"frac18", "\xE2\x85\x9B"},
    {"frac23", "\xE2\x85\x94"},
    {"frac25", "\xE2\x85\x96"},
    {"frac34", "\xC2\xBE"},
    {"frac35", "\xE2\x85\x97"},
    {"frac38", "\xE2\x85\x9C"},
    {"frac45", "\xE2\x85\x98"},
    {"frac56", "\xE2\x85\x9A"},
    {"frac58", "\xE2\x85\x9D"},
    {"frac78", "\xE2\x85\x9E"},
    {"frasl", "\xE2\x81\x84"},
    {"frown", "\xE2\x8C\xA2"},
    {"fscr", "\xF0\x9D\x92\xBB"},
    {"gE", "\xE2\x89\xA7"},
    {"gEl", "\xE2\xAA\x8C"},
    {"gacute", "\xC7\xB5"},
    {"gamma", "\xCE\xB3"},
    {"gammad", "\xCF\x9D"},
    {"gap", "\xE2\xAA\x86"},
    {"gbreve", "\xC4\x9F"},
    {"gcirc", "\xC4\x9D"},
    {"gcy", "\xD0\xB3"},
    {"gdot", "\xC4\xA1"},
    {"ge", "\xE2\x89\xA5"},
    {"gel", "\xE2\x8B\x9B"},
    {"geq", "\xE2\x89\xA5"},
    {"geqq", "\xE2\x89\xA7"},
    {"geqslant", "\xE2\xA9\xBE"},
    {"ges", "\xE2\xA9\xBE"},
    {"gescc", "\xE2\xAA\xA9"},
    {"gesdot", "\xE2\xAA\x80"},
    {"gesdoto", "\xE2\xAA\x82"},
    {"gesdotol", "\xE2\xAA\x84"},
    {"gesl", "\xE2\x8B\x9B\xEF\xB8\x80"},
    {"gesles", "\xE2\xAA\x94"},
    {"gfr", "\xF0\x9D\x94\xA4"},
    {"gg", "\xE2\x89\xAB"},
    {"ggg", "\xE2\x8B\x99"},
    {"gimel", "\xE2\x84\xB7"},
    {"gjcy", "\xD1\x93"},
    {"gl", "\xE2\x89\xB7"},
    {"glE", "\xE2\xAA\x92"},
    {"gla", "\xE2\xAA\xA5"},
    {"glj", "\xE2\xAA\xA4"},
    {"gnE", "\xE2\x89\xA9"},
    {"gnap", "\xE2\xAA\x8A"},
    {"gnapprox", "\xE2\xAA\x8A"},
    {"gne", "\xE2\xAA\x88"},
    {"gneq", "\xE2\xAA\x88"},
    {"gneqq", "\xE2\x89\xA9"},
    {"gnsim", "\xE2\x8B\xA7"},
    {"gopf", "\xF0\x9D\x95\x98"},
    {"grave", "\x60"},
    {"gscr", "\xE2\x84\x8A"},
    {"gsim", "\xE2\x89\xB3"},
    {"gsime", "\xE2\xAA\x8E"},
    {"gsiml", "\xE2\xAA\x90"},
    {"gt", "\x3E"},
    {"gtcc", "\xE2\xAA\xA7"},
    {"gtcir", "\xE2\xA9\xBA"},
    {"gtdot", "\xE2\x8B\x97"},
    {"gtlPar", "\xE2\xA6\x95"},
    {"gtquest", "\xE2\xA9\xBC"},
    {"gtrapprox", "\xE2\xAA\x86"},
    {"gtrarr", "\xE2\xA5\xB8"},
    {"gtrdot", "\xE2\x8B\x97"},
    {"gtreqless", "\xE2\x8B\x9B"},
    {"gtreqqless", "\xE2\xAA\x8C"},
    {"gtrless", "\xE2\x89\xB7"},
    {"gtrsim", "\xE2\x89\xB3"},
    {"gvertneqq", "\xE2\x89\xA9\xEF\xB8\x80"},
    {"gvnE", "\xE2\x89\xA9\xEF\xB8\x80"},
    {"hArr", "\xE2\x87\x94"},
    {"hairsp", "\xE2\x80\x8A"},
    {"half", "\xC2\xBD"},
    {"hamilt", "\xE2\x84\x8B"},
    {"hardcy", "\xD1\x8A"},
    {"harr", "\xE2\x86\x94"},
    {"harrcir", "\xE2\xA5\x88"},
    {"harrw", "\xE2\x86\xAD"},
    {"hbar", "\xE2\x84\x8F"},
    {"hcirc", "\xC4\xA5"},
    {"hearts", "\xE2\x99\xA5"},
    {"heartsuit", "\xE2\x99\xA5"},
    {"hellip", "\xE2\x80\xA6"},
    {"hercon", "\xE2\x8A\xB9"},
    {"hfr", "\xF0\x9D\x94\xA5"},
    {"hksearow", "\xE2\xA4\xA5"},
    {"hkswarow", "\xE2\xA4\xA6"},
    {"hoarr", "\xE2\x87\xBF"},
    {"homtht", "\xE2\x88\xBB"},
    {"hookleftarrow", "\xE2\x86\xA9"},
    {"hookrightarrow", "\xE2\x86\xAA"},
    {"hopf", "\xF0\x9D\x95\x99"},
    {"horbar", "\xE2\x80\x95"},
    {"hscr", "\xF0\x9D\x92\xBD"},
    {"hslash", "\xE2\x84\x8F"},
    {"hstrok", "\xC4\xA7"},
    {"hybull", "\xE2\x81\x83"},
    {"hyphen", "\xE2\x80\x90"},
    {"iacute", "\xC3\xAD"},
    {"ic", "\xE2\x81\xA3"},
    {"icirc", "\xC3\xAE"},
    {"icy", "\xD0\xB8"},
    {"iecy", "\xD0\xB5"},
    {"iexcl", "\xC2\xA1"},
    {"iff", "\xE2\x87\x94"},
    {"ifr", "\xF0\x9D\x94\xA6"},
    {"igrave", "\xC3\xAC"},
    {"ii", "\xE2\x85\x88"},
    {"iiiint", "\xE2\xA8\x8C"},
    {"iiint", "\xE2\x88\xAD"},
    {"iinfin", "\xE2\xA7\x9C"},
    {"iiota", "\xE2\x84\xA9"},
    {"ijlig", "\xC4\xB3"},
    {"imacr", "\xC4\xAB"},
    {"image", "\xE2\x84\x91"},
    {"imagline", "\xE2\x84\x90"},
    {"imagpart", "\xE2\x84\x91"},
    {"imath", "\xC4\xB1"},
    {"imof", "\xE2\x8A\xB7"},
    {"imped", "\xC6\xB5"},
    {"in", "\xE2\x88\x88"},
    {"incare", "\xE2\x84\x85"},
    {"infin", "\xE2\x88\x9E"},
    {"infintie", "\xE2\xA7\x9D"},
    {"inodot", "\xC4\xB1"},
    {"int", "\xE2\x88\xAB"},
    {"intcal", "\xE2\x8A\xBA"},
    {"integers", "\xE2\x84\xA4"},
    {"intercal", "\xE2\x8A\xBA"},
    {"intlarhk", "\xE2\xA8\x97"},
    {"intprod", "\xE2\xA8\xBC"},
    {"iocy", "\xD1\x91"},
    {"iogon", "\xC4\xAF"},
    {"iopf", "\xF0\x9D\x95\x9A"},
    {"iota", "\xCE\xB9"},
    {"iprod", "\xE2\xA8\xBC"},
    {"iquest", "\xC2\xBF"},
    {"iscr", "\xF0\x9D\x92\xBE"},
    {"isin", "\xE2\x88\x88"},
    {"isinE", "\xE2\x8B\xB9"},
    {"isindot", "\xE2\x8B\xB5"},
    {"isins", "\xE2\x8B\xB4"},
    {"isinsv", "\xE2\x8B\xB3"},
    {"isinv", "\xE2\x88\x88"},
    {"it", "\xE2\x81\xA2"},
    {"itilde", "\xC4\xA9"},
    {"iukcy", "\xD1\x96"},
    {"iuml", "\xC3\xAF"},
    {"jcirc", "\xC4\xB5"},
    {"jcy", "\xD0\xB9"},
    {"jfr", "\xF0\x9D\x94\xA7"},
    {"jmath", "\xC8\xB7"},
    {"jopf", "\xF0\x9D\x95\x9B"},
    {"jscr", "\xF0\x9D\x92\xBF"},
    {"jsercy", "\xD1\x98"},
    {"jukcy", "\xD1\x94"},
    {"kappa", "\xCE\xBA"},
    {"kappav", "\xCF\xB0"},
    {"kcedil", "\xC4\xB7"},
    {"kcy", "\xD0\xBA"},
    {"kfr", "\xF0\x9D\x94\xA8"},
    {"kgreen", "\xC4\xB8"},
    {"khcy", "\xD1\x85"},
    {"kjcy", "\xD1\x9C"},
    {"kopf", "\xF0\x9D\x95\x9C"},
    {"kscr", "\xF0\x9D\x93\x80"},
    {"lAarr", "\xE2\x87\x9A"},
    {"lArr", "\xE2\x87\x90"},
    {"lAtail", "\xE2\xA4\x9B"},
    {"lBarr", "\xE2\xA4\x8E"},
    {"lE", "\xE2\x89\xA6"},
    {"lEg", "\xE2\xAA\x8B"},
    {"lHar", "\xE2\xA5\xA2"},
    {"lacute", "\xC4\xBA"},
    {"laemptyv", "\xE2\xA6\xB4"},
    {"lagran", "\xE2\x84\x92"},
    {"lambda", "\xCE\xBB"},
    {"lang", "\xE2\x9F\xA8"},
    {"langd", "\xE2\xA6\x91"},
    {"langle", "\xE2\x9F\xA8"},
    {"lap", "\xE2\xAA\x85"},
    {"laquo", "\xC2\xAB"},
    {"larr", "\xE2\x86\x90"},
    {"larrb", "\xE2\x87\xA4"},
    {"larrbfs", "\xE2\xA4\x9F"},
    {"larrfs", "\xE2\xA4\x9D"},
    {"larrhk", "\xE2\x86\xA9"},
    {"larrlp", "\xE2\x86\xAB"},
    {"larrpl", "\xE2\xA4\xB9"},
    {"larrsim", "\xE2\xA5\xB3"},
    {"larrtl", "\xE2\x86\xA2"},
    {"lat", "\xE2\xAA\xAB"},
    {"latail", "\xE2\xA4\x99"},
    {"late", "\xE2\xAA\xAD"},
    {"lates", "\xE2\xAA\xAD\xEF\xB8\x80"},
    {"lbarr", "\xE2\xA4\x8C"},
    {"lbbrk", "\xE2\x9D\xB2"},
    {"lbrace", "\x7B"},
    {"lbrack", "\x5B"},
    {"lbrke", "\xE2\xA6\x8B"},
    {"lbrksld", "\xE2\xA6\x8F"},
    {"lbrkslu", "\xE2\xA6\x8D"},
    {"lcaron", "\xC4\xBE"},
    {"lcedil", "\xC4\xBC"},
    {"lceil", "\xE2\x8C\x88"},
    {"lcub", "\x7B"},
    {"lcy", "\xD0\xBB"},
    {"ldca", "\xE2\xA4\xB6"},
    {"ldquo", "\xE2\x80\x9C"},
    {"ldquor", "\xE2\x80\x9E"},
    {"ldrdhar", "\xE2\xA5\xA7"},
    {"ldrushar", "\xE2\xA5\x8B"},
    {"ldsh", "\xE2\x86\xB2"},
    {"le", "\xE2\x89\xA4"},
    {"leftarrow", "\xE2\x86\x90"},
    {"leftarrowtail", "\xE2\x86\xA2"},
    {"leftharpoondown", "\xE2\x86\xBD"},
    {"leftharpoonup", "\xE2\x86\xBC"},
    {"leftleftarrows", "\xE2\x87\x87"},
    {"leftrightarrow", "\xE2\x86\x94"},
    {"leftrightarrows", "\xE2\x87\x86"},
    {"leftrightharpoons", "\xE2\x87\x8B"},
    {"leftrightsquigarrow", "\xE2\x86\xAD"},
    {"leftthreetimes", "\xE2\x8B\x8B"},
    {"leg", "\xE2\x8B\x9A"},
    {"leq", "\xE2\x89\xA4"},
    {"leqq", "\xE2\x89\xA6"},
    {"leqslant", "\xE2\xA9\xBD"},
    {"les", "\xE2\xA9\xBD"},
    {"lescc", "\xE2\xAA\xA8"},
    {"lesdot", "\xE2\xA9\xBF"},
    {"lesdoto", "\xE2\xAA\x81"},
    {"lesdotor", "\xE2\xAA\x83"},
    {"lesg", "\xE2\x8B\x9A\xEF\xB8\x80"},
    {"lesges", "\xE2\xAA\x93"},
    {"lessapprox", "\xE2\xAA\x85"},
    {"lessdot", "\xE2\x8B\x96"},
    {"lesseqgtr", "\xE2\x8B\x9A"},
    {"lesseqqgtr", "\xE2\xAA\x8B"},
    {"lessgtr", "\xE2\x89\xB6"},
    {"lesssim", "\xE2\x89\xB2"},
    {"lfisht", "\xE2\xA5\xBC"},
    {"lfloor", "\xE2\x8C\x8A"},
    {"lfr", "\xF0\x9D\x94\xA9"},
    {"lg", "\xE2\x89\xB6"},
    {"lgE", "\xE2\xAA\x91"},
    {"lhard", "\xE2\x86\xBD"},
    {"lharu", "\xE2\x86\xBC"},
    {"lharul", "\xE2\xA5\xAA"},
    {"lhblk", "\xE2\x96\x84"},
    {"ljcy", "\xD1\x99"},
    {"ll", "\xE2\x89\xAA"},
    {"llarr", "\xE2\x87\x87"},
    {"llcorner", "\xE2\x8C\x9E"},
    {"llhard", "\xE2\xA5\xAB"},
    {"lltri", "\xE2\x97\xBA"},
    {"lmidot", "\xC5\x80"},
    {"lmoust", "\xE2\x8E\xB0"},
    {"lmoustache", "\xE2\x8E\xB0"},
    {"lnE", "\xE2\x89\xA8"},
    {"lnap", "\xE2\xAA\x89"},
    {"lnapprox", "\xE2\xAA\x89"},
    {"lne", "\xE2\xAA\x87"},
    {"lneq", "\xE2\xAA\x87"},
    {"lneqq", "\xE2\x89\xA8"},
    {"lnsim", "\xE2\x8B\xA6"},
    {"loang", "\xE2\x9F\xAC"},
    {"loarr", "\xE2\x87\xBD"},
    {"lobrk", "\xE2\x9F\xA6"},
    {"longleftarrow", "\xE2\x9F\xB5"},
    {"longleftrightarrow", "\xE2\x9F\xB7"},
    {"longmapsto", "\xE2\x9F\xBC"},
    {"longrightarrow", "\xE2\x9F\xB6"},
    {"looparrowleft", "\xE2\x86\xAB"},
    {"looparrowright", "\xE2\x86\xAC"},
    {"lopar", "\xE2\xA6\x85"},
    {"lopf", "\xF0\x9D\x95\x9D"},
    {"loplus", "\xE2\xA8\xAD"},
    {"lotimes", "\xE2\xA8\xB4"},
    {"lowast", "\xE2\x88\x97"},
    {"lowbar", "\x5F"},
    {"loz", "\xE2\x97\x8A"},
    {"lozenge", "\xE2\x97\x8A"},
    {"lozf", "\xE2\xA7\xAB"},
    {"lpar", "\x28"},
    {"lparlt", "\xE2\xA6\x93"},
    {"lrarr", "\xE2\x87\x86"},
    {"lrcorner", "\xE2\x8C\x9F"},
    {"lrhar", "\xE2\x87\x8B"},
    {"lrhard", "\xE2\xA5\xAD"},
    {"lrm", "\xE2\x80\x8E"},
    {"lrtri", "\xE2\x8A\xBF"},
    {"lsaquo", "\xE2\x80\xB9"},
    {"lscr", "\xF0\x9D\x93\x81"},
    {"lsh", "\xE2\x86\xB0"},
    {"lsim", "\xE2\x89\xB2"},
    {"lsime", "\xE2\xAA\x8D"},
    {"lsimg", "\xE2\xAA\x8F"},
    {"lsqb", "\x5B"},
    {"lsquo", "\xE2\x80\x98"},
    {"lsquor", "\xE2\x80\x9A"},
    {"lstrok", "\xC5\x82"},
    {"lt", "\x3C"},
    {"ltcc", "\xE2\xAA\xA6"},
    {"ltcir", "\xE2\xA9\xB9"},
    {"ltdot", "\xE2\x8B\x96"},
    {"lthree", "\xE2\x8B\x8B"},
    {"ltimes", "\xE2\x8B\x89"},
    {"ltlarr", "\xE2\xA5\xB6"},
    {"ltquest", "\xE2\xA9\xBB"},
    {"ltrPar", "\xE2\xA6\x96"},
    {"ltri", "\xE2\x97\x83"},
    {"ltrie", "\xE2\x8A\xB4"},
    {"ltrif", "\xE2\x97\x82"},
    {"lurdshar", "\xE2\xA5\x8A"},
    {"luruhar", "\xE2\xA5\xA6"},
    {"lvertneqq", "\xE2\x89\xA8\xEF\xB8\x80"},
    {"lvnE", "\xE2\x89\xA8\xEF\xB8\x80"},
    {"mDDot", "\xE2\x88\xBA"},
    {"macr", "\xC2\xAF"},
    {"male", "\xE2\x99\x82"},
    {"malt", "\xE2\x9C\xA0"},
    {"maltese", "\xE2\x9C\xA0"},
    {"map", "\xE2\x86\xA6"},
    {"mapsto", "\xE2\x86\xA6"},
    {"mapstodown", "\xE2\x86\xA7"},
    {"mapstoleft", "\xE2\x86\xA4"},
    {"mapstoup", "\xE2\x86\xA5"},
    {"marker", "\xE2\x96\xAE"},
    {"mcomma", "\xE2\xA8\xA9"},
    {"mcy", "\xD0\xBC"},
    {"mdash", "\xE2\x80\x94"},
    {"measuredangle", "\xE2\x88\xA1"},
    {"mfr", "\xF0\x9D\x94\xAA"},
    {"mho", "\xE2\x84\xA7"},
    {"micro", "\xC2\xB5"},
    {"mid", "\xE2\x88\xA3"},
    {"midast", "\x2A"},
    {"midcir", "\xE2\xAB\xB0"},
    {"middot", "\xC2\xB7"},
    {"minus", "\xE2\x88\x92"},
    {"minusb", "\xE2\x8A\x9F"},
    {"minusd", "\xE2\x88\xB8"},
    {"minusdu", "\xE2\xA8\xAA"},
    {"mlcp", "\xE2\xAB\x9B"},
    {"mldr", "\xE2\x80\xA6"},
    {"mnplus", "\xE2\x88\x93"},
    {"models", "\xE2\x8A\xA7"},
    {"mopf", "\xF0\x9D\x95\x9E"},
    {"mp", "\xE2\x88\x93"},
    {"mscr", "\xF0\x9D\x93\x82"},
    {"mstpos", "\xE2\x88\xBE"},
    {"mu", "\xCE\xBC"},
    {"multimap", "\xE2\x8A\xB8"},
    {"mumap", "\xE2\x8A\xB8"},
    {"nGg", "\xE2\x8B\x99\xCC\xB8"},
    {"nGt", "\xE2\x89\xAB\xE2\x83\x92"},
    {"nGtv", "\xE2\x89\xAB\xCC\xB8"},
    {"nLeftarrow", "\xE2\x87\x8D"},
    {"nLeftrightarrow", "\xE2\x87\x8E"},
    {"nLl", "\xE2\x8B\x98\xCC\xB8"},
    {"nLt", "\xE2\x89\xAA\xE2\x83\x92"},
    {"nLtv", "\xE2\x89\xAA\xCC\xB8"},
    {"nRightarrow", "\xE2\x87\x8F"},
    {"nVDash", "\xE2\x8A\xAF"},
    {"nVdash", "\xE2\x8A\xAE"},
    {"nabla", "\xE2\x88\x87"},
    {"nacute", "\xC5\x84"},
    {"nang", "\xE2\x88\xA0\xE2\x83\x92"},
    {"nap", "\xE2\x89\x89"},
    {"napE", "\xE2\xA9\xB0\xCC\xB8"},
    {"napid", "\xE2\x89\x8B\xCC\xB8"},
    {"napos", "\xC5\x89"},
    {"napprox", "\xE2\x89\x89"},
    {"natur", "\xE2\x99\xAE"},
    {"natural", "\xE2\x99\xAE"},
    {"naturals", "\xE2\x84\x95"},
    {"nbsp", "\xC2\xA0"},
    {"nbump", "\xE2\x89\x8E\xCC\xB8"},
    {"nbumpe", "\xE2\x89\x8F\xCC\xB8"},
    {"ncap", "\xE2\xA9\x83"},
    {"ncaron", "\xC5\x88"},
    {"ncedil", "\xC5\x86"},
    {"ncong", "\xE2\x89\x87"},
    {"ncongdot", "\xE2\xA9\xAD\xCC\xB8"},
    {"ncup", "\xE2\xA9\x82"},
    {"ncy", "\xD0\xBD"},
    {"ndash", "\xE2\x80\x93"},
    {"ne", "\xE2\x89\xA0"},
    {"neArr", "\xE2\x87\x97"},
    {"nearhk", "\xE2\xA4\xA4"},
    {"nearr", "\xE2\x86\x97"},
    {"nearrow", "\xE2\x86\x97"},
    {"nedot", "\xE2\x89\x90\xCC\xB8"},
    {"nequiv", "\xE2\x89\xA2"},
    {"nesear", "\xE2\xA4\xA8"},
    {"nesim", "\xE2\x89\x82\xCC\xB8"},
    {"nexist", "\xE2\x88\x84"},
    {"nexists", "\xE2\x88\x84"},
    {"nfr", "\xF0\x9D\x94\xAB"},
    {"ngE", "\xE2\x89\xA7\xCC\xB8"},
    {"nge", "\xE2\x89\xB1"},
    {"ngeq", "\xE2\x89\xB1"},
    {"ngeqq", "\xE2\x89\xA7\xCC\xB8"},
    {"ngeqslant", "\xE2\xA9\xBE\xCC\xB8"},
    {"nges", "\xE2\xA9\xBE\xCC\xB8"},
    {"ngsim", "\xE2\x89\xB5"},
    {"ngt", "\xE2\x89\xAF"},
    {"ngtr", "\xE2\x89\xAF"},
    {"nhArr", "\xE2\x87\x8E"},
    {"nharr", "\xE2\x86\xAE"},
    {"nhpar", "\xE2\xAB\xB2"},
    {"ni", "\xE2\x88\x8B"},
    {"nis", "\xE2\x8B\xBC"},
    {"nisd", "\xE2\x8B\xBA"},
    {"niv", "\xE2\x88\x8B"},
    {"njcy", "\xD1\x9A"},
    {"nlArr", "\xE2\x87\x8D"},
    {"nlE", "\xE2\x89\xA6\xCC\xB8"},
    {"nlarr", "\xE2\x86\x9A"},
    {"nldr", "\xE2\x80\xA5"},
    {"nle", "\xE2\x89\xB0"},
    {"nleftarrow", "\xE2\x86\x9A"},
    {"nleftrightarrow", "\xE2\x86\xAE"},
    {"nleq", "\xE2\x89\xB0"},
    {"nleqq", "\xE2\x89\xA6\xCC\xB8"},
    {"nleqslant", "\xE2\xA9\xBD\xCC\xB8"},
    {"nles", "\xE2\xA9\xBD\xCC\xB8"},
    {"nless", "\xE2\x89\xAE"},
    {"nlsim", "\xE2\x89\xB4"},
    {"nlt", "\xE2\x89\xAE"},
    {"nltri", "\xE2\x8B\xAA"},
    {"nltrie", "\xE2\x8B\xAC"},
    {"nmid", "\xE2\x88\xA4"},
    {"nopf", "\xF0\x9D\x95\x9F"},
    {"not", "\xC2\xAC"},
    {"notin", "\xE2\x88\x89"},
    {"notinE", "\xE2\x8B\xB9\xCC\xB8"},
    {"notindot", "\xE2\x8B\xB5\xCC\xB8"},
    {"notinva", "\xE2\x88\x89"},
    {"notinvb", "\xE2\x8B\xB7"},
    {"notinvc", "\xE2\x8B\xB6"},
    {"notni", "\xE2\x88\x8C"},
    {"notniva", "\xE2\x88\x8C"},
    {"notnivb", "\xE2\x8B\xBE"},
    {"notnivc", "\xE2\x8B\xBD"},
    {"npar", "\xE2\x88\xA6"},
    {"nparallel", "\xE2\x88\xA6"},
    {"nparsl", "\xE2\xAB\xBD\xE2\x83\xA5"},
    {"npart", "\xE2\x88\x82\xCC\xB8"},
    {"npolint", "\xE2\xA8\x94"},
    {"npr", "\xE2\x8A\x80"},
    {"nprcue", "\xE2\x8B\xA0"},
    {"npre", "\xE2\xAA\xAF\xCC\xB8"},
    {"nprec", "\xE2\x8A\x80"},
    {"npreceq", "\xE2\xAA\xAF\xCC\xB8"},
    {"nrArr", "\xE2\x87\x8F"},
    {"nrarr", "\xE2\x86\x9B"},
    {"nrarrc", "\xE2\xA4\xB3\xCC\xB8"},
    {"nrarrw", "\xE2\x86\x9D\xCC\xB8"},
    {"nrightarrow", "\xE2\x86\x9B"},
    {"nrtri", "\xE2\x8B\xAB"},
    {"nrtrie", "\xE2\x8B\xAD"},
    {"nsc", "\xE2\x8A\x81"},
    {"nsccue", "\xE2\x8B\xA1"},
    {"nsce", "\xE2\xAA\xB0\xCC\xB8"},
    {"nscr", "\xF0\x9D\x93\x83"},
    {"nshortmid", "\xE2\x88\xA4"},
    {"nshortparallel", "\xE2\x88\xA6"},
    {"nsim", "\xE2\x89\x81"},
    {"nsime", "\xE2\x89\x84"},
    {"nsimeq", "\xE2\x89\x84"},
    {"nsmid", "\xE2\x88\xA4"},
    {"nspar", "\xE2\x88\xA6"},
    {"nsqsube", "\xE2\x8B\xA2"},
    {"nsqsupe", "\xE2\x8B\xA3"},
    {"nsub", "\xE2\x8A\x84"},
    {"nsubE", "\xE2\xAB\x85\xCC\xB8"},
    {"nsube", "\xE2\x8A\x88"},
    {"nsubset", "\xE2\x8A\x82\xE2\x83\x92"},
    {"nsubseteq", "\xE2\x8A\x88"},
    {"nsubseteqq", "\xE2\xAB\x85\xCC\xB8"},
    {"nsucc", "\xE2\x8A\x81"},
    {"nsucceq", "\xE2\xAA\xB0\xCC\xB8"},
    {"nsup", "\xE2\x8A\x85"},
    {"nsupE", "\xE2\xAB\x86\xCC\xB8"},
    {"nsupe", "\xE2\x8A\x89"},
    {"nsupset", "\xE2\x8A\x83\xE2\x83\x92"},
    {"nsupseteq", "\xE2\x8A\x89"},
    {"nsupseteqq", "\xE2\xAB\x86\xCC\xB8"},
    {"ntgl", "\xE2\x89\xB9"},
    {"ntilde", "\xC3\xB1"},
    {"ntlg", "\xE2\x89\xB8"},
    {"ntriangleleft", "\xE2\x8B\xAA"},
    {"ntrianglelefteq", "\xE2\x8B\xAC"},
    {"ntriangleright", "\xE2\x8B\xAB"},
    {"ntrianglerighteq", "\xE2\x8B\xAD"},
    {"nu", "\xCE\xBD"},
    {"num", "\x23"},
    {"numero", "\xE2\x84\x96"},
    {"numsp", "\xE2\x80\x87"},
    {"nvDash", "\xE2\x8A\xAD"},
    {"nvHarr", "\xE2\xA4\x84"},
    {"nvap", "\xE2\x89\x8D\xE2\x83\x92"},
    {"nvdash", "\xE2\x8A\xAC"},
    {"nvge", "\xE2\x89\xA5\xE2\x83\x92"},
    {"nvgt", "\x3E\xE2\x83\x92"},
    {"nvinfin", "\xE2\xA7\x9E"},
    {"nvlArr", "\xE2\xA4\x82"},
    {"nvle", "\xE2\x89\xA4\xE2\x83\x92"},
    {"nvlt", "\x3C\xE2\x83\x92"},
    {"nvltrie", "\xE2\x8A\xB4\xE2\x83\x92"},
    {"nvrArr", "\xE2\xA4\x83"},
    {"nvrtrie", "\xE2\x8A\xB5\xE2\x83\x92"},
    {"nvsim", "\xE2\x88\xBC\xE2\x83\x92"},
    {"nwArr", "\xE2\x87\x96"},
    {"nwarhk", "\xE2\xA4\xA3"},
    {"nwarr", "\xE2\x86\x96"},
    {"nwarrow", "\xE2\x86\x96"},
    {"nwnear", "\xE2\xA4\xA7"},
    {"oS", "\xE2\x93\x88"},
    {"oacute", "\xC3\xB3"},
    {"oast", "\xE2\x8A\x9B"},
    {"ocir", "\xE2\x8A\x9A"},
    {"ocirc", "\xC3\xB4"},
    {"ocy", "\xD0\xBE"},
    {"odash", "\xE2\x8A\x9D"},
    {"odblac", "\xC5\x91"},
    {"odiv", "\xE2\xA8\xB8"},
    {"odot", "\xE2\x8A\x99"},
    {"odsold", "\xE2\xA6\xBC"},
    {"oelig", "\xC5\x93"},
    {"ofcir", "\xE2\xA6\xBF"},
    {"ofr", "\xF0\x9D\x94\xAC"},
    {"ogon", "\xCB\x9B"},
    {"ograve", "\xC3\xB2"},
    {"ogt", "\xE2\xA7\x81"},
    {"ohbar", "\xE2\xA6\xB5"},
    {"ohm", "\xCE\xA9"},
    {"oint", "\xE2\x88\xAE"},
    {"olarr", "\xE2\x86\xBA"},
    {"olcir", "\xE2\xA6\xBE"},
    {"olcross", "\xE2\xA6\xBB"},
    {"oline", "\xE2\x80\xBE"},
    {"olt", "\xE2\xA7\x80"},
    {"omacr", "\xC5\x8D"},
    {"omega", "\xCF\x89"},
    {"omicron", "\xCE\xBF"},
    {"omid", "\xE2\xA6\xB6"},
    {"ominus", "\xE2\x8A\x96"},
    {"oopf", "\xF0\x9D\x95\xA0"},
    {"opar", "\xE2\xA6\xB7"},
    {"operp", "\xE2\xA6\xB9"},
    {"oplus", "\xE2\x8A\x95"},
    {"or", "\xE2\x88\xA8"},
    {"orarr", "\xE2\x86\xBB"},
    {"ord", "\xE2\xA9\x9D"},
    {"order", "\xE2\x84\xB4"},
    {"orderof", "\xE2\x84\xB4"},
    {"ordf", "\xC2\xAA"},
    {"ordm", "\xC2\xBA"},
    {"origof", "\xE2\x8A\xB6"},
    {"oror", "\xE2\xA9\x96"},
    {"orslope", "\xE2\xA9\x97"},
    {"orv", "\xE2\xA9\x9B"},
    {"oscr", "\xE2\x84\xB4"},
    {"oslash", "\xC3\xB8"},
    {"osol", "\xE2\x8A\x98"},
    {"otilde", "\xC3\xB5"},
    {"otimes", "\xE2\x8A\x97"},
    {"otimesas", "\xE2\xA8\xB6"},
    {"ouml", "\xC3\xB6"},
    {"ovbar", "\xE2\x8C\xBD"},
    {"par", "\xE2\x88\xA5"},
    {"para", "\xC2\xB6"},
    {"parallel", "\xE2\x88\xA5"},
    {"parsim", "\xE2\xAB\xB3"},
    {"parsl", "\xE2\xAB\xBD"},
    {"part", "\xE2\x88\x82"},
    {"pcy", "\xD0\xBF"},
    {"percnt", "\x25"},
    {"period", "\x2E"},
    {"permil", "\xE2\x80\xB0"},
    {"perp", "\xE2\x8A\xA5"},
    {"pertenk", "\xE2\x80\xB1"},
    {"pfr", "\xF0\x9D\x94\xAD"},
    {"phi", "\xCF\x86"},
    {"phiv", "\xCF\x95"},
    {"phmmat", "\xE2\x84\xB3"},
    {"phone", "\xE2\x98\x8E"},
    {"pi", "\xCF\x80"},
    {"pitchfork", "\xE2\x8B\x94"},
    {"piv", "\xCF\x96"},
    {"planck", "\xE2\x84\x8F"},
    {"planckh", "\xE2\x84\x8E"},
    {"plankv", "\xE2\x84\x8F"},
    {"plus", "\x2B"},
    {"plusacir", "\xE2\xA8\xA3"},
    {"plusb", "\xE2\x8A\x9E"},
    {"pluscir", "\xE2\xA8\xA2"},
    {"plusdo", "\xE2\x88\x94"},
    {"plusdu", "\xE2\xA8\xA5"},
    {"pluse", "\xE2\xA9\xB2"},
    {"plusmn", "\xC2\xB1"},
    {"plussim", "\xE2\xA8\xA6"},
    {"plustwo", "\xE2\xA8\xA7"},
    {"pm", "\xC2\xB1"},
    {"pointint", "\xE2\xA8\x95"},
    {"popf", "\xF0\x9D\x95\xA1"},
    {"pound", "\xC2\xA3"},
    {"pr", "\xE2\x89\xBA"},
    {"prE", "\xE2\xAA\xB3"},
    {"prap", "\xE2\xAA\xB7"},
    {"prcue", "\xE2\x89\xBC"},
    {"pre", "\xE2\xAA\xAF"},
    {"prec", "\xE2\x89\xBA"},
    {"precapprox", "\xE2\xAA\xB7"},
    {"preccurlyeq", "\xE2\x89\xBC"},
    {"preceq", "\xE2\xAA\xAF"},
    {"precnapprox", "\xE2\xAA\xB9"},
    {"precneqq", "\xE2\xAA\xB5"},
    {"precnsim", "\xE2\x8B\xA8"},
    {"precsim", "\xE2\x89\xBE"},
    {"prime", "\xE2\x80\xB2"},
    {"primes", "\xE2\x84\x99"},
    {"prnE", "\xE2\xAA\xB5"},
    {"prnap", "\xE2\xAA\xB9"},
    {"prnsim", "\xE2\x8B\xA8"},
    {"prod", "\xE2\x88\x8F"},
    {"profalar", "\xE2\x8C\xAE"},
    {"profline", "\xE2\x8C\x92"},
    {"profsurf", "\xE2\x8C\x93"},
    {"prop", "\xE2\x88\x9D"},
    {"propto", "\xE2\x88\x9D"},
    {"prsim", "\xE2\x89\xBE"},
    {"prurel", "\xE2\x8A\xB0"},
    {"pscr", "\xF0\x9D\x93\x85"},
    {"psi", "\xCF\x88"},
    {"puncsp", "\xE2\x80\x88"},
    {"qfr", "\xF0\x9D\x94\xAE"},
    {"qint", "\xE2\xA8\x8C"},
    {"qopf", "\xF0\x9D\x95\xA2"},
    {"qprime", "\xE2\x81\x97"},
    {"qscr", "\xF0\x9D\x93\x86"},
    {"quaternions", "\xE2\x84\x8D"},
    {"quatint", "\xE2\xA8\x96"},
    {"quest", "\x3F"},
    {"questeq", "\xE2\x89\x9F"},
    {"quot", "\x22"},
    {"rAarr", "\xE2\x87\x9B"},
    {"rArr", "\xE2\x87\x92"},
    {"rAtail", "\xE2\xA4\x9C"},
    {"rBarr", "\xE2\xA4\x8F"},
    {"rHar", "\xE2\xA5\xA4"},
    {"race", "\xE2\x88\xBD\xCC\xB1"},
    {"racute", "\xC5\x95"},
    {"radic", "\xE2\x88\x9A"},
    {"raemptyv", "\xE2\xA6\xB3"},
    {"rang", "\xE2\x9F\xA9"},
    {"rangd", "\xE2\xA6\x92"},
    {"range", "\xE2\xA6\xA5"},
    {"rangle", "\xE2\x9F\xA9"},
    {"raquo", "\xC2\xBB"},
    {"rarr", "\xE2\x86\x92"},
    {"rarrap", "\xE2\xA5\xB5"},
    {"rarrb", "\xE2\x87\xA5"},
    {"rarrbfs", "\xE2\xA4\xA0"},
    {"rarrc", "\xE2\xA4\xB3"},
    {"rarrfs", "\xE2\xA4\x9E"},
    {"rarrhk", "\xE2\x86\xAA"},
    {"rarrlp", "\xE2\x86\xAC"},
    {"rarrpl", "\xE2\xA5\x85"},
    {"rarrsim", "\xE2\xA5\xB4"},
    {"rarrtl", "\xE2\x86\xA3"},
    {"rarrw", "\xE2\x86\x9D"},
    {"ratail", "\xE2\xA4\x9A"},
    {"ratio", "\xE2\x88\xB6"},
    {"rationals", "\xE2\x84\x9A"},
    {"rbarr", "\xE2\xA4\x8D"},
    {"rbbrk", "\xE2\x9D\xB3"},
    {"rbrace", "\x7D"},
    {"rbrack", "\x5D"},
    {"rbrke", "\xE2\xA6\x8C"},
    {"rbrksld", "\xE2\xA6\x8E"},
    {"rbrkslu", "\xE2\xA6\x90"},
    {"rcaron", "\xC5\x99"},
    {"rcedil", "\xC5\x97"},
    {"rceil", "\xE2\x8C\x89"},
    {"rcub", "\x7D"},
    {"rcy", "\xD1\x80"},
    {"rdca", "\xE2\xA4\xB7"},
    {"rdldhar", "\xE2\xA5\xA9"},
    {"rdquo", "\xE2\x80\x9D"},
    {"rdquor", "\xE2\x80\x9D"},
    {"rdsh", "\xE2\x86\xB3"},
    {"real", "\xE2\x84\x9C"},
    {"realine", "\xE2\x84\x9B"},
    {"realpart", "\xE2\x84\x9C"},
    {"reals", "\xE2\x84\x9D"},
    {"rect", "\xE2\x96\xAD"},
    {"reg", "\xC2\xAE"},
    {"rfisht", "\xE2\xA5\xBD"},
    {"rfloor", "\xE2\x8C\x8B"},
    {"rfr", "\xF0\x9D\x94\xAF"},
    {"rhard", "\xE2\x87\x81"},
    {"rharu", "\xE2\x87\x80"},
    {"rharul", "\xE2\xA5\xAC"},
    {"rho", "\xCF\x81"},
    {"rhov", "\xCF\xB1"},
    {"rightarrow", "\xE2\x86\x92"},
    {"rightarrowtail", "\xE2\x86\xA3"},
    {"rightharpoondown", "\xE2\x87\x81"},
    {"rightharpoonup", "\xE2\x87\x80"},
    {"rightleftarrows", "\xE2\x87\x84"},
    {"rightleftharpoons", "\xE2\x87\x8C"},
    {"rightrightarrows", "\xE2\x87\x89"},
    {"rightsquigarrow", "\xE2\x86\x9D"},
    {"rightthreetimes", "\xE2\x8B\x8C"},
    {"ring", "\xCB\x9A"},
    {"risingdotseq", "\xE2\x89\x93"},
    {"rlarr", "\xE2\x87\x84"},
    {"rlhar", "\xE2\x87\x8C"},
    {"rlm", "\xE2\x80\x8F"},
    {"rmoust", "\xE2\x8E\xB1"},
    {"rmoustache", "\xE2\x8E\xB1"},
    {"rnmid", "\xE2\xAB\xAE"},
    {"roang", "\xE2\x9F\xAD"},
    {"roarr", "\xE2\x87\xBE"},
    {"robrk", "\xE2\x9F\xA7"},
    {"ropar", "\xE2\xA6\x86"},
    {"ropf", "\xF0\x9D\x95\xA3"},
    {"roplus", "\xE2\xA8\xAE"},
    {"rotimes", "\xE2\xA8\xB5"},
    {"rpar", "\x29"},
    {"rpargt", "\xE2\xA6\x94"},
    {"rppolint", "\xE2\xA8\x92"},
    {"rrarr", "\xE2\x87\x89"},
    {"rsaquo", "\xE2\x80\xBA"},
    {"rscr", "\xF0\x9D\x93\x87"},
    {"rsh", "\xE2\x86\xB1"},
    {"rsqb", "\x5D"},
    {"rsquo", "\xE2\x80\x99"},
    {"rsquor", "\xE2\x80\x99"},
    {"rthree", "\xE2\x8B\x8C"},
    {"rtimes", "\xE2\x8B\x8A"},
    {"rtri", "\xE2\x96\xB9"},
    {"rtrie", "\xE2\x8A\xB5"},
    {"rtrif", "\xE2\x96\xB8"},
    {"rtriltri", "\xE2\xA7\x8E"},
    {"ruluhar", "\xE2\xA5\xA8"},
    {"rx", "\xE2\x84\x9E"},
    {"sacute", "\xC5\x9B"},
    {"sbquo", "\xE2\x80\x9A"},
    {"sc", "\xE2\x89\xBB"},
    {"scE", "\xE2\xAA\xB4"},
    {"scap", "\xE2\xAA\xB8"},
    {"scaron", "\xC5\xA1"},
    {"sccue", "\xE2\x89\xBD"},
    {"sce", "\xE2\xAA\xB0"},
    {"scedil", "\xC5\x9F"},
    {"scirc", "\xC5\x9D"},
    {"scnE", "\xE2\xAA\xB6"},
    {"scnap", "\xE2\xAA\xBA"},
    {"scnsim", "\xE2\x8B\xA9"},
    {"scpolint", "\xE2\xA8\x93"},
    {"scsim", "\xE2\x89\xBF"},
    {"scy", "\xD1\x81"},
    {"sdot", "\xE2\x8B\x85"},
    {"sdotb", "\xE2\x8A\xA1"},
    {"sdote", "\xE2\xA9\xA6"},
    {"seArr", "\xE2\x87\x98"},
    {"searhk", "\xE2\xA4\xA5"},
    {"searr", "\xE2\x86\x98"},
    {"searrow", "\xE2\x86\x98"},
    {"sect", "\xC2\xA7"},
    {"semi", "\x3B"},
    {"seswar", "\xE2\xA4\xA9"},
    {"setminus", "\xE2\x88\x96"},
    {"setmn", "\xE2\x88\x96"},
    {"sext", "\xE2\x9C\xB6"},
    {"sfr", "\xF0\x9D\x94\xB0"},
    {"sfrown", "\xE2\x8C\xA2"},
    {"sharp", "\xE2\x99\xAF"},
    {"shchcy", "\xD1\x89"},
    {"shcy", "\xD1\x88"},
    {"shortmid", "\xE2\x88\xA3"},
    {"shortparallel", "\xE2\x88\xA5"},
    {"shy", "\xC2\xAD"},
    {"sigma", "\xCF\x83"},
    {"sigmaf", "\xCF\x82"},
    {"sigmav", "\xCF\x82"},
    {"sim", "\xE2\x88\xBC"},
    {"simdot", "\xE2\xA9\xAA"},
    {"sime", "\xE2\x89\x83"},
    {"simeq", "\xE2\x89\x83"},
    {"simg", "\xE2\xAA\x9E"},
    {"simgE", "\xE2\xAA\xA0"},
    {"siml", "\xE2\xAA\x9D"},
    {"simlE", "\xE2\xAA\x9F"},
    {"simne", "\xE2\x89\x86"},
    {"simplus", "\xE2\xA8\xA4"},
    {"simrarr", "\xE2\xA5\xB2"},
    {"slarr", "\xE2\x86\x90"},
    {"smallsetminus", "\xE2\x88\x96"},
    {"smashp", "\xE2\xA8\xB3"},
    {"smeparsl", "\xE2\xA7\xA4"},
    {"smid", "\xE2\x88\xA3"},
    {"smile", "\xE2\x8C\xA3"},
    {"smt", "\xE2\xAA\xAA"},
    {"smte", "\xE2\xAA\xAC"},
    {"smtes", "\xE2\xAA\xAC\xEF\xB8\x80"},
    {"softcy", "\xD1\x8C"},
    {"sol", "\x2F"},
    {"solb", "\xE2\xA7\x84"},
    {"solbar", "\xE2\x8C\xBF"},
    {"sopf", "\xF0\x9D\x95\xA4"},
    {"spades", "\xE2\x99\xA0"},
    {"spadesuit", "\xE2\x99\xA0"},
    {"spar", "\xE2\x88\xA5"},
    {"sqcap", "\xE2\x8A\x93"},
    {"sqcaps", "\xE2\x8A\x93\xEF\xB8\x80"},
    {"sqcup", "\xE2\x8A\x94"},
    {"sqcups", "\xE2\x8A\x94\xEF\xB8\x80"},
    {"sqsub", "\xE2\x8A\x8F"},
    {"sqsube", "\xE2\x8A\x91"},
    {"sqsubset", "\xE2\x8A\x8F"},
    {"sqsubseteq", "\xE2\x8A\x91"},
    {"sqsup", "\xE2\x8A\x90"},
    {"sqsupe", "\xE2\x8A\x92"},
    {"sqsupset", "\xE2\x8A\x90"},
    {"sqsupseteq", "\xE2\x8A\x92"},
    {"squ", "\xE2\x96\xA1"},
    {"square", "\xE2\x96\xA1"},
    {"squarf", "\xE2\x96\xAA"},
    {"squf", "\xE2\x96\xAA"},
    {"srarr", "\xE2\x86\x92"},
    {"sscr", "\xF0\x9D\x93\x88"},
    {"ssetmn", "\xE2\x88\x96"},
    {"ssmile", "\xE2\x8C\xA3"},
    {"sstarf", "\xE2\x8B\x86"},
    {"star", "\xE2\x98\x86"},
    {"starf", "\xE2\x98\x85"},
    {"straightepsilon", "\xCF\xB5"},
    {"straightphi", "\xCF\x95"},
    {"strns", "\xC2\xAF"},
    {"sub", "\xE2\x8A\x82"},
    {"subE", "\xE2\xAB\x85"},
    {"subdot", "\xE2\xAA\xBD"},
    {"sube", "\xE2\x8A\x86"},
    {"subedot", "\xE2\xAB\x83"},
    {"submult", "\xE2\xAB\x81"},
    {"subnE", "\xE2\xAB\x8B"},
    {"subne", "\xE2\x8A\x8A"},
    {"subplus", "\xE2\xAA\xBF"},
    {"subrarr", "\xE2\xA5\xB9"},
    {"subset", "\xE2\x8A\x82"},
    {"subseteq", "\xE2\x8A\x86"},
    {"subseteqq", "\xE2\xAB\x85"},
    {"subsetneq", "\xE2\x8A\x8A"},
    {"subsetneqq", "\xE2\xAB\x8B"},
    {"subsim", "\xE2\xAB\x87"},
    {"subsub", "\xE2\xAB\x95"},
    {"subsup", "\xE2\xAB\x93"},
    {"succ", "\xE2\x89\xBB"},
    {"succapprox", "\xE2\xAA\xB8"},
    {"succcurlyeq", "\xE2\x89\xBD"},
    {"succeq", "\xE2\xAA\xB0"},
    {"succnapprox", "\xE2\xAA\xBA"},
    {"succneqq", "\xE2\xAA\xB6"},
    {"succnsim", "\xE2\x8B\xA9"},
    {"succsim", "\xE2\x89\xBF"},
    {"sum", "\xE2\x88\x91"},
    {"sung", "\xE2\x99\xAA"},
    {"sup", "\xE2\x8A\x83"},
    {"sup1", "\xC2\xB9"},
    {"sup2", "\xC2\xB2"},
    {"sup3", "\xC2\xB3"},
    {"supE", "\xE2\xAB\x86"},
    {"supdot", "\xE2\xAA\xBE"},
    {"supdsub", "\xE2\xAB\x98"},
    {"supe", "\xE2\x8A\x87"},
    {"supedot", "\xE2\xAB\x84"},
    {"suphsol", "\xE2\x9F\x89"},
    {"suphsub", "\xE2\xAB\x97"},
    {"suplarr", "\xE2\xA5\xBB"},
    {"supmult", "\xE2\xAB\x82"},
    {"supnE", "\xE2\xAB\x8C"},
    {"supne", "\xE2\x8A\x8B"},
    {"supplus", "\xE2\xAB\x80"},
    {"supset", "\xE2\x8A\x83"},
    {"supseteq", "\xE2\x8A\x87"},
    {"supseteqq", "\xE2\xAB\x86"},
    {"supsetneq", "\xE2\x8A\x8B"},
    {"supsetneqq", "\xE2\xAB\x8C"},
    {"supsim", "\xE2\xAB\x88"},
    {"supsub", "\xE2\xAB\x94"},
    {"supsup", "\xE2\xAB\x96"},
    {"swArr", "\xE2\x87\x99"},
    {"swarhk", "\xE2\xA4\xA6"},
    {"swarr", "\xE2\x86\x99"},
    {"swarrow", "\xE2\x86\x99"},
    {"swnwar", "\xE2\xA4\xAA"},
    {"szlig", "\xC3\x9F"},
    {"target", "\xE2\x8C\x96"},
    {"tau", "\xCF\x84"},
    {"tbrk", "\xE2\x8E\xB4"},
    {"tcaron", "\xC5\xA5"},
    {"tcedil", "\xC5\xA3"},
    {"tcy", "\xD1\x82"},
    {"tdot", "\xE2\x83\x9B"},
    {"telrec", "\xE2\x8C\x95"},
    {"tfr", "\xF0\x9D\x94\xB1"},
    {"there4", "\xE2\x88\xB4"},
    {"therefore", "\xE2\x88\xB4"},
    {"theta", "\xCE\xB8"},
    {"thetasym", "\xCF\x91"},
    {"thetav", "\xCF\x91"},
    {"thickapprox", "\xE2\x89\x88"},
    {"thicksim", "\xE2\x88\xBC"},
    {"thinsp", "\xE2\x80\x89"},
    {"thkap", "\xE2\x89\x88"},
    {"thksim", "\xE2\x88\xBC"},
    {"thorn", "\xC3\xBE"},
    {"tilde", "\xCB\x9C"},
    {"times", "\xC3\x97"},
    {"timesb", "\xE2\x8A\xA0"},
    {"timesbar", "\xE2\xA8\xB1"},
    {"timesd", "\xE2\xA8\xB0"},
    {"tint", "\xE2\x88\xAD"},
    {"toea", "\xE2\xA4\xA8"},
    {"top", "\xE2\x8A\xA4"},
    {"topbot", "\xE2\x8C\xB6"},
    {"topcir", "\xE2\xAB\xB1"},
    {"topf", "\xF0\x9D\x95\xA5"},
    {"topfork", "\xE2\xAB\x9A"},
    {"tosa", "\xE2\xA4\xA9"},
    {"tprime", "\xE2\x80\xB4"},
    {"trade", "\xE2\x84\xA2"},
    {"triangle", "\xE2\x96\xB5"},
    {"triangledown", "\xE2\x96\xBF"},
    {"triangleleft", "\xE2\x97\x83"},
    {"trianglelefteq", "\xE2\x8A\xB4"},
    {"triangleq", "\xE2\x89\x9C"},
    {"triangleright", "\xE2\x96\xB9"},
    {"trianglerighteq", "\xE2\x8A\xB5"},
    {"tridot", "\xE2\x97\xAC"},
    {"trie", "\xE2\x89\x9C"},
    {"triminus", "\xE2\xA8\xBA"},
    {"triplus", "\xE2\xA8\xB9"},
    {"trisb", "\xE2\xA7\x8D"},
    {"tritime", "\xE2\xA8\xBB"},
    {"trpezium", "\xE2\x8F\xA2"},
    {"tscr", "\xF0\x9D\x93\x89"},
    {"tscy", "\xD1\x86"},
    {"tshcy", "\xD1\x9B"},
    {"tstrok", "\xC5\xA7"},
    {"twixt", "\xE2\x89\xAC"},
    {"twoheadleftarrow", "\xE2\x86\x9E"},
    {"twoheadrightarrow", "\xE2\x86\xA0"},
    {"uArr", "\xE2\x87\x91"},
    {"uHar", "\xE2\xA5\xA3"},
    {"uacute", "\xC3\xBA"},
    {"uarr", "\xE2\x86\x91"},
    {"ubrcy", "\xD1\x9E"},
    {"ubreve", "\xC5\xAD"},
    {"ucirc", "\xC3\xBB"},
    {"ucy", "\xD1\x83"},
    {"udarr", "\xE2\x87\x85"},
    {"udblac", "\xC5\xB1"},
    {"udhar", "\xE2\xA5\xAE"},
    {"ufisht", "\xE2\xA5\xBE"},
    {"ufr", "\xF0\x9D\x94\xB2"},
    {"ugrave", "\xC3\xB9"},
    {"uharl", "\xE2\x86\xBF"},
    {"uharr", "\xE2\x86\xBE"},
    {"uhblk", "\xE2\x96\x80"},
    {"ulcorn", "\xE2\x8C\x9C"},
    {"ulcorner", "\xE2\x8C\x9C"},
    {"ulcrop", "\xE2\x8C\x8F"},
    {"ultri", "\xE2\x97\xB8"},
    {"umacr", "\xC5\xAB"},
    {"uml", "\xC2\xA8"},
    {"uogon", "\xC5\xB3"},
    {"uopf", "\xF0\x9D\x95\xA6"},
    {"uparrow", "\xE2\x86\x91"},
    {"updownarrow", "\xE2\x86\x95"},
    {"upharpoonleft", "\xE2\x86\xBF"},
    {"upharpoonright", "\xE2\x86\xBE"},
    {"uplus", "\xE2\x8A\x8E"},
    {"upsi", "\xCF\x85"},
    {"upsih", "\xCF\x92"},
    {"upsilon", "\xCF\x85"},
    {"upuparrows", "\xE2\x87\x88"},
    {"urcorn", "\xE2\x8C\x9D"},
    {"urcorner", "\xE2\x8C\x9D"},
    {"urcrop", "\xE2\x8C\x8E"},
    {"uring", "\xC5\xAF"},
    {"urtri", "\xE2\x97\xB9"},
    {"uscr", "\xF0\x9D\x93\x8A"},
    {"utdot", "\xE2\x8B\xB0"},
    {"utilde", "\xC5\xA9"},
    {"utri", "\xE2\x96\xB5"},
    {"utrif", "\xE2\x96\xB4"},
    {"uuarr", "\xE2\x87\x88"},
    {"uuml", "\xC3\xBC"},
    {"uwangle", "\xE2\xA6\xA7"},
    {"vArr", "\xE2\x87\x95"},
    {"vBar", "\xE2\xAB\xA8"},
    {"vBarv", "\xE2\xAB\xA9"},
    {"vDash", "\xE2\x8A\xA8"},
    {"vangrt", "\xE2\xA6\x9C"},
    {"varepsilon", "\xCF\xB5"},
    {"varkappa", "\xCF\xB0"},
    {"varnothing", "\xE2\x88\x85"},
    {"varphi", "\xCF\x95"},
    {"varpi", "\xCF\x96"},
    {"varpropto", "\xE2\x88\x9D"},
    {"varr", "\xE2\x86\x95"},
    {"varrho", "\xCF\xB1"},
    {"varsigma", "\xCF\x82"},
    {"varsubsetneq", "\xE2\x8A\x8A\xEF\xB8\x80"},
    {"varsubsetneqq", "\xE2\xAB\x8B\xEF\xB8\x80"},
    {"varsupsetneq", "\xE2\x8A\x8B\xEF\xB8\x80"},
    {"varsupsetneqq", "\xE2\xAB\x8C\xEF\xB8\x80"},
    {"vartheta", "\xCF\x91"},
    {"vartriangleleft", "\xE2\x8A\xB2"},
    {"vartriangleright", "\xE2\x8A\xB3"},
    {"vcy", "\xD0\xB2"},
    {"vdash", "\xE2\x8A\xA2"},
    {"vee", "\xE2\x88\xA8"},
    {"veebar", "\xE2\x8A\xBB"},
    {"veeeq", "\xE2\x89\x9A"},
    {"vellip", "\xE2\x8B\xAE"},
    {"verbar", "\x7C"},
    {"vert", "\x7C"},
    {"vfr", "\xF0\x9D\x94\xB3"},
    {"vltri", "\xE2\x8A\xB2"},
    {"vnsub", "\xE2\x8A\x82\xE2\x83\x92"},
    {"vnsup", "\xE2\x8A\x83\xE2\x83\x92"},
    {"vopf", "\xF0\x9D\x95\xA7"},
    {"vprop", "\xE2\x88\x9D"},
    {"vrtri", "\xE2\x8A\xB3"},
    {"vscr", "\xF0\x9D\x93\x8B"},
    {"vsubnE", "\xE2\xAB\x8B\xEF\xB8\x80"},
    {"vsubne", "\xE2\x8A\x8A\xEF\xB8\x80"},
    {"vsupnE", "\xE2\xAB\x8C\xEF\xB8\x80"},
    {"vsupne", "\xE2\x8A\x8B\xEF\xB8\x80"},
    {"vzigzag", "\xE2\xA6\x9A"},
    {"wcirc", "\xC5\xB5"},
    {"wedbar", "\xE2\xA9\x9F"},
    {"wedge", "\xE2\x88\xA7"},
    {"wedgeq", "\xE2\x89\x99"},
    {"weierp", "\xE2\x84\x98"},
    {"wfr", "\xF0\x9D\x94\xB4"},
    {"wopf", "\xF0\x9D\x95\xA8"},
    {"wp", "\xE2\x84\x98"},
    {"wr", "\xE2\x89\x80"},
    {"wreath", "\xE2\x89\x80"},
    {"wscr", "\xF0\x9D\x93\x8C"},
    {"xcap", "\xE2\x8B\x82"},
    {"xcirc", "\xE2\x97\xAF"},
    {"xcup", "\xE2\x8B\x83"},
    {"xdtri", "\xE2\x96\xBD"},
    {"xfr", "\xF0\x9D\x94\xB5"},
    {"xhArr", "\xE2\x9F\xBA"},
    {"xharr", "\xE2\x9F\xB7"},
    {"xi", "\xCE\xBE"},
    {"xlArr", "\xE2\x9F\xB8"},
    {"xlarr", "\xE2\x9F\xB5"},
    {"xmap", "\xE2\x9F\xBC"},
    {"xnis", "\xE2\x8B\xBB"},
    {"xodot", "\xE2\xA8\x80"},
    {"xopf", "\xF0\x9D\x95\xA9"},
    {"xoplus", "\xE2\xA8\x81"},
    {"xotime", "\xE2\xA8\x82"},
    {"xrArr", "\xE2\x9F\xB9"},
    {"xrarr", "\xE2\x9F\xB6"},
    {"xscr", "\xF0\x9D\x93\x8D"},
    {"xsqcup", "\xE2\xA8\x86"},
    {"xuplus", "\xE2\xA8\x84"},
    {"xutri", "\xE2\x96\xB3"},
    {"xvee", "\xE2\x8B\x81"},
    {"xwedge", "\xE2\x8B\x80"},
    {"yacute", "\xC3\xBD"},
    {"yacy", "\xD1\x8F"},
    {"ycirc", "\xC5\xB7"},
    {"ycy", "\xD1\x8B"},
    {"yen", "\xC2\xA5"},
    {"yfr", "\xF0\x9D\x94\xB6"},
    {"yicy", "\xD1\x97"},
    {"yopf", "\xF0\x9D\x95\xAA"},
    {"yscr", "\xF0\x9D\x93\x8E"},
    {"yucy", "\xD1\x8E"},
    {"yuml", "\xC3\xBF"},
    {"zacute", "\xC5\xBA"},
    {"zcaron", "\xC5\xBE"},
    {"zcy", "\xD0\xB7"},
    {"zdot", "\xC5\xBC"},
    {"zeetrf", "\xE2\x84\xA8"},
    {"zeta", "\xCE\xB6"},
    {"zfr", "\xF0\x9D\x94\xB7"},
    {"zhcy", "\xD0\xB6"},
    {"zigrarr", "\xE2\x87\x9D"},
    {"zopf", "\xF0\x9D\x95\xAB"},
    {"zscr", "\xF0\x9D\x93\x8F"},
    {"zwj", "\xE2\x80\x8D"},
    {"zwnj", "\xE2\x80\x8C"},
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
        encodeUnicode(out, UnicodeType::UTF8, normalizeNumericCharacterReference(point));
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
Array<Owned<Span>> parseInlineSpans(StringView markdown, const ParseOptions& options) {
    Owned<Parser> parser = createParser(options);
    return expandInlineSpans(parser, markdown);
}

// Convenience helper that parses inline Markdown and returns rendered HTML without block markup.
String convertInlineToHtml(StringView src, const ParseOptions& parseOptions, const HTMLOptions& htmlOptions) {
    Array<Owned<Span>> spans = parseInlineSpans(src, parseOptions);
    MemStream out;
    for (const Span* span : spans) {
        convertSpanToHtml(&out, span, htmlOptions);
    }
    return out.moveToString();
}

// Convenience helper that parses an entire Markdown string into a list of top-level blocks.
Array<Owned<Block>> parse(StringView markdown, const ParseOptions& options) {
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

// Returns true if the table's header row is empty.
static bool isTableHeaderEmpty(const Block::Table* table) {
    if (!table->childBlocks)
        return true;
    const Block* headerBlock = table->childBlocks[0];
    PLY_ASSERT(headerBlock->var.is<Block::TableRow>());
    for (const Block* cell : headerBlock->var.as<Block::TableRow>()->childBlocks) {
        if (cell->var.as<Block::TableCell>()->spans)
            return false;
    }
    return true;
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
        outs->write("<table>\n");
        // The header row is only part of the output when it has visible content; an all-empty
        // header row exists only to satisfy the Markdown syntax and is dropped entirely.
        bool hasHeader = !isTableHeaderEmpty(table);
        if (hasHeader) {
            outs->write("<thead>\n");
            convertToHtml(outs, table->childBlocks[0], options);
            outs->write("</thead>\n");
        }
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
        bool isHeader = (table->childBlocks[0] == rowBlock) && !isTableHeaderEmpty(table);
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
