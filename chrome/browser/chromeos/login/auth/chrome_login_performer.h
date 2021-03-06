// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/login_performer.h"
#include "chromeos/login/auth/online_attempt_host.h"
#include "chromeos/login/auth/user_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {
class WildcardLoginChecker;
}

namespace chromeos {

// This class implements chrome-specific elements of Login Performer.

class ChromeLoginPerformer : public LoginPerformer {
 public:
  explicit ChromeLoginPerformer(Delegate* delegate);
  ~ChromeLoginPerformer() override;

 protected:
  bool RunTrustedCheck(const base::Closure& callback) override;
  void DidRunTrustedCheck(const base::Closure& callback);
  bool IsUserWhitelisted(const std::string& user_id,
                         bool* wildcard_match) override;

  void RunOnlineWhitelistCheck(const std::string& user_id,
                               bool wildcard_match,
                               const base::Closure& success_callback,
                               const base::Closure& failure_callback) override;
  bool AreSupervisedUsersAllowed() override;

  bool UseExtendedAuthenticatorForSupervisedUser(
      const UserContext& user_context) override;

  UserContext TransformSupervisedKey(const UserContext& context) override;

  void SetupSupervisedUserFlow(const std::string& user_id) override;

  void SetupEasyUnlockUserFlow(const std::string& user_id) override;

  scoped_refptr<Authenticator> CreateAuthenticator() override;
  bool CheckPolicyForUser(const std::string& user_id) override;
  content::BrowserContext* GetSigninContext() override;
  net::URLRequestContextGetter* GetSigninRequestContext() override;

 private:
  void OnlineWildcardLoginCheckCompleted(
      const base::Closure& success_callback,
      const base::Closure& failure_callback,
      policy::WildcardLoginChecker::Result result);

  // Used to verify logins that matched wildcard on the login whitelist.
  scoped_ptr<policy::WildcardLoginChecker> wildcard_login_checker_;
  base::WeakPtrFactory<ChromeLoginPerformer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_CHROME_LOGIN_PERFORMER_H_
