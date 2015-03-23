// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_SIDEBAR_DEFAULTS_H_
#define EXTENSIONS_COMMON_EXTENSION_SIDEBAR_DEFAULTS_H_
#pragma once

#include <string>

#include "base/strings/string16.h"
#include "url/gurl.h"

// ExtensionSidebarDefaults encapsulates the default parameters of a sidebar,
// as defined in the extension manifest.
class ExtensionSidebarDefaults {
 public:
  ExtensionSidebarDefaults();
  ~ExtensionSidebarDefaults();
  // Default title, stores manifest default_title key value.
  void set_default_scope(const base::string16& scope);
  // Default title, stores manifest default_title key value.
  void set_default_title(const base::string16& title);
  const base::string16& default_title() const { return default_title_; }
  const base::string16& default_scope() const { return default_scope_; }

  // Default icon path, stores manifest default_icon key value.
  void set_default_icon_path(const std::string& path);
  const std::string& default_icon_path() const {
    return default_icon_path_;
  }

  // A resolved |url| to extension resource (manifest default_page key value)
  // to navigate sidebar to by default.
  void set_default_page(const GURL& url);
  const GURL& default_page() const {
    return default_page_;
  }

 private:
  
  base::string16 default_scope_;
  base::string16 default_title_;
  std::string default_icon_path_;
  GURL default_page_;
};

#endif  // EXTENSIONS_COMMON_EXTENSION_SIDEBAR_DEFAULTS_H_
