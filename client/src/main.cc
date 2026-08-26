
#include <iostream>
#include <string>
#include "websocket_client.h"

int main() {
  bool done = false;
  std::string input;
  SimpleThread net_thread("WebSocketNetThread");
  WebSocketClient client(&net_thread); // 传入 nullptr 或实际的 net_thread 指针
  net_thread.Start(); // 启动线程
 
  while (!done) {
    std::cout << "Enter Command: ";
    std::cin >> std::ws;
    auto& r = std::getline(std::cin, input);
 
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
            std::cout << "Connecting to " << url << "...";
        } else {
            std::cout << "Failed to initiate connection.";
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

  net_thread.Stop(); // 停止线程
 
  return 0;
}
