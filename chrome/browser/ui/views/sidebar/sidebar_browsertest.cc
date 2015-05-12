// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
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

using content::NavigationController;
using content::WebContents;

namespace {

const char kSimplePage[] = "files/sidebar/simple_page.html";

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

    ASSERT_TRUE(LoadExtension(extension_path));

    // For now content_id == extension_id.
    content_id_ = last_loaded_extension_id();
  }

  void ShowSidebarForCurrentTab() {
    ShowSidebar(browser()->tab_strip_model()->GetActiveWebContents());
  }

  void HideSidebarForCurrentTab() {
    HideSidebar(browser()->tab_strip_model()->GetActiveWebContents());
  }

  void NavigateSidebarForCurrentTabTo(const std::string& test_page) {
    GURL url = test_server()->GetURL(test_page);

    WebContents* tab = static_cast<WebContents*>(
        browser()->tab_strip_model()->GetActiveWebContents());

    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    SidebarContainer* sidebar_container =
        sidebar_manager->GetSidebarContainerFor(tab, content_id_);
    WebContents* client_contents = sidebar_container->sidebar_contents();

    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &client_contents->GetController()));
    sidebar_manager->NavigateSidebar(tab, content_id_, url);
    observer.Wait();
  }

  void ShowSidebar(WebContents* temp) {
    WebContents* tab = static_cast<WebContents*>(temp);
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->ShowSidebar(tab, content_id_);
  }

  void HideSidebar(WebContents* temp) {
    WebContents* tab = static_cast<WebContents*>(temp);
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->HideSidebar(tab, content_id_);
    if (browser()->tab_strip_model()->GetActiveWebContents() == tab)
      EXPECT_EQ(0, browser_view()->GetSidebarWidth());
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
};

IN_PROC_BROWSER_TEST_F(SidebarTest, OpenClose) {
  ShowSidebarForCurrentTab();

  HideSidebarForCurrentTab();

  ShowSidebarForCurrentTab();

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SwitchingTabs) {
  ShowSidebarForCurrentTab();

  OpenNewTab();

  // Make sure sidebar is not visbile for the newly opened tab.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);

  // Make sure it is visible now.
  EXPECT_GT(browser_view()->GetSidebarWidth(), 0);

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SidebarOnInactiveTab) {
  ShowSidebarForCurrentTab();

  OpenNewTab();

  // Hide sidebar on inactive (first) tab.
  HideSidebar(web_contents(0));

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);

  // Make sure sidebar is not visbile anymore.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Show sidebar on inactive (second) tab.
  ShowSidebar(web_contents(1));
  // Make sure sidebar is not visible yet.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Switch back to the second tab.
  tab_strip_model->ActivateTabAt(1, false);
  // Make sure sidebar is visible now.
  EXPECT_GT(browser_view()->GetSidebarWidth(), 0);

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SidebarNavigate) {
  ASSERT_TRUE(test_server()->Start());

  ShowSidebarForCurrentTab();

  NavigateSidebarForCurrentTabTo(kSimplePage);

  HideSidebarForCurrentTab();
}

}  // namespace
