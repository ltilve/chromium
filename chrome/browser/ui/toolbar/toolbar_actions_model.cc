// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include <algorithm>
#include <string>

#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_toolbar_icon_surfacing_bubble_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/one_shot_event.h"

ToolbarActionsModel::ToolbarActionsModel(
    Profile* profile,
    extensions::ExtensionPrefs* extension_prefs)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      prefs_(profile_->GetPrefs()),
      extension_action_api_(extensions::ExtensionActionAPI::Get(profile_)),
      extension_registry_(extensions::ExtensionRegistry::Get(profile_)),
      actions_initialized_(false),
      use_redesign_(extensions::FeatureSwitch::extension_action_redesign()
                        ->IsEnabled()),
      highlight_type_(HIGHLIGHT_NONE),
      extension_action_observer_(this),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::Bind(&ToolbarActionsModel::OnReady,
                            weak_ptr_factory_.GetWeakPtr()));
  visible_icon_count_ =
      prefs_->GetInteger(extensions::pref_names::kToolbarSize);

  // We only care about watching the prefs if not in incognito mode.
  if (!profile_->IsOffTheRecord()) {
    pref_change_registrar_.Init(prefs_);
    pref_change_callback_ =
        base::Bind(&ToolbarActionsModel::OnActionToolbarPrefChange,
                   base::Unretained(this));
    pref_change_registrar_.Add(extensions::pref_names::kToolbar,
                               pref_change_callback_);
  }
}

ToolbarActionsModel::~ToolbarActionsModel() {}

// static
ToolbarActionsModel* ToolbarActionsModel::Get(Profile* profile) {
  return ToolbarActionsModelFactory::GetForProfile(profile);
}

void ToolbarActionsModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ToolbarActionsModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ToolbarActionsModel::MoveActionIcon(const std::string& id, size_t index) {
  std::vector<ToolbarItem>::iterator pos = toolbar_items_.begin();
  while (pos != toolbar_items_.end() && (*pos).id != id)
    ++pos;
  if (pos == toolbar_items_.end()) {
    NOTREACHED();
    return;
  }

  ToolbarItem action = *pos;
  toolbar_items_.erase(pos);

  std::vector<std::string>::iterator pos_id =
      std::find(last_known_positions_.begin(), last_known_positions_.end(), id);
  if (pos_id != last_known_positions_.end())
    last_known_positions_.erase(pos_id);

  if (index < toolbar_items_.size()) {
    // If the index is not at the end, find the item currently at |index|, and
    // insert |action| before it in |toolbar_items_| and |action|'s id in
    // |last_known_positions_|.
    std::vector<ToolbarItem>::iterator iter = toolbar_items_.begin() + index;
    last_known_positions_.insert(
        std::find(last_known_positions_.begin(), last_known_positions_.end(),
                  iter->id),
        id);
    toolbar_items_.insert(iter, action);
  } else {
    // Otherwise, put |action| and |id| at the end.
    DCHECK_EQ(toolbar_items_.size(), index);
    toolbar_items_.push_back(action);
    last_known_positions_.push_back(id);
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnToolbarActionMoved(id, index));
  MaybeUpdateVisibilityPref(action, index);
  UpdatePrefs();
}

void ToolbarActionsModel::SetVisibleIconCount(size_t count) {
  visible_icon_count_ = (count >= toolbar_items_.size()) ? -1 : count;

  // Only set the prefs if we're not in highlight mode and the profile is not
  // incognito. Highlight mode is designed to be a transitory state, and should
  // not persist across browser restarts (though it may be re-entered), and we
  // don't store anything in incognito.
  if (!is_highlighting() && !profile_->IsOffTheRecord()) {
    // Additionally, if we are using the new toolbar, any icons which are in the
    // overflow menu are considered "hidden". But it so happens that the times
    // we are likely to call SetVisibleIconCount() are also those when we are
    // in flux. So wait for things to cool down before setting the prefs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ToolbarActionsModel::MaybeUpdateVisibilityPrefs,
                              weak_ptr_factory_.GetWeakPtr()));
    prefs_->SetInteger(extensions::pref_names::kToolbarSize,
                       visible_icon_count_);
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnToolbarVisibleCountChanged());
}

void ToolbarActionsModel::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  // Notify observers if the extension exists and is in the model.
  if (std::find(toolbar_items_.begin(), toolbar_items_.end(),
                ToolbarItem(extension_action->extension_id(),
                            EXTENSION_ACTION)) != toolbar_items_.end()) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnToolbarActionUpdated(extension_action->extension_id()));
  }
}

ScopedVector<ToolbarActionViewController> ToolbarActionsModel::CreateActions(
    Browser* browser,
    ToolbarActionsBar* bar) {
  DCHECK(browser);
  DCHECK(bar);
  ScopedVector<ToolbarActionViewController> action_list;

  // Get the component action list.
  ScopedVector<ToolbarActionViewController> component_actions =
      ComponentToolbarActionsFactory::GetInstance()->GetComponentToolbarActions(
          browser);

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);

  // toolbar_items() might not equate to toolbar_items_ in the case where a
  // subset are highlighted.
  for (const ToolbarItem& item : toolbar_items()) {
    switch (item.type) {
      case EXTENSION_ACTION: {
        // Get the extension.
        const extensions::Extension* extension = GetExtensionById(item.id);
        DCHECK(extension);

        // Create and add an ExtensionActionViewController for the extension.
        action_list.push_back(new ExtensionActionViewController(
            extension, browser, action_manager->GetExtensionAction(*extension),
            bar));
        break;
      }
      case COMPONENT_ACTION: {
        DCHECK(use_redesign_);
        // Find the index of the component action with the id.
        auto iter = std::find_if(
            component_actions.begin(), component_actions.end(),
            [&item](const ToolbarActionViewController* action) {
          return action->GetId() == item.id;
        });
        // We should always find a corresponding action.
        DCHECK(iter != component_actions.end());
        action_list.push_back(*iter);

        // We have moved ownership of the action from |component_actions| to
        // |action_list|.
        component_actions.weak_erase(iter);
        break;
      }
      case UNKNOWN_ACTION:
        NOTREACHED();  // Should never have an UNKNOWN_ACTION in toolbar_items.
        break;
    }
  }

  // We've moved ownership of the subset of the component actions that we
  // kept track of via toolbar_items() from |component_actions| to
  // |action_list|. The rest will be deleted when |component_actions| goes out
  // of scope.

  return action_list.Pass();
}

void ToolbarActionsModel::OnExtensionActionVisibilityChanged(
    const std::string& extension_id,
    bool is_now_visible) {
  // Hiding works differently with the new and old toolbars.
  if (use_redesign_) {
    // It's possible that we haven't added this action yet, if its
    // visibility was adjusted in the course of its initialization.
    if (std::find(toolbar_items_.begin(), toolbar_items_.end(),
                  ToolbarItem(extension_id, EXTENSION_ACTION)) ==
        toolbar_items_.end())
      return;

    int new_size = 0;
    int new_index = 0;
    if (is_now_visible) {
      // If this action used to be hidden, we can't possibly be showing all.
      DCHECK_LT(visible_icon_count(), toolbar_items_.size());
      // Grow the bar by one and move the action to the end of the visibles.
      new_size = visible_icon_count() + 1;
      new_index = new_size - 1;
    } else {
      // If we're hiding one, we must be showing at least one.
      DCHECK_GE(visible_icon_count(), 0u);
      // Shrink the bar by one and move the action to the beginning of the
      // overflow menu.
      new_size = visible_icon_count() - 1;
      new_index = new_size;
    }
    SetVisibleIconCount(new_size);
    MoveActionIcon(extension_id, new_index);
  } else {  // Don't include all extensions.
    const extensions::Extension* extension = GetExtensionById(extension_id);
    if (is_now_visible)
      AddExtension(extension);
    else
      RemoveExtension(extension);
  }
}

void ToolbarActionsModel::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // We don't want to add the same extension twice. It may have already been
  // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
  // hides the browser action and then disables and enables the extension.
  if (std::find(toolbar_items_.begin(), toolbar_items_.end(),
                ToolbarItem(extension->id(), EXTENSION_ACTION)) !=
      toolbar_items_.end())
    return;

  AddExtension(extension);
}

void ToolbarActionsModel::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  RemoveExtension(extension);
}

void ToolbarActionsModel::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // Remove the extension id from the ordered list, if it exists (the extension
  // might not be represented in the list because it might not have an icon).
  std::vector<std::string>::iterator pos =
      std::find(last_known_positions_.begin(), last_known_positions_.end(),
                extension->id());

  if (pos != last_known_positions_.end()) {
    last_known_positions_.erase(pos);
    UpdatePrefs();
  }
}

void ToolbarActionsModel::OnReady() {
  InitializeActionList();
  // Wait until the extension system is ready before observing any further
  // changes so that the toolbar buttons can be shown in their stable ordering
  // taken from prefs.
  extension_registry_observer_.Add(extension_registry_);
  extension_action_observer_.Add(extension_action_api_);

  if (ExtensionToolbarIconSurfacingBubbleDelegate::ShouldShowForProfile(
          profile_)) {
    std::vector<std::string> ids;
    for (const ToolbarItem& action : toolbar_items_)
      ids.push_back(action.id);
    HighlightActions(ids, HIGHLIGHT_INFO);
  }

  actions_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnToolbarModelInitialized());
}

size_t ToolbarActionsModel::FindNewPositionFromLastKnownGood(
    const ToolbarItem& action) {
  // See if we have last known good position for this action.
  size_t new_index = 0;
  // Loop through the ID list of known positions, to count the number of
  // visible action icons preceding |action|'s id.
  for (const std::string& last_pos_id : last_known_positions_) {
    if (last_pos_id == action.id)
      return new_index;  // We've found the right position.
    // Found an action, need to see if it is visible.
    for (const ToolbarItem& item : toolbar_items_) {
      if (item.id == last_pos_id) {
        // This extension is visible, update the index value.
        ++new_index;
        break;
      }
    }
  }

  // Position not found.
  return toolbar_items_.size();
}

bool ToolbarActionsModel::ShouldAddExtension(
    const extensions::Extension* extension) {
  // In incognito mode, don't add any extensions that aren't incognito-enabled.
  if (profile_->IsOffTheRecord() &&
      !extensions::util::IsIncognitoEnabled(extension->id(), profile_))
    return false;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  if (use_redesign_) {
    // In this case, we don't care about the browser action visibility, because
    // we want to show each extension regardless.
    return action_manager->GetExtensionAction(*extension) != NULL;
  }

  return action_manager->GetBrowserAction(*extension) &&
         extension_action_api_->GetBrowserActionVisibility(extension->id());
}

void ToolbarActionsModel::AddExtension(const extensions::Extension* extension) {
  // We only use AddExtension() once the system is initialized.
  DCHECK(actions_initialized_);
  if (!ShouldAddExtension(extension))
    return;

  // See if we have a last known good position for this extension.
  bool is_new_extension =
      std::find(last_known_positions_.begin(), last_known_positions_.end(),
                extension->id()) == last_known_positions_.end();

  // New extensions go at the right (end) of the visible extensions. Other
  // extensions go at their previous position.
  size_t new_index = 0;
  if (is_new_extension) {
    new_index = extensions::Manifest::IsComponentLocation(extension->location())
                    ? 0
                    : visible_icon_count();
    // For the last-known position, we use the index of the extension that is
    // just before this extension, plus one. (Note that this isn't the same
    // as new_index + 1, because last_known_positions_ can include disabled
    // extensions.)
    int new_last_known_index =
        new_index == 0 ? 0 : std::find(last_known_positions_.begin(),
                                       last_known_positions_.end(),
                                       toolbar_items_[new_index - 1].id) -
                                 last_known_positions_.begin() + 1;
    // In theory, the extension before this one should always
    // be in last known positions, but if something funny happened with prefs,
    // make sure we handle it.
    // TODO(devlin): Track down these cases so we can CHECK this.
    new_last_known_index =
        std::min<int>(new_last_known_index, last_known_positions_.size());
    last_known_positions_.insert(
        last_known_positions_.begin() + new_last_known_index, extension->id());
    UpdatePrefs();
  } else {
    new_index = FindNewPositionFromLastKnownGood(
        ToolbarItem(extension->id(), EXTENSION_ACTION));
  }

  toolbar_items_.insert(toolbar_items_.begin() + new_index,
                        ToolbarItem(extension->id(), EXTENSION_ACTION));

  // If we're currently highlighting, then even though we add a browser action
  // to the full list (|toolbar_items_|, there won't be another *visible*
  // browser action, which was what the observers care about.
  if (!is_highlighting()) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnToolbarActionAdded(extension->id(), new_index));

    int visible_count_delta = 0;
    if (is_new_extension && !all_icons_visible()) {
      // If this is a new extension (and not all extensions are visible), we
      // expand the toolbar out so that the new one can be seen.
      visible_count_delta = 1;
    } else if (profile_->IsOffTheRecord()) {
      // If this is an incognito profile, we also have to check to make sure the
      // overflow matches the main bar's status.
      ToolbarActionsModel* main_model =
          ToolbarActionsModel::Get(profile_->GetOriginalProfile());
      // Find what the index will be in the main bar. Because Observer calls are
      // nondeterministic, we can't just assume the main bar will have the
      // extension and look it up.
      size_t main_index = main_model->FindNewPositionFromLastKnownGood(
          ToolbarItem(extension->id(), EXTENSION_ACTION));
      bool visible = main_index < main_model->visible_icon_count();
      // We may need to adjust the visible count if the incognito bar isn't
      // showing all icons and this one is visible, or if it is showing all
      // icons and this is hidden.
      if (visible && !all_icons_visible())
        visible_count_delta = 1;
      else if (!visible && all_icons_visible())
        visible_count_delta = -1;
    }

    if (visible_count_delta)
      SetVisibleIconCount(visible_icon_count() + visible_count_delta);
  }

  MaybeUpdateVisibilityPref(ToolbarItem(extension->id(), EXTENSION_ACTION),
                            new_index);
}

void ToolbarActionsModel::RemoveExtension(
    const extensions::Extension* extension) {
  std::vector<ToolbarItem>::iterator pos =
      std::find(toolbar_items_.begin(), toolbar_items_.end(),
                ToolbarItem(extension->id(), EXTENSION_ACTION));

  if (pos == toolbar_items_.end())
    return;

  size_t index = pos - toolbar_items_.begin();
  // If the removed extension was on the toolbar, a new one will take its place
  // if there are any in overflow.
  bool new_extension_shown =
      !all_icons_visible() && index < visible_icon_count();

  // If our visible count is set to the current size, we need to decrement it.
  if (visible_icon_count_ == static_cast<int>(toolbar_items_.size()))
    SetVisibleIconCount(toolbar_items_.size() - 1);

  toolbar_items_.erase(pos);

  // If we're in highlight mode, we also have to remove the extension from
  // the highlighted list.
  if (is_highlighting()) {
    pos = std::find(highlighted_items_.begin(), highlighted_items_.end(),
                    ToolbarItem(extension->id(), EXTENSION_ACTION));
    if (pos != highlighted_items_.end()) {
      highlighted_items_.erase(pos);
      FOR_EACH_OBSERVER(Observer, observers_,
                        OnToolbarActionRemoved(extension->id()));
      // If the highlighted list is now empty, we stop highlighting.
      if (highlighted_items_.empty())
        StopHighlighting();
    }
  } else {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnToolbarActionRemoved(extension->id()));
  }

  UpdatePrefs();
  if (new_extension_shown) {
    size_t newly_visible_index = visible_icon_count() - 1;
    MaybeUpdateVisibilityPref(toolbar_items_[newly_visible_index],
                              newly_visible_index);
  }
}

// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionRegistry) and component actions (which we get from
// ComponentToolbarActionsFactory) with the ordering we get from the pref
// service. For robustness we use a somewhat inefficient process:
// 1. Create a vector of actions sorted by their pref values. This vector may
// have holes.
// 2. Create a vector of actions that did not have a pref value.
// 3. Remove holes from the sorted vector and append the unsorted vector.
void ToolbarActionsModel::InitializeActionList() {
  DCHECK(toolbar_items_.empty());  // We shouldn't have any items yet.

  last_known_positions_ = extension_prefs_->GetToolbarOrder();
  if (profile_->IsOffTheRecord())
    IncognitoPopulate();
  else
    Populate();

  MaybeUpdateVisibilityPrefs();
}

void ToolbarActionsModel::Populate() {
  DCHECK(!profile_->IsOffTheRecord());

  std::vector<ToolbarItem> all_actions;
  // Ids of actions that have explicit positions.
  std::vector<ToolbarItem> sorted(last_known_positions_.size(), ToolbarItem());
  // Ids of actions that don't have explicit positions.
  std::vector<ToolbarItem> unsorted;

  // Populate the lists.
  int hidden = 0;
  int browser_actions_count = 0;
  int component_actions_count = 0;

  // First, add the extension action ids to all_actions.
  const extensions::ExtensionSet& extensions =
      extension_registry_->enabled_extensions();
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions) {
    if (!ShouldAddExtension(extension.get())) {
      if (!extension_action_api_->GetBrowserActionVisibility(extension->id()))
        ++hidden;
      continue;
    }

    all_actions.push_back(ToolbarItem(extension->id(), EXTENSION_ACTION));
  }

  // Next, add the component action ids.
  std::vector<std::string> component_ids =
      ComponentToolbarActionsFactory::GetComponentIds();
  for (const std::string& id : component_ids)
    all_actions.push_back(ToolbarItem(id, COMPONENT_ACTION));

  // Add each action id to the appropriate list. Since the |sorted| list is
  // created with enough room for each id in |positions| (which helps with
  // proper order insertion), holes can be present if there isn't an action
  // for each id. This is handled below when we add the actions to
  // |toolbar_items_| to ensure that there are never any holes in
  // |toolbar_items_| itself (or, relatedly, CreateActions()).
  for (const ToolbarItem& action : all_actions) {
    std::vector<std::string>::const_iterator pos =
        std::find(last_known_positions_.begin(), last_known_positions_.end(),
                  action.id);
    if (pos != last_known_positions_.end()) {
      sorted[pos - last_known_positions_.begin()] = action;
    } else {
      // Unknown action - push it to the back of unsorted, and add it to the
      // list of ids at the end.
      unsorted.push_back(action);
      last_known_positions_.push_back(action.id);
    }
  }

  // Merge the lists.
  sorted.insert(sorted.end(), unsorted.begin(), unsorted.end());
  toolbar_items_.reserve(sorted.size());

  // We don't notify observers of the added extension yet. Rather, observers
  // should wait for the "OnToolbarModelInitialized" notification, and then
  // bulk-update. (This saves a lot of bouncing-back-and-forth here, and allows
  // observers to ensure that the extension system is always initialized before
  // using the extensions).
  for (const ToolbarItem& action : sorted) {
    switch (action.type) {
      case EXTENSION_ACTION:
        // It's possible for the extension order to contain items that aren't
        // actually loaded on this machine.  For example, when extension sync is
        // on, we sync the extension order as-is but double-check with the user
        // before syncing NPAPI-containing extensions, so if one of those is not
        // actually synced, we'll get a NULL in the list.  This sort of case can
        // also happen if some error prevents an extension from loading.
        if (GetExtensionById(action.id)) {
          toolbar_items_.push_back(ToolbarItem(action.id, EXTENSION_ACTION));
          ++browser_actions_count;
        }
        break;
      case COMPONENT_ACTION:
        toolbar_items_.push_back(ToolbarItem(action.id, COMPONENT_ACTION));
        ++component_actions_count;
        break;
      case UNKNOWN_ACTION:
        // Since |sorted| can have holes in it, they will be default-constructed
        // ToolbarItems with an action type of UNKNOWN. Ignore them.
        break;
    }
  }

  // Histogram names are prefixed with "ExtensionToolbarModel" rather than
  // "ToolbarActionsModel" for historical reasons.
  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionToolbarModel.BrowserActionsPermanentlyHidden", hidden);
  UMA_HISTOGRAM_COUNTS_100("ExtensionToolbarModel.BrowserActionsCount",
                           browser_actions_count);
  UMA_HISTOGRAM_COUNTS_100("Toolbar.ActionsModel.ComponentActionsCount",
                           component_actions_count);
  UMA_HISTOGRAM_COUNTS_100("Toolbar.ActionsModel.OverallActionsCount",
                           toolbar_items_.size());

  if (!toolbar_items_.empty()) {
    // Visible count can be -1, meaning: 'show all'. Since UMA converts negative
    // values to 0, this would be counted as 'show none' unless we convert it to
    // max.
    UMA_HISTOGRAM_COUNTS_100(
        "ExtensionToolbarModel.BrowserActionsVisible",
        visible_icon_count_ == -1
            ? visible_icon_count_
            : visible_icon_count_ - component_actions_count);

    if (use_redesign_) {
      // The only time this will useful and possibly vary from
      // BrowserActionsVisible is when the redesign has been enabled.
      UMA_HISTOGRAM_COUNTS_100("Toolbar.ActionsModel.ToolbarActionsVisible",
                               visible_icon_count_ == -1
                                   ? base::HistogramBase::kSampleType_MAX
                                   : visible_icon_count_);
    }
  }
}

void ToolbarActionsModel::IncognitoPopulate() {
  DCHECK(profile_->IsOffTheRecord());
  const ToolbarActionsModel* original_model =
      ToolbarActionsModel::Get(profile_->GetOriginalProfile());

  // Find the absolute value of the original model's count.
  int original_visible = original_model->visible_icon_count();

  // In incognito mode, we show only those actions that are incognito-enabled
  // Further, any actions that were overflowed in regular mode are still
  // overflowed. Order is the same as in regular mode.
  visible_icon_count_ = 0;

  for (std::vector<ToolbarItem>::const_iterator iter =
           original_model->toolbar_items_.begin();
       iter != original_model->toolbar_items_.end(); ++iter) {
    // The extension might not be shown in incognito mode.
    // We may also disable certain component actions in incognito mode.
    bool should_add = false;
    switch (iter->type) {
      case EXTENSION_ACTION:
        should_add = ShouldAddExtension(GetExtensionById(iter->id));
        break;
      case COMPONENT_ACTION:
        should_add = ComponentToolbarActionsFactory::EnabledIncognito(iter->id);
        break;
      case UNKNOWN_ACTION:
        // We should never have an uninitialized action in the model.
        NOTREACHED();
        break;
    }
    if (!should_add)
      continue;
    toolbar_items_.push_back(*iter);
    if (iter - original_model->toolbar_items_.begin() < original_visible)
      ++visible_icon_count_;
  }
}

void ToolbarActionsModel::UpdatePrefs() {
  if (!extension_prefs_ || profile_->IsOffTheRecord())
    return;

  // Don't observe change caused by self.
  pref_change_registrar_.Remove(extensions::pref_names::kToolbar);
  extension_prefs_->SetToolbarOrder(last_known_positions_);
  pref_change_registrar_.Add(extensions::pref_names::kToolbar,
                             pref_change_callback_);
}

void ToolbarActionsModel::MaybeUpdateVisibilityPref(const ToolbarItem& action,
                                                    size_t index) {
  // Component actions don't have prefs to update.
  if (action.type == COMPONENT_ACTION)
    return;

  // We only update the visibility pref for hidden/not hidden based on the
  // overflow menu with the new toolbar design.
  if (use_redesign_ && !profile_->IsOffTheRecord()) {
    bool visible = index < visible_icon_count();
    if (visible !=
        extension_action_api_->GetBrowserActionVisibility(action.id)) {
      // Don't observe changes caused by ourselves.
      bool was_registered = false;
      if (extension_action_observer_.IsObserving(extension_action_api_)) {
        was_registered = true;
        extension_action_observer_.RemoveAll();
      }
      extension_action_api_->SetBrowserActionVisibility(action.id, visible);
      if (was_registered)
        extension_action_observer_.Add(extension_action_api_);
    }
  }
}

void ToolbarActionsModel::MaybeUpdateVisibilityPrefs() {
  for (size_t i = 0u; i < toolbar_items_.size(); ++i)
    MaybeUpdateVisibilityPref(toolbar_items_[i], i);
}

void ToolbarActionsModel::OnActionToolbarPrefChange() {
  // If extensions are not ready, defer to later Populate() call.
  if (!actions_initialized_)
    return;

  // Recalculate |last_known_positions_| to be |pref_positions| followed by
  // ones that are only in |last_known_positions_|.
  std::vector<std::string> pref_positions = extension_prefs_->GetToolbarOrder();
  size_t pref_position_size = pref_positions.size();
  for (size_t i = 0; i < last_known_positions_.size(); ++i) {
    if (std::find(pref_positions.begin(), pref_positions.end(),
                  last_known_positions_[i]) == pref_positions.end()) {
      pref_positions.push_back(last_known_positions_[i]);
    }
  }
  last_known_positions_.swap(pref_positions);

  // Loop over the updated list of last known positions, moving any extensions
  // that are in the wrong place.
  auto desired_pos = toolbar_items_.begin();
  for (const std::string& id : last_known_positions_) {
    auto current_pos = std::find_if(
        toolbar_items_.begin(), toolbar_items_.end(),
        [&id](const ToolbarItem& item) { return item.id == id; });
    if (current_pos == toolbar_items_.end())
      continue;

    if (current_pos != desired_pos) {
      if (current_pos < desired_pos)
        std::rotate(current_pos, current_pos + 1, desired_pos + 1);
      else
        std::rotate(desired_pos, current_pos, current_pos + 1);
      // Notify the observers to keep them up-to-date, unless we're highlighting
      // (in which case we're deliberately only showing a subset of actions).
      if (!is_highlighting())
        FOR_EACH_OBSERVER(
            Observer, observers_,
            OnToolbarActionMoved(id, desired_pos - toolbar_items_.begin()));
    }
    ++desired_pos;
  }

  if (last_known_positions_.size() > pref_position_size) {
    // Need to update pref because we have extra icons. But can't call
    // UpdatePrefs() directly within observation closure.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ToolbarActionsModel::UpdatePrefs,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ToolbarActionsModel::HighlightActions(const std::vector<std::string>& ids,
                                           HighlightType highlight_type) {
  highlighted_items_.clear();

  for (const std::string& action_id : ids) {
    for (const ToolbarItem& item : toolbar_items_) {
      if (action_id == item.id)
        highlighted_items_.push_back(item);
    }
  }

  // If we have any items in |highlighted_items_|, then we entered highlighting
  // mode.
  if (highlighted_items_.size()) {
    // It's important that is_highlighting_ is changed immediately before the
    // observers are notified since it changes the result of toolbar_items().
    highlight_type_ = highlight_type;
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnToolbarHighlightModeChanged(true));

    // We set the visible icon count after the highlight mode change because
    // the UI actions are created/destroyed during highlight, and doing that
    // prior to changing the size allows us to still have smooth animations.
    if (visible_icon_count() < ids.size())
      SetVisibleIconCount(ids.size());

    return true;
  }

  // Otherwise, we didn't enter highlighting mode (and, in fact, exited it if
  // we were otherwise in it).
  if (is_highlighting())
    StopHighlighting();
  return false;
}

void ToolbarActionsModel::StopHighlighting() {
  if (is_highlighting()) {
    // It's important that is_highlighting_ is changed immediately before the
    // observers are notified since it changes the result of toolbar_items().
    highlight_type_ = HIGHLIGHT_NONE;
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnToolbarHighlightModeChanged(false));

    // For the same reason, we don't clear highlighted_items_ until after the
    // mode changed.
    highlighted_items_.clear();

    // We set the visible icon count after the highlight mode change because
    // the UI actions are created/destroyed during highlight, and doing that
    // prior to changing the size allows us to still have smooth animations.
    int saved_icon_count =
        prefs_->GetInteger(extensions::pref_names::kToolbarSize);
    if (saved_icon_count != visible_icon_count_)
      SetVisibleIconCount(saved_icon_count);
  }
}

bool ToolbarActionsModel::RedesignIsShowingNewIcons() const {
  for (const ToolbarItem& action : toolbar_items_) {
    if (action.type == EXTENSION_ACTION) {
      // Without the redesign, we only show extensions with browser actions.
      // Any extension without a browser action is an indication that we're
      // showing something new.
      if (!GetExtensionById(action.id)->manifest()->HasKey(
              extensions::manifest_keys::kBrowserAction))
        return true;
    }
  }
  return false;
}

const extensions::Extension* ToolbarActionsModel::GetExtensionById(
    const std::string& id) const {
  return extension_registry_->enabled_extensions().GetByID(id);
}
