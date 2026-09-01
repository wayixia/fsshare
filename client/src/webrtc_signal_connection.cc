
#include "webrtc_signal_connection.h"
#include <iostream>



WebRTCSignalConnection::WebRTCSignalConnection(SimpleThread* net_thread)
: WebSocketClient(net_thread)
{
  // Dispatch Message map
  msgmap_[MessageType::GetAllPeerIDs] = std::bind( &WebRTCSignalConnection::HandleMessageGetAllPeerIDs, this, std::placeholders::_1);
  msgmap_[MessageType::TextMessage] = std::bind( &WebRTCSignalConnection::HandleMessageTextMessage, this, std::placeholders::_1);
  msgmap_[MessageType::Disconnect] = std::bind( &WebRTCSignalConnection::HandleMessageDisconnect, this, std::placeholders::_1);
  msgmap_[MessageType::Offer] = std::bind( &WebRTCSignalConnection::HandleMessageOffer, this, std::placeholders::_1);
  msgmap_[MessageType::Answer] = std::bind( &WebRTCSignalConnection::HandleMessageAnswer, this, std::placeholders::_1);
  msgmap_[MessageType::ICECandidate] = std::bind( &WebRTCSignalConnection::HandleMessageICECandidate, this, std::placeholders::_1);
  msgmap_[MessageType::IdentifySelf] = std::bind( &WebRTCSignalConnection::HandleMessageIdentifySelf, this, std::placeholders::_1);
  msgmap_[MessageType::DisconnectionNotification] = std::bind( &WebRTCSignalConnection::HandleMessageDisconnectionNotification, this, std::placeholders::_1);
}

WebRTCSignalConnection::~WebRTCSignalConnection()
{
  
}

void WebRTCSignalConnection::IdentifySelf()
{
  std::unique_ptr<BaseContent> msg = createContentByKind(MessageType::IdentifySelf);
    IdentifySelfContent identifySelfMsgContent{};
    identifySelfMsgContent.ID = "";

    // 序列化内层content
    Json::Value contentJson = identifySelfMsgContent.toJson();
    std::string identifySelfMsgContentJson;
    try {
      identifySelfMsgContentJson = contentJson.toStyledString();
    } catch (const std::exception& e) {
      std::cout << "Error parsing message content " << e.what() << std::endl;
      return;
    }

    // 组装外层Message
    Message identifySelfMsg{};
    identifySelfMsg.kind    = MessageType::IdentifySelf;
    identifySelfMsg.reach   = ReachType::Self;
    identifySelfMsg.peerID  = "";
    identifySelfMsg.content = identifySelfMsgContentJson;

    // 序列化整条消息
    Json::Value outerJson = messageToJson(identifySelfMsg);

    std::string identifySelfMsgJson;
    try {
        identifySelfMsgJson = outerJson.toStyledString();
    } catch (const std::exception& e) {
        std::cout << "Error marshalling message " << e.what() << std::endl;
        return;
    }

    SendText(identifySelfMsgJson); 
}

bool WebRTCSignalConnection::HandleMessage(const std::string &msg, std::string& err)
{
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
     
  std::string errors;
  bool success = reader->parse(
    msg.c_str(),                         // 起始指针
    msg.c_str() + msg.length(),   // 结束指针
    &root,
    &errors
  );

  if (!success) {
    std::cout << "解析失败: " << errors << std::endl;
    return false;
  }

  std::cout << "mesasge type: " << root["kind"].asString() << std::endl;
  Message outMsg;
  if( !jsonToMessage(root, outMsg, err) ) {
    std::cout << "[signal] handle message error: invalid message" << std::endl;
    return false;
  }
  
  std::unique_ptr<BaseContent> content = outMsg.unmarshalContent(err);
  if( !content ) {
    err = "[signal] handle message error: invalid message";
    return false;
  }
  
  auto handler = msgmap_.find(outMsg.kind);
  if( handler != msgmap_.end()) {
    auto f = handler->second;
    if( f ) {
      return f(content.get());
    }
  }
  
  std::cout << "[signal] invalid message type " << int(outMsg.kind) << std::endl;
  return false;
}


bool WebRTCSignalConnection::HandleMessageGetAllPeerIDs( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageTextMessage( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageDisconnect( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageOffer( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageAnswer( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageICECandidate( BaseContent* content ) {
  return false;
}

bool WebRTCSignalConnection::HandleMessageIdentifySelf( BaseContent* content ) {
  {
    IdentifySelfContent* c = content->ToIdentifySelfContent();
    if( !c ) {
      return false;
    }
    if( c->ID.empty() )
    {
      return false;
    }
    
    id_ = c->ID;
    
    return true;
  }
}

bool WebRTCSignalConnection::HandleMessageDisconnectionNotification( BaseContent* content)
{
  return true;
}


