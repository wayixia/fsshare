


#include "sharertc/sharertc.h"

#include <api/environment/environment_factory.h>
#include <api/field_trials.h>
#include "sharertc/peerconnection/src/sharertc_session.h"

namespace {
  static webrtc::Environment g_webrtc_env;
}

SHARERTCCLIENT_API void InitializeShareRTC()
{
  g_webrtc_env = webrtc::CreateEnvironment();
}

SHARERTCCLIENT_API void UnInitializeShareRTC()
{

}


SHARERTCCLIENT_API IShareRTCClient* CreateShareRTCClient()
{
  IShareRTCClient* instance = new ShareRTCClient(g_webrtc_env);
}

/**
 * @brief 销毁 IShareRTCClient 实例
 * @param pClient 由 CreateShareRTCClient 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCClient(IShareRTCClient* pClient)
{

}


void test_client(){
  ISignalSocket* ss;
  ShareRTCClient client(ss);

  client.SetObserver();

  std::string username;
  std::string crdential;
  client.Login( username, crdential );


}


void test_control()
{
  ISignalSocket* ss;



}