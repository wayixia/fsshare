/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sharertc/peerconnection/src/sharertc_client.h"

#include "rtc_base/logging.h"

ShareRTCClient::ShareRTCClient(const webrtc::Environment& env,
                               std::unique_ptr<ISignalSocket> signalsocket)
    : ShareRTCBase(env, std::move(signalsocket)) {}

ShareRTCClient::~ShareRTCClient() = default;

void ShareRTCClient::Login(const char* signalserver, const char* token) {
  // The signaling server assigns this endpoint an id (IdentifySelf) and then
  // routes incoming offers to it.
  ShareRTCBase::Login(signalserver, token);
}

void ShareRTCClient::DisconnectFromServer() {
  ShareRTCBase::Logout();
}

void ShareRTCClient::DisconnectPeer(int peer_id) {
  // Peer ids are strings on the signaling channel; the int based overload of
  // the public API cannot be mapped reliably, so close every connection.
  RunOnSignalingThread([this] { DestroyAllPeerConnections(); });
}

void ShareRTCClient::Logout() {
  ShareRTCBase::Logout();
}

void ShareRTCClient::OnRemoteDataChannel(const std::string& peer_id,
                                         const std::string& label) {
  RTC_LOG(LS_INFO) << "Data channel '" << label << "' from peer " << peer_id;
}

void ShareRTCClient::OnPeerConnectionStateChange(
    const std::string& peer_id,
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  RTC_LOG(LS_INFO) << "Peer " << peer_id << " connection state "
                   << static_cast<int>(state);
}
