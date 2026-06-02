/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-http.h>

using namespace ply;

String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");

//-------------------------------------
// servePlywoodDocs
//-------------------------------------

void servePlywoodDocs(const HTTPRequest& request) {
    String urlPath = request.uri;
    s32 queryPos = urlPath.find('?');
    if (queryPos >= 0) {
        urlPath = urlPath.left(queryPos);
    }
    Array<StringView> parts = urlPath.split("/");
    if (parts.numItems() > 5) {
        parts = parts.subview(0, 5);
    }
    for (u32 i = 1; i < parts.numItems(); i++) {
        if (parts[i].startsWith('.')) {
            parts.erase(i--);
        }
    }

    if (parts.numItems() > 0) {
        if (parts[0] == "static") {
            String localPath = joinPath(docsFolder, StringView{'/'}.join(parts));
            if (!Filesystem::exists(localPath)) {
                request.sendGenericResponse(HTTPResponse::NotFound);
                return;
            }

            HTTPResponse response{HTTPResponse::OK};
            bool isTextFile = false;
            if (localPath.endsWith(".css")) {
                *response.headers.insert("content-type").value = "text/css";
                isTextFile = true;
            } else if (localPath.endsWith(".js")) {
                *response.headers.insert("content-type").value = "application/javascript";
                isTextFile = true;
            } else if (localPath.endsWith(".woff")) {
                *response.headers.insert("content-type").value = "font/woff";
            } else if (localPath.endsWith(".woff2")) {
                *response.headers.insert("content-type").value = "font/woff2";
            } else if (localPath.endsWith(".png")) {
                *response.headers.insert("content-type").value = "image/png";
            } else {
                PLY_ASSERT(0);
            }
            if (isTextFile) {
                request.sendFullResponse(std::move(response), Filesystem::loadText(localPath));
            } else {
                request.sendFullResponse(std::move(response), Filesystem::loadBinary(localPath));
            }
            return;
        }
        if (parts[0].isEmpty()) {
            HTTPResponse response{HTTPResponse::OK};
            *response.headers.insert("content-type").value = "text/html";
            String templ = Filesystem::loadText(joinPath(docsFolder, "content/index.html"));
            String toc = Filesystem::loadText(joinPath(docsFolder, "content/toc.html"));
            String fullHtml = templ.replace("{%toc%}", toc);
            request.sendFullResponse(std::move(response), fullHtml);
            return;
        }
        if (parts[0] == "docs") {
            if (parts.numItems() == 1) {
                // FIXME: Include the hostname in the Location URL.
                HTTPResponse response{HTTPResponse::PermanentRedirect};
                *response.headers.insert("location").value = "/docs/intro";
                request.sendFullResponse(std::move(response));
                return;
            }

            String localPath = joinPath(docsFolder, "content/docs", StringView{'/'}.join(parts.subview(1)));
            bool isAjaxRequest = localPath.endsWith(".ajax");
            if (isAjaxRequest) {
                localPath = localPath.left(localPath.numBytes() - 5); // Remove ".ajax"
            }

            if (Filesystem::isDir(localPath)) {
                localPath = joinPath(localPath, "index.html");
            } else {
                localPath += ".html";
            }

            if (!Filesystem::exists(localPath)) {
                request.sendGenericResponse(HTTPResponse::NotFound);
                return;
            }

            HTTPResponse response{HTTPResponse::OK};
            *response.headers.insert("content-type").value = "text/html";

            if (isAjaxRequest) {
                // Serve AJAX content directly
                request.sendFullResponse(std::move(response), Filesystem::loadText(localPath));
            } else {
                // Assemble full page from template + TOC + AJAX content
                String templ = Filesystem::loadText(joinPath(docsFolder, "content/docs-template.html"));
                String toc = Filesystem::loadText(joinPath(docsFolder, "content/toc.html"));
                String ajaxContent = Filesystem::loadText(localPath);

                // Parse title from first line of AJAX content
                s32 newlinePos = ajaxContent.find('\n');
                String title = ajaxContent.left(newlinePos);
                String content = ajaxContent.substr(newlinePos + 1);

                // Replace placeholders
                String fullHtml = templ.replace("{%title%}", title);
                fullHtml = fullHtml.replace("{%toc%}", toc);
                fullHtml = fullHtml.replace("{%content%}", content);
                request.sendFullResponse(std::move(response), fullHtml);
            }
            return;
        }
    }

    request.sendGenericResponse(HTTPResponse::NotFound);
}

//-------------------------------------
// serveEchoPage (for testing)
//-------------------------------------

void serveEchoPage(const HTTPRequest& request) {
    HTTPResponse response{HTTPResponse::OK};
    *response.headers.insert("content-type").value = "text/html";
    MemStream out;
    out.write(R"(<html>
<head><title>Echo</title></head>
<body>
<center><h1>Echo</h1></center>
)");

    // Write client IP
    out.format("<p>Connection from: <code>{&}:{}</code></p>", request.clientAddr.toString(), request.clientPort);

    // Write request header
    out.write("<p>Request header:</p>\n");
    out.write("<pre>\n");
    out.format("{&} {&} {&}\n", request.method, request.uri, request.httpVersion);
    for (const auto& item : request.headers.items()) {
        out.format("{&}: {&}\n", item.key, item.value);
    }
    out.write("</pre>\n");
    out.write(R"(</body>
</html>
)");
    request.sendFullResponse(std::move(response), out.moveToString());
}

//-------------------------------------
// main
//-------------------------------------

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    Network::initialize(IPV4);
    // runHttpServer(8080, serveEchoPage);
    runHttpServer(8080, servePlywoodDocs);
    Network::shutdown();
    return 0;
}
