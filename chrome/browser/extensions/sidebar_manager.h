// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_

#include <map>
#include <string>

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/sidebar_container.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class SidebarContainer;
class SidebarManagerObserver;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
///////////////////////////////////////////////////////////////////////////////
// SidebarManager
//
//  This class is a singleton that manages SidebarContainer instances and
//  maintains a connection between tabs and sidebars.
//
class SidebarManager : public content::NotificationObserver {
 public:
  // Returns SidebarManager instance registered with BrowserContext.
  static SidebarManager* GetFromContext(content::BrowserContext* context);

  SidebarManager();

  // Returns SidebarContainer registered for |tab| or nullptr if there is no
  // SidebarContainer registered for |tab|.
  SidebarContainer* GetSidebarContainerFor(content::WebContents* tab);

  // Creates a new sidebar identified by |tab| (adds sidebar's mini tab).
  void CreateSidebar(content::WebContents* tab,
                     const GURL& url,
                     Browser* browser);

  // Hides and destroys sidebar identified by |tab| (removes sidebar's mini
  // tab).
  void HideSidebar(content::WebContents* tab);

  // Check if |tab| has a sidebar assigned.
  bool HasSidebar(content::WebContents* tab);

  void AddObserver(SidebarManagerObserver* observer);
  void RemoveObserver(SidebarManagerObserver* observer);

  ~SidebarManager() override;

 private:
  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns SidebarContainer corresponding to |sidebar_contents|.
  SidebarContainer* FindSidebarContainerFor(
      content::WebContents* sidebar_contents);

  // Records the link between |tab| and |container|.
  void BindSidebarContainer(content::WebContents* tab,
                            SidebarContainer* container);

  // Forgets the link between |tab| and |container|.
  void UnbindSidebarContainer(content::WebContents* tab,
                              SidebarContainer* container);

  content::NotificationRegistrar registrar_;

  // This map stores sidebars linked to a particular tab. Sidebars are
  // identified by their unique content id (string).
  typedef std::map<std::string, SidebarContainer*>
      ContentIdToSidebarContainerMap;

  // These two maps are for tracking dependencies between tabs and
  // their SidebarContainers.
  //
  // SidebarManager start listening to SidebarContainers when they are put
  // into these maps and removes them when they are closing.
  typedef std::map<content::WebContents*, SidebarContainer*>
      TabToSidebarContainerMap;
  TabToSidebarContainerMap tab_to_sidebar_container_;

  base::ObserverList<SidebarManagerObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SidebarManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIDEBAR_MANAGER_H_
