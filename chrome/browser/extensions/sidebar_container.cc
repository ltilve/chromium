// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sidebar_container.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/extensions/sidebar_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "url/gurl.h"

namespace extensions {
SidebarContainer::SidebarContainer(Browser* browser,
                                   content::WebContents* tab,
                                   const GURL& url)
    : host_(extensions::ExtensionViewHostFactory::CreateSidebarHost(url,
                                                                    browser)),
      tab_(tab),
      browser_(browser) {
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      host_contents());

  // Listen for the containing view calling window.close();
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<content::BrowserContext>(host_->browser_context()));

  host_->CreateRenderViewSoon();
  host_contents()->SetInitialFocus();
  browser_->tab_strip_model()->AddObserver(this);
}

SidebarContainer::~SidebarContainer() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void SidebarContainer::TabClosingAt(TabStripModel* tab_strip_model,
                                    content::WebContents* contents,
                                    int index) {
  if (tab_ == contents)
    extensions::SidebarManager::GetFromContext(host_->browser_context())
        ->HideSidebar(tab_);
}

void SidebarContainer::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE)
     << "Received unexpected notification";

  // If we aren't the host of the popup, then disregard the notification.
  if (content::Details<extensions::ExtensionHost>(host_.get()) == details)
    extensions::SidebarManager::GetFromContext(host_->browser_context())
        ->HideSidebar(tab_);
}

}  // namespace extensions
