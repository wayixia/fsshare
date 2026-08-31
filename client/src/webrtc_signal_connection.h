
#include "websocket_client.h"

#pragma once

class WebRTCSignalConnection : public WebSocketClient {
public:
  WebRTCSignalConnection(SimpleThread* net_thread);
  ~WebRTCSignalConnection();
  
public:
  void IdentifySelf();
  void HandleMessage(const std::string& msg);

};
