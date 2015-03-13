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
  },
  // state result was changed: should return
  // an object with 'shown' and 'pinned' members
  function testGetStateReturnsAnObject(id) {
    chrome.sidebar.getState({tabId: id},
                            pass(function(state) {
                              assertEq('object', typeof state, "getState should no longer return a string");
                            }));
  },
  // ensure getState returns shown
  function testGetStateReturnsShown(id) {
    chrome.sidebar.getState({tabId: id},
                            pass(function(state) {
                              assertFalse(state.shown === undefined, "getState should include 'shown'");
                            }));
  },
  // ensure getState returns pinned
  function testGetStateReturnsPinned(id) {
    chrome.sidebar.getState({tabId: id},
                            pass(function(state) {
                              assertFalse(state.pinned === undefined, "getState should include 'pinned'");
                            }));
  },
  // navigate was removed from the API
  function testNavigateIsUndefined(id) {
    assertTrue(chrome.sidebar.navigate === undefined, 'chrome.sidebar.navigate should not be defined');
    chrome.sidebar.getState(null, pass(function(state){}));
  },
  // setIcon was removed from the API
  function testSetIconIsUndefined(id) {
    assertTrue(chrome.sidebar.setIcon === undefined, 'chrome.sidebar.setIcon should not be defined');
    chrome.sidebar.getState(null, pass(function(state){}));
  },
  // setTitle was removed from the API
  function testSetTitleIsUndefined(id) {
    assertTrue(chrome.sidebar.setTitle === undefined, 'chrome.sidebar.setTitle should not be defined');
    chrome.sidebar.getState(null, pass(function(state){}));
  },
  // test that navigation works via show
  function testCanNavigateViaShow(id) {
    chrome.sidebar.show({tabId: id, sidebar:"./simple_page.html"});
    chrome.sidebar.getState({tabId:id},
                            pass(function(state) {
                              assertEq('active', state);
                              // TODO check we're on the right page
                            }));
  },
  // test that width works in show
  function testWidthWorksInShow(id) {
    chrome.sidebar.show({tabId: id, sidebar:"./simple_page.html", width: "100px"});
    chrome.sidebar.getState({tabId:id},
                            pass(function(state) {
                              assertEq('active', state);
                              // TODO check the contents is properly sized
                            }));
  },
  // test that onStateChanged gets called when tab state
  // changes; ensure returned details include tabId and state
  function testOnStateChangedGetsCalledOnStateChange(id) {
    // TODO write this test
    assertTrue(false, "unimplemented");
  }
]);
