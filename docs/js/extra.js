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
   */
  function addLanguageSelectorToRtdFlyOutMenu() {
    var flyOutWrapper = $('.rst-other-versions .injected');
    // only works on RTD
    if (flyOutWrapper.length > 0) {
      var selectedLanguageCode = determineSelectedLanguageCode();
      var dl = document.createElement('dl');
      var dt = document.createElement('dt');
      dl.appendChild(dt);
      dt.appendChild(document.createTextNode('Languages'));
      for (var languageCode in languageCodeToNameMap) {
        dl.appendChild(createLanguageLinkFor(languageCode, selectedLanguageCode === languageCode));
      }
      flyOutWrapper.prepend(dl);
    }
  }
  function createLanguageLinkFor(languageCode, isCurrentlySelected) {
    var strong;
    var pathSegments = window.location.pathname.split('/');
    var dd = document.createElement("dd");
    var href = document.createElement("href");
    href.setAttribute('a', '/' + pathSegments[0] + '/' + pathSegments[1] + '/' + languageCode);
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
      selectedLanguageCode = path.split('/')[2];
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