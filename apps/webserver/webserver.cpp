/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-network.h>

using namespace ply;

String docs_folder = join_path(PLYWOOD_ROOT_DIR, "docs/build");

//-------------------------------------
// Public request handling API
//-------------------------------------

struct Request {
    IPAddress client_addr;
    u16 client_port = 0;
    String method;
    String uri;
    String http_version;
    Map<String, String> headers;
};

class Response;
using RequestHandler = Functor<void(const Request& request, Response& response)>;

class Response {
private:
    Stream* out = nullptr;
    friend void handle_http_request(TCPConnection* tcp_conn, const RequestHandler& req_handler);

public:
    enum Code {
        OK = 200,
        PermanentRedirect = 301,
        TemporaryRedirect = 302,
        BadRequest = 400,
        NotFound = 404,
        InternalError = 500,
    };

    Map<String, String> headers;

    Stream* begin(Code response_code);
};

void send_generic_response(Response& response, Response::Code response_code);

//-------------------------------------
// serve_plywood_docs
//-------------------------------------

void serve_plywood_docs(const Request& request, Response& response) {
    String url_path = request.uri;
    s32 query_pos = url_path.find('?');
    if (query_pos >= 0) {
        url_path = url_path.left(query_pos);
    }
    Array<StringView> parts = url_path.split_byte('/');
    if (parts.num_items() > 5) {
        parts = parts.subview(0, 5);
    }
    for (u32 i = 1; i < parts.num_items(); i++) {
        if (parts[i].starts_with('.')) {
            parts.erase(i--);
        }
    }

    if (parts.num_items() > 0) {
        if (parts[0] == "static") {
            String local_path = join_path(docs_folder, StringView{'/'}.join(parts));
            if (!Filesystem::exists(local_path)) {
                send_generic_response(response, Response::NotFound);
                return;
            }

            bool is_text_file = false;
            if (local_path.ends_with(".css")) {
                *response.headers.insert("Content-type").value = "text/css";
                is_text_file = true;
            } else if (local_path.ends_with(".js")) {
                *response.headers.insert("Content-type").value = "application/javascript";
                is_text_file = true;
            } else if (local_path.ends_with(".woff")) {
                *response.headers.insert("Content-type").value = "font/woff";
            } else if (local_path.ends_with(".woff2")) {
                *response.headers.insert("Content-type").value = "font/woff2";
            } else if (local_path.ends_with(".png")) {
                *response.headers.insert("Content-type").value = "image/png";
            } else {
                PLY_ASSERT(0);
            }
            Stream* out = response.begin(Response::OK);
            if (is_text_file) {
                out->write(Filesystem::load_text(local_path));
            } else {
                out->write(Filesystem::load_binary(local_path));
            }
            return;
        }
        if (parts[0].is_empty()) {
            *response.headers.insert("Content-type").value = "text/html";
            Stream* out = response.begin(Response::OK);
            String local_path = join_path(docs_folder, "content/index.html");
            out->write(Filesystem::load_text(local_path));
            return;
        }
        if (parts[0] == "docs") {
            if (parts.num_items() == 1) {
                // FIXME: Include the hostname in the Location URL.
                *response.headers.insert("Location").value = "/docs/intro";
                response.begin(Response::PermanentRedirect);
                return;
            }

            String local_path = join_path(docs_folder, "content/docs", StringView{'/'}.join(parts.subview(1)));
            if (Filesystem::is_dir(local_path)) {
                local_path = join_path(local_path, "index.html");
            } else if (local_path.ends_with(".ajax")) {
                // AJAX content-only request (e.g. /docs/intro.ajax or /docs/parsers.ajax)
                String path_without_ajax = local_path.left(local_path.num_bytes - 5); // Remove ".ajax"
                if (Filesystem::is_dir(path_without_ajax)) {
                    local_path = join_path(path_without_ajax, "index.ajax.html");
                } else {
                    local_path += ".html";
                }
            } else {
                local_path += ".html";
            }

            if (!Filesystem::exists(local_path)) {
                send_generic_response(response, Response::NotFound);
                return;
            }

            *response.headers.insert("Content-type").value = "text/html";
            Stream* out = response.begin(Response::OK);
            out->write(Filesystem::load_text(local_path));
            return;
        }
    }

    send_generic_response(response, Response::NotFound);
}

//-------------------------------------
// serve_echo_page (for testing)
//-------------------------------------

void serve_echo_page(const Request& request, Response& response) {
    *response.headers.insert("Content-type").value = "text/html";
    Stream* out = response.begin(Response::OK);
    out->write(R"(<html>
<head><title>Echo</title></head>
<body>
<center><h1>Echo</h1></center>
)");

    // Write client IP
    out->format("<p>Connection from: <code>{&}:{}</code></p>", request.client_addr.to_string(), request.client_port);

    // Write request header
    out->write("<p>Request header:</p>\n");
    out->write("<pre>\n");
    out->format("{&} {&} {&}\n", request.method, request.uri, request.http_version);
    for (const auto& item : request.headers.item_set.items) {
        out->format("{&}: {&}\n", item.key, item.value);
    }
    out->write("</pre>\n");
    out->write(R"(</body>
</html>
)");
}

//-------------------------------------
// run_http_server
//-------------------------------------

StringView get_response_description(Response::Code response_code) {
    switch (response_code) {
        case Response::OK:
            return "OK";
        case Response::PermanentRedirect:
            return "Moved Permanently";
        case Response::TemporaryRedirect:
            return "Found";
        case Response::BadRequest:
            return "Bad Request";
        case Response::NotFound:
            return "Not Found";
        case Response::InternalError:
        default:
            return "Internal Server Error";
    }
}

Stream* Response::begin(Response::Code response_code) {
    StringView message = get_response_description(response_code);
    out->format("HTTP/1.1 {} {}\r\n", response_code, message);
    for (const auto& item : headers.item_set.items) {
        out->format("{}: {}\r\n", item.key, item.value);
    }
    out->write("\r\n");
    return out;
}

void send_generic_response(Response& response, Response::Code response_code) {
    *response.headers.insert("Content-type").value = "text/html";
    Stream* out = response.begin(response_code);
    StringView message = get_response_description(response_code);
    out->format(R"(<html>
<head><title>{} {}</title></head>
<body>
<center><h1>{} {}</h1></center>
<hr>
</body>
</html>
)", response_code, message, response_code, message);
}

void handle_http_request(TCPConnection* tcp_conn, const RequestHandler& req_handler) {
    Stream in = tcp_conn->create_in_stream();
    Stream out = tcp_conn->create_out_stream();

    // Create request and response objects
    Request request;
    request.client_addr = tcp_conn->remote_address();
    request.client_port = tcp_conn->remote_port();
    Response response;
    response.out = &out;

    // Parse HTTP request line
    String request_line = read_line(in);
    Array<StringView> tokens = request_line.trim_right().split_byte(' ');
    if (tokens.num_items() != 3) {
        // Ill-formed request
        send_generic_response(response, Response::BadRequest);
        return;
    }
    request.method = tokens[0];
    request.uri = tokens[1];
    request.http_version = tokens[2];

    // Parse HTTP headers
    for (;;) {
        String line = read_line(in);
        if (line.trim().is_empty())
            break; // Blank line
        if (is_whitespace(line[0]))
            continue; // FIXME: Support unfolding https://tools.ietf.org/html/rfc822#section-3.1
        s32 colon_pos = line.find(':');
        if (colon_pos < 0) {
            // Ill-formed request
            send_generic_response(response, Response::BadRequest);
            return;
        }
        *request.headers.insert(line.left(colon_pos).trim()).value = line.substr(colon_pos + 1).trim();
    }

    // Invoke request handler
    req_handler(request, response);
}

void run_http_server(u16 port, const RequestHandler& req_handler) {
    TCPListener listener = Network::bind_tcp(port);
    if (!listener.is_valid()) {
        get_stderr().format("Error: Can't bind to port {}\n", port);
        return;
    }

    for (;;) {
        Owned<TCPConnection> tcp_conn = listener.accept();
        if (!tcp_conn)
            break;
        spawn_thread([tcp_conn = std::move(tcp_conn), &req_handler] { handle_http_request(tcp_conn.get(), req_handler); });
    }
}

//-------------------------------------
// main
//-------------------------------------

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    Network::initialize(IPV4);
//    run_http_server(8080, serve_echo_page);
    run_http_server(8080, serve_plywood_docs);
    Network::shutdown();
    return 0;
}
