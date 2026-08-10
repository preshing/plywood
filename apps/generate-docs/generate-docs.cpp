/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-json.h>
#include <ply-markdown.h>
#include <ply-cpp.h>

using namespace ply;
using namespace ply::cpp;

String sourceFolder = joinPath(PLYWOOD_ROOT_DIR, "apps/generate-docs/data");
String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs");
String outFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");
json::Node contents;
u32 publishKey = 0; // Prevent browsers from caching old stylesheets

// Describes one article heading included in its expanded sidebar entry.
struct PageHeading {
    u32 level = 0;
    String id;
    String titleHtml;
};

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
    if ((Filesystem::exists(path) == ER_FILE) && (Filesystem::loadBinary(path) == contents)) {
        stats.numUnchanged++;
        return;
    }

    // Create the parent directory and write the new contents.
    Filesystem::makeDirs(splitPath(path).directory);
    FSResult result = Filesystem::saveBinary(path, contents);
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

// Parses an {example} section and emits it as a captioned code block.
void parseExample(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Example</div>\n");
    out.write("<pre><code>");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/example}")
            break;
        printXmlEscapedString(out, line);
    }
    out.write("</code></pre>\n");
}

// Parses an {output} section and emits it as a captioned code block.
void parseOutput(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Output</div>\n");
    out.write("<pre><code>");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/output}")
            break;
        printXmlEscapedString(out, line);
    }
    out.write("</code></pre>\n");
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
            } else if (cmd == "example") {
                parseExample(out, in);
            } else if (cmd == "output") {
                parseOutput(out, in);
            } else {
                PLY_ASSERT(0); // Unrecognized section type
            }
        } else {
            blockProcessor.parseMarkdownLine(line);
        }
    }
    blockProcessor.flushToOutput();
}

// Flattens nested contents.json page entries into a linear traversal order.
void flattenPages(Array<const json::Node*>& pages, const json::Node& items) {
    for (const json::Node& item : items.arrayView()) {
        pages.append(&item);
        if (item.get("children").isValid()) {
            flattenPages(pages, item.get("children"));
        }
    }
}

// Renders nested table-of-contents entries as HTML list markup.
void generateTableOfContentsHtml(Stream& out, const json::Node& items,
                                 const Map<const json::Node*, Array<PageHeading>>& pageHeadings, u32 depth = 0) {
    for (const json::Node& item : items.arrayView()) {
        const json::Node& children = item.get("children");
        const Array<PageHeading>* headings = pageHeadings.find(&item);
        PLY_ASSERT(headings);
        String headerFile;
        if (item.get("header-file").isValid()) {
            headerFile =
                String::format(" <span class=\"toc-header\">&lt;{:&}&gt;</span>", item.get("header-file").text());
        }
        String url = convertDocsPathToURL(item.get("path").text());
        out.format("<li class=\"toc-entry toc-depth-{}\"><div class=\"toc-page\">"
                   "<a class=\"toc-page-link\" href=\"{}\"><span>{:&}</span>{}</a>",
                   min(depth, 5u), url, item.get("title").text(), headerFile);
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
        out.write("</div>");
        if (children.isValid()) {
            out.write("<ul class=\"toc-children\">");
            generateTableOfContentsHtml(out, children, pageHeadings, depth + 1);
            out.write("</ul>");
        }
        out.write("</li>");
    }
}

// Converts one contents.json entry into the generated documentation page HTML.
void convertPage(const json::Node& item, const json::Node* prevPage, const json::Node* nextPage,
                 Array<PageHeading>& headings) {
    // Resolve the repo-root-relative source path while preserving its old generated-file location.
    StringView docsPath = item.get("path").text();
    PLY_ASSERT(docsPath.startsWith("/docs/") && docsPath.endsWith(".md"));
    String markdownPath = joinPath(PLYWOOD_ROOT_DIR, docsPath.substr(1));
    String relName = docsPath.substr(6).shortenedBy(3);
    String markdown = Filesystem::loadTextAutodetect(markdownPath);
    ViewStream in{markdown};
    MemStream mem;
    parseMarkdown(mem, in, headings);
    String articleContent = mem.moveToString();
    String pageTitle = item.get("title").text();

    // Generate prev/next navigation
    String prevLink, nextLink;
    if (prevPage) {
        String url = convertDocsPathToURL(prevPage->get("path").text());
        prevLink =
            String::format("<a class=\"nav-card nav-prev\" href=\"{}\"><span class=\"nav-meta\">Previous</span>"
                           "<span class=\"nav-title\">{:&}</span></a>",
                           url, prevPage->get("title").text());
    }
    if (nextPage) {
        String url = convertDocsPathToURL(nextPage->get("path").text());
        nextLink =
            String::format("<a class=\"nav-card nav-next\" href=\"{}\"><span class=\"nav-meta\">Next</span>"
                           "<span class=\"nav-title\">{:&}</span></a>",
                           url, nextPage->get("title").text());
    }
    String navHtml = String::format("<div class=\"page-nav\">{}{}</div>", prevLink, nextLink);

    // Write content-only file for AJAX loading
    String ajaxContent = String::format("{} :: Plywood C++ Runtime Library\n{}{}", pageTitle, articleContent, navHtml);
    String ajaxPath = joinPath(outFolder, "content/docs", relName + ".html");
    writeFileIfChanged(ajaxPath, ajaxContent);
}

// Loads and parses a JSON file from disk.
json::Node parseJson(StringView path) {
    String src = Filesystem::loadTextAutodetect(path);
    return json::Parser{}.parse(path, src).root;
}

// Regenerates the complete documentation site into docs/build.
void generateWholeSite() {
    publishKey = Random{}.generateU32(); // Prevent browsers from caching old stylesheets

    // Copy front page to content/index.html.
    String frontPage = Filesystem::loadText(joinPath(sourceFolder, "index.html"));
    appendPublishKeyToAsset(frontPage, "/static/common.css");
    appendPublishKeyToAsset(frontPage, "/static/front.css");
    appendPublishKeyToAsset(frontPage, "/static/common.js");
    writeFileIfChanged(joinPath(outFolder, "content/index.html"), frontPage);

    // Copy static files to static/.
    for (const DirectoryEntry& entry : Filesystem::listDir(joinPath(sourceFolder, "static"))) {
        if (entry.isFile()) {
            String srcPath = joinPath(sourceFolder, "static", entry.name);
            String dstPath = joinPath(outFolder, "static", entry.name);
            if (entry.name.endsWith(".css") || entry.name.endsWith(".js") || entry.name.endsWith(".html")) {
                String text = Filesystem::loadTextAutodetect(srcPath);
                writeFileIfChanged(dstPath, text);
            } else {
                writeFileIfChanged(dstPath, Filesystem::loadBinary(srcPath));
            }
        }
    }

    // Copy docs template to content/.
    String templateText = Filesystem::loadTextAutodetect(joinPath(sourceFolder, "docs-template.html"));
    appendPublishKeyToAsset(templateText, "/static/common.css");
    appendPublishKeyToAsset(templateText, "/static/docs.css");
    appendPublishKeyToAsset(templateText, "/static/common.js");
    appendPublishKeyToAsset(templateText, "/static/doc-viewer.js");
    writeFileIfChanged(joinPath(outFolder, "content/docs-template.html"), templateText);

    // Parse contents.json and flatten its pages into navigation order.
    contents = parseJson(joinPath(docsFolder, "contents.json"));
    Array<const json::Node*> pages;
    flattenPages(pages, contents);

    // Generate pages while collecting their level 2-3 headings for the sidebar.
    Map<const json::Node*, Array<PageHeading>> pageHeadings;
    for (u32 i = 0; i < pages.numItems(); i++) {
        const json::Node* prevPage = (i > 0) ? pages[i - 1] : nullptr;
        const json::Node* nextPage = (i + 1 < pages.numItems()) ? pages[i + 1] : nullptr;
        auto insertResult = pageHeadings.insert(pages[i]);
        PLY_ASSERT(!insertResult.wasFound);
        convertPage(*pages[i], prevPage, nextPage, *insertResult.value);
    }

    // Generate the table of contents after all per-page heading metadata is available.
    MemStream tocStream;
    generateTableOfContentsHtml(tocStream, contents, pageHeadings);
    writeFileIfChanged(joinPath(outFolder, "content/toc.html"), tocStream.moveToString());

    // Delete files left behind by pages or assets that are no longer generated.
    if (Filesystem::isDir(outFolder)) {
        for (const WalkTriple& triple : Filesystem::walk(outFolder)) {
            for (const DirectoryEntry& entry : triple.files) {
                String path = joinPath(triple.dirPath, entry.name);
                if (!stats.generatedPaths.find(path)) {
                    FSResult result = Filesystem::deleteFile(path);
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
}

// Entry point that runs a full generation pass and optional filesystem watch loop.
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

    generateWholeSite();

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
