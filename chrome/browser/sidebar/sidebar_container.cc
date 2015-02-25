// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_container.h"

#include "extensions/browser/extension_system.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_sidebar_defaults.h"
#include "extensions/common/extension_sidebar_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "url/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"


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

void SidebarContainer::Expand() {
  if (navigate_to_default_page_on_expand_) {
    navigate_to_default_page_on_expand_ = false;
    // Check whether a default page is specified for this sidebar.
    const extensions::Extension* extension = GetExtension();
    if (extension) {  // Can be NULL in tests.
      //  TODO(me):no member sidebar defaults
      if (extension->sidebar_defaults()->default_page().is_valid())
        Navigate(extension->sidebar_defaults()->default_page());
    }
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

void SidebarContainer::SetBadgeText(const base::string16& badge_text) {
  badge_text_ = badge_text;
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
