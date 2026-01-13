// doc-viewer.js - Documentation-specific interactive features
// Features:
//   1. Collapsible sidebar table of contents
//   2. AJAX page loading with caching and history management
// Note: Common features (theme toggle, hamburger menu) are in common.js

var directory = null;
var article = null;

// AJAX page loading state
var pageCache = [];
var currentRequest = null;
var currentLoadingTimer = null;
var spinnerShown = false;

// Cleanup handler after CSS height transition ends
function onEndTransition(evt) {
    this.removeEventListener('transitionend', onEndTransition);
    this.style.removeProperty('display');
    this.style.removeProperty('transition');
    this.style.removeProperty('height');
}

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

// Navigate to a new page using AJAX
function navigateTo(dstPath, forward, pageYOffset) {
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

    // Extract anchor from path (e.g., /docs/intro#section -> anchor = "section")
    var anchorPos = dstPath.indexOf('#');
    var anchor = (anchorPos >= 0) ? dstPath.substr(anchorPos + 1) : '';
    var pathWithoutAnchor = (anchorPos >= 0) ? dstPath.substr(0, anchorPos) : dstPath;

    // Function to apply loaded content to the page
    var applyArticle = function(responseText) {
        // First line is the title, rest is content
        var n = responseText.indexOf('\n');
        document.title = responseText.substr(0, n);
        article.innerHTML = responseText.substr(n + 1);

        // Set up AJAX links in the new content
        replaceLinks(article);

        // Scroll to anchor or restore scroll position
        if (forward && anchor !== '') {
            var anchorElement = document.getElementById(anchor);
            if (anchorElement) {
                anchorElement.scrollIntoView();
            }
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

// Check if a point is inside a rectangle
function rectContains(rect, x, y) {
    return rect.left <= x && x < rect.right && rect.top <= y && y < rect.bottom;
}

// Highlight the current page in the sidebar
function updateSelectedItem(path) {
    if (!directory) return;

    // Remove existing selection
    var selected = directory.querySelector('li.selected');
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

    // Find matching link in sidebar and select its <li>
    var links = directory.getElementsByTagName('a');
    for (var i = 0; i < links.length; i++) {
        var href = links[i].getAttribute('href');
        if (href === path) {
            var li = links[i].querySelector('li');
            if (li) {
                li.classList.add('selected');
            }
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
        if (href && href.substr(0, 6) === '/docs/') {
            a.onclick = function(evt) {
                // Check if this link contains a caret (collapsible TOC entry)
                var caretSpan = this.querySelector('span.caret');
                if (caretSpan) {
                    // Check if click was on the caret arrow area
                    var spanRect = caretSpan.getBoundingClientRect();
                    var inflate = 8;
                    // The caret ::before is positioned at left: -19px, and is about 11px wide
                    var caretRect = {
                        left: spanRect.left - 19 - inflate,
                        top: spanRect.top - inflate,
                        right: spanRect.left - 8 + inflate,
                        bottom: spanRect.bottom + inflate
                    };
                    if (rectContains(caretRect, evt.clientX, evt.clientY)) {
                        // Click was on caret - toggle expand/collapse only
                        var childList = this.nextElementSibling;
                        if (childList && childList.tagName === 'UL') {
                            if (caretSpan.classList.contains('caret-down')) {
                                // Collapse
                                caretSpan.classList.remove('caret-down');
                                childList.classList.remove('active');
                            } else {
                                // Expand
                                caretSpan.classList.add('caret-down');
                                childList.classList.add('active');
                            }
                        }
                        return false; // Don't navigate
                    }
                }
                // Click was on text - expand if not already expanded, then navigate
                var childList = this.nextElementSibling;
                if (childList && childList.tagName === 'UL') {
                    if (!caretSpan.classList.contains('caret-down')) {
                        caretSpan.classList.add('caret-down');
                        childList.classList.add('active');
                    }
                }
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
    }
    if (article) {
        replaceLinks(article);
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
