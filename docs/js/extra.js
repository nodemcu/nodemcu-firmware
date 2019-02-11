var nodemcu = nodemcu || {};
(function () {
  'use strict';

  $(document).ready(function () {
    addToc();
    replaceRelativeLinksWithStaticGitHubUrl();
  });

  /**
   * Adds a TOC-style table to each page in the 'Modules' section.
   */
  function addToc() {
    var func, intro, tocHtmlTable;
    if (isModulePage()) {
      tocHtmlTable = '<table class="docutils">';
      $('h2').each(function (index) {
        // 'slice' cuts off the single permalink character at the end of the text (e.g. '¶')
        func = $(this).text().slice(0, -1);
        // get the first sentence of the paragraph directly below h2
        intro = $(this).next().text();
        intro = intro.substring(0, intro.indexOf('.') + 1);
        tocHtmlTable += createTocTableRow(func, intro);
      });
      tocHtmlTable += '</table>';
      $(tocHtmlTable).insertBefore($('h2').first())
    }
    function isModulePage() {
      // if the breadcrumb contains 'Modules »' it must be an API page
      return $("ul.wy-breadcrumbs li:contains('Modules »')").size() > 0;
    }
    function createTocTableRow(func, intro) {
      // fragile attempt to auto-create the in-page anchor
      // good tests: file.md,
      var href = func.replace(/[\.:\(\)]/g, '').replace(/ --|, | /g, '-');
      var link = '<a href="#' + href.toLowerCase() + '">' + func + '</a>';
      return '<tr><td>' + link + '</td><td>' + intro + '</td></tr>';
    }
  }

  /**
   * The module doc pages contain relative links to artifacts in the GitHub repository. For those links to work both
   * on GitHub (i.e. when the page is viewed on GitHub) and on RTD they are defined with a relative URL. This function
   * replaces the relative path with an absolute path based on the selected branch.
   */
  function replaceRelativeLinksWithStaticGitHubUrl() {
    if (isOnRtd()) {
      var relativePath = "../../..";
      var gitHubPath = "https://github.com/nodemcu/nodemcu-firmware/tree/" + determineSelectedBranch();
      var gitHubLinks = $("a[href^='" + relativePath + "']").each(function (index) {
        var url = $(this).attr('href');
        $(this).attr('href', url.replace(relativePath, gitHubPath));
      });
    }
  }

  /**
   * Analyzes the URL of the current page to find out what the selected GitHub branch is. It's usually
   * part of the location path. The code needs to distinguish between running MkDocs standalone
   * and docs served from RTD. If no valid branch could be determined 'dev' returned.
   *
   * @returns GitHub branch name
   */
  function determineSelectedBranch() {
    var branch = 'dev', path = window.location.pathname;
    if (isOnRtd()) {
      // path is like /en/<branch>/<lang>/build/ -> extract 'lang'
      // split[0] is an '' because the path starts with the separator
      var thirdPathSegment = path.split('/')[2];
      // 'latest' is an alias on RTD for the 'dev' branch - which is the default for 'branch' here
      if (thirdPathSegment != 'latest') {
        branch = thirdPathSegment;
      }
    }
    return branch;
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

  function isOnRtd() {
    return window.location.origin.indexOf('readthedocs') > -1;
  }
}());
