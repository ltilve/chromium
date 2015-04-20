function visit(b,d) {
  if (b) {
    if (b.url) {
      d.innerHTML += ('<li><a href="' + b.url + '">' + b.title + '</a></li>');
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
}

document.addEventListener("DOMContentLoaded", function(event) {
  chrome.bookmarks.getTree(makeBookmarksList);
  document.querySelector("#show-sidebar-button").addEventListener('click',
    function(s) {
      chrome.sidebar.show({sidebar:"html/sidebar.html"});
    });
  document.querySelector("#hide-sidebar-button").addEventListener('click',
    function(s) {
      chrome.sidebar.hide();
    });
  document.querySelector('body').addEventListener('click',
    function(event) {
      if (event.target.tagName.toLowerCase() === 'a') {
        chrome.tabs.update({url: event.target.href});
        return false;
      }
    });
  });