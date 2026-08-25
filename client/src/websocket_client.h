#pragma once

#include "simple_thread.h"
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
class WebSocketClient {
public:
  explicit WebSocketClient( SimpleThread* net_thread);
  ~WebSocketClient();

  void SetConfig(const WsClientConfig& cfg);

  bool ConnectUrl(const std::string& url); // ws:// wss://
  void Close();

  bool SendText(const std::string& text);
  bool SendBinary(const uint8_t* data, size_t len);

  // 信号，全部在 net_thread 触发
  std::function<void(WsClientState)> on_state_change;
  std::function<void(const std::string&)> on_text_msg;
  std::function<void(const uint8_t*, size_t)> on_binary_msg;
  std::function<void(int err)> on_error;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  SimpleThread* net_thread_;
};