

#include "sharertc/peerconnection/include/sharertc/sharertc.h"
#include <api/environment/environment_factory.h>
#include <api/field_trials.h>
#include "sharertc/peerconnection/src/sharertc_connection.h"
#include "sharertc/peerconnection/src/sharertc_client.h"
#include "sharertc/peerconnection/src/sharertc_control.h"

namespace {
  static webrtc::Environment g_webrtc_env = webrtc::CreateEnvironment();
}

SHARERTCCLIENT_API void InitializeShareRTC()
{
}

SHARERTCCLIENT_API void UnInitializeShareRTC()
{

}


SHARERTCCLIENT_API IShareRTCClient* CreateShareRTCClient( ISignalSocket* sock)
{
  IShareRTCClient* instance = new ShareRTCClient(g_webrtc_env, sock);

  return instance;
}

/**
 * @brief 销毁 IShareRTCClient 实例
 * @param pClient 由 CreateShareRTCClient 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCClient(IShareRTCClient* p)
{
  if( p )
  {
    delete p;
  }
}


SHARERTCCLIENT_API IShareRTCControl* CreateShareRTCControl( ISignalSocket* sock)
{
  IShareRTCControl* instance = new ShareRTCControl(g_webrtc_env, sock);

  return instance;
}

/**
 * @brief 销毁 IShareRTCClient 实例
 * @param pClient 由 CreateShareRTCClient 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCControl(IShareRTCClient* p)
{
  if( p )
  {
    delete p;
  }
}



// void test_client(){
//   ISignalSocket* ss;
//   ShareRTCClient client(ss);

//   client.SetObserver();

//   std::string username;
//   std::string crdential;
//   client.Login( username, crdential );


// }


// void test_control()
// {
//   ISignalSocket* ss;



// }