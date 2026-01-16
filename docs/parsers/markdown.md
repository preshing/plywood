{title text="Markdown Parser" include="ply-markdown.h" namespace="ply::markdown"}

The Markdown parser converts Markdown-formatted text into sequences of `Element` objects, which can then be converted to HTML or processed in other ways.

All functions and types in this module are defined in the `ply::markdown` namespace.

## `Parser`

`Parser` is the main class for parsing Markdown. It's designed for incremental use. Create a parser with `create_parser()`, feed it lines of input with `parse_line()`, and call `flush()` when input is complete. Each function returns an `Element` when a top-level block has ended.

{api_summary}
-- Creation and Destruction
Owned<Parser> create_parser()
void destroy(Parser* parser)
-- Parsing
Owned<Element> parse_line(Parser* parser, StringView line)
Owned<Element> flush(Parser* parser)
Array<Owned<Element>> parse_whole_document(StringView markdown)
-- Converting to HTML
void convert_to_html(Stream* outs, const Element* element, const HTML_Options& options)
String convert_to_html(StringView src)
{/api_summary}

### Creation and Destruction

{api_descriptions}
Owned<Parser> create_parser()
--
Creates and returns a new Markdown parser. The parser maintains state across multiple calls to `parse_line()`.

>>
void destroy(Parser* parser)
--
Destroys a parser and frees its resources. This is typically handled automatically when using `Owned<Parser>`.
{/api_descriptions}

### Parsing

{api_descriptions}
Owned<Element> parse_line(Parser* parser, StringView line)
--
Parses a single line of Markdown input. Returns an `Element` representing a completed top-level block (such as a paragraph or list) if one has ended, or `nullptr` if the current block is still being built.

>>
Owned<Element> flush(Parser* parser)
--
Terminates the current top-level block and returns it. Call this after all input lines have been processed to retrieve any remaining content.

>>
Array<Owned<Element>> parse_whole_document(StringView markdown)
--
Parses an entire Markdown document and returns all top-level elements. This is a convenience function equivalent to calling `parse_line()` for each line followed by `flush()`.
{/api_descriptions}

### Converting to HTML

{api_descriptions}
void convert_to_html(Stream* outs, const Element* element, const HTML_Options& options)
--
Converts an `Element` and all its children to HTML, writing the output to the provided stream. The `HTML_Options` struct controls conversion behavior:

{table caption="`HTML_Options` members"}
`bool`|`child_anchors`|If true, generates anchor elements for headings
{/table}

>>
String convert_to_html(StringView src)
--
Convenience function that parses an entire Markdown document and converts it directly to HTML. This is equivalent to parsing all lines, collecting the elements, and converting each to HTML.
{/api_descriptions}

### Parser State

The `Parser` struct exposes three members that represent the top-level element currently being built:

{table caption="`Parser` member variables"}
`Element`|`root_element`|The top-level element being constructed; returned by `parse_line()` or `flush()` when complete
`Array<Element*>`|`element_stack`|Ancestor elements (`BlockQuote` or `ListItem`) containing the current parsing location
`Element*`|`leaf_element`|The innermost block (`Paragraph` or `CodeBlock`) receiving text, or `nullptr` if none is active
{/table}

These members let you inspect the parser's current state. All elements in `element_stack` and `leaf_element` are owned by `root_element` (as descendants in its `children` tree). When a top-level block is complete, it is detached from `root_element` and returned.

## `Element`

The parser produces a tree of `Element` objects with the following member variables:

{table caption="`Element` member variables"}
`Type`|`type`|The element type
`u32`|`indent_or_level`|Indentation (for list items) or heading level (1-6)
`s32`|`list_start_number`|Starting number for ordered lists; -1 for unordered
`bool`|`is_loose`|Whether a list has blank lines between items
`char`|`list_punc`|List marker character (`-`, `*`, `+`, or `.`)
`Array<Owned<Element>>`|`children`|Child elements
`Element*`|`parent`|Parent element (or `nullptr` for root)
`Array<String>`|`raw_lines`|Raw text lines for leaf blocks
`String`|`text`|Text content for `Text`, `CodeSpan`, or link destination
`String`|`id`|HTML id attribute for headings
{/table}

Each element has a type indicating what kind of Markdown element it represents, and may contain child elements or text content depending on its type.

{api_summary title="Element types"}
-- Container Blocks
Element::None
Element::List
Element::ListItem
Element::BlockQuote
-- Leaf Blocks
Element::Heading
Element::Paragraph
Element::CodeBlock
-- Inline Elements
Element::Text
Element::Link
Element::CodeSpan
Element::SoftBreak
Element::Emphasis
Element::Strong
{/api_summary}

{api_descriptions class=Element}
Element::None
--
Default element type, typically used for the root of the document.

>>
Element::List
--
An ordered or unordered list. Contains `ListItem` children. Use `list_start_number` to determine if the list is ordered (>= 0) or unordered (-1). The `list_punc` member indicates the list marker character (e.g., `-`, `*`, or `.`).

>>
Element::ListItem
--
An individual item within a list. The `indent_or_level` member indicates the indentation level.

>>
Element::BlockQuote
--
A block quote. Contains other block-level elements as children.

>>
Element::Heading
--
A heading (H1-H6). The `indent_or_level` member indicates the heading level (1-6). The `id` member can be used to set an HTML id attribute. Text content is stored in `raw_lines`.

>>
Element::Paragraph
--
A paragraph of text. Text content is stored in `raw_lines`.

>>
Element::CodeBlock
--
A fenced or indented code block. The raw code is stored in `raw_lines`.

>>
Element::Text
--
Plain text content within an inline context. The text is stored in the `text` member.

>>
Element::Link
--
A hyperlink. The link destination URL is stored in the `text` member. Child elements contain the link text.

>>
Element::CodeSpan
--
Inline code (backtick-delimited). The code content is stored in the `text` member.

>>
Element::SoftBreak
--
A soft line break within a paragraph.

>>
Element::Emphasis
--
Emphasized text (typically rendered as italic). Child elements contain the emphasized content.

>>
Element::Strong
--
Strongly emphasized text (typically rendered as bold). Child elements contain the content.
{/api_descriptions}

### `Element` Member Functions

{api_summary class=Element title="Element member functions"}
bool is_container_block() const
bool is_leaf_block() const
bool is_inline_element() const
bool is_ordered_list() const
void add_children(ArrayView<Owned<Element>> new_children)
{/api_summary}

{api_descriptions class=Element}
bool is_container_block() const
--
Returns `true` if the element is a container block (`None`, `List`, `ListItem`, or `BlockQuote`) that can have child blocks.

>>
bool is_leaf_block() const
--
Returns `true` if the element is a leaf block (`Heading`, `Paragraph`, or `CodeBlock`) that contains text but not child blocks.

>>
bool is_inline_element() const
--
Returns `true` if the element is an inline element (`Text`, `Link`, `CodeSpan`, `SoftBreak`, `Emphasis`, or `Strong`).

>>
bool is_ordered_list() const
--
Returns `true` if the element is an ordered list (type is `List` and `list_start_number` >= 0).

>>
void add_children(ArrayView<Owned<Element>> new_children)
--
Adds child elements to this element and sets their parent pointers.
{/api_descriptions}
