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
  const base::string16& default_scope() const { return default_scope_; }

 private:
  
  base::string16 default_scope_;
};

#endif  // EXTENSIONS_COMMON_EXTENSION_SIDEBAR_DEFAULTS_H_
