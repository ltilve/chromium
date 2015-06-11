// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIDEBAR_CONTAINER_H_
#define CHROME_BROWSER_EXTENSIONS_SIDEBAR_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/extension_host_observer.h"

namespace content {
class WebContents;
}

namespace extensions {
///////////////////////////////////////////////////////////////////////////////
// SidebarContainer
//
//  Stores one particular sidebar state: sidebar's content, its content id,
//  tab it is linked to, mini tab icon, title etc.
//
class SidebarContainer : public content::NotificationObserver {
 public:
  // Interface to implement to listen for sidebar update notification.

  SidebarContainer(Browser* browser,
                   content::WebContents* tab,
                   const GURL& url);
  ~SidebarContainer() override;

  // Retruns HostContents sidebar is linked to.
  content::WebContents* host_contents() const { return host_->host_contents(); }

  // Returns TabContents sidebar is linked to.
  content::WebContents* web_contents() const { return tab_; }

  const std::string& extension_id() { return host_->extension_id(); }

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  scoped_ptr<extensions::ExtensionViewHost> host_;

  content::NotificationRegistrar registrar_;

  // Contents of the tab this sidebar is linked to.
  content::WebContents* tab_;

  DISALLOW_COPY_AND_ASSIGN(SidebarContainer);
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_SIDEBAR_CONTAINER_H_
