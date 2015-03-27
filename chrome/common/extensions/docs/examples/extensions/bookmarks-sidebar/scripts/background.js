var bookmarks;

function initialize(tab) {
  if (tab) {
    var tabId = tab.id;
    chrome.browserAction.enable();
    chrome.browserAction.setBadgeText({'tabId':tabId, 'text':""+tabId});
    chrome.browserAction.setPopup({'tabId':tabId, 'popup':"./html/popup.html"});
  }
  chrome.bookmarks.getTree(function(tree) {
    bookmarks = tree;
  });
}

$(document).ready(
  function() {
    chrome.tabs.onUpdated.addListener(
      function(tabId, changeInfo, tab) {
        if (changeInfo.status === 'complete') {
          chrome.tabs.query({currentWindow:true, active:true},
                            function(tabs) {
                              initialize(tabs[0]);
                            });
        }
      });
    chrome.runtime.onMessage.addListener(
      function(request, sender, sendResponse) {
        if (request.type == "hide") {
          chrome.sidebar.hide();
          sendResponse("OK");
        } else if (request.type == "bookmarks") {
          sendResponse(bookmarks);
        } else if (request.type == "navigate") {
          chrome.tabs.update({url: request.url});
          sendResponse("OK");
        }
      });

  });
