
#include "webrtc_signal_connection.h"
#include <iostream>



WebRTCSignalConnection::WebRTCSignalConnection(SimpleThread* net_thread)
: WebSocketClient(net_thread)
{
  
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

void WebRTCSignalConnection::HandleMessage(const std::string &msg)
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
    return;
  }

  std::cout << "mesasge type: " << root["kind"].asString() << std::endl;
  Message outMsg;
  std::string err;
  if( !jsonToMessage(root, outMsg, err) ) {
    std::cout << "[signal] handle message error: invalid message" << std::endl;
    return;
  }
  
  //std::unique_ptr<BaseContent> content = outMsg.unmarshalContent(err);
  
  bool result = false;
  
  // Dispatch Message
  switch( outMsg.kind ) {
    case MessageType::GetAllPeerIDs:
      
      break;
    case MessageType::TextMessage:
      
      break;
    case MessageType::Disconnect:
      
      break;
    case MessageType::Offer:
      
      break;
    case MessageType::Answer:
      
      break;
    case MessageType::ICECandidate:
      
      break;
    case MessageType::IdentifySelf:
      {
        IdentifySelfContent content;
        if(content.fromJson(outMsg.content, err) ) {
          HandleIdentifySelf(content);
        }
      }
      break;
    case MessageType::DisconnectionNotification:
      
      break;
  }
  
  std::cout << "" << std::endl;
}


bool WebRTCSignalConnection::HandleIdentifySelf( BaseContent* content )
{
  IdentifySelfContent* c = content->ToIdentifySelfContent();
  if( !c ) {
    return false;
  }
  if( c->ID.empty() )
  {
    return false;
  }
  
  id_ = content.ID;
  return true;
}
