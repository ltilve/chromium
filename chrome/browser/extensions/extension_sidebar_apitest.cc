// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/common/switches.h"

class SidebarApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
	extensions::switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(SidebarApiTest, Sidebar) {
  ASSERT_TRUE(RunExtensionTest("sidebar")) << message_;
}
