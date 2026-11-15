/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARERTC_CLIENT_H_
#define SHARERTC_CLIENT_H_

#include <string>

#include "api/environment/environment.h"
#include "api/peer_connection_interface.h"
#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_base.h"

// Connection receiver. After Login() the ShareRTCBase implementation answers
// incoming offers and completes the ICE exchange; no offer is initiated here.
class ShareRTCClient : public ShareRTCBase, public IShareRTCClient {
 public:
  ShareRTCClient(const webrtc::Environment& env,
                 std::unique_ptr<ISignalSocket> signalsocket);
  ~ShareRTCClient() override;

  // IShareRTCClient implementation.
  void Login(const char* signalserver, const char* token) override;
  void DisconnectFromServer() override;
  void DisconnectPeer(int peer_id) override;
  void Logout() override;

 protected:
  void OnRemoteDataChannel(const std::string& peer_id,
                           const std::string& label) override;
  void OnPeerConnectionStateChange(
      const std::string& peer_id,
      webrtc::PeerConnectionInterface::PeerConnectionState state) override;
};

#endif  // SHARERTC_CLIENT_H_
