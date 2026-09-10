
#include "sharertc/peerconnection/src/sharertc_signal_connection.h"
#include <iostream>



ShareRTCSignalConnection::ShareRTCSignalConnection( const std::function<int(const std::string&)>& sendmsg )
: sendmsg_fn_(std::move(sendmsg))
{
  // Dispatch Message map
  msgmap_[MessageType::GetAllPeerIDs] = std::bind( &ShareRTCSignalConnection::HandleMessageGetAllPeerIDs, this, std::placeholders::_1);
  msgmap_[MessageType::TextMessage] = std::bind( &ShareRTCSignalConnection::HandleMessageTextMessage, this, std::placeholders::_1);
  msgmap_[MessageType::Disconnect] = std::bind( &ShareRTCSignalConnection::HandleMessageDisconnect, this, std::placeholders::_1);
  msgmap_[MessageType::Offer] = std::bind( &ShareRTCSignalConnection::HandleMessageOffer, this, std::placeholders::_1);
  msgmap_[MessageType::Answer] = std::bind( &ShareRTCSignalConnection::HandleMessageAnswer, this, std::placeholders::_1);
  msgmap_[MessageType::ICECandidate] = std::bind( &ShareRTCSignalConnection::HandleMessageICECandidate, this, std::placeholders::_1);
  msgmap_[MessageType::IdentifySelf] = std::bind( &ShareRTCSignalConnection::HandleMessageIdentifySelf, this, std::placeholders::_1);
  msgmap_[MessageType::DisconnectionNotification] = std::bind( &ShareRTCSignalConnection::HandleMessageDisconnectionNotification, this, std::placeholders::_1);
}

ShareRTCSignalConnection::~ShareRTCSignalConnection()
{
  
}

void ShareRTCSignalConnection::IdentifySelf()
{
  std::unique_ptr<BaseContent> msg = createContentByKind(MessageType::IdentifySelf);
    IdentifySelfContent identifySelfMsgContent{};
    identifySelfMsgContent.ID = "";

    // 序列化内层content
    Json::Value contentJson = identifySelfMsgContent.toJson();
    std::string identifySelfMsgContentJson;
    identifySelfMsgContentJson = contentJson.toStyledString();

    // 组装外层Message
    Message identifySelfMsg{};
    identifySelfMsg.kind    = MessageType::IdentifySelf;
    identifySelfMsg.reach   = ReachType::Self;
    identifySelfMsg.peerID  = "";
    identifySelfMsg.content = identifySelfMsgContentJson;

    // 序列化整条消息
    Json::Value outerJson = messageToJson(identifySelfMsg);

    std::string identifySelfMsgJson;
        identifySelfMsgJson = outerJson.toStyledString();

    //SendText(identifySelfMsgJson); 
    sendmsg_fn_(identifySelfMsgJson);
}

bool ShareRTCSignalConnection::HandleMessage(const std::string &msg, std::string& err)
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


bool ShareRTCSignalConnection::HandleMessageGetAllPeerIDs( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageTextMessage( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageDisconnect( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageOffer( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageAnswer( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageICECandidate( BaseContent* content ) {
  return false;
}

bool ShareRTCSignalConnection::HandleMessageIdentifySelf( BaseContent* content ) {
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

bool ShareRTCSignalConnection::HandleMessageDisconnectionNotification( BaseContent* content)
{
  return true;
}


