// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_

#include <string>
#include "chrome/browser/extensions/chrome_extension_function.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace extension_sidebar_constants {
extern const char kActiveState[];
extern const char kHiddenState[];
extern const char kShownState[];
}  // namespace extension_sidebar_constants

// Event router class for events related to the sidebar API.
class ExtensionSidebarEventRouter {
 public:
  // Sidebar state changed.
  static void OnStateChanged(
      Profile* profile, content::WebContents* tab,
      const std::string& content_id, const std::string& state);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionSidebarEventRouter);
};

// Base class for sidebar function APIs.
class SidebarFunction : public ChromeSyncExtensionFunction {
 public:
  bool RunSync() override;
 protected:
  ~SidebarFunction() override {};

 private:
  virtual bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) = 0;
};

class CollapseSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
 protected:
  ~CollapseSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.collapse", SIDEBAR_COLLAPSE);
};

class ExpandSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
 protected:
  ~ExpandSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.expand", SIDEBAR_EXPAND);
};

class GetStateSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~GetStateSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.getState", SIDEBAR_GETSTATE);
};

class HideSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~HideSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.hide", SIDEBAR_HIDE);
};

class NavigateSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~NavigateSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.navigate", SIDEBAR_NAVIGATE);
};

class SetBadgeTextSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~SetBadgeTextSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.setBadgeText", SIDEBAR_BADGETEXT);
};

class SetIconSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~SetIconSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.setIcon", SIDEBAR_SETICON);
};

class SetTitleSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~SetTitleSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.setTitle", SIDEBAR_SETTITLE);
};

class ShowSidebarFunction : public SidebarFunction {
 private:
  bool RunImpl(content::WebContents* tab,
                       const std::string& content_id,
                       const base::DictionaryValue& details) override;
protected:
  ~ShowSidebarFunction() override {} ;

  DECLARE_EXTENSION_FUNCTION("experimental.sidebar.show", SIDEBAR_SHOW);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SIDEBAR_API_H_
