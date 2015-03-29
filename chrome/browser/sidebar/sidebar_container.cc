// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_container.h"

#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/extension_system.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "extensions/common/extension_sidebar_defaults.h"
#include "extensions/common/extension_sidebar_utils.h"
#include "content/public/browser/web_contents.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "url/gurl.h"
namespace {
  const char kGlobalScopeName[] = "global";
}
SidebarContainer::SidebarContainer(content::WebContents* tab,
                                   const std::string& content_id,
                                   Delegate* delegate)
    : tab_(tab),
      content_id_(content_id),
      delegate_(delegate),
      navigate_to_default_page_on_expand_(true) {
  // Create WebContents for sidebar.

   sidebar_contents_.reset(
       content::WebContents::Create(
           content::WebContents::CreateParams(tab->GetBrowserContext())));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      sidebar_contents_.get());

   sidebar_contents_->SetDelegate(this);
}

SidebarContainer::~SidebarContainer() {
}

void SidebarContainer::SidebarClosing() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Show() {
  delegate_->UpdateSidebar(this);
}

bool SidebarContainer::HasGlobalScope() const {
  const extensions::Extension* extension = GetExtension();
  if (!extension || !extension->sidebar_defaults())
    return false;
  base::string16 scope = extension->sidebar_defaults()->default_scope();
  return !scope.empty() && (std::string(base::UTF16ToUTF8(scope)) == kGlobalScopeName);
}

void SidebarContainer::Expand() {
  if (navigate_to_default_page_on_expand_) {
    navigate_to_default_page_on_expand_ = false;
    /*
    // Check whether a default page is specified for this sidebar.
    const extensions::Extension* extension = GetExtension();
    if (extension && extension->sidebar_defaults()) {  // Can be NULL in tests.
      LOG(INFO) << extension->sidebar_defaults()->default_page();
      if (extension->sidebar_defaults()->default_page().is_valid())
        Navigate(extension->sidebar_defaults()->default_page());
    }
    */
  }

  delegate_->UpdateSidebar(this);
  sidebar_contents_->SetInitialFocus();
}

void SidebarContainer::Collapse() {
  delegate_->UpdateSidebar(this);
}

void SidebarContainer::Navigate(const GURL& url) {
  // TODO(alekseys): add a progress UI.
  navigate_to_default_page_on_expand_ = false;
  sidebar_contents_->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_HOME_PAGE,
      std::string());
}

content::JavaScriptDialogManager*
SidebarContainer::GetJavaScriptDialogManager(content::WebContents* source) {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

const extensions::Extension* SidebarContainer::GetExtension() const {
  Profile* profile =
      Profile::FromBrowserContext(sidebar_contents_->GetBrowserContext());
  ExtensionService* service =
          extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return NULL;
  return service->GetExtensionById(
      extension_sidebar_utils::GetExtensionIdByContentId(content_id_), false);
}
