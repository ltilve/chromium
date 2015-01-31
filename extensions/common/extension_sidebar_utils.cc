// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_sidebar_utils.h"

#include "extensions/common/extension.h"
#include "extensions/common/error_utils.h"
#include "url/gurl.h"

namespace {

// Errors.
const char kInvalidPathError[] = "Invalid path: \"*\".";

}  // namespace

namespace extension_sidebar_utils {

std::string GetExtensionIdByContentId(const std::string& content_id) {
  // At the moment, content_id == extension_id.
  return content_id;
}

GURL ResolveRelativePath(const std::string& relative_path,
                         const extensions::Extension* extension,
                         std::string* error) {
  GURL url(extension->GetResourceURL(relative_path));
  if (!url.is_valid()) {
    *error = extensions::ErrorUtils::FormatErrorMessage(kInvalidPathError,
                                                     relative_path);
    return GURL();
  }
  return url;
}

}  // namespace extension_sidebar_utils
