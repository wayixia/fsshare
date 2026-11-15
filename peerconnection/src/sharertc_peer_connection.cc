/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sharertc/peerconnection/src/sharertc_peer_connection.h"

#include <utility>

#include "api/candidate.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace {

using webrtc::RTCError;
using webrtc::SessionDescriptionInterface;

class CreateSdpObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSdpObserver(
      std::function<void(SessionDescriptionInterface*)> on_success,
      std::function<void(RTCError)> on_failure)
      : on_success_(std::move(on_success)),
        on_failure_(std::move(on_failure)) {}

  void OnSuccess(SessionDescriptionInterface* desc) override {
    std::move(on_success_)(desc);
  }
  void OnFailure(RTCError error) override {
    std::move(on_failure_)(std::move(error));
  }

 private:
  std::function<void(SessionDescriptionInterface*)> on_success_;
  std::function<void(RTCError)> on_failure_;
};

class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  explicit SetLocalDescriptionObserver(std::function<void(RTCError)> done)
      : done_(std::move(done)) {}

  void OnSetLocalDescriptionComplete(RTCError error) override {
    std::move(done_)(std::move(error));
  }

 private:
  std::function<void(RTCError)> done_;
};

class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  explicit SetRemoteDescriptionObserver(std::function<void(RTCError)> done)
      : done_(std::move(done)) {}

  void OnSetRemoteDescriptionComplete(RTCError error) override {
    std::move(done_)(std::move(error));
  }

 private:
  std::function<void(RTCError)> done_;
};

}  // namespace

class ShareRTCPeerConnection::ChannelObserver
    : public webrtc::DataChannelObserver {
 public:
  ChannelObserver(ShareRTCPeerConnection* parent, int channel_id)
      : parent_(parent), channel_id_(channel_id) {}

  void OnStateChange() override { parent_->OnChannelStateChange(channel_id_); }
  void OnMessage(const webrtc::DataBuffer& buffer)
      override {
    parent_->OnChannelMessage(channel_id_, buffer);
  }

 private:
  ShareRTCPeerConnection* parent_;
  int channel_id_;
};

ShareRTCPeerConnection::ShareRTCPeerConnection(
    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
    std::string local_id,
    std::string remote_id,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    Callbacks callbacks)
    : factory_(std::move(factory)),
      local_id_(std::move(local_id)),
      remote_id_(std::move(remote_id)),
      config_(config),
      callbacks_(std::move(callbacks)) {}

ShareRTCPeerConnection::~ShareRTCPeerConnection() {
  Close();
}

bool ShareRTCPeerConnection::Initialize() {
  if (peer_connection_) {
    return true;
  }
  if (!factory_) {
    RTC_LOG(LS_ERROR) << "No PeerConnectionFactory";
    return false;
  }

  webrtc::PeerConnectionDependencies dependencies(this);
  auto result = factory_->CreatePeerConnectionOrError(config_,
                                                      std::move(dependencies));
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "CreatePeerConnection failed: "
                      << result.error().message();
    return false;
  }
  peer_connection_ = result.MoveValue();
  RTC_LOG(LS_INFO) << "PeerConnection created for local peer " << local_id_
                   << " -> remote peer " << remote_id_;
  return true;
}

bool ShareRTCPeerConnection::CreateOffer() {
  if (!peer_connection_) {
    return false;
  }
  if (signaling_state_ != webrtc::PeerConnectionInterface::kStable) {
    RTC_LOG(LS_WARNING) << "CreateOffer called while signaling state is "
                        << signaling_state_;
    return false;
  }

  auto observer = webrtc::make_ref_counted<CreateSdpObserver>(
      [this](SessionDescriptionInterface* desc) {
        OnCreatedSessionDescription(desc);
      },
      [](RTCError error) {
        RTC_LOG(LS_ERROR) << "CreateOffer failed: " << error.message();
      });
  peer_connection_->CreateOffer(
      observer.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  return true;
}

void ShareRTCPeerConnection::CreateAnswer() {
  if (!peer_connection_) {
    return;
  }
  auto observer = webrtc::make_ref_counted<CreateSdpObserver>(
      [this](SessionDescriptionInterface* desc) {
        OnCreatedSessionDescription(desc);
      },
      [](RTCError error) {
        RTC_LOG(LS_ERROR) << "CreateAnswer failed: " << error.message();
      });
  peer_connection_->CreateAnswer(
      observer.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

void ShareRTCPeerConnection::OnCreatedSessionDescription(
    SessionDescriptionInterface* desc) {
  // The CreateSessionDescriptionObserver contract transfers ownership.
  std::unique_ptr<SessionDescriptionInterface> description(desc);
  if (!peer_connection_) {
    return;
  }
  const webrtc::SdpType type = description->GetType();

  auto observer = webrtc::make_ref_counted<SetLocalDescriptionObserver>(
      [this, type](RTCError error) {
        if (!error.ok()) {
          RTC_LOG(LS_ERROR) << "SetLocalDescription failed: "
                            << error.message();
          return;
        }
        // Read the description that was actually applied, not the one
        // returned by CreateOffer/CreateAnswer.
        std::string sdp;
        const SessionDescriptionInterface* local =
            peer_connection_ ? peer_connection_->local_description() : nullptr;
        if (!local || !local->ToString(&sdp)) {
          RTC_LOG(LS_ERROR) << "Failed to serialize local description";
          return;
        }
        if (type == webrtc::SdpType::kOffer) {
          if (callbacks_.on_local_offer) {
            callbacks_.on_local_offer(sdp);
          }
        } else if (type == webrtc::SdpType::kAnswer) {
          if (callbacks_.on_local_answer) {
            callbacks_.on_local_answer(sdp);
          }
        }
      });
  peer_connection_->SetLocalDescription(std::move(description), observer);
}

bool ShareRTCPeerConnection::HandleRemoteOffer(const std::string& sdp) {
  SetRemoteDescription(webrtc::SdpType::kOffer, sdp);
  return true;
}

bool ShareRTCPeerConnection::HandleRemoteAnswer(const std::string& sdp) {
  SetRemoteDescription(webrtc::SdpType::kAnswer, sdp);
  return true;
}

void ShareRTCPeerConnection::SetRemoteDescription(webrtc::SdpType type,
                                                  const std::string& sdp) {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "SetRemoteDescription without a PeerConnection";
    return;
  }

  webrtc::SdpParseError parse_error;
  std::unique_ptr<SessionDescriptionInterface> description =
      webrtc::CreateSessionDescription(type, sdp, &parse_error);
  if (!description) {
    RTC_LOG(LS_ERROR) << "Failed to parse remote "
                      << webrtc::SdpTypeToString(type)
                      << ": " << parse_error.description;
    return;
  }

  auto observer = webrtc::make_ref_counted<SetRemoteDescriptionObserver>(
      [this, type](RTCError error) {
        if (!error.ok()) {
          RTC_LOG(LS_ERROR) << "SetRemoteDescription failed: "
                            << error.message();
          return;
        }
        remote_description_set_ = true;
        FlushPendingRemoteCandidates();
        if (type == webrtc::SdpType::kOffer) {
          CreateAnswer();
        }
      });
  peer_connection_->SetRemoteDescription(std::move(description), observer);
}

bool ShareRTCPeerConnection::AddRemoteIceCandidate(
    const ICECandidateContent& candidate) {
  if (!peer_connection_) {
    return false;
  }
  if (!remote_description_set_) {
    // Trickle ICE: candidates may arrive before the offer/answer. Buffer them
    // and apply once the remote description is in place.
    pending_remote_candidates_.push_back(candidate);
    return true;
  }

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidate> ice_candidate(
      webrtc::CreateIceCandidate(candidate.sdpMid, candidate.sdpMLineIndex,
                                 candidate.candidate, &error));
  if (!ice_candidate) {
    RTC_LOG(LS_WARNING) << "Failed to parse remote ICE candidate: "
                        << error.description;
    return false;
  }
  if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
    RTC_LOG(LS_WARNING) << "Failed to apply remote ICE candidate";
    return false;
  }
  RTC_LOG(LS_INFO) << "Remote ICE candidate applied: " << candidate.candidate;
  return true;
}

void ShareRTCPeerConnection::FlushPendingRemoteCandidates() {
  std::vector<ICECandidateContent> pending;
  pending.swap(pending_remote_candidates_);
  for (const auto& candidate : pending) {
    AddRemoteIceCandidate(candidate);
  }
}

bool ShareRTCPeerConnection::CreateDataChannel(
    int channel_id,
    const std::string& label,
    IShareRTCChannelObserver* observer) {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "CreateDataChannel without a PeerConnection";
    return false;
  }
  if (channels_.find(channel_id) != channels_.end()) {
    RTC_LOG(LS_WARNING) << "Duplicate data channel id " << channel_id;
    return false;
  }

  auto result = peer_connection_->CreateDataChannelOrError(label, nullptr);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "CreateDataChannel failed: "
                      << result.error().message();
    return false;
  }
  AttachChannel(channel_id, label, result.MoveValue(), observer);
  return true;
}

bool ShareRTCPeerConnection::Send(int channel_id,
                                  const uint8_t* data,
                                  size_t len) {
  if (!data || len == 0) {
    return false;
  }
  auto it = channels_.find(channel_id);
  if (it == channels_.end() || !it->second->channel) {
    RTC_LOG(LS_WARNING) << "Send on unknown channel " << channel_id;
    return false;
  }
  if (it->second->channel->state() != webrtc::DataChannelInterface::kOpen) {
    RTC_LOG(LS_WARNING) << "Data channel " << channel_id << " is not open";
    return false;
  }
  return it->second->channel->Send(webrtc::DataBuffer(
      webrtc::CopyOnWriteBuffer(data, len), /*binary=*/true));
}

void ShareRTCPeerConnection::Close() {
  for (auto& entry : channels_) {
    Channel* channel = entry.second.get();
    // Do not notify channels that already reported kClosed.
    if (channel->external_observer && channel->channel &&
        channel->channel->state() == webrtc::DataChannelInterface::kOpen) {
      channel->external_observer->OnClose();
    }
    if (channel->channel) {
      channel->channel->UnregisterObserver();
    }
    channel->channel = nullptr;
  }
  channels_.clear();
  pending_remote_candidates_.clear();
  remote_description_set_ = false;
  if (peer_connection_) {
    peer_connection_->Close();
    peer_connection_ = nullptr;
  }
}

void ShareRTCPeerConnection::AttachChannel(
    int channel_id,
    const std::string& label,
    webrtc::scoped_refptr<webrtc::DataChannelInterface> channel,
    IShareRTCChannelObserver* external_observer) {
  auto context = std::make_unique<Channel>();
  context->id = channel_id;
  context->label = label;
  context->channel = std::move(channel);
  context->observer = std::make_unique<ChannelObserver>(this, channel_id);
  context->external_observer = external_observer;
  context->channel->RegisterObserver(context->observer.get());
  channels_[channel_id] = std::move(context);
}

void ShareRTCPeerConnection::OnChannelStateChange(int channel_id) {
  auto it = channels_.find(channel_id);
  if (it == channels_.end()) {
    return;
  }
  IShareRTCChannelObserver* observer = it->second->external_observer;
  if (!observer) {
    return;
  }
  switch (it->second->channel->state()) {
    case webrtc::DataChannelInterface::kOpen:
      observer->OnOpen();
      break;
    case webrtc::DataChannelInterface::kClosed:
      observer->OnClose();
      break;
    default:
      break;
  }
}

void ShareRTCPeerConnection::OnChannelMessage(
    int channel_id,
    const webrtc::DataBuffer& buffer) {
  // The public channel interface currently has no OnMessage callback; log so
  // data is not silently lost while the API is being extended.
  RTC_LOG(LS_INFO) << "Data channel " << channel_id << " received "
                   << buffer.size() << " bytes";
}

// ---------------- webrtc::PeerConnectionObserver ----------------

void ShareRTCPeerConnection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  signaling_state_ = new_state;
  if (callbacks_.on_signaling_state_change) {
    callbacks_.on_signaling_state_change(new_state);
  }
}

void ShareRTCPeerConnection::OnDataChannel(
    webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (!data_channel) {
    return;
  }
  const std::string label = data_channel->label();
  const int channel_id = next_remote_channel_id_--;
  RTC_LOG(LS_INFO) << "Remote data channel '" << label << "' created (id "
                   << channel_id << ")";
  AttachChannel(channel_id, label, std::move(data_channel),
                /*external_observer=*/nullptr);
  if (callbacks_.on_data_channel) {
    callbacks_.on_data_channel(label);
  }
}

void ShareRTCPeerConnection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "ICE gathering state changed to " << new_state;
}

void ShareRTCPeerConnection::OnIceCandidate(
    const webrtc::IceCandidate* candidate) {
  if (!candidate || !callbacks_.on_local_ice_candidate) {
    return;
  }
  ICECandidateContent content;
  content.candidate = candidate->ToString();
  content.sdpMid = candidate->sdp_mid();
  content.sdpMLineIndex = candidate->sdp_mline_index();
  content.usernameFragment = candidate->candidate().username();
  RTC_LOG(LS_INFO) << "Local ICE candidate gathered: " << content.candidate;
  callbacks_.on_local_ice_candidate(content);
}

void ShareRTCPeerConnection::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  RTC_LOG(LS_INFO) << "Peer connection state changed to "
                   << static_cast<int>(new_state);
  if (callbacks_.on_connection_state_change) {
    callbacks_.on_connection_state_change(new_state);
  }
}

void ShareRTCPeerConnection::OnRenegotiationNeeded() {
  if (callbacks_.on_negotiation_needed) {
    callbacks_.on_negotiation_needed();
  }
}

void ShareRTCPeerConnection::OnNegotiationNeededEvent(uint32_t event_id) {
  if (!peer_connection_ ||
      !peer_connection_->ShouldFireNegotiationNeededEvent(event_id)) {
    return;
  }
  if (callbacks_.on_negotiation_needed) {
    callbacks_.on_negotiation_needed();
  }
}
