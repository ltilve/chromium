// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_message_filter.h"

#include "base/callback.h"
#include "content/browser/notifications/page_notification_delegate.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

NotificationMessageFilter::NotificationMessageFilter(
    int process_id,
    PlatformNotificationContextImpl* notification_context,
    ResourceContext* resource_context,
    BrowserContext* browser_context)
    : BrowserMessageFilter(PlatformNotificationMsgStart),
      process_id_(process_id),
      notification_context_(notification_context),
      resource_context_(resource_context),
      browser_context_(browser_context),
      weak_factory_io_(this) {}

NotificationMessageFilter::~NotificationMessageFilter() {}

void NotificationMessageFilter::DidCloseNotification(int notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool NotificationMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationMessageFilter, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Show,
                        OnShowPlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ShowPersistent,
                        OnShowPersistentNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_GetNotifications,
                        OnGetNotifications)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Close,
                        OnClosePlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ClosePersistent,
                        OnClosePersistentNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PlatformNotificationHostMsg_Show::ID ||
      message.type() == PlatformNotificationHostMsg_Close::ID)
    *thread = BrowserThread::UI;
}

void NotificationMessageFilter::OnCheckNotificationPermission(
    const GURL& origin, blink::WebNotificationPermission* permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  *permission = GetPermissionForOriginOnIO(origin);
}

void NotificationMessageFilter::OnShowPlatformNotification(
    int notification_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  scoped_ptr<DesktopNotificationDelegate> delegate(
      new PageNotificationDelegate(process_id_, notification_id));

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  if (!VerifyNotificationPermissionGranted(service, origin))
    return;

  base::Closure close_closure;
  service->DisplayNotification(browser_context_,
                               origin,
                               icon,
                               notification_data,
                               delegate.Pass(),
                               &close_closure);

  if (!close_closure.is_null())
    close_closures_[notification_id] = close_closure;
}

void NotificationMessageFilter::OnShowPersistentNotification(
    int request_id,
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetPermissionForOriginOnIO(origin) !=
          blink::WebNotificationPermissionAllowed) {
    BadMessageReceived();
    return;
  }

  NotificationDatabaseData database_data;
  database_data.origin = origin;
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = notification_data;

  // TODO(peter): Significantly reduce the amount of information we need to
  // retain outside of the database for displaying notifications.
  notification_context_->WriteNotificationData(
      origin,
      database_data,
      base::Bind(&NotificationMessageFilter::DidWritePersistentNotificationData,
                 weak_factory_io_.GetWeakPtr(),
                 request_id,
                 origin,
                 icon,
                 notification_data));
}

void NotificationMessageFilter::DidWritePersistentNotificationData(
    int request_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data,
    bool success,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (success) {
    PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
    DCHECK(service);

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&PlatformNotificationService::DisplayPersistentNotification,
                   base::Unretained(service),  // The service is a singleton.
                   browser_context_,
                   persistent_notification_id,
                   origin,
                   icon,
                   notification_data));
  }

  Send(new PlatformNotificationMsg_DidShowPersistent(request_id, success));
}

void NotificationMessageFilter::OnGetNotifications(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& filter_tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Implement retrieval of persistent Web Notifications from the
  // database. Reply with an empty vector until this has been implemented.
  // Tracked in https://crbug.com/442143.

  Send(new PlatformNotificationMsg_DidGetNotifications(
      request_id,
      std::vector<PersistentNotificationInfo>()));
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  if (!close_closures_.count(notification_id))
    return;

  close_closures_[notification_id].Run();
  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnClosePersistentNotification(
    const GURL& origin,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetPermissionForOriginOnIO(origin) !=
          blink::WebNotificationPermissionAllowed) {
    BadMessageReceived();
    return;
  }

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  // There's no point in waiting until the database data has been removed before
  // closing the notification presented to the user. Post that task immediately.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PlatformNotificationService::ClosePersistentNotification,
                 base::Unretained(service),  // The service is a singleton.
                 browser_context_,
                 persistent_notification_id));

  notification_context_->DeleteNotificationData(
      persistent_notification_id,
      origin,
      base::Bind(&NotificationMessageFilter::
                     DidDeletePersistentNotificationData,
                 weak_factory_io_.GetWeakPtr()));
}

void NotificationMessageFilter::DidDeletePersistentNotificationData(
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(peter): Consider feeding back to the renderer that the notification
  // has been closed.
}

blink::WebNotificationPermission
NotificationMessageFilter::GetPermissionForOriginOnIO(
    const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (!service)
    return blink::WebNotificationPermissionDenied;

  return service->CheckPermissionOnIOThread(resource_context_,
                                            origin,
                                            process_id_);
}

bool NotificationMessageFilter::VerifyNotificationPermissionGranted(
    PlatformNotificationService* service,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::WebNotificationPermission permission =
      service->CheckPermissionOnUIThread(browser_context_,
                                         origin,
                                         process_id_);

  if (permission == blink::WebNotificationPermissionAllowed)
    return true;

  BadMessageReceived();
  return false;
}

}  // namespace content
