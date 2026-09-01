
#include "webrtc_signal_connection.h"
#include <iostream>
#include <string>

int main() {
  WebSocketClient::Initialize();

  SimpleThread net_thread("WebSocketNetThread");

  WebRTCSignalConnection client(&net_thread); // 传入 nullptr 或实际的 net_thread 指针
  client.on_state_change = [&client](WsClientState state) {
    const char* sstate = "unknown";
    switch( state ) {
      case WsClientState::kOpen:
        sstate = "open";
        client.IdentifySelf();
        //client.SendText("{\"kind\":\"TextMessage\"}");
        break;
      case WsClientState::kClosing:
        sstate = "closing";
        break;
      case WsClientState::kConnecting:
        sstate = "connecting";
        break;
      case WsClientState::kDisconnected:
        sstate = "disconnected";
        break;
    } // end switch
    std::cout << "[websocket] state => " << sstate << std::endl;
  };
  
  client.on_text_msg = [&client](const std::string& msg){
    //std::cout << "[wsclient]text message ->" << msg << std::endl;
    std::string err;
    if( !client.HandleMessage(msg, err) ) {
      std::cout << "[wsclient] handle message failed ->" << msg << std::endl;
    }
  };
  
  client.on_error = []( int err ) {
    
  };
  
  
  net_thread.Start(); // 启动线程

#if 0
  std::string input;
  while (std::getline(std::cin, input)) {

    if (input == "quit") {
      done = true;
    } else if (input == "help") {
      std::cout << "\nCommand List:\n"
        << "help: Display this help text\n"
        << "quit: Exit the program\n"
        << std::endl;
    } else if (input == "connect") {
        std::string url;
        //std::cout << "Enter WebSocket URL (ws:// or wss://): ";
        //std::getline(std::cin, url);
        url = "ws://localhost:8090/signalingserver";
        if (client.ConnectUrl(url)) {
            std::cout << "Connecting to " << url << "..." << std::endl;
        } else {
            std::cout << "Failed to initiate connection." << std::endl;
        }
    } else if (input == "send") {
        std::string message;
        std::cout << "Enter message to send: ";
        std::getline(std::cin, message);
        if (client.SendText(message)) {
            std::cout << "Message sent: " << message << std::endl;
        } else {
            std::cout << "Failed to send message." << std::endl;
        }
    } else if (input == "close") {
        client.Close();
        std::cout << "Connection closed." << std::endl;
    } else {
      std::cout << "Unrecognized Command" << std::endl;
    } 
  }
#else
  std::string url;
  //std::cout << "Enter WebSocket URL (ws:// or wss://): ";
  //std::getline(std::cin, url);
  url = "ws://localhost:8090/signalingserver";
  if (client.ConnectUrl(url)) {
      std::cout << "Connecting to " << url << "..." << std::endl;
  } else {
      std::cout << "Failed to initiate connection." << std::endl;
  }
  
  while(1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
#endif
  
  WebSocketClient::Uninitialize();
  net_thread.Stop(); // 停止线程
 
  return 0;
}
