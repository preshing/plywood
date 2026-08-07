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

// Renders a declaration as a highlighted two-column API summary table row.
void printDeclAsHtml(Stream& out, const Parser* parser, const Declaration& decl) {
    Array<TokenSpan> spans = parser->syntaxHighlight(decl);
    StringView mainRowHeader = "<tr class=\"entry\"><td class=\"prefix\"><code>";

    // Find first declarator.
    const Declaration* mainDeclaration = &decl;
    Token firstMainToken;
    if (auto* tmpl = mainDeclaration->var.as<Declaration::Template>()) {
        mainDeclaration = tmpl->childDecl;
        firstMainToken = mainDeclaration->getFirstToken();
        out.write("<tr><td colspan=\"2\" class=\"template\"><code>");
    } else {
        out.write(mainRowHeader);
    }

    const cpp::QualifiedID* firstDeclaratorQid = nullptr;
    if (auto* entity = mainDeclaration->var.as<Declaration::Entity>()) {
        if (!entity->initDeclarators.isEmpty()) {
            if (!entity->initDeclarators[0].qid.isEmpty()) {
                firstDeclaratorQid = &entity->initDeclarators[0].qid;
            }
        }
    }

    // Output token spans.
    TokenSpan::Color lastColor = TokenSpan::None;
    bool gotFirstDeclaratorQid = false;
    for (const TokenSpan& span : spans) {
        if (firstMainToken.isValid() && (span.token == firstMainToken)) {
            out.write("</code></td></tr>\n");
            out.write(mainRowHeader);
        }
        if (!gotFirstDeclaratorQid && firstDeclaratorQid && (firstDeclaratorQid == span.qid)) {
            if (lastColor != TokenSpan::None) {
                out.write("</span>");
                lastColor = TokenSpan::None;
            }
            out.write("</code></td><td class=\"suffix\"><code>");
            gotFirstDeclaratorQid = true;
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
            out.write(gotFirstDeclaratorQid ? " " : "&nbsp;");
        } else {
            printXmlEscapedString(out, span.token.text);
        }
    }
    if (lastColor != TokenSpan::None) {
        out.write("</span>");
    }
    out.write("</code></td></tr>\n");
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

// Converts Markdown to HTML while adapting documentation links for the generated site.
String convertDocsMarkdownToHtml(StringView source) {
    Array<Owned<markdown::Block>> blocks =
        markdown::parseWholeDocument(source, markdown::ParseOptions::githubFlavored());
    markdown::HTMLOptions options;
    options.filterLinks = convertDocsPathToURL;
    MemStream out;

    // Render every top-level block in document order.
    for (markdown::Block* block : blocks) {
        markdown::convertToHtml(&out, block, options);
    }
    return out.moveToString();
}

// Parses an {apiSummary} section and emits the corresponding HTML table.
void parseApiSummary(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    // Write optional caption.
    if (const String* caption = args.find("caption")) {
        String html = convertDocsMarkdownToHtml(*caption);
        out.format("<div class=\"caption\">{}</div>\n", html.substr(3, html.numBytes() - 8));
    }

    // Get class name.
    StringView className;
    if (const String* c = args.find("class")) {
        className = *c;
    }

    Array<String> lines;
    u32 numHeadings = 0;
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/apiSummary}")
            break;
        if (s.startsWith("--") && s.substr(2).trim()) {
            numHeadings++;
        }
        lines.append(line);
    }

    if (numHeadings <= 1) {
        out.write("<table class=\"api single-group\">\n");
    } else {
        out.write("<table class=\"api\">\n");
    }

    for (const String& line : lines) {
        StringView s = line.trim();
        if (s.startsWith("--")) {
            StringView caption = s.substr(2).trim();
            if (caption) {
                out.format("<tr class=\"heading\"><td colspan=\"2\" class=\"heading\">{:&}</td></tr>\n", caption);
            }
            continue;
        }
        Owned<Parser> parser = Parser::create();
        Declaration decl = parser->parseDeclaration(s, className);
        printDeclAsHtml(out, parser, decl);
    }
    out.write("</table>\n");
}

// Parses a {table} section and emits a simple two-dimensional HTML table.
void parseTable(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    out.write("<table class=\"grid\">\n");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/table}")
            break;
        out.write("<tr>");
        for (StringView column : s.split("|")) {
            String html = convertDocsMarkdownToHtml(column);
            out.format("<td>{}</td>", html.substr(3, html.numBytes() - 8));
        }
        out.write("</tr>\n");
    }
    out.write("</table>\n");
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

// Buffers parsed markdown blocks and emits special API description structures when detected.
class MarkdownBlockProcessor {
public:
    // Creates a block processor that writes converted output to the supplied stream.
    MarkdownBlockProcessor(Stream& out) : out{out} {
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
void parseMarkdown(Stream& out, ViewStream& in) {
    MarkdownBlockProcessor blockProcessor{out};
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
            if (cmd == "apiSummary") {
                parseApiSummary(out, args, in);
            } else if (cmd == "context") {
                if (const String* c = args.find("class")) {
                    blockProcessor.setApiClassContext(*c);
                } else {
                    blockProcessor.setApiClassContext({});
                }
            } else if (cmd == "table") {
                parseTable(out, args, in);
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
void generateTableOfContentsHtml(Stream& out, const json::Node& items) {
    for (const json::Node& item : items.arrayView()) {
        const json::Node& children = item.get("children");
        StringView spanClass;
        if (children.isValid()) {
            spanClass = " class=\"caret caret-down\"";
        }
        String headerFile;
        if (item.get("header-file").isValid()) {
            headerFile =
                String::format(" <span class=\"toc-header\">&lt;{:&}&gt;</span>", item.get("header-file").text());
        }
        String url = convertDocsPathToURL(item.get("path").text());
        out.format("<a href=\"{}\"><li class=\"selectable\"><span{}>{:&}</span>{}</li></a>", url, spanClass,
                   item.get("title").text(), headerFile);
        if (children.isValid()) {
            out.write("<ul class=\"nested active\">");
            generateTableOfContentsHtml(out, children);
            out.write("</ul>");
        }
    }
}

// Converts one contents.json entry into the generated documentation page HTML.
void convertPage(const json::Node& item, const json::Node* prevPage, const json::Node* nextPage) {
    // Resolve the repo-root-relative source path while preserving its old generated-file location.
    StringView docsPath = item.get("path").text();
    PLY_ASSERT(docsPath.startsWith("/docs/") && docsPath.endsWith(".md"));
    String markdownPath = joinPath(PLYWOOD_ROOT_DIR, docsPath.substr(1));
    String relName = docsPath.substr(6).shortenedBy(3);
    String markdown = Filesystem::loadTextAutodetect(markdownPath);
    ViewStream in{markdown};
    MemStream mem;
    parseMarkdown(mem, in);
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

    // Parse contents.json and generate table of contents HTML.
    contents = parseJson(joinPath(docsFolder, "contents.json"));
    MemStream tocStream;
    generateTableOfContentsHtml(tocStream, contents);
    writeFileIfChanged(joinPath(outFolder, "content/toc.html"), tocStream.moveToString());

    // Traverse contents.json and generate pages in content/docs/.
    Array<const json::Node*> pages;
    flattenPages(pages, contents);
    for (u32 i = 0; i < pages.numItems(); i++) {
        const json::Node* prevPage = (i > 0) ? pages[i - 1] : nullptr;
        const json::Node* nextPage = (i + 1 < pages.numItems()) ? pages[i + 1] : nullptr;
        convertPage(*pages[i], prevPage, nextPage);
    }

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
