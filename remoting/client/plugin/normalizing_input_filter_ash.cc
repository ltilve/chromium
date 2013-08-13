// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NormalizingInputFilterAsh addresses the problems generated by key rewritings
// such as Down->PageDown, 1->F1, etc, when keys are pressed in combination with
// the OSKey (aka Search). Rewriting OSKey+Down, for example, causes us to
// receive the following:
//
// keydown OSKey
// keydown PageDown
// keyup PageDown
// keyup OSKey
//
// The host system will therefore behave as if OSKey+PageDown were pressed,
// rather than PageDown alone.
//
// This file must be kept up-to-date with changes to
// chrome/browser/ui/ash/event_rewriter.cc

#include "remoting/client/plugin/normalizing_input_filter.h"

#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

namespace {

// Returns true for OSKey codes.
static bool IsOsKey(unsigned int code) {
  const unsigned int kUsbLeftOsKey      = 0x0700e3;
  const unsigned int kUsbRightOsKey     = 0x0700e7;
  return code == kUsbLeftOsKey || code == kUsbRightOsKey;
}

// Returns true for codes generated by EventRewriter::RewriteFunctionKeys().
static bool IsRewrittenFunctionKey(unsigned int code) {
  const unsigned int kUsbFunctionKeyMin = 0x07003a;
  const unsigned int kUsbFunctionKeyMax = 0x070045;
  return code >= kUsbFunctionKeyMin && code <= kUsbFunctionKeyMax;
}

// Returns true for codes generated by EventRewriter::RewriteExtendedKeys().
static bool IsRewrittenExtendedKey(unsigned int code) {
  const unsigned int kUsbExtendedKeyMin = 0x070049;
  const unsigned int kUsbExtendedKeyMax = 0x07004e;
  return code >= kUsbExtendedKeyMin && code <= kUsbExtendedKeyMax;
}

// Returns true for codes generated by EventRewriter::Rewrite().
static bool IsRewrittenKey(unsigned int code) {
  return IsRewrittenExtendedKey(code) || IsRewrittenFunctionKey(code);
}

// The input filter tries to avoid sending keydown/keyup events for OSKey
// (aka Search, WinKey, Cmd, Super) when it is used to rewrite other key events.
// Rewriting via other combinations is not currently handled.
//
// OSKey events can be categorised as one of three kinds:
// - Modifying - Holding the key down while executing other input modifies the
//     effect of that input, e.g. OSKey+L causes the workstation to lock, e.g.
//     OSKey + mouse-move performs an extended selection.
// - Rewriting (ChromeOS only) - Holding the key down while pressing certain
//     keys causes them to be treated as different keys, e.g. OSKey causes the
//     Down key to behave as PageDown.
// - Normal - Press & release of the key trigger an action, e.g. showing the
//     Start menu.
//
// The input filter has four states:
// 1. No OSKey has been pressed.
//    - When an OSKey keydown is received, the event is deferred, and we move to
//      State #2.
// 2. An OSKey is pressed, but may be Normal, Rewriting or Modifying.
//    - If the OSKey keyup is received, the key is Normal, both events are sent
//      and we return to State #1.
//    - If a Rewritten event is received we move to State #3.
//    - If a Modified event is received the OSKey keydown is sent and we enter
//      State #4.
// 3. An OSKey is pressed, and is being used to Rewrite other key events.
//    - If the OSKey keyup is received then it is suppressed, and we move to
//      State #1.
//    - If a Modified event is received the OSKey keydown is sent and we enter
//      State #4.
//    - If a Rewritten event is received then we stay in State #3.
// 4. An OSKey is pressed, and is Modifying.
//    - If the OSKey keyup is received then we send it and we move to State #1.
//    - All other key event pass through the filter unchanged.

class NormalizingInputFilterAsh : public protocol::InputFilter {
 public:
  explicit NormalizingInputFilterAsh(protocol::InputStub* input_stub)
      : protocol::InputFilter(input_stub),
        deferred_key_is_rewriting_(false),
        modifying_key_(0) {
  }
  virtual ~NormalizingInputFilterAsh() {}

  // InputFilter overrides.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE {
    DCHECK(event.has_usb_keycode());
    DCHECK(event.has_pressed());

    if (event.pressed())
      ProcessKeyDown(event);
    else
      ProcessKeyUp(event);
  }

  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE {
    if (deferred_keydown_event_.has_usb_keycode())
      SwitchRewritingKeyToModifying();
    InputFilter::InjectMouseEvent(event);
  }

 private:
  void ProcessKeyDown(const protocol::KeyEvent& event) {
    // If |event| is |deferred_keydown_event_| auto-repeat then assume
    // that the user is holding the key down rather than using it to Rewrite.
    if (deferred_keydown_event_.has_usb_keycode() &&
        deferred_keydown_event_.usb_keycode() == event.usb_keycode()) {
      SwitchRewritingKeyToModifying();
    }

    // If |event| is a |modifying_key_| repeat then let it pass through.
    if (modifying_key_ == event.usb_keycode()) {
      InputFilter::InjectKeyEvent(event);
      return;
    }

    // If |event| is for an OSKey and we don't know whether it's a Normal,
    // Rewriting or Modifying use, then hold the keydown event.
    if (IsOsKey(event.usb_keycode())) {
      deferred_keydown_event_ = event;
      deferred_key_is_rewriting_ = false;
      return;
    }

    // If |event| is for a Rewritten key then set a flag to prevent any deferred
    // OSKey keydown from being sent when keyup is received for it. Otherwise,
    // inject the deferred OSKey keydown, if any, and switch that key into
    // Modifying mode.
    if (IsRewrittenKey(event.usb_keycode())) {
      // Note that there may not be a deferred OSKey event if there is a full
      // PC keyboard connected, which can generate e.g. PageDown without
      // rewriting.
      deferred_key_is_rewriting_ = true;
    } else {
      if (deferred_keydown_event_.has_usb_keycode())
        SwitchRewritingKeyToModifying();
    }

    InputFilter::InjectKeyEvent(event);
  }

  void ProcessKeyUp(const protocol::KeyEvent& event) {
    if (deferred_keydown_event_.has_usb_keycode() &&
        deferred_keydown_event_.usb_keycode() == event.usb_keycode()) {
      if (deferred_key_is_rewriting_) {
        // If we never sent the keydown then don't send a keyup.
        deferred_keydown_event_ = protocol::KeyEvent();
        return;
      }

      // If the OSKey hasn't Rewritten anything then treat as Modifying.
      SwitchRewritingKeyToModifying();
    }

    if (modifying_key_ == event.usb_keycode())
      modifying_key_ = 0;

    InputFilter::InjectKeyEvent(event);
  }

  void SwitchRewritingKeyToModifying() {
    DCHECK(deferred_keydown_event_.has_usb_keycode());
    modifying_key_ = deferred_keydown_event_.usb_keycode();
    InputFilter::InjectKeyEvent(deferred_keydown_event_);
    deferred_keydown_event_ = protocol::KeyEvent();
  }

  // Holds the keydown event for the most recent OSKey to have been pressed,
  // while it is Rewriting, or we are not yet sure whether it is Normal,
  // Rewriting or Modifying. The event is sent on if we switch to Modifying, or
  // discarded if the OSKey is released while in Rewriting mode.
  protocol::KeyEvent deferred_keydown_event_;

  // True while the |rewrite_keydown_event_| key is Rewriting, i.e. was followed
  // by one or more Rewritten key events, and not by any Modified events.
  bool deferred_key_is_rewriting_;

  // Stores the code of the OSKey while it is pressed for use as a Modifier.
  uint32 modifying_key_;

  DISALLOW_COPY_AND_ASSIGN(NormalizingInputFilterAsh);
};

}  // namespace

scoped_ptr<protocol::InputFilter> CreateNormalizingInputFilter(
    protocol::InputStub* input_stub) {
  return scoped_ptr<protocol::InputFilter>(
      new NormalizingInputFilterAsh(input_stub));
}

}  // namespace remoting
