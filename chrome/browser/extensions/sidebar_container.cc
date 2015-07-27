// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sidebar_container.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {
SidebarContainer::SidebarContainer(Browser* browser,
                                   content::WebContents* tab,
                                   const GURL& url)
    : host_(extensions::ExtensionViewHostFactory::CreateSidebarHost(url,
                                                                    browser)),
      tab_(tab),
      browser_(browser),
      tab_strip_model_observer_(this) {
  // Listen for the containing view calling window.close();
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<content::BrowserContext>(host_->browser_context()));

  host_->CreateRenderViewSoon();
  sidebar_contents()->SetInitialFocus();

  tab_strip_model_observer_.Add(browser_->tab_strip_model());
}

SidebarContainer::~SidebarContainer() {
  tab_strip_model_observer_.Remove(browser_->tab_strip_model());
}

void SidebarContainer::TabClosingAt(TabStripModel* tab_strip_model,
                                    content::WebContents* contents,
                                    int index) {
}

void SidebarContainer::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE)
      << "Received unexpected notification";
}

}  // namespace extensions
