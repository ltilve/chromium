// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_manager.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/switches.h"
#include "url/gurl.h"

using content::WebContents;

struct SidebarManager::SidebarStateForTab {
  // Sidebars linked to this tab.
  ContentIdToSidebarContainerMap content_id_to_sidebar_container;
  // Content id of the currently active (expanded and visible) sidebar.
  std::string active_content_id;
};

// static
SidebarManager* SidebarManager::GetInstance() {
  return g_browser_process->sidebar_manager();
}

SidebarManager::SidebarManager() {
}

SidebarContainer* SidebarManager::GetActiveSidebarContainerFor(
    content::WebContents* tab) {
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return NULL;
  if (it->second.active_content_id.empty())
    return NULL;
  ContentIdToSidebarContainerMap::iterator container_it =
      it->second.content_id_to_sidebar_container.find(
          it->second.active_content_id);
  DCHECK(container_it != it->second.content_id_to_sidebar_container.end());
  return container_it->second;
}

SidebarContainer* SidebarManager::MigrateSidebarTo(WebContents* tab) {
  if (tab_to_sidebar_container_.empty())
    return NULL;
  SidebarContainer* container =
      GetActiveSidebarContainerFor(tab_to_sidebar_container_.begin()->first);
  if (container)
    return NULL;

  return container;
}

SidebarContainer* SidebarManager::GetSidebarContainerFor(
    WebContents* tab,
    const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return NULL;
  ContentIdToSidebarContainerMap::iterator container_it =
      it->second.content_id_to_sidebar_container.find(content_id);
  if (container_it == it->second.content_id_to_sidebar_container.end())
    return NULL;
  return container_it->second;
}

content::WebContents* SidebarManager::GetSidebarTabContents(
    content::WebContents* tab,
    const std::string& content_id) {
  DCHECK(!content_id.empty());
  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  if (!container)
    return NULL;
  return container->host_contents();
}

void SidebarManager::NotifyStateChanges(
    content::WebContents* was_active_sidebar_contents,
    content::WebContents* active_sidebar_contents) {
  if (was_active_sidebar_contents == active_sidebar_contents)
    return;

  SidebarContainer* was_active_container =
      was_active_sidebar_contents == NULL
          ? NULL
          : FindSidebarContainerFor(was_active_sidebar_contents);
  SidebarContainer* active_container =
      active_sidebar_contents == NULL
          ? NULL
          : FindSidebarContainerFor(active_sidebar_contents);

  content::WebContents* old_tab = was_active_container == NULL
                                      ? NULL
                                      : was_active_container->web_contents();
  content::WebContents* new_tab =
      active_container == NULL ? NULL : active_container->web_contents();
  const std::string& old_content_id =
      was_active_container == NULL ? "" : was_active_container->extension_id();
  const std::string& new_content_id =
      active_container == NULL ? "" : active_container->extension_id();

  FOR_EACH_OBSERVER(
      SidebarManagerObserver, observer_list_,
      OnSidebarSwitched(old_tab, old_content_id, new_tab, new_content_id));
}

void SidebarManager::ShowSidebar(content::WebContents* tab,
                                 const std::string& content_id,
                                 const GURL& url,
                                 Browser* browser) {
  DCHECK(!content_id.empty());
  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  if (!container) {
    container = new SidebarContainer(browser, tab, url, this);
    RegisterSidebarContainerFor(tab, container);
  }

  container->Show();
  ExpandSidebar(tab, content_id);

  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarShown(tab, content_id));
}

void SidebarManager::ExpandSidebar(content::WebContents* tab,
                                   const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return;
  // If it's already active, bail out.
  if (it->second.active_content_id == content_id)
    return;

  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  DCHECK(container);
  if (!container)
    return;
  it->second.active_content_id = content_id;

  container->Expand();
}

void SidebarManager::CollapseSidebar(content::WebContents* tab,
                                     const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return;
  // If it's not the one active now, bail out.
  if (it->second.active_content_id != content_id)
    return;

  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  DCHECK(container);
  if (!container)
    return;
  it->second.active_content_id.clear();

  container->Collapse();
}

void SidebarManager::HideSidebar(WebContents* tab,
                                 const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return;
  if (it->second.active_content_id == content_id)
    it->second.active_content_id.clear();

  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  DCHECK(container);
  CollapseSidebar(tab, content_id);
  UnregisterSidebarContainerFor(tab, content_id);

  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarHidden(tab, content_id));
}

void SidebarManager::NavigateSidebar(content::WebContents* tab,
                                     const std::string& content_id,
                                     const GURL& url) {
  DCHECK(!content_id.empty());
  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  if (!container)
    return;

  container->Navigate(url);
}

SidebarManager::~SidebarManager() {
  DCHECK(tab_to_sidebar_container_.empty());
  DCHECK(sidebar_container_to_tab_.empty());
}

void SidebarManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    HideAllSidebars(content::Source<WebContents>(source).ptr());
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

void SidebarManager::UpdateSidebar(SidebarContainer* container) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SIDEBAR_CHANGED,
      content::Source<SidebarManager>(this),
      content::Details<SidebarContainer>(container));
}

void SidebarManager::HideAllSidebars(WebContents* tab) {
  TabToSidebarContainerMap::iterator tab_it =
      tab_to_sidebar_container_.find(tab);
  if (tab_it == tab_to_sidebar_container_.end())
    return;
  const ContentIdToSidebarContainerMap& containers =
      tab_it->second.content_id_to_sidebar_container;

  std::vector<std::string> content_ids;
  for (ContentIdToSidebarContainerMap::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    content_ids.push_back(it->first);
  }

  for (std::vector<std::string>::iterator it = content_ids.begin();
       it != content_ids.end(); ++it) {
    HideSidebar(tab, *it);
  }
}

SidebarContainer* SidebarManager::FindSidebarContainerFor(
    content::WebContents* sidebar_contents) {
  for (SidebarContainerToTabMap::iterator it =
           sidebar_container_to_tab_.begin();
       it != sidebar_container_to_tab_.end(); ++it) {
    if (sidebar_contents == it->first->host_contents())
      return it->first;
  }
  return NULL;
}

void SidebarManager::RegisterSidebarContainerFor(WebContents* tab,
                                                 SidebarContainer* container) {
  DCHECK(!GetSidebarContainerFor(tab, container->extension_id()));

  // If it's a first sidebar for this tab, register destroy notification.
  if (tab_to_sidebar_container_.find(tab) == tab_to_sidebar_container_.end()) {
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<WebContents>(tab));
  }

  BindSidebarContainer(tab, container);
}

void SidebarManager::UnregisterSidebarContainerFor(
    WebContents* tab,
    const std::string& content_id) {
  SidebarContainer* container = GetSidebarContainerFor(tab, content_id);
  DCHECK(container);
  if (!container)
    return;

  UnbindSidebarContainer(tab, container);

  // If there's no more sidebars linked to this tab, unsubscribe.
  if (tab_to_sidebar_container_.find(tab) == tab_to_sidebar_container_.end()) {
    registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      content::Source<WebContents>(tab));
  }

  // Issue tab closing event post unbound.
  container->SidebarClosing();
  // Destroy sidebar container.
  delete container;
}

void SidebarManager::BindSidebarContainer(WebContents* tab,
                                          SidebarContainer* container) {
  const std::string& content_id = container->extension_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == NULL);
  DCHECK(sidebar_container_to_tab_.find(container) ==
         sidebar_container_to_tab_.end());

  tab_to_sidebar_container_[tab].content_id_to_sidebar_container[content_id] =
      container;
  sidebar_container_to_tab_[container] = tab;
}

void SidebarManager::UnbindSidebarContainer(WebContents* tab,
                                            SidebarContainer* container) {
  const std::string& content_id = container->extension_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == container);
  DCHECK(sidebar_container_to_tab_.find(container)->second == tab);
  DCHECK(tab_to_sidebar_container_[tab].active_content_id != content_id);

  tab_to_sidebar_container_[tab].content_id_to_sidebar_container.erase(
      content_id);
  if (tab_to_sidebar_container_[tab].content_id_to_sidebar_container.empty())
    tab_to_sidebar_container_.erase(tab);
  sidebar_container_to_tab_.erase(container);
}

void SidebarManager::AddObserver(SidebarManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SidebarManager::RemoveObserver(SidebarManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}
