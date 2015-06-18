// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sidebar_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"

using content::NavigationController;
using content::WebContents;
using extensions::SidebarManager;

namespace extensions {

const char kSimplePage[] = "/simple_page.html";

class SidebarTest : public ExtensionBrowserTest {
 public:
  SidebarTest() {}

 protected:
  // InProcessBrowserTest overrides.
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Load test sidebar extension.
    base::FilePath extension_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
    extension_path = extension_path.AppendASCII("sidebar");
    extension_ = LoadExtension(extension_path);

    // ASSERT_TRUE(LoadExtension(extension_path));
    ASSERT_TRUE(extension_);


    // For now content_id == extension_id.
    content_id_ = last_loaded_extension_id();
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(browser()->profile())->
        GetBrowserAction(extension);
  }

  BrowserActionTestUtil* GetBrowserActionsBar() {
    if (!browser_action_test_util_)
      browser_action_test_util_.reset(new BrowserActionTestUtil(browser()));
    return browser_action_test_util_.get();
  }

  void CreateSidebarForCurrentTab() {
    ASSERT_TRUE(extension_);
    // Simulate click
   GetBrowserActionsBar()->Press(0);
  }

  void HideSidebarForCurrentTab() {
    HideSidebar(browser()->tab_strip_model()->GetActiveWebContents());
  }

  void CreateSidebar(WebContents* temp) {
    WebContents* tab = static_cast<WebContents*>(temp);
    SidebarManager* sidebar_manager =
        SidebarManager::GetFromContext(browser()->profile());
    GURL url("chrome-extension://" + content_id_ + kSimplePage);
    sidebar_manager->CreateSidebar(tab, url, browser());
  }

  void HideSidebar(WebContents* temp) {
    WebContents* tab = static_cast<WebContents*>(temp);
    SidebarManager* sidebar_manager =
        SidebarManager::GetFromContext(browser()->profile());
    sidebar_manager->HideSidebar(tab);
    EXPECT_FALSE(sidebar_manager->HasSidebar(tab));
  }
  // Opens a new tab and waits for navigations to finish. If there are pending
  // navigations, the constrained prompt might be dismissed when the navigation
  // completes.
  void OpenNewTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"), NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  WebContents* web_contents(int i) {
    return static_cast<WebContents*>(
        browser()->tab_strip_model()->GetWebContentsAt(i));
  }

  BrowserView* browser_view() const {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  std::string content_id_;
  const Extension* extension_;

  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;
};

IN_PROC_BROWSER_TEST_F(SidebarTest, OpenClose) {
  CreateSidebarForCurrentTab();

  HideSidebarForCurrentTab();

  CreateSidebarForCurrentTab();

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, CreateSidebar) {
  SidebarManager* sidebar_manager =
      SidebarManager::GetFromContext(browser()->profile());

  CreateSidebarForCurrentTab();

  EXPECT_TRUE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

  CreateSidebarForCurrentTab();

  EXPECT_FALSE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

}

IN_PROC_BROWSER_TEST_F(SidebarTest, SwitchingTabs) {
  SidebarManager* sidebar_manager =
      SidebarManager::GetFromContext(browser()->profile());

  CreateSidebarForCurrentTab();

  OpenNewTab();

  // Make sure sidebar is not visbile for the newly opened tab.
  EXPECT_FALSE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);

  // Make sure it is visible now.
  EXPECT_TRUE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SidebarOnInactiveTab) {


  SidebarManager* sidebar_manager =
      SidebarManager::GetFromContext(browser()->profile());

  CreateSidebarForCurrentTab();

  OpenNewTab();

  // Hide sidebar on inactive (first) tab.
  HideSidebar(web_contents(0));

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);

  // Make sure sidebar is not visbile anymore.
  EXPECT_FALSE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Show sidebar on inactive (second) tab.
  CreateSidebar(web_contents(1));
  // Make sure sidebar is not visible yet.
  EXPECT_FALSE(sidebar_manager->HasSidebar(
      browser()->tab_strip_model()->GetActiveWebContents()));

}

}  // namespace
