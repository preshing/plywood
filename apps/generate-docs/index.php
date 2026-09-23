<?php
define('CONTENT_PATH', '../../webcontent/plywood/');

$urlPath = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
// Split urlPath into components, strip leading '.' characters, and drop empty parts.
$parts = [];
foreach (explode('/', $urlPath) as $part) {
    $part = ltrim($part, '.');
    if ($part !== '') {
        $parts[] = $part;
    }
}

// Serve the front page at the site root.
if (empty($parts)) {
    $file = CONTENT_PATH . "index.html";
    if (!file_exists($file)) {
        http_response_code(404);
        echo "Not found";
        exit;
    }
    echo file_get_contents($file);
    exit;
}

// Strip .ajax extension from the last item in parts.
$is_ajax = str_ends_with(end($parts), '.ajax');
if ($is_ajax) {
    $parts[array_key_last($parts)] = substr(end($parts), 0, -5); // Remove ".ajax"
}

// Serve the introduction page at the documentation root, including AJAX requests from the page viewer.
if ($parts === ["docs"]) {
    $parts[] = "introduction";
}

// Must start with '/docs/'.
if ($parts[0] !== "docs") {
    http_response_code(404);
    echo "Not found";
    exit;
}

// Reject "index" as last part.
if (end($parts) == "index") {
    http_response_code(404);
    echo "Not found";
    exit;
}

// Make file path.
$normRelPath = implode('/', $parts);
$relFilePath = $normRelPath;
if ($normRelPath == 'docs') {
    $relFilePath = 'docs/introduction';
}

// Detect directories.
$filePath = CONTENT_PATH . $relFilePath;
if (is_dir($filePath)) {
    $filePath = $filePath . "/index.html";
} else {
    $filePath = $filePath . ".html";
}

// Check file existence.
if (!file_exists($filePath)) {
    http_response_code(404);
    echo "Not found";
    exit;
}

if ($is_ajax) {
    // Serve AJAX content directly.
    readfile($filePath);
} else {
    // Assemble full page from template + TOC + content.
    $template = file_get_contents(CONTENT_PATH . "docs-template.html");
    $toc = file_get_contents(CONTENT_PATH . "toc.html");
    $ajax_content = file_get_contents($filePath);

    // Parse title from first line of AJAX content.
    $newline_pos = strpos($ajax_content, "\n");
    $title = substr($ajax_content, 0, $newline_pos);
    $content = substr($ajax_content, $newline_pos + 1);
    $url = 'https://plywood.dev/' . $relFilePath;

    // Escape text used in both the title element and social metadata attributes.
    $full_html = str_replace("{%title%}", htmlspecialchars($title, ENT_QUOTES, 'UTF-8'), $template);
    $full_html = str_replace("{%url%}", htmlspecialchars($url, ENT_QUOTES, 'UTF-8'), $full_html);
    $full_html = str_replace("{%toc%}", $toc, $full_html);
    $full_html = str_replace("{%content%}", $content, $full_html);
    echo $full_html;
}
?>
