function visit(b,d) {
  if (b) {
    if (b.url) {
      d.append('<li><a href="' + b.url + '"><button>' + b.title + '</button></a></li>');
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
  $('body').on('click', 'a', function(){
    chrome.runtime.sendMessage({type: "navigate", url: $(this).attr('href')});
  });

}

function refreshBookmarksList() {
  $("#bookmarks").hide();
  $("#bookmarks").empty();
  chrome.runtime.sendMessage({"type":"bookmarks"},
                             function(response) {
                               makeBookmarksList(response);
                             });
  $("#bookmarks").show();
}

$(document).ready(
  function() {
    chrome.runtime.sendMessage({"type":"bookmarks"},
                               function(response) {
                                 makeBookmarksList(response);
                               });
    $("#hide-sidebar-button").click(
      function(s) {
        chrome.runtime.sendMessage({"type":"hide"});
      });
    chrome.bookmarks.onCreated.addListener(function (id, node) {
      refreshBookmarksList();
    });
  });
