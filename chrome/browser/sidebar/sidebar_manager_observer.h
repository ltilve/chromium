// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_

class SidebarManagerObserver {
 public:
  // Called when sidebar is shown
  virtual void OnSidebarShown(content::WebContents* tab,
                              const std::string& content_id) {}

  // Called when sidebar is hidden
  virtual void OnSidebarHidden(content::WebContents* tab,
                               const std::string& content_id) {}

  // Called when sidebar changes
  virtual void OnSidebarSwitched(content::WebContents* old_tab,
                                 const std::string& old_content_id,
                                 content::WebContents* new_tab,
                                 const std::string& new_content_id) {}
};

#endif // CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_
