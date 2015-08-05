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
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

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

    firstExtension_ = LoadExtension(extension_path.AppendASCII("sidebar"));
    ASSERT_TRUE(firstExtension_);

    secondExtension_ = LoadExtension(extension_path.AppendASCII("sidebar2"));
    ASSERT_TRUE(secondExtension_);

    ASSERT_NE(firstExtension_->id(),secondExtension_->id());

    browser_action_test_util_.reset(new BrowserActionTestUtil(browser()));
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(browser()->profile())
        ->GetBrowserAction(extension);
  }

  void ClickExtensionBrowserAction() {
    browser_action_test_util_.get()->Press(0);
  }

  ToolbarActionsBar* toolbar_actions_bar() {
    return browser_action_test_util_->GetToolbarActionsBar();
  }

  void DisableOpenInSidebar() {
    GetBrowserAction(*firstExtension_)->set_open_in_sidebar(false);
    GetBrowserAction(*secondExtension_)->set_open_in_sidebar(false);
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

  bool isShowingSidebarForExtension(const ExtensionId id) {
    ExtensionActionViewController* controller;
    for (unsigned int i = 0; i < toolbar_actions_bar()->GetActions().size(); i++) {
      controller = static_cast<ExtensionActionViewController*>(toolbar_actions_bar()->GetActions()[i]);
      if ( controller->extension()->id() == id) {
        return controller->is_showing_sidebar();
      }
    }
    return false;
  }

  void clickBrowserActionForExtension(const ExtensionId id) {
  ExtensionActionViewController* controller;
    for (unsigned int i = 0; i < toolbar_actions_bar()->GetActions().size(); i++) {
      controller = static_cast<ExtensionActionViewController*>(toolbar_actions_bar()->GetActions()[i]);
      if ( controller->extension()->id() == id) {
        browser_action_test_util_.get()->Press(i);
      }
    }
  }

  bool isShowingFirstExtension() {
    return isShowingSidebarForExtension(firstExtension_->id());
  }
  void clickFirstExtension() {
    clickBrowserActionForExtension(firstExtension_->id());
  }

  bool isShowingSecondExtension() {
    return isShowingSidebarForExtension(secondExtension_->id());
  }
  void clickSecondExtension() {
    clickBrowserActionForExtension(secondExtension_->id());
  }

 private:
  const Extension* firstExtension_;
  const Extension* secondExtension_;
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;
};

// 1 - Tests that cliking on the browser action show/hides the sidebar
IN_PROC_BROWSER_TEST_F(SidebarTest, CreateSidebar) {
  EXPECT_FALSE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_TRUE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_FALSE(HasSidebarForCurrentTab());
}

// 2 - Tests that sidebar visible at the other tabs
IN_PROC_BROWSER_TEST_F(SidebarTest, SwitchingTabs) {
  // Open sidebar and move to a new tab
  ClickExtensionBrowserAction();
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(HasSidebarForCurrentTab());

  // Close sidebar and switch back to the first tab
  ClickExtensionBrowserAction();
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);
  EXPECT_FALSE(HasSidebarForCurrentTab());
}

// 3 - Tests that sidebars are not shown if open_in_sidebar: false
IN_PROC_BROWSER_TEST_F(SidebarTest, CreateDisabledSidebar) {
  DisableOpenInSidebar();
  ClickExtensionBrowserAction();
  EXPECT_FALSE(HasSidebarForCurrentTab());
}


// 4 - Tests that cliking on the browser action show/hides the sidebar
IN_PROC_BROWSER_TEST_F(SidebarTest, MultipleExtensions) {
  EXPECT_FALSE(isShowingFirstExtension());
  EXPECT_FALSE(isShowingSecondExtension());
  clickFirstExtension();
  EXPECT_TRUE(isShowingFirstExtension());
  EXPECT_FALSE(isShowingSecondExtension());
  clickSecondExtension();
  EXPECT_FALSE(isShowingFirstExtension());
  EXPECT_TRUE(isShowingSecondExtension());
  clickSecondExtension();
  EXPECT_FALSE(isShowingFirstExtension());
  EXPECT_FALSE(isShowingSecondExtension());
}


}  // namespace extensions
