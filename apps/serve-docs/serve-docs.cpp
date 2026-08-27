/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    serve-docs                                        │
│               Documentation: /docs/apps/serve-docs.md           │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-network.h>

using namespace ply;

String docsFolder = joinPath(PLYWOOD_ROOT_DIR, "docs/build");

//-------------------------------------
// servePlywoodDocumentation
//-------------------------------------
void servePlywoodDocumentation(HTTPServer::Request& request) {
    String urlPath = request.uri;
    s32 queryPos = urlPath.find('?');
    if (queryPos >= 0) {
        urlPath = urlPath.left(queryPos);
    }

    // Serve the introduction page at the documentation root, including AJAX requests from the page viewer.
    if ((urlPath == "/docs") || (urlPath == "/docs/")) {
        urlPath = "/docs/introduction";
    } else if (urlPath == "/docs.ajax") {
        urlPath = "/docs/introduction.ajax";
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
            if (FileSystem::exists(localPath) == ExistsResult::NotFound) {
                request.sendGenericResponse(HTTPServer::Response::NotFound);
                return;
            }

            HTTPServer::Response response{HTTPServer::Response::OK};
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
                request.sendFullResponse(std::move(response), FileSystem::loadText(localPath));
            } else {
                request.sendFullResponse(std::move(response), FileSystem::loadBinary(localPath));
            }
            return;
        }
        if (parts[0].isEmpty()) {
            HTTPServer::Response response{HTTPServer::Response::OK};
            *response.headers.insert("content-type").value = "text/html";
            String templ = FileSystem::loadText(joinPath(docsFolder, "content/index.html"));
            String toc = FileSystem::loadText(joinPath(docsFolder, "content/toc.html"));
            String fullHtml = templ.replace("{%toc%}", toc);
            request.sendFullResponse(std::move(response), fullHtml);
            return;
        }
        if (parts[0] == "docs") {
            String localPath = joinPath(docsFolder, "content/docs", StringView{'/'}.join(parts.subview(1)));
            bool isAjaxRequest = localPath.endsWith(".ajax");
            if (isAjaxRequest) {
                localPath = localPath.left(localPath.numBytes() - 5); // Remove ".ajax"
            }

            if (FileSystem::isDir(localPath)) {
                localPath = joinPath(localPath, "index.html");
            } else {
                localPath += ".html";
            }

            if (FileSystem::exists(localPath) == ExistsResult::NotFound) {
                request.sendGenericResponse(HTTPServer::Response::NotFound);
                return;
            }

            HTTPServer::Response response{HTTPServer::Response::OK};
            *response.headers.insert("content-type").value = "text/html";

            if (isAjaxRequest) {
                // Serve AJAX content directly
                request.sendFullResponse(std::move(response), FileSystem::loadText(localPath));
            } else {
                // Assemble full page from template + TOC + AJAX content
                String templ = FileSystem::loadText(joinPath(docsFolder, "content/docs-template.html"));
                String toc = FileSystem::loadText(joinPath(docsFolder, "content/toc.html"));
                String ajaxContent = FileSystem::loadText(localPath);

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

    request.sendGenericResponse(HTTPServer::Response::NotFound);
}

//-------------------------------------
// main
//-------------------------------------
int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    Network::initialize(IPv4);
    u16 port = 8080;
    getStdOut().format("Listening for connections on port {}...\n", port);
    HTTPServer::run({}, port, servePlywoodDocumentation);
    Network::shutdown();
    return 0;
}
