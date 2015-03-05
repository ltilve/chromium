// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WEBUI_INJECTION_HOST_H_
#define EXTENSIONS_RENDERER_WEBUI_INJECTION_HOST_H_

#include "extensions/renderer/injection_host.h"

class WebUIInjectionHost : public InjectionHost {
 public:
  WebUIInjectionHost(const HostID& host_id);
  ~WebUIInjectionHost() override;

 private:
  // InjectionHost:
  std::string GetContentSecurityPolicy() const override;
  const GURL& url() const override;
  const std::string& name() const override;
  extensions::PermissionsData::AccessType CanExecuteOnFrame(
      const GURL& document_url,
      const GURL& top_frame_url,
      int tab_id,
      bool is_declarative) const override;
  bool ShouldNotifyBrowserOfInjection() const override;

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WebUIInjectionHost);
};

#endif  // EXTENSIONS_RENDERER_WEBUI_INJECTION_HOST_H_
