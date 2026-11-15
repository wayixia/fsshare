/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sharertc/peerconnection/src/sharertc_control.h"

#include <memory>
#include <sstream>
#include <utility>

#include "rtc_base/logging.h"
#include "sharertc/peerconnection/src/sharertc_peer_connection.h"
#include "sharertc/peerconnection/src/sharertc_url_parser.h"

namespace {

constexpr char kDefaultChannelLabel[] = "data";

std::vector<std::string> SplitIceServers(const std::string& value) {
  std::vector<std::string> urls;
  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(',', start);
    std::string url =
        end == std::string::npos ? value.substr(start)
                                 : value.substr(start, end - start);
    const size_t first = url.find_first_not_of(" \t");
    const size_t last = url.find_last_not_of(" \t");
    if (first != std::string::npos) {
      urls.push_back(url.substr(first, last - first + 1));
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return urls;
}

std::string QueryValue(const std::map<std::string, std::string>& query,
                       const char* key) {
  auto it = query.find(key);
  return it == query.end() ? std::string() : it->second;
}

}  // namespace

ShareRTCControl::ShareRTCControl(const webrtc::Environment& env,
                                 std::unique_ptr<ISignalSocket> signalsocket)
    : ShareRTCBase(env, std::move(signalsocket)) {}

ShareRTCControl::~ShareRTCControl() = default;

void ShareRTCControl::SetObserver(IShareRTCChannelObserver* observer) {
  channel_observer_.store(observer);
}

int ShareRTCControl::Connect(const char* peerurl, const char* token) {
  if (!peerurl || !token) {
    return -1;
  }

  const ShareRTCUrlParseResult parsed = ShareRTCUrl::parse(peerurl);
  if (!parsed.ok) {
    RTC_LOG(LS_ERROR) << "Invalid peer url '" << peerurl
                      << "': " << parsed.error;
    return -2;
  }
  if (inited_.exchange(true)) {
    return -3;
  }

  const ShareRTCUrl url = parsed.value;
  const std::string usertoken = token;
  RunOnSignalingThread([this, url, usertoken] {
    peer_id_ = url.peerid();
    signal_server_addr_ = url.host();

    // ICE servers can be supplied by the API through the URL query, e.g.
    //   ?ice_servers=stun:stun.example.com:3478,turn:turn.example.com:3478
    //   &ice_username=user&ice_credential=pass
    // Without them the default STUN server is used.
    ConfigureIceServers(SplitIceServers(QueryValue(url.query(), "ice_servers")),
                        QueryValue(url.query(), "ice_username"),
                        QueryValue(url.query(), "ice_credential"));

    if (!IsSignalServerOK()) {
      std::ostringstream signalserver_url;
      signalserver_url << "ws://" << url.host() << ":" << url.port()
                       << "/signalingserver";
      RTC_LOG(LS_INFO) << "Logging into signaling server "
                       << signalserver_url.str() << " as peer " << peer_id_;
      Login(signalserver_url.str().c_str(), usertoken.c_str());
    }
  });

  return 0;
}

void ShareRTCControl::Disconnect() {
  RunOnSignalingThread([this] {
    if (!peer_id_.empty()) {
      signal_connection_.SendDisconnect(peer_id_);
    }
    DestroyAllPeerConnections();
    pending_channels_.clear();
    offer_sent_ = false;
    offer_in_flight_ = false;
    renegotiate_pending_ = false;
    Logout();
    inited_.store(false);
  });
}

int ShareRTCControl::CreateChannel(const char* label,
                                   IShareRTCChannelObserver* observer) {
  if (!label || label[0] == '\0') {
    return -1;
  }
  const int channel_id = next_channel_id_.fetch_add(1);
  std::string channel_label(label);
  RunOnSignalingThread([this, channel_id, channel_label,
                        observer]() mutable {
    AddChannelOnSignalingThread(channel_id, std::move(channel_label),
                                observer);
  });
  return channel_id;
}

void ShareRTCControl::AddChannelOnSignalingThread(
    int channel_id,
    std::string label,
    IShareRTCChannelObserver* observer) {
  ShareRTCPeerConnection* pc =
      peer_id_.empty() ? nullptr : FindPeerConnection(peer_id_);
  if (!pc) {
    // The connection is created when the signaling server identifies us; all
    // channels requested before that are created right before the offer.
    pending_channels_.push_back({channel_id, std::move(label), observer});
    return;
  }
  if (!pc->CreateDataChannel(channel_id, label, observer)) {
    return;
  }
  if (!offer_in_flight_ &&
      pc->signaling_state() == webrtc::PeerConnectionInterface::kStable) {
    offer_sent_ = SendOfferOnSignalingThread();
  } else {
    renegotiate_pending_ = true;
  }
}

bool ShareRTCControl::SendOfferOnSignalingThread() {
  ShareRTCPeerConnection* pc = FindPeerConnection(peer_id_);
  if (!pc || !pc->CreateOffer()) {
    return false;
  }
  offer_in_flight_ = true;
  return true;
}

int ShareRTCControl::Send(int channelid, const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return -1;
  }
  auto buffer = std::make_shared<std::vector<uint8_t>>(data, data + len);
  RunOnSignalingThread([this, channelid, buffer] {
    ShareRTCPeerConnection* pc =
        peer_id_.empty() ? nullptr : FindPeerConnection(peer_id_);
    if (pc) {
      pc->Send(channelid, buffer->data(), buffer->size());
    }
  });
  return 0;
}

// ---------------- Signaling / PeerConnection callbacks ----------------

void ShareRTCControl::OnSignalIdentified(const std::string& id) {
  RTC_LOG(LS_INFO) << "Identified as " << id << ", connecting to peer "
                   << peer_id_;
  if (peer_id_.empty() || offer_sent_) {
    return;
  }
  if (!InitializePeerConnection()) {
    return;
  }
  ShareRTCPeerConnection* pc = GetOrCreatePeerConnection(peer_id_);
  if (!pc) {
    RTC_LOG(LS_ERROR) << "Failed to create PeerConnection to " << peer_id_;
    return;
  }

  for (const PendingChannel& channel : pending_channels_) {
    pc->CreateDataChannel(channel.id, channel.label, channel.observer);
  }
  if (pending_channels_.empty()) {
    // An offer without an m= section has nothing to negotiate, so make sure
    // there is at least one data channel.
    pc->CreateDataChannel(next_channel_id_.fetch_add(1), kDefaultChannelLabel,
                          channel_observer_.load());
  }
  pending_channels_.clear();

  offer_sent_ = SendOfferOnSignalingThread();
}

void ShareRTCControl::OnPeerNegotiationNeeded(const std::string& peer_id) {
  if (!offer_sent_ || offer_in_flight_ || peer_id != peer_id_) {
    // The initial offer is created explicitly in OnSignalIdentified(); while
    // an offer is in flight WebRTC always reports negotiation-needed.
    return;
  }
  ShareRTCPeerConnection* pc = FindPeerConnection(peer_id);
  if (!pc) {
    return;
  }
  if (pc->signaling_state() == webrtc::PeerConnectionInterface::kStable) {
    offer_sent_ = SendOfferOnSignalingThread();
  } else {
    renegotiate_pending_ = true;
  }
}

void ShareRTCControl::OnPeerSignalingStateChange(
    const std::string& peer_id,
    webrtc::PeerConnectionInterface::SignalingState state) {
  if (peer_id != peer_id_) {
    return;
  }
  if (state != webrtc::PeerConnectionInterface::kStable) {
    return;
  }
  offer_in_flight_ = false;
  if (renegotiate_pending_) {
    renegotiate_pending_ = false;
    offer_sent_ = SendOfferOnSignalingThread();
  }
}

void ShareRTCControl::OnPeerConnectionStateChange(
    const std::string& peer_id,
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  RTC_LOG(LS_INFO) << "Peer " << peer_id << " connection state "
                   << static_cast<int>(state);
}

void ShareRTCControl::OnSignalConnectionClosed() {
  offer_sent_ = false;
  offer_in_flight_ = false;
  renegotiate_pending_ = false;
  pending_channels_.clear();
}
