/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-http.h>

using namespace ply;

String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");

//-------------------------------------
// servePlywoodDocumentation
//-------------------------------------
void servePlywoodDocumentation(HTTPServerRequest& request) {
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
                request.sendGenericResponse(HTTPServerResponse::NotFound);
                return;
            }

            HTTPServerResponse response{HTTPServerResponse::OK};
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
            HTTPServerResponse response{HTTPServerResponse::OK};
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
                HTTPServerResponse response{HTTPServerResponse::PermanentRedirect};
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
                request.sendGenericResponse(HTTPServerResponse::NotFound);
                return;
            }

            HTTPServerResponse response{HTTPServerResponse::OK};
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

    request.sendGenericResponse(HTTPServerResponse::NotFound);
}

//-------------------------------------
// main
//-------------------------------------
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    Network::initialize(IPV4);
    u16 port = 8080;
    getStdOut().format("Listening for connections on port {}...\n", port);
    runHTTPServer(port, servePlywoodDocumentation);
    Network::shutdown();
    return 0;
}
