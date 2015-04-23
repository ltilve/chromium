var matchURL = "github.com"

function initialize(tab) {
  if (tab) {
    var tabId = tab.id;
    chrome.browserAction.enable();
    chrome.browserAction.setPopup({'tabId':tabId, 'popup':"popup.html"});
  }

  if (tab.url.indexOf(matchURL) > -1) {
    chrome.browserAction.openPopup(function(chromeWindow) {
      console.log("Opening Popup for matched URL");
    })
  };
}

document.addEventListener("DOMContentLoaded", function(event) {
  chrome.tabs.onUpdated.addListener( function(tabId, changeInfo, tab) {
    if (changeInfo.status === 'complete') {
      chrome.tabs.query({currentWindow:true, active:true},
                         function(tabs) {
                           initialize(tabs[0]);
                         });
      }
  });
});
