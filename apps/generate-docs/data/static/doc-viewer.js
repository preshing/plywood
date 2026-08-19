// doc-viewer.js - Documentation-specific interactive features
// Feature: AJAX page loading with caching and history management
// Note: Common features (theme toggle, hamburger menu) are in common.js

var directory = null;
var article = null;

// AJAX page loading state
var pageCache = [];
var currentRequest = null;
var currentLoadingTimer = null;
var spinnerShown = false;

//------------------------------------
// AJAX Page Loading
//------------------------------------

// Save current page state to browser history (for back/forward)
function savePageState() {
    var stateData = {
        path: location.pathname + location.hash,
        pageYOffset: window.pageYOffset
    };
    window.history.replaceState(stateData, null);
}

function getScrollBehavior(smooth) {
    return (smooth && !window.matchMedia('(prefers-reduced-motion: reduce)').matches) ? 'smooth' : 'auto';
}

// Scroll to a fragment.
function scrollToAnchor(anchor, smooth) {
    var anchorElement = document.getElementById(anchor);
    if (!anchorElement)
        return;

    anchorElement.scrollIntoView({ behavior: getScrollBehavior(smooth) });
}

// Navigate to a new page using AJAX
function navigateTo(dstPath, forward, pageYOffset) {
    // Extract anchor from path (e.g., /docs/system#section -> anchor = "section")
    var anchorPos = dstPath.indexOf('#');
    var anchor = (anchorPos >= 0) ? dstPath.substr(anchorPos + 1) : '';
    var pathWithoutAnchor = (anchorPos >= 0) ? dstPath.substr(0, anchorPos) : dstPath;
    var currentPath = location.pathname;
    if (currentPath.length > 1 && currentPath.charAt(currentPath.length - 1) === '/') {
        currentPath = currentPath.substr(0, currentPath.length - 1);
    }
    var comparableDstPath = pathWithoutAnchor;
    if (comparableDstPath.length > 1 && comparableDstPath.charAt(comparableDstPath.length - 1) === '/') {
        comparableDstPath = comparableDstPath.substr(0, comparableDstPath.length - 1);
    }
    var isCurrentPageTitle = forward && anchor === '' && comparableDstPath === currentPath;

    // Abort any in-flight request
    if (currentRequest !== null) {
        currentRequest.abort();
        currentRequest = null;
    }

    // Update browser history
    if (forward) {
        history.pushState(null, null, dstPath);
    }

    // Highlight current page in sidebar
    updateSelectedItem(dstPath);

    // Clicking the current page title only changes the scroll position; don't reload the article.
    if (isCurrentPageTitle) {
        window.scrollTo({ top: 0, behavior: getScrollBehavior(true) });
        savePageState();
        return;
    }

    // Function to apply loaded content to the page
    var applyArticle = function(responseText) {
        // First line is the title, rest is content
        var n = responseText.indexOf('\n');
        document.title = responseText.substr(0, n);
        article.innerHTML = responseText.substr(n + 1);

        // Discard any text selection or caret retained from the previous article.
        var selection = window.getSelection();
        if (selection) {
            selection.removeAllRanges();
        }

        // Set up AJAX links in the new content
        replaceLinks(article);

        // Scroll to anchor or restore scroll position
        if (forward && anchor !== '') {
            // pushState sets the fragment before AJAX inserts the heading, so :target and native
            // fragment scrolling aren't reliably recalculated when the new article is applied.
            scrollToAnchor(anchor, true);
        } else {
            window.scrollTo(0, pageYOffset);
        }

        savePageState();
    };

    // Check cache first
    for (var i = pageCache.length - 1; i >= 0; i--) {
        var cached = pageCache[i];
        if (cached.path === pathWithoutAnchor) {
            // Move to end of cache (most recently used)
            pageCache.splice(i, 1);
            pageCache.push(cached);
            applyArticle(cached.responseText);
            return;
        }
    }

    // Not in cache - fetch via AJAX
    currentRequest = new XMLHttpRequest();
    currentRequest.onreadystatechange = function() {
        if (currentRequest !== this)
            return;
        if (this.readyState === 4 && this.status === 200) {
            currentRequest = null;
            spinnerShown = false;
            applyArticle(this.responseText);

            // Add to cache (limit to 20 pages)
            pageCache.push({ path: pathWithoutAnchor, responseText: this.responseText });
            if (pageCache.length > 20) {
                pageCache.shift();
            }
        }
    };
    
    // Request the .ajax version of the page
    currentRequest.open('GET', pathWithoutAnchor + '.ajax', true);
    currentRequest.send();

    // Show loading spinner if request takes too long
    if (currentLoadingTimer !== null) {
        window.clearTimeout(currentLoadingTimer);
    }
    var showSpinnerForRequest = currentRequest;
    currentLoadingTimer = window.setTimeout(function() {
        if (spinnerShown || currentRequest !== showSpinnerForRequest)
            return;
        spinnerShown = true;
        article.innerHTML =
            '<svg xmlns="http://www.w3.org/2000/svg" width="32px" height="32px" viewBox="0 0 100 100" style="margin: 0 auto;">' +
            '<g>' +
            '  <circle cx="50" cy="50" fill="none" stroke="#dbe6e8" stroke-width="12" r="36" />' +
            '  <circle cx="50" cy="50" fill="none" stroke="#4aa5e0" stroke-width="12" r="36" stroke-dasharray="50 180" />' +
            '  <animateTransform attributeName="transform" type="rotate" repeatCount="indefinite" dur="1s" values="0 50 50;360 50 50" keyTimes="0;1" />' +
            '</g>' +
            '</svg>';
    }, 750);
}

// Highlight the current page in the sidebar
function updateSelectedItem(path) {
    if (!directory) return;

    // Collapse the previously selected page and its section links.
    var selected = directory.querySelector('.toc-page.selected');
    if (selected) {
        selected.classList.remove('selected');
    }

    // Normalize path: strip anchor and trailing slash
    var anchorPos = path.indexOf('#');
    if (anchorPos >= 0) {
        path = path.substr(0, anchorPos);
    }
    if (path.length > 1 && path.charAt(path.length - 1) === '/') {
        path = path.substr(0, path.length - 1);
    }

    // Find the matching page link and expand its enclosing TOC group.
    var links = directory.getElementsByClassName('toc-page-link');
    for (var i = 0; i < links.length; i++) {
        var href = links[i].getAttribute('href');
        if (href === path) {
            var page = links[i].parentNode;
            if (page && page.classList.contains('toc-page')) {
                // Use the section list's actual height as the transition endpoint.
                updateSelectedItemHeight(page);
                page.classList.add('selected');
                break;
            }
        }
    }
}

// Remeasures an expanded section list after layout-affecting resources change.
function updateSelectedItemHeight(page) {
    for (var i = 0; i < page.children.length; i++) {
        var child = page.children[i];
        if (child.classList.contains('toc-sections')) {
            page.style.setProperty('--toc-sections-height', child.scrollHeight + 'px');
            break;
        }
    }
}

// Intercept clicks on internal links to use AJAX loading
function replaceLinks(root) {
    var links = root.getElementsByTagName('a');
    for (var i = 0; i < links.length; i++) {
        var a = links[i];
        var href = a.getAttribute('href');
        if (href && ((href === '/docs') || (href.substr(0, 6) === '/docs/') ||
                     (href.substr(0, 6) === '/docs#'))) {
            a.onclick = function() {
                savePageState();
                navigateTo(this.getAttribute('href'), true, 0);
                cancelPopupMenu();
                return false; // Prevent default navigation
            };
        }
    }
}

//------------------------------------
// Initialization
//------------------------------------

document.addEventListener('DOMContentLoaded', function() {
    directory = document.querySelector('.directory');
    article = document.getElementById('article');

    // Disable browser's automatic scroll restoration (we handle it manually)
    if ('scrollRestoration' in history) {
        history.scrollRestoration = 'manual';
    }

    // Set up AJAX links in sidebar and article
    if (directory) {
        replaceLinks(directory);
        updateSelectedItem(location.pathname);

        // Correct the initial measurement after the sidebar web fonts finish loading.
        if (document.fonts) {
            document.fonts.ready.then(function() {
                var selected = directory.querySelector('.toc-page.selected');
                if (selected) {
                    updateSelectedItemHeight(selected);
                }
            });
        }
    }
    if (article) {
        replaceLinks(article);

        // Scroll explicitly on a full-page load too. Manual history restoration and late layout
        // changes can otherwise prevent the browser's native fragment scroll from taking effect.
        if (location.hash !== '') {
            scrollToAnchor(location.hash.substr(1), false);
        }
    }
});

// Save scroll position on scroll
window.onscroll = function() {
    savePageState();
};

// Handle back/forward navigation
window.addEventListener('popstate', function(evt) {
    if (evt.state) {
        cancelPopupMenu();
        navigateTo(evt.state.path, false, evt.state.pageYOffset);
    }
});
