// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_manager.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/switches.h"
#include "url/gurl.h"

using content::WebContents;

struct SidebarManager::SidebarStateForTab {
  // Sidebars linked to this tab.
  ContentIdToSidebarHostMap content_id_to_sidebar_host;
  // Content id of the currently active (expanded and visible) sidebar.
  std::string active_content_id;
};

// static
SidebarManager* SidebarManager::GetInstance() {
  return g_browser_process->sidebar_manager();
}

// static
bool SidebarManager::IsSidebarAllowed() {
  NOTIMPLEMENTED();
  return true;
}

SidebarManager::SidebarManager() {
}

SidebarContainer* SidebarManager::GetActiveSidebarContainerFor(
    content::WebContents* tab) {
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end()) {
    return NULL;
  }
  if (it->second.active_content_id.empty())
    return NULL;
  ContentIdToSidebarHostMap::iterator host_it =
      it->second.content_id_to_sidebar_host.find(it->second.active_content_id);
  DCHECK(host_it != it->second.content_id_to_sidebar_host.end());
  return host_it->second;
}

SidebarContainer* SidebarManager::MigrateSidebarTo(WebContents* tab) {
  if (tab_to_sidebar_host_.empty())
    return NULL;
  SidebarContainer* container =
      GetActiveSidebarContainerFor(tab_to_sidebar_host_.begin()->first);
  if (container && !container->HasGlobalScope())
    return NULL;
  
  return container;
}

SidebarContainer* SidebarManager::GetSidebarContainerFor(
    WebContents* tab, const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end()) {
    return NULL;
  }
  ContentIdToSidebarHostMap::iterator host_it =
      it->second.content_id_to_sidebar_host.find(content_id);
  if (host_it == it->second.content_id_to_sidebar_host.end())
    return NULL;
  return host_it->second;
}

content::WebContents* SidebarManager::GetSidebarTabContents(
    content::WebContents* tab, const std::string& content_id) {
  DCHECK(!content_id.empty());
  SidebarContainer* sidebar_host = GetSidebarContainerFor(tab, content_id);
  if (!sidebar_host)
    return NULL;
  return sidebar_host->sidebar_contents();
}

void SidebarManager::ShowSidebar(content::WebContents* tab,
                                 const std::string& content_id) {
  DCHECK(!content_id.empty());
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host) {
    host = new SidebarContainer(tab, content_id, this);
    RegisterSidebarContainerFor(tab, host);
  }

  host->Show();
  ExpandSidebar(tab, content_id);
}

void SidebarManager::ExpandSidebar(content::WebContents* tab,
                                   const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  // If it's already active, bail out.
  if (it->second.active_content_id == content_id)
    return;

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;
  it->second.active_content_id = content_id;

  host->Expand();
}

void SidebarManager::CollapseSidebar(content::WebContents* tab,
                                     const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  // If it's not the one active now, bail out.
  if (it->second.active_content_id != content_id)
    return;

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;
  it->second.active_content_id.clear();

  host->Collapse();
}

void SidebarManager::HideSidebar(WebContents* tab,
                                 const std::string& content_id) {
  DCHECK(!content_id.empty());
  TabToSidebarHostMap::iterator it = tab_to_sidebar_host_.find(tab);
  if (it == tab_to_sidebar_host_.end())
    return;
  if (it->second.active_content_id == content_id)
    it->second.active_content_id.clear();

  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  CollapseSidebar(tab, content_id);
  UnregisterSidebarContainerFor(tab, content_id);
}

void SidebarManager::NavigateSidebar(content::WebContents* tab,
                                     const std::string& content_id,
                                     const GURL& url) {
  DCHECK(!content_id.empty());
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  if (!host)
    return;

  host->Navigate(url);
}

SidebarManager::~SidebarManager() {
  DCHECK(tab_to_sidebar_host_.empty());
  DCHECK(sidebar_host_to_tab_.empty());
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

void SidebarManager::UpdateSidebar(SidebarContainer* host) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SIDEBAR_CHANGED,
      content::Source<SidebarManager>(this),
      content::Details<SidebarContainer>(host));
}

void SidebarManager::HideAllSidebars(WebContents* tab) {
  TabToSidebarHostMap::iterator tab_it = tab_to_sidebar_host_.find(tab);
  if (tab_it == tab_to_sidebar_host_.end())
    return;
  const ContentIdToSidebarHostMap& hosts =
      tab_it->second.content_id_to_sidebar_host;

  std::vector<std::string> content_ids;
  for (ContentIdToSidebarHostMap::const_iterator it = hosts.begin();
       it != hosts.end(); ++it) {
    content_ids.push_back(it->first);
  }

  for (std::vector<std::string>::iterator it = content_ids.begin();
       it != content_ids.end(); ++it) {
    HideSidebar(tab, *it);
  }
}

SidebarContainer* SidebarManager::FindSidebarContainerFor(
    content::WebContents* sidebar_contents) {
  for (SidebarHostToTabMap::iterator it = sidebar_host_to_tab_.begin();
       it != sidebar_host_to_tab_.end();
       ++it) {
    if (sidebar_contents == it->first->sidebar_contents())
      return it->first;
  }
  return NULL;
}

void SidebarManager::RegisterSidebarContainerFor(
    WebContents* tab, SidebarContainer* sidebar_host) {
  DCHECK(!GetSidebarContainerFor(tab, sidebar_host->content_id()));

  // If it's a first sidebar for this tab, register destroy notification.
  if (tab_to_sidebar_host_.find(tab) == tab_to_sidebar_host_.end()) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<WebContents>(tab));
  }

  BindSidebarHost(tab, sidebar_host);
}

void SidebarManager::UnregisterSidebarContainerFor(
      WebContents* tab, const std::string& content_id) {
  SidebarContainer* host = GetSidebarContainerFor(tab, content_id);
  DCHECK(host);
  if (!host)
    return;

  UnbindSidebarHost(tab, host);

  // If there's no more sidebars linked to this tab, unsubscribe.
  if (tab_to_sidebar_host_.find(tab) == tab_to_sidebar_host_.end()) {
    registrar_.Remove(this,
                      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      content::Source<WebContents>(tab));
  }

  // Issue tab closing event post unbound.
  host->SidebarClosing();
  // Destroy sidebar container.
  delete host;
}

void SidebarManager::BindSidebarHost(WebContents* tab,
                                     SidebarContainer* sidebar_host) {
  const std::string& content_id = sidebar_host->content_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == NULL);
  DCHECK(sidebar_host_to_tab_.find(sidebar_host) ==
         sidebar_host_to_tab_.end());

  tab_to_sidebar_host_[tab].content_id_to_sidebar_host[content_id] =
      sidebar_host;
  sidebar_host_to_tab_[sidebar_host] = tab;
}

void SidebarManager::UnbindSidebarHost(WebContents* tab,
                                       SidebarContainer* sidebar_host) {
  const std::string& content_id = sidebar_host->content_id();

  DCHECK(GetSidebarContainerFor(tab, content_id) == sidebar_host);
  DCHECK(sidebar_host_to_tab_.find(sidebar_host)->second == tab);
  DCHECK(tab_to_sidebar_host_[tab].active_content_id != content_id);

  tab_to_sidebar_host_[tab].content_id_to_sidebar_host.erase(content_id);
  if (tab_to_sidebar_host_[tab].content_id_to_sidebar_host.empty())
    tab_to_sidebar_host_.erase(tab);
  sidebar_host_to_tab_.erase(sidebar_host);
}
