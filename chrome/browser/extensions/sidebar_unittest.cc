// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/chrome_paths.h"

namespace extensions {

class SidebarUnitTest : public BrowserWithTestWindowTest {
 public:
  SidebarUnitTest() {}
  ~SidebarUnitTest() override {}

 protected:
  void SetUp() override;
  void TearDown() override;

  ToolbarActionsBar* toolbar_actions_bar() {
    return browser_action_test_util_->GetToolbarActionsBar();
  }
  BrowserActionTestUtil* browser_action_test_util() {
    return browser_action_test_util_.get();
  }

  bool HasSidebarForCurrentTab();
  void ClickExtensionBrowserAction();

  void SetOpenInSidebar(const Extension* extension, bool open_in_sidebar) {
    (ExtensionActionManager::Get(browser()->profile())
         ->GetBrowserAction(*extension))->set_open_in_sidebar(open_in_sidebar);
  }

  bool isShowingSidebarForExtension(const ExtensionId id) {
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

  void clickBrowserActionForExtension(const ExtensionId id) {
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

  bool isShowingFirstExtension() {
    return isShowingSidebarForExtension(firstExtension_->id());
  }
  void clickFirstExtension() {
    clickBrowserActionForExtension(firstExtension_->id());
  }

 private:
  scoped_refptr<Extension> firstExtension_;
  scoped_refptr<Extension> secondExtension_;

  scoped_ptr<BrowserActionTestUtil> browser_action_test_util_;

  DISALLOW_COPY_AND_ASSIGN(SidebarUnitTest);
};

void SidebarUnitTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extensions::extension_action_test_util::CreateToolbarModelForProfile(
      profile());

  // Load test sidebar extension.
  base::FilePath extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));

  TestExtensionSystem* system = static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(browser()->profile()));

  ExtensionService* extension_service = system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), extension_path, false);

  extension_path = extension_path.AppendASCII("sidebar");

  std::string error;
  firstExtension_ = file_util::LoadExtension(extension_path, Manifest::UNPACKED,
                                             Extension::NO_FLAGS, &error);

  ASSERT_TRUE(firstExtension_.get());

  extension_service->AddExtension(firstExtension_.get());

  extensions::extension_action_test_util::CreateToolbarModelForProfile(
      profile());

  browser_action_test_util_.reset(new BrowserActionTestUtil(browser(), false));
}

void SidebarUnitTest::TearDown() {
  browser_action_test_util_.reset();
  BrowserWithTestWindowTest::TearDown();
}

bool SidebarUnitTest::HasSidebarForCurrentTab() {
  ExtensionActionViewController* controller =
      static_cast<ExtensionActionViewController*>(
          toolbar_actions_bar()->GetActions()[0]);
  return controller->is_showing_sidebar();
}

void SidebarUnitTest::ClickExtensionBrowserAction() {
  ASSERT_TRUE(toolbar_actions_bar()->GetActions().size() > 0);
  ASSERT_TRUE(toolbar_actions_bar()->GetActions().size() > 0);
  browser_action_test_util()->Press(0);
}

// 1 - Tests that cliking on the browser action show/hides the sidebar
TEST_F(SidebarUnitTest, CreateSidebar) {
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_FALSE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_TRUE(HasSidebarForCurrentTab());
  ClickExtensionBrowserAction();
  EXPECT_FALSE(HasSidebarForCurrentTab());
}

TEST_F(SidebarUnitTest, MultipleExtensions) {
  EXPECT_FALSE(isShowingFirstExtension());
  clickFirstExtension();
  EXPECT_TRUE(isShowingFirstExtension());
}

}  // namespace extensions
