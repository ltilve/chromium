// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sidebar/sidebar_api.h"

#include "common/extensions/api/sidebar.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/event_router.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_sidebar_utils.h"
#include "content/public/browser/web_contents.h"



using content::WebContents;

namespace {
// Errors.
const char kNoSidebarError[] =
    "This extension has no sidebar specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kNoCurrentWindowError[] = "No current browser window was found";
const char kNoDefaultTabError[] = "No default tab was found";
// Keys.
const char kStateKey[] = "state";
const char kTabIdKey[] = "tabId";
const char kShownFlag[] = "shown";
const char kPinnedFlag[] = "pinned";
}  // namespace

namespace extension_sidebar_constants {
// Sidebar states.
const char kActiveState[] = "active"; // TODO(kfowler) remove
const char kHiddenState[] = "hidden"; // TODO(kfowler) remove
const char kShownState[] = "shown";   // TODO(kfowler) remove
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
      extensions::api::sidebar::OnStateChanged::kEventName, event_args.Pass()));
  event->restrict_to_browser_context = profile;

  router->DispatchEventToExtension(
      extension_sidebar_utils::GetExtensionIdByContentId(content_id),
      event.Pass());
}

bool SidebarGetStateFunction::RunSync() {
  if (!extension()->sidebar_defaults()) {
    error_ = kNoSidebarError;
    return false;
  }

  if (!args_.get()) {
    return false;
  }

  scoped_ptr<extensions::api::sidebar::GetState::Params>
      params(extensions::api::sidebar::GetState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int active_tab_id = -1;
  if (params->tab_id == 0L) {
    VLOG(1) << "getState called with no tabId, will use active tab on frontmost window.";
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      VLOG(1) << "error: no current window (" << kNoCurrentWindowError << ")";
      error_ = kNoCurrentWindowError;
      return false;
    }
    content::WebContents* web_contents = 0L;
    if (!extensions::ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &active_tab_id)) {
      VLOG(1) << "error: no default tab.";
      error_ = kNoDefaultTabError;
      return false;
    }
    VLOG(1) << "Found tabId: " << active_tab_id;
  } else {
    active_tab_id = *params->tab_id;
  }
  VLOG(1) << "getState of tab_id = " << active_tab_id;
  
  SidebarManager* manager = SidebarManager::GetInstance();
  base::DictionaryValue* sidebar_state = new base::DictionaryValue;
  sidebar_state->SetBoolean(kShownFlag, false);
  sidebar_state->SetBoolean(kPinnedFlag, false);

  content::WebContents* web_contents = NULL;
  if (!extensions::ExtensionTabUtil::GetTabById(active_tab_id,
                                                GetProfile(), include_incognito(),
                                                NULL, NULL, &web_contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(active_tab_id));
    VLOG(1) << "Could not find tab for " << active_tab_id << " " << error_;
    return false;
  }
  if (!web_contents) {
    VLOG(1) << "No web contents.";
    return false;
  }

  manager->GetSidebarTabContents(web_contents, extension()->id());
  SetResult(sidebar_state);

  return true;
}

bool SidebarHideFunction::RunSync() {
  if (!extension()->sidebar_defaults()) {
    error_ = kNoSidebarError;
    return false;
  }

  if (!args_.get()) {
    return false;
  }

  scoped_ptr<extensions::api::sidebar::Hide::Params>
      params(extensions::api::sidebar::Hide::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int active_tab_id = -1;
  if (params->tab_id == 0L) {
    VLOG(1) << "hide called with no tabId, will use active tab on frontmost window.";
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      VLOG(1) << "error: no current window (" << kNoCurrentWindowError << ")";
      error_ = kNoCurrentWindowError;
      return false;
    }
    content::WebContents* web_contents = 0L;
    if (!extensions::ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &active_tab_id)) {
      VLOG(1) << "error: no default tab.";
      error_ = kNoDefaultTabError;
      return false;
    }
    VLOG(1) << "Found tabId: " << active_tab_id;
  } else {

    active_tab_id = *params->tab_id;
  }
  VLOG(1) << "hide tab with tab_id = " << active_tab_id;

  content::WebContents* web_contents = NULL;
  if (!extensions::ExtensionTabUtil::GetTabById(active_tab_id,
                                                GetProfile(), include_incognito(),
                                                NULL, NULL, &web_contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(active_tab_id));
    VLOG(1) << "Could not find tab for " << active_tab_id << " " << error_;
    return false;
  }
  if (!web_contents) {
    VLOG(1) << "No web contents.";
    return false;
  }

  SidebarManager::GetInstance()->HideSidebar(web_contents, extension()->id());
  return true;
}

bool SidebarShowFunction::RunSync() {
  
  if (!extension()->sidebar_defaults()) {
    VLOG(1) << "No sidebar";
    error_ = kNoSidebarError;
    return false;
  }

  if (!args_.get()) {
    VLOG(1) << "No args";
    return false;
  }

  scoped_ptr<extensions::api::sidebar::Show::Params>
      params(extensions::api::sidebar::Show::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // extract tab_id or use active tab id
  int active_tab_id = -1;
  if (params->details == 0L) {
    VLOG(1) << "No details. Bail.";
    return false;
  }

  /*
   * tabId ( optional integer )
   * The tab id for the tab to show the sidebar for. Uses the current
   * selected tab when omitted.
  */
  if (params->details->tab_id == 0L) {
    VLOG(1) << "hide called with no tabId, will use active tab on frontmost window.";
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      VLOG(1) << "error: no current window (" << kNoCurrentWindowError << ")";
      error_ = kNoCurrentWindowError;
      return false;
    }
    content::WebContents* web_contents = 0L;
    if (!extensions::ExtensionTabUtil::GetDefaultTab(browser, &web_contents, &active_tab_id)) {
      VLOG(1) << "error: no default tab.";
      error_ = kNoDefaultTabError;
      return false;
    }
    VLOG(1) << "Found tabId: " << active_tab_id;
  } else {
    active_tab_id = *params->details->tab_id;
  }


  /* 
   * width ( optional integer )
   * The width in pixels for the sidebar display area. If not
   * specified, the sidebar can contain any HTML contents that you
   * like, and it's automatically sized to fit its contents.
   *
   * -1 indicates width was not supplied
   */
  const int width(params->details->width ? *params->details->width : -1);

  /*
   * sidebar ( string ) // required
   * URL to navigate sidebar content to.
   */
  VLOG(1) << "params->details->sidebar = " << params->details->sidebar;
  const GURL sidebarUrl = extension_sidebar_utils::ResolveRelativePath(
      params->details->sidebar, extension(), &error_);
  
  if (!sidebarUrl.is_valid()) {
    VLOG(1) << "invalid URL (" << sidebarUrl << ") "
            << "passed to chrome.sidebar.show()";
    return false;
  }
  
  VLOG(1) << "show tab with tab_id = " << active_tab_id
          << " width = " << width
          << " sidebar = " << sidebarUrl;

  content::WebContents* web_contents = NULL;
  if (!extensions::ExtensionTabUtil::GetTabById(active_tab_id,
                                                GetProfile(), include_incognito(),
                                                NULL, NULL, &web_contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(active_tab_id));
    VLOG(1) << "Could not find tab for " << active_tab_id << " " << error_;
    return false;
  }
  if (!web_contents) {
    VLOG(1) << "No web contents.";
    return false;
  }

  SidebarManager::GetInstance()->ShowSidebar(web_contents, extension()->id());
  SidebarManager::GetInstance()->NavigateSidebar(web_contents, extension()->id(), sidebarUrl);
  return true;
}
