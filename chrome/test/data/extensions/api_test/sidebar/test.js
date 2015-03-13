// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

chrome.test.runTests([
  function testShowSetsStateToActive(id, callback) {
    chrome.sidebar.getState({tabId: id}, pass(function(state) {
      assertEq('hidden', state);
      chrome.sidebar.show({tabId: id});
      chrome.sidebar.getState({tabId: id}, function(state) {
        assertEq('active', state);
      });
    }));
  }
]);
