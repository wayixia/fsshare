/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARERTC_PEER_CONNECTION_H_
#define SHARERTC_PEER_CONNECTION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_signal_message.h"

// One WebRTC PeerConnection to a single remote peer.
//
// Threading: like webrtc::PeerConnection, every method of this class must be
// called on the signaling thread that owns the PeerConnectionFactory.
//
// The ICE flow implemented here is trickle ICE:
//   * CreateOffer()/HandleRemoteOffer() create the local description and
//     invoke the corresponding callback (on_local_offer/on_local_answer) so
//     the owner can send it over the signaling channel.
//   * OnIceCandidate() reports every gathered local candidate through
//     on_local_ice_candidate.
//   * AddRemoteIceCandidate() applies candidates coming from the signaling
//     channel. Candidates received before the remote description is set are
//     buffered and applied once it is.
class ShareRTCPeerConnection : public webrtc::PeerConnectionObserver {
 public:
  struct Callbacks {
    std::function<void(const std::string& sdp)> on_local_offer;
    std::function<void(const std::string& sdp)> on_local_answer;
    std::function<void(const ICECandidateContent& candidate)>
        on_local_ice_candidate;
    std::function<void(webrtc::PeerConnectionInterface::PeerConnectionState)>
        on_connection_state_change;
    std::function<void(webrtc::PeerConnectionInterface::SignalingState)>
        on_signaling_state_change;
    std::function<void(const std::string& label)> on_data_channel;
    std::function<void()> on_negotiation_needed;
  };

  ShareRTCPeerConnection(
      webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
      std::string local_id,
      std::string remote_id,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      Callbacks callbacks);
  ~ShareRTCPeerConnection() override;

  ShareRTCPeerConnection(const ShareRTCPeerConnection&) = delete;
  ShareRTCPeerConnection& operator=(const ShareRTCPeerConnection&) = delete;

  // Creates the underlying webrtc::PeerConnection. Must be called before any
  // other method.
  bool Initialize();

  // Creates and sends a local offer.
  bool CreateOffer();

  // Applies a remote offer and answers it through on_local_answer.
  bool HandleRemoteOffer(const std::string& sdp);
  // Applies a remote answer to a previously sent offer.
  bool HandleRemoteAnswer(const std::string& sdp);
  // Applies a trickled remote candidate, buffering it if the remote
  // description has not been set yet.
  bool AddRemoteIceCandidate(const ICECandidateContent& candidate);

  // Creates a data channel with a caller supplied channel id and returns true
  // on success. `observer` may be null.
  bool CreateDataChannel(int channel_id,
                         const std::string& label,
                         IShareRTCChannelObserver* observer);
  bool Send(int channel_id, const uint8_t* data, size_t len);

  void Close();

  const std::string& remote_id() const { return remote_id_; }
  webrtc::PeerConnectionInterface::SignalingState signaling_state() const {
    return signaling_state_;
  }

  // webrtc::PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidate* candidate) override;
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
  void OnRenegotiationNeeded() override;
  void OnNegotiationNeededEvent(uint32_t event_id) override;

 private:
  class ChannelObserver;
  struct Channel {
    int id = 0;
    std::string label;
    webrtc::scoped_refptr<webrtc::DataChannelInterface> channel;
    std::unique_ptr<ChannelObserver> observer;
    IShareRTCChannelObserver* external_observer = nullptr;
  };

  void OnCreatedSessionDescription(webrtc::SessionDescriptionInterface* desc);
  void CreateAnswer();
  void SetRemoteDescription(webrtc::SdpType type, const std::string& sdp);
  void FlushPendingRemoteCandidates();
  void AttachChannel(int channel_id,
                     const std::string& label,
                     webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
                     IShareRTCChannelObserver* external_observer);
  void OnChannelStateChange(int channel_id);
  void OnChannelMessage(int channel_id,
                        const webrtc::DataBuffer& buffer);

  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
  std::string local_id_;
  std::string remote_id_;
  webrtc::PeerConnectionInterface::RTCConfiguration config_;
  Callbacks callbacks_;

  webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  webrtc::PeerConnectionInterface::SignalingState signaling_state_ =
      webrtc::PeerConnectionInterface::kStable;
  bool remote_description_set_ = false;

  std::map<int, std::unique_ptr<Channel>> channels_;
  int next_remote_channel_id_ = -1;
  std::vector<ICECandidateContent> pending_remote_candidates_;
};

#endif  // SHARERTC_PEER_CONNECTION_H_
