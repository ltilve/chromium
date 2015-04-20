function visit(b,d) {
  if (b) {
    if (b.url) {
      d.innerHTML += ('<li><a href="' + b.url + '"><button>' + b.title + '</button></a></li>');
    }
    if (b.children) {
      for(var ix = 0; ix < b.children.length; ++ix) {
        visit(b.children[ix],d);
      }
    }
  }
}

function makeBookmarksList(bookmarks_tree) {
  var root = document.querySelector("#bookmarks");
  visit(bookmarks_tree[0],root);
  document.querySelector('body').addEventListener('click',
    function(event) {
      if (event.target.tagName.toLowerCase() === 'a') {
      chrome.runtime.sendMessage({type: "navigate", url: event.target.href});
    }
  });

}

function refreshBookmarksList() {
  document.querySelector("#bookmarks").hide();
  document.querySelector("#bookmarks").empty();
  chrome.runtime.sendMessage({"type":"bookmarks"},
                             function(response) {
                               makeBookmarksList(response);
                             });
  document.querySelector("#bookmarks").show();
}

document.addEventListener("DOMContentLoaded", function(event) {

    chrome.runtime.sendMessage({"type":"bookmarks"},
                               function(response) {
                                 makeBookmarksList(response);
                               });
    document.querySelector("#hide-sidebar-button").addEventListener('click',
      function(s) {
        chrome.runtime.sendMessage({"type":"hide"});
      });
    chrome.bookmarks.onCreated.addListener(function (id, node) {
      refreshBookmarksList();
    });
  });
