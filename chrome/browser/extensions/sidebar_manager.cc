// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sidebar_manager.h"

#include "chrome/browser/extensions/sidebar_container.h"
#include "chrome/browser/extensions/sidebar_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "url/gurl.h"

using content::WebContents;

namespace extensions {

// static
SidebarManager* SidebarManager::GetFromContext(
    content::BrowserContext* context) {
  return ExtensionSystem::Get(context)->sidebar_manager();
}

SidebarManager::SidebarManager() {
}

SidebarManager::~SidebarManager() {
  DCHECK(tab_to_sidebar_container_.empty());
}

SidebarContainer* SidebarManager::GetSidebarContainerFor(
    content::WebContents* tab) {
  TabToSidebarContainerMap::iterator it = tab_to_sidebar_container_.find(tab);
  return it == tab_to_sidebar_container_.end() ? nullptr : it->second;
}

void SidebarManager::CreateSidebar(content::WebContents* tab,
                                   const GURL& url,
                                   Browser* browser) {
  DCHECK(tab);
  SidebarContainer* container = GetSidebarContainerFor(tab);

  HideSidebar(tab);

  container = new SidebarContainer(browser, tab, url);
  tab_to_sidebar_container_[tab] = container;

  const std::string id = container->extension_id();
  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarShown(tab, id));
}

void SidebarManager::HideSidebar(WebContents* tab) {
  DCHECK(tab);
  SidebarContainer* container = GetSidebarContainerFor(tab);
  if (!container)
    return;

  const std::string content_id = container->extension_id();
  tab_to_sidebar_container_.erase(tab);
  delete container;

  FOR_EACH_OBSERVER(SidebarManagerObserver, observer_list_,
                    OnSidebarHidden(tab, content_id));
}

void SidebarManager::AddObserver(SidebarManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SidebarManager::RemoveObserver(SidebarManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace extensions
