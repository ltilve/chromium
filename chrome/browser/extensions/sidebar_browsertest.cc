// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"

using content::NavigationController;
using content::WebContents;

namespace extensions {

class SidebarTest : public ExtensionBrowserTest {
 public:
  SidebarTest() {}
  ~SidebarTest() override {}

 protected:
  // InProcessBrowserTest
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Load test sidebar extension.
    base::FilePath extension_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
    extension_path = extension_path.AppendASCII("sidebar");
    extension_ = LoadExtension(extension_path);

    ASSERT_TRUE(extension_);

    browser_action_test_util_.reset(new BrowserActionTestUtil(browser()));
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(browser()->profile())
        ->GetBrowserAction(extension);
  }

  void ClickExtensionBrowserAction() {
    browser_action_test_util_.get()->Press(0);
  }

  void DisableOpenInSidebar() {
    GetBrowserAction(*extension_)->set_open_in_sidebar(false);
  }

  ExtensionActionViewController* getExtensionActionViewController() {
    return static_cast<ExtensionActionViewController*>(
        browser_action_test_util_.get()
            ->GetToolbarActionsBar()
            ->GetActions()[0]);
  }

  bool HasSidebarForCurrentTab() {
    return getExtensionActionViewController()->is_showing_sidebar();
  }

 private:
  const Extension* extension_;
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;
};

// Tests that cliking on the browser action show/hides the sidebar
IN_PROC_BROWSER_TEST_F(SidebarTest, CreateSidebar) {
  EXPECT_FALSE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_TRUE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_FALSE(HasSidebarForCurrentTab());
}

// Tests that sidebars are not shown if open_in_sidebar: false
IN_PROC_BROWSER_TEST_F(SidebarTest, CreateDisabledSidebar) {
  DisableOpenInSidebar();
  ClickExtensionBrowserAction();
  EXPECT_FALSE(HasSidebarForCurrentTab());
}

}  // namespace extensions
