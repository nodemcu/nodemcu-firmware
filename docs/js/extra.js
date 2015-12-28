var nodemcu = nodemcu || {};
(function () {
  'use strict';
  var languageCodeToNameMap = {en: 'English', de: 'Deutsch'};
  var languageNames = values(languageCodeToNameMap);
  var defaultLanguageCode = 'en';

  $(document).ready(function () {
    hideNavigationForAllButSelectedLanguage();
    addLanguageSelectorToRtdFlyOutMenu();
  });

  function hideNavigationForAllButSelectedLanguage() {
    var selectedLanguageCode = determineSelectedLanguageCode();
    var selectedLanguageName = languageCodeToNameMap[selectedLanguageCode];
    // Finds all subnav elements and hides them if they're /language/ subnavs. Hence, all 'Modules' subnav elements
    // won't be hidden.
    // <ul class="subnav">
    //   <li><span>Modules</span></li>
    //   <li class="toctree-l1 ">
    //     <a class="" href="EN/modules/node/">node</a>
    //   </li>
    $('.subnav li span').not(':contains(' + selectedLanguageName + ')').each(function (index) {
      var spanElement = $(this);
      if ($.inArray(spanElement.text(), languageNames) > -1) {
        spanElement.parent().parent().hide();
      }
    });
  }

  /**
   * Adds a language selector to the RTD fly out menu found bottom left. Example
   * <dl>
   *   <dt>Languages</dt>
   *   <dd><a href="http://nodemcu.readthedocs.org/en/<branch>/de/">de</a></dd>
   *   <strong>
   *     <dd><a href="http://nodemcu.readthedocs.org/en/<branch>/en/">en</a></dd>
   *   </strong>
   * </dl>
   *
   * UGLY. That menu is added by RTD with an AJAX call after page load. Hence, we need to react to
   * the subsequent DOM manipulation using a DOM4 MutationObserver.
   */
  function addLanguageSelectorToRtdFlyOutMenu() {
    var observer, observerConfig = {childList: true};
    var flyOutWrapper = $('.rst-other-versions');

    // Create an observer instance
    observer = new MutationObserver(function (mutations) {
      mutations.forEach(function (mutation) {
          debugger;
        var newNodes = mutation.addedNodes; // DOM NodeList
        if (newNodes !== null) { // If there are new nodes added
          var $nodes = $(newNodes); // jQuery set
          $nodes.each(function () {
            var $node = $(this);

          });
          var selectedLanguageCode = determineSelectedLanguageCode();
          var dl = document.createElement('dl');
          var dt = document.createElement('dt');
          dl.appendChild(dt);
          dt.appendChild(document.createTextNode('Languages'));
          for (var languageCode in languageCodeToNameMap) {
            dl.appendChild(createLanguageLinkFor(languageCode, selectedLanguageCode === languageCode));
          }
          flyOutWrapper.prepend(dl);
          // Later, you can stop observing
          observer.disconnect();
        }
      });
    });

    // Pass in the target node, as well as the observer options
    observer.observe(flyOutWrapper[0], observerConfig);
  }
  function createLanguageLinkFor(languageCode, isCurrentlySelected) {
    var strong;
    // split[0] is an '' because the path starts with the separator
    var pathSegments = window.location.pathname.split('/');
    var dd = document.createElement("dd");
    var href = document.createElement("a");
    href.setAttribute('href', '/' + pathSegments[1] + '/' + pathSegments[2] + '/' + languageCode);
    href.appendChild(document.createTextNode(languageCode));
    dd.appendChild(href);
    if (isCurrentlySelected) {
      strong = document.createElement("strong");
      strong.appendChild(dd);
      return strong;
    } else {
      return dd;
    }
  }

  function determineSelectedLanguageCode() {
    var selectedLanguageCode, path = window.location.pathname;
    if (window.location.origin.indexOf('readthedocs') > -1) {
      // path is like /en/<branch>/<lang>/build/ -> extract 'lang'
      // split[0] is an '' because the path starts with the separator
      selectedLanguageCode = path.split('/')[3];
    } else {
      // path is like /<lang>/build/ -> extract 'lang'
      selectedLanguageCode = path.substr(1, 2);
    }
    if (!selectedLanguageCode || selectedLanguageCode.length > 2) {
      selectedLanguageCode = defaultLanguageCode;
    }
    return selectedLanguageCode;
  }

  function values(associativeArray) {
    var values = [];
    for (var key in associativeArray) {
      if (associativeArray.hasOwnProperty(key)) {
        values.push(associativeArray[key]);
      }
    }
    return values;
  }
}());