// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sidebar_manager.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"

using content::NavigationController;
using content::WebContents;
using extensions::SidebarManager;

namespace extensions {

const char kSimplePage[] = "/simple_page.html";

class SidebarManagerTest : public BrowserWithTestWindowTest {
 public:
  SidebarManagerTest() {}
  ~SidebarManagerTest() override {}

 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Load test sidebar extension.
    base::FilePath extension_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));

    TestExtensionSystem* system = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(browser()->profile()));

    ExtensionService* extension_service = system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), extension_path, false);

    extension_path = extension_path.AppendASCII("sidebar");

    std::string error;
    extension_ = file_util::LoadExtension(extension_path, Manifest::UNPACKED,
                                          Extension::NO_FLAGS, &error);

    ASSERT_TRUE(extension_.get());

    extension_service->AddExtension(extension_.get());

    browser_action_test_util_.reset(new BrowserActionTestUtil(browser()));

    BrowserWithTestWindowTest::AddTab(browser(), GURL(url::kAboutBlankURL));
    browser()->tab_strip_model()->ActivateTabAt(0, false);
  }

  void CreateSidebarForCurrentTab() {
    CreateSidebar(browser()->tab_strip_model()->GetActiveWebContents());
  }

  void CreateSidebar(WebContents* temp) {
    SidebarManager* sidebar_manager =
        SidebarManager::GetFromContext(browser()->profile());
    GURL url("chrome-extension://" + extension_.get()->id() + kSimplePage);
    sidebar_manager->CreateSidebar(temp, url, browser());
  }

  void HideSidebarForTab(WebContents* temp) {
    SidebarManager* sidebar_manager =
        SidebarManager::GetFromContext(browser()->profile());
    sidebar_manager->HideSidebarForTab(temp);
    EXPECT_FALSE(sidebar_manager->GetSidebarContainerFor(temp) != nullptr);
  }

  WebContents* web_contents(int i) {
    return browser()->tab_strip_model()->GetWebContentsAt(i);
  }

  bool HasSidebarForCurrentTab() {
    SidebarManager* sidebar_manager =
        SidebarManager::GetFromContext(browser()->profile());
    return sidebar_manager->GetSidebarContainerFor(
               browser()->tab_strip_model()->GetActiveWebContents()) != nullptr;
  }

 private:
  scoped_refptr<Extension> extension_;
  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;
};

// Tests that sidebar is only visible at the proper tab
TEST_F(SidebarManagerTest, SwitchingTabs) {
  // Do not expect initial sidebar and create at current Tab
  EXPECT_FALSE(HasSidebarForCurrentTab());
  CreateSidebarForCurrentTab();
  EXPECT_TRUE(HasSidebarForCurrentTab());

  // Switch to new tab and make sure that sidebar is not visible
  BrowserWithTestWindowTest::AddTab(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(HasSidebarForCurrentTab());

  // Hide sidebar on inactive (first) tab.
  HideSidebarForTab(web_contents(0));

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, false);

  // Make sure it is not visible now.
  EXPECT_FALSE(HasSidebarForCurrentTab());

  // Show sidebar on inactive (second) tab.
  CreateSidebar(web_contents(1));

  tab_strip_model->ActivateTabAt(1, false);

  // Make sure sidebar is now visible
  EXPECT_TRUE(HasSidebarForCurrentTab());
}

}  // namespace extensions
