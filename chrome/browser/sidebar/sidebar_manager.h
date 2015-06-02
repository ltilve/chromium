// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Profile;
class SidebarContainer;
class SidebarManagerObserver;

namespace content {
class WebContents;
}

namespace extensions {
///////////////////////////////////////////////////////////////////////////////
// SidebarManager
//
//  This class is a singleton that manages SidebarContainer instances and
//  maintains a connection between tabs and sidebars.
//
class SidebarManager : public content::NotificationObserver,
                       public base::RefCounted<SidebarManager> {
 public:
  // Returns s singleton instance.
  static SidebarManager* GetInstanceFromProfile(Profile* profile);

  SidebarManager();

  // Returns SidebarContainer registered for |tab| and active or NULL if
  // there is no alive and active SidebarContainer registered for |tab|.
  SidebarContainer* GetActiveSidebarContainerFor(content::WebContents* tab);

  // Returns SidebarContainer registered for |tab| and |content_id| or NULL if
  // there is no such SidebarContainer registered.
  SidebarContainer* GetSidebarContainerFor(content::WebContents* tab,
                                           const std::string& content_id);

  // Returns sidebar's TabContents registered for |tab| and |content_id|.
  content::WebContents* GetSidebarTabContents(content::WebContents* tab,
                                              const std::string& content_id);

  // Sends sidebar state change notification to extensions.
  void NotifyStateChanges(content::WebContents* was_active_sidebar_contents,
                          content::WebContents* active_sidebar_contents);

  // Shows sidebar identified by |tab| and |content_id| (only sidebar's
  // mini tab is visible).
  void ShowSidebar(content::WebContents* tab,
                   const std::string& content_id,
                   const GURL& url,
                   Browser* browser);

  // Expands sidebar identified by |tab| and |content_id|.
  void ExpandSidebar(content::WebContents* tab, const std::string& content_id);

  // Collapses sidebar identified by |tab| and |content_id| (has no effect
  // if sidebar is not expanded).
  void CollapseSidebar(content::WebContents* tab,
                       const std::string& content_id);

  SidebarContainer* MigrateSidebarTo(content::WebContents* tab);
  // Hides sidebar identified by |tab| and |content_id| (removes sidebar's
  // mini tab).
  void HideSidebar(content::WebContents* tab, const std::string& content_id);

  // Navigates sidebar identified by |tab| and |content_id| to |url|.
  void NavigateSidebar(content::WebContents* tab,
                       const std::string& content_id,
                       const GURL& url);

  void AddObserver(SidebarManagerObserver* observer);
  void RemoveObserver(SidebarManagerObserver* observer);

 private:
  friend class base::RefCounted<SidebarManager>;

  ~SidebarManager() override;

  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Hides all sidebars registered for |tab|.
  void HideAllSidebars(content::WebContents* tab);

  // Returns SidebarContainer corresponding to |sidebar_contents|.
  SidebarContainer* FindSidebarContainerFor(
      content::WebContents* sidebar_contents);

  // Registers new SidebarContainer for |tab|. There must be no
  // other SidebarContainers registered for the RenderViewHost at the moment.
  void RegisterSidebarContainerFor(content::WebContents* tab,
                                   SidebarContainer* container);

  // Unregisters SidebarContainer identified by |tab| and |content_id|.
  void UnregisterSidebarContainerFor(content::WebContents* tab,
                                     const std::string& content_id);

  // Records the link between |tab| and |container|.
  void BindSidebarContainer(content::WebContents* tab,
                       SidebarContainer* container);

  // Forgets the link between |tab| and |container|.
  void UnbindSidebarContainer(content::WebContents* tab,
                         SidebarContainer* container);

  content::NotificationRegistrar registrar_;

  // This map stores sidebars linked to a particular tab. Sidebars are
  // identified by their unique content id (string).
  typedef std::map<std::string, SidebarContainer*> ContentIdToSidebarContainerMap;

  // These two maps are for tracking dependencies between tabs and
  // their SidebarContainers.
  //
  // SidebarManager start listening to SidebarContainers when they are put
  // into these maps and removes them when they are closing.
  struct SidebarStateForTab;
  typedef std::map<content::WebContents*, SidebarStateForTab>
      TabToSidebarContainerMap;
  TabToSidebarContainerMap tab_to_sidebar_container_;

  typedef std::map<SidebarContainer*, content::WebContents*>
      SidebarContainerToTabMap;
  SidebarContainerToTabMap sidebar_container_to_tab_;

  ObserverList<SidebarManagerObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SidebarManager);
};
}

#endif  // CHROME_BROWSER_SIDEBAR_SIDEBAR_MANAGER_H_
