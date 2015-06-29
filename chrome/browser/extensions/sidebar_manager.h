// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_

#include "base/observer_list.h"
#include "chrome/browser/extensions/sidebar_container.h"

class GURL;
class SidebarContainer;
class SidebarManagerObserver;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

// Singleton that manages SidebarContainer instances and
// maintains a connection between tabs and sidebars.
class SidebarManager {
 public:
  // Returns SidebarManager instance registered with BrowserContext.
  static SidebarManager* GetFromContext(content::BrowserContext* context);

  SidebarManager();

  ~SidebarManager();

  // Returns SidebarContainer registered for |tab| or nullptr if there is no
  // SidebarContainer registered for |tab|.
  SidebarContainer* GetSidebarContainerFor(content::WebContents* tab);

  // Sends sidebar state change notification to extensions.
  void NotifyStateChanges(content::WebContents* was_active_sidebar_contents,
                          content::WebContents* active_sidebar_contents);

  // Creates a new sidebar identified by |tab| (adds sidebar's mini tab).
  void CreateSidebar(content::WebContents* tab,
                     const GURL& url,
                     Browser* browser);

  // Hides and destroys sidebar identified by |tab| (removes sidebar's mini
  // tab).
  void HideSidebar(content::WebContents* tab);

  void AddObserver(SidebarManagerObserver* observer);
  void RemoveObserver(SidebarManagerObserver* observer);

 private:
  // Returns SidebarContainer corresponding to |sidebar_contents|.
  SidebarContainer* FindSidebarContainerFor(
      content::WebContents* sidebar_contents);

  // These two maps are for tracking dependencies between tabs and
  // their SidebarContainers.
  //
  // SidebarManager start listening to SidebarContainers when they are put
  // into these maps and removes them when they are closing.
  using TabToSidebarContainerMap =
      std::map<content::WebContents*, SidebarContainer*>;

  TabToSidebarContainerMap tab_to_sidebar_container_;

  base::ObserverList<SidebarManagerObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SidebarManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_
