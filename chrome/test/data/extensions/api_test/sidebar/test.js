// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

/**
 * A helper function to show sidebar. Verifies that sidebar was hidden before
 * and is shown after the call.
 * @param {id} tab id to expand sidebar for.
 * @param {function} callback Closure.
 */
function showSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    assertEq('hidden', state);
    chrome.sidebar.show({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      assertEq('shown', state);
      callback();
    });
  });
};

/**
 * A helper function to hide sidebar. Verifies that sidebar was not hidden
 * before and is hidden after the call.
 * @param {id} tab id to hide sidebar for.
 * @param {function} callback Closure.
 */
function hideSidebar(id, callback) {
  chrome.sidebar.getState({tabId: id}, function(state) {
    assertTrue('hidden' != state);
    chrome.sidebar.hide({tabId: id});
    chrome.sidebar.getState({tabId: id}, function(state) {
      assertEq('hidden', state);
      callback();
    });
  });
};

/**
 * A helper function to show sidebar for the current tab.
 * @param {function} callback Closure.
 */
function showSidebarForCurrentTab(callback) {
  chrome.tabs.query({active: true, currentWindow: true},
                    function(tabs) {
                      showSidebar(tabs[0].id, callback);
                    });
};

/**
 * A helper function to hide sidebar for the current tab.
 * @param {function} callback Closure.
 */
function hideSidebarForCurrentTab(callback) {
  hideSidebar(undefined, callback);
};

chrome.test.runTests([
  // ensure that showing the sidebar changes its
  // state from 'hidden' to 'active'
  function testShowSetsStateToActive(id) {
    chrome.sidebar.getState({tabId: id}, pass(function(state) {
      assertEq('hidden', state);
      chrome.sidebar.show({tabId: id});
      chrome.sidebar.getState({tabId: id}, pass(function(state) {
        assertEq('active', state);
      }));
    }));
  },
  // ensure that hiding the sidebar changes its
  // state from 'active' to 'hidden'
  function testHideSetsStateToHidden(id) {
    chrome.sidebar.getState({tabId: id}, pass(function(state) {
      assertEq('active',state);
      chrome.sidebar.hide({tabId: id});
      chrome.sidebar.getState({tabId: id}, pass(function(state) {
        assertEq('hidden', state);
      }));
      chrome.sidebar.hide({tabId: id});
      chrome.sidebar.getState({tabId: id}, pass(function(state) {
        assertEq('hidden', state);
      }));
    }));
  },
  // ensure that chrome does not crash when passing undefined
  // to chrome.sidebar.getState
  function testGetStateWithUndefined(id) {
    chrome.sidebar.getState(undefined, pass(function(state) {
      assertEq('hidden', state);
      chrome.sidebar.getState(null, pass(function(state) {
        assertEq('hidden', state);
        chrome.sidebar.show({tabId: id});
        chrome.sidebar.getState(null, pass(function(state) {
          assertEq('active', state);
        }));
      }));
    }));
  }
]);
