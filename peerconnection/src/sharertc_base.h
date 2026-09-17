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

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "api/data_channel_interface.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

#include "sharertc/peerconnection/src/sharertc_connection.h"
#include "sharertc/peerconnection/src/sharertc_signal_connection.h"
#include "sharertc/peerconnection/include/sharertc/sharertc.h"



class ShareRTCBase
: public ISignalSocketObserver
//: public webrtc::PeerConnectionObserver
//, public webrtc::CreateSessionDescriptionObserver
{
public:
  ShareRTCBase(const webrtc::Environment& env, ISignalSocket* signalsocket );
  ~ShareRTCBase();

public:
  void Login(const char* signalserver, const char* token );

public:
  void onConnected() override;
  void onDisconnected() override {}
  void onError(int code, const char* msg) override {}
  void onDataReceived(const uint8_t* data, size_t len) override;

 protected:
  // Send a message to the remote peer.
  int SendMessage(const std::string& json_object);

  int peer_id_;
  bool loopback_;
  const webrtc::Environment env_;
  std::unique_ptr<webrtc::Thread> signaling_thread_;
  //webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  std::deque<std::string*> pending_messages_;
  std::string server_;

  ShareRTCSignalConnection signal_connection_;
  ISignalSocket* socket_;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
