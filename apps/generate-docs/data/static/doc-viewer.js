// doc-viewer.js - Interactive features for documentation pages
// Features:
//   1. Mobile hamburger menu (#three-lines) shows sidebar as popup
//   2. Collapsible sidebar table of contents

var directory = null;
var menuShown = null; // { button, menu, clickHandler }

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

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', function() {
    injectKeyframeAnimation();
    directory = document.querySelector('.directory');

    // Set up collapsible sidebar TOC
    var carets = document.getElementsByClassName('caret');
    for (var i = 0; i < carets.length; i++) {
        carets[i].addEventListener('click', function() {
            var childList = this.nextElementSibling;
            if (this.classList.contains('caret-down')) {
                // Collapse
                this.classList.remove('caret-down');
                childList.style.display = 'block';
                childList.style.height = childList.scrollHeight + 'px';
                childList.classList.remove('active');
                requestAnimationFrame(function() {
                    childList.style.transition = 'height 0.15s ease-out';
                    requestAnimationFrame(function() {
                        childList.style.height = '0px';
                        childList.addEventListener('transitionend', onEndTransition);
                    });
                });
            } else {
                // Expand
                this.classList.add('caret-down');
                childList.style.display = 'block';
                childList.style.transition = 'height 0.15s ease-out';
                childList.style.height = childList.scrollHeight + 'px';
                childList.classList.add('active');
                childList.addEventListener('transitionend', onEndTransition);
            }
        });
    }

    var threeLines = document.getElementById('three-lines');
    if (threeLines && directory) {
        // Click to toggle the directory popup
        threeLines.addEventListener('click', function() {
            togglePopupMenu(threeLines, directory);
        });

        // Hover effects for the hamburger button
        threeLines.addEventListener('mouseover', function() {
            this.firstElementChild.classList.add('highlight');
        });
        threeLines.addEventListener('mouseout', function() {
            this.firstElementChild.classList.remove('highlight');
        });
    }
});
