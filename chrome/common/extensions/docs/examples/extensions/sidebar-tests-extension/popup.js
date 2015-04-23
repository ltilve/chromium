// Copyright (c) 2015 Igalia. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var counter = 0;

function init() {
  document.getElementById("add").onclick = function () {
    counter += 1;
    document.getElementById("clicks").innerHTML = counter;
  }

  document.getElementById("close").onclick = function () {
    console.log("window.close requested");
    window.close();
  }

  chrome.browserAction.openPopup(function(chromeWindow) {
    console.log(chromeWindow);
  });
}

function getFirstCpuLoad(callback) {
  chrome.system.cpu.getInfo(function(cpuinfo) {
    var load = cpuinfo.processors[0].usage.user / cpuinfo.processors[0].usage.idle;
    callback(load);
  });
}

function renderStatus(statusText) {
  document.getElementById('status').textContent = statusText;
}

document.addEventListener('DOMContentLoaded', function() {
  var seconds = 5;
  // Looping each 0.5s to refresh the info
  window.setInterval(function() {
    getFirstCpuLoad(function(cpuinfo) {
      renderStatus('CPU#1 load = ' + cpuinfo.toFixed(5));
    });
    seconds -= 1;
    chrome.browserAction.setBadgeText({text: String(seconds)});
  }, 1000);
  // Popup self closed after 5s
  window.setInterval(function() {
    chrome.browserAction.setBadgeText({text: ""});
    window.close();
  }, 5000);
});

/* chrome.browserAction.onClicked.addListener(function(tab) {
    console.log(" broserAction clicked" );
    chrome.browserAction.openPopup(function(chromeWindow) {
      console.log(chromeWindow);
    });
}); */

document.addEventListener('DOMContentLoaded', init);
