/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-markdown.h"

namespace ply {
namespace markdown {

//------------------------------------------------------------------
// Parser implementation details not exposed in the public API.
//------------------------------------------------------------------
struct Parser {
    // The current stack of nested Markdown blocks based on the content of previous lines.
    // Consists of ListItems and BlockQuotes.
    Array<Block*> activeBlocks;

    // The current leaf block, if any; Paragraphs, Headings, IndentedCodeBlocks and FencedCodeBlocks go here.
    Block* leafBlock = nullptr;

    // Accumulates raw text to be added to the leaf block.
    // Inline delimiter spans are parsed when the leaf block is flushed.
    MemStream rawLeafText;

    // Root block of the document. Top-level blocks are popped from the front and returned to the caller as we go.
    Block rootBlock;

    // Only used if leafBlock is IndentedCodeBlock.
    u32 numBlankLinesInIndentedCodeBlock = 0;

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
bool parseOpeningFence(StringView remainingLine, u32 relativeIndent, Block::FencedCodeBlock& outFenced) {
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
    outFenced.infoString = String{info};
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
            // Consume additional spaces.
            while (ctReader.point == ' ' || ctReader.point == '\t') {
                ctReader.advance();
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

// Marks ancestor lists as "loose if continued" when a blank line is seen inside them.
void markContainingListsLooseIfContinued(Parser* parser) {
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

    // Terminate paragraph if any.
    if (parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>()) {
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
    } else if (parentCtr->var.is<Block::ListItem>()) {
        // Begin new list as a sublist of existing list
        parentCtr = parentCtr->parent;
        PLY_ASSERT(parentCtr->var.is<Block::List>());
        parentInner = parentCtr->asInner();
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
        if (isThematicBreak(ctReader.viewRemaining(), lp.relativeIndent()))
            break;

        ColumnTrackingReader savedPos = ctReader;

        if (ctReader.point == '>') {
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
        } else if (ctReader.point == '*' || ctReader.point == '-' || ctReader.point == '+') {
            char punctuator = numericCast<char>(ctReader.point);
            ctReader.advance();
            // It's an unordered list item.
            if (!tryStartListItem(parser, lp, punctuator, -1))
                goto notMarker;
        } else if (ctReader.point >= '0' && ctReader.point <= '9') {
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
        // A non-blank continuation after a blank line turns pending lists loose.
        for (Block* block : parser->activeBlocks) {
            if (block->var.is<Block::ListItem>()) {
                auto* parentList = block->parent->var.as<Block::List>();
                PLY_ASSERT(parentList);
                if (parentList->isLooseIfContinued) {
                    parentList->isLoose = true;
                    parentList->isLooseIfContinued = false;
                }
            }
        }
        parser->checkListContinuations = false;
    }
    if (!hasPara && lp.relativeIndent() >= 4) {
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
            Block::FencedCodeBlock newFenced;
            if (parseOpeningFence(lp.ctReader.viewRemaining(), lp.relativeIndent(), newFenced)) {
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
            if (hasPara && isSetextUnderline(lp.ctReader.viewRemaining(), lp.relativeIndent(), &setextLevel)) {
                // Convert current paragraph block to a Setext heading.
                PLY_ASSERT(parser->leafBlock->var.is<Block::Paragraph>());
                auto& heading = parser->leafBlock->var.switchTo<Block::Heading>();
                heading.level = setextLevel;
                finalizeLeafBlock(parser);
                return;
            }

            if (isThematicBreak(lp.ctReader.viewRemaining(), lp.relativeIndent())) {
                // Thematic breaks terminate an open paragraph and become a standalone block.
                if (hasPara) {
                    finalizeLeafBlock(parser);
                }
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                addBlock<Block::ThematicBreak>(parent);
                return;
            }

            if (remainingText.startsWith('#') && (lp.relativeIndent() <= 3)) {
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
                    if (StringView remainingText = in.viewRemainingBytes().trim()) {
                        parser->rawLeafText.write(remainingText);
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
// Used when determining whether a neighboring delimiter is left or right-flanking.
inline bool isAscPunc(char c) {
    return (c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e);
}

// Token produced by inline scanning. It can represent raw text, emphasis runs, link markers, or a completed span.
struct Delimiter {
    enum Type {
        RawText,
        Stars,
        Underscores,
        OpenLink,
        InlineElem,
    };

    Type type = RawText;
    bool leftFlanking = false;  // Stars & Underscores only
    bool rightFlanking = false; // Stars & Underscores only
    bool active = true;         // OpenLink only
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
        bool precededByWhite = (start == 0) || isWhite(rawLine[start - 1]);
        bool followedByWhite = (start + numBytes >= rawLine.numBytes()) || isWhite(rawLine[start + numBytes]);
        bool precededByPunc = (start > 0) && isAscPunc(rawLine[start - 1]);
        bool followedByPunc = (start + numBytes < rawLine.numBytes()) && isAscPunc(rawLine[start + numBytes]);

        Delimiter result{type, rawLine.substr(start, numBytes)};
        result.leftFlanking =
            !followedByWhite && (!followedByPunc || (followedByPunc && (precededByWhite || precededByPunc)));
        result.rightFlanking =
            !precededByWhite && (!precededByPunc || (precededByPunc && (followedByWhite || followedByPunc)));
        return result;
    }
};

// Result of parsing a link destination after a closing ']'.
struct LinkDestination {
    bool success = false;
    String dest;
};

// Parses a link destination from rawText starting at pos and advances pos past the closing ')' on success.
LinkDestination parseLinkDestination(StringView rawText, u32* pos) {
    // FIXME: Support < > destinations
    // FIXME: Support link titles

    u32 i = *pos;

    // Skip initial whitespace
    while (i < rawText.numBytes() && isWhite(rawText[i])) {
        i++;
    }
    if (i >= rawText.numBytes())
        return {false, String{}};

    MemStream mout;
    u32 parenNestLevel = 0;
    for (; i < rawText.numBytes(); i++) {
        char c = rawText[i];
        if (c == '\n')
            break;

        if (c == '\\') {
            i++;
            if (i >= rawText.numBytes() || rawText[i] == '\n') {
                mout.write('\\');
                break;
            }
            c = rawText[i];
            if (!isAscPunc(c)) {
                mout.write('\\');
            }
            mout.write(c);
        } else if (c == '(') {
            mout.write(c);
            parenNestLevel++;
        } else if (c == ')') {
            if (parenNestLevel > 0) {
                mout.write(c);
                parenNestLevel--;
            } else {
                break;
            }
        } else if (c >= 0 && c <= 32)
            break;
        else {
            mout.write(c);
        }
    }

    if (parenNestLevel != 0)
        return {false, String{}};

    // Skip trailing whitespace
    while (i < rawText.numBytes() && isWhite(rawText[i])) {
        i++;
    }
    if (i >= rawText.numBytes() || rawText[i] != ')')
        return {false, String{}};

    i++;
    *pos = i;
    return {true, mout.moveToString()};
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

// Resolves '*' and '_' delimiter runs into italic/bold spans in-place, then returns inline elems from bottomPos.
Array<Owned<Span>> processEmphasis(Array<Delimiter>& delimiters, u32 bottomPos) {
    u32 starOpener = bottomPos;
    u32 underscoreOpener = bottomPos;
    for (u32 pos = bottomPos; pos < delimiters.numItems(); pos++) {
        auto handleCloser = [&](Delimiter::Type type, u32& openerPos) {
            for (u32 j = pos; j > openerPos;) {
                --j;
                if (delimiters[j].type == type && delimiters[j].leftFlanking) {
                    u32 spanLength = min(delimiters[j].text.numBytes(), delimiters[pos].text.numBytes());
                    PLY_ASSERT(spanLength > 0);
                    Owned<Span> emphSpan;
                    if (spanLength >= 2) {
                        emphSpan = makeSpan<Span::Bold>();
                    } else {
                        emphSpan = makeSpan<Span::Italic>();
                    }
                    emphSpan->asContainer()->childSpans = convertToInlineElems(delimiters.subview(j + 1, pos - j - 1));
                    u32 delimsToSubtract = min(spanLength, 2u);
                    delimiters[j].text = delimiters[j].text.left(delimiters[j].text.numBytes() - delimsToSubtract);
                    delimiters[pos].text =
                        delimiters[pos].text.left(delimiters[pos].text.numBytes() - delimsToSubtract);
                    // We're going to delete from j to pos inclusive, so leave remaining
                    // delimiters if any
                    if (!delimiters[j].text.isEmpty()) {
                        j++;
                    }
                    if (!delimiters[pos].text.isEmpty()) {
                        pos--;
                    }
                    delimiters.erase(j, pos + 1 - j);
                    delimiters.insert(j) = std::move(emphSpan);
                    pos = j;
                    starOpener = min(starOpener, pos + 1);
                    underscoreOpener = min(starOpener, pos + 1);
                    return;
                }
            }
            // None found
            openerPos = pos + 1;
        };
        if (delimiters[pos].type == Delimiter::Stars && delimiters[pos].rightFlanking) {
            handleCloser(Delimiter::Stars, starOpener);
        } else if (delimiters[pos].type == Delimiter::Underscores && delimiters[pos].rightFlanking) {
            handleCloser(Delimiter::Underscores, underscoreOpener);
        }
    }
    Array<Owned<Span>> result = convertToInlineElems(delimiters.subview(bottomPos));
    delimiters.resize(bottomPos);
    return result;
}

// Parses inline Markdown spans within rawText and returns the expanded span sequence.
Array<Owned<Span>> expandInlineSpans(StringView rawText) {
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
        if (c == '\n') {
            // At line boundaries, trailing spaces are trimmed and can convert to hard breaks.
            u32 savedPos = i;
            while (i > flushedIndex && rawText[i - 1] == ' ') {
                i--;
            }
            bool hardBreakFromSpaces = (savedPos - i >= 2);
            flushText();
            i = savedPos + 1;
            flushedIndex = i;
            if (i < rawText.numBytes()) {
                if (hardBreakFromSpaces) {
                    delimiters.append(makeSpan<Span::HardBreak>());
                } else {
                    delimiters.append(makeSpan<Span::SoftBreak>());
                }
            }
            continue;
        }

        if (c == '`') {
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
        } else if (c == '*') {
            flushText();
            u32 runLength = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '*'; i++) {
                runLength++;
            }
            delimiters.append(Delimiter::makeRun(Delimiter::Stars, rawText, i - runLength, runLength));
            flushedIndex = i;
        } else if (c == '_') {
            flushText();
            u32 runLength = 1;
            for (i++; i < rawText.numBytes() && rawText[i] == '_'; i++) {
                runLength++;
            }
            delimiters.append(Delimiter::makeRun(Delimiter::Underscores, rawText, i - runLength, runLength));
            flushedIndex = i;
        } else if (c == '\\') {
            flushText();
            i++;
            if (i >= rawText.numBytes()) {
                delimiters.append({Delimiter::RawText, StringView{"\\"}});
                flushedIndex = i;
            } else if (rawText[i] == '\n') {
                delimiters.append(makeSpan<Span::HardBreak>());
                i++;
                flushedIndex = i;
            } else if (isAscPunc(rawText[i])) {
                delimiters.append({Delimiter::RawText, rawText.substr(i, 1)});
                i++;
                flushedIndex = i;
            } else {
                delimiters.append({Delimiter::RawText, StringView{"\\"}});
                flushedIndex = i;
            }
        } else if (c == '[') {
            flushText();
            delimiters.append({Delimiter::OpenLink, rawText.substr(i, 1)});
            i++;
            flushedIndex = i;
        } else if (c == ']') {
            // Try to parse an inline link
            flushText();
            i++;
            if (!(i < rawText.numBytes() && rawText[i] == '('))
                continue; // No parenthesis

            // Got opening parenthesis
            i++;

            // Look for preceding OpenLink delimiter
            s32 openLink =
                reverseFind(delimiters, [](const Delimiter& delim) { return delim.type == Delimiter::OpenLink; });
            if (openLink < 0)
                continue; // No preceding OpenLink delimiter

            // Found a preceding OpenLink delimiter
            // Try to parse link destination
            u32 backup = i;
            LinkDestination linkDest = parseLinkDestination(rawText, &i);
            if (!linkDest.success) {
                // Couldn't parse link destination
                i = backup;
                continue;
            }

            // Successfully parsed link destination
            Owned<Span> linkSpan = makeSpan<Span::Link>();
            linkSpan->var.as<Span::Link>()->destination = std::move(linkDest.dest);
            linkSpan->asContainer()->childSpans = processEmphasis(delimiters, openLink + 1);
            delimiters.resize(openLink);
            delimiters.append(std::move(linkSpan));
            flushedIndex = i;
        } else {
            i++;
        }
    }

    flushText();
    return processEmphasis(delimiters, 0);
}

// Finalizes parser->leafBlock by moving raw text into spans, then clears leaf parsing state.
void finalizeLeafBlock(Parser* parser) {
    if (!parser->leafBlock)
        return;

    Block::Leaf* leaf = parser->leafBlock->asLeaf();
    PLY_ASSERT(leaf);
    PLY_ASSERT(leaf->spans.isEmpty());
    String rawText = parser->rawLeafText.moveToString();
    new (&parser->rawLeafText) MemStream;
    if (parser->leafBlock->var.is<Block::IndentedCodeBlock>() || parser->leafBlock->var.is<Block::FencedCodeBlock>()) {
        if (rawText) {
            Owned<Span> textSpan = makeSpan<Span::Text>();
            textSpan->var.as<Span::Text>()->text = std::move(rawText);
            leaf->spans.append(std::move(textSpan));
        }
    } else {
        leaf->spans = expandInlineSpans(rawText);
    }
    parser->leafBlock = nullptr;
    parser->numBlankLinesInIndentedCodeBlock = 0;
}

// For non-blank lines that no longer match all open containers, either keep a paragraph as a lazy continuation
// (by restoring full block depth) or finalize the current leaf and trim active containers to the matched depth.
void closeBlocksIfNotLazyContinuation(LineParser& lp) {
    Parser* parser = lp.parser;
    if (lp.blockDepth >= parser->activeBlocks.numItems())
        return;

    bool canLazyContinueParagraph = parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>();
    if (canLazyContinueParagraph) {
        Block::FencedCodeBlock maybeFence;
        u32 maybeSetextLevel = 0;
        for (u32 i = lp.blockDepth; i < parser->activeBlocks.numItems(); i++) {
            if (!parser->activeBlocks[i]->var.is<Block::BlockQuote>()) {
                canLazyContinueParagraph = false;
                break;
            }
        }
        if (canLazyContinueParagraph && parseOpeningFence(lp.ctReader.viewRemaining(), lp.relativeIndent(), maybeFence)) {
            canLazyContinueParagraph = false;
        }
        if (canLazyContinueParagraph && isThematicBreak(lp.ctReader.viewRemaining(), lp.relativeIndent())) {
            canLazyContinueParagraph = false;
        }
        if (canLazyContinueParagraph && isSetextUnderline(lp.ctReader.viewRemaining(), lp.relativeIndent(), &maybeSetextLevel)) {
            canLazyContinueParagraph = false;
        }
    }

    if (canLazyContinueParagraph) {
        lp.blockDepth = parser->activeBlocks.numItems();
    } else {
        finalizeLeafBlock(parser);
        parser->activeBlocks.resize(lp.blockDepth);
    }
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

// Creates a parser with an initialized root container block.
Owned<Parser> createParser() {
    Owned<Parser> parser = Heap::create<Parser>();
    parser->rootBlock.var.switchTo<Block::BlockQuote>();
    return parser;
}

// Creates an independent deep copy of a parser, including any unfinished block.
Parser* duplicate(Parser* parser) {
    Parser* result = Heap::create<Parser>();

    // Duplicate the owned tree and scalar parsing state.
    result->rootBlock = parser->rootBlock;
    result->numBlankLinesInIndentedCodeBlock = parser->numBlankLinesInIndentedCodeBlock;
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

    // Give the duplicate its own copy of the unfinished leaf text.
    result->rawLeafText = parser->rawLeafText.duplicate();
    return result;
}

// Parses one source line and returns the next completed top-level block, if one becomes available.
Owned<Block> parseLine(Parser* parser, StringView line) {
    LineParser lp{parser, line};

    // Match existing indentation and blockquote '>' markers.
    matchExistingIndentation(lp);

    bool handledFencedLine = false;
    if (auto* fenced = parser->leafBlock ? parser->leafBlock->var.as<Block::FencedCodeBlock>() : nullptr) {
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

    if (!handledFencedLine) {
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
            // Parse new markers.
            parseNewMarkers(lp);
            // Handle remaining paragraph text.
            parseParagraphText(lp);
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

// Convenience helper that parses an entire Markdown string into a list of top-level blocks.
Array<Owned<Block>> parseWholeDocument(StringView markdown) {
    Array<Owned<Block>> blocks;
    Owned<Parser> parser = createParser();
    ViewStream in{markdown};

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
String convertToHtml(StringView src) {
    ViewStream in{src};
    MemStream out;
    markdown::HTMLOptions options;
    Owned<Parser> parser = createParser();

    while (StringView line = readLine(in)) {
        if (Owned<Block> block = parseLine(parser, line)) {
            convertToHtml(&out, block, options);
        }
    }
    if (Owned<Block> block = flush(parser)) {
        convertToHtml(&out, block, options);
    }

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
    } else if (auto* code = span->var.as<Span::Code>()) {
        outs->write("code \"");
        printEscapedString(*outs, code->text);
        outs->write('"');
    } else if (span->var.is<Span::SoftBreak>()) {
        outs->write("softbreak");
    } else if (span->var.is<Span::HardBreak>()) {
        outs->write("hardbreak");
    } else if (span->var.is<Span::Italic>()) {
        outs->write("italic");
    } else if (span->var.is<Span::Bold>()) {
        outs->write("bold");
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
    } else if (block->var.is<Block::ListItem>()) {
        outs->write("item");
    } else if (block->var.is<Block::BlockQuote>()) {
        outs->write("block_quote");
    } else if (auto* heading = block->var.as<Block::Heading>()) {
        outs->format("heading level={}", heading->level);
    } else if (block->var.is<Block::Paragraph>()) {
        outs->write("paragraph");
    } else if (block->var.is<Block::IndentedCodeBlock>()) {
        outs->write("indented_code_block");
    } else if (block->var.is<Block::FencedCodeBlock>()) {
        outs->write("fenced_code_block");
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

// Renders one inline span subtree to HTML.
void convertSpanToHtml(Stream* outs, const Span* span, const HTMLOptions& options) {
    if (auto* text = span->var.as<Span::Text>()) {
        printXmlEscapedString(*outs, text->text);
    } else if (auto* link = span->var.as<Span::Link>()) {
        String destination = link->destination;
        if (options.filterLinks) {
            destination = options.filterLinks(destination);
        }
        outs->format("<a href=\"{:&}\">", destination);
        for (const Span* child : link->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</a>");
    } else if (auto* code = span->var.as<Span::Code>()) {
        outs->format("<code>{:&}</code>", code->text);
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
        if (!parentList->isLoose && listItem->childBlocks && listItem->childBlocks[0]->var.is<Block::Paragraph>()) {
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
    } else if (block->var.is<Block::ThematicBreak>()) {
        outs->write("<hr />\n");
    } else {
        PLY_ASSERT(0);
    }
}

} // namespace markdown
} // namespace ply
