// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sidebar/sidebar_defaults.h"

#include <string>
#include "url/gurl.h"

ExtensionSidebarDefaults::ExtensionSidebarDefaults() {
}
ExtensionSidebarDefaults::~ExtensionSidebarDefaults() {
}

// Default title, stores manifest default_title key value.
void ExtensionSidebarDefaults::set_default_scope(const base::string16& scope) {
    default_scope_ = scope;
}
