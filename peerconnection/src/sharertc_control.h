/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARERTC_CONTROL_H_
#define SHARERTC_CONTROL_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "api/environment/environment.h"
#include "api/peer_connection_interface.h"
#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_base.h"

// Connection initiator: logs into the signaling server, creates the offer and
// drives the ICE exchange with the peer named in the Connect() URL.
class ShareRTCControl : public ShareRTCBase, public IShareRTCControl {
 public:
  ShareRTCControl(const webrtc::Environment& env,
                  std::unique_ptr<ISignalSocket> signalsocket);
  ~ShareRTCControl() override;

  // IShareRTCControl implementation.
  void SetObserver(IShareRTCChannelObserver* observer) override;

  // Parses `url` (https://signalserver:port/peerid[?ice_servers=...]) and
  // logs into the signaling server. The offer is sent once the server has
  // identified us. Returns 0 on success, negative on invalid arguments.
  int Connect(const char* url, const char* usertoken) override;
  void Disconnect() override;

  // Returns a positive channel id, or -1 on invalid arguments.
  int CreateChannel(const char* label,
                    IShareRTCChannelObserver* observer) override;
  int Send(int channelid, const uint8_t* data, size_t len) override;

 protected:
  void OnSignalIdentified(const std::string& id) override;
  void OnPeerNegotiationNeeded(const std::string& peer_id) override;
  void OnPeerSignalingStateChange(
      const std::string& peer_id,
      webrtc::PeerConnectionInterface::SignalingState state) override;
  void OnPeerConnectionStateChange(
      const std::string& peer_id,
      webrtc::PeerConnectionInterface::PeerConnectionState state) override;
  void OnSignalConnectionClosed() override;

 private:
  struct PendingChannel {
    int id;
    std::string label;
    IShareRTCChannelObserver* observer;
  };

  // Signaling thread only.
  void AddChannelOnSignalingThread(int channel_id,
                                   std::string label,
                                   IShareRTCChannelObserver* observer);
  bool SendOfferOnSignalingThread();

  std::atomic<bool> inited_{false};
  std::atomic<IShareRTCChannelObserver*> channel_observer_{nullptr};
  std::atomic<int> next_channel_id_{1};

  // Signaling thread only.
  std::vector<PendingChannel> pending_channels_;
  bool offer_sent_ = false;
  // True between CreateOffer() and the signaling state returning to stable.
  bool offer_in_flight_ = false;
  bool renegotiate_pending_ = false;
};

#endif  // SHARERTC_CONTROL_H_
