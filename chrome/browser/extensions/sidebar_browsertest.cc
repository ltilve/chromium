// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"

namespace extensions {

class SidebarBrowserTest : public ExtensionBrowserTest {
 public:
  SidebarBrowserTest() {}
  ~SidebarBrowserTest() override {}

 protected:
  // InProcessBrowserTest
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Load test sidebar extensions
    firstExtension_ = LoadExtension(test_data_dir_.AppendASCII("sidebar"));
    ASSERT_TRUE(firstExtension_);

    secondExtension_ = LoadExtension(test_data_dir_.AppendASCII("sidebar2"));
    ASSERT_TRUE(secondExtension_);

    ASSERT_NE(firstExtension_->id(), secondExtension_->id());

    browser_action_test_util_.reset(new BrowserActionTestUtil(browser()));
  }

  const ExtensionId first_extension_id() { return firstExtension_->id(); }

  const ExtensionId second_extension_id() { return secondExtension_->id(); }

  void ClickExtensionBrowserAction() {
    browser_action_test_util_.get()->Press(0);
  }

  ToolbarActionsBar* toolbar_actions_bar() {
    return browser_action_test_util_->GetToolbarActionsBar();
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(browser()->profile())
        ->GetBrowserAction(extension);
  }

  void DisableOpenInSidebar() {
    GetBrowserAction(*firstExtension_)->set_open_in_sidebar(false);
    GetBrowserAction(*secondExtension_)->set_open_in_sidebar(false);
  }

  bool IsShowingSidebar(const ExtensionId id) {
    ExtensionActionViewController* controller;
    for (unsigned int i = 0; i < toolbar_actions_bar()->GetActions().size();
         i++) {
      controller = static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[i]);
      if (controller->extension()->id() == id) {
        return controller->is_showing_sidebar();
      }
    }
    return false;
  }

  void ClickBrowserAction(const ExtensionId id) {
    ExtensionActionViewController* controller;
    for (unsigned int i = 0; i < toolbar_actions_bar()->GetActions().size();
         i++) {
      controller = static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[i]);
      if (controller->extension()->id() == id) {
        browser_action_test_util_.get()->Press(i);
      }
    }
  }

 private:
  const Extension* firstExtension_;
  const Extension* secondExtension_;
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;
};

// 1 - Tests that cliking on the browser action show/hides the sidebar
IN_PROC_BROWSER_TEST_F(SidebarBrowserTest, CreateSidebar) {
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
  ClickBrowserAction(first_extension_id());
  EXPECT_TRUE(IsShowingSidebar(first_extension_id()));
  ClickBrowserAction(first_extension_id());
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
}

// Tests that sidebar visible at the other tabs
IN_PROC_BROWSER_TEST_F(SidebarBrowserTest, SwitchingTabs) {
  // Open sidebar and move to a new tab
  ClickBrowserAction(first_extension_id());
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(IsShowingSidebar(first_extension_id()));

  // Close sidebar and switch back to the first tab
  ClickBrowserAction(first_extension_id());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
}

// Tests that sidebars are not shown if open_in_sidebar: false
IN_PROC_BROWSER_TEST_F(SidebarBrowserTest, CreateDisabledSidebar) {
  DisableOpenInSidebar();
  ClickBrowserAction(first_extension_id());
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
}

// Tests that cliking on the browser action show/hides the sidebar
IN_PROC_BROWSER_TEST_F(SidebarBrowserTest, MultipleExtensions) {
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
  EXPECT_FALSE(IsShowingSidebar(second_extension_id()));
  ClickBrowserAction(first_extension_id());
  EXPECT_TRUE(IsShowingSidebar(first_extension_id()));
  EXPECT_FALSE(IsShowingSidebar(second_extension_id()));

  ClickBrowserAction(second_extension_id());
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
  EXPECT_TRUE(IsShowingSidebar(second_extension_id()));

  ClickBrowserAction(second_extension_id());
  EXPECT_FALSE(IsShowingSidebar(first_extension_id()));
  EXPECT_FALSE(IsShowingSidebar(second_extension_id()));
}

}  // namespace extensions
