// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling creation and teardown of a remoting client session.
 *
 * The ClientSession class controls lifetime of the client plugin
 * object and provides the plugin with the functionality it needs to
 * establish connection. Specifically it:
 *  - Delivers incoming/outgoing signaling messages,
 *  - Adjusts plugin size and position when destop resolution changes,
 *
 * This class should not access the plugin directly, instead it should
 * do it through ClientPlugin class which abstracts plugin version
 * differences.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ClientPlugin} plugin
 * @param {remoting.Host} host The host to connect to.
 * @param {remoting.SignalStrategy} signalStrategy Signal strategy.
 *
 * @constructor
 * @extends {base.EventSourceImpl}
 * @implements {base.Disposable}
 * @implements {remoting.ClientPlugin.ConnectionEventHandler}
 */
remoting.ClientSession = function(plugin, host, signalStrategy) {
  base.inherits(this, base.EventSourceImpl);

  /** @private */
  this.state_ = remoting.ClientSession.State.INITIALIZING;

  /** @private {!remoting.Error} */
  this.error_ = remoting.Error.none();

  /** @private */
  this.host_ = host;

  /** @private */
  this.sessionId_ = '';

  /** @private */
  this.hasReceivedFrame_ = false;
  this.logToServer = new remoting.LogToServer(signalStrategy);

  /** @private */
  this.signalStrategy_ = signalStrategy;
  base.debug.assert(this.signalStrategy_.getState() ==
                    remoting.SignalStrategy.State.CONNECTED);
  this.signalStrategy_.setIncomingStanzaCallback(
      this.onIncomingMessage_.bind(this));
  remoting.formatIq.setJids(this.signalStrategy_.getJid(), host.jabberId);

  /**
   * Allow host-offline error reporting to be suppressed in situations where it
   * would not be useful, for example, when using a cached host JID.
   *
   * @type {boolean} @private
   */
  this.logHostOfflineErrors_ = true;

  /** @private {remoting.ClientPlugin} */
  this.plugin_ = plugin;
  plugin.setConnectionEventHandler(this);

  /** @private  */
  this.connectedDisposables_ = new base.Disposables();

  this.defineEvents(Object.keys(remoting.ClientSession.Events));
};

/** @enum {string} */
remoting.ClientSession.Events = {
  stateChanged: 'stateChanged', // deprecated.
  videoChannelStateChanged: 'videoChannelStateChanged',
};

/**
 * @interface
 * [START]-------> [onConnected] ------> [onDisconnected]
 *    |                              |
 *    |-----> [OnConnectionFailed]   |----> [onError]
 *
 * TODO(kelvinp): Route session state changes through this interface.
 */
remoting.ClientSession.EventHandler = function() {};

/**
 * Called when the connection failed before it is connected.
 *
 * @param {!remoting.Error} error
 */
remoting.ClientSession.EventHandler.prototype.onConnectionFailed =
    function(error) {};

/**
 * Called when a new session has been connected.  The |connectionInfo| will be
 * valid until onDisconnected() or onError() is called.
 *
 * @param {!remoting.ConnectionInfo} connectionInfo
 */
remoting.ClientSession.EventHandler.prototype.onConnected =
    function(connectionInfo) {};

/**
 * Called when the current session has been disconnected.
 */
remoting.ClientSession.EventHandler.prototype.onDisconnected = function() {};

/**
 * Called when an error needs to be displayed to the user.
 * @param {!remoting.Error} error
 */
remoting.ClientSession.EventHandler.prototype.onError = function(error) {};

// Note that the positive values in both of these enums are copied directly
// from connection_to_host.h and must be kept in sync. Code in
// chromoting_instance.cc converts the C++ enums into strings that must match
// the names given here.
// The negative values represent state transitions that occur within the
// web-app that have no corresponding plugin state transition.
/** @enum {number} */
remoting.ClientSession.State = {
  CONNECTION_CANCELED: -3,  // Connection closed (gracefully) before connecting.
  CONNECTION_DROPPED: -2,  // Succeeded, but subsequently closed with an error.
  CREATED: -1,
  UNKNOWN: 0,
  INITIALIZING: 1,
  CONNECTING: 2,
  // We don't currently receive AUTHENTICATED from the host - it comes through
  // as 'CONNECTING' instead.
  // TODO(garykac) Update chromoting_instance.cc to send this once we've
  // shipped a webapp release with support for AUTHENTICATED.
  AUTHENTICATED: 3,
  CONNECTED: 4,
  CLOSED: 5,
  FAILED: 6
};

/**
 * @param {string} state The state name.
 * @return {remoting.ClientSession.State} The session state enum value.
 */
remoting.ClientSession.State.fromString = function(state) {
  if (!remoting.ClientSession.State.hasOwnProperty(state)) {
    throw "Invalid ClientSession.State: " + state;
  }
  return remoting.ClientSession.State[state];
};

/**
  @param {remoting.ClientSession.State} current
  @param {remoting.ClientSession.State} previous
  @constructor
*/
remoting.ClientSession.StateEvent = function(current, previous) {
  /** @type {remoting.ClientSession.State} */
  this.previous = previous

  /** @type {remoting.ClientSession.State} */
  this.current = current;
};

/** @enum {number} */
remoting.ClientSession.ConnectionError = {
  UNKNOWN: -1,
  NONE: 0,
  HOST_IS_OFFLINE: 1,
  SESSION_REJECTED: 2,
  INCOMPATIBLE_PROTOCOL: 3,
  NETWORK_FAILURE: 4,
  HOST_OVERLOAD: 5
};

/**
 * @param {string} error The connection error name.
 * @return {remoting.ClientSession.ConnectionError} The connection error enum.
 */
remoting.ClientSession.ConnectionError.fromString = function(error) {
  if (!remoting.ClientSession.ConnectionError.hasOwnProperty(error)) {
    console.error('Unexpected ClientSession.ConnectionError string: ', error);
    return remoting.ClientSession.ConnectionError.UNKNOWN;
  }
  return remoting.ClientSession.ConnectionError[error];
}

/**
 * Type used for performance statistics collected by the plugin.
 * @constructor
 */
remoting.ClientSession.PerfStats = function() {};
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.videoBandwidth;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.videoFrameRate;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.captureLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.encodeLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.decodeLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.renderLatency;
/** @type {number} */
remoting.ClientSession.PerfStats.prototype.roundtripLatency;

// Keys for connection statistics.
remoting.ClientSession.STATS_KEY_VIDEO_BANDWIDTH = 'videoBandwidth';
remoting.ClientSession.STATS_KEY_VIDEO_FRAME_RATE = 'videoFrameRate';
remoting.ClientSession.STATS_KEY_CAPTURE_LATENCY = 'captureLatency';
remoting.ClientSession.STATS_KEY_ENCODE_LATENCY = 'encodeLatency';
remoting.ClientSession.STATS_KEY_DECODE_LATENCY = 'decodeLatency';
remoting.ClientSession.STATS_KEY_RENDER_LATENCY = 'renderLatency';
remoting.ClientSession.STATS_KEY_ROUNDTRIP_LATENCY = 'roundtripLatency';

/**
 * Set of capabilities for which hasCapability() can be used to test.
 *
 * @enum {string}
 */
remoting.ClientSession.Capability = {
  // When enabled this capability causes the client to send its screen
  // resolution to the host once connection has been established. See
  // this.plugin_.notifyClientResolution().
  SEND_INITIAL_RESOLUTION: 'sendInitialResolution',

  // Let the host know that we're interested in knowing whether or not it
  // rate limits desktop-resize requests.
  // TODO(kelvinp): This has been supported since M-29.  Currently we only have
  // <1000 users on M-29 or below. Remove this and the capability on the host.
  RATE_LIMIT_RESIZE_REQUESTS: 'rateLimitResizeRequests',

  // Indicates that host/client supports Google Drive integration, and that the
  // client should send to the host the OAuth tokens to be used by Google Drive
  // on the host.
  GOOGLE_DRIVE: "googleDrive",

  // Indicates that the client supports the video frame-recording extension.
  VIDEO_RECORDER: 'videoRecorder',

  // Indicates that the client supports 'cast'ing the video stream to a
  // cast-enabled device.
  CAST: 'casting',
};

/**
 * The set of capabilities negotiated between the client and host.
 * @type {Array<string>}
 * @private
 */
remoting.ClientSession.prototype.capabilities_ = null;

/**
 * @param {remoting.ClientSession.Capability} capability The capability to test
 *     for.
 * @return {boolean} True if the capability has been negotiated between
 *     the client and host.
 */
remoting.ClientSession.prototype.hasCapability = function(capability) {
  if (this.capabilities_ == null)
    return false;

  return this.capabilities_.indexOf(capability) > -1;
};

/**
 * Disconnect the current session with a particular |error|.  The session will
 * raise a |stateChanged| event in response to it.  The caller should then call
 * dispose() to remove and destroy the <embed> element.
 *
 * @param {!remoting.Error} error The reason for the disconnection.  Use
 *    remoting.Error.none() if there is no error.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.disconnect = function(error) {
  this.sendIq_(
      '<cli:iq ' +
          'to="' + this.host_.jabberId + '" ' +
          'type="set" ' +
          'id="session-terminate" ' +
          'xmlns:cli="jabber:client">' +
        '<jingle ' +
            'xmlns="urn:xmpp:jingle:1" ' +
            'action="session-terminate" ' +
            'sid="' + this.sessionId_ + '">' +
          '<reason><success/></reason>' +
        '</jingle>' +
      '</cli:iq>');

  var state = error.isNone() ?
                  remoting.ClientSession.State.CLOSED :
                  remoting.ClientSession.State.FAILED;

  // The plugin won't send a state change notification, so we explicitly log
  // the fact that the connection has closed.
  this.logToServer.logClientSessionStateChange(state, error);
  this.error_ = error;
  this.setState_(state);
};

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.dispose = function() {
  base.dispose(this.connectedDisposables_);
  this.connectedDisposables_ = null;
  this.plugin_ = null;
};

/**
 * @return {remoting.ClientSession.State} The current state.
 */
remoting.ClientSession.prototype.getState = function() {
  return this.state_;
};

/**
 * @return {!remoting.Error} The current error code.
 */
remoting.ClientSession.prototype.getError = function() {
  return this.error_;
};

/**
 * Called when the client receives its first frame.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onFirstFrameReceived = function() {
  this.hasReceivedFrame_ = true;
};

/**
 * @return {boolean} Whether the client has received a video buffer.
 */
remoting.ClientSession.prototype.hasReceivedFrame = function() {
  return this.hasReceivedFrame_;
};

/**
 * Sends a signaling message.
 *
 * @param {string} message XML string of IQ stanza to send to server.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.sendIq_ = function(message) {
  // Extract the session id, so we can close the session later.
  var parser = new DOMParser();
  var iqNode = parser.parseFromString(message, 'text/xml').firstChild;
  var jingleNode = iqNode.firstChild;
  if (jingleNode) {
    var action = jingleNode.getAttribute('action');
    if (jingleNode.nodeName == 'jingle' && action == 'session-initiate') {
      this.sessionId_ = jingleNode.getAttribute('sid');
    }
  }

  console.log(remoting.timestamp(), remoting.formatIq.prettifySendIq(message));
  if (this.signalStrategy_.getState() !=
      remoting.SignalStrategy.State.CONNECTED) {
    console.log("Message above is dropped because signaling is not connected.");
    return;
  }

  this.signalStrategy_.sendMessage(message);
};

/**
 * @param {string} message XML string of IQ stanza to send to server.
 */
remoting.ClientSession.prototype.onOutgoingIq = function(message) {
  this.sendIq_(message);
}

/**
 * @param {string} msg
 */
remoting.ClientSession.prototype.onDebugMessage = function(msg) {
  console.log('plugin: ' + msg.trimRight());
};

/**
 * @param {Element} message
 * @private
 */
remoting.ClientSession.prototype.onIncomingMessage_ = function(message) {
  if (!this.plugin_) {
    return;
  }
  var formatted = new XMLSerializer().serializeToString(message);
  console.log(remoting.timestamp(),
              remoting.formatIq.prettifyReceiveIq(formatted));
  this.plugin_.onIncomingIq(formatted);
};

/**
 * Callback that the plugin invokes to indicate that the connection
 * status has changed.
 *
 * @param {remoting.ClientSession.State} status The plugin's status.
 * @param {remoting.ClientSession.ConnectionError} error The plugin's error
 *        state, if any.
 */
remoting.ClientSession.prototype.onConnectionStatusUpdate =
    function(status, error) {
  if (status == remoting.ClientSession.State.FAILED) {
    switch (error) {
      case remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE:
        this.error_ = new remoting.Error(
            remoting.Error.Tag.HOST_IS_OFFLINE);
        break;
      case remoting.ClientSession.ConnectionError.SESSION_REJECTED:
        this.error_ = new remoting.Error(
            remoting.Error.Tag.INVALID_ACCESS_CODE);
        break;
      case remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL:
        this.error_ = new remoting.Error(
            remoting.Error.Tag.INCOMPATIBLE_PROTOCOL);
        break;
      case remoting.ClientSession.ConnectionError.NETWORK_FAILURE:
        this.error_ = new remoting.Error(
            remoting.Error.Tag.P2P_FAILURE);
        break;
      case remoting.ClientSession.ConnectionError.HOST_OVERLOAD:
        this.error_ = new remoting.Error(
            remoting.Error.Tag.HOST_OVERLOAD);
        break;
      default:
        this.error_ = remoting.Error.unexpected();
    }
  }
  this.setState_(status);
};

/**
 * Callback that the plugin invokes to indicate that the connection type for
 * a channel has changed.
 *
 * @param {string} channel The channel name.
 * @param {string} connectionType The new connection type.
 * @private
 */
remoting.ClientSession.prototype.onRouteChanged =
    function(channel, connectionType) {
  console.log('plugin: Channel ' + channel + ' using ' +
              connectionType + ' connection.');
  this.logToServer.setConnectionType(connectionType);
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @param {boolean} ready True if the connection is ready.
 */
remoting.ClientSession.prototype.onConnectionReady = function(ready) {
  // TODO(jamiewalch): Currently, the logic for determining whether or not the
  // connection is available is based solely on whether or not any video frames
  // have been received recently. which leads to poor UX on slow connections.
  // Re-enable this once crbug.com/435315 has been fixed.
  var ignoreVideoChannelState = true;
  if (ignoreVideoChannelState) {
    console.log('Video channel ' + (ready ? '' : 'not ') + 'ready.');
    return;
  }

  this.raiseEvent(remoting.ClientSession.Events.videoChannelStateChanged,
                  ready);
};

/**
 * Called when the client-host capabilities negotiation is complete.
 * TODO(kelvinp): Move this function out of ClientSession.
 *
 * @param {!Array<string>} capabilities The set of capabilities negotiated
 *     between the client and host.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.onSetCapabilities = function(capabilities) {
  if (this.capabilities_ != null) {
    console.error('onSetCapabilities_() is called more than once');
    return;
  }
  this.capabilities_ = capabilities;
};

/** @return {boolean} */
remoting.ClientSession.prototype.isFinished = function() {
  var finishedStates = [
    remoting.ClientSession.State.CLOSED,
    remoting.ClientSession.State.FAILED,
    remoting.ClientSession.State.CONNECTION_DROPPED
  ];
  return finishedStates.indexOf(this.getState()) !== -1;
};
/**
 * @param {remoting.ClientSession.State} newState The new state for the session.
 * @return {void} Nothing.
 * @private
 */
remoting.ClientSession.prototype.setState_ = function(newState) {
  var oldState = this.state_;
  this.state_ = this.translateState_(oldState, newState);

  if (newState == remoting.ClientSession.State.CONNECTED) {
    this.connectedDisposables_.add(
        new base.RepeatingTimer(this.reportStatistics.bind(this), 1000));
  } else if (this.isFinished()) {
    base.dispose(this.connectedDisposables_);
    this.connectedDisposables_ = null;
  }

  this.logToServer.logClientSessionStateChange(this.state_, this.error_);

  this.raiseEvent(remoting.ClientSession.Events.stateChanged,
    new remoting.ClientSession.StateEvent(newState, oldState)
  );
};

/**
 * @param {remoting.ClientSession.State} previous
 * @param {remoting.ClientSession.State} current
 * @return {remoting.ClientSession.State}
 * @private
 */
remoting.ClientSession.prototype.translateState_ = function(previous, current) {
  var State = remoting.ClientSession.State;
  if (previous == State.CONNECTING || previous == State.AUTHENTICATED) {
    if (current == State.CLOSED) {
      return remoting.ClientSession.State.CONNECTION_CANCELED;
    } else if (current == State.FAILED &&
        this.error_.hasTag(remoting.Error.Tag.HOST_IS_OFFLINE) &&
        !this.logHostOfflineErrors_) {
      // The application requested host-offline errors to be suppressed, for
      // example, because this connection attempt is using a cached host JID.
      console.log('Suppressing host-offline error.');
      return State.CONNECTION_CANCELED;
    }
  } else if (previous == State.CONNECTED && current == State.FAILED) {
    return State.CONNECTION_DROPPED;
  }
  return current;
};

/** @private */
remoting.ClientSession.prototype.reportStatistics = function() {
  this.logToServer.logStatistics(this.plugin_.getPerfStats());
};

/**
 * Enable or disable logging of connection errors due to a host being offline.
 * For example, if attempting a connection using a cached JID, host-offline
 * errors should not be logged because the JID will be refreshed and the
 * connection retried.
 *
 * @param {boolean} enable True to log host-offline errors; false to suppress.
 */
remoting.ClientSession.prototype.logHostOfflineErrors = function(enable) {
  this.logHostOfflineErrors_ = enable;
};

