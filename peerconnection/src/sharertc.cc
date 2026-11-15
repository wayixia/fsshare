#include "sharertc/peerconnection/include/sharertc/sharertc.h"

#include <memory>

#include <api/environment/environment_factory.h>
#include <api/field_trials.h>

#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"
#include "sharertc/peerconnection/src/sharertc_client.h"
#include "sharertc/peerconnection/src/sharertc_control.h"

namespace {
webrtc::Environment GetWebRtcEnvironment() {
  static webrtc::Environment env = webrtc::CreateEnvironment();
  return env;
}

std::unique_ptr<ISignalSocket> CreateSignalSocket(
    IWebSocketFactory* factory) {
  if (!factory) {
    return nullptr;
  }
  return std::unique_ptr<ISignalSocket>(factory->CreateWebSocket());
}
}  // namespace

SHARERTCCLIENT_API void InitializeShareRTC() {
  static bool ssl_initialized = false;
  if (!ssl_initialized) {
    if (!webrtc::InitializeSSL()) {
      RTC_LOG(LS_ERROR) << "Failed to initialize SSL";
    }
    ssl_initialized = true;
  }
}

SHARERTCCLIENT_API void UnInitializeShareRTC() {
  static bool ssl_cleaned_up = false;
  if (!ssl_cleaned_up) {
    webrtc::CleanupSSL();
    ssl_cleaned_up = true;
  }
}

SHARERTCCLIENT_API IShareRTCClient* CreateShareRTCClient(
    IWebSocketFactory* factory) {
  InitializeShareRTC();
  auto socket = CreateSignalSocket(factory);
  if (!socket) {
    RTC_LOG(LS_ERROR) << "WebSocket factory returned no socket";
    return nullptr;
  }
  return new ShareRTCClient(GetWebRtcEnvironment(), std::move(socket));
}

SHARERTCCLIENT_API void DestroyShareRTCClient(IShareRTCClient* p) {
  delete p;
}

SHARERTCCLIENT_API IShareRTCControl* CreateShareRTCControl(
    IWebSocketFactory* factory) {
  InitializeShareRTC();
  auto socket = CreateSignalSocket(factory);
  if (!socket) {
    RTC_LOG(LS_ERROR) << "WebSocket factory returned no socket";
    return nullptr;
  }
  return new ShareRTCControl(GetWebRtcEnvironment(), std::move(socket));
}

SHARERTCCLIENT_API void DestroyShareRTCControl(IShareRTCControl* p) {
  delete p;
}
