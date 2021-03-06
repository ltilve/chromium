// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace content {

// Derives BlinkPlatformImpl for testing shared timers.
class TestBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  TestBlinkPlatformImpl() : mock_monotonically_increasing_time_(0) {}

  // Returns mock time when enabled.
  virtual double monotonicallyIncreasingTime() override {
    if (mock_monotonically_increasing_time_ > 0.0)
      return mock_monotonically_increasing_time_;
    return BlinkPlatformImpl::monotonicallyIncreasingTime();
  }

  void OnStartSharedTimer(base::TimeDelta delay) override {
    shared_timer_delay_ = delay;
  }

  base::TimeDelta shared_timer_delay() { return shared_timer_delay_; }

  void set_mock_monotonically_increasing_time(double mock_time) {
    mock_monotonically_increasing_time_ = mock_time;
  }

 private:
  base::TimeDelta shared_timer_delay_;
  double mock_monotonically_increasing_time_;

  DISALLOW_COPY_AND_ASSIGN(TestBlinkPlatformImpl);
};

TEST(BlinkPlatformTest, SuspendResumeSharedTimer) {
  base::MessageLoop message_loop;

  TestBlinkPlatformImpl platform_impl;

  // Set a timer to fire as soon as possible.
  platform_impl.setSharedTimerFireInterval(0);

  // Suspend timers immediately so the above timer wouldn't be fired.
  platform_impl.SuspendSharedTimer();

  // The above timer would have posted a task which can be processed out of the
  // message loop.
  base::RunLoop().RunUntilIdle();

  // Set a mock time after 1 second to simulate timers suspended for 1 second.
  double new_time = base::Time::Now().ToDoubleT() + 1;
  platform_impl.set_mock_monotonically_increasing_time(new_time);

  // Resume timers so that the timer set above will be set again to fire
  // immediately.
  platform_impl.ResumeSharedTimer();

  EXPECT_TRUE(base::TimeDelta() == platform_impl.shared_timer_delay());
}

TEST(BlinkPlatformTest, IsReservedIPAddress) {
  TestBlinkPlatformImpl platform_impl;

  // Unreserved IPv4 addresses (in various forms).
  EXPECT_FALSE(platform_impl.isReservedIPAddress("8.8.8.8"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("99.64.0.0"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("212.15.0.0"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("212.15"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("212.15.0"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("3557752832"));

  // Reserved IPv4 addresses (in various forms).
  EXPECT_TRUE(platform_impl.isReservedIPAddress("192.168.0.0"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("192.168.0.6"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("10.0.0.5"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("10.0.0"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("10.0"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("3232235526"));

  // Unreserved IPv6 addresses.
  EXPECT_FALSE(platform_impl.isReservedIPAddress(
      "[FFC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress(
      "[2000:ba98:7654:2301:EFCD:BA98:7654:3210]"));

  // Reserved IPv6 addresses.
  EXPECT_TRUE(platform_impl.isReservedIPAddress("[::1]"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("[::192.9.5.5]"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress("[FEED::BEEF]"));
  EXPECT_TRUE(platform_impl.isReservedIPAddress(
      "[FEC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));

  // Not IP addresses at all.
  EXPECT_FALSE(platform_impl.isReservedIPAddress("example.com"));
  EXPECT_FALSE(platform_impl.isReservedIPAddress("127.0.0.1.example.com"));

  // Moar IPv4
  uint8 address[4] = {0, 0, 0, 1};
  for (int i = 0; i < 256; i++) {
    address[0] = i;
    std::string addressString =
        net::IPAddressToString(address, sizeof(address));
    if (i == 0 || i == 10 || i == 127 || i > 223) {
      EXPECT_TRUE(platform_impl.isReservedIPAddress(
          blink::WebString::fromUTF8(addressString)));
    } else {
      EXPECT_FALSE(platform_impl.isReservedIPAddress(
          blink::WebString::fromUTF8(addressString)));
    }
  }
}

TEST(BlinkPlatformTest, portAllowed) {
  TestBlinkPlatformImpl platform_impl;
  EXPECT_TRUE(platform_impl.portAllowed(GURL("http://example.com")));
  EXPECT_TRUE(platform_impl.portAllowed(GURL("file://example.com")));
  EXPECT_TRUE(platform_impl.portAllowed(GURL("file://example.com:87")));
  EXPECT_TRUE(platform_impl.portAllowed(GURL("ftp://example.com:21")));
  EXPECT_FALSE(platform_impl.portAllowed(GURL("ftp://example.com:87")));
  EXPECT_FALSE(platform_impl.portAllowed(GURL("ws://example.com:21")));
  EXPECT_TRUE(platform_impl.portAllowed(GURL("http://example.com:80")));
  EXPECT_TRUE(platform_impl.portAllowed(GURL("http://example.com:8889")));
}

}  // namespace content
