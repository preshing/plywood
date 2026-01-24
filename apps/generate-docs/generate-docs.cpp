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

String source_folder = join_path(PLYWOOD_ROOT_DIR, "apps/generate-docs/data");
String docs_folder = join_path(PLYWOOD_ROOT_DIR, "docs");
String out_folder = join_path(PLYWOOD_ROOT_DIR, "docs/build");
TextFormat server_text_format = {UTF8, TextFormat::LF, false};
json::Node contents;
u32 publish_key = 0; // Prevent browsers from caching old stylesheets

void print_decl_as_api_title(Stream& out, const Parser* parser, const Declaration& decl) {
    Array<TokenSpan> spans = parser->syntax_highlight(decl);
    out.write("<code>");

    // Output token spans.
    TokenSpan::Color last_color = TokenSpan::None;
    bool got_first_declarator_qid = false;
    for (const TokenSpan& span : spans) {
        if (last_color != span.color) {
            if (last_color != TokenSpan::None) {
                out.write("</span>");
            }
            if (span.color == TokenSpan::Type) {
                out.write("<span class=\"type\">");
            } else if (span.color == TokenSpan::Symbol) {
                out.write("<span class=\"symbol\">");
            } else if (span.color == TokenSpan::Variable) {
                out.write("<span class=\"var\">");
            }
            last_color = span.color;
        }
        if (span.is_space) {
            out.write(got_first_declarator_qid ? " " : "&nbsp;");
        } else {
            print_xml_escaped_string(out, span.token.text);
        }
    }
    if (last_color != TokenSpan::None) {
        out.write("</span>");
    }
    out.write("</code>");
}

void print_decl_as_html(Stream& out, const Parser* parser, const Declaration& decl) {
    Array<TokenSpan> spans = parser->syntax_highlight(decl);
    StringView main_row_header = "<tr class=\"entry\"><td class=\"prefix\"><code>";

    // Find first declarator.
    const Declaration* main_declaration = &decl;
    Token first_main_token;
    if (auto* tmpl = main_declaration->var.as<Declaration::Template>()) {
        main_declaration = tmpl->child_decl;
        first_main_token = main_declaration->get_first_token();
        out.write("<tr><td colspan=\"2\" class=\"template\"><code>");
    } else {
        out.write(main_row_header);
    }

    const cpp::QualifiedID* first_declarator_qid = nullptr;
    if (auto* entity = main_declaration->var.as<Declaration::Entity>()) {
        if (!entity->init_declarators.is_empty()) {
            if (!entity->init_declarators[0].qid.is_empty()) {
                first_declarator_qid = &entity->init_declarators[0].qid;
            }
        }
    }

    // Output token spans.
    TokenSpan::Color last_color = TokenSpan::None;
    bool got_first_declarator_qid = false;
    for (const TokenSpan& span : spans) {
        if (first_main_token.is_valid() && (span.token == first_main_token)) {
            out.write("</code></td></tr>\n");
            out.write(main_row_header);
        }
        if (!got_first_declarator_qid && first_declarator_qid && (first_declarator_qid == span.qid)) {
            if (last_color != TokenSpan::None) {
                out.write("</span>");
                last_color = TokenSpan::None;
            }
            out.write("</code></td><td class=\"suffix\"><code>");
            got_first_declarator_qid = true;
        }
        if (last_color != span.color) {
            if (last_color != TokenSpan::None) {
                out.write("</span>");
            }
            if (span.color == TokenSpan::Type) {
                out.write("<span class=\"type\">");
            } else if (span.color == TokenSpan::Symbol) {
                out.write("<span class=\"symbol\">");
            } else if (span.color == TokenSpan::Variable) {
                out.write("<span class=\"var\">");
            }
            last_color = span.color;
        }
        if (span.is_space) {
            out.write(got_first_declarator_qid ? " " : "&nbsp;");
        } else {
            print_xml_escaped_string(out, span.token.text);
        }
    }
    if (last_color != TokenSpan::None) {
        out.write("</span>");
    }
    out.write("</code></td></tr>\n");
}

void parse_api_summary(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    // Write optional caption.
    if (const String* caption = args.find("caption")) {
        String html = markdown::convert_to_html(*caption);
        out.format("<div class=\"caption\">{}</div>\n", html.substr(3, html.num_bytes() - 8));
    }

    // Get class name.
    StringView class_name;
    if (const String* c = args.find("class")) {
        class_name = *c;
    }

    out.write("<table class=\"api\">\n");
    while (StringView line = read_line(in)) {
        StringView s = line.trim();
        if (s.starts_with("--")) {
            StringView caption = s.substr(2).trim();
            if (caption) {
                out.format("<tr class=\"heading\"><td colspan=\"2\" class=\"heading\">{&}</td></tr>\n", caption);
            }
            continue;
        }
        if (s == "{/api_summary}")
            break;
        Owned<Parser> parser = Parser::create();
        Declaration decl = parser->parse_declaration(s, class_name);
        print_decl_as_html(out, parser, decl);
    }
    out.write("</table>\n");
}

void parse_api_descriptions(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    // Get class name.
    StringView class_name;
    if (const String* c = args.find("class")) {
        class_name = *c;
    }

    markdown::HTML_Options options;
    Owned<markdown::Parser> md = markdown::create_parser();
    out.write("<dl class=\"api_defs\"><dt>");
    bool in_title = true;
    bool first_decl = true;
    while (StringView line = read_line(in)) {
        if (line.trim() == "{/api_descriptions}")
            break;
        if (in_title) {
            if (line.trim().is_empty())
                continue;
            if (line.starts_with("--")) {
                out.write("</dt>\n<dd>");
                in_title = false;
            } else {
                Owned<Parser> parser = Parser::create();
                Declaration decl = parser->parse_declaration(line.trim(), class_name);
                if (!first_decl) {
                    out.write("<br>\n");
                }
                print_decl_as_api_title(out, parser, decl);
                first_decl = false;
            }
        } else {
            if (line.starts_with(">>")) {
                // Flush current markdown block.
                if (Owned<markdown::Element> node = flush(md)) {
                    convert_to_html(&out, node, options);
                }
                out.write("</dd>\n<dt>");
                in_title = true;
                first_decl = true;
            } else {
                if (Owned<markdown::Element> node = parse_line(md, line)) {
                    convert_to_html(&out, node, options);
                }
            }
        }
    }
    if (in_title) {
        out.write("</dt></dl>\n");
    } else {
        // Flush current markdown block.
        if (Owned<markdown::Element> node = flush(md)) {
            convert_to_html(&out, node, options);
        }
        out.write("</dd></dl>\n");
    }
}

void parse_table(Stream& out, const Map<StringView, String>& args, ViewStream& in) {
    out.write("<table class=\"grid\">\n");
    while (StringView line = read_line(in)) {
        StringView s = line.trim();
        if (s == "{/table}")
            break;
        out.write("<tr>");
        for (StringView column : s.split("|")) {
            String html = markdown::convert_to_html(column);
            out.format("<td>{}</td>", html.substr(3, html.num_bytes() - 8));
        }
        out.write("</tr>\n");
    }
    out.write("</table>\n");
}

void parse_example(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Example</div>\n");
    out.write("<pre>\n");
    while (StringView line = read_line(in)) {
        StringView s = line.trim();
        if (s == "{/example}")
            break;
        print_xml_escaped_string(out, line);
    }
    out.write("</pre>\n");
}

void parse_output(Stream& out, ViewStream& in) {
    out.format("<div class=\"caption\">Output</div>\n");
    out.write("<pre>\n");
    while (StringView line = read_line(in)) {
        StringView s = line.trim();
        if (s == "{/output}")
            break;
        print_xml_escaped_string(out, line);
    }
    out.write("</pre>\n");
}

void parse_markdown(Stream& out, ViewStream& in) {
    markdown::HTML_Options options;
    Owned<markdown::Parser> parser = markdown::create_parser();
    while (StringView line = read_line(in)) {
        ViewStream line_in{line};
        StringView cmd;
        if (line_in.match("'{%i", &cmd)) {
            // Flush current markdown block.
            if (Owned<markdown::Element> node = flush(parser)) {
                convert_to_html(&out, node, options);
            }

            // Parse section arguments.
            Map<StringView, String> args;
            {
                StringView key;
                String value;
                while (line_in.match(" *%i=(%i|%q)", &key, &value, &value)) {
                    *args.insert(key).value = std::move(value);
                }
            }
            PLY_ASSERT(line_in.match(" *'}"));

            // Handle section type.
            if (cmd == "api_summary") {
                parse_api_summary(out, args, in);
            } else if (cmd == "api_descriptions") {
                parse_api_descriptions(out, args, in);
            } else if (cmd == "table") {
                parse_table(out, args, in);
            } else if (cmd == "example") {
                parse_example(out, in);
            } else if (cmd == "output") {
                if (Owned<markdown::Element> node = flush(parser)) {
                    convert_to_html(&out, node, options);
                }
                parse_output(out, in);
            } else if (cmd == "title") {
                out.format("<h1><span class=\"right\"><div class=\"include\"><code>&lt;{&}&gt;</code></div><div "
                           "class=\"namespace\"><code>namespace {&}</code></div></span>{&}</h1>\n",
                           *args.find("include"), *args.find("namespace"), *args.find("text"));
            } else {
                PLY_ASSERT(0); // Unrecognized section type
            }
        } else {
            if (Owned<markdown::Element> node = parse_line(parser, line)) {
                convert_to_html(&out, node, options);
            }
        }
    }
    if (Owned<markdown::Element> node = flush(parser)) {
        convert_to_html(&out, node, options);
    }
}

void flatten_pages(Array<const json::Node*>& pages, const json::Node& items) {
    for (const json::Node& item : items.array_view()) {
        pages.append(&item);
        if (item.get("children").is_valid()) {
            flatten_pages(pages, item.get("children"));
        }
    }
}

void generate_table_of_contents_html(Stream& out, const json::Node& items) {
    for (const json::Node& item : items.array_view()) {
        const json::Node& children = item.get("children");
        StringView span_class;
        if (children.is_valid()) {
            span_class = " class=\"caret caret-down\"";
        }
        String header_file;
        if (item.get("header-file").is_valid()) {
            header_file =
                String::format(" <span class=\"toc-header\">&lt;{&}&gt;</span>", item.get("header-file").text());
        }
        out.format("<a href=\"/docs/{}\"><li class=\"selectable\"><span{}>{&}</span>{}</li></a>",
                   item.get("path").text(), span_class, item.get("title").text(), header_file);
        if (children.is_valid()) {
            out.write("<ul class=\"nested active\">");
            generate_table_of_contents_html(out, children);
            out.write("</ul>");
        }
    }
}

void convert_page(const json::Node& item, const json::Node* prev_page, const json::Node* next_page) {
    String rel_name = item.get("path").text();
    String markdown_path = join_path(docs_folder, rel_name);
    if (Filesystem::is_dir(markdown_path)) {
        rel_name = join_path(rel_name, "index");
        markdown_path = join_path(markdown_path, "index.md");
    } else {
        markdown_path += ".md";
    }
    String markdown = Filesystem::load_text_autodetect(markdown_path);
    ViewStream in{markdown};
    MemStream mem;
    parse_markdown(mem, in);
    String article_content = mem.move_to_string();
    String page_title = item.get("title").text();

    // Generate prev/next navigation
    String prev_link, next_link;
    if (prev_page) {
        prev_link = String::format("<a href=\"/docs/{}\"><span class=\"nav-button\">&#9664;&nbsp; {&}</span></a>",
                                   prev_page->get("path").text(), prev_page->get("title").text());
    }
    if (next_page) {
        next_link = String::format("<a href=\"/docs/{}\"><span class=\"nav-button right\">{&}&nbsp; &#9654;</span></a>",
                                   next_page->get("path").text(), next_page->get("title").text());
    }
    String nav_html = String::format("<div class=\"page-nav\">{}{}</div>", prev_link, next_link);

    // Write content-only file for AJAX loading
    String ajax_content = String::format("{} :: Plywood C++ Base Library\n{}{}", page_title, article_content, nav_html);
    String ajax_path = join_path(out_folder, "content/docs", rel_name + ".html");
    Filesystem::make_dirs(split_path(ajax_path).directory);
    Filesystem::save_text(ajax_path, ajax_content, server_text_format);
}

json::Node parse_json(StringView path) {
    String src = Filesystem::load_text_autodetect(path);
    return json::Parser{}.parse(path, src).root;
}

void generate_whole_site() {
    publish_key = Random{}.generate_u32(); // Prevent browsers from caching old stylesheets

    Filesystem::make_dirs(join_path(out_folder, "content"));
    Filesystem::make_dirs(join_path(out_folder, "static"));

    // Copy front page to content/index.html.
    String front_page = Filesystem::load_text(join_path(source_folder, "index.html"));
    front_page = front_page.replace("/static/style.css", String::format("/static/style.css?key={}", publish_key));
    Filesystem::save_text(join_path(out_folder, "content/index.html"), front_page, server_text_format);

    // Copy static files to static/.
    for (const DirectoryEntry& entry : Filesystem::list_dir(join_path(source_folder, "static"))) {
        if (entry.is_file()) {
            String src_path = join_path(source_folder, "static", entry.name);
            String dst_path = join_path(out_folder, "static", entry.name);
            if (entry.name.ends_with(".css") || entry.name.ends_with(".js") || entry.name.ends_with(".html")) {
                String text = Filesystem::load_text_autodetect(src_path);
                Filesystem::save_text(dst_path, text, server_text_format);
            } else {
                Filesystem::copy_file(src_path, dst_path);
            }
        }
    }

    // Copy docs template to content/.
    String template_text = Filesystem::load_text_autodetect(join_path(source_folder, "docs-template.html"));
    Filesystem::save_text(join_path(out_folder, "content/docs-template.html"), template_text, server_text_format);

    // Parse contents.json and generate table of contents HTML.
    contents = parse_json(join_path(docs_folder, "contents.json"));
    MemStream toc_stream;
    generate_table_of_contents_html(toc_stream, contents);
    Filesystem::make_dirs(join_path(out_folder, "content/docs"));
    Filesystem::save_text(join_path(out_folder, "content/toc.html"), toc_stream.move_to_string(), server_text_format);

    // Traverse contents.json and generate pages in content/docs/.
    Array<const json::Node*> pages;
    flatten_pages(pages, contents);
    for (u32 i = 0; i < pages.num_items(); i++) {
        const json::Node* prev_page = (i > 0) ? pages[i - 1] : nullptr;
        const json::Node* next_page = (i + 1 < pages.num_items()) ? pages[i + 1] : nullptr;
        convert_page(*pages[i], prev_page, next_page);
    }
}

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Check for -watch argument
    bool watch_mode = false;
    for (int i = 1; i < argc; i++) {
        if (StringView{argv[i]} == "-watch") {
            watch_mode = true;
            break;
        }
    }

    generate_whole_site();

    if (watch_mode) {
#if PLY_WITH_DIRECTORY_WATCHER
        get_stdout().write("Watching for changes...\n");

        Mutex mutex;
        ConditionVariable cond;
        Atomic<u32> changed = 0;

        auto on_change = [&](StringView path, bool must_recurse) {
            if (split_path_full(path)[0] != "build") {
                LockGuard<Mutex> lock{mutex};
                changed.store_release(1);
                cond.wake_one();
            }
        };

        DirectoryWatcher source_watcher{source_folder, on_change};
        DirectoryWatcher docs_watcher{docs_folder, on_change};

        for (;;) {
            {
                LockGuard<Mutex> lock{mutex};
                while (!changed.load_acquire()) {
                    cond.wait(lock);
                }
            }

            get_stdout().write("Change detected, regenerating...\n");
            sleep_millis(100);
            changed.store_release(0);
            generate_whole_site();
            get_stdout().write("Done.\n");
        }
#else
        get_stdout().write("-watch is not supported on this platform.");
#endif
    }

    return 0;
}
