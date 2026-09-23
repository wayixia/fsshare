
#include "webrtc_signal_connection.h"
#include <iostream>
#include <string>

int main() {
  WebSocketClient::Initialize();
  SimpleThread net_thread("WebSocketNetThread");
  WebSocketClient wssock(&net_thread); // 传入 nullptr 或实际的 net_thread 指针
  // client.on_state_change = [&client](WsClientState state) {
  //   const char* sstate = "unknown";
  //   switch( state ) {
  //     case WsClientState::kOpen:
  //       sstate = "open";
  //       //client.IdentifySelf();
  //       //client.SendText("{\"kind\":\"TextMessage\"}");
  //       break;
  //     case WsClientState::kClosing:
  //       sstate = "closing";
  //       break;
  //     case WsClientState::kConnecting:
  //       sstate = "connecting";
  //       break;
  //     case WsClientState::kDisconnected:
  //       sstate = "disconnected";
  //       break;
  //   } // end switch
  //   std::cout << "[websocket] state => " << sstate << std::endl;
  // };
  
  // client.on_text_msg = [&client](const std::string& msg){
  //   //std::cout << "[wsclient]text message ->" << msg << std::endl;
  //   std::string err;
  //   if( !client.HandleMessage(msg, err) ) {
  //     std::cout << "[wsclient] handle message failed ->" << msg << std::endl;
  //   }
  // };
  
  // client.on_error = []( int err ) {
    
  // };
  
  IShareRTCClient* cli = CreateShareRTCClient(&wssock);
  net_thread.Start(); // 启动线程

#if 0

#else
  std::string url;
  //std::cout << "Enter WebSocket URL (ws:// or wss://): ";
  //std::getline(std::cin, url);
  url = "ws://localhost:8090/signalingserver";
  cli->Login( url.c_str(), "localhost");
  
  
  SimpleThread net_thread2("WebSocketNetThread");
  WebSocketClient wssock2(&net_thread2); // 传入 nullptr 或实际的 net_thread 指针
  IShareRTCControl* ctrl = CreateShareRTCControl(&wssock2);
  net_thread2.Start(); // 启动线程
  
  
//  while(1) {
//    std::this_thread::sleep_for(std::chrono::milliseconds(50));
//  }
#endif
  
  
  std::string input;
  while (std::getline(std::cin, input)) {

    if (input == "quit") {
      break;
    } else if (input == "help") {
      std::cout << "\nCommand List:\n"
        << "help: Display this help text\n"
        << "quit: Exit the program\n"
        << std::endl;
    } else if (input == "connect") {
      ctrl->Connect("https://localhost:8090/abc", "abc");
    } else if (input == "send") {
        std::string message;
        
    } else if (input == "close") {
        //std::cout << "Connection closed." << std::endl;
      
    } else {
      std::cout << "Unrecognized Command" << std::endl;
    }
  }
  
  WebSocketClient::Uninitialize();
  net_thread.Stop(); // 停止线程
 
  return 0;
}
