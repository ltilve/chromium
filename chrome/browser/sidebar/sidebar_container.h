// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_delegate.h"

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
class SidebarContainer : public content::WebContentsDelegate {
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

  SidebarContainer(content::WebContents* tab,
                   const std::string& content_id,
                   Delegate* delegate);
  ~SidebarContainer() override;

  // Called right before destroying this sidebar.
  // Does all the necessary cleanup.
  void SidebarClosing();

  // Returns sidebar's content id.
  const std::string& content_id() const { return content_id_; }

  // Returns TabContents sidebar is linked to.
  content::WebContents* web_contents() const { return tab_; }

  // Returns sidebar's TabContents.
  content::WebContents* sidebar_contents() const { return sidebar_contents_.get(); }

  bool HasGlobalScope() const;

  // Notifies hosting window that this sidebar was expanded.
  void Show();

  // Notifies hosting window that this sidebar was expanded.
  void Expand();

  // Notifies hosting window that this sidebar was collapsed.
  void Collapse();

  // Navigates sidebar contents to the |url|.
  void Navigate(const GURL& url);

 private:
  // Overridden from content::WebContentsDelegate:
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;

  void CloseContents(content::WebContents *contents) override;

  // Returns an extension this sidebar belongs to.
  const extensions::Extension* GetExtension() const;

  // Contents of the tab this sidebar is linked to.
  content::WebContents* tab_;

  // Sidebar's content id. There might be more than one sidebar liked to each
  // particular tab and they are identified by their unique content id.
  const std::string content_id_;

  // Sidebar update notification listener.
  Delegate* delegate_;

  // Sidebar contents.
  scoped_ptr<content::WebContents> sidebar_contents_;

  // On the first expand sidebar will be automatically navigated to the default
  // page (specified in the extension manifest), but only if the extension has
  // not explicitly navigated it yet. This variable is set to false on the first
  // sidebar navigation.
  bool navigate_to_default_page_on_expand_;

  DISALLOW_COPY_AND_ASSIGN(SidebarContainer);
};

#endif  // CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
