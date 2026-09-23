/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    serve-docs                                        │
│               Documentation: docs/apps/serve-docs.md            │
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

    // Split urlPath into components, strip leading '.' characters, and drop empty parts.
    Array<StringView> parts;
    for (StringView part : urlPath.split("/")) {
        while (part.startsWith(".")) {
            part = part.substr(1);
        }
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }

    // Serve the front page at the site root.
    if (parts.isEmpty()) {
        HTTPServer::Response response{HTTPServer::Response::OK};
        *response.headers.insert("content-type").value = "text/html";
        String templ = FileSystem::loadText(joinPath(docsFolder, "content/index.html"));
        request.sendFullResponse(std::move(response), templ);
        return;
    }

    // Serve static assets (the production web server handles these; needed for local serving).
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
        } else if (localPath.endsWith(".jpg")) {
            *response.headers.insert("content-type").value = "image/jpeg";
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

    // Strip ".ajax" extension from the last part.
    bool isAjax = parts.back().endsWith(".ajax");
    if (isAjax) {
        parts.back() = parts.back().shortenedBy(5); // Remove ".ajax"
    }

    // Serve the introduction page at the documentation root, including AJAX requests from the page viewer.
    if ((parts.numItems() == 1) && (parts[0] == "docs")) {
        parts.append("introduction");
    }

    // Must start with '/docs/'.
    if (parts[0] != "docs") {
        request.sendGenericResponse(HTTPServer::Response::NotFound);
        return;
    }

    // Reject "index" as last part.
    if (parts.back() == "index") {
        request.sendGenericResponse(HTTPServer::Response::NotFound);
        return;
    }

    // Make file path.
    String normRelPath = StringView{'/'}.join(parts);
    String relFilePath = normRelPath;
    if (normRelPath == "docs") {
        relFilePath = "docs/introduction";
    }

    // Detect directories.
    String filePath = joinPath(joinPath(docsFolder, "content"), relFilePath);
    if (FileSystem::isDir(filePath)) {
        filePath = joinPath(filePath, "index.html");
    } else {
        filePath += ".html";
    }

    // Check file existence.
    if (FileSystem::exists(filePath) == ExistsResult::NotFound) {
        request.sendGenericResponse(HTTPServer::Response::NotFound);
        return;
    }

    HTTPServer::Response response{HTTPServer::Response::OK};
    *response.headers.insert("content-type").value = "text/html";

    if (isAjax) {
        // Serve AJAX content directly.
        request.sendFullResponse(std::move(response), FileSystem::loadText(filePath));
    } else {
        // Assemble full page from template + TOC + content.
        String templ = FileSystem::loadText(joinPath(docsFolder, "content/docs-template.html"));
        String toc = FileSystem::loadText(joinPath(docsFolder, "content/toc.html"));
        String ajaxContent = FileSystem::loadText(filePath);

        // Parse title from first line of AJAX content.
        s32 newlinePos = ajaxContent.find('\n');
        String title = ajaxContent.left(newlinePos);
        String content = ajaxContent.substr(newlinePos + 1);
        String url = "https://plywood.dev/" + relFilePath;

        // Escape text used in both the title element and social metadata attributes.
        String fullHtml = templ.replace("{%title%}", String::format("{:&}", title));
        fullHtml = fullHtml.replace("{%url%}", String::format("{:&}", url));
        fullHtml = fullHtml.replace("{%toc%}", toc);
        fullHtml = fullHtml.replace("{%content%}", content);
        request.sendFullResponse(std::move(response), fullHtml);
    }
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