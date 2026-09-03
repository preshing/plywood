`ply-markdown.h`: Markdown Parser
=================================

The Markdown parser converts Markdown text into a tree of block and span objects. All functions and types in this
module are defined in the `ply::markdown` namespace.

## `Parser`

`Parser` supports incremental parsing. Create one with `createParser()`, pass input lines to `parseLine()`, then call
`flush()` after the last line. Top level blocks are returned as they become available.

### Creation and streaming

`Owned<Parser> createParser(const ParseOptions& options = {})`
> Creates a stateful parser using the selected extensions. Retain the returned `Owned<Parser>` while streaming input.
> Available options:
>
> | | |
> | --- | --- |
> | `bool backslashEscapes` | Recognizes backslash escapes before ASCII punctuation. |
> | `bool characterReferences` | Decodes named and numeric character references. |
> | `bool codeSpans` | Recognizes inline code delimited by backticks. |
> | `bool emphasis` | Recognizes emphasis delimiters. |
> | `bool strongEmphasis` | Recognizes strong-emphasis delimiters. |
> | `bool inlineLinks` | Recognizes links with inline destinations. |
> | `bool referenceLinks` | Resolves full, collapsed and shortcut reference links. |
> | `bool inlineImages` | Recognizes images with inline destinations. |
> | `bool referenceImages` | Resolves full, collapsed and shortcut reference images. |
> | `bool autolinks` | Recognizes URI and email autolinks enclosed in angle brackets. |
> | `bool inlineHTML` | Recognizes CommonMark inline HTML. |
> | `bool softLineBreaks` | Produces soft-break spans at ordinary line boundaries. |
> | `bool hardLineBreaks` | Recognizes hard breaks created by spaces or a backslash. |
> | `bool blockQuotes` | Recognizes block quote markers. |
> | `bool orderedLists` | Recognizes ordered list markers. |
> | `bool unorderedLists` | Recognizes unordered list markers. |
> | `bool indentedCodeBlocks` | Recognizes code blocks created by indentation. |
> | `bool fencedCodeBlocks` | Recognizes backtick- and tilde-fenced code blocks. |
> | `bool htmlBlocks` | Recognizes CommonMark HTML blocks. |
> | `bool atxHeadings` | Recognizes headings beginning with `#` markers. |
> | `bool setextHeadings` | Recognizes headings followed by `=` or `-` underlines. |
> | `bool thematicBreaks` | Recognizes thematic breaks. |
> | `bool linkReferenceDefinitions` | Collects and removes link reference definitions. |
> 
> GitHub Flavored Markdown extensions are disabled by default.
> Use `ParseOptions::githubFlavored()` to obtain an object with all element types enabled.
> 
> | | |
> | --- | --- |
> | `bool tables` | Enables pipe tables. |
> | `bool taskListItems` | Recognizes task markers at the start of list items. |
> | `bool strikethrough` | Enables text delimited by pairs of tildes. |
> | `bool extendedAutolinks` | Recognizes URL and email autolinks without angle brackets. |
> | `bool tagFilter` | Escapes the opening characters of the raw HTML tags disallowed by. |


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

`Array<Owned<Block>> parse(StringView markdown, const ParseOptions& options = {})`
> Parses a complete string and returns its top-level blocks in document order. This is the whole-document equivalent
> of streaming the lines through a parser configured with the same options and then calling `flush()`.

`String convertToHtml(StringView src, const ParseOptions& options = {})`
> Parses a complete Markdown string and returns its HTML. This is the direct convenience API; parsing extensions are
> selected with `ParseOptions`.

`void convertToHtml(Stream* outs, const Block* block, const HTMLOptions& options)`
> Renders an already-parsed block and its descendants to a stream. Use this overload to inspect or modify the AST, or
> to render blocks incrementally. `HTMLOptions` controls rendering rather than Markdown syntax.
> 
> | | |
> | --- | --- |
> | `bool childAnchors` | Writes a child anchor span for a heading with an `id`, instead of putting the `id` on the heading. Default is false. |
> | `Functor<String(StringView)> filterLinks` | Transforms each link or image destination before XML escaping and output |
