// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_test_util.h"

MockNotificationDelegate::MockNotificationDelegate(const std::string& id)
    : id_(id) {}

MockNotificationDelegate::~MockNotificationDelegate() {}

std::string MockNotificationDelegate::id() const { return id_; }

// -----------------------------------------------------------------------------

StubNotificationUIManager::StubNotificationUIManager() {}

StubNotificationUIManager::~StubNotificationUIManager() {}

unsigned int StubNotificationUIManager::GetNotificationCount() const {
  return notifications_.size();
}

const Notification& StubNotificationUIManager::GetNotificationAt(
    unsigned int index) const {
  DCHECK_GT(GetNotificationCount(), index);
  return notifications_[index].first;
}

void StubNotificationUIManager::SetNotificationAddedCallback(
    const base::Closure& callback) {
  notification_added_callback_ = callback;
}

void StubNotificationUIManager::Add(const Notification& notification,
                                    Profile* profile) {
  notifications_.push_back(std::make_pair(notification, profile));

  if (!notification_added_callback_.is_null()) {
    notification_added_callback_.Run();
    notification_added_callback_.Reset();
  }

  // Fire the Display() event on the delegate.
  notification.delegate()->Display();
}

bool StubNotificationUIManager::Update(const Notification& notification,
                                       Profile* profile) {
  return false;
}

const Notification* StubNotificationUIManager::FindById(
    const std::string& delegate_id,
    ProfileID profile_id) const {
  return nullptr;
}

bool StubNotificationUIManager::CancelById(const std::string& delegate_id,
                                           ProfileID profile_id) {
  auto iter = notifications_.begin();
  for (; iter != notifications_.end(); ++iter) {
    if (iter->first.delegate_id() != delegate_id ||
        iter->second != profile_id)
      continue;

    iter->first.delegate()->Close(false /* by_user */);
    notifications_.erase(iter);
    return true;
  }

  return false;
}

std::set<std::string>
StubNotificationUIManager::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  std::set<std::string> delegate_ids;
  for (const auto& pair : notifications_) {
    if (pair.second == profile && pair.first.origin_url() == source)
      delegate_ids.insert(pair.first.delegate_id());
  }
  return delegate_ids;
}

bool StubNotificationUIManager::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  NOTIMPLEMENTED();
  return false;
}

bool StubNotificationUIManager::CancelAllByProfile(ProfileID profile_id) {
  NOTIMPLEMENTED();
  return false;
}

void StubNotificationUIManager::CancelAll() {
  for (const auto& pair : notifications_)
    pair.first.delegate()->Close(false /* by_user */);
  notifications_.clear();
}
