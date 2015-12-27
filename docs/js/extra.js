var nodemcu = nodemcu || {};
(function () {
  'use strict';
  var languageCodeToNameMap = {EN: 'English', DE: 'Deutsch'};
  var languageNames = values(languageCodeToNameMap);
  var defaultLanguageCode = 'EN';

  $(document).ready(function () {
    hideNavigationForAllButSelectedLanguage();
  });

  function hideNavigationForAllButSelectedLanguage() {
    // URL is like http://host/EN/build/ -> extract 'EN'
    var selectedLanguageCode = window.location.pathname.substr(1, 2);
    if (!selectedLanguageCode) {
      selectedLanguageCode = defaultLanguageCode;
    }
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