#include "websocket_client.h"
#include "compatible.h"
#include <libwebsockets.h>
#include <cstring>
#include <cstdarg>
#include <memory>
#include <iostream>




struct WebSocketClient::Impl {
  Impl(SimpleThread* t);
  ~Impl();

  void Start();
  void EvtLoop();
  std::vector<uint8_t> msg_buf;
  uint8_t msg_opcode; // 记录第一条分片opcode
  WsClientConfig config;
  std::string url_;
  std::string host;
  int port = 0;
  std::string path;
  bool use_tls = false;
  unsigned int pending_write_flags = 0; // 新增：保存待发送帧标记
  struct lws_context* context = nullptr;
  struct lws* wsi = nullptr;

  int retry_counter = 0;
  bool exiting = false;

  WebSocketClient* outer = nullptr;
  SimpleThread* net_thread_;

  WsClientState state = WsClientState::kDisconnected;

  void SetState(WsClientState s);
  void Log(const char* fmt, ...);
  bool ParseUrl(const std::string& url);
  void TryReconnect();

  // lws回调
  static int LwsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len);

  std::vector<std::string> build_custom_headers();
};

WebSocketClient::WebSocketClient(SimpleThread* net_thread)
  : impl_(std::make_unique<Impl>(net_thread))
  , net_thread_(net_thread) {
  impl_->outer = this;
  
  lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, nullptr);
}

WebSocketClient::~WebSocketClient() {
  Close();
}

void WebSocketClient::SetConfig(const WsClientConfig& cfg) {
  //RTC_DCHECK_RUN_ON(net_thread_);
  impl_->config = cfg;
}


bool WebSocketClient::ConnectUrl(const std::string& url) {
  net_thread_->PostTask([this,url]{
    DoConnectUrl(url);
  });
  impl_->Start();
}

bool WebSocketClient::DoConnectUrl(const std::string& url) {
  //RTC_DCHECK_RUN_ON(net_thread_);
  if (!impl_->ParseUrl(url)) {
    if(on_error) {
      on_error(-100);
    }
    return false;
  }
  
  impl_->retry_counter = 0;
  impl_->exiting = false;
  impl_->SetState(WsClientState::kConnecting);

  
  static struct lws_protocols protocols[] = {
    { "wss", WebSocketClient::Impl::LwsCallback, 0, 0 },
    { NULL, NULL, 0, 0 }
  };
  
  lws_context_creation_info info{};
  info.port = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  info.ssl_cert_filepath = nullptr;
  info.ssl_private_key_filepath = nullptr;


  info.user = impl_.get();
  info.protocols = protocols;

  impl_->context = lws_create_context(&info);
  if (!impl_->context) {
    impl_->Log("lws create context failed");
    if(on_error) on_error(-101);
    return false;
  }

  // 发起连接
  struct lws_client_connect_info cci{};
  cci.context = impl_->context;
  cci.address = impl_->host.c_str();
  cci.port = impl_->port;
  cci.path = impl_->path.c_str();
  cci.ssl_connection = impl_->use_tls ? LCCSCF_USE_SSL : 0;

  if(impl_->use_tls){
    if(!impl_->config.verify_ssl_peer)
      cci.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    if(impl_->config.allow_self_signed)
      cci.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
  }

  cci.host = impl_->host.c_str();
  cci.origin = impl_->config.origin.c_str();
  cci.userdata = impl_.get();
  cci.opaque_user_data = impl_.get();
  cci.protocol = "wss";
  auto hdrs = impl_->build_custom_headers();
  std::vector<const char*> hdr_ptrs;
  for(auto& s : hdrs) hdr_ptrs.push_back(s.c_str());
  hdr_ptrs.push_back(nullptr);
  //cci.extra_headers = hdr_ptrs.data();

  impl_->wsi = lws_client_connect_via_info(&cci);
  if (!impl_->wsi) {
    impl_->Log("lws_client_connect_via_info fail");
    impl_->TryReconnect();
  }



  return true;
}

void WebSocketClient::Close() {
  //RTC_DCHECK_RUN_ON(net_thread_);
  impl_->exiting = true;
  if (impl_->wsi) {
    lws_close_reason(impl_->wsi, LWS_CLOSE_STATUS_NORMAL, nullptr,0);
    impl_->wsi = nullptr;
  }
  if (impl_->context) {
    lws_context_destroy(impl_->context);
    impl_->context = nullptr;
  }
  impl_->SetState(WsClientState::kDisconnected);
}

bool WebSocketClient::SendText(const std::string& text) {
  DCHECK_RUN_ON(net_thread_);
  if(impl_->state != WsClientState::kOpen || !impl_->wsi)
    return false;
  size_t buf_len = LWS_PRE + text.size();
  unsigned char* p = (unsigned char*)malloc(buf_len);
  memcpy(p + LWS_PRE, text.data(), text.size());
  int ret = lws_write(impl_->wsi, p+LWS_PRE, text.size(), LWS_WRITE_TEXT);
  free(p);
  return ret >= 0;
}

bool WebSocketClient::SendBinary(const uint8_t* data, size_t len) {
  //RTC_DCHECK_RUN_ON(net_thread_);
  if(impl_->state != WsClientState::kOpen || !impl_->wsi) return false;
  size_t buf_len = LWS_PRE + len;
  unsigned char* p = (unsigned char*)malloc(buf_len);
  memcpy(p + LWS_PRE, data, len);
  int ret = lws_write(impl_->wsi, p+LWS_PRE, len, LWS_WRITE_BINARY);
  free(p);
  return ret >=0;
}

// Impl 实现
WebSocketClient::Impl::Impl(SimpleThread* t)
: net_thread_(t)
{
}

WebSocketClient::Impl::~Impl() {
}

void WebSocketClient::Impl::Start()
{
  net_thread_->PostDelayedTask([this](){ EvtLoop(); }, 0);
}

void WebSocketClient::Impl::EvtLoop() {
  if (exiting) 
  {
    return;
  }
  lws_service(context, 20);
  net_thread_->PostDelayedTask([this](){ EvtLoop(); }, 0);
}

void WebSocketClient::Impl::SetState(WsClientState s) {
  if(state == s) return;
  state = s;
  outer->net_thread_->PostTask([this,s](){
   if(outer->on_state_change) outer->on_state_change(s);
  });
}

void WebSocketClient::Impl::Log(const char* fmt, ...) {
  char buf[1024] = {0};
  memset(buf, 0x00, sizeof(buf));
  va_list ap;
  va_start(ap,fmt);
  vsnprintf(buf,sizeof(buf),fmt,ap);
  va_end(ap);
  printf( "[LibWSClient] %s\n", buf );
}

bool WebSocketClient::Impl::ParseUrl(const std::string& url) {
  use_tls = false;
  host.clear(); path.clear();
  port = 0;
  size_t proto_end = 0;
  if(url.substr(0,5)=="ws://"){
    proto_end =5;
    use_tls=false;
    port=80;
  } else if(url.substr(0,6)=="wss://") {
    proto_end=6;
    use_tls=true;
    port=443;
  } else {
    return false;
  }

  size_t host_start = proto_end;
  size_t path_pos = url.find('/', host_start);
  size_t port_pos = url.find(':', host_start);
  if(path_pos == std::string::npos) {
    path_pos = url.size(); path="/";
  } else {
    path = url.substr(path_pos);
  }

  if(port_pos != std::string::npos && port_pos < path_pos){
    host = url.substr(host_start, port_pos-host_start);
    std::string ps = url.substr(port_pos+1, path_pos-port_pos-1);
    port = atoi(ps.c_str());
  } else {
    host = url.substr(host_start, path_pos-host_start);
  }
  if(host.empty()) {
    return false;
  }
  Log("parse url tls=%d host=%s port=%d path=%s", use_tls, host.c_str(), port, path.c_str());
  return true;
}

std::vector<std::string> WebSocketClient::Impl::build_custom_headers() {
  std::vector<std::string> out;
  if(!config.authorization.empty()){
    out.emplace_back("Authorization: " + config.authorization);
  }
  for(auto& kv : config.extra_headers){
    out.emplace_back(kv.first + ": " + kv.second);
  }
  return out;
}

void WebSocketClient::Impl::TryReconnect() {
  if(exiting) return;
  if(retry_counter >= config.max_retry_count){
    Log("max retry reached");
    net_thread_->PostTask([this](){
     if(outer->on_error) outer->on_error(-2);
    });
    return;
  }
  retry_counter++;
  Log("schedule reconnect try=%d delay=%dms", retry_counter, config.retry_delay_ms);
  net_thread_->PostDelayedTask([this](){
    outer->ConnectUrl(url_);
  }, config.retry_delay_ms);
}

int WebSocketClient::Impl::LwsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                            void *user, void *in, size_t len) {
  auto* impl = (WebSocketClient::Impl*)user;
  switch (reason) {
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    impl->SetState(WsClientState::kOpen);
    ///impl->outer->SendText("{\"kind\":\"TextMessage\"}");
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE: {
    uint8_t opcode = lws_get_opcode(wsi);
    int is_first = lws_is_first_fragment(wsi);
    int is_fin = lws_is_final_fragment(wsi);
    // 第一条分片保存opcode，后续continuation(0x0)直接复用
    if(is_first) {
      impl->msg_opcode = opcode;
    }

    // 追加payload
    impl->msg_buf.insert(impl->msg_buf.end(),(uint8_t*)in, ((uint8_t*)in)+len);

    if(is_fin) {
      // 完整消息
      if(impl->msg_opcode == 0x1) {
        // TEXT消息
        impl->net_thread_->PostTask([impl](){
          if(impl->outer->on_text_msg) {
            impl->outer->on_text_msg(
              std::string((const char*)impl->msg_buf.data(), impl->msg_buf.size()));
          }
          impl->msg_buf.clear();
        });
      } else if(impl->msg_opcode == 0x2) {
        // BINARY消息
        impl->net_thread_->PostTask([impl](){
          if(impl->outer->on_binary_msg) {
            impl->outer->on_binary_msg(impl->msg_buf.data(), impl->msg_buf.size());
          }
          impl->msg_buf.clear();
        });
      }
    }
    // lws回调内部245行替换
    //bool is_text = !!(impl->pending_write_flags & LWS_WRITE_TEXT);
//    enum lws_write_protocol pktype = lws_get_packet_type(wsi) ;
//    if( pktype == LWS_WRITE_TEXT ) {
//      std::string msg((char*)in, len);
//      impl->net_thread_->PostTask([impl,msg](){
//        if(impl->outer->on_text_msg) impl->outer->on_text_msg(msg);
//      });
//    }else if(pktype == LWS_WRITE_BINARY) {
//      std::vector<uint8_t> bin((uint8_t*)in, (uint8_t*)in+len);
//      impl->net_thread_->PostTask([impl, bin](){
//        if(impl->outer->on_binary_msg) impl->outer->on_binary_msg(bin.data(), bin.size());
//      });
//    }
    break;
  }

  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    impl->Log("client connect error %s", in ? (char*)in:"unknown");
    impl->wsi = nullptr;
    impl->SetState(WsClientState::kDisconnected);
    impl->TryReconnect();
    break;

  case LWS_CALLBACK_CLOSED:
    impl->Log("connection closed");
    impl->wsi = nullptr;
    impl->SetState(WsClientState::kDisconnected);
    if(!impl->exiting) impl->TryReconnect();
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
    impl->Log("recv pong");
    break;

  default:
    break;
  }
  return 0;
}


void WebSocketClient::Initialize() {
  lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, nullptr);
}

void WebSocketClient::Uninitialize() {
  // nothing to do
}
