<?php
define('CONTENT_PATH', '../../webcontent/plywood/');

$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

if ($path === '/') {
    $file = CONTENT_PATH . "index.html";
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
    $file = CONTENT_PATH . "docs/" . $subpath;
    if (is_dir($file)) {
        $file = $file . "/index.html";
    } elseif (str_ends_with($file, '.ajax')) {
        $path_without_ajax = substr($file, 0, -5);
        if (is_dir($path_without_ajax)) {
            $file = $path_without_ajax . "/index.ajax.html";
        } else {
            $file = $file . ".html";
        }
    } else {
        $file = $file . ".html";
    }
}

if (empty($file) || !file_exists($file)) {
    http_response_code(404);
    echo "Not found";
    exit;
}

readfile($file);
?>