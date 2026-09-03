/*───────────────────────────────────────────────────────────────────┐
│                                                                    │
│     ____      Plywood C++ Runtime Library                          │
│    ╱   ╱╲     https://plywood.dev/                                 │
│   ╱___╱╭╮╲                                                         │
│    └──┴┴┴┘    generate-docs                                        │
│               Documentation: docs/apps/generate-docs.md            │
│                                                                    │
└───────────────────────────────────────────────────────────────────*/

#include <ply-markdown.h>
#include <ply-cpp.h>
#include <ply-reflect.h>

using namespace ply;
using namespace ply::cpp;

String sourceFolder = joinPath(PLYWOOD_ROOT_DIR, "apps/generate-docs/data");
String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs");
String outFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");
u32 publishKey = 0; // Prevent browsers from caching old stylesheets

// Describes one article heading included in its expanded sidebar entry.
struct PageHeading {
    u32 level = 0;
    String id;
    String titleHtml;
};

// Stores one page from the Markdown table of contents in a form ready for HTML generation.
struct ContentsPage {
    String titleHtml;
    String documentTitle;
    String path;
    Array<Owned<ContentsPage>> children;
};
Array<Owned<ContentsPage>> contents;

// Tracks generated output so unchanged files can be preserved and obsolete files removed.
struct GenerationStats {
    Set<String> generatedPaths;
    u32 numUpdated = 0;
    u32 numUnchanged = 0;
    u32 numOrphanedFilesRemoved = 0;
    u32 numOrphanedDirsRemoved = 0;
};
GenerationStats stats;

// Stores the command-line options that control documentation generation.
struct CommandLineOptions {
    bool watch = false;
    bool printUsage = false;
    PLY_DECLARE_TYPE_INFO(CommandLineOptions)
};

// Writes one generated file only when its contents changed and records its path as expected output.
void writeFileIfChanged(StringView path, StringView contents) {
    stats.generatedPaths.insert(path);

    // Retain an existing file when its bytes already match the generated contents.
    if ((FileSystem::exists(path) == ExistsResult::File) && (FileSystem::loadBinary(path) == contents)) {
        stats.numUnchanged++;
        return;
    }

    // Create the parent directory and write the new contents.
    FileSystem::makeDirs(splitPath(path).directory);
    FSResult result = FileSystem::saveBinary(path, contents);
    PLY_ASSERT(result == FSResult::OK);
    PLY_UNUSED(result);
    stats.numUpdated++;
}

// Appends the current publish key query parameter to an asset path in HTML text.
void appendPublishKeyToAsset(String& text, StringView assetPath) {
    text = text.replace(assetPath, String::format("{}?key={}", assetPath, publishKey));
}

// Returns the first function declarator, if present.
const DeclProduction::Function* findFunctionDeclarator(const Declaration& decl) {
    const Declaration* mainDecl = &decl;
    while (auto* tmpl = mainDecl->var.as<Declaration::Template>()) {
        mainDecl = tmpl->childDecl;
    }

    auto* entity = mainDecl->var.as<Declaration::Entity>();
    if (!entity || entity->initDeclarators.isEmpty())
        return nullptr;

    // Find the function production belonging to the first declared entity.
    for (const DeclProduction* prod = entity->initDeclarators[0].prod; prod; prod = prod->child) {
        if (auto* function = prod->var.as<DeclProduction::Function>()) {
            return function;
        }
    }
    return nullptr;
}

// Renders a declaration as a single highlighted code fragment for API description titles.
void printDeclAsApiTitle(Stream& out, const Parser* parser, const Declaration& decl) {
    Array<TokenSpan> spans = parser->syntaxHighlight(decl);
    const DeclProduction::Function* function = findFunctionDeclarator(decl);
    if (function) {
        out.write("<code class=\"api-decl\"><span class=\"api-decl-prefix\">");
    } else {
        out.write("<code>");
    }

    // Output token spans.
    TokenSpan::Color lastColor = TokenSpan::None;
    bool inFunctionParams = false;
    bool startNextParam = false;
    u32 paramIndex = 0;
    for (const TokenSpan& span : spans) {
        if (startNextParam) {
            out.write(" <span class=\"api-param\">");
            startNextParam = false;
            if (span.isSpace)
                continue;
        }
        if (lastColor != span.color) {
            if (lastColor != TokenSpan::None) {
                out.write("</span>");
            }
            if (span.color == TokenSpan::Type) {
                out.write("<span class=\"type\">");
            } else if (span.color == TokenSpan::Symbol) {
                out.write("<span class=\"symbol\">");
            } else if (span.color == TokenSpan::Variable) {
                out.write("<span class=\"var\">");
            }
            lastColor = span.color;
        }
        if (span.isSpace) {
            out.write(" ");
        } else {
            printXmlEscapedString(out, span.token.text);
        }
        if (function && (span.token == function->openParen)) {
            if (lastColor != TokenSpan::None) {
                out.write("</span>");
                lastColor = TokenSpan::None;
            }
            out.write("</span><span class=\"api-decl-params\">");
            if (!function->params.isEmpty()) {
                out.write("<span class=\"api-param\">");
            }
            inFunctionParams = true;
        }
        if (function && inFunctionParams && (paramIndex < function->params.numItems()) &&
            function->params[paramIndex].comma.isValid() &&
            (span.token == function->params[paramIndex].comma)) {
            out.write("</span>");
            startNextParam = true;
            paramIndex++;
        }
    }
    if (lastColor != TokenSpan::None) {
        out.write("</span>");
    }
    if (function) {
        if (!function->params.isEmpty()) {
            out.write("</span>");
        }
        out.write("</span>");
        PLY_ASSERT(inFunctionParams);
        PLY_ASSERT(!startNextParam);
    }
    out.write("</code>");
}

// Renders a table declaration, preserving source text the syntax visitor does not expose.
void printHighlightedTableDeclaration(Stream& out, ArrayView<const TokenSpan> spans, StringView source) {
    out.write("<code>");

    // Match highlighted tokens to the source while preserving text between them, including array extents.
    u32 searchOffset = 0;
    u32 outputOffset = 0;
    for (const TokenSpan& span : spans) {
        if (span.isSpace || !span.token.text)
            continue;

        // Match the visitor's ordered tokens against the source; nested token offsets aren't monotonic.
        s32 tokenOffset = source.find(span.token.text, searchOffset);
        if (tokenOffset < 0)
            continue;
        searchOffset = tokenOffset + span.token.text.numBytes();

        // Preserve source text between parser tokens.
        printXmlEscapedString(out, source.substr(outputOffset, tokenOffset - outputOffset));
        if (span.color == TokenSpan::Type) {
            out.write("<span class=\"type\">");
        } else if (span.color == TokenSpan::Symbol) {
            out.write("<span class=\"symbol\">");
        } else if (span.color == TokenSpan::Variable) {
            out.write("<span class=\"var\">");
        }
        printXmlEscapedString(out, span.token.text);
        if (span.color != TokenSpan::None) {
            out.write("</span>");
        }
        outputOffset = searchOffset;
    }
    printXmlEscapedString(out, source.substr(outputOffset));
    out.write("</code>");
}

// Highlights one blank-heading declaration/description table, leaving all other tables unchanged.
void highlightMemberTable(markdown::Block::Table* table, StringView className) {
    if (table->childBlocks.numItems() < 2)
        return;

    // Require exactly two empty header cells before inspecting body declarations.
    auto* header = table->childBlocks[0]->var.as<markdown::Block::TableRow>();
    if (!header || (header->childBlocks.numItems() != 2))
        return;
    auto* declarationHeader = header->childBlocks[0]->var.as<markdown::Block::TableCell>();
    auto* descriptionHeader = header->childBlocks[1]->var.as<markdown::Block::TableCell>();
    if (!declarationHeader || !descriptionHeader || !declarationHeader->spans.isEmpty() ||
        !descriptionHeader->spans.isEmpty())
        return;

    Array<markdown::Span*> declarationSpans;
    for (u32 rowIndex = 1; rowIndex < table->childBlocks.numItems(); rowIndex++) {
        // Require exactly two cells and one code span in the declaration cell.
        auto* row = table->childBlocks[rowIndex]->var.as<markdown::Block::TableRow>();
        if (!row || (row->childBlocks.numItems() != 2))
            return;
        auto* declarationCell = row->childBlocks[0]->var.as<markdown::Block::TableCell>();
        if (!declarationCell || (declarationCell->spans.numItems() != 1))
            return;
        auto* declarationCode = declarationCell->spans[0]->var.as<markdown::Span::Code>();
        if (!declarationCode)
            return;

        // Require a complete, error-free declaration with exactly one named entity declarator.
        Owned<Parser> validationParser = Parser::create();
        ParseResult parseResult = validationParser->parseFile({}, declarationCode->text + ";");
        auto* validatedEntity = (parseResult.declarations.numItems() == 1)
                                    ? parseResult.declarations[0].var.as<Declaration::Entity>()
                                    : nullptr;
        if (!parseResult.success || !validatedEntity || (validatedEntity->initDeclarators.numItems() != 1) ||
            validatedEntity->initDeclarators[0].qid.isEmpty())
            return;

        declarationSpans.append(declarationCell->spans[0]);
    }

    // Reparse and highlight the declarations only after the entire table has validated.
    for (markdown::Span* span : declarationSpans) {
        auto* declarationCode = span->var.as<markdown::Span::Code>();
        PLY_ASSERT(declarationCode);
        Owned<Parser> parser = Parser::create();
        Declaration decl = parser->parseDeclaration(declarationCode->text, className);
        auto* entity = decl.var.as<Declaration::Entity>();
        PLY_ASSERT(entity && (entity->initDeclarators.numItems() == 1) && !entity->initDeclarators[0].qid.isEmpty());
        Array<TokenSpan> spans = parser->syntaxHighlight(decl);
        MemStream html;
        printHighlightedTableDeclaration(html, spans, declarationCode->text);
        auto& rawHTML = span->var.switchTo<markdown::Span::RawHTML>();
        rawHTML.text = html.moveToString();
    }
}

// Recursively highlights member tables at top level and inside API-description blockquotes.
void highlightMemberTables(markdown::Block* block, StringView className) {
    if (auto* table = block->var.as<markdown::Block::Table>()) {
        highlightMemberTable(table, className);
    }

    // Visit tables in blockquotes and any other Markdown containers.
    if (markdown::Block::Inner* inner = block->asInner()) {
        for (markdown::Block* child : inner->childBlocks) {
            highlightMemberTables(child, className);
        }
    }
}

// Converts a source documentation link to the corresponding generated site URL.
String convertDocsPathToURL(StringView destination) {
    if (!destination.startsWith("/docs/"))
        return destination;

    // Separate any query or fragment before inspecting the Markdown path.
    s32 suffixPos = destination.find([](char c) { return c == '?' || c == '#'; });
    StringView path = suffixPos >= 0 ? destination.left(suffixPos) : destination;
    StringView suffix = suffixPos >= 0 ? destination.substr(suffixPos) : StringView{};

    // Use the documentation root as the public URL for the introduction page.
    if (path == "/docs/introduction.md")
        return StringView{"/docs"} + suffix;

    // Remove the source filename while retaining the documentation route and suffix.
    if (path.endsWith("/index.md")) {
        path = path.shortenedBy(9);
    } else if (path.endsWith(".md")) {
        path = path.shortenedBy(3);
    } else {
        return destination;
    }
    return path + suffix;
}

// Returns true when a paragraph is made of code spans separated only by soft breaks.
bool parseApiDeclarationParagraph(const markdown::Block* block, Array<String>* declarations = nullptr) {
    auto* para = block->var.as<markdown::Block::Paragraph>();
    if (!para) {
        return false;
    }

    if (declarations) {
        declarations->clear();
    }

    bool expectDeclaration = true;
    for (const markdown::Span* span : para->spans) {
        if (auto* code = span->var.as<markdown::Span::Code>()) {
            if (!expectDeclaration) {
                return false;
            }
            if (declarations) {
                declarations->append(code->text);
            }
            expectDeclaration = false;
        } else if (span->var.is<markdown::Span::SoftBreak>()) {
            if (expectDeclaration) {
                return false;
            }
            expectDeclaration = true;
        } else {
            return false;
        }
    }

    return !expectDeclaration;
}

// Parses and renders one or more declaration strings as an API definition title.
void printApiDeclarationsAsTitle(Stream& out, StringView className, const Array<String>& declarations) {
    bool firstDecl = true;
    for (const String& declText : declarations) {
        Owned<Parser> parser = Parser::create();
        Declaration decl = parser->parseDeclaration(declText, className);
        if (!firstDecl) {
            out.write("<br>\n");
        }
        printDeclAsApiTitle(out, parser, decl);
        firstDecl = false;
    }
}

// Writes the plain-text contribution of one inline span for use in navigation labels and fragment IDs.
void writeHeadingText(Stream& out, const markdown::Span* span) {
    if (auto* text = span->var.as<markdown::Span::Text>()) {
        out.write(text->text);
    } else if (auto* code = span->var.as<markdown::Span::Code>()) {
        out.write(code->text);
    } else if (span->var.is<markdown::Span::SoftBreak>() || span->var.is<markdown::Span::HardBreak>()) {
        out.write(' ');
    } else if (const markdown::Span::Container* container = span->asContainer()) {
        for (const markdown::Span* child : container->childSpans) {
            writeHeadingText(out, child);
        }
    }
}

// Creates a unique, readable fragment ID from a heading title.
String makeHeadingID(StringView title, const Array<PageHeading>& existingHeadings) {
    String lowerTitle = title.lower();
    MemStream out;
    bool needSeparator = false;
    for (u32 i = 0; i < title.numBytes(); i++) {
        char c = title[i];
        if (!isAlpha(c) && !isDigit(c)) {
            needSeparator = out.getSeekPos() > 0;
            continue;
        }

        // Split camel-case words while keeping initialisms such as HTTP together.
        bool isUpper = isAlpha(c) && (c != lowerTitle[i]);
        bool previousIsLower = (i > 0) && isAlpha(title[i - 1]) && (title[i - 1] == lowerTitle[i - 1]);
        bool previousIsUpper = (i > 0) && isAlpha(title[i - 1]) && (title[i - 1] != lowerTitle[i - 1]);
        bool nextIsLower = (i + 1 < title.numBytes()) && isAlpha(title[i + 1]) &&
                           (title[i + 1] == lowerTitle[i + 1]);
        bool startsWord = isUpper && (previousIsLower || (previousIsUpper && nextIsLower));
        if ((needSeparator || startsWord) && (out.getSeekPos() > 0)) {
            out.write('-');
        }
        needSeparator = false;
        out.write(lowerTitle[i]);
    }
    String baseID = out.moveToString();
    if (!baseID) {
        baseID = "section";
    }

    // Add a numeric suffix when a page repeats the same heading title.
    String id = baseID;
    for (u32 suffix = 1;; suffix++) {
        bool found = false;
        for (const PageHeading& heading : existingHeadings) {
            if (heading.id == id) {
                found = true;
                break;
            }
        }
        if (!found)
            return id;
        id = String::format("{}-{}", baseID, suffix);
    }
}

// Assigns fragment IDs to level 2-3 headings and records them for the sidebar table of contents.
void collectPageHeadings(markdown::Block* block, Array<PageHeading>& headings) {
    if (auto* heading = block->var.as<markdown::Block::Heading>()) {
        if ((heading->level == 2) || (heading->level == 3)) {
            MemStream titleOut;
            for (const markdown::Span* span : heading->spans) {
                writeHeadingText(titleOut, span);
            }
            String title = titleOut.moveToString().trim();
            heading->id = makeHeadingID(title, headings);
            MemStream htmlOut;
            markdown::HTMLOptions options;
            for (const markdown::Span* span : heading->spans) {
                markdown::convertSpanToHtml(&htmlOut, span, options);
            }
            headings.append({heading->level, heading->id, htmlOut.moveToString()});
        }
    }
    if (markdown::Block::Inner* inner = block->asInner()) {
        for (markdown::Block* child : inner->childBlocks) {
            collectPageHeadings(child, headings);
        }
    }
}

// Buffers parsed markdown blocks and emits special API description structures when detected.
class MarkdownBlockProcessor {
public:
    // Creates a block processor that writes converted output to the supplied stream.
    MarkdownBlockProcessor(Stream& out, Array<PageHeading>& headings) : out{out}, headings{headings} {
        this->options.filterLinks = convertDocsPathToURL;
    }

    // Sets the class context used when parsing declaration-only markdown entries.
    void setApiClassContext(StringView className) {
        this->apiClassContext = className;
    }

    // Parses one markdown line and appends any completed block to the pending queue.
    void parseMarkdownLine(StringView line) {
        if (Owned<markdown::Block> node = markdown::parseLine(this->parser, line)) {
            this->pendingBlocks.append(std::move(node));
        }
    }

    // Flushes the markdown parser and emits all pending blocks in one pass.
    void flushToOutput() {
        if (Owned<markdown::Block> node = markdown::flush(this->parser)) {
            this->pendingBlocks.append(std::move(node));
        }
        this->emitPendingBlocks();
    }

private:
    Stream& out;
    Array<PageHeading>& headings;
    markdown::HTMLOptions options;
    Owned<markdown::Parser> parser = markdown::createParser(markdown::ParseOptions::githubFlavored());
    Array<Owned<markdown::Block>> pendingBlocks;
    String apiClassContext;
    bool foundFirstH1 = false;

    // Emits and removes a split filename/title heading when the first h1 uses a recognized Markdown form.
    void emitSplitPageTitle() {
        if (this->foundFirstH1)
            return;

        // Find the page's first top-level h1 in the current batch.
        for (u32 blockIndex = 0; blockIndex < this->pendingBlocks.numItems(); blockIndex++) {
            auto* heading = this->pendingBlocks[blockIndex]->var.as<markdown::Block::Heading>();
            if (!heading || (heading->level != 1))
                continue;
            this->foundFirstH1 = true;

            // Recognize a code-formatted prefix followed by a colon and page title.
            if (heading->spans.numItems() >= 2) {
                auto* prefix = heading->spans[0]->var.as<markdown::Span::Code>();
                auto* title = heading->spans[1]->var.as<markdown::Span::Text>();
                if (prefix && title && title->text.startsWith(": ") &&
                    ((title->text.numBytes() > 2) || (heading->spans.numItems() > 2))) {
                    this->out.write("<h1>");
                    markdown::convertSpanToHtml(&this->out, heading->spans[0], this->options);
                    this->out.write("<br><span class=\"title-subheading\">");
                    printXmlEscapedString(this->out, title->text.substr(2));
                    for (u32 spanIndex = 2; spanIndex < heading->spans.numItems(); spanIndex++) {
                        markdown::convertSpanToHtml(&this->out, heading->spans[spanIndex], this->options);
                    }
                    this->out.write("</span></h1>\n");
                    this->pendingBlocks.erase(blockIndex);
                    return;
                }
            }

            // Recognize a page title followed by a parenthesized subheading containing any inline markup.
            u32 numSpans = heading->spans.numItems();
            if (numSpans > 0) {
                auto* subheadingEnd = heading->spans.back()->var.as<markdown::Span::Text>();
                s32 openingSpanIndex = -1;
                s32 openingTextPos = -1;

                // Find the last plain-text opening delimiter before the heading's final closing parenthesis.
                if (subheadingEnd && subheadingEnd->text.endsWith(")")) {
                    for (s32 spanIndex = numSpans - 1; spanIndex >= 0; spanIndex--) {
                        auto* text = heading->spans[spanIndex]->var.as<markdown::Span::Text>();
                        if (text && ((openingTextPos = text->text.reverseFind(" (")) >= 0)) {
                            openingSpanIndex = spanIndex;
                            break;
                        }
                    }
                }

                bool hasTitle = (openingSpanIndex > 0) || (openingTextPos > 0);
                if (hasTitle) {
                    this->out.write("<h1>");
                    for (s32 spanIndex = 0; spanIndex < openingSpanIndex; spanIndex++) {
                        markdown::convertSpanToHtml(&this->out, heading->spans[spanIndex], this->options);
                    }
                    auto* openingText = heading->spans[openingSpanIndex]->var.as<markdown::Span::Text>();
                    printXmlEscapedString(this->out, openingText->text.left(openingTextPos));
                    this->out.write("<br><span class=\"title-subheading\">(");

                    // Emit the suffix between the delimiters while preserving all of its inline markup.
                    StringView firstText = openingText->text.substr(openingTextPos + 2);
                    if (openingSpanIndex == s32(numSpans - 1)) {
                        printXmlEscapedString(this->out, firstText.shortenedBy(1));
                    } else {
                        printXmlEscapedString(this->out, firstText);
                        for (u32 spanIndex = openingSpanIndex + 1; spanIndex + 1 < numSpans; spanIndex++) {
                            markdown::convertSpanToHtml(&this->out, heading->spans[spanIndex], this->options);
                        }
                        printXmlEscapedString(this->out, subheadingEnd->text.shortenedBy(1));
                    }
                    this->out.write(")</span></h1>\n");
                    this->pendingBlocks.erase(blockIndex);
                    return;
                }
            }
            return;
        }
    }

    // Emits a contiguous run of declaration-paragraph + blockquote pairs as api_defs HTML.
    u32 emitApiDescriptionRun(u32 startIndex) {
        Array<String> declarations;
        u32 index = startIndex;
        bool firstPair = true;

        this->out.write("<dl class=\"api_defs\"><dt>");
        while (index + 1 < this->pendingBlocks.numItems()) {
            if (!parseApiDeclarationParagraph(this->pendingBlocks[index], &declarations)) {
                break;
            }
            if (!this->pendingBlocks[index + 1]->var.is<markdown::Block::BlockQuote>()) {
                break;
            }

            if (!firstPair) {
                this->out.write("</dd>\n<dt>");
            }
            printApiDeclarationsAsTitle(this->out, this->apiClassContext, declarations);

            auto* bq = this->pendingBlocks[index + 1]->var.as<markdown::Block::BlockQuote>();
            PLY_ASSERT(bq);
            this->out.write("</dt>\n<dd>");
            for (const markdown::Block* child : bq->childBlocks) {
                convertToHtml(&this->out, child, this->options);
            }
            firstPair = false;
            index += 2;
        }
        this->out.write("</dd></dl>\n");
        return index;
    }

    // Emits all pending blocks, converting matching paragraph+blockquote runs to api_defs HTML.
    void emitPendingBlocks() {
        // Pull a structured page title out before rendering the remaining Markdown tree.
        this->emitSplitPageTitle();

        // Highlight validated declaration tables before ordinary recursive Markdown rendering.
        for (markdown::Block* block : this->pendingBlocks) {
            highlightMemberTables(block, this->apiClassContext);
        }

        // Collect navigation entries before special API-description blocks consume the pending range.
        for (markdown::Block* block : this->pendingBlocks) {
            collectPageHeadings(block, this->headings);
        }

        u32 index = 0;
        while (index < this->pendingBlocks.numItems()) {
            if ((index + 1 < this->pendingBlocks.numItems()) &&
                parseApiDeclarationParagraph(this->pendingBlocks[index]) &&
                this->pendingBlocks[index + 1]->var.is<markdown::Block::BlockQuote>()) {
                index = this->emitApiDescriptionRun(index);
            } else {
                convertToHtml(&this->out, this->pendingBlocks[index], this->options);
                index++;
            }
        }
        this->pendingBlocks.clear();
    }
};

// Parses an entire documentation markdown file with custom section directives.
void parseMarkdown(Stream& out, ViewStream& in, Array<PageHeading>& headings) {
    MarkdownBlockProcessor blockProcessor{out, headings};
    while (StringView line = readLine(in)) {
        ViewStream lineIn{line};
        StringView cmd;
        if (lineIn.match("'{%i", &cmd)) {
            // Flush current markdown blocks.
            blockProcessor.flushToOutput();

            // Parse section arguments.
            Map<StringView, String> args;
            {
                StringView key;
                String value;
                while (lineIn.match(" *%i=(%i|%q)", &key, &value, &value)) {
                    *args.insert(key).value = std::move(value);
                }
            }
            PLY_ASSERT(lineIn.match(" *'}"));

            // Handle section type.
            if (cmd == "context") {
                if (const String* c = args.find("class")) {
                    blockProcessor.setApiClassContext(*c);
                } else {
                    blockProcessor.setApiClassContext({});
                }
            } else {
                PLY_ASSERT(0); // Unrecognized section type
            }
        } else {
            blockProcessor.parseMarkdownLine(line);
        }
    }
    blockProcessor.flushToOutput();
}

// Extracts page entries from one Markdown list and validates its item structure.
bool parseContentsList(Array<Owned<ContentsPage>>& pages, const markdown::Block* block) {
    const markdown::Block::List* list = block->var.as<markdown::Block::List>();
    if (!list) {
        getStdErr().write("The documentation contents must be a Markdown list.\n");
        return false;
    }

    bool isValid = true;
    for (const markdown::Block* itemBlock : list->childBlocks) {
        const markdown::Block::ListItem* item = itemBlock->var.as<markdown::Block::ListItem>();
        if (!item || item->childBlocks.isEmpty() || (item->childBlocks.numItems() > 2)) {
            getStdErr().write("Each documentation contents item must contain one link and an optional sublist.\n");
            isValid = false;
            continue;
        }

        // Require the item's first block to be a paragraph containing only one Markdown link.
        const markdown::Block::Paragraph* paragraph = item->childBlocks[0]->var.as<markdown::Block::Paragraph>();
        const markdown::Span::Link* link = nullptr;
        if (paragraph && (paragraph->spans.numItems() == 1)) {
            link = paragraph->spans[0]->var.as<markdown::Span::Link>();
        }
        if (!link || link->childSpans.isEmpty()) {
            getStdErr().write("Each documentation contents item must contain a single Markdown link.\n");
            isValid = false;
            continue;
        }

        // Retain the rendered link label and the first span's plain text for their different output contexts.
        Owned<ContentsPage> page = Heap::create<ContentsPage>();
        MemStream htmlOut;
        for (const markdown::Span* span : link->childSpans) {
            markdown::convertSpanToHtml(&htmlOut, span, {});
        }
        page->titleHtml = htmlOut.moveToString();
        MemStream documentTitleOut;
        writeHeadingText(documentTitleOut, link->childSpans[0]);
        page->documentTitle = documentTitleOut.moveToString();
        page->path = link->destination;

        // Recursively extract an optional nested list after the link paragraph.
        if (item->childBlocks.numItems() == 2) {
            if (!parseContentsList(page->children, item->childBlocks[1])) {
                isValid = false;
            }
        }
        pages.append(std::move(page));
    }
    return isValid;
}

// Flattens nested Markdown contents entries into a linear traversal order.
void flattenPages(Array<const ContentsPage*>& pages, const Array<Owned<ContentsPage>>& items) {
    for (const ContentsPage* item : items) {
        pages.append(item);
        flattenPages(pages, item->children);
    }
}

// Validates source paths before generation can modify the last successful output.
bool validatePages(ArrayView<const ContentsPage*> pages) {
    Set<String> paths;
    bool isValid = true;

    // Check the path syntax, uniqueness and corresponding Markdown source for every page.
    for (const ContentsPage* item : pages) {
        StringView docsPath = item->path;
        if (!docsPath.startsWith("/docs/") || !docsPath.endsWith(".md")) {
            getStdErr().format("Invalid documentation path: {}\n", docsPath);
            isValid = false;
            continue;
        }

        if (paths.insert(String{docsPath}).wasFound) {
            getStdErr().format("Duplicate documentation path: {}\n", docsPath);
            isValid = false;
        }

        String markdownPath = joinPath(PLYWOOD_ROOT_DIR, docsPath.substr(1));
        if (FileSystem::exists(markdownPath) != ExistsResult::File) {
            getStdErr().format("Documentation source does not exist: {}\n", docsPath);
            isValid = false;
        }
    }
    return isValid;
}

// Renders nested table-of-contents entries as HTML list markup.
void generateTableOfContentsHtml(Stream& out, const Array<Owned<ContentsPage>>& items,
                                 const Map<const ContentsPage*, Array<PageHeading>>& pageHeadings, u32 depth = 0) {
    for (const ContentsPage* item : items) {
        const Array<PageHeading>* headings = pageHeadings.find(item);
        PLY_ASSERT(headings);
        String titleHtml = String::format("<span class=\"toc-title\">{}</span>", item->titleHtml);
        String url = convertDocsPathToURL(item->path);
        out.format("<li class=\"toc-entry toc-page toc-depth-{}\">"
                   "<a class=\"toc-page-link\" href=\"{}\">{}</a>",
                   min(depth, 5u), url, titleHtml);
        if (*headings) {
            bool hasLevel2 = false;
            for (const PageHeading& heading : *headings) {
                if (heading.level == 2) {
                    hasLevel2 = true;
                    break;
                }
            }

            // Show H2 headings when present; otherwise promote the page's H3 headings to the same TOC level.
            out.write("<ul class=\"toc-sections\">");
            for (const PageHeading& heading : *headings) {
                if (hasLevel2 && (heading.level == 3))
                    continue;
                out.format("<li class=\"toc-section\"><a href=\"{}#{:&}\">{}</a></li>", url, heading.id,
                           heading.titleHtml);
            }
            out.write("</ul>");
        }
        if (item->children) {
            out.write("<ul>");
            generateTableOfContentsHtml(out, item->children, pageHeadings, depth + 1);
            out.write("</ul>");
        }
        out.write("</li>");
    }
}

// Converts one Markdown contents entry into the generated documentation page HTML.
void convertPage(const ContentsPage& item, const ContentsPage* prevPage, const ContentsPage* nextPage,
                 Array<PageHeading>& headings) {
    // Resolve the repo-root-relative source path while preserving its old generated-file location.
    StringView docsPath = item.path;
    PLY_ASSERT(docsPath.startsWith("/docs/") && docsPath.endsWith(".md"));
    String markdownPath = joinPath(PLYWOOD_ROOT_DIR, docsPath.substr(1));
    String relName = docsPath.substr(6).shortenedBy(3);
    String markdown = FileSystem::loadTextAutodetect(markdownPath);
    ViewStream in{markdown};
    MemStream mem;
    parseMarkdown(mem, in, headings);
    String articleContent = mem.moveToString();

    // Generate prev/next navigation
    String prevLink, nextLink;
    if (prevPage) {
        String url = convertDocsPathToURL(prevPage->path);
        prevLink =
            String::format("<a class=\"nav-card nav-prev\" href=\"{}\"><span class=\"nav-meta\">Previous</span>"
                           "<span class=\"nav-title\">{}</span></a>",
                           url, prevPage->titleHtml);
    }
    if (nextPage) {
        String url = convertDocsPathToURL(nextPage->path);
        nextLink =
            String::format("<a class=\"nav-card nav-next\" href=\"{}\"><span class=\"nav-meta\">Next</span>"
                           "<span class=\"nav-title\">{}</span></a>",
                           url, nextPage->titleHtml);
    }
    String navHtml = String::format("<div class=\"page-nav\">{}{}</div>", prevLink, nextLink);

    // Write content-only file for AJAX loading
    String ajaxContent =
        String::format("{} - Plywood C++ Runtime Library\n{}{}", item.documentTitle, articleContent, navHtml);
    String ajaxPath = joinPath(outFolder, "content/docs", relName + ".html");
    writeFileIfChanged(ajaxPath, ajaxContent);
}

// Regenerates the complete documentation site into docs/build if all source pages are valid.
bool generateWholeSite() {
    // Parse and validate the page list before modifying any generated output.
    String contentsMarkdown = FileSystem::loadTextAutodetect(joinPath(docsFolder, "table-of-contents.md"));
    Array<Owned<markdown::Block>> contentsBlocks = markdown::parse(contentsMarkdown);
    contents.clear();
    if (contentsBlocks.numItems() != 1) {
        getStdErr().write("The documentation contents must contain exactly one Markdown list.\n");
        return false;
    }
    if (!parseContentsList(contents, contentsBlocks[0]))
        return false;
    Array<const ContentsPage*> pages;
    flattenPages(pages, contents);
    if (!validatePages(pages))
        return false;

    // Reset per-pass state after validation succeeds.
    stats = {};
    publishKey = Random{}.generateU32(); // Prevent browsers from caching old stylesheets

    // Copy front page to content/index.html.
    String frontPage = FileSystem::loadText(joinPath(sourceFolder, "index.html"));
    appendPublishKeyToAsset(frontPage, "/static/common.css");
    appendPublishKeyToAsset(frontPage, "/static/front.css");
    appendPublishKeyToAsset(frontPage, "/static/common.js");
    writeFileIfChanged(joinPath(outFolder, "content/index.html"), frontPage);

    // Copy static files to static/.
    for (const DirectoryEntry& entry : FileSystem::listDir(joinPath(sourceFolder, "static"))) {
        if (entry.isFile()) {
            String srcPath = joinPath(sourceFolder, "static", entry.name);
            String dstPath = joinPath(outFolder, "static", entry.name);
            if (entry.name.endsWith(".css") || entry.name.endsWith(".js") || entry.name.endsWith(".html")) {
                String text = FileSystem::loadTextAutodetect(srcPath);
                writeFileIfChanged(dstPath, text);
            } else {
                writeFileIfChanged(dstPath, FileSystem::loadBinary(srcPath));
            }
        }
    }

    // Copy docs template to content/.
    String templateText = FileSystem::loadTextAutodetect(joinPath(sourceFolder, "docs-template.html"));
    appendPublishKeyToAsset(templateText, "/static/common.css");
    appendPublishKeyToAsset(templateText, "/static/docs.css");
    appendPublishKeyToAsset(templateText, "/static/common.js");
    appendPublishKeyToAsset(templateText, "/static/doc-viewer.js");
    writeFileIfChanged(joinPath(outFolder, "content/docs-template.html"), templateText);

    // Generate pages while collecting their level 2-3 headings for the sidebar.
    Map<const ContentsPage*, Array<PageHeading>> pageHeadings;
    for (u32 i = 0; i < pages.numItems(); i++) {
        const ContentsPage* prevPage = (i > 0) ? pages[i - 1] : nullptr;
        const ContentsPage* nextPage = (i + 1 < pages.numItems()) ? pages[i + 1] : nullptr;
        auto insertResult = pageHeadings.insert(pages[i]);
        PLY_ASSERT(!insertResult.wasFound);
        convertPage(*pages[i], prevPage, nextPage, *insertResult.value);
    }

    // Generate the table of contents after all per-page heading metadata is available.
    MemStream tocStream;
    generateTableOfContentsHtml(tocStream, contents, pageHeadings);
    writeFileIfChanged(joinPath(outFolder, "content/toc.html"), tocStream.moveToString());

    // Delete files left behind by pages or assets that are no longer generated.
    Array<String> existingDirs;
    if (FileSystem::isDir(outFolder)) {
        for (const WalkTriple& triple : FileSystem::walk(outFolder)) {
            existingDirs.append(triple.dirPath);
            for (const DirectoryEntry& entry : triple.files) {
                String path = joinPath(triple.dirPath, entry.name);
                if (!stats.generatedPaths.find(path)) {
                    FSResult result = FileSystem::deleteFile(path);
                    PLY_ASSERT(result == FSResult::OK);
                    PLY_UNUSED(result);
                    stats.numOrphanedFilesRemoved++;
                }
            }
        }
    }

    // Delete directories left empty by the generation pass, starting with the deepest directories.
    for (u32 i = existingDirs.numItems(); i-- > 1;) {
        if (FileSystem::listDir(existingDirs[i]).isEmpty()) {
            FSResult result = FileSystem::removeDirTree(existingDirs[i]);
            PLY_ASSERT(result == FSResult::OK);
            PLY_UNUSED(result);
            stats.numOrphanedDirsRemoved++;
        }
    }

    // Report the result of this generation pass.
    Stream out = getStdOut();
    out.write("Generation complete");
    char separator = ':';
    if (stats.numUpdated > 0) {
        out.format("{} {} file{} updated", separator, stats.numUpdated, stats.numUpdated == 1 ? "" : "s");
        separator = ',';
    }
    if (stats.numUnchanged > 0) {
        out.format("{} {} file{} unchanged", separator, stats.numUnchanged, stats.numUnchanged == 1 ? "" : "s");
        separator = ',';
    }
    if (stats.numOrphanedFilesRemoved > 0) {
        out.format("{} {} orphaned file{} removed", separator, stats.numOrphanedFilesRemoved,
                   stats.numOrphanedFilesRemoved == 1 ? "" : "s");
        separator = ',';
    }
    if (stats.numOrphanedDirsRemoved > 0) {
        out.format("{} {} orphaned director{} removed", separator, stats.numOrphanedDirsRemoved,
                   stats.numOrphanedDirsRemoved == 1 ? "y" : "ies");
    }
    out.write(".\n");
    return true;
}

// Prints the command-line syntax and registered options.
static void printUsage(Stream& out, StringView executablePath, const CommandLineParser& parser) {
    out.format("Usage: {} [options]\n", executablePath);
    parser.printAvailableOptions(out);
}

// Entry point that runs a full generation pass and optional file system watch loop.
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Parse command-line options before generating any files.
    CommandLineOptions options;
    CommandLineParser parser({
        {"-w", "--watch", PLY_LOOKUP_MEMBER(CommandLineOptions, watch), "Watch for changes and regenerate"},
        {"-h", "--help", PLY_LOOKUP_MEMBER(CommandLineOptions, printUsage), "Print this help"},
    });
    if (!parser.apply(argc, argv, &options)) {
        Stream err = getStdErr();
        err.write("\n");
        printUsage(err, argv[0], parser);
        return 1;
    }
    if (options.printUsage) {
        Stream out = getStdOut();
        printUsage(out, argv[0], parser);
        return 0;
    }

    if (!generateWholeSite()) {
        return 1;
    }

    if (options.watch) {
#if PLY_WITH_DIRECTORY_WATCHER
        getStdOut().write("Watching for changes...\n");

        Mutex mutex;
        ConditionVariable cond;
        Atomic<u32> changed = 0;

        auto onChange = [&](StringView path, bool mustRecurse) {
            if (splitPathFull(path)[0] != "build") {
                LockGuard<Mutex> lock{mutex};
                changed.store(1, MemoryOrder::Release);
                cond.wakeOne();
            }
        };

        DirectoryWatcher sourceWatcher{sourceFolder, onChange};
        DirectoryWatcher docsWatcher{docsFolder, onChange};

        for (;;) {
            {
                LockGuard<Mutex> lock{mutex};
                while (!changed.load(MemoryOrder::Acquire)) {
                    cond.wait(lock);
                }
            }

            getStdOut().write("Change detected, regenerating...\n");
            sleepMillis(100);
            changed.store(0, MemoryOrder::Release);
            generateWholeSite();
        }
#else
        getStdOut().write("--watch is not supported on this platform.\n");
#endif
    }

    return 0;
}

PLY_STRUCT_BEGIN(CommandLineOptions)
PLY_STRUCT_MEMBER(watch)
PLY_STRUCT_MEMBER(printUsage)
PLY_STRUCT_END()
