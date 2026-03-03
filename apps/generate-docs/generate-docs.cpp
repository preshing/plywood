/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
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
TextFormat serverTextFormat = {UTF8, TextFormat::LF, false};
json::Node contents;
u32 publishKey = 0; // Prevent browsers from caching old stylesheets

void appendPublishKeyToAsset(String& text, StringView assetPath) {
    text = text.replace(assetPath, String::format("{}?key={}", assetPath, publishKey));
}

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

void parseApiSummary(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    // Write optional caption.
    if (const String* caption = args.find("caption")) {
        String html = markdown::convertToHtml(*caption);
        out.format("<div class=\"caption\">{}</div>\n", html.substr(3, html.numBytes() - 8));
    }

    // Get class name.
    StringView className;
    if (const String* c = args.find("class")) {
        className = *c;
    }

    out.write("<table class=\"api\">\n");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s.startsWith("--")) {
            StringView caption = s.substr(2).trim();
            if (caption) {
                out.format("<tr class=\"heading\"><td colspan=\"2\" class=\"heading\">{&}</td></tr>\n", caption);
            }
            continue;
        }
        if (s == "{/apiSummary}")
            break;
        Owned<Parser> parser = Parser::create();
        Declaration decl = parser->parseDeclaration(s, className);
        printDeclAsHtml(out, parser, decl);
    }
    out.write("</table>\n");
}

void parseApiDescriptions(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    // Get class name.
    StringView className;
    if (const String* c = args.find("class")) {
        className = *c;
    }

    markdown::HTML_Options options;
    Owned<markdown::Parser> md = markdown::createParser();
    out.write("<dl class=\"api_defs\"><dt>");
    bool inTitle = true;
    bool firstDecl = true;
    while (StringView line = readLine(in)) {
        if (line.trim() == "{/apiDescriptions}")
            break;
        if (inTitle) {
            if (line.trim().isEmpty())
                continue;
            if (line.startsWith("--")) {
                out.write("</dt>\n<dd>");
                inTitle = false;
            } else {
                Owned<Parser> parser = Parser::create();
                Declaration decl = parser->parseDeclaration(line.trim(), className);
                if (!firstDecl) {
                    out.write("<br>\n");
                }
                printDeclAsApiTitle(out, parser, decl);
                firstDecl = false;
            }
        } else {
            if (line.startsWith(">>")) {
                // Flush current markdown block.
                if (Owned<markdown::Block> node = flush(md)) {
                    convertToHtml(&out, node, options);
                }
                out.write("</dd>\n<dt>");
                inTitle = true;
                firstDecl = true;
            } else {
                if (Owned<markdown::Block> node = parseLine(md, line)) {
                    convertToHtml(&out, node, options);
                }
            }
        }
    }
    if (inTitle) {
        out.write("</dt></dl>\n");
    } else {
        // Flush current markdown block.
        if (Owned<markdown::Block> node = flush(md)) {
            convertToHtml(&out, node, options);
        }
        out.write("</dd></dl>\n");
    }
}

void parseTable(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    out.write("<table class=\"grid\">\n");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/table}")
            break;
        out.write("<tr>");
        for (StringView column : s.split("|")) {
            String html = markdown::convertToHtml(column);
            out.format("<td>{}</td>", html.substr(3, html.numBytes() - 8));
        }
        out.write("</tr>\n");
    }
    out.write("</table>\n");
}

void parseExample(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Example</div>\n");
    out.write("<pre>\n");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/example}")
            break;
        printXmlEscapedString(out, line);
    }
    out.write("</pre>\n");
}

void parseOutput(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Output</div>\n");
    out.write("<pre>\n");
    while (StringView line = readLine(in)) {
        StringView s = line.trim();
        if (s == "{/output}")
            break;
        printXmlEscapedString(out, line);
    }
    out.write("</pre>\n");
}

void parseMarkdown(Stream& out, ViewStream& in) {
    markdown::HTML_Options options;
    Owned<markdown::Parser> parser = markdown::createParser();
    while (StringView line = readLine(in)) {
        ViewStream lineIn{line};
        StringView cmd;
        if (lineIn.match("'{%i", &cmd)) {
            // Flush current markdown block.
            if (Owned<markdown::Block> node = flush(parser)) {
                convertToHtml(&out, node, options);
            }

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
            } else if (cmd == "apiDescriptions") {
                parseApiDescriptions(out, args, in);
            } else if (cmd == "table") {
                parseTable(out, args, in);
            } else if (cmd == "example") {
                parseExample(out, in);
            } else if (cmd == "output") {
                if (Owned<markdown::Block> node = flush(parser)) {
                    convertToHtml(&out, node, options);
                }
                parseOutput(out, in);
            } else if (cmd == "title") {
                out.format("<h1><span class=\"right\"><span class=\"meta-label\">Header file:</span><span "
                           "class=\"meta-value\">&lt;{&}&gt;</span><span class=\"meta-label\">Namespace:</span><span "
                           "class=\"meta-value\">{&}</span></span>{&}</h1>\n",
                           *args.find("include"), *args.find("namespace"), *args.find("text"));
            } else {
                PLY_ASSERT(0); // Unrecognized section type
            }
        } else {
            if (Owned<markdown::Block> node = parseLine(parser, line)) {
                convertToHtml(&out, node, options);
            }
        }
    }
    if (Owned<markdown::Block> node = flush(parser)) {
        convertToHtml(&out, node, options);
    }
}

void flattenPages(Array<const json::Node*>& pages, const json::Node& items) {
    for (const json::Node& item : items.arrayView()) {
        pages.append(&item);
        if (item.get("children").isValid()) {
            flattenPages(pages, item.get("children"));
        }
    }
}

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
                String::format(" <span class=\"toc-header\">&lt;{&}&gt;</span>", item.get("header-file").text());
        }
        out.format("<a href=\"/docs/{}\"><li class=\"selectable\"><span{}>{&}</span>{}</li></a>",
                   item.get("path").text(), spanClass, item.get("title").text(), headerFile);
        if (children.isValid()) {
            out.write("<ul class=\"nested active\">");
            generateTableOfContentsHtml(out, children);
            out.write("</ul>");
        }
    }
}

void convertPage(const json::Node& item, const json::Node* prevPage, const json::Node* nextPage) {
    String relName = item.get("path").text();
    String markdownPath = joinPath(docsFolder, relName);
    if (Filesystem::isDir(markdownPath)) {
        relName = joinPath(relName, "index");
        markdownPath = joinPath(markdownPath, "index.md");
    } else {
        markdownPath += ".md";
    }
    String markdown = Filesystem::loadTextAutodetect(markdownPath);
    ViewStream in{markdown};
    MemStream mem;
    parseMarkdown(mem, in);
    String articleContent = mem.moveToString();
    String pageTitle = item.get("title").text();

    // Generate prev/next navigation
    String prevLink, nextLink;
    if (prevPage) {
        prevLink = String::format(
            "<a class=\"nav-card nav-prev\" href=\"/docs/{}\"><span class=\"nav-meta\">Previous</span>"
            "<span class=\"nav-title\">{&}</span></a>",
            prevPage->get("path").text(), prevPage->get("title").text());
    }
    if (nextPage) {
        nextLink = String::format(
            "<a class=\"nav-card nav-next\" href=\"/docs/{}\"><span class=\"nav-meta\">Next</span>"
            "<span class=\"nav-title\">{&}</span></a>",
            nextPage->get("path").text(), nextPage->get("title").text());
    }
    String navHtml = String::format("<div class=\"page-nav\">{}{}</div>", prevLink, nextLink);

    // Write content-only file for AJAX loading
    String ajaxContent = String::format("{} :: Plywood C++ Base Library\n{}{}", pageTitle, articleContent, navHtml);
    String ajaxPath = joinPath(outFolder, "content/docs", relName + ".html");
    Filesystem::makeDirs(splitPath(ajaxPath).directory);
    Filesystem::saveText(ajaxPath, ajaxContent, serverTextFormat);
}

json::Node parseJson(StringView path) {
    String src = Filesystem::loadTextAutodetect(path);
    return json::Parser{}.parse(path, src).root;
}

void generateWholeSite() {
    publishKey = Random{}.generateU32(); // Prevent browsers from caching old stylesheets

    Filesystem::makeDirs(joinPath(outFolder, "content"));
    Filesystem::makeDirs(joinPath(outFolder, "static"));

    // Copy front page to content/index.html.
    String frontPage = Filesystem::loadText(joinPath(sourceFolder, "index.html"));
    appendPublishKeyToAsset(frontPage, "/static/common.css");
    appendPublishKeyToAsset(frontPage, "/static/front.css");
    appendPublishKeyToAsset(frontPage, "/static/common.js");
    Filesystem::saveText(joinPath(outFolder, "content/index.html"), frontPage, serverTextFormat);

    // Copy static files to static/.
    for (const DirectoryEntry& entry : Filesystem::listDir(joinPath(sourceFolder, "static"))) {
        if (entry.isFile()) {
            String srcPath = joinPath(sourceFolder, "static", entry.name);
            String dstPath = joinPath(outFolder, "static", entry.name);
            if (entry.name.endsWith(".css") || entry.name.endsWith(".js") || entry.name.endsWith(".html")) {
                String text = Filesystem::loadTextAutodetect(srcPath);
                Filesystem::saveText(dstPath, text, serverTextFormat);
            } else {
                Filesystem::copyFile(srcPath, dstPath);
            }
        }
    }

    // Copy docs template to content/.
    String templateText = Filesystem::loadTextAutodetect(joinPath(sourceFolder, "docs-template.html"));
    appendPublishKeyToAsset(templateText, "/static/common.css");
    appendPublishKeyToAsset(templateText, "/static/docs.css");
    appendPublishKeyToAsset(templateText, "/static/common.js");
    appendPublishKeyToAsset(templateText, "/static/doc-viewer.js");
    Filesystem::saveText(joinPath(outFolder, "content/docs-template.html"), templateText, serverTextFormat);

    // Parse contents.json and generate table of contents HTML.
    contents = parseJson(joinPath(docsFolder, "contents.json"));
    MemStream tocStream;
    generateTableOfContentsHtml(tocStream, contents);
    Filesystem::makeDirs(joinPath(outFolder, "content/docs"));
    Filesystem::saveText(joinPath(outFolder, "content/toc.html"), tocStream.moveToString(), serverTextFormat);

    // Traverse contents.json and generate pages in content/docs/.
    Array<const json::Node*> pages;
    flattenPages(pages, contents);
    for (u32 i = 0; i < pages.numItems(); i++) {
        const json::Node* prevPage = (i > 0) ? pages[i - 1] : nullptr;
        const json::Node* nextPage = (i + 1 < pages.numItems()) ? pages[i + 1] : nullptr;
        convertPage(*pages[i], prevPage, nextPage);
    }
}

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
            getStdOut().write("Done.\n");
        }
#else
        getStdOut().write("-watch is not supported on this platform.");
#endif
    }

    return 0;
}
