// doc2.js - Mobile dropdown menu for documentation table of contents
// This script makes the hamburger button (#three-lines) show the sidebar (.directory)
// as a popup menu when on narrow screens (mobile).

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

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', function() {
    injectKeyframeAnimation();
    directory = document.querySelector('.directory');

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
