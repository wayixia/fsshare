#include "websocket_client.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace {

enum class EndpointType { kClient, kControl };

// 使用外部传入的网络线程创建 WebSocket。
class WebSocketFactory : public IWebSocketFactory {
 public:
  explicit WebSocketFactory(SimpleThread* net_thread)
      : net_thread_(net_thread) {}

  ISignalSocket* CreateWebSocket() override {
    return new WebSocketClient(net_thread_);
  }

 private:
  SimpleThread* net_thread_;
};

// 监听 control 端 data channel 的打开/关闭状态。
class ConsoleChannelObserver : public IShareRTCChannelObserver {
 public:
  void OnOpen() override {
    open_ = true;
    std::cout << "[channel] open" << std::endl;
  }
  void OnClose() override {
    open_ = false;
    std::cout << "[channel] closed" << std::endl;
  }

  bool is_open() const { return open_.load(); }

 private:
  std::atomic<bool> open_{false};
};

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0 << " --type <client|control>\n"
            << "  -t, --type   Endpoint type to start\n"
            << std::endl;
}

void PrintCommands() {
  std::cout << "\nCommand List:\n"
            << "help: Display this help text\n"
            << "quit: Exit the program\n"
            << "login <url> <token>\n"
            << "    client:  login ws://host:port/signalingserver <token>\n"
            << "    control: login https://host:port/<peerid> <token>\n"
            << "logout: Disconnect from the signaling server\n"
            << "send <text>: Send text on the 'data' channel (control only)\n"
            << std::endl;
}

bool ParseEndpointType(int argc, char** argv, EndpointType* type) {
  bool has_type = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    std::string value;
    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else if (arg == "--type" || arg == "-t") {
      if (i + 1 >= argc) {
        return false;
      }
      value = argv[++i];
    } else if (arg.rfind("--type=", 0) == 0) {
      value = arg.substr(std::strlen("--type="));
    } else {
      return false;
    }

    if (value == "client") {
      *type = EndpointType::kClient;
    } else if (value == "control") {
      *type = EndpointType::kControl;
    } else {
      return false;
    }
    has_type = true;
  }
  return has_type;
}

std::string TrimLeft(std::string text) {
  const size_t first = text.find_first_not_of(" \t");
  return first == std::string::npos ? std::string() : text.substr(first);
}

}  // namespace

int main(int argc, char** argv) {
  EndpointType type;
  if (!ParseEndpointType(argc, argv, &type)) {
    PrintUsage(argv[0]);
    return 1;
  }

  WebSocketClient::Initialize();

  SimpleThread net_thread("WebSocketNetThread");
  net_thread.Start();

  WebSocketFactory factory(&net_thread);
  ConsoleChannelObserver channel_observer;
  IShareRTCClient* client = nullptr;
  IShareRTCControl* control = nullptr;
  int channel_id = 0;

  if (type == EndpointType::kClient) {
    client = CreateShareRTCClient(&factory);
  } else {
    control = CreateShareRTCControl(&factory);
    if (control) {
      control->SetObserver(&channel_observer);
    }
  }
  if (!client && !control) {
    std::cout << "Failed to create ShareRTC object" << std::endl;
    return 1;
  }

  std::cout << "ShareRTC " << (client ? "client" : "control")
            << " started. Type 'help' for commands." << std::endl;

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream iss(line);
    std::string command;
    if (!(iss >> command)) {
      continue;
    }

    if (command == "quit" || command == "exit") {
      break;
    } else if (command == "help") {
      PrintCommands();
    } else if (command == "login") {
      std::string url;
      std::string token;
      if (!(iss >> url >> token)) {
        std::cout << "Usage: login <url> <token>" << std::endl;
        continue;
      }
      if (client) {
        client->Login(url.c_str(), token.c_str());
        std::cout << "login: " << url << std::endl;
      } else {
        if (channel_id <= 0) {
          channel_id = control->CreateChannel("data", &channel_observer);
        }
        const int rc = control->Connect(url.c_str(), token.c_str());
        if (rc == 0) {
          std::cout << "login: " << url << " (channel " << channel_id << ")"
                    << std::endl;
        } else {
          std::cout << "login failed, code " << rc << std::endl;
        }
      }
    } else if (command == "logout") {
      if (client) {
        client->Logout();
      } else {
        control->Disconnect();
        channel_id = 0;
      }
      std::cout << "logout" << std::endl;
    } else if (command == "send") {
      std::string text;
      std::getline(iss, text);
      text = TrimLeft(std::move(text));
      if (text.empty()) {
        std::cout << "Usage: send <text>" << std::endl;
        continue;
      }
      if (!control) {
        std::cout << "send is only supported for control endpoints"
                  << std::endl;
        continue;
      }
      if (channel_id <= 0) {
        channel_id = control->CreateChannel("data", &channel_observer);
      }
      if (!channel_observer.is_open()) {
        std::cout << "channel is not open yet, message dropped" << std::endl;
        continue;
      }
      const int rc = control->Send(
          channel_id, reinterpret_cast<const uint8_t*>(text.data()),
          text.size());
      std::cout << "send(" << channel_id << "): "
                << (rc == 0 ? "queued" : "failed") << std::endl;
    } else {
      std::cout << "Unrecognized Command: " << command << std::endl;
    }
  }

  if (control) {
    control->Disconnect();
    DestroyShareRTCControl(control);
  }
  if (client) {
    client->Logout();
    DestroyShareRTCClient(client);
  }
  // The network thread must outlive the sockets, which are owned (and
  // destroyed) by the ShareRTC objects above.
  net_thread.Stop();
  WebSocketClient::Uninitialize();
  return 0;
}
