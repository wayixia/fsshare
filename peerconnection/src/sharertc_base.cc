/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sharertc/peerconnection/src/sharertc_base.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "sharertc/peerconnection/src/defaults.h"

ShareRTCBase::ShareRTCBase(const webrtc::Environment& env,
                           std::unique_ptr<ISignalSocket> signalsocket)
    : env_(env),
      socket_(std::move(signalsocket)),
      signaling_thread_(webrtc::Thread::CreateWithSocketServer()),
      signal_connection_(
          std::bind(&ShareRTCBase::SendMessage, this, std::placeholders::_1),
          this) {
  if (signaling_thread_) {
    signaling_thread_->Start();
  }
  if (socket_) {
    socket_->setObserver(this);
  }
  // Default to WEBRTC_CONNECT (or a public STUN server) until the control
  // side supplies ICE servers while parsing its connection URL.
  ConfigureIceServers({}, "", "");
}

ShareRTCBase::~ShareRTCBase() {
  if (!signaling_thread_) {
    return;
  }
  auto cleanup = [this] {
    DestroyAllPeerConnections();
    peer_connection_factory_ = nullptr;
    pending_remote_candidates_.clear();
  };
  if (signaling_thread_->IsCurrent()) {
    // The Thread object cannot be stopped from its own thread; it is expected
    // that the owner is destroyed from the thread that created it.
    cleanup();
  } else {
    signaling_thread_->BlockingCall(cleanup);
    signaling_thread_->Stop();
  }
}

void ShareRTCBase::Login(const char* signalserver, const char* token) {
  if (!socket_ || !signalserver || !token) {
    return;
  }
  socket_->connect(signalserver, token);
}

void ShareRTCBase::Logout() {
  if (socket_) {
    socket_->disconnect();
  }
}

const char* ShareRTCBase::SignalServerAddr() const {
  return signal_server_addr_.c_str();
}

bool ShareRTCBase::IsSignalServerOK() {
  return socket_ && socket_->isConnected();
}

// ---------------- ISignalSocketObserver ----------------

void ShareRTCBase::onConnected() {
  RunOnSignalingThread([this] { signal_connection_.IdentifySelf(); });
}

void ShareRTCBase::onDisconnected() {
  RunOnSignalingThread([this] {
    DestroyAllPeerConnections();
    pending_remote_candidates_.clear();
    OnSignalConnectionClosed();
  });
}

void ShareRTCBase::onError(int code, const char* msg) {
  RTC_LOG(LS_WARNING) << "Signal socket error " << code << ": "
                      << (msg ? msg : "");
}

void ShareRTCBase::onDataReceived(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return;
  }
  std::string message(reinterpret_cast<const char*>(data), len);
  RunOnSignalingThread([this, message = std::move(message)] {
    std::string err;
    if (!signal_connection_.HandleMessage(message, err)) {
      RTC_LOG(LS_WARNING) << "Failed to handle signal message: " << err;
    }
  });
}

// ---------------- ShareRTCSignalConnection::Delegate ----------------

void ShareRTCBase::OnIdentified(const std::string& id) {
  my_id_ = id;
  OnSignalIdentified(id);
}

void ShareRTCBase::OnOffer(const std::string& sender,
                           const std::string& sdp) {
  if (sender.empty()) {
    return;
  }
  ShareRTCPeerConnection* pc = GetOrCreatePeerConnection(sender);
  if (!pc) {
    RTC_LOG(LS_ERROR) << "Failed to create PeerConnection for " << sender;
    return;
  }
  pc->HandleRemoteOffer(sdp);
}

void ShareRTCBase::OnAnswer(const std::string& sender,
                            const std::string& sdp) {
  ShareRTCPeerConnection* pc = ResolvePeerConnection(sender);
  if (!pc) {
    RTC_LOG(LS_WARNING) << "Answer from unknown peer " << sender;
    return;
  }
  pc->HandleRemoteAnswer(sdp);
}

void ShareRTCBase::OnRemoteIceCandidate(
    const std::string& sender,
    const ICECandidateContent& candidate) {
  ShareRTCPeerConnection* pc = ResolvePeerConnection(sender);
  if (!pc) {
    // The offer may not have arrived yet; keep the candidate for later.
    pending_remote_candidates_[sender].push_back(candidate);
    return;
  }
  pc->AddRemoteIceCandidate(candidate);
}

void ShareRTCBase::OnDisconnect(const std::string& sender) {
  ShareRTCPeerConnection* pc = ResolvePeerConnection(sender);
  if (pc) {
    DestroyPeerConnection(pc->remote_id());
  } else {
    DestroyPeerConnection(sender);
  }
  OnPeerDisconnected(sender);
}

void ShareRTCBase::OnTextMessage(const std::string& sender,
                                 const std::string& text) {
  RTC_LOG(LS_INFO) << "Text message from " << sender << ": " << text;
}

void ShareRTCBase::OnDisconnectionNotification(const std::string& peer_id) {
  DestroyPeerConnection(peer_id);
  OnPeerDisconnected(peer_id);
}

// ---------------- Signaling ----------------

int ShareRTCBase::SendMessage(const std::string& json_object) {
  if (!socket_ || !socket_->isConnected()) {
    return -1;
  }
  if (!socket_->send(reinterpret_cast<const uint8_t*>(json_object.data()),
                     json_object.size())) {
    return -2;
  }
  return 0;
}

// ---------------- PeerConnection management ----------------

bool ShareRTCBase::InitializePeerConnection() {
  if (peer_connection_factory_) {
    return true;
  }
  RTC_DCHECK(signaling_thread_);
  if (!signaling_thread_) {
    return false;
  }

  webrtc::PeerConnectionFactoryDependencies deps;
  deps.signaling_thread = signaling_thread_.get();
  deps.env = env_;
  deps.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  deps.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  deps.video_encoder_factory =
      std::make_unique<webrtc::VideoEncoderFactoryTemplate<
          webrtc::LibvpxVp8EncoderTemplateAdapter,
          webrtc::LibvpxVp9EncoderTemplateAdapter,
          webrtc::OpenH264EncoderTemplateAdapter,
          webrtc::LibaomAv1EncoderTemplateAdapter>>();
  deps.video_decoder_factory =
      std::make_unique<webrtc::VideoDecoderFactoryTemplate<
          webrtc::LibvpxVp8DecoderTemplateAdapter,
          webrtc::LibvpxVp9DecoderTemplateAdapter,
          webrtc::OpenH264DecoderTemplateAdapter,
          webrtc::Dav1dDecoderTemplateAdapter>>();
  webrtc::EnableMedia(deps);

  peer_connection_factory_ =
      webrtc::CreateModularPeerConnectionFactory(std::move(deps));
  if (!peer_connection_factory_) {
    RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnectionFactory";
    return false;
  }
  return true;
}

ShareRTCPeerConnection* ShareRTCBase::GetOrCreatePeerConnection(
    const std::string& remote_id) {
  RTC_DCHECK(signaling_thread_ && signaling_thread_->IsCurrent());

  auto it = peer_connections_.find(remote_id);
  if (it != peer_connections_.end()) {
    return it->second.get();
  }
  if (!InitializePeerConnection()) {
    return nullptr;
  }

  ShareRTCPeerConnection::Callbacks callbacks;
  callbacks.on_local_offer = [this, remote_id](const std::string& sdp) {
    signal_connection_.SendOffer(remote_id, sdp);
  };
  callbacks.on_local_answer = [this, remote_id](const std::string& sdp) {
    signal_connection_.SendAnswer(remote_id, sdp);
  };
  callbacks.on_local_ice_candidate =
      [this, remote_id](const ICECandidateContent& candidate) {
        signal_connection_.SendIceCandidate(remote_id, candidate);
      };
  callbacks.on_connection_state_change =
      [this, remote_id](
          webrtc::PeerConnectionInterface::PeerConnectionState state) {
        OnPeerConnectionStateChange(remote_id, state);
      };
  callbacks.on_signaling_state_change =
      [this, remote_id](
          webrtc::PeerConnectionInterface::SignalingState state) {
        OnPeerSignalingStateChange(remote_id, state);
      };
  callbacks.on_data_channel = [this, remote_id](const std::string& label) {
    OnRemoteDataChannel(remote_id, label);
  };
  callbacks.on_negotiation_needed = [this, remote_id] {
    OnPeerNegotiationNeeded(remote_id);
  };

  auto pc = std::make_unique<ShareRTCPeerConnection>(
      peer_connection_factory_, my_id_, remote_id,
      CreatePeerConnectionConfig(), std::move(callbacks));
  if (!pc->Initialize()) {
    return nullptr;
  }

  ShareRTCPeerConnection* raw_pc = pc.get();
  peer_connections_[remote_id] = std::move(pc);
  RTC_LOG(LS_INFO) << "PeerConnection to " << remote_id << " created";

  auto pending = pending_remote_candidates_.find(remote_id);
  if (pending != pending_remote_candidates_.end()) {
    for (const auto& candidate : pending->second) {
      raw_pc->AddRemoteIceCandidate(candidate);
    }
    pending_remote_candidates_.erase(pending);
  }
  return raw_pc;
}

ShareRTCPeerConnection* ShareRTCBase::FindPeerConnection(
    const std::string& remote_id) {
  auto it = peer_connections_.find(remote_id);
  return it == peer_connections_.end() ? nullptr : it->second.get();
}

ShareRTCPeerConnection* ShareRTCBase::ResolvePeerConnection(
    const std::string& sender) {
  ShareRTCPeerConnection* pc = FindPeerConnection(sender);
  if (pc) {
    return pc;
  }
  if (peer_connections_.size() == 1) {
    pc = peer_connections_.begin()->second.get();
    RTC_LOG(LS_WARNING) << "Routing message from " << sender << " to "
                        << pc->remote_id();
    return pc;
  }
  return nullptr;
}

void ShareRTCBase::DestroyPeerConnection(const std::string& remote_id) {
  auto it = peer_connections_.find(remote_id);
  if (it != peer_connections_.end()) {
    it->second->Close();
    peer_connections_.erase(it);
  }
  pending_remote_candidates_.erase(remote_id);
}

void ShareRTCBase::DestroyAllPeerConnections() {
  for (auto& entry : peer_connections_) {
    entry.second->Close();
  }
  peer_connections_.clear();
}

void ShareRTCBase::ConfigureIceServers(const std::vector<std::string>& urls,
                                       const std::string& username,
                                       const std::string& credential) {
  pc_config_.servers.clear();
  webrtc::PeerConnectionInterface::IceServer server;
  if (urls.empty()) {
    server.urls.push_back(GetPeerConnectionString());
  } else {
    server.urls = urls;
  }
  if (!username.empty()) {
    server.username = username;
  }
  if (!credential.empty()) {
    server.password = credential;
  }
  pc_config_.servers.push_back(server);
}

webrtc::PeerConnectionInterface::RTCConfiguration
ShareRTCBase::CreatePeerConnectionConfig() const {
  webrtc::PeerConnectionInterface::RTCConfiguration config = pc_config_;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  if (config.servers.empty()) {
    webrtc::PeerConnectionInterface::IceServer server;
    server.urls.push_back(GetPeerConnectionString());
    config.servers.push_back(server);
  }
  return config;
}
