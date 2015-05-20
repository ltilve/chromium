// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_container.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "url/gurl.h"

SidebarContainer::SidebarContainer(Browser* browser,
                                   content::WebContents* tab,
                                   const GURL& url,
                                   Delegate* delegate)
    : host_(extensions::ExtensionViewHostFactory::CreateSidebarHost(url,
            browser)),
      host_observer_(this),
      tab_(tab),
      delegate_(delegate),
      navigate_to_default_page_on_expand_(true) {
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      host_contents());
  host_->CreateView(browser);
  host_observer_.Add(host_.get());
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
  if (navigate_to_default_page_on_expand_)
    navigate_to_default_page_on_expand_ = false;

  delegate_->UpdateSidebar(this);
  host_contents()->SetInitialFocus();
}

void SidebarContainer::Collapse() {
  delegate_->UpdateSidebar(this);
}
