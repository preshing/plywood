/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-markdown.h"

namespace ply {
namespace markdown {

//------------------------------------------------------------------
// Private Parser implementation not exposed in the public API.
//------------------------------------------------------------------

struct Parser {
    Array<Block*> activeBlocks;
    Block* leafBlock = nullptr;
    Block rootBlock;

    // Only used if leafBlock is CodeBlock:
    u32 numBlankLinesInCodeBlock = 0;

    // This flag indicates that some Lists on the stack have their isLooseIfContinued flag set: (Alternatively, we
    // *could* store the number of such Lists on the stack, and eliminate the isLooseIfContinued flag completely, but
    // it would complicate matchExistingIndentation a little bit. Sticking with this approach for now.)
    bool checkListContinuations = false;
};

//------------------------------------------------------------------
// LineParser contains all the internal state used while parsing a single line of input.
//------------------------------------------------------------------

struct LineParser {
    Parser* parser = nullptr;

    // Keeps track of the current read position.
    ViewStream in;

    // Keeps track of how many entries in Parser::activeBlocks were matched by current line's indentation and
    // blockquote > markers.
    u32 blockDepth = 0;

    // If the last matching stack entry was a blockquote, this is the column number after the > marker and optional
    // following single space (if any). If the last matching stack entry was a list item, this is the column number
    // where sufficient indentation was reached for the rest of the line to be considered part of the list item. Note
    // that different lines can have different outerColumn numbers for the same stack entry, because blockquote >
    // markers can be preceded by a different number (from 0 to 3) of spaces on each line.
    u32 outerColumn = 0;

    // The number of columns of leading indentation (including blockquote > markers) that have been read on this line.
    u32 column = 0;

    // How much leading space was encountered on this line after outerColumn.
    u32 relativeIndent() const {
        return this->column - this->outerColumn;
    }

    // Constructor.
    LineParser(Parser* parser, StringView line) : parser{parser}, in{line} {
    }
};

//  ▄▄▄▄▄  ▄▄▄               ▄▄         ▄▄▄▄▄                       ▄▄
//  ██  ██  ██   ▄▄▄▄   ▄▄▄▄ ██  ▄▄     ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀█▄  ██  ██  ██ ██    ██▄█▀      ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██▄▄█▀ ▄██▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄     ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                                                             ▄▄▄█▀

// Code to parse blocks (first pass).

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

// Helper function to extract a line from a code block without leading indentation.
String extractCodeLine(StringView line, u32 fromIndent) {
    u32 indent = 0;
    for (u32 i = 0; i < line.numBytes(); i++) {
        if (indent == fromIndent) {
            return line.substr(i);
        }
        u8 c = line[i];
        PLY_ASSERT(c < 128);              // No high code points
        PLY_ASSERT(c >= 32 || c == '\t'); // No control characters
        if (c == '\t') {
            u32 tabSize = 4;
            u32 newIndent = indent + tabSize - (indent % tabSize);
            if (newIndent > fromIndent) {
                return StringView{" "} * (newIndent - fromIndent) + line.substr(i + 1);
            }
            indent = newIndent;
        } else {
            indent++;
        }
    }
    PLY_ASSERT(0);
    return {};
}

// This is called at the start of each line. It figures out which of the existing blocks we are still inside by
// consuming indentation and blockquote '>' markers that match activeBlocks.
void matchExistingIndentation(LineParser& lp) {
    Parser* parser = lp.parser;

    // Consume leading spaces.
    while (lp.in.hasRemainingBytes() && (*lp.in.curByte == ' ')) {
        lp.in.curByte++;
        lp.column++;
    }

    // Iterate over stack items, matching as much leading indentation and BlockQuote '>' markers as possible.
    PLY_ASSERT(lp.blockDepth == 0);
    while (lp.blockDepth < parser->activeBlocks.numItems()) {
        Block* block = parser->activeBlocks[lp.blockDepth];
        if (block->var.is<Block::BlockQuote>()) {
            // If there is a '>' within 3 columns of outerColumn, match this BlockQuote.
            if (lp.in.hasRemainingBytes() && (*lp.in.curByte == '>') && (lp.relativeIndent() <= 3)) {
                lp.blockDepth++;
                lp.in.curByte++;
                lp.column++;
                if (lp.in.hasRemainingBytes() && (*lp.in.curByte == ' ')) {
                    // Read optional space after '>'.
                    lp.in.curByte++;
                    lp.column++;
                }
                lp.outerColumn = lp.column;
                continue;
            }
            // Consume additional spaces.
            while (lp.in.hasRemainingBytes() && (*lp.in.curByte == ' ')) {
                lp.in.curByte++;
                lp.column++;
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

// This is called after matchExistingIndentation() if the remainder of the line is blank.
void handleBlankLine(LineParser& lp) {
    Parser* parser = lp.parser;

    // Terminate paragraph if any.
    if (parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>()) {
        parser->leafBlock = nullptr;
        PLY_ASSERT(parser->numBlankLinesInCodeBlock == 0);
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
        parser->leafBlock = nullptr;
        parser->numBlankLinesInCodeBlock = 0;
    }

    if (parser->leafBlock) {
        // At this point, the only possible leaf block is a CodeBlock, because Paragraphs are terminated above, and
        // Headings don't persist across lines.
        PLY_ASSERT(parser->leafBlock->var.is<Block::CodeBlock>());
        Block::Leaf* leaf = parser->leafBlock->asLeaf();
        // Count blank lines in CodeBlocks
        if (lp.column - lp.outerColumn > 4) {
            // Add intermediate blank lines.
            for (u32 i = 0; i < parser->numBlankLinesInCodeBlock; i++) {
                leaf->rawLines.append("\n");
            }
            parser->numBlankLinesInCodeBlock = 0;
            String codeLine = extractCodeLine({lp.in.view.startByte, lp.in.endByte}, lp.outerColumn + 4);
            leaf->rawLines.append(std::move(codeLine));
        } else {
            parser->numBlankLinesInCodeBlock++;
        }
    } else {
        // There's no leaf block and the remainder of the line is blank.
        // Walk the stack and set the "isLooseIfContinued" flag on all Lists.
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
}

// This is called after matchExistingIndentation() if the remainder of the line is not blank. It consumes new
// blockquote '>' markers and list item markers such as '*', creating new list blocks for each marker encountered.
void parseNewMarkers(LineParser& lp) {
    Parser* parser = lp.parser;

    // Line must not be blank.
    PLY_ASSERT(!lp.in.viewRemainingBytes().trim().isEmpty());

    // Attempt to parse new Block markers
    while (lp.in.hasRemainingBytes()) {
        if (lp.relativeIndent() >= 4)
            break;

        char* startByte = lp.in.curByte;
        u32 savedColumn = lp.column;

        // This code block will handle any list markers encountered:
        auto gotListMarker = [&](char bullet, u32 startNumber) {
            parser->leafBlock = nullptr;
            parser->numBlankLinesInCodeBlock = 0;
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
                    if (potentialList->bullet == bullet) {
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
                list.bullet = bullet;
                list.startNumber = startNumber;
                parentInner->childBlocks.append(listBlock);
            }
            Block* listItemBlock = addBlock<Block::ListItem>(listBlock);
            listItemBlock->var.as<Block::ListItem>()->relativeIndent = lp.outerColumn;
            parser->activeBlocks.append(listItemBlock);
        };

        char c = *lp.in.curByte;
        PLY_ASSERT(!isWhitespace(c));
        if (c == '>') {
            // Begin a new blockquote
            Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
            Block* bqBlock = addBlock<Block::BlockQuote>(parent);
            parser->activeBlocks.append(bqBlock);
            lp.in.readByte();
            lp.column++;
            if (lp.in.hasRemainingBytes() && (*lp.in.curByte == ' ')) {
                lp.in.curByte++;
                lp.column++;
            }
            lp.outerColumn = lp.column;
        } else if (c == '*' || c == '-' || c == '+') {
            lp.in.readByte();
            lp.column++;
            u32 indentAfterStar = lp.column;
            if (!lp.in.hasRemainingBytes() || (*lp.in.curByte != ' '))
                goto notMarker;
            lp.in.curByte++;
            lp.column++;
            if (parser->leafBlock && lp.in.viewRemainingBytes().trim().isEmpty())
                // If the list item interrupts a paragraph, it must not begin with a
                // blank line.
                goto notMarker;

            // It's an unordered list item.
            lp.outerColumn = indentAfterStar + 1;
            gotListMarker(c, 0);
        } else if (c >= '0' && c <= '9') {
            u64 num = readU64FromText(lp.in);
            if (parser->leafBlock && num != 1) {
                // If list item interrupts a paragraph, the start number must be 1.
                goto notMarker;
            }
            uptr markerLength = (lp.in.curByte - startByte);
            if (markerLength > 9)
                goto notMarker; // marker too long
            lp.column += numericCast<u32>(markerLength);
            if (lp.in.numRemainingBytes() < 2)
                goto notMarker;
            char punc = *lp.in.curByte;
            // FIXME: support alternate punctuator ')'.
            // If the punctuator doesn't match, it should start a new list.
            if (punc != '.' && punc != ')')
                goto notMarker;
            lp.in.readByte();
            lp.column++;
            u32 indentAfterMarker = lp.column;
            if (!lp.in.hasRemainingBytes() || (*lp.in.curByte != ' '))
                goto notMarker;
            lp.in.curByte++;
            lp.column++;
            if (parser->leafBlock && lp.in.viewRemainingBytes().trim().isEmpty()) {
                // If the list item interrupts a paragraph, it must not begin with a blank line.
                goto notMarker;
            }

            // It's an ordered list item.
            // 32-bit demotion is safe because we know the marker is 9 digits or less.
            lp.outerColumn = indentAfterMarker + 1;
            gotListMarker(0, numericCast<u32>(num));
        } else {
            goto notMarker;
        }

        // Consume whitespace
        while (lp.in.hasRemainingBytes() && (*lp.in.curByte == ' ')) {
            lp.in.curByte++;
            lp.column++;
        }
        continue;

    notMarker:
        lp.in.seekTo(startByte);
        lp.column = savedColumn;
        break;
    }
}

void parseParagraphText(LineParser& lp) {
    Parser* parser = lp.parser;

    StringView remainingText = lp.in.viewRemainingBytes().trim();
    bool hasPara = parser->leafBlock && parser->leafBlock->var.is<Block::Paragraph>();
    if (!hasPara && lp.relativeIndent() >= 4) {
        // Potentially begin or append to code block
        if (remainingText && !parser->leafBlock) {
            Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
            parser->leafBlock = addBlock<Block::CodeBlock>(parent);
            PLY_ASSERT(parser->numBlankLinesInCodeBlock == 0);
        }
        if (parser->leafBlock) {
            PLY_ASSERT(parser->leafBlock->var.is<Block::CodeBlock>());
            Block::Leaf* leaf = parser->leafBlock->asLeaf();
            // Add intermediate blank lines
            for (u32 i = 0; i < parser->numBlankLinesInCodeBlock; i++) {
                leaf->rawLines.append("\n");
            }
            parser->numBlankLinesInCodeBlock = 0;
            String codeLine = extractCodeLine({lp.in.view.startByte, lp.in.endByte}, lp.outerColumn + 4);
            leaf->rawLines.append(std::move(codeLine));
        }
    } else {
        if (remainingText) {
            // We're going to create or extend a leaf block. First, check if any Lists should be marked loose:
            if (parser->checkListContinuations) {
                // Yes, we should mark some (possibly zero) lists loose. It's impossible for a leaf block to exist at
                // this point:
                PLY_ASSERT(!parser->leafBlock);
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

            if (*lp.in.curByte == '#' && lp.relativeIndent() <= 3) {
                // Attempt to parse a heading
                char* startByte = lp.in.curByte;
                while (lp.in.hasRemainingBytes() && *lp.in.curByte == '#') {
                    lp.in.curByte++;
                }
                StringView poundSeq{startByte, lp.in.curByte};
                StringView space = readWhitespace(lp.in);
                if (poundSeq.numBytes() <= 6 && (!space.isEmpty() || !lp.in.hasRemainingBytes())) {
                    // Got a heading
                    Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                    Block* headingBlock = addBlock<Block::Heading>(parent);
                    auto* heading = headingBlock->var.as<Block::Heading>();
                    heading->level = poundSeq.numBytes();
                    if (StringView remainingText = lp.in.viewRemainingBytes().trim()) {
                        heading->rawLines.append(remainingText);
                    }
                    parser->leafBlock = nullptr;
                    parser->numBlankLinesInCodeBlock = 0;
                    return;
                }
                lp.in.seekTo(startByte);
            }
            // If parser->leafBlock already exists, it's a lazy paragraph continuation
            if (!hasPara) {
                // Begin new paragraph
                Block* parent = parser->activeBlocks ? parser->activeBlocks.back() : &parser->rootBlock;
                parser->leafBlock = addBlock<Block::Paragraph>(parent);
                parser->numBlankLinesInCodeBlock = 0;
            }
            parser->leafBlock->asLeaf()->rawLines.append(remainingText);
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

// Code to parse inline spans (second pass)

struct InlineConsumer {
    ArrayView<const String> rawLines;
    StringView rawLine;
    u32 lineIndex = 0;
    u32 i = 0;

    InlineConsumer(ArrayView<const String> rawLines) : rawLines{rawLines} {
        PLY_ASSERT(rawLines.numItems() > 0);
        rawLine = rawLines[0];
        PLY_ASSERT(rawLine);
    }

    enum ValidIndexResult { SameLine, NextLine, End };

    ValidIndexResult validIndex() {
        if (this->i >= this->rawLine.numBytes()) {
            if (this->lineIndex >= this->rawLines.numItems()) {
                return End;
            }
            this->i = 0;
            this->lineIndex++;
            if (this->lineIndex >= this->rawLines.numItems()) {
                this->rawLine = {};
                return End;
            }
            this->rawLine = this->rawLines[this->lineIndex];
            PLY_ASSERT(this->rawLine);
            return NextLine;
        }
        return SameLine;
    }
};

String getCodeSpan(InlineConsumer& ic, u32 endTickCount) {
    MemStream mout;
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.validIndex();
        if (res == InlineConsumer::End)
            return {};
        if (res == InlineConsumer::NextLine) {
            mout.write(' ');
        }
        char c = ic.rawLine[ic.i];
        ic.i++;
        if (c == '`') {
            u32 tickCount = 1;
            for (; ic.i < ic.rawLine.numBytes() && ic.rawLine[ic.i] == '`'; ic.i++) {
                tickCount++;
            }
            if (tickCount == endTickCount) {
                String result = mout.moveToString();
                PLY_ASSERT(result);
                if (result[0] == ' ' && result.back() == ' ' && result.find([](char c) { return c != ' '; }) >= 0) {
                    result = result.substr(1, result.numBytes() - 2);
                }
                return result;
            }
            mout.write(ic.rawLine.substr(ic.i - tickCount, tickCount));
        } else {
            mout.write(c);
        }
    }
}

inline bool isAscPunc(char c) {
    return (c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e);
}

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
    bool active = true;         // Open_Link only
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
        bool precededByWhite = (start == 0) || isWhitespace(rawLine[start - 1]);
        bool followedByWhite = (start + numBytes >= rawLine.numBytes()) || isWhitespace(rawLine[start + numBytes]);
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

struct LinkDestination {
    bool success = false;
    String dest;
};

LinkDestination parseLinkDestination(InlineConsumer& ic) {
    // FIXME: Support < > destinations
    // FIXME: Support link titles

    // Skip initial whitespace
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.validIndex();
        if (res == InlineConsumer::End) {
            return {false, String{}};
        }
        if (!isWhitespace(ic.rawLine[ic.i]))
            break;
        ic.i++;
    }

    MemStream mout;
    u32 parenNestLevel = 0;
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.validIndex();
        if (res != InlineConsumer::SameLine)
            break;

        char c = ic.rawLine[ic.i];
        if (c == '\\') {
            ic.i++;
            if (ic.validIndex() != InlineConsumer::SameLine) {
                mout.write('\\');
                break;
            }
            c = ic.rawLine[ic.i];
            if (!isAscPunc(c)) {
                mout.write('\\');
            }
            mout.write(c);
        } else if (c == '(') {
            ic.i++;
            mout.write(c);
            parenNestLevel++;
        } else if (c == ')') {
            if (parenNestLevel > 0) {
                ic.i++;
                mout.write(c);
                parenNestLevel--;
            } else {
                break;
            }
        } else if (c >= 0 && c <= 32) {
            break;
        } else {
            ic.i++;
            mout.write(c);
        }
    }

    if (parenNestLevel != 0) {
        return {false, String{}};
    }

    // Skip trailing whitespace
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.validIndex();
        if (res == InlineConsumer::End) {
            return {false, String{}};
        }
        char c = ic.rawLine[ic.i];
        if (c == ')') {
            ic.i++;
            return {true, mout.moveToString()};
        } else if (!isWhitespace(c)) {
            return {false, String{}};
        }
        ic.i++;
    }
}

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

Array<Owned<Span>> expandInlineSpans(ArrayView<const String> rawLines) {
    Array<Delimiter> delimiters;
    InlineConsumer ic{rawLines};
    u32 flushedIndex = 0;
    auto flushText = [&] {
        if (ic.i > flushedIndex) {
            delimiters.append({Delimiter::RawText, ic.rawLine.substr(flushedIndex, ic.i - flushedIndex)});
            flushedIndex = ic.i;
        }
    };
    for (;;) {
        if (ic.i >= ic.rawLine.numBytes()) {
            flushText();
            ic.i = 0;
            flushedIndex = 0;
            ic.lineIndex++;
            if (ic.lineIndex >= ic.rawLines.numItems())
                break;
            ic.rawLine = ic.rawLines[ic.lineIndex];
            delimiters.append(makeSpan<Span::SoftBreak>());
        }

        char c = ic.rawLine[ic.i];
        if (c == '`') {
            flushText();
            u32 tickCount = 1;
            for (ic.i++; ic.i < ic.rawLine.numBytes() && ic.rawLine[ic.i] == '`'; ic.i++) {
                tickCount++;
            }
            // Try consuming code span
            InlineConsumer backup = ic;
            String codeStr = getCodeSpan(ic, tickCount);
            if (codeStr) {
                Owned<Span> codeSpan = makeSpan<Span::Code>();
                codeSpan->var.as<Span::Code>()->text = std::move(codeStr);
                delimiters.append(std::move(codeSpan));
                flushedIndex = ic.i;
            } else {
                ic = backup;
                flushText();
            }
        } else if (c == '*') {
            flushText();
            u32 runLength = 1;
            for (ic.i++; ic.i < ic.rawLine.numBytes() && ic.rawLine[ic.i] == '*'; ic.i++) {
                runLength++;
            }
            delimiters.append(Delimiter::makeRun(Delimiter::Stars, ic.rawLine, ic.i - runLength, runLength));
            flushedIndex = ic.i;
        } else if (c == '_') {
            flushText();
            u32 runLength = 1;
            for (ic.i++; ic.i < ic.rawLine.numBytes() && ic.rawLine[ic.i] == '_'; ic.i++) {
                runLength++;
            }
            delimiters.append(Delimiter::makeRun(Delimiter::Underscores, ic.rawLine, ic.i - runLength, runLength));
            flushedIndex = ic.i;
        } else if (c == '[') {
            flushText();
            delimiters.append({Delimiter::OpenLink, ic.rawLine.substr(ic.i, 1)});
            ic.i++;
            flushedIndex = ic.i;
        } else if (c == ']') {
            // Try to parse an inline link
            flushText();
            ic.i++;
            if (!(ic.i < ic.rawLine.numBytes() && ic.rawLine[ic.i] == '('))
                continue; // No parenthesis

            // Got opening parenthesis
            ic.i++;

            // Look for preceding Open_Link delimiter
            s32 openLink =
                reverseFind(delimiters, [](const Delimiter& delim) { return delim.type == Delimiter::OpenLink; });
            if (openLink < 0)
                continue; // No preceding Open_Link delimiter

            // Found a preceding Open_Link delimiter
            // Try to parse link destination
            InlineConsumer backup = ic;
            LinkDestination linkDest = parseLinkDestination(ic);
            if (!linkDest.success) {
                // Couldn't parse link destination
                ic = backup;
                continue;
            }

            // Successfully parsed link destination
            Owned<Span> linkSpan = makeSpan<Span::Link>();
            linkSpan->var.as<Span::Link>()->destination = std::move(linkDest.dest);
            linkSpan->asContainer()->childSpans = processEmphasis(delimiters, openLink + 1);
            delimiters.resize(openLink);
            delimiters.append(std::move(linkSpan));
            flushedIndex = ic.i;
        } else {
            ic.i++;
        }
    }

    return processEmphasis(delimiters, 0);
}

static void doInlines(Block* block) {
    if (Block::Inner* inner = block->asInner()) {
        for (Block* child : inner->childBlocks) {
            doInlines(child);
        }
    } else {
        Block::Leaf* leaf = block->asLeaf();
        PLY_ASSERT(leaf);
        if (!block->var.is<Block::CodeBlock>()) {
            leaf->spans = expandInlineSpans(leaf->rawLines);
            leaf->rawLines.clear();
        }
    }
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

Owned<Parser> createParser() {
    Owned<Parser> parser = Heap::create<Parser>();
    parser->rootBlock.var.switchTo<Block::BlockQuote>();
    return parser;
}

String untabify(StringView str, u32 tabSize) {
    MemStream mem;
    u32 column = 0;
    for (char c : str) {
        if (c == '\t') {
            u32 spaces = tabSize - (column % tabSize);
            for (u32 i = 0; i < spaces; i++) {
                mem.write(' ');
            }
            column += spaces;
        } else {
            mem.write(c);
            if (c == '\n') {
                column = 0;
            } else if (c >= 32) {
                column++;
            }
        }
    }
    return mem.moveToString();
}

Owned<Block> parseLine(Parser* parser, StringView line) {
    // Untabify the input line (if needed) to simplify internal processing.
    String untabified;
    if (line.find('\t') >= 0) {
        constexpr u32 tabSize = 4;
        untabified = untabify(line, tabSize);
        line = untabified;
    }

    LineParser lp{parser, line};

    // Match existing indentation and blockquote '>' markers.
    matchExistingIndentation(lp);

    if (lp.in.viewRemainingBytes().trim().isEmpty()) {
        // The rest of the line is blank.
        handleBlankLine(lp);
    } else {
        // There's more text on the current line.
        if (lp.blockDepth < parser->activeBlocks.numItems()) {
            parser->activeBlocks.resize(lp.blockDepth);
            parser->leafBlock = nullptr;
            parser->numBlankLinesInCodeBlock = 0;
        }
        parseNewMarkers(lp);
        parseParagraphText(lp);
    }

    auto& rootChildren = parser->rootBlock.asInner()->childBlocks;
    if (rootChildren.numItems() > 1) {
        // parseParagraphText can only add one child block, so rootBlock can only have
        // exactly 2 blocks at this point. Pop the first one and return it.
        PLY_ASSERT(rootChildren.numItems() == 2);
        Owned<Block> out = std::move(rootChildren[0]);
        rootChildren.erase(0);
        doInlines(out);
        return out;
    }
    return {};
}

Owned<Block> flush(Parser* parser) {
    // Terminate all existing blocks.
    parser->activeBlocks.clear();
    parser->leafBlock = nullptr;
    parser->numBlankLinesInCodeBlock = 0;

    auto& rootChildren = parser->rootBlock.asInner()->childBlocks;
    if (rootChildren) {
        // There cannot be more than one child block at this point.
        PLY_ASSERT(rootChildren.numItems() == 1);
        Owned<Block> block = std::move(rootChildren[0]);
        rootChildren.erase(0);
        doInlines(block);
        block->parent = nullptr;
        return block;
    }
    return {};
}

void destroy(Parser* parser) {
    Heap::destroy(parser);
}

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

String convertToHtml(StringView src) {
    ViewStream in{src};
    MemStream out;
    markdown::HTML_Options options;
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
        if (list->bullet == 0) {
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
    } else if (block->var.is<Block::CodeBlock>()) {
        outs->write("code_block");
    } else {
        PLY_ASSERT(0);
        outs->write("???");
    }
    outs->write("\n");
    if (const Block::Leaf* leaf = block->asLeaf()) {
        for (StringView text : leaf->rawLines) {
            outs->format("{}  \"", indent);
            printEscapedString(*outs, text);
            outs->write("\"\n");
        }
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

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄   ▄▄ ▄▄
//  ██  ██   ██   ███▄███ ██
//  ██▀▀██   ██   ██▀█▀██ ██
//  ██  ██   ██   ██   ██ ██▄▄▄
//

void convertSpanToHtml(Stream* outs, const Span* span, const HTML_Options& options) {
    if (auto* text = span->var.as<Span::Text>()) {
        printXmlEscapedString(*outs, text->text);
    } else if (auto* link = span->var.as<Span::Link>()) {
        outs->write("<a href=\"");
        printXmlEscapedString(*outs, link->destination);
        outs->write("\">");
        for (const Span* child : link->childSpans) {
            convertSpanToHtml(outs, child, options);
        }
        outs->write("</a>");
    } else if (auto* code = span->var.as<Span::Code>()) {
        outs->write("<code>");
        printXmlEscapedString(*outs, code->text);
        outs->write("</code>");
    } else if (span->var.is<Span::SoftBreak>()) {
        outs->write("\n");
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

void convertToHtml(Stream* outs, const Block* block, const HTML_Options& options) {
    if (auto* list = block->var.as<Block::List>()) {
        if (list->bullet == 0) {
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
        if (list->bullet == 0) {
            outs->write("</ol>\n");
        } else {
            outs->write("</ul>\n");
        }
    } else if (auto* listItem = block->var.as<Block::ListItem>()) {
        auto* parentList = block->parent->var.as<Block::List>();
        outs->write("<li>");
        if (!parentList->isLoose && listItem->childBlocks[0]->var.is<Block::Paragraph>()) {
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
                outs->write(" class=\"anchored\"><span class=\"anchor\" id=\"");
                printXmlEscapedString(*outs, heading->id);
                outs->write("\">&nbsp;</span>");
            } else {
                outs->write(" id=\"");
                printXmlEscapedString(*outs, heading->id);
                outs->write("\">");
            }
        } else {
            outs->write('>');
        }
        PLY_ASSERT(heading->rawLines.isEmpty());
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
        PLY_ASSERT(para->rawLines.isEmpty());
        for (const Span* span : para->spans) {
            convertSpanToHtml(outs, span, options);
        }
        if (!isInsideTight) {
            outs->write("</p>\n");
        }
    } else if (auto* codeBlock = block->var.as<Block::CodeBlock>()) {
        outs->write("<pre>");
        PLY_ASSERT(codeBlock->spans.isEmpty());
        for (StringView rawLine : codeBlock->rawLines) {
            printXmlEscapedString(*outs, rawLine);
        }
        outs->write("</pre>\n");
    } else {
        PLY_ASSERT(0);
    }
}

} // namespace markdown
} // namespace ply
