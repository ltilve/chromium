// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_

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
class Extension;
}

///////////////////////////////////////////////////////////////////////////////
// SidebarContainer
//
//  Stores one particular sidebar state: sidebar's content, its content id,
//  tab it is linked to, mini tab icon, title etc.
//
class SidebarContainer : public extensions::ExtensionHostObserver,
                         public content::NotificationObserver {
 public:
  // Interface to implement to listen for sidebar update notification.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void UpdateSidebar(SidebarContainer* host) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  SidebarContainer(Browser* browser,
                   content::WebContents* tab,
                   const GURL& url,
                   Delegate* delegate);
  ~SidebarContainer() override;

  // Called right before destroying this sidebar.
  // Does all the necessary cleanup.
  void SidebarClosing();

  // Retruns HostContents sidebar is linked to.
  content::WebContents* host_contents() const { return host_->host_contents(); }

  // Returns TabContents sidebar is linked to.
  content::WebContents* web_contents() const { return tab_; }

  // Notifies hosting window that this sidebar was expanded.
  void Show();

  // Notifies hosting window that this sidebar was expanded.
  void Expand();

  // Notifies hosting window that this sidebar was collapsed.
  void Collapse();

  // Navigates sidebar contents to the |url|.
  void Navigate(const GURL& url);

  const std::string& extension_id() { return host_->extension_id(); }

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  scoped_ptr<extensions::ExtensionViewHost> host_;

  ScopedObserver<extensions::ExtensionHost, extensions::ExtensionHostObserver>
      host_observer_;

  content::NotificationRegistrar registrar_;

  // Contents of the tab this sidebar is linked to.
  content::WebContents* tab_;

  // Sidebar update notification listener.
  Delegate* delegate_;

  // On the first expand sidebar will be automatically navigated to the default
  // page (specified in the extension manifest), but only if the extension has
  // not explicitly navigated it yet. This variable is set to false on the first
  // sidebar navigation.
  bool navigate_to_default_page_on_expand_;

  DISALLOW_COPY_AND_ASSIGN(SidebarContainer);
};

#endif  // CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
