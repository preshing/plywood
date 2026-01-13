// common.js - Shared interactive features for all pages
// Features:
//   1. Theme toggle (dark/light mode)
//   2. Mobile hamburger menu with popup animation

var menuShown = null; // { button, menu, clickHandler }

//------------------------------------
// Popup Menu Animation
//------------------------------------

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

//------------------------------------
// Initialization
//------------------------------------

document.addEventListener('DOMContentLoaded', function() {
    injectKeyframeAnimation();

    // Set up theme toggle buttons
    var toggles = document.querySelectorAll('.theme-toggle');
    toggles.forEach(function(toggle) {
        toggle.addEventListener('click', function() {
            var currentTheme = document.documentElement.getAttribute('data-theme');
            var newTheme = currentTheme === 'dark' ? 'light' : 'dark';
            if (newTheme === 'light') {
                document.documentElement.removeAttribute('data-theme');
            } else {
                document.documentElement.setAttribute('data-theme', 'dark');
            }
            localStorage.setItem('theme', newTheme);
        });
    });

    // Set up mobile hamburger menu
    var hamburger = document.getElementById('hamburger');
    var directory = document.querySelector('.directory');
    if (hamburger && directory) {
        hamburger.addEventListener('click', function() {
            togglePopupMenu(hamburger, directory);
        });
        hamburger.addEventListener('mouseover', function() {
            this.classList.add('highlight');
        });
        hamburger.addEventListener('mouseout', function() {
            this.classList.remove('highlight');
        });
    }
});

// Close popup menu when window is resized and hamburger becomes hidden
window.addEventListener('resize', function() {
    var hamburger = document.getElementById('hamburger');
    if (hamburger && window.getComputedStyle(hamburger).display === 'none') {
        cancelPopupMenu();
    }
});
