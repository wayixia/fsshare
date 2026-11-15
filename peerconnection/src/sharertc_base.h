/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARERTC_BASE_H_
#define SHARERTC_BASE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/environment/environment.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_peer_connection.h"
#include "sharertc/peerconnection/src/sharertc_signal_connection.h"
#include "sharertc/peerconnection/src/sharertc_signal_message.h"

// Common part of ShareRTCControl and ShareRTCClient.
//
// Owns the signaling connection and a dedicated WebRTC signaling thread. The
// ISignalSocket callbacks can arrive on any thread; they are reposted to the
// signaling thread, where all PeerConnection work happens.
class ShareRTCBase : public ISignalSocketObserver,
                     public ShareRTCSignalConnection::Delegate {
 public:
  ShareRTCBase(const webrtc::Environment& env,
               std::unique_ptr<ISignalSocket> signalsocket);
  ~ShareRTCBase() override;

  void Login(const char* signalserver, const char* token);
  void Logout();
  const char* SignalServerAddr() const;
  bool IsSignalServerOK();

  // ISignalSocketObserver implementation. These callbacks may run on the
  // socket's own thread.
  void onConnected() override;
  void onDisconnected() override;
  void onError(int code, const char* msg) override;
  void onDataReceived(const uint8_t* data, size_t len) override;

  // ShareRTCSignalConnection::Delegate implementation. Always called on the
  // signaling thread.
  void OnIdentified(const std::string& id) override;
  void OnOffer(const std::string& sender, const std::string& sdp) override;
  void OnAnswer(const std::string& sender, const std::string& sdp) override;
  void OnRemoteIceCandidate(const std::string& sender,
                            const ICECandidateContent& candidate) override;
  void OnDisconnect(const std::string& sender) override;
  void OnTextMessage(const std::string& sender,
                     const std::string& text) override;
  void OnDisconnectionNotification(const std::string& peer_id) override;

 protected:
  // Posts `task` to the signaling thread. Runs it inline when already there.
  // The signaling thread outlives every derived object.
  template <typename Functor>
  void RunOnSignalingThread(Functor&& task) {
    if (!signaling_thread_) {
      return;
    }
    if (signaling_thread_->IsCurrent()) {
      std::forward<Functor>(task)();
    } else {
      signaling_thread_->PostTask(std::forward<Functor>(task));
    }
  }

  // Send a raw (already serialized) message through the signal socket.
  int SendMessage(const std::string& json_object);

  // Creates the PeerConnectionFactory if needed. Signaling thread only.
  bool InitializePeerConnection();

  // Returns the PeerConnection for `remote_id`, creating it if necessary.
  // Signaling thread only.
  ShareRTCPeerConnection* GetOrCreatePeerConnection(
      const std::string& remote_id);
  ShareRTCPeerConnection* FindPeerConnection(const std::string& remote_id);
  // Like FindPeerConnection(), but falls back to the only connection when the
  // signaling server addresses us with a different id than the one used when
  // the connection was created.
  ShareRTCPeerConnection* ResolvePeerConnection(const std::string& sender);
  void DestroyPeerConnection(const std::string& remote_id);
  void DestroyAllPeerConnections();

  // ICE servers used for every PeerConnection. An empty `urls` keeps the
  // default (GetPeerConnectionString(), overridable with WEBRTC_CONNECT).
  void ConfigureIceServers(const std::vector<std::string>& urls,
                           const std::string& username,
                           const std::string& credential);

  webrtc::PeerConnectionInterface::RTCConfiguration
  CreatePeerConnectionConfig() const;

  // Role specific hooks. All are invoked on the signaling thread.
  virtual void OnSignalIdentified(const std::string& id) {}
  virtual void OnPeerConnectionStateChange(
      const std::string& peer_id,
      webrtc::PeerConnectionInterface::PeerConnectionState state) {}
  virtual void OnPeerSignalingStateChange(
      const std::string& peer_id,
      webrtc::PeerConnectionInterface::SignalingState state) {}
  virtual void OnPeerNegotiationNeeded(const std::string& peer_id) {}
  virtual void OnRemoteDataChannel(const std::string& peer_id,
                                   const std::string& label) {}
  virtual void OnPeerDisconnected(const std::string& peer_id) {}
  virtual void OnSignalConnectionClosed() {}

  std::string peer_id_;  // Target peer (control side).
  std::string my_id_;    // Id assigned by the signaling server.
  const webrtc::Environment env_;
  std::unique_ptr<ISignalSocket> socket_;
  std::unique_ptr<webrtc::Thread> signaling_thread_;
  ShareRTCSignalConnection signal_connection_;
  std::string signal_server_addr_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  webrtc::PeerConnectionInterface::RTCConfiguration pc_config_;

  // Declared after signaling_thread_ so that connections are destroyed before
  // the thread stops.
  std::map<std::string, std::unique_ptr<ShareRTCPeerConnection>>
      peer_connections_;
  std::map<std::string, std::vector<ICECandidateContent>>
      pending_remote_candidates_;
};

#endif  // SHARERTC_BASE_H_
