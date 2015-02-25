// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sidebar/sidebar_api.h"

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "extensions/common/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_sidebar_utils.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::WebContents;

namespace {
// Errors.
const char kNoSidebarError[] =
    "This extension has no sidebar specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kNoCurrentWindowError[] = "No current browser window was found";
const char kNoDefaultTabError[] = "No default tab was found";
//const char kInvalidExpandContextError[] =
//    "Sidebar can be expanded only in response to an explicit user gesture";
// Keys.
const char kBadgeTextKey[] = "text";
const char kImageDataKey[] = "imageData";
const char kPathKey[] = "path";
const char kStateKey[] = "state";
const char kTabIdKey[] = "tabId";
const char kTitleKey[] = "title";
// Events.
const char kOnStateChanged[] = "experimental.sidebar.onStateChanged";
}  // namespace

namespace extension_sidebar_constants {
// Sidebar states.
const char kActiveState[] = "active";
const char kHiddenState[] = "hidden";
const char kShownState[] = "shown";
}  // namespace extension_sidebar_constants

// static
void ExtensionSidebarEventRouter::OnStateChanged(
    Profile* profile, content::WebContents* tab, const std::string& content_id,
    const std::string& state) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
  base::DictionaryValue* details = new base::DictionaryValue;
  details->SetInteger(kTabIdKey, tab_id);
  details->SetString(kStateKey, state);

  base::ListValue args;
  args.Set(0, details);
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->Append(new base::StringValue(json_args));

  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  scoped_ptr<extensions::Event> event(new extensions::Event(
      kOnStateChanged, event_args.Pass()));
  event->restrict_to_browser_context = profile;

  router->DispatchEventToExtension(
      extension_sidebar_utils::GetExtensionIdByContentId(content_id),
      event.Pass());
}


// List is considered empty if it is actually empty or contains just one value,
// either 'null' or 'undefined'.
static bool IsArgumentListEmpty(const base::ListValue* arguments) {
  if (arguments->empty())
    return true;
  if (arguments->GetSize() == 1) {
    const base::Value* first_value = 0;
    if (!arguments->Get(0, &first_value))
      return true;
    if (first_value->GetType() == base::Value::TYPE_NULL)
      return true;
  }
  return false;
}


bool SidebarFunction::RunSync() {
  if (!extension()->sidebar_defaults()) {
    error_ = kNoSidebarError;
    return false;
  }

  if (!args_.get())
    return false;

  base::DictionaryValue* details = NULL;
  base::DictionaryValue default_details;
  if (IsArgumentListEmpty(args_.get())) {
    details = &default_details;
  } else {
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  }

  int tab_id;
  content::WebContents* web_contents = NULL;
  if (details->HasKey(kTabIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetInteger(kTabIdKey, &tab_id));
    if (!extensions::ExtensionTabUtil::GetTabById(tab_id, GetProfile(), include_incognito(),
                                      NULL, NULL, &web_contents, NULL)) {
      error_ = extensions::ErrorUtils::FormatErrorMessage(
          kNoTabError, base::IntToString(tab_id));
      return false;
    }
  } else {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = kNoCurrentWindowError;
      return false;
    }
    if (!extensions::ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &tab_id)) {
      error_ = kNoDefaultTabError;
      return false;
    }
  }
  if (!web_contents)
    return false;

  std::string content_id(extension()->id());
  return RunImpl(static_cast<content::WebContents*>(web_contents),
                 content_id, *details);
return true;
}


bool SidebarCollapseFunction::RunImpl(content::WebContents* tab,
                                      const std::string& content_id,
                                      const base::DictionaryValue& details) {
  SidebarManager::GetInstance()->CollapseSidebar(tab, content_id);
  return true;
}

bool SidebarExpandFunction::RunImpl(content::WebContents* tab,
                                    const std::string& content_id,
                                    const base::DictionaryValue& details) {
  SidebarManager::GetInstance()->ExpandSidebar(tab, content_id);
  return true;
}

bool SidebarGetStateFunction::RunImpl(content::WebContents* tab,
                                      const std::string& content_id,
                                      const base::DictionaryValue& details) {
  SidebarManager* manager = SidebarManager::GetInstance();

  const char* result = extension_sidebar_constants::kHiddenState;
  if (manager->GetSidebarTabContents(tab, content_id)) {
    bool is_active = false;
    // Sidebar is considered active only if tab is selected, sidebar UI
    // is expanded and this extension's content is displayed on it.
    SidebarContainer* active_sidebar =
        manager->GetActiveSidebarContainerFor(tab);
    // Check if sidebar UI is expanded and this extension's content
    // is displayed on it.
    if (active_sidebar && active_sidebar->content_id() == content_id) {
      if (!details.HasKey(kTabIdKey)) {
        is_active = NULL != GetCurrentBrowser();
      } else {
        int tab_id;
        EXTENSION_FUNCTION_VALIDATE(details.GetInteger(kTabIdKey, &tab_id));

        // Check if this tab is selected.
        Browser* browser = GetCurrentBrowser();
        content::WebContents* contents = NULL;
        int default_tab_id = -1;
        if (browser &&
            extensions::ExtensionTabUtil::GetDefaultTab(browser, &contents,
                                            &default_tab_id)) {
          is_active = default_tab_id == tab_id;
        }
      }
    }

    result = is_active ? extension_sidebar_constants::kActiveState :
                         extension_sidebar_constants::kShownState;
  }

  SetResult(new base::StringValue(result));
  return true;
}

bool SidebarHideFunction::RunImpl(content::WebContents* tab,
                                  const std::string& content_id,
                                  const base::DictionaryValue& details) {
  SidebarManager::GetInstance()->HideSidebar(tab, content_id);
  return true;
}

bool SidebarNavigateFunction::RunImpl(content::WebContents* tab,
                                      const std::string& content_id,
                                      const base::DictionaryValue& details) {
 std::string path_string;
  EXTENSION_FUNCTION_VALIDATE(details.GetString(kPathKey, &path_string));

  GURL url = extension_sidebar_utils::ResolveRelativePath(
      path_string, extension(), &error_);
  if (!url.is_valid())
    return false;

  SidebarManager::GetInstance()->NavigateSidebar(tab, content_id, url);
  return true;
}

bool SidebarSetBadgeTextFunction::RunImpl(content::WebContents* tab,
                                          const std::string& content_id,
                                          const base::DictionaryValue& details) {
  base::string16 badge_text;
  EXTENSION_FUNCTION_VALIDATE(details.GetString(kBadgeTextKey, &badge_text));
  SidebarManager::GetInstance()->SetSidebarBadgeText(
      tab, content_id, badge_text);
  return true;
}

bool SidebarShowFunction::RunImpl(content::WebContents* tab,
                                  const std::string& content_id,
                                  const base::DictionaryValue& details) {
  SidebarManager::GetInstance()->ShowSidebar(tab, content_id);
  return true;
}
