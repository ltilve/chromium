// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class implements the functionality that is specific to desktop
 * remoting ("Chromoting" or CRD).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.ApplicationInterface}
 * @extends {remoting.Application}
 */
remoting.DesktopRemoting = function() {
  base.inherits(this, remoting.Application);

  /** @private {remoting.Activity} */
  this.activity_ = null;
};

/**
 * @return {string} Application product name to be used in UI.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.getApplicationName = function() {
  return chrome.i18n.getMessage(/*i18n-content*/'PRODUCT_NAME');
};

/**
 * @param {!remoting.Error} error The failure reason.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.signInFailed_ = function(error) {
  remoting.showErrorMessage(error);
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.initApplication_ = function() {
  remoting.initElementEventHandlers();

  if (base.isAppsV2()) {
    remoting.windowFrame = new remoting.WindowFrame(
        document.getElementById('title-bar'), this.disconnect_.bind(this));
    remoting.optionsMenu = remoting.windowFrame.createOptionsMenu();

    var START_FULLSCREEN = 'start-fullscreen';
    remoting.fullscreen = new remoting.FullscreenAppsV2();
    remoting.fullscreen.addListener(function(isFullscreen) {
      chrome.storage.local.set({START_FULLSCREEN: isFullscreen});
    });
    // TODO(jamiewalch): This should be handled by the background page when the
    // window is created, but due to crbug.com/51587 it needs to be done here.
    // Remove this hack once that bug is fixed.
    chrome.storage.local.get(
        START_FULLSCREEN,
        /** @param {Object} values */
        function(values) {
          if (values[START_FULLSCREEN]) {
            remoting.fullscreen.activate(true);
          }
        }
    );

  } else {
    remoting.fullscreen = new remoting.FullscreenAppsV1();
    remoting.toolbar = new remoting.Toolbar(
        document.getElementById('session-toolbar'),
        this.disconnect_.bind(this));
    remoting.optionsMenu = remoting.toolbar.createOptionsMenu();

    window.addEventListener('beforeunload',
                            this.promptClose_.bind(this), false);
    window.addEventListener('unload', this.disconnect_.bind(this), false);
  }

  remoting.initHostlist_(this.connectMe2Me_.bind(this));
  document.getElementById('access-mode-button').addEventListener(
      'click', this.connectIt2Me_.bind(this), false);

  var homeFeedback = new remoting.MenuButton(
      document.getElementById('help-feedback-main'));
  var toolbarFeedback = new remoting.MenuButton(
      document.getElementById('help-feedback-toolbar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('title-bar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('help-feedback-toolbar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('help-feedback-main'));

  remoting.windowShape.updateClientWindowShape();

  remoting.showOrHideIT2MeUi();
  remoting.showOrHideMe2MeUi();

  // For Apps v1, check the tab type to warn the user if they are not getting
  // the best keyboard experience.
  if (!base.isAppsV2() && !remoting.platformIsMac()) {
    /** @param {boolean} isWindowed */
    var onIsWindowed = function(isWindowed) {
      if (!isWindowed) {
        document.getElementById('startup-mode-box-me2me').hidden = false;
        document.getElementById('startup-mode-box-it2me').hidden = false;
      }
    };
    this.isWindowed_(onIsWindowed);
  }

  remoting.ClientPlugin.factory.preloadPlugin();
};

/**
 * @param {string} token An OAuth access token.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.startApplication_ = function(token) {
  remoting.identity.getEmail().then(
      function(/** string */ email) {
        document.getElementById('current-email').innerText = email;
        document.getElementById('get-started-it2me').disabled = false;
        document.getElementById('get-started-me2me').disabled = false;
      });
};

/** @override {remoting.ApplicationInterface} */
remoting.DesktopRemoting.prototype.exitApplication_ = function() {
  this.closeMainWindow_();
};

/**
 * Determine whether or not the app is running in a window.
 * @param {function(boolean):void} callback Callback to receive whether or not
 *     the current tab is running in windowed mode.
 * @private
 */
remoting.DesktopRemoting.prototype.isWindowed_ = function(callback) {
  /** @param {chrome.Window} win The current window. */
  var windowCallback = function(win) {
    callback(win.type == 'popup');
  };
  /** @param {chrome.Tab} tab The current tab. */
  var tabCallback = function(tab) {
    if (tab.pinned) {
      callback(false);
    } else {
      chrome.windows.get(tab.windowId, null, windowCallback);
    }
  };
  if (chrome.tabs) {
    chrome.tabs.getCurrent(tabCallback);
  } else {
    console.error('chome.tabs is not available.');
  }
}

/**
 * If an IT2Me client or host is active then prompt the user before closing.
 * If a Me2Me client is active then don't bother, since closing the window is
 * the more intuitive way to end a Me2Me session, and re-connecting is easy.
 * @private
 */
remoting.DesktopRemoting.prototype.promptClose_ = function() {
  if (this.getConnectionMode() === remoting.Application.Mode.IT2ME) {
    switch (remoting.currentMode) {
      case remoting.AppMode.CLIENT_CONNECTING:
      case remoting.AppMode.HOST_WAITING_FOR_CODE:
      case remoting.AppMode.HOST_WAITING_FOR_CONNECTION:
      case remoting.AppMode.HOST_SHARED:
      case remoting.AppMode.IN_SESSION:
        return chrome.i18n.getMessage(/*i18n-content*/'CLOSE_PROMPT');
      default:
        return null;
    }
  }
};

/** @returns {remoting.DesktopConnectedView} */
remoting.DesktopRemoting.prototype.getConnectedViewForTesting = function() {
  var activity = /** @type {remoting.Me2MeActivity} */ (this.activity_);
  return activity.getDesktopActivity().getConnectedView();
};

remoting.DesktopRemoting.prototype.getActivity = function() {
  return this.activity_;
};

remoting.DesktopRemoting.prototype.disconnect_ = function() {
  if (this.activity_) {
    this.activity_.stop();
  }
};

/**
 * Entry-point for Me2Me connections.
 *
 * @param {string} hostId The unique id of the host.
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopRemoting.prototype.connectMe2Me_ = function(hostId) {
  var host = remoting.hostList.getHostForId(hostId);
  base.dispose(this.activity_);
  this.activity_ = new remoting.Me2MeActivity(host);
  this.activity_.start();
};

/**
 * Entry-point for It2Me connections.
 *
 * @private
 */
remoting.DesktopRemoting.prototype.connectIt2Me_ = function() {
  base.dispose(this.activity_);
  this.activity_ = new remoting.It2MeActivity();
  this.activity_.start();
};
