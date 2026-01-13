<?php
define('CONTENT_PATH', '../../webcontent/plywood/');

$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

if ($path === '/') {
    $file = CONTENT_PATH . "index.html";
    if (!file_exists($file)) {
        http_response_code(404);
        echo "Not found";
        exit;
    }
    $template = file_get_contents($file);
    $toc = file_get_contents(CONTENT_PATH . "toc.html");
    $full_html = str_replace("{%toc%}", $toc, $template);
    echo $full_html;
    exit;
} elseif (($path === "/docs") || ($path === "/docs/")) {
    header("Location: /docs/intro");
    exit();
} elseif (str_starts_with($path, '/docs/')) {
    $path = str_replace('\0', '', $path);
    // Split path into components and filter out those starting with '.'
    $parts = array_filter(explode('/', substr($path, 6)), function($part) {
        return $part !== '' && !str_starts_with($part, '.');
    });
    $subpath = implode('/', $parts);

    // Check for AJAX request (URL ends with .ajax)
    $is_ajax = str_ends_with($subpath, '.ajax');
    if ($is_ajax) {
        $subpath = substr($subpath, 0, -5); // Remove ".ajax"
    }

    $file = CONTENT_PATH . "docs/" . $subpath;
    if (is_dir($file)) {
        $file = $file . "/index.html";
    } else {
        $file = $file . ".html";
    }

    if (!file_exists($file)) {
        http_response_code(404);
        echo "Not found";
        exit;
    }

    if ($is_ajax) {
        // Serve AJAX content directly
        readfile($file);
    } else {
        // Assemble full page from template + TOC + content
        $template = file_get_contents(CONTENT_PATH . "docs-template.html");
        $toc = file_get_contents(CONTENT_PATH . "toc.html");
        $ajax_content = file_get_contents($file);

        // Parse title from first line of AJAX content
        $newline_pos = strpos($ajax_content, "\n");
        $title = substr($ajax_content, 0, $newline_pos);
        $content = substr($ajax_content, $newline_pos + 1);

        // Replace placeholders
        $full_html = str_replace("{%title%}", $title, $template);
        $full_html = str_replace("{%toc%}", $toc, $full_html);
        $full_html = str_replace("{%content%}", $content, $full_html);
        echo $full_html;
    }
    exit;
}

http_response_code(404);
echo "Not found";
?>
