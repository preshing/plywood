`ply-markdown.h`: Markdown Parser
=================================

The Markdown parser converts Markdown text into a tree of block and span objects. All functions and types in this
module are defined in the `ply::markdown` namespace.

## Parsing options

`ParseOptions` independently controls recognition of each supported Markdown construct. CommonMark constructs are
enabled by default. Inline elements appear first, matching the declaration order:

| | | |
| --- | --- | --- |
| `bool` | `backslashEscapes` | Recognizes backslash escapes before ASCII punctuation |
| `bool` | `characterReferences` | Decodes named and numeric character references |
| `bool` | `codeSpans` | Recognizes inline code delimited by backticks |
| `bool` | `emphasis` | Recognizes emphasis delimiters |
| `bool` | `strongEmphasis` | Recognizes strong-emphasis delimiters |
| `bool` | `inlineLinks` | Recognizes links with inline destinations |
| `bool` | `referenceLinks` | Resolves full, collapsed and shortcut reference links |
| `bool` | `inlineImages` | Recognizes images with inline destinations |
| `bool` | `referenceImages` | Resolves full, collapsed and shortcut reference images |
| `bool` | `autolinks` | Recognizes URI and email autolinks enclosed in angle brackets |
| `bool` | `inlineHTML` | Recognizes CommonMark inline HTML |
| `bool` | `softLineBreaks` | Produces soft-break spans at ordinary line boundaries |
| `bool` | `hardLineBreaks` | Recognizes hard breaks created by spaces or a backslash |

Block elements follow:

| | | |
| --- | --- | --- |
| `bool` | `blockQuotes` | Recognizes block quote markers |
| `bool` | `orderedLists` | Recognizes ordered list markers |
| `bool` | `unorderedLists` | Recognizes unordered list markers |
| `bool` | `indentedCodeBlocks` | Recognizes code blocks created by indentation |
| `bool` | `fencedCodeBlocks` | Recognizes backtick- and tilde-fenced code blocks |
| `bool` | `htmlBlocks` | Recognizes CommonMark HTML blocks |
| `bool` | `atxHeadings` | Recognizes headings beginning with `#` markers |
| `bool` | `setextHeadings` | Recognizes headings followed by `=` or `-` underlines |
| `bool` | `thematicBreaks` | Recognizes thematic breaks |
| `bool` | `linkReferenceDefinitions` | Collects and removes link reference definitions |

Paragraphs and plain text are always available as fallbacks. When a construct is disabled, its markers remain text
and do not open, close or interrupt blocks. Alternate delimiters for one construct share a flag; for example,
`fencedCodeBlocks` controls both backtick and tilde fences.

GitHub Flavored Markdown (GFM) extensions are disabled by default:

| | | |
| --- | --- | --- |
| `bool` | `tables` | Enables GFM pipe tables |
| `bool` | `taskListItems` | Recognizes task markers at the start of list items |
| `bool` | `strikethrough` | Enables text delimited by pairs of tildes |
| `bool` | `extendedAutolinks` | Recognizes GFM URL and email autolinks without angle brackets |
| `bool` | `tagFilter` | Escapes the opening characters of the raw HTML tags disallowed by GFM |

`ParseOptions::none()` returns an options object with recognition of all CommonMark and GFM constructs disabled.
`ParseOptions::githubFlavored()` returns the default CommonMark configuration with all five GFM fields set to
`true`.

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

`Array<Owned<Span>> parseInlineSpans(StringView markdown, const ParseOptions& options = {})`
> Parses text directly as paragraph-style inline content without recognizing block constructs. Link reference
> definitions are not collected by this entry point, so reference links require document parsing.

`String convertInlineToHtml(StringView src, const ParseOptions& parseOptions = {}, const HTMLOptions& htmlOptions = {})`
> Parses paragraph-style inline content and returns its rendered HTML without adding block markup. Parsing extensions
> are selected with `ParseOptions`, while `HTMLOptions` controls rendering and link transformation.

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

| | | |
| --- | --- | --- |
| `bool` | `childAnchors` | Writes a child anchor span for a heading with an `id`, instead of putting the `id` on the heading |
| `Functor<String(StringView)>` | `filterLinks` | Transforms each link or image destination before XML escaping and output |

## Block tree

`Block` stores its concrete type in `var`, a `Variant` of the block structs listed below. `parent` points to its parent
block, or is `nullptr` for a returned top-level block. `userData` is available for application-specific state.

Container types derive from `Block::Inner` and own `childBlocks`. Text-bearing leaf types derive from `Block::Leaf`
and own parsed `spans`. `asInner()` and `asLeaf()` return the corresponding base pointer when applicable, or `nullptr`.
Use `block.var.is<Type>()` and `block.var.as<Type>()` to test and access a concrete type.

| | |
| --- | --- |
| `Block::List` | Ordered or unordered list; stores `punctuator`, `startNumber`, and loose-list state |
| `Block::ListItem` | List item; stores indentation and optional GFM task state in `isTask` and `isChecked` |
| `Block::BlockQuote` | Block quote containing child blocks |
| `Block::Table` | GFM table; contains rows and one `TableAlignment` value per column |
| `Block::TableRow` | Header or body row containing `TableCell` blocks |

| | |
| --- | --- |
| `Block::Heading` | Heading spans plus `level` and optional HTML `id` |
| `Block::Paragraph` | Paragraph spans |
| `Block::TableCell` | GFM table-cell spans |
| `Block::IndentedCodeBlock` | Indented code stored as text spans |
| `Block::FencedCodeBlock` | Fenced code plus `fenceMarker`, `infoString`, and `relativeIndent` |
| `Block::HTMLBlock` | Raw HTML text and the `tagFilter` rendering choice captured during parsing |
| `Block::ThematicBreak` | A thematic break |

`TableAlignment` has the values `None`, `Left`, `Center`, and `Right`. The delimiter row of a GFM table determines the
alignment recorded for each column.

## Spans

Text-bearing blocks contain a sequence of `Span` objects. As with blocks, the concrete span type is stored in `var` and
can be accessed with `span.var.is<Type>()` or `span.var.as<Type>()`. Container span types derive from `Span::Container`
and own `childSpans`; `asContainer()` returns that base or `nullptr`.

| | |
| --- | --- |
| `Span::Link` | Parsed label spans plus `destination` and optional `title` |
| `Span::Image` | Parsed alt-text spans plus `destination` and optional `title` |
| `Span::Italic` | Emphasized child spans |
| `Span::Bold` | Strongly emphasized child spans |
| `Span::Strikethrough` | GFM strikethrough child spans |

| | |
| --- | --- |
| `Span::Text` | Plain text |
| `Span::Code` | Inline code |
| `Span::RawHTML` | Raw HTML text and the `tagFilter` rendering choice captured during parsing |
| `Span::SoftBreak` | A soft line break |
| `Span::HardBreak` | A hard line break |

## GitHub Flavored Markdown AST

With the corresponding options enabled, pipe-table syntax produces `Block::Table`, `Block::TableRow`, and
`Block::TableCell`; task-list markers set `Block::ListItem::isTask` and `isChecked`; and strikethrough syntax produces
`Span::Strikethrough`. Extended autolinks produce ordinary `Span::Link` nodes. The tag-filter choice is captured on
`Block::HTMLBlock` and `Span::RawHTML` nodes and applied by the HTML renderer.
