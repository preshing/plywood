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
Owned<json::Node> contents;
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
        out.format("<div class=\"caption\">{}</div>\n", html.substr(3, html.num_bytes - 8));
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
                if (Owned<markdown::Node> node = flush(md)) {
                    convert_to_html(&out, node, options);
                }
                out.write("</dd>\n<dt>");
                in_title = true;
                first_decl = true;
            } else {
                if (Owned<markdown::Node> node = parse_line(md, line)) {
                    convert_to_html(&out, node, options);
                }
            }
        }
    }
    if (in_title) {
        out.write("</dt></dl>\n");
    } else {
        // Flush current markdown block.
        if (Owned<markdown::Node> node = flush(md)) {
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
        for (StringView column : s.split_byte('|')) {
            String html = markdown::convert_to_html(column);
            out.format("<td>{}</td>", html.substr(3, html.num_bytes - 8));
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
            if (Owned<markdown::Node> node = flush(parser)) {
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
                if (Owned<markdown::Node> node = flush(parser)) {
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
            if (Owned<markdown::Node> node = parse_line(parser, line)) {
                convert_to_html(&out, node, options);
            }
        }
    }
    if (Owned<markdown::Node> node = flush(parser)) {
        convert_to_html(&out, node, options);
    }
}

void generate_table_of_contents_html(Stream& out, const json::Node* items) {
    for (const json::Node* item : items->array_view()) {
        const json::Node* children = item->get("children");
        if (children->is_valid()) {
            // Item with children: collapsible AND navigable
            // Start expanded (caret-down) by default
            out.format("<a href=\"/docs/{}\"><li class=\"caret caret-down selectable\"><span>{&}</span></li></a>",
                       item->get("path")->text(), item->get("title")->text());
            out.write("<ul class=\"nested active\">");
            generate_table_of_contents_html(out, children);
            out.write("</ul>");
        } else {
            // Leaf item: navigable link
            out.format("<li class=\"selectable\"><a href=\"/docs/{}\">{&}</a></li>", item->get("path")->text(),
                       item->get("title")->text());
        }
    }
}

void convert_page(const json::Node* item) {
    String rel_name = item->get("path")->text();
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
    String page_title = item->get("title")->text();

    // Write content-only file for AJAX loading (format: "Title\n<content>")
    String ajax_content = String::format("{} :: Plywood C++ Base Library\n{}", page_title, article_content);
    String ajax_path = join_path(out_folder, "content/docs", rel_name + ".ajax.html");
    Filesystem::make_dirs(split_path(ajax_path).directory);
    Filesystem::save_text(ajax_path, ajax_content);

    // Write full HTML page
    MemStream table_of_contents;
    generate_table_of_contents_html(table_of_contents, contents);
    StringView format_str = R"(
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{&} :: Plywood C++ Base Library</title>
<link rel="stylesheet" href="/static/common.css?key={}">
<link rel="stylesheet" href="/static/docs.css?key={}">
<script>
  // Apply saved theme immediately to prevent flash
  (function() {{
    var theme = localStorage.getItem('theme');
    if (theme === 'dark' || (!theme && window.matchMedia('(prefers-color-scheme: dark)').matches)) {{
      document.documentElement.setAttribute('data-theme', 'dark');
    }}
  }})();
</script>
</head>
<body>
  <div class="banner">
    <div class="title">
        <a href="/">
            <img class="logo" src="/static/plywood-house-small.png" srcset="/static/plywood-house-small.png 1x, /static/plywood-house-small@2x.png 2x" alt="Plywood logo" style="position: absolute; left: 19px; top: 5px;">
            <span style="color: #e8e8e8; font-size: 22px; position: absolute; left: 73px; top: 10px;">Plywood</span>
        </a>
        <span class="right">
          <div id="nav-links"><a href="/docs/intro"><span class="nav-link">DOCS</span></a><a href="https://github.com/preshing/plywood"><span class="nav-link">GITHUB</span></a>
            <span class="theme-toggle-parent"><button class="theme-toggle" id="theme-toggle" aria-label="Toggle dark mode" title="Toggle dark/light mode">
                <svg class="moon-icon" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
                    <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>
                </svg>
                <svg class="sun-icon" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
                    <circle cx="12" cy="12" r="5"/>
                    <line x1="12" y1="1" x2="12" y2="3" stroke-width="2" stroke-linecap="round"/>
                    <line x1="12" y1="21" x2="12" y2="23" stroke-width="2" stroke-linecap="round"/>
                    <line x1="4.22" y1="4.22" x2="5.64" y2="5.64" stroke-width="2" stroke-linecap="round"/>
                    <line x1="18.36" y1="18.36" x2="19.78" y2="19.78" stroke-width="2" stroke-linecap="round"/>
                    <line x1="1" y1="12" x2="3" y2="12" stroke-width="2" stroke-linecap="round"/>
                    <line x1="21" y1="12" x2="23" y2="12" stroke-width="2" stroke-linecap="round"/>
                    <line x1="4.22" y1="19.78" x2="5.64" y2="18.36" stroke-width="2" stroke-linecap="round"/>
                    <line x1="18.36" y1="5.64" x2="19.78" y2="4.22" stroke-width="2" stroke-linecap="round"/>
                </svg>
            </button></span>
          </div>
            <span id="hamburger" class="button"></span>
        </span>
    </div>
  </div>
  <div class="inner-body">  
  <div class="directory">
    <div class="scroller">
      <div class="inner">
        <ul>
{}
        </ul>
      </div>
    </div>
  </div>
  <div class="content-parent"><article class="content" id="article">
{}
  </article></div>
  <script>
    (function() {{
      var toggle = document.getElementById('theme-toggle');
      toggle.addEventListener('click', function() {{
        var currentTheme = document.documentElement.getAttribute('data-theme');
        var newTheme = currentTheme === 'dark' ? 'light' : 'dark';
        if (newTheme === 'light') {{
          document.documentElement.removeAttribute('data-theme');
        }} else {{
          document.documentElement.setAttribute('data-theme', 'dark');
        }}
        localStorage.setItem('theme', newTheme);
      }});
    }})();
  </script>
  <script src="/static/doc-viewer.js"></script>
</div></body>
</html>
)";
    String full_html = String::format(format_str, page_title, publish_key, publish_key,
                                      table_of_contents.move_to_string(), article_content);

    String out_path = join_path(out_folder, "content/docs", rel_name + ".html");
    Filesystem::make_dirs(split_path(out_path).directory);
    Filesystem::save_text(out_path, full_html);

    if (item->get("children")->is_valid()) {
        for (const json::Node* child : item->get("children")->array_view()) {
            convert_page(child);
        }
    }
}

Owned<json::Node> parse_json(StringView path) {
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
    Filesystem::save_text(join_path(out_folder, "content/index.html"), front_page);

    // Copy static files to static/.
    for (const DirectoryEntry& entry : Filesystem::list_dir(join_path(source_folder, "static"))) {
        if (entry.is_file()) {
            Filesystem::copy_file(join_path(source_folder, "static", entry.name),
                                  join_path(out_folder, "static", entry.name));
        }
    }

    // Traverse contents.json and generate pages in content/docs/.
    contents = parse_json(join_path(docs_folder, "contents.json"));
    for (const json::Node* item : contents->array_view()) {
        convert_page(item);
    }
}

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
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
    }

    return 0;
}
