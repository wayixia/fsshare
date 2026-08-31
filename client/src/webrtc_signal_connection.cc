
#include "webrtc_signal_connection.h"
#include "webrtc_signal_message.h"

#include <json/json.h>
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
  Json::Reader reader;
  Json::Value root;
  if( reader.parse(msg, root)
}
