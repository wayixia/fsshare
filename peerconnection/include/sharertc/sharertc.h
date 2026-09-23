#ifndef SHARERTC_H
#define SHARERTC_H

#include <memory>
#include <string>
#include <cstdint>


// ==================== DLL 导出/导入宏 ====================
#ifdef _WIN32
    #ifdef SHARERTCCLIENT_EXPORTS
        #define SHARERTCCLIENT_API __declspec(dllexport)
    #else
        #define SHARERTCCLIENT_API __declspec(dllimport)
    #endif
#else  // Linux / macOS
    #ifdef SHARERTCCLIENT_EXPORTS
        #define SHARERTCCLIENT_API __attribute__((visibility("default")))
    #else
        #define SHARERTCCLIENT_API
    #endif
#endif



// 网络事件回调
class ISignalSocketObserver {
public:
    virtual ~ISignalSocketObserver() = default;
    virtual void onConnected() = 0;
    virtual void onDisconnected() = 0;
    virtual void onDataReceived(const uint8_t* data, size_t len) = 0;
    virtual void onError(int code, const char* msg) = 0;
};

// 底层网络抽象
class ISignalSocket {
public:
    virtual ~ISignalSocket() = default;
    virtual void setObserver(ISignalSocketObserver* observer) = 0;
    virtual bool connect(const char* url, const char* token) = 0;
    virtual void disconnect() = 0;
    virtual bool send(const uint8_t* data, size_t len) = 0;
    virtual bool isConnected() const = 0;
};

// // ==================== 信令连接接口 ====================
// /**
//  * @brief WebRTC 信令连接抽象接口
//  *
//  * 负责 SDP（Offer/Answer）和 ICE Candidate 的收发，以及房间管理。
//  * 具体实现应封装底层信令传输协议（如 WebSocket、HTTP 等）。
//  */
// class ISignalConnection {
// public:
//     virtual ~ISignalConnection() = default;

//     // ----- 连接管理 -----
//     virtual bool Connect(const std::string& serverAddress) = 0;
//     virtual void Disconnect() = 0;
//     virtual bool IsConnected() const = 0;

//     // ----- SDP 信令 -----
//     virtual void SendDescription(const webrtc::SessionDescriptionInterface* sdp) = 0;
//     virtual void OnRemoteDescription(
//         std::function<void(std::unique_ptr<webrtc::SessionDescriptionInterface> sdp)> callback
//     ) = 0;

//     // ----- ICE 信令 -----
//     virtual void SendIceCandidate(const webrtc::IceCandidateInterface* candidate) = 0;
//     virtual void OnIceCandidate(
//         std::function<void(std::unique_ptr<webrtc::IceCandidateInterface> candidate)> callback
//     ) = 0;

//     // ----- 房间与会话 -----
//     virtual void JoinRoom(const std::string& roomId, const std::string& userId) = 0;
//     virtual void LeaveRoom() = 0;

//     // ----- 事件回调 -----
//     virtual void OnConnectionStateChange(std::function<void(bool connected)> callback) = 0;
//     virtual void OnError(std::function<void(const std::string& error)> callback) = 0;
// };


class IShareRTCChannelObserver {
public:
  virtual ~IShareRTCChannelObserver() = default;
  virtual void OnOpen() = 0;
  virtual void OnClose() = 0;
};


// class IShareRTCChannel {
// public:
//   virtual ~IShareRTCChannel() = default;
//   virtual int Send( const uint8_t* data, size_t len) = 0;
// };

// class IShareRTCConnection {
// public:
//   virtual ~IShareRTCConnection() = default;
// };


// ==================== RTC 客户端控制接口 ====================
/**
 * @brief WebRTC 媒体会话控制接口
 *
 * 负责登录/登出信令服务器，以及对远端 Peer 的连接/断开。
 * 内部会利用 ISignalConnection 进行信令交互。
 */
class IShareRTCClient {
public:
    virtual ~IShareRTCClient() = default;

    virtual void Login(const char* signalserver, const char* token ) = 0;
    virtual void DisconnectFromServer() = 0;
    virtual void DisconnectPeer(int peer_id) = 0;
    virtual void Logout() = 0;
};



class IShareRTCControlObserver {
public:
  virtual ~IShareRTCControlObserver() = default;
  virtual void OnStateChange() = 0;
};


class IShareRTCControl {
public:
  virtual ~IShareRTCControl() = default;

  /** \brief 设置外部状态监听 
   * 
   */
  virtual void SetObserver( IShareRTCChannelObserver* ) = 0;

  /** \brief 登录API服务器，获取登录信令服务器token，用于链接客户端 
   *  
   */
  virtual int Connect( const char* url, const char* usertoken ) = 0;


  /** \brief 退出API服务器，关闭与信令服务器的连接 
   * 
   */
  virtual void Disconnect() = 0;

  /** \brief 创建Channel 返回ChannelID
   * 
   */
  virtual int CreateChannel( const char* label, IShareRTCChannelObserver* ) = 0;

  /** \brief 发送channel数据
   * 
   */
  virtual int Send( int channelid, const uint8_t* data, size_t len) = 0;
};



// ==================== C 风格工厂函数（DLL 导出） ====================
#ifdef __cplusplus
extern "C" {
#endif


SHARERTCCLIENT_API void InitializeShareRTC();
SHARERTCCLIENT_API void UnInitializeShareRTC();

/**
 * @brief 创建 IShareRTCClient 实例
 * @param pSignal 信令连接对象指针（由调用者创建并管理生命周期，必须保持有效）
 * @return 成功返回对象指针，失败返回 nullptr
 */
SHARERTCCLIENT_API IShareRTCClient* CreateShareRTCClient( ISignalSocket* sock );

/**
 * @brief 销毁 IShareRTCClient 实例
 * @param pClient 由 CreateShareRTCClient 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCClient(IShareRTCClient* p);

/**
 * @brief 创建 IShareRTCControl 实例
 * @param pSignal 信令连接对象指针（由调用者创建并管理生命周期，必须保持有效）
 * @return 成功返回对象指针，失败返回 nullptr
 */
SHARERTCCLIENT_API IShareRTCControl* CreateShareRTCControl( ISignalSocket* sock );

/**
 * @brief 销毁 IShareRTCControl 实例
 * @param pClient 由 CreateShareRTCControl 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCControl(IShareRTCControl* p);




#ifdef __cplusplus
}
#endif

#endif // SHARERTC_H
