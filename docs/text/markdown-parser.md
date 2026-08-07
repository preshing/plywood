`ply-markdown.h`: Markdown Parser
=================================

The Markdown parser converts Markdown text into a tree of block and span objects. All functions and types in this
module are defined in the `ply::markdown` namespace.

## Parsing options

CommonMark parsing is the default. `ParseOptions` independently enables the supported GitHub Flavored Markdown
(GFM) extensions:

{table caption="`ParseOptions` members"}
`bool`|`tables`|Enables GFM pipe tables
`bool`|`taskListItems`|Recognizes task markers at the start of list items
`bool`|`strikethrough`|Enables text delimited by pairs of tildes
`bool`|`extendedAutolinks`|Recognizes GFM URL and email autolinks without angle brackets
`bool`|`tagFilter`|Escapes the opening characters of the raw HTML tags disallowed by GFM
{/table}

Every field defaults to `false`. `ParseOptions::githubFlavored()` returns an options object with all five fields set
to `true`.

The tag filter implements GFM's tag-filter extension when raw HTML is rendered. It only filters the named GFM tags;
it is not a general-purpose HTML sanitizer. Applications that accept untrusted input must apply an appropriate HTML
sanitization policy to the rendered output.

## Parser

`Parser` supports incremental parsing. Create one with `createParser()`, pass input lines to `parseLine()`, then call
`flush()` after the last line. A completed top-level `Block` is returned when one becomes available; otherwise these
functions return `nullptr`.

### Creation and streaming

`Owned<Parser> createParser(const ParseOptions& options = {})`
> Creates a stateful parser using the selected extensions. Retain the returned `Owned<Parser>` while streaming input.

`Parser* duplicate(Parser* parser)`
> Makes an independent deep copy, including any unfinished block. Copying an `Owned<Parser>` uses this function, so a
> parser snapshot can be retained while either copy continues parsing.

`void destroy(Parser* parser)`
> Destroys a parser created by `createParser()`. `Owned<Parser>` normally performs this automatically.

`Owned<Block> parseLine(Parser* parser, StringView line)`
> Consumes one input line and returns a completed top-level block when available. Call it repeatedly in source order.

`Owned<Block> flush(Parser* parser)`
> Finishes the current top-level block and returns it. Call it once all lines have been supplied.

### Whole-document parsing and HTML

`Array<Owned<Block>> parseWholeDocument(StringView markdown, const ParseOptions& options = {})`
> Parses a complete string and returns its top-level blocks in document order. This is the whole-document equivalent
> of streaming the lines through a parser configured with the same options and then calling `flush()`.

`String convertToHtml(StringView src, const ParseOptions& options = {})`
> Parses a complete Markdown string and returns its HTML. This is the direct convenience API; parsing extensions are
> selected with `ParseOptions`.

`void convertToHtml(Stream* outs, const Block* block, const HTMLOptions& options)`
> Renders an already-parsed block and its descendants to a stream. Use this overload to inspect or modify the AST, or
> to render blocks incrementally. `HTMLOptions` controls rendering rather than Markdown syntax.

{table caption="`HTMLOptions` members"}
`bool`|`childAnchors`|Writes a child anchor span for a heading with an `id`, instead of putting the `id` on the heading
`Functor<String(StringView)>`|`filterLinks`|Transforms each link or image destination before XML escaping and output
{/table}

## Block tree

`Block` stores its concrete type in `var`, a `Variant` of the block structs listed below. `parent` points to its parent
block, or is `nullptr` for a returned top-level block. `userData` is available for application-specific state.

Container types derive from `Block::Inner` and own `childBlocks`. Text-bearing leaf types derive from `Block::Leaf`
and own parsed `spans`. `asInner()` and `asLeaf()` return the corresponding base pointer when applicable, or `nullptr`.
Use `block.var.is<Type>()` and `block.var.as<Type>()` to test and access a concrete type.

{table caption="Container block types"}
`Block::List`|Ordered or unordered list; stores `punctuator`, `startNumber`, and loose-list state
`Block::ListItem`|List item; stores indentation and optional GFM task state in `isTask` and `isChecked`
`Block::BlockQuote`|Block quote containing child blocks
`Block::Table`|GFM table; contains rows and one `TableAlignment` value per column
`Block::TableRow`|Header or body row containing `TableCell` blocks
{/table}

{table caption="Leaf and standalone block types"}
`Block::Heading`|Heading spans plus `level` and optional HTML `id`
`Block::Paragraph`|Paragraph spans
`Block::TableCell`|GFM table-cell spans
`Block::IndentedCodeBlock`|Indented code stored as text spans
`Block::FencedCodeBlock`|Fenced code plus `fenceMarker`, `infoString`, and `relativeIndent`
`Block::HTMLBlock`|Raw HTML text and the `tagFilter` rendering choice captured during parsing
`Block::ThematicBreak`|A thematic break
{/table}

`TableAlignment` has the values `None`, `Left`, `Center`, and `Right`. The delimiter row of a GFM table determines the
alignment recorded for each column.

## Spans

Text-bearing blocks contain a sequence of `Span` objects. As with blocks, the concrete span type is stored in `var` and
can be accessed with `span.var.is<Type>()` or `span.var.as<Type>()`. Container span types derive from `Span::Container`
and own `childSpans`; `asContainer()` returns that base or `nullptr`.

{table caption="Container span types"}
`Span::Link`|Parsed label spans plus `destination` and optional `title`
`Span::Image`|Parsed alt-text spans plus `destination` and optional `title`
`Span::Italic`|Emphasized child spans
`Span::Bold`|Strongly emphasized child spans
`Span::Strikethrough`|GFM strikethrough child spans
{/table}

{table caption="Leaf span types"}
`Span::Text`|Plain text
`Span::Code`|Inline code
`Span::RawHTML`|Raw HTML text and the `tagFilter` rendering choice captured during parsing
`Span::SoftBreak`|A soft line break
`Span::HardBreak`|A hard line break
{/table}

## GitHub Flavored Markdown AST

With the corresponding options enabled, pipe-table syntax produces `Block::Table`, `Block::TableRow`, and
`Block::TableCell`; task-list markers set `Block::ListItem::isTask` and `isChecked`; and strikethrough syntax produces
`Span::Strikethrough`. Extended autolinks produce ordinary `Span::Link` nodes. The tag-filter choice is captured on
`Block::HTMLBlock` and `Span::RawHTML` nodes and applied by the HTML renderer.
