// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sidebar_manager.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sidebar_container.h"
#include "chrome/browser/extensions/sidebar_manager_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/switches.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::WebContents;

namespace extensions {
// static
SidebarManager* SidebarManager::GetFromContext(BrowserContext* context) {
  return ExtensionSystem::Get(context)->sidebar_manager();
}

SidebarManager::SidebarManager() {
}

SidebarContainer* SidebarManager::GetSidebarContainerFor(
    content::WebContents* tab) {
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  if (it == tab_to_sidebar_container_.end())
    return nullptr;
  return it->second;
}

void SidebarManager::NotifyStateChanges(
    content::WebContents* was_active_sidebar_contents,
    content::WebContents* active_sidebar_contents) {
  if (was_active_sidebar_contents == active_sidebar_contents)
    return;

  SidebarContainer* was_active_container =
      was_active_sidebar_contents == nullptr
          ? nullptr
          : FindSidebarContainerFor(was_active_sidebar_contents);
  SidebarContainer* active_container =
      active_sidebar_contents == nullptr
          ? nullptr
          : FindSidebarContainerFor(active_sidebar_contents);

  content::WebContents* old_tab =
      was_active_container == nullptr ? nullptr : was_active_container->web_contents();
  content::WebContents* new_tab =
      active_container == nullptr ? nullptr : active_container->web_contents();
  const std::string& old_content_id =
      was_active_container == nullptr ? "" : was_active_container->extension_id();
  const std::string& new_content_id =
      active_container == nullptr ? "" : active_container->extension_id();

  FOR_EACH_OBSERVER(
      SidebarManagerObserver, observer_list_,
      OnSidebarSwitched(old_tab, old_content_id, new_tab, new_content_id));
}

void SidebarManager::ShowSidebar(content::WebContents* tab,
                                 const GURL& url, Browser* browser) {
  SidebarContainer* container = GetSidebarContainerFor(tab);
  if (container)
    HideSidebar(tab);

  container = new SidebarContainer(browser, tab, url);
  BindSidebarContainer(tab, container);

  container->Show();
  container->Expand();

  const std::string id = container->extension_id();
  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarShown(tab, id));
}

void SidebarManager::HideSidebar(WebContents* tab) {
  SidebarContainer* container = GetSidebarContainerFor(tab);
  if (!container)
    return;

  const std::string content_id = container->extension_id();
  UnbindSidebarContainer(tab, container);
  delete container;

  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarHidden(tab, content_id));
}

void SidebarManager::NavigateSidebar(content::WebContents* tab) {
  SidebarContainer* container = GetSidebarContainerFor(tab);
  if (!container)
    return;

  container->Navigate();
}

SidebarManager::~SidebarManager() {
  DCHECK(tab_to_sidebar_container_.empty());
  DCHECK(sidebar_container_to_tab_.empty());
}

void SidebarManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    HideSidebar(content::Source<WebContents>(source).ptr());
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

SidebarContainer* SidebarManager::FindSidebarContainerFor(
    content::WebContents* sidebar_contents) {
  for (SidebarContainerToTabMap::iterator it = sidebar_container_to_tab_.begin();
       it != sidebar_container_to_tab_.end(); ++it) {
    if (sidebar_contents == it->first->host_contents())
      return it->first;
  }
  return nullptr;
}

void SidebarManager::BindSidebarContainer(WebContents* tab,
                                          SidebarContainer* container) {

  tab_to_sidebar_container_[tab] = container;
  sidebar_container_to_tab_[container] = tab;
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(tab));
}

void SidebarManager::UnbindSidebarContainer(WebContents* tab,
                                       SidebarContainer* container) {
  tab_to_sidebar_container_.erase(tab);
  sidebar_container_to_tab_.erase(container);
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(tab));
}

void SidebarManager::AddObserver(SidebarManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SidebarManager::RemoveObserver(SidebarManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

} // namespace extensions
