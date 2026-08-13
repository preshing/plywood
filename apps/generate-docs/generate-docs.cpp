/*───────────────────────────────────────────────────────────────────┐
│                                                                    │
│     ____      Plywood C++ Runtime Library                          │
│    ╱   ╱╲     https://plywood.dev/                                 │
│   ╱___╱╭╮╲                                                         │
│    └──┴┴┴┘    generate-docs                                        │
│               Documentation: /docs/apps/generate-docs.md           │
│                                                                    │
└───────────────────────────────────────────────────────────────────*/

#include <ply-markdown.h>
#include <ply-cpp.h>

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
    u32 numOrphansRemoved = 0;
};
GenerationStats stats;

// Writes one generated file only when its contents changed and records its path as expected output.
void writeFileIfChanged(StringView path, StringView contents) {
    stats.generatedPaths.insert(path);

    // Retain an existing file when its bytes already match the generated contents.
    if ((FileSystem::exists(path) == ER_FILE) && (FileSystem::loadBinary(path) == contents)) {
        stats.numUnchanged++;
        return;
    }

    // Create the parent directory and write the new contents.
    FileSystem::makeDirs(splitPath(path).directory);
    FSResult result = FileSystem::saveBinary(path, contents);
    PLY_ASSERT(result == FS_OK);
    PLY_UNUSED(result);
    stats.numUpdated++;
}

// Appends the current publish key query parameter to an asset path in HTML text.
void appendPublishKeyToAsset(String& text, StringView assetPath) {
    text = text.replace(assetPath, String::format("{}?key={}", assetPath, publishKey));
}

// Renders a declaration as a single highlighted code fragment for API description titles.
void printDeclAsApiTitle(Stream& out, const Parser* parser, const Declaration& decl) {
    Array<TokenSpan> spans = parser->syntaxHighlight(decl);
    out.write("<code>");

    // Output token spans.
    TokenSpan::Color lastColor = TokenSpan::None;
    bool gotFirstDeclaratorQid = false;
    for (const TokenSpan& span : spans) {
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
            out.write(gotFirstDeclaratorQid ? " " : "&nbsp;");
        } else {
            printXmlEscapedString(out, span.token.text);
        }
    }
    if (lastColor != TokenSpan::None) {
        out.write("</span>");
    }
    out.write("</code>");
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
        if (FileSystem::exists(markdownPath) != ER_FILE) {
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
    Array<Owned<markdown::Block>> contentsBlocks = markdown::parseWholeDocument(contentsMarkdown);
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
    if (FileSystem::isDir(outFolder)) {
        for (const WalkTriple& triple : FileSystem::walk(outFolder)) {
            for (const DirectoryEntry& entry : triple.files) {
                String path = joinPath(triple.dirPath, entry.name);
                if (!stats.generatedPaths.find(path)) {
                    FSResult result = FileSystem::deleteFile(path);
                    PLY_ASSERT(result == FS_OK);
                    PLY_UNUSED(result);
                    stats.numOrphansRemoved++;
                }
            }
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
    if (stats.numOrphansRemoved > 0) {
        out.format("{} {} orphaned file{} removed", separator, stats.numOrphansRemoved,
                   stats.numOrphansRemoved == 1 ? "" : "s");
    }
    out.write(".\n");
    return true;
}

// Entry point that runs a full generation pass and optional file system watch loop.
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Check for -watch argument
    bool watchMode = false;
    for (int i = 1; i < argc; i++) {
        if (StringView{argv[i]} == "-watch") {
            watchMode = true;
            break;
        }
    }

    if (!generateWholeSite())
        return 1;

    if (watchMode) {
#if PLY_WITH_DIRECTORY_WATCHER
        getStdOut().write("Watching for changes...\n");

        Mutex mutex;
        ConditionVariable cond;
        Atomic<u32> changed = 0;

        auto onChange = [&](StringView path, bool mustRecurse) {
            if (splitPathFull(path)[0] != "build") {
                LockGuard<Mutex> lock{mutex};
                changed.store(1, Release);
                cond.wakeOne();
            }
        };

        DirectoryWatcher sourceWatcher{sourceFolder, onChange};
        DirectoryWatcher docsWatcher{docsFolder, onChange};

        for (;;) {
            {
                LockGuard<Mutex> lock{mutex};
                while (!changed.load(Acquire)) {
                    cond.wait(lock);
                }
            }

            getStdOut().write("Change detected, regenerating...\n");
            sleepMillis(100);
            changed.store(0, Release);
            generateWholeSite();
        }
#else
        getStdOut().write("-watch is not supported on this platform.");
#endif
    }

    return 0;
}
