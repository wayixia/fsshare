
#include "websocket_client.h"
#include "webrtc_signal_message.h"

#pragma once

class WebRTCSignalConnection : public WebSocketClient {
public:
  WebRTCSignalConnection(SimpleThread* net_thread);
  ~WebRTCSignalConnection();

// Operations
public:
  void IdentifySelf();

// Handle messages
public:
  void HandleMessage(const std::string& msg);
  bool HandleIdentifySelf( BaseContent* content );
  
  
private:
  std::string id_;
};
