// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

// Length of the various components of the access code in number of digits.
var SUPPORT_ID_LENGTH = 7;
var HOST_SECRET_LENGTH = 5;
var ACCESS_CODE_LENGTH = SUPPORT_ID_LENGTH + HOST_SECRET_LENGTH;

/**
 * @constructor
 * @implements {remoting.Activity}
 */
remoting.It2MeActivity = function() {
  /** @private */
  this.hostId_ = '';
  /** @private */
  this.passCode_ = '';

  var form = document.getElementById('access-code-form');
  /** @private */
  this.accessCodeDialog_ = new remoting.InputDialog(
    remoting.AppMode.CLIENT_UNCONNECTED,
    form,
    form.querySelector('#access-code-entry'),
    form.querySelector('#cancel-access-code-button'));

  /** @private {remoting.DesktopRemotingActivity} */
  this.desktopActivity_ = null;
};

remoting.It2MeActivity.prototype.dispose = function() {
  base.dispose(this.desktopActivity_);
  this.desktopActivity_ = null;
};

remoting.It2MeActivity.prototype.start = function() {
  var that = this;

  this.accessCodeDialog_.show().then(function(/** string */ accessCode) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    return that.verifyAccessCode_(accessCode);
  }).then(function() {
    return remoting.identity.getToken();
  }).then(function(/** string */ token) {
    return that.getHostInfo_(token);
  }).then(function(/** !remoting.Xhr.Response */ response) {
    return that.onHostInfo_(response);
  }).then(function(/** remoting.Host */ host) {
    that.connect_(host);
  }).catch(function(/** remoting.Error */ error) {
    if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
      remoting.setMode(remoting.AppMode.HOME);
    } else {
      var errorDiv = document.getElementById('connect-error-message');
      l10n.localizeElementFromTag(errorDiv, error.getTag());
      remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
    }
  });
};

remoting.It2MeActivity.prototype.stop = function() {
  this.desktopActivity_.stop();
};

/**
 * @param {!remoting.Error} error
 */
remoting.It2MeActivity.prototype.onConnectionFailed = function(error) {
  this.onError(error);
};

/**
 * @param {!remoting.ConnectionInfo} connectionInfo
 */
remoting.It2MeActivity.prototype.onConnected = function(connectionInfo) {
  this.accessCodeDialog_.inputField().value = '';
};

remoting.It2MeActivity.prototype.onDisconnected = function() {
  this.showFinishDialog_(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
};

/**
 * @param {!remoting.Error} error
 */
remoting.It2MeActivity.prototype.onError = function(error) {
  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, error.getTag());
  this.showFinishDialog_(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
};

/** @return {remoting.DesktopRemotingActivity} */
remoting.It2MeActivity.prototype.getDesktopActivityForTesting = function() {
  return this.desktopActivity_;
};

/**
 * @param {remoting.AppMode} mode
 * @private
 */
remoting.It2MeActivity.prototype.showFinishDialog_ = function(mode) {
  var finishDialog = new remoting.MessageDialog(
      mode,
      document.getElementById('client-finished-it2me-button'));
  finishDialog.show().then(function() {
    remoting.setMode(remoting.AppMode.HOME);
  });
};

/**
 * @param {string} accessCode
 * @return {Promise}  Promise that resolves if the access code is valid.
 * @private
 */
remoting.It2MeActivity.prototype.verifyAccessCode_ = function(accessCode) {
  var normalizedAccessCode = accessCode.replace(/\s/g, '');
  if (normalizedAccessCode.length !== ACCESS_CODE_LENGTH) {
    return Promise.reject(new remoting.Error(
        remoting.Error.Tag.INVALID_ACCESS_CODE));
  }

  this.hostId_ = normalizedAccessCode.substring(0, SUPPORT_ID_LENGTH);
  this.passCode_ = normalizedAccessCode;

  return Promise.resolve();
};

/**
 * Continues an IT2Me connection once an access token has been obtained.
 *
 * @param {string} token An OAuth2 access token.
 * @return {Promise<!remoting.Xhr.Response>}
 * @private
 */
remoting.It2MeActivity.prototype.getHostInfo_ = function(token) {
  var that = this;
  return new remoting.Xhr({
    method: 'GET',
    url: remoting.settings.DIRECTORY_API_BASE_URL + '/support-hosts/' +
        encodeURIComponent(that.hostId_),
    oauthToken: token
  }).start();
};

/**
 * Continues an IT2Me connection once the host JID has been looked up.
 *
 * @param {!remoting.Xhr.Response} xhrResponse The server response to the
 *     support-hosts query.
 * @return {!Promise<!remoting.Host>} Rejects on error.
 * @private
 */
remoting.It2MeActivity.prototype.onHostInfo_ = function(xhrResponse) {
  if (xhrResponse.status == 200) {
    var response = /** @type {{data: {jabberId: string, publicKey: string}}} */
        (base.jsonParseSafe(xhrResponse.getText()));
    if (response && response.data &&
        response.data.jabberId && response.data.publicKey) {
      var host = new remoting.Host(this.hostId_);
      host.jabberId = response.data.jabberId;
      host.publicKey = response.data.publicKey;
      host.hostName = response.data.jabberId.split('/')[0];
      return Promise.resolve(host);
    } else {
      console.error('Invalid "support-hosts" response from server.');
      return Promise.reject(remoting.Error.unexpected());
    }
  } else {
    return Promise.reject(translateSupportHostsError(xhrResponse.status));
  }
};

/**
 * @param {remoting.Host} host
 * @private
 */
remoting.It2MeActivity.prototype.connect_ = function(host) {
  base.dispose(this.desktopActivity_);
  this.desktopActivity_ = new remoting.DesktopRemotingActivity(this);
  var sessionConnector = remoting.SessionConnector.factory.createConnector(
      document.getElementById('client-container'),
      remoting.app_capabilities(),
      this.desktopActivity_);
  sessionConnector.connect(
      remoting.Application.Mode.IT2ME, host,
      new remoting.CredentialsProvider({ accessCode: this.passCode_ }));
};

/**
 * TODO(jrw): Replace with remoting.Error.fromHttpStatus.
 * @param {number} error An HTTP error code returned by the support-hosts
 *     endpoint.
 * @return {remoting.Error} The equivalent remoting.Error code.
 */
function translateSupportHostsError(error) {
  switch (error) {
    case 0: return new remoting.Error(remoting.Error.Tag.NETWORK_FAILURE);
    case 404: return new remoting.Error(remoting.Error.Tag.INVALID_ACCESS_CODE);
    case 502: // No break
    case 503: return new remoting.Error(remoting.Error.Tag.SERVICE_UNAVAILABLE);
    default: return remoting.Error.unexpected();
  }
}

})();
