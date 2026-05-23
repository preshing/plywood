/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-webserver.h>

using namespace ply;

String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");

//-------------------------------------
// servePlywoodDocs
//-------------------------------------

void servePlywoodDocs(const Request& request, Response& response) {
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
                sendGenericResponse(response, Response::NotFound);
                return;
            }

            bool isTextFile = false;
            if (localPath.endsWith(".css")) {
                *response.headers.insert("Content-type").value = "text/css";
                isTextFile = true;
            } else if (localPath.endsWith(".js")) {
                *response.headers.insert("Content-type").value = "application/javascript";
                isTextFile = true;
            } else if (localPath.endsWith(".woff")) {
                *response.headers.insert("Content-type").value = "font/woff";
            } else if (localPath.endsWith(".woff2")) {
                *response.headers.insert("Content-type").value = "font/woff2";
            } else if (localPath.endsWith(".png")) {
                *response.headers.insert("Content-type").value = "image/png";
            } else {
                PLY_ASSERT(0);
            }
            Stream* out = response.begin(Response::OK);
            if (isTextFile) {
                out->write(Filesystem::loadText(localPath));
            } else {
                out->write(Filesystem::loadBinary(localPath));
            }
            return;
        }
        if (parts[0].isEmpty()) {
            *response.headers.insert("Content-type").value = "text/html";
            Stream* out = response.begin(Response::OK);
            String templ = Filesystem::loadText(joinPath(docsFolder, "content/index.html"));
            String toc = Filesystem::loadText(joinPath(docsFolder, "content/toc.html"));
            String fullHtml = templ.replace("{%toc%}", toc);
            out->write(fullHtml);
            return;
        }
        if (parts[0] == "docs") {
            if (parts.numItems() == 1) {
                // FIXME: Include the hostname in the Location URL.
                *response.headers.insert("Location").value = "/docs/intro";
                response.begin(Response::PermanentRedirect);
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
                sendGenericResponse(response, Response::NotFound);
                return;
            }

            *response.headers.insert("Content-type").value = "text/html";
            Stream* out = response.begin(Response::OK);

            if (isAjaxRequest) {
                // Serve AJAX content directly
                out->write(Filesystem::loadText(localPath));
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
                out->write(fullHtml);
            }
            return;
        }
    }

    sendGenericResponse(response, Response::NotFound);
}

//-------------------------------------
// serveEchoPage (for testing)
//-------------------------------------

void serveEchoPage(const Request& request, Response& response) {
    *response.headers.insert("Content-type").value = "text/html";
    Stream* out = response.begin(Response::OK);
    out->write(R"(<html>
<head><title>Echo</title></head>
<body>
<center><h1>Echo</h1></center>
)");

    // Write client IP
    out->format("<p>Connection from: <code>{&}:{}</code></p>", request.clientAddr.toString(), request.clientPort);

    // Write request header
    out->write("<p>Request header:</p>\n");
    out->write("<pre>\n");
    out->format("{&} {&} {&}\n", request.method, request.uri, request.httpVersion);
    for (const auto& item : request.headers.items()) {
        out->format("{&}: {&}\n", item.key, item.value);
    }
    out->write("</pre>\n");
    out->write(R"(</body>
</html>
)");
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
