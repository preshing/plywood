// doc-viewer.js - Interactive features for documentation pages
// Features:
//   1. Mobile hamburger menu (#hamburger) shows sidebar as popup
//   2. Collapsible sidebar table of contents
//   3. AJAX page loading with caching and history management

var directory = null;
var article = null;
var menuShown = null; // { button, menu, clickHandler }

// AJAX page loading state
var pageCache = [];
var currentRequest = null;
var currentLoadingTimer = null;
var spinnerShown = false;

// Inject CSS keyframe animations for the popup menu scale effect
function injectKeyframeAnimation() {
    var animation = '';
    var inverseAnimation = '';
    for (var step = 0; step <= 100; step++) {
        var f = 1 - Math.pow(1 - step / 100.0, 4); // ease-out curve
        var xScale = 0.5 + 0.5 * f;
        var yScale = 0.01 + 0.99 * f;
        animation += step + '% { transform: scale(' + xScale + ', ' + yScale + '); }\n';
        inverseAnimation += step + '% { transform: scale(' + (1.0 / xScale) + ', ' + (1.0 / yScale) + '); }\n';
    }
    var style = document.createElement('style');
    style.textContent = '@keyframes menuAnimation {\n' + animation + '}\n\n' +
        '@keyframes menuContentsAnimation {\n' + inverseAnimation + '}\n\n';
    document.head.appendChild(style);
}

// Close any open popup menu
function cancelPopupMenu() {
    if (menuShown) {
        menuShown.menu.firstElementChild.classList.remove('expanded-content');
        menuShown.menu.classList.remove('expanded');
        document.removeEventListener('click', menuShown.clickHandler);
        menuShown = null;
    }
}

// Toggle a popup menu open/closed
function togglePopupMenu(button, menu) {
    var mustShow = !menuShown || (menuShown.menu !== menu);
    cancelPopupMenu();

    if (mustShow) {
        menu.classList.add('expanded');
        menu.firstElementChild.classList.add('expanded-content');

        // Safari needs this to force animation to replay
        menu.style.animation = 'none';
        menu.firstElementChild.style.animation = 'none';
        void menu.offsetHeight; // triggers reflow
        menu.style.animation = '';
        menu.firstElementChild.style.animation = '';

        // Register click handler to close when clicking outside
        var clickHandler = function(evt) {
            if (menuShown && menuShown.menu === menu) {
                if (!menuShown.button.contains(evt.target) && !menuShown.menu.contains(evt.target)) {
                    cancelPopupMenu();
                }
            }
        };
        menuShown = {
            button: button,
            menu: menu,
            clickHandler: clickHandler
        };
        document.addEventListener('click', clickHandler);
    }

    return mustShow;
}

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
    injectKeyframeAnimation();
    directory = document.querySelector('.directory');
    article = document.getElementById('article');

    // Disable browser's automatic scroll restoration (we handle it manually)
    if ('scrollRestoration' in history) {
        history.scrollRestoration = 'manual';
    }

    // Set up mobile hamburger menu
    var threeLines = document.getElementById('hamburger');
    if (threeLines && directory) {
        threeLines.addEventListener('click', function() {
            togglePopupMenu(threeLines, directory);
        });
        threeLines.addEventListener('mouseover', function() {
            this.classList.add('highlight');
        });
        threeLines.addEventListener('mouseout', function() {
            this.classList.remove('highlight');
        });
    }

    // Set up AJAX links in sidebar and article
    if (directory) {
        replaceLinks(directory);
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
