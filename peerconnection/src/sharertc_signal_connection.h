
#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_signal_message.h"
#include <map>
#include <functional>

#pragma once

class ShareRTCSignalConnection 
: public ISignalSocketObserver {
public:
  ShareRTCSignalConnection(ISignalSocket* sock);
  ~ShareRTCSignalConnection();

// Operations
public:
  void IdentifySelf();

public:
  void onConnected() override {}
  void onDisconnected() override {}
  void onError(int code, const char* msg) override {}
  void onDataReceived(const uint8_t* data, size_t len) override {}

// Handle messages
public:
  bool HandleMessage(const std::string& msg, std::string& err);
  bool HandleMessageGetAllPeerIDs( BaseContent* content );
  bool HandleMessageTextMessage( BaseContent* content );
  bool HandleMessageDisconnect( BaseContent* content );
  bool HandleMessageOffer( BaseContent* content );
  bool HandleMessageAnswer( BaseContent* content );
  bool HandleMessageICECandidate( BaseContent* content );
  bool HandleMessageIdentifySelf( BaseContent* content );
  bool HandleMessageDisconnectionNotification( BaseContent* content);
  
private:
  std::map<MessageType, std::function<bool(BaseContent*)> > msgmap_;
  std::string id_;
  std::unique_ptr<ISignalSocket> socket_;
};
