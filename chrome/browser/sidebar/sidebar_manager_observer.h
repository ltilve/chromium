// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_

class SidebarManagerObserver {
 public:
  virtual void OnSidebarShown(const std::string& content_id) {}
  virtual void OnSidebarHidden(const std::string& content_id) {}
};

#endif // CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_OBSERVER_H_
