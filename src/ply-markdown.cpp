/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include "ply-markdown.h"

namespace ply {
namespace markdown {

//  ▄▄▄▄▄  ▄▄▄               ▄▄         ▄▄▄▄▄ ▄▄▄                                 ▄▄
//  ██  ██  ██   ▄▄▄▄   ▄▄▄▄ ██  ▄▄     ██     ██   ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄
//  ██▀▀█▄  ██  ██  ██ ██    ██▄█▀      ██▀▀   ██  ██▄▄██ ██ ██ ██ ██▄▄██ ██  ██  ██   ▀█▄▄▄
//  ██▄▄█▀ ▄██▄ ▀█▄▄█▀ ▀█▄▄▄ ██ ▀█▄     ██▄▄▄ ▄██▄ ▀█▄▄▄  ██ ██ ██ ▀█▄▄▄  ██  ██  ▀█▄▄  ▄▄▄█▀
//

// Code to parse block elements (first pass).

struct LineParser {
    // Keeps track of the current read position.
    ViewStream in;

    // Keeps track of how many elements in Parser::elementStack were matched by current line's indentation and
    // blockquote > markers.
    u32 stackDepth = 0;

    // If the last matching stack element was a blockquote, this is the column number after the > marker and optional
    // following single space (if any). If the last matching stack element was a list item, this is the column number
    // where sufficient indentation was reached for the rest of the line to be considered part of the list item. Note
    // that different lines can have different outerIndent numbers for the same stack element, because blockquote >
    // markers can be preceded by a different number (from 0 to 3) of spaces on each line.
    u32 outerIndent = 0;

    // The number of columns of leading indentation (including blockquote > markers) that have been read on this line.
    u32 indent = 0;

    // How much leading space was encountered on this line after outerIndent.
    u32 innerIndent() const {
        return this->indent - this->outerIndent;
    }

    // Constructor.
    LineParser(StringView line) : in{line} {
    }
};

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

// ParserDetails extends Parser with internal state not exposed in the public API.
struct ParserDetails : Parser {
    // Only used if leafElement is CodeBlock:
    u32 numBlankLinesInCodeBlock = 0;

    // This flag indicates that some Lists on the stack have their isLooseIfContinued flag set: (Alternatively, we
    // *could* store the number of such Lists on the stack, and eliminate the isLooseIfContinued flag completely, but
    // it would complicate matchExistingIndentation a little bit. Sticking with this approach for now.)
    bool checkListContinuations = false;
};

// This is called at the start of each line. It figures out which of the existing elements we are still inside by
// consuming indentation and blockquote '>' markers that match the element stack.
void matchExistingIndentation(ParserDetails* parser, LineParser& lp) {
    // Consume leading spaces.
    while (lp.in.numRemainingBytes() > 0 && (*lp.in.curByte == ' ')) {
        lp.in.curByte++;
        lp.indent++;
    }

    // Iterate over stack items, matching as much leading indentation and BlockQuote '>' markers as possible.
    PLY_ASSERT(lp.stackDepth == 0);
    while (lp.stackDepth < parser->elementStack.numItems()) {
        Element* element = parser->elementStack[lp.stackDepth];
        if (element->type == Element::BlockQuote) {
            // If there is a '>' within 3 columns of outerIndent, match this BlockQuote element.
            if ((lp.in.numRemainingBytes() > 0) && (*lp.in.curByte == '>') && (lp.innerIndent() <= 3)) {
                lp.stackDepth++;
                lp.in.curByte++;
                lp.indent++;
                if (lp.in.numRemainingBytes() > 0 && (*lp.in.curByte == ' ')) {
                    // Read optional space after '>'.
                    lp.in.curByte++;
                    lp.indent++;
                }
                lp.outerIndent = lp.indent;
                continue;
            }
            // Consume additional spaces.
            while (lp.in.numRemainingBytes() > 0 && (*lp.in.curByte == ' ')) {
                lp.in.curByte++;
                lp.indent++;
            }
        } else if (element->type == Element::ListItem) {
            // If the line's indentation surpasses the list item's indentation, match this ListItem element.
            if (lp.innerIndent() >= element->relativeIndent) {
                lp.stackDepth++;
                lp.outerIndent += element->relativeIndent;
                continue;
            }
        } else {
            // elementStack can only hold BlockQuote and ListItem elements.
            PLY_ASSERT(0);
        }
        break;
    }
}

// This is called after matchExistingIndentation() if the remainder of the line is blank.
void handleBlankLine(ParserDetails* parser, LineParser& lp) {
    // Terminate paragraph if any.
    if (parser->leafElement && (parser->leafElement->type == Element::Paragraph)) {
        parser->leafElement = nullptr;
        PLY_ASSERT(parser->numBlankLinesInCodeBlock == 0);
    }

    // Stay inside lists.
    while ((lp.stackDepth < parser->elementStack.numItems()) &&
           (parser->elementStack[lp.stackDepth]->type == Element::ListItem)) {
        lp.stackDepth++;
    }

    // If there's another element in elementStack, it must be a BlockQuote. Terminate it.
    if (lp.stackDepth < parser->elementStack.numItems()) {
        PLY_ASSERT(parser->elementStack[lp.stackDepth]->type == Element::BlockQuote);
        parser->elementStack.resize(lp.stackDepth);
        parser->leafElement = nullptr;
        parser->numBlankLinesInCodeBlock = 0;
    }

    if (parser->leafElement) {
        // At this point, the only possible leaf element is a CodeBlock, because Paragraphs are terminated above, and
        // Headings don't persist across lines.
        PLY_ASSERT(parser->leafElement->type == Element::CodeBlock);
        // Count blank lines in CodeBlocks
        if (lp.indent - lp.outerIndent > 4) {
            // Add intermediate blank lines.
            for (u32 i = 0; i < parser->numBlankLinesInCodeBlock; i++) {
                parser->leafElement->rawLines.append("\n");
            }
            parser->numBlankLinesInCodeBlock = 0;
            String codeLine = extractCodeLine({lp.in.view.startByte, lp.in.endByte}, lp.outerIndent + 4);
            parser->leafElement->rawLines.append(std::move(codeLine));
        } else {
            parser->numBlankLinesInCodeBlock++;
        }
    } else {
        // There's no leaf element and the remainder of the line is blank.
        // Walk the stack and set the "isLooseIfContinued" flag on all Lists.
        for (Element* element : parser->elementStack) {
            if (element->type == Element::ListItem) {
                PLY_ASSERT(element->parent->type == Element::List);
                if (!element->parent->isLoose) {
                    element->parent->isLooseIfContinued = true;
                    parser->checkListContinuations = true;
                }
            }
        }
    }
}

// This is called after matchExistingIndentation() if the remainder of the line is not blank. It consumes new
// blockquote '>' markers and list item markers such as '*', creating new list elements for each marker encountered.
void parseNewMarkers(ParserDetails* parser, LineParser& lp) {
    // Line must not be blank.
    PLY_ASSERT(!lp.in.viewRemainingBytes().trim().isEmpty());

    // Attempt to parse new Element markers
    while (lp.in.numRemainingBytes() > 0) {
        if (lp.innerIndent() >= 4)
            break;

        char* startByte = lp.in.curByte;
        u32 savedIndent = lp.indent;

        // This code block will handle any list markers encountered:
        auto gotListMarker = [&](s32 markerNumber, char punc) {
            bool isOrdered = (markerNumber >= 0);
            parser->leafElement = nullptr;
            parser->numBlankLinesInCodeBlock = 0;
            Element* listElement = nullptr;
            Element* parentCtr = &parser->rootElement;
            if (parser->elementStack) {
                parentCtr = parser->elementStack.back();
            }
            PLY_ASSERT(parentCtr->isContainerBlock());
            if (!parentCtr->children.isEmpty()) {
                Element* potentialParent = parentCtr->children.back();
                if (potentialParent->type == Element::List && potentialParent->isOrderedList() == isOrdered &&
                    potentialParent->listPunc == punc) {
                    // Add item to existing list
                    listElement = potentialParent;
                }
            } else if (parentCtr->type == Element::ListItem) {
                // Begin new list as a sublist of existing list
                parentCtr = parentCtr->parent;
                PLY_ASSERT(parentCtr->type == Element::List);
            }
            if (!listElement) {
                // Begin new list
                // Note: parentCtr automatically owns the new Element through its children member.
                listElement = Heap::create<Element>(parentCtr, Element::List);
                listElement->listStartNumber = markerNumber;
                listElement->listPunc = punc;
            }
            Element* listItem = Heap::create<Element>(listElement, Element::ListItem);
            listItem->relativeIndent = lp.outerIndent;
            parser->elementStack.append(listItem);
        };

        char c = *lp.in.curByte;
        PLY_ASSERT(!isWhitespace(c));
        if (c == '>') {
            // Begin a new blockquote
            Element* parent = parser->elementStack ? parser->elementStack.back() : &parser->rootElement;
            // Note: parent automatically owns the new Element through its children member.
            parser->elementStack.append(Heap::create<Element>(parent, Element::BlockQuote));
            lp.in.readByte();
            lp.indent++;
            if ((lp.in.numRemainingBytes() > 0) && (*lp.in.curByte == ' ')) {
                lp.in.curByte++;
                lp.indent++;
            }
            lp.outerIndent = lp.indent;
        } else if (c == '*' || c == '-' || c == '+') {
            lp.in.readByte();
            lp.indent++;
            u32 indentAfterStar = lp.indent;
            if ((lp.in.numRemainingBytes() == 0) || (*lp.in.curByte != ' '))
                goto notMarker;
            lp.in.curByte++;
            lp.indent++;
            if (parser->leafElement && lp.in.viewRemainingBytes().trim().isEmpty())
                // If the list item interrupts a paragraph, it must not begin with a
                // blank line.
                goto notMarker;

            // It's an unordered list item.
            lp.outerIndent = indentAfterStar + 1;
            gotListMarker(-1, c);
        } else if (c >= '0' && c <= '9') {
            u64 num = readU64FromText(lp.in);
            if (parser->leafElement && num != 1) {
                // If list item interrupts a paragraph, the start number must be 1.
                goto notMarker;
            }
            uptr markerLength = (lp.in.curByte - startByte);
            if (markerLength > 9)
                goto notMarker; // marker too long
            lp.indent += numericCast<u32>(markerLength);
            if (lp.in.numRemainingBytes() < 2)
                goto notMarker;
            char punc = *lp.in.curByte;
            // FIXME: support alternate punctuator ')'.
            // If the punctuator doesn't match, it should start a new list.
            if (punc != '.' && punc != ')')
                goto notMarker;
            lp.in.readByte();
            lp.indent++;
            u32 indentAfterMarker = lp.indent;
            if ((lp.in.numRemainingBytes() == 0) || (*lp.in.curByte != ' '))
                goto notMarker;
            lp.in.curByte++;
            lp.indent++;
            if (parser->leafElement && lp.in.viewRemainingBytes().trim().isEmpty()) {
                // If the list item interrupts a paragraph, it must not begin with a blank line.
                goto notMarker;
            }

            // It's an ordered list item.
            // 32-bit demotion is safe because we know the marker is 9 digits or less.
            lp.outerIndent = indentAfterMarker + 1;
            gotListMarker(numericCast<s32>(num), punc);
        } else {
            goto notMarker;
        }

        // Consume whitespace
        while ((lp.in.numRemainingBytes() > 0) && (*lp.in.curByte == ' ')) {
            lp.in.curByte++;
            lp.indent++;
        }
        continue;

    notMarker:
        lp.in.seekTo(startByte);
        lp.indent = savedIndent;
        break;
    }
}

void parseParagraphText(ParserDetails* parser, LineParser& lp) {
    StringView remainingText = lp.in.viewRemainingBytes().trim();
    bool hasPara = parser->leafElement && parser->leafElement->type == Element::Paragraph;
    if (!hasPara && lp.innerIndent() >= 4) {
        // Potentially begin or append to code Element
        if (remainingText && !parser->leafElement) {
            Element* parent = parser->elementStack ? parser->elementStack.back() : &parser->rootElement;
            Element* leafElement = Heap::create<Element>(parent, Element::CodeBlock);
            parser->leafElement = leafElement;
            PLY_ASSERT(parser->numBlankLinesInCodeBlock == 0);
        }
        if (parser->leafElement) {
            PLY_ASSERT(parser->leafElement->type == Element::CodeBlock);
            // Add intermediate blank lines
            for (u32 i = 0; i < parser->numBlankLinesInCodeBlock; i++) {
                parser->leafElement->rawLines.append("\n");
            }
            parser->numBlankLinesInCodeBlock = 0;
            String codeLine = extractCodeLine({lp.in.view.startByte, lp.in.endByte}, lp.outerIndent + 4);
            parser->leafElement->rawLines.append(std::move(codeLine));
        }
    } else {
        if (remainingText) {
            // We're going to create or extend a leaf element. First, check if any Lists should be marked loose:
            if (parser->checkListContinuations) {
                // Yes, we should mark some (possibly zero) lists loose. It's impossible for a leaf element to exist at
                // this point:
                PLY_ASSERT(!parser->leafElement);
                for (Element* element : parser->elementStack) {
                    if (element->type == Element::ListItem) {
                        PLY_ASSERT(element->parent->type == Element::List);
                        if (element->parent->isLooseIfContinued) {
                            element->parent->isLoose = true;
                            element->parent->isLooseIfContinued = false;
                        }
                    }
                }
                parser->checkListContinuations = false;
            }

            if (*lp.in.curByte == '#' && lp.innerIndent() <= 3) {
                // Attempt to parse a heading
                char* startByte = lp.in.curByte;
                while (lp.in.numRemainingBytes() > 0 && *lp.in.curByte == '#') {
                    lp.in.curByte++;
                }
                StringView poundSeq{startByte, lp.in.curByte};
                StringView space = readWhitespace(lp.in);
                if (poundSeq.numBytes() <= 6 && (!space.isEmpty() || lp.in.numRemainingBytes() == 0)) {
                    // Got a heading
                    Element* parent = parser->elementStack ? parser->elementStack.back() : &parser->rootElement;
                    Element* headingElement = Heap::create<Element>(parent, Element::Heading);
                    headingElement->headingLevel = poundSeq.numBytes();
                    if (StringView remainingText = lp.in.viewRemainingBytes().trim()) {
                        headingElement->rawLines.append(remainingText);
                    }
                    parser->leafElement = nullptr;
                    parser->numBlankLinesInCodeBlock = 0;
                    return;
                }
                lp.in.seekTo(startByte);
            }
            // If parser->leafElement already exists, it's a lazy paragraph continuation
            if (!hasPara) {
                // Begin new paragraph
                Element* parent = parser->elementStack ? parser->elementStack.back() : &parser->rootElement;
                parser->leafElement = Heap::create<Element>(parent, Element::Paragraph);
                parser->numBlankLinesInCodeBlock = 0;
            }
            parser->leafElement->rawLines.append(remainingText);
        } else {
            PLY_ASSERT(!parser->leafElement); // Should already be cleared by this point
        }
    }
}

//  ▄▄▄▄        ▄▄▄  ▄▄                   ▄▄▄▄▄ ▄▄▄                                 ▄▄
//   ██  ▄▄▄▄▄   ██  ▄▄ ▄▄▄▄▄   ▄▄▄▄      ██     ██   ▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄
//   ██  ██  ██  ██  ██ ██  ██ ██▄▄██     ██▀▀   ██  ██▄▄██ ██ ██ ██ ██▄▄██ ██  ██  ██   ▀█▄▄▄
//  ▄██▄ ██  ██ ▄██▄ ██ ██  ██ ▀█▄▄▄      ██▄▄▄ ▄██▄ ▀█▄▄▄  ██ ██ ██ ▀█▄▄▄  ██  ██  ▀█▄▄  ▄▄▄█▀
//

// Code to parse inline elements (second pass)

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
    Owned<Element> element; // Inline_Elem only, and it'll be an inline element type

    Delimiter() = default;
    Delimiter(Type type, StringView text) : type{type}, text{text} {
    }
    Delimiter(Type type, String&& text) : type{type}, textStorage{std::move(text)}, text{textStorage} {
    }
    Delimiter(Owned<Element>&& elem) : type{InlineElem}, element{std::move(elem)} {
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

Array<Owned<Element>> convertToInlineElems(ArrayView<Delimiter> delimiters) {
    Array<Owned<Element>> elements;
    for (Delimiter& delimiter : delimiters) {
        if (delimiter.type == Delimiter::InlineElem) {
            elements.append(std::move(delimiter.element));
        } else {
            if (!(elements.numItems() > 0 && elements.back()->type == Element::Text)) {
                elements.append(Heap::create<Element>(nullptr, Element::Text));
            }
            elements.back()->text += delimiter.text;
        }
    }
    return elements;
}

Array<Owned<Element>> processEmphasis(Array<Delimiter>& delimiters, u32 bottomPos) {
    u32 starOpener = bottomPos;
    u32 underscoreOpener = bottomPos;
    for (u32 pos = bottomPos; pos < delimiters.numItems(); pos++) {
        auto handleCloser = [&](Delimiter::Type type, u32& openerPos) {
            for (u32 j = pos; j > openerPos;) {
                --j;
                if (delimiters[j].type == type && delimiters[j].leftFlanking) {
                    u32 spanLength = min(delimiters[j].text.numBytes(), delimiters[pos].text.numBytes());
                    PLY_ASSERT(spanLength > 0);
                    Owned<Element> elem =
                        Heap::create<Element>(nullptr, spanLength >= 2 ? Element::Strong : Element::Emphasis);
                    elem->addChildren(convertToInlineElems(delimiters.subview(j + 1, pos - j - 1)));
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
                    delimiters.insert(j) = std::move(elem);
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
    Array<Owned<Element>> result = convertToInlineElems(delimiters.subview(bottomPos));
    delimiters.resize(bottomPos);
    return result;
}

Array<Owned<Element>> expandInlineElements(ArrayView<const String> rawLines) {
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
            delimiters.append(Heap::create<Element>(nullptr, Element::SoftBreak));
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
                Owned<Element> codeSpan = Heap::create<Element>(nullptr, Element::CodeSpan);
                codeSpan->text = std::move(codeStr);
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
            Owned<Element> elem = Heap::create<Element>(nullptr, Element::Link);
            elem->text = std::move(linkDest.dest);
            elem->addChildren(processEmphasis(delimiters, openLink + 1));
            delimiters.resize(openLink);
            delimiters.append(std::move(elem));
            flushedIndex = ic.i;
        } else {
            ic.i++;
        }
    }

    return processEmphasis(delimiters, 0);
}

static void doInlines(Element* element) {
    if (element->isContainerBlock()) {
        PLY_ASSERT(element->rawLines.isEmpty());
        for (Element* child : element->children) {
            doInlines(child);
        }
    } else {
        PLY_ASSERT(element->isLeafBlock());
        if (element->type != Element::CodeBlock) {
            element->addChildren(expandInlineElements(element->rawLines));
            element->rawLines.clear();
        }
    }
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

Owned<Parser> createParser() {
    return Heap::create<ParserDetails>();
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

Owned<Element> parseLine(Parser* parser, StringView line) {
    ParserDetails* details = static_cast<ParserDetails*>(parser);

    // Untabify the input line (if needed) to simplify internal processing.
    String untabified;
    if (line.find('\t') >= 0) {
        constexpr u32 tabSize = 4;
        untabified = untabify(line, tabSize);
        line = untabified;
    }

    LineParser lp{line};

    // Match existing indentation and blockquote '>' markers.
    matchExistingIndentation(details, lp);

    if (lp.in.viewRemainingBytes().trim().isEmpty()) {
        // The rest of the line is blank.
        handleBlankLine(details, lp);
    } else {
        // There's more text on the current line.
        if (lp.stackDepth < details->elementStack.numItems()) {
            details->elementStack.resize(lp.stackDepth);
            details->leafElement = nullptr;
            details->numBlankLinesInCodeBlock = 0;
        }
        parseNewMarkers(details, lp);
        parseParagraphText(details, lp);
    }

    if (details->rootElement.children.numItems() > 1) {
        // parseParagraphText can only add one child element, so rootElement can only have
        // exactly 2 elements at this point. Pop the first one and return it.
        PLY_ASSERT(details->rootElement.children.numItems() == 2);
        Owned<Element> out = std::move(details->rootElement.children[0]);
        details->rootElement.children.erase(0);
        doInlines(out);
        return out;
    }
    return {};
}

Owned<Element> flush(Parser* parser) {
    ParserDetails* details = static_cast<ParserDetails*>(parser);
    // Terminate all existing elements.
    details->elementStack.clear();
    details->leafElement = nullptr;
    details->numBlankLinesInCodeBlock = 0;

    if (details->rootElement.children) {
        // There cannot be more than one child element at this point.
        PLY_ASSERT(details->rootElement.children.numItems() == 1);
        Owned<Element> element = std::move(details->rootElement.children[0]);
        details->rootElement.children.erase(0);
        doInlines(element);
        element->parent = nullptr;
        return element;
    }
    return {};
}

void destroy(Parser* parser) {
    Heap::destroy(static_cast<ParserDetails*>(parser));
}

Array<Owned<Element>> parseWholeDocument(StringView markdown) {
    Array<Owned<Element>> elements;
    Owned<Parser> parser = createParser();
    ViewStream in{markdown};

    while (StringView line = readLine(in)) {
        if (Owned<Element> element = parseLine(parser, line)) {
            elements.append(std::move(element));
        }
    }
    if (Owned<Element> element = flush(parser)) {
        elements.append(std::move(element));
    }

    return elements;
}

String convertToHtml(StringView src) {
    ViewStream in{src};
    MemStream out;
    markdown::HTML_Options options;
    Owned<Parser> parser = createParser();

    while (StringView line = readLine(in)) {
        if (Owned<Element> element = parseLine(parser, line)) {
            convertToHtml(&out, element, options);
        }
    }
    if (Owned<Element> element = flush(parser)) {
        convertToHtml(&out, element, options);
    }

    return out.moveToString();
}

//  ▄▄▄▄▄         ▄▄                          ▄▄
//  ██  ██  ▄▄▄▄  ██▄▄▄  ▄▄  ▄▄  ▄▄▄▄▄  ▄▄▄▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██  ██ ██▄▄██ ██  ██ ██  ██ ██  ██ ██  ██ ██ ██  ██ ██  ██
//  ██▄▄█▀ ▀█▄▄▄  ██▄▄█▀ ▀█▄▄██ ▀█▄▄██ ▀█▄▄██ ██ ██  ██ ▀█▄▄██
//                               ▄▄▄█▀  ▄▄▄█▀            ▄▄▄█▀

void dump(Stream* outs, const Element* element, u32 level) {
    String indent = StringView{"  "} * level;
    outs->write(indent);
    switch (element->type) {
        case Element::List: {
            outs->write("list");
            if (element->isLoose) {
                outs->write(" (loose");
            } else {
                outs->write(" (tight");
            }
            if (element->isOrderedList()) {
                outs->format(", ordered, start={})", element->listStartNumber);
            } else {
                outs->write(", unordered)");
            }
            break;
        }
        case Element::ListItem: {
            outs->write("item");
            break;
        }
        case Element::BlockQuote: {
            outs->write("block_quote");
            break;
        }
        case Element::Heading: {
            outs->format("heading level={}", element->headingLevel);
            break;
        }
        case Element::Paragraph: {
            outs->write("paragraph");
            break;
        }
        case Element::CodeBlock: {
            outs->write("code_block");
            break;
        }
        case Element::Text: {
            outs->write("text \"");
            printEscapedString(*outs, element->text);
            outs->write('"');
            break;
        }
        case Element::Link: {
            outs->write("link destination=\"");
            printEscapedString(*outs, element->text);
            outs->write('"');
            break;
        }
        case Element::CodeSpan: {
            outs->write("code \"");
            printEscapedString(*outs, element->text);
            outs->write('"');
            break;
        }
        case Element::SoftBreak: {
            outs->write("softbreak");
            break;
        }
        case Element::Emphasis: {
            outs->write("emph");
            break;
        }
        case Element::Strong: {
            outs->write("strong");
            break;
        }
        default: {
            PLY_ASSERT(0);
            outs->write("???");
            break;
        }
    }
    outs->write("\n");
    for (StringView text : element->rawLines) {
        outs->format("{}  \"", indent);
        printEscapedString(*outs, text);
        outs->write("\"\n");
    }
    for (const Element* child : element->children) {
        PLY_ASSERT(child->parent == element);
        dump(outs, child, level + 1);
    }
}

//  ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄   ▄▄ ▄▄
//  ██  ██   ██   ███▄███ ██
//  ██▀▀██   ██   ██▀█▀██ ██
//  ██  ██   ██   ██   ██ ██▄▄▄
//

void convertToHtml(Stream* outs, const Element* element, const HTML_Options& options) {
    switch (element->type) {
        case Element::List: {
            if (element->isOrderedList()) {
                if (element->listStartNumber != 1) {
                    outs->format("<ol start=\"{}\">\n", element->listStartNumber);
                } else {
                    outs->write("<ol>\n");
                }
            } else {
                outs->write("<ul>\n");
            }
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            if (element->isOrderedList()) {
                outs->write("</ol>\n");
            } else {
                outs->write("</ul>\n");
            }
            break;
        }
        case Element::ListItem: {
            outs->write("<li>");
            if (!element->parent->isLoose && element->children[0]->type == Element::Paragraph) {
                // Don't output a newline before the paragraph in a tight list.
            } else {
                outs->write("\n");
            }
            for (u32 i = 0; i < element->children.numItems(); i++) {
                convertToHtml(outs, element->children[i], options);
                if (!element->parent->isLoose && element->children[i]->type == Element::Paragraph &&
                    i + 1 < element->children.numItems()) {
                    // This paragraph had no <p> tag and didn't end in a newline, but
                    // there are more children following it, so add a newline here.
                    outs->write("\n");
                }
            }
            outs->write("</li>\n");
            break;
        }
        case Element::BlockQuote: {
            outs->write("<blockquote>\n");
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            outs->write("</blockquote>\n");
            break;
        }
        case Element::Heading: {
            outs->format("<h{}", element->headingLevel);
            if (element->id) {
                if (options.childAnchors) {
                    outs->write(" class=\"anchored\"><span class=\"anchor\" id=\"");
                    printXmlEscapedString(*outs, element->id);
                    outs->write("\">&nbsp;</span>");
                } else {
                    outs->write(" id=\"");
                    printXmlEscapedString(*outs, element->id);
                    outs->write("\">");
                }
            } else {
                outs->write('>');
            }
            PLY_ASSERT(element->rawLines.isEmpty());
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            outs->format("</h{}>\n", element->headingLevel);
            break;
        }
        case Element::Paragraph: {
            bool isInsideTight =
                (element->parent && element->parent->type == Element::ListItem && !element->parent->parent->isLoose);
            if (!isInsideTight) {
                outs->write("<p>");
            }
            PLY_ASSERT(element->rawLines.isEmpty());
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            if (!isInsideTight) {
                outs->write("</p>\n");
            }
            break;
        }
        case Element::CodeBlock: {
            outs->write("<pre>");
            PLY_ASSERT(element->children.isEmpty());
            for (StringView rawLine : element->rawLines) {
                printXmlEscapedString(*outs, rawLine);
            }
            outs->write("</pre>\n");
            break;
        }
        case Element::Text: {
            printXmlEscapedString(*outs, element->text);
            PLY_ASSERT(element->children.isEmpty());
            break;
        }
        case Element::Link: {
            outs->write("<a href=\"");
            printXmlEscapedString(*outs, element->text);
            outs->write("\">");
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            outs->write("</a>");
            break;
        }
        case Element::CodeSpan: {
            outs->write("<code>");
            printXmlEscapedString(*outs, element->text);
            outs->write("</code>");
            PLY_ASSERT(element->children.isEmpty());
            break;
        }
        case Element::SoftBreak: {
            outs->write("\n");
            PLY_ASSERT(element->children.isEmpty());
            break;
        }
        case Element::Emphasis: {
            outs->write("<em>");
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            outs->write("</em>");
            break;
        }
        case Element::Strong: {
            outs->write("<strong>");
            for (const Element* child : element->children) {
                convertToHtml(outs, child, options);
            }
            outs->write("</strong>");
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

} // namespace markdown
} // namespace ply
