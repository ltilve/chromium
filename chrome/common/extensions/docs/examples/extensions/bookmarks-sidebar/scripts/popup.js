function visit(b,d) {
  if (b) {
    if (b.url) {
      d.append('<li><a href="' + b.url + '">' + b.title + '</a></li>');
    }
    if (b.children) {
      for(var ix = 0; ix < b.children.length; ++ix) {
        visit(b.children[ix],d);
      }
    }
  }
}

function makeBookmarksList(bookmarks_tree) {
  var root = $("#bookmarks");
  visit(bookmarks_tree[0],root);
}

$(document).ready(
  function() {
    chrome.bookmarks.getTree(makeBookmarksList);
    $("#show-sidebar-button").click(
      function(s) {
        chrome.sidebar.show({sidebar:"html/sidebar.html"});
      });
    $("#hide-sidebar-button").click(
      function(s) {
        chrome.sidebar.hide();
      });
    $('body').on('click', 'a', function(){
      chrome.tabs.update({url: $(this).attr('href')});
      return false;
    });
  });
