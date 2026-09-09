
#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include "sharertc/peerconnection/src/sharertc_signal_message.h"
#include <map>
#include <functional>

#pragma once

class ShareRTCSignalConnection 
{
public:
  ShareRTCSignalConnection( const std::function<int(const std::string&)>& sendmsg);
  ~ShareRTCSignalConnection();

// Operations
public:
  void IdentifySelf();


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
  std::function<int(const std::string&)> sendmsg_fn_;
};
