// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_STATS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_STATS_H_

#include "base/callback.h"
#include "base/prefs/pref_member.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

namespace net {
class HttpResponseHeaders;
class ProxyConfig;
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;

class DataReductionProxyBypassStats
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  typedef base::Callback<void(bool /* unreachable */)> UnreachableCallback;

  // Records a data reduction proxy bypass event as a "BlockType" if
  // |bypass_all| is true and as a "BypassType" otherwise. Records the event as
  // "Primary" if |is_primary| is true and "Fallback" otherwise.
  static void RecordDataReductionProxyBypassInfo(
      bool is_primary,
      bool bypass_all,
      const net::ProxyServer& proxy_server,
      DataReductionProxyBypassType bypass_type);

  // For the given response |headers| that are expected to include the data
  // reduction proxy via header, records response code UMA if the data reduction
  // proxy via header is not present.
  static void DetectAndRecordMissingViaHeaderResponseCode(
      bool is_primary,
      const net::HttpResponseHeaders* headers);

  // |config| outlives this class instance. |unreachable_callback| provides a
  // hook to inform the user that the Data Reduction Proxy is unreachable.
  // |config| must not be null.
  DataReductionProxyBypassStats(
      DataReductionProxyConfig* config,
      UnreachableCallback unreachable_callback);

  ~DataReductionProxyBypassStats() override;

  // Callback intended to be called from |DataReductionProxyNetworkDelegate|
  // when a request completes. This method is used to gather bypass stats.
  void OnUrlRequestCompleted(const net::URLRequest* request, bool started);

  // Records the last bypass reason to |bypass_type_| and sets
  // |triggering_request_| to true. A triggering request is the first request to
  // cause the current bypass.
  void SetBypassType(DataReductionProxyBypassType type);

  // Visible for testing.
  DataReductionProxyBypassType GetBypassType() const;

  // Records all the data reduction proxy bytes-related histograms for the
  // completed URLRequest |request|.
  void RecordBytesHistograms(
      const net::URLRequest& request,
      const BooleanPrefMember& data_reduction_proxy_enabled,
      const net::ProxyConfig& data_reduction_proxy_config);

  // Called by |ChromeNetworkDelegate| when a proxy is put into the bad proxy
  // list. Used to track when the data reduction proxy falls back.
  void OnProxyFallback(const net::ProxyServer& bypassed_proxy,
                       int net_error);

  // Called by |ChromeNetworkDelegate| when an HTTP connect has been called.
  // Used to track proxy connection failures.
  void OnConnectComplete(const net::HostPortPair& proxy_server,
                         int net_error);

 private:
  friend class DataReductionProxyBypassStatsTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyBypassStatsTest,
                           RecordMissingViaHeaderBytes);

  enum BypassedBytesType {
    NOT_BYPASSED = 0,         /* Not bypassed. */
    SSL,                      /* Bypass due to SSL. */
    LOCAL_BYPASS_RULES,       /* Bypass due to client-side bypass rules. */
    PROXY_OVERRIDDEN,         /* Bypass due to a proxy taking precedence. */
    AUDIO_VIDEO,              /* Audio/Video bypass. */
    APPLICATION_OCTET_STREAM, /* "application/octet-stream" content bypass. */
    TRIGGERING_REQUEST,       /* Triggering request bypass. */
    NETWORK_ERROR,            /* Network error. */
    BYPASSED_BYTES_TYPE_MAX   /* This must always be last.*/
  };

  // Given |data_reduction_proxy_enabled|, a |request|, and the
  // |data_reduction_proxy_config| records the number of bypassed bytes for that
  // |request| into UMAs based on bypass type. |data_reduction_proxy_enabled|
  // tells us the state of the kDataReductionProxyEnabled preference.
  void RecordBypassedBytesHistograms(
      const net::URLRequest& request,
      const BooleanPrefMember& data_reduction_proxy_enabled,
      const net::ProxyConfig& data_reduction_proxy_config);

  // Records UMA of the number of response bytes of responses that are expected
  // to have the data reduction proxy via header, but where the data reduction
  // proxy via header is not present.
  void RecordMissingViaHeaderBytes(const net::URLRequest& request);

  // NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Clears request counts unconditionally.
  void ClearRequestCounts();

  // Checks if the availability status of the data reduction proxy has changed,
  // and notifies the UIThread via NotifyUnavailabilityOnUIThread if so. The
  // data reduction proxy is considered unavailable if and only if no requests
  // went through the proxy but some eligible requests were service by other
  // routes.
  void NotifyUnavailabilityIfChanged();
  void NotifyUnavailabilityOnUIThread(bool unavailable);

  void RecordBypassedBytes(
      DataReductionProxyBypassType bypass_type,
      BypassedBytesType bypassed_bytes_type,
      int64 content_length);

  DataReductionProxyConfig* data_reduction_proxy_config_;

  UnreachableCallback unreachable_callback_;

  // The last reason for bypass as determined by
  // MaybeBypassProxyAndPrepareToRetry
  DataReductionProxyBypassType last_bypass_type_;
  // True if the last request triggered the current bypass.
  bool triggering_request_;

  // The following 2 fields are used to determine if data reduction proxy is
  // unreachable. We keep a count of requests which should go through
  // data request proxy, as well as those which actually do. The proxy is
  // unreachable if no successful requests are made through it despite a
  // non-zero number of requests being eligible.

  // Count of successful requests through the data reduction proxy.
  unsigned long successful_requests_through_proxy_count_;

  // Count of network errors encountered when connecting to a data reduction
  // proxy.
  unsigned long proxy_net_errors_count_;

  // Whether or not the data reduction proxy is unavailable.
  bool unavailable_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyBypassStats);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_STATS_H_
