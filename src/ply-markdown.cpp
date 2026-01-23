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

    // Keeps track of how many elements in Parser::element_stack were matched by current line's indentation and
    // blockquote > markers.
    u32 stack_depth = 0;

    // If the last matching stack element was a blockquote, this is the column number after the > marker and optional
    // following single space (if any). If the last matching stack element was a list item, this is the column number
    // where sufficient indentation was reached for the rest of the line to be considered part of the list item. Note
    // that different lines can have different outer_indent numbers for the same stack element, because blockquote >
    // markers can be preceded by a different number (from 0 to 3) of spaces on each line.
    u32 outer_indent = 0;

    // The number of columns of leading indentation (including blockquote > markers) that have been read on this line.
    u32 indent = 0;

    // How much leading space was encountered on this line after outer_indent.
    u32 inner_indent() const {
        return this->indent - this->outer_indent;
    }

    // Constructor.
    LineParser(StringView line) : in{line} {
    }
};

// Helper function to extract a line from a code block without leading indentation.
String extract_code_line(StringView line, u32 from_indent) {
    u32 indent = 0;
    for (u32 i = 0; i < line.num_bytes(); i++) {
        if (indent == from_indent) {
            return line.substr(i);
        }
        u8 c = line[i];
        PLY_ASSERT(c < 128);              // No high code points
        PLY_ASSERT(c >= 32 || c == '\t'); // No control characters
        if (c == '\t') {
            u32 tab_size = 4;
            u32 new_indent = indent + tab_size - (indent % tab_size);
            if (new_indent > from_indent) {
                return StringView{" "} * (new_indent - from_indent) + line.substr(i + 1);
            }
            indent = new_indent;
        } else {
            indent++;
        }
    }
    PLY_ASSERT(0);
    return {};
}

// ParserDetails extends Parser with internal state not exposed in the public API.
struct ParserDetails : Parser {
    // Only used if leaf_element is CodeBlock:
    u32 num_blank_lines_in_code_block = 0;

    // This flag indicates that some Lists on the stack have their is_loose_if_continued flag set: (Alternatively, we
    // *could* store the number of such Lists on the stack, and eliminate the is_loose_if_continued flag completely, but
    // it would complicate match_existing_indentation a little bit. Sticking with this approach for now.)
    bool check_list_continuations = false;
};

// This is called at the start of each line. It figures out which of the existing elements we are still inside by
// consuming indentation and blockquote '>' markers that match the element stack.
void match_existing_indentation(ParserDetails* parser, LineParser& lp) {
    // Consume leading spaces.
    while (lp.in.num_remaining_bytes() > 0 && (*lp.in.cur_byte == ' ')) {
        lp.in.cur_byte++;
        lp.indent++;
    }

    // Iterate over stack items, matching as much leading indentation and BlockQuote '>' markers as possible.
    PLY_ASSERT(lp.stack_depth == 0);
    while (lp.stack_depth < parser->element_stack.num_items()) {
        Element* element = parser->element_stack[lp.stack_depth];
        if (element->type == Element::BlockQuote) {
            // If there is a '>' within 3 columns of outer_indent, match this BlockQuote element.
            if ((lp.in.num_remaining_bytes() > 0) && (*lp.in.cur_byte == '>') && (lp.inner_indent() <= 3)) {
                lp.stack_depth++;
                lp.in.cur_byte++;
                lp.indent++;
                if (lp.in.num_remaining_bytes() > 0 && (*lp.in.cur_byte == ' ')) {
                    // Read optional space after '>'.
                    lp.in.cur_byte++;
                    lp.indent++;
                }
                lp.outer_indent = lp.indent;
                continue;
            }
            // Consume additional spaces.
            while (lp.in.num_remaining_bytes() > 0 && (*lp.in.cur_byte == ' ')) {
                lp.in.cur_byte++;
                lp.indent++;
            }
        } else if (element->type == Element::ListItem) {
            // If the line's indentation surpasses the list item's indentation, match this ListItem element.
            if (lp.inner_indent() >= element->relative_indent) {
                lp.stack_depth++;
                lp.outer_indent += element->relative_indent;
                continue;
            }
        } else {
            // element_stack can only hold BlockQuote and ListItem elements.
            PLY_ASSERT(0);
        }
        break;
    }
}

// This is called after match_existing_indentation() if the remainder of the line is blank.
void handle_blank_line(ParserDetails* parser, LineParser& lp) {
    // Terminate paragraph if any.
    if (parser->leaf_element && (parser->leaf_element->type == Element::Paragraph)) {
        parser->leaf_element = nullptr;
        PLY_ASSERT(parser->num_blank_lines_in_code_block == 0);
    }

    // Stay inside lists.
    while ((lp.stack_depth < parser->element_stack.num_items()) &&
           (parser->element_stack[lp.stack_depth]->type == Element::ListItem)) {
        lp.stack_depth++;
    }

    // If there's another element in element_stack, it must be a BlockQuote. Terminate it.
    if (lp.stack_depth < parser->element_stack.num_items()) {
        PLY_ASSERT(parser->element_stack[lp.stack_depth]->type == Element::BlockQuote);
        parser->element_stack.resize(lp.stack_depth);
        parser->leaf_element = nullptr;
        parser->num_blank_lines_in_code_block = 0;
    }

    if (parser->leaf_element) {
        // At this point, the only possible leaf element is a CodeBlock, because Paragraphs are terminated above, and
        // Headings don't persist across lines.
        PLY_ASSERT(parser->leaf_element->type == Element::CodeBlock);
        // Count blank lines in CodeBlocks
        if (lp.indent - lp.outer_indent > 4) {
            // Add intermediate blank lines.
            for (u32 i = 0; i < parser->num_blank_lines_in_code_block; i++) {
                parser->leaf_element->raw_lines.append("\n");
            }
            parser->num_blank_lines_in_code_block = 0;
            String code_line = extract_code_line({lp.in.view.start_byte, lp.in.end_byte}, lp.outer_indent + 4);
            parser->leaf_element->raw_lines.append(std::move(code_line));
        } else {
            parser->num_blank_lines_in_code_block++;
        }
    } else {
        // There's no leaf element and the remainder of the line is blank.
        // Walk the stack and set the "isLooseIfContinued" flag on all Lists.
        for (Element* element : parser->element_stack) {
            if (element->type == Element::ListItem) {
                PLY_ASSERT(element->parent->type == Element::List);
                if (!element->parent->is_loose) {
                    element->parent->is_loose_if_continued = true;
                    parser->check_list_continuations = true;
                }
            }
        }
    }
}

// This is called after match_existing_indentation() if the remainder of the line is not blank. It consumes new
// blockquote '>' markers and list item markers such as '*', creating new list elements for each marker encountered.
void parse_new_markers(ParserDetails* parser, LineParser& lp) {
    // Line must not be blank.
    PLY_ASSERT(!lp.in.view_remaining_bytes().trim().is_empty());

    // Attempt to parse new Element markers
    while (lp.in.num_remaining_bytes() > 0) {
        if (lp.inner_indent() >= 4)
            break;

        char* start_byte = lp.in.cur_byte;
        u32 saved_indent = lp.indent;

        // This code block will handle any list markers encountered:
        auto got_list_marker = [&](s32 marker_number, char punc) {
            bool is_ordered = (marker_number >= 0);
            parser->leaf_element = nullptr;
            parser->num_blank_lines_in_code_block = 0;
            Element* list_element = nullptr;
            Element* parent_ctr = &parser->root_element;
            if (parser->element_stack) {
                parent_ctr = parser->element_stack.back();
            }
            PLY_ASSERT(parent_ctr->is_container_block());
            if (!parent_ctr->children.is_empty()) {
                Element* potential_parent = parent_ctr->children.back();
                if (potential_parent->type == Element::List && potential_parent->is_ordered_list() == is_ordered &&
                    potential_parent->list_punc == punc) {
                    // Add item to existing list
                    list_element = potential_parent;
                }
            } else if (parent_ctr->type == Element::ListItem) {
                // Begin new list as a sublist of existing list
                parent_ctr = parent_ctr->parent;
                PLY_ASSERT(parent_ctr->type == Element::List);
            }
            if (!list_element) {
                // Begin new list
                // Note: parent_ctr automatically owns the new Element through its children member.
                list_element = Heap::create<Element>(parent_ctr, Element::List);
                list_element->list_start_number = marker_number;
                list_element->list_punc = punc;
            }
            Element* list_item = Heap::create<Element>(list_element, Element::ListItem);
            list_item->relative_indent = lp.outer_indent;
            parser->element_stack.append(list_item);
        };

        char c = *lp.in.cur_byte;
        PLY_ASSERT(!is_whitespace(c));
        if (c == '>') {
            // Begin a new blockquote
            Element* parent = parser->element_stack ? parser->element_stack.back() : &parser->root_element;
            // Note: parent automatically owns the new Element through its children member.
            parser->element_stack.append(Heap::create<Element>(parent, Element::BlockQuote));
            lp.in.read_byte();
            lp.indent++;
            if ((lp.in.num_remaining_bytes() > 0) && (*lp.in.cur_byte == ' ')) {
                lp.in.cur_byte++;
                lp.indent++;
            }
            lp.outer_indent = lp.indent;
        } else if (c == '*' || c == '-' || c == '+') {
            lp.in.read_byte();
            lp.indent++;
            u32 indent_after_star = lp.indent;
            if ((lp.in.num_remaining_bytes() == 0) || (*lp.in.cur_byte != ' '))
                goto not_marker;
            lp.in.cur_byte++;
            lp.indent++;
            if (parser->leaf_element && lp.in.view_remaining_bytes().trim().is_empty())
                // If the list item interrupts a paragraph, it must not begin with a
                // blank line.
                goto not_marker;

            // It's an unordered list item.
            lp.outer_indent = indent_after_star + 1;
            got_list_marker(-1, c);
        } else if (c >= '0' && c <= '9') {
            u64 num = read_u64_from_text(lp.in);
            if (parser->leaf_element && num != 1) {
                // If list item interrupts a paragraph, the start number must be 1.
                goto not_marker;
            }
            uptr marker_length = (lp.in.cur_byte - start_byte);
            if (marker_length > 9)
                goto not_marker; // marker too long
            lp.indent += numeric_cast<u32>(marker_length);
            if (lp.in.num_remaining_bytes() < 2)
                goto not_marker;
            char punc = *lp.in.cur_byte;
            // FIXME: support alternate punctuator ')'.
            // If the punctuator doesn't match, it should start a new list.
            if (punc != '.' && punc != ')')
                goto not_marker;
            lp.in.read_byte();
            lp.indent++;
            u32 indent_after_marker = lp.indent;
            if ((lp.in.num_remaining_bytes() == 0) || (*lp.in.cur_byte != ' '))
                goto not_marker;
            lp.in.cur_byte++;
            lp.indent++;
            if (parser->leaf_element && lp.in.view_remaining_bytes().trim().is_empty()) {
                // If the list item interrupts a paragraph, it must not begin with a blank line.
                goto not_marker;
            }

            // It's an ordered list item.
            // 32-bit demotion is safe because we know the marker is 9 digits or less.
            lp.outer_indent = indent_after_marker + 1;
            got_list_marker(numeric_cast<s32>(num), punc);
        } else {
            goto not_marker;
        }

        // Consume whitespace
        while ((lp.in.num_remaining_bytes() > 0) && (*lp.in.cur_byte == ' ')) {
            lp.in.cur_byte++;
            lp.indent++;
        }
        continue;

    not_marker:
        lp.in.seek_to(start_byte);
        lp.indent = saved_indent;
        break;
    }
}

void parse_paragraph_text(ParserDetails* parser, LineParser& lp) {
    StringView remaining_text = lp.in.view_remaining_bytes().trim();
    bool has_para = parser->leaf_element && parser->leaf_element->type == Element::Paragraph;
    if (!has_para && lp.inner_indent() >= 4) {
        // Potentially begin or append to code Element
        if (remaining_text && !parser->leaf_element) {
            Element* parent = parser->element_stack ? parser->element_stack.back() : &parser->root_element;
            Element* leaf_element = Heap::create<Element>(parent, Element::CodeBlock);
            parser->leaf_element = leaf_element;
            PLY_ASSERT(parser->num_blank_lines_in_code_block == 0);
        }
        if (parser->leaf_element) {
            PLY_ASSERT(parser->leaf_element->type == Element::CodeBlock);
            // Add intermediate blank lines
            for (u32 i = 0; i < parser->num_blank_lines_in_code_block; i++) {
                parser->leaf_element->raw_lines.append("\n");
            }
            parser->num_blank_lines_in_code_block = 0;
            String code_line = extract_code_line({lp.in.view.start_byte, lp.in.end_byte}, lp.outer_indent + 4);
            parser->leaf_element->raw_lines.append(std::move(code_line));
        }
    } else {
        if (remaining_text) {
            // We're going to create or extend a leaf element. First, check if any Lists should be marked loose:
            if (parser->check_list_continuations) {
                // Yes, we should mark some (possibly zero) lists loose. It's impossible for a leaf element to exist at
                // this point:
                PLY_ASSERT(!parser->leaf_element);
                for (Element* element : parser->element_stack) {
                    if (element->type == Element::ListItem) {
                        PLY_ASSERT(element->parent->type == Element::List);
                        if (element->parent->is_loose_if_continued) {
                            element->parent->is_loose = true;
                            element->parent->is_loose_if_continued = false;
                        }
                    }
                }
                parser->check_list_continuations = false;
            }

            if (*lp.in.cur_byte == '#' && lp.inner_indent() <= 3) {
                // Attempt to parse a heading
                char* start_byte = lp.in.cur_byte;
                while (lp.in.num_remaining_bytes() > 0 && *lp.in.cur_byte == '#') {
                    lp.in.cur_byte++;
                }
                StringView pound_seq{start_byte, lp.in.cur_byte};
                StringView space = read_whitespace(lp.in);
                if (pound_seq.num_bytes() <= 6 && (!space.is_empty() || lp.in.num_remaining_bytes() == 0)) {
                    // Got a heading
                    Element* parent = parser->element_stack ? parser->element_stack.back() : &parser->root_element;
                    Element* heading_element = Heap::create<Element>(parent, Element::Heading);
                    heading_element->heading_level = pound_seq.num_bytes();
                    if (StringView remaining_text = lp.in.view_remaining_bytes().trim()) {
                        heading_element->raw_lines.append(remaining_text);
                    }
                    parser->leaf_element = nullptr;
                    parser->num_blank_lines_in_code_block = 0;
                    return;
                }
                lp.in.seek_to(start_byte);
            }
            // If parser->leaf_element already exists, it's a lazy paragraph continuation
            if (!has_para) {
                // Begin new paragraph
                Element* parent = parser->element_stack ? parser->element_stack.back() : &parser->root_element;
                parser->leaf_element = Heap::create<Element>(parent, Element::Paragraph);
                parser->num_blank_lines_in_code_block = 0;
            }
            parser->leaf_element->raw_lines.append(remaining_text);
        } else {
            PLY_ASSERT(!parser->leaf_element); // Should already be cleared by this point
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
    ArrayView<const String> raw_lines;
    StringView raw_line;
    u32 line_index = 0;
    u32 i = 0;

    InlineConsumer(ArrayView<const String> raw_lines) : raw_lines{raw_lines} {
        PLY_ASSERT(raw_lines.num_items() > 0);
        raw_line = raw_lines[0];
        PLY_ASSERT(raw_line);
    }

    enum ValidIndexResult { SameLine, NextLine, End };

    ValidIndexResult valid_index() {
        if (this->i >= this->raw_line.num_bytes()) {
            if (this->line_index >= this->raw_lines.num_items()) {
                return End;
            }
            this->i = 0;
            this->line_index++;
            if (this->line_index >= this->raw_lines.num_items()) {
                this->raw_line = {};
                return End;
            }
            this->raw_line = this->raw_lines[this->line_index];
            PLY_ASSERT(this->raw_line);
            return NextLine;
        }
        return SameLine;
    }
};

String get_code_span(InlineConsumer& ic, u32 end_tick_count) {
    MemStream mout;
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.valid_index();
        if (res == InlineConsumer::End)
            return {};
        if (res == InlineConsumer::NextLine) {
            mout.write(' ');
        }
        char c = ic.raw_line[ic.i];
        ic.i++;
        if (c == '`') {
            u32 tick_count = 1;
            for (; ic.i < ic.raw_line.num_bytes() && ic.raw_line[ic.i] == '`'; ic.i++) {
                tick_count++;
            }
            if (tick_count == end_tick_count) {
                String result = mout.move_to_string();
                PLY_ASSERT(result);
                if (result[0] == ' ' && result.back() == ' ' && result.find([](char c) { return c != ' '; }) >= 0) {
                    result = result.substr(1, result.num_bytes() - 2);
                }
                return result;
            }
            mout.write(ic.raw_line.substr(ic.i - tick_count, tick_count));
        } else {
            mout.write(c);
        }
    }
}

inline bool is_asc_punc(char c) {
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
    bool left_flanking = false;  // Stars & Underscores only
    bool right_flanking = false; // Stars & Underscores only
    bool active = true;          // Open_Link only
    String text_storage;
    StringView text;
    Owned<Element> element; // Inline_Elem only, and it'll be an inline element type

    Delimiter() = default;
    Delimiter(Type type, StringView text) : type{type}, text{text} {
    }
    Delimiter(Type type, String&& text) : type{type}, text_storage{std::move(text)}, text{text_storage} {
    }
    Delimiter(Owned<Element>&& elem) : type{InlineElem}, element{std::move(elem)} {
    }
    static Delimiter make_run(Type type, StringView raw_line, u32 start, u32 num_bytes) {
        bool preceded_by_white = (start == 0) || is_whitespace(raw_line[start - 1]);
        bool followed_by_white =
            (start + num_bytes >= raw_line.num_bytes()) || is_whitespace(raw_line[start + num_bytes]);
        bool preceded_by_punc = (start > 0) && is_asc_punc(raw_line[start - 1]);
        bool followed_by_punc = (start + num_bytes < raw_line.num_bytes()) && is_asc_punc(raw_line[start + num_bytes]);

        Delimiter result{type, raw_line.substr(start, num_bytes)};
        result.left_flanking =
            !followed_by_white && (!followed_by_punc || (followed_by_punc && (preceded_by_white || preceded_by_punc)));
        result.right_flanking =
            !preceded_by_white && (!preceded_by_punc || (preceded_by_punc && (followed_by_white || followed_by_punc)));
        return result;
    }
};

struct LinkDestination {
    bool success = false;
    String dest;
};

LinkDestination parse_link_destination(InlineConsumer& ic) {
    // FIXME: Support < > destinations
    // FIXME: Support link titles

    // Skip initial whitespace
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.valid_index();
        if (res == InlineConsumer::End) {
            return {false, String{}};
        }
        if (!is_whitespace(ic.raw_line[ic.i]))
            break;
        ic.i++;
    }

    MemStream mout;
    u32 paren_nest_level = 0;
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.valid_index();
        if (res != InlineConsumer::SameLine)
            break;

        char c = ic.raw_line[ic.i];
        if (c == '\\') {
            ic.i++;
            if (ic.valid_index() != InlineConsumer::SameLine) {
                mout.write('\\');
                break;
            }
            c = ic.raw_line[ic.i];
            if (!is_asc_punc(c)) {
                mout.write('\\');
            }
            mout.write(c);
        } else if (c == '(') {
            ic.i++;
            mout.write(c);
            paren_nest_level++;
        } else if (c == ')') {
            if (paren_nest_level > 0) {
                ic.i++;
                mout.write(c);
                paren_nest_level--;
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

    if (paren_nest_level != 0) {
        return {false, String{}};
    }

    // Skip trailing whitespace
    for (;;) {
        InlineConsumer::ValidIndexResult res = ic.valid_index();
        if (res == InlineConsumer::End) {
            return {false, String{}};
        }
        char c = ic.raw_line[ic.i];
        if (c == ')') {
            ic.i++;
            return {true, mout.move_to_string()};
        } else if (!is_whitespace(c)) {
            return {false, String{}};
        }
        ic.i++;
    }
}

Array<Owned<Element>> convert_to_inline_elems(ArrayView<Delimiter> delimiters) {
    Array<Owned<Element>> elements;
    for (Delimiter& delimiter : delimiters) {
        if (delimiter.type == Delimiter::InlineElem) {
            elements.append(std::move(delimiter.element));
        } else {
            if (!(elements.num_items() > 0 && elements.back()->type == Element::Text)) {
                elements.append(Heap::create<Element>(nullptr, Element::Text));
            }
            elements.back()->text += delimiter.text;
        }
    }
    return elements;
}

Array<Owned<Element>> process_emphasis(Array<Delimiter>& delimiters, u32 bottom_pos) {
    u32 star_opener = bottom_pos;
    u32 underscore_opener = bottom_pos;
    for (u32 pos = bottom_pos; pos < delimiters.num_items(); pos++) {
        auto handle_closer = [&](Delimiter::Type type, u32& opener_pos) {
            for (u32 j = pos; j > opener_pos;) {
                --j;
                if (delimiters[j].type == type && delimiters[j].left_flanking) {
                    u32 span_length = min(delimiters[j].text.num_bytes(), delimiters[pos].text.num_bytes());
                    PLY_ASSERT(span_length > 0);
                    Owned<Element> elem =
                        Heap::create<Element>(nullptr, span_length >= 2 ? Element::Strong : Element::Emphasis);
                    elem->add_children(convert_to_inline_elems(delimiters.subview(j + 1, pos - j - 1)));
                    u32 delims_to_subtract = min(span_length, 2u);
                    delimiters[j].text = delimiters[j].text.left(delimiters[j].text.num_bytes() - delims_to_subtract);
                    delimiters[pos].text =
                        delimiters[pos].text.left(delimiters[pos].text.num_bytes() - delims_to_subtract);
                    // We're going to delete from j to pos inclusive, so leave remaining
                    // delimiters if any
                    if (!delimiters[j].text.is_empty()) {
                        j++;
                    }
                    if (!delimiters[pos].text.is_empty()) {
                        pos--;
                    }
                    delimiters.erase(j, pos + 1 - j);
                    delimiters.insert(j) = std::move(elem);
                    pos = j;
                    star_opener = min(star_opener, pos + 1);
                    underscore_opener = min(star_opener, pos + 1);
                    return;
                }
            }
            // None found
            opener_pos = pos + 1;
        };
        if (delimiters[pos].type == Delimiter::Stars && delimiters[pos].right_flanking) {
            handle_closer(Delimiter::Stars, star_opener);
        } else if (delimiters[pos].type == Delimiter::Underscores && delimiters[pos].right_flanking) {
            handle_closer(Delimiter::Underscores, underscore_opener);
        }
    }
    Array<Owned<Element>> result = convert_to_inline_elems(delimiters.subview(bottom_pos));
    delimiters.resize(bottom_pos);
    return result;
}

Array<Owned<Element>> expand_inline_elements(ArrayView<const String> raw_lines) {
    Array<Delimiter> delimiters;
    InlineConsumer ic{raw_lines};
    u32 flushed_index = 0;
    auto flush_text = [&] {
        if (ic.i > flushed_index) {
            delimiters.append({Delimiter::RawText, ic.raw_line.substr(flushed_index, ic.i - flushed_index)});
            flushed_index = ic.i;
        }
    };
    for (;;) {
        if (ic.i >= ic.raw_line.num_bytes()) {
            flush_text();
            ic.i = 0;
            flushed_index = 0;
            ic.line_index++;
            if (ic.line_index >= ic.raw_lines.num_items())
                break;
            ic.raw_line = ic.raw_lines[ic.line_index];
            delimiters.append(Heap::create<Element>(nullptr, Element::SoftBreak));
        }

        char c = ic.raw_line[ic.i];
        if (c == '`') {
            flush_text();
            u32 tick_count = 1;
            for (ic.i++; ic.i < ic.raw_line.num_bytes() && ic.raw_line[ic.i] == '`'; ic.i++) {
                tick_count++;
            }
            // Try consuming code span
            InlineConsumer backup = ic;
            String code_str = get_code_span(ic, tick_count);
            if (code_str) {
                Owned<Element> code_span = Heap::create<Element>(nullptr, Element::CodeSpan);
                code_span->text = std::move(code_str);
                delimiters.append(std::move(code_span));
                flushed_index = ic.i;
            } else {
                ic = backup;
                flush_text();
            }
        } else if (c == '*') {
            flush_text();
            u32 run_length = 1;
            for (ic.i++; ic.i < ic.raw_line.num_bytes() && ic.raw_line[ic.i] == '*'; ic.i++) {
                run_length++;
            }
            delimiters.append(Delimiter::make_run(Delimiter::Stars, ic.raw_line, ic.i - run_length, run_length));
            flushed_index = ic.i;
        } else if (c == '_') {
            flush_text();
            u32 run_length = 1;
            for (ic.i++; ic.i < ic.raw_line.num_bytes() && ic.raw_line[ic.i] == '_'; ic.i++) {
                run_length++;
            }
            delimiters.append(Delimiter::make_run(Delimiter::Underscores, ic.raw_line, ic.i - run_length, run_length));
            flushed_index = ic.i;
        } else if (c == '[') {
            flush_text();
            delimiters.append({Delimiter::OpenLink, ic.raw_line.substr(ic.i, 1)});
            ic.i++;
            flushed_index = ic.i;
        } else if (c == ']') {
            // Try to parse an inline link
            flush_text();
            ic.i++;
            if (!(ic.i < ic.raw_line.num_bytes() && ic.raw_line[ic.i] == '('))
                continue; // No parenthesis

            // Got opening parenthesis
            ic.i++;

            // Look for preceding Open_Link delimiter
            s32 open_link =
                reverse_find(delimiters, [](const Delimiter& delim) { return delim.type == Delimiter::OpenLink; });
            if (open_link < 0)
                continue; // No preceding Open_Link delimiter

            // Found a preceding Open_Link delimiter
            // Try to parse link destination
            InlineConsumer backup = ic;
            LinkDestination link_dest = parse_link_destination(ic);
            if (!link_dest.success) {
                // Couldn't parse link destination
                ic = backup;
                continue;
            }

            // Successfully parsed link destination
            Owned<Element> elem = Heap::create<Element>(nullptr, Element::Link);
            elem->text = std::move(link_dest.dest);
            elem->add_children(process_emphasis(delimiters, open_link + 1));
            delimiters.resize(open_link);
            delimiters.append(std::move(elem));
            flushed_index = ic.i;
        } else {
            ic.i++;
        }
    }

    return process_emphasis(delimiters, 0);
}

static void do_inlines(Element* element) {
    if (element->is_container_block()) {
        PLY_ASSERT(element->raw_lines.is_empty());
        for (Element* child : element->children) {
            do_inlines(child);
        }
    } else {
        PLY_ASSERT(element->is_leaf_block());
        if (element->type != Element::CodeBlock) {
            element->add_children(expand_inline_elements(element->raw_lines));
            element->raw_lines.clear();
        }
    }
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

Owned<Parser> create_parser() {
    return Heap::create<ParserDetails>();
}

String untabify(StringView str, u32 tab_size) {
    MemStream mem;
    u32 column = 0;
    for (char c : str) {
        if (c == '\t') {
            u32 spaces = tab_size - (column % tab_size);
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
    return mem.move_to_string();
}

Owned<Element> parse_line(Parser* parser, StringView line) {
    ParserDetails* details = static_cast<ParserDetails*>(parser);

    // Untabify the input line (if needed) to simplify internal processing.
    String untabified;
    if (line.find(' ') >= 0) {
        constexpr u32 tab_size = 4;
        untabified = untabify(line, tab_size);
        line = untabified;
    }

    LineParser lp{line};

    // Match existing indentation and blockquote '>' markers.
    match_existing_indentation(details, lp);

    if (lp.in.view_remaining_bytes().trim().is_empty()) {
        // The rest of the line is blank.
        handle_blank_line(details, lp);
    } else {
        // There's more text on the current line.
        if (lp.stack_depth < details->element_stack.num_items()) {
            details->element_stack.resize(lp.stack_depth);
            details->leaf_element = nullptr;
            details->num_blank_lines_in_code_block = 0;
        }
        parse_new_markers(details, lp);
        parse_paragraph_text(details, lp);
    }

    if (details->root_element.children.num_items() > 1) {
        // parse_paragraph_text can only add one child element, so root_element can only have
        // exactly 2 elements at this point. Pop the first one and return it.
        PLY_ASSERT(details->root_element.children.num_items() == 2);
        Owned<Element> out = std::move(details->root_element.children[0]);
        details->root_element.children.erase(0);
        do_inlines(out);
        return out;
    }
    return {};
}

Owned<Element> flush(Parser* parser) {
    ParserDetails* details = static_cast<ParserDetails*>(parser);
    // Terminate all existing elements.
    details->element_stack.clear();
    details->leaf_element = nullptr;
    details->num_blank_lines_in_code_block = 0;

    if (details->root_element.children) {
        // There cannot be more than one child element at this point.
        PLY_ASSERT(details->root_element.children.num_items() == 1);
        Owned<Element> element = std::move(details->root_element.children[0]);
        details->root_element.children.erase(0);
        do_inlines(element);
        element->parent = nullptr;
        return element;
    }
    return {};
}

void destroy(Parser* parser) {
    Heap::destroy(static_cast<ParserDetails*>(parser));
}

Array<Owned<Element>> parse_whole_document(StringView markdown) {
    Array<Owned<Element>> elements;
    Owned<Parser> parser = create_parser();
    ViewStream in{markdown};

    while (StringView line = read_line(in)) {
        if (Owned<Element> element = parse_line(parser, line)) {
            elements.append(std::move(element));
        }
    }
    if (Owned<Element> element = flush(parser)) {
        elements.append(std::move(element));
    }

    return elements;
}

String convert_to_html(StringView src) {
    ViewStream in{src};
    MemStream out;
    markdown::HTML_Options options;
    Owned<Parser> parser = create_parser();

    while (StringView line = read_line(in)) {
        if (Owned<Element> element = parse_line(parser, line)) {
            convert_to_html(&out, element, options);
        }
    }
    if (Owned<Element> element = flush(parser)) {
        convert_to_html(&out, element, options);
    }

    return out.move_to_string();
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
            if (element->is_loose) {
                outs->write(" (loose");
            } else {
                outs->write(" (tight");
            }
            if (element->is_ordered_list()) {
                outs->format(", ordered, start={})", element->list_start_number);
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
            outs->format("heading level={}", element->heading_level);
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
            print_escaped_string(*outs, element->text);
            outs->write('"');
            break;
        }
        case Element::Link: {
            outs->write("link destination=\"");
            print_escaped_string(*outs, element->text);
            outs->write('"');
            break;
        }
        case Element::CodeSpan: {
            outs->write("code \"");
            print_escaped_string(*outs, element->text);
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
    for (StringView text : element->raw_lines) {
        outs->format("{}  \"", indent);
        print_escaped_string(*outs, text);
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

void convert_to_html(Stream* outs, const Element* element, const HTML_Options& options) {
    switch (element->type) {
        case Element::List: {
            if (element->is_ordered_list()) {
                if (element->list_start_number != 1) {
                    outs->format("<ol start=\"{}\">\n", element->list_start_number);
                } else {
                    outs->write("<ol>\n");
                }
            } else {
                outs->write("<ul>\n");
            }
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
            }
            if (element->is_ordered_list()) {
                outs->write("</ol>\n");
            } else {
                outs->write("</ul>\n");
            }
            break;
        }
        case Element::ListItem: {
            outs->write("<li>");
            if (!element->parent->is_loose && element->children[0]->type == Element::Paragraph) {
                // Don't output a newline before the paragraph in a tight list.
            } else {
                outs->write("\n");
            }
            for (u32 i = 0; i < element->children.num_items(); i++) {
                convert_to_html(outs, element->children[i], options);
                if (!element->parent->is_loose && element->children[i]->type == Element::Paragraph &&
                    i + 1 < element->children.num_items()) {
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
                convert_to_html(outs, child, options);
            }
            outs->write("</blockquote>\n");
            break;
        }
        case Element::Heading: {
            outs->format("<h{}", element->heading_level);
            if (element->id) {
                if (options.child_anchors) {
                    outs->write(" class=\"anchored\"><span class=\"anchor\" id=\"");
                    print_xml_escaped_string(*outs, element->id);
                    outs->write("\">&nbsp;</span>");
                } else {
                    outs->write(" id=\"");
                    print_xml_escaped_string(*outs, element->id);
                    outs->write("\">");
                }
            } else {
                outs->write('>');
            }
            PLY_ASSERT(element->raw_lines.is_empty());
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
            }
            outs->format("</h{}>\n", element->heading_level);
            break;
        }
        case Element::Paragraph: {
            bool is_inside_tight =
                (element->parent && element->parent->type == Element::ListItem && !element->parent->parent->is_loose);
            if (!is_inside_tight) {
                outs->write("<p>");
            }
            PLY_ASSERT(element->raw_lines.is_empty());
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
            }
            if (!is_inside_tight) {
                outs->write("</p>\n");
            }
            break;
        }
        case Element::CodeBlock: {
            outs->write("<pre>");
            PLY_ASSERT(element->children.is_empty());
            for (StringView raw_line : element->raw_lines) {
                print_xml_escaped_string(*outs, raw_line);
            }
            outs->write("</pre>\n");
            break;
        }
        case Element::Text: {
            print_xml_escaped_string(*outs, element->text);
            PLY_ASSERT(element->children.is_empty());
            break;
        }
        case Element::Link: {
            outs->write("<a href=\"");
            print_xml_escaped_string(*outs, element->text);
            outs->write("\">");
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
            }
            outs->write("</a>");
            break;
        }
        case Element::CodeSpan: {
            outs->write("<code>");
            print_xml_escaped_string(*outs, element->text);
            outs->write("</code>");
            PLY_ASSERT(element->children.is_empty());
            break;
        }
        case Element::SoftBreak: {
            outs->write("\n");
            PLY_ASSERT(element->children.is_empty());
            break;
        }
        case Element::Emphasis: {
            outs->write("<em>");
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
            }
            outs->write("</em>");
            break;
        }
        case Element::Strong: {
            outs->write("<strong>");
            for (const Element* child : element->children) {
                convert_to_html(outs, child, options);
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
