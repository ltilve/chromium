// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include <string>

#include "base/test/values_test_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(DeclarativeChromeContentRulesRegistryTest, ActiveRulesDoesntGrow) {
  TestExtensionEnvironment env;

  scoped_refptr<ChromeContentRulesRegistry> registry(
      new ChromeContentRulesRegistry(env.profile(), NULL));

  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  scoped_ptr<content::WebContents> tab = env.MakeTab();
  // Let the registry know about the tab.
  registry->Observe(chrome::NOTIFICATION_TAB_ADDED,
                    content::Source<content::WebContentsDelegate>(nullptr),
                    content::Details<content::WebContents>(tab.get()));
  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  // Add a rule.
  linked_ptr<RulesRegistry::Rule> rule(new RulesRegistry::Rule);
  RulesRegistry::Rule::Populate(
      *base::test::ParseJson(
          "{\n"
          "  \"id\": \"rule1\",\n"
          "  \"priority\": 100,\n"
          "  \"conditions\": [\n"
          "    {\n"
          "      \"instanceType\": \"declarativeContent.PageStateMatcher\",\n"
          "      \"css\": [\"input\"]\n"
          "    }],\n"
          "  \"actions\": [\n"
          "    { \"instanceType\": \"declarativeContent.ShowPageAction\" }\n"
          "  ]\n"
          "}"),
      rule.get());
  std::vector<linked_ptr<RulesRegistry::Rule> > rules;
  rules.push_back(rule);

  const Extension* extension = env.MakeExtension(*base::test::ParseJson(
      "{\"page_action\": {}}"));
  registry->AddRulesImpl(extension->id(), rules);

  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  std::vector<std::string> css_selectors;
  css_selectors.push_back("input");
  registry->UpdateMatchingCssSelectorsForTesting(tab.get(), css_selectors);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());

  // Closing the tab should erase its entry from active_rules_.
  tab.reset();
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());

  tab = env.MakeTab();
  // Let the registry know about the tab.
  registry->Observe(chrome::NOTIFICATION_TAB_ADDED,
                    content::Source<content::WebContentsDelegate>(nullptr),
                    content::Details<content::WebContents>(tab.get()));
  registry->UpdateMatchingCssSelectorsForTesting(tab.get(), css_selectors);
  EXPECT_EQ(1u, registry->GetActiveRulesCountForTesting());
  // Navigating the tab should erase its entry from active_rules_ if
  // it no longer matches.
  registry->DidNavigateMainFrame(tab.get(), content::LoadCommittedDetails(),
                                 content::FrameNavigateParams());
  EXPECT_EQ(0u, registry->GetActiveRulesCountForTesting());
}

}  // namespace extensions
