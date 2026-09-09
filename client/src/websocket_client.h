#pragma once

#include "simple_thread.h"
#include "sharertc/sharertc.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>


struct WsClientConfig {
  int connect_timeout_ms = 10000;
  int ping_interval_ms = 30000;
  int pong_timeout_ms = 10000;
  int max_retry_count = 5;
  int retry_delay_ms = 2000;

  bool verify_ssl_peer = true;
  bool allow_self_signed = false;

  std::string origin;
  std::string authorization;
  std::vector<std::pair<std::string, std::string>> extra_headers;
};

enum class WsClientState {
  kDisconnected,
  kConnecting,
  kOpen,
  kClosing
};

/**
 * libwebsockets WebSocket客户端
 * 事件回调内部跑在libws事件线程；消息会转发到传入的 webrtc net_thread
*/
class WebSocketClient : public ISignalSocket {
public:
  /** 初始化 */
  static void Initialize();

  /**  反初始化 */
  static void Uninitialize();

  void setObserver(ISignalSocketObserver* observer) override{
    assert( observer_ == nullptr);
    observer_ = observer;
  }
  
  
  bool connect(  const char* url, const char* token ) override {
    if( !url || strlen(url) == 0) {
      return false;
    }

    if( !token || strlen(token) == 0) {
      return false;
    }
    
    return ConnectUrl(url, token);
  }
  
  void disconnect() override {}
  bool send(const uint8_t* data, size_t len) override {return true;}
  bool isConnected() const override{ return true;};

public:
  explicit WebSocketClient( SimpleThread* net_thread);
  ~WebSocketClient();

  void SetConfig(const WsClientConfig& cfg);
  bool ConnectUrl(const std::string& url, const std::string& token); // ws:// wss://
  void Close();

  bool SendText(const std::string& text);
  bool SendBinary(const uint8_t* data, size_t len);
  
  void OnStateChange(WsClientState);
  void OnTextMessage(const std::string&);
  void OnBinaryMessage(const uint8_t*, size_t);
  void OnError(int err);

  // 信号，全部在 net_thread 触发
//  std::function<void(WsClientState)> on_state_change;
//  std::function<void(const std::string&)> on_text_msg;
//  std::function<void(const uint8_t*, size_t)> on_binary_msg;
//  std::function<void(int err)> on_error;

private:
  bool DoConnectUrl(const std::string& url, const std::string& token);
  
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  SimpleThread* net_thread_;
  ISignalSocketObserver* observer_ = nullptr;
};
