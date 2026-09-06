#ifndef SHARERTCCLIENT_H
#define SHARERTCCLIENT_H

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
    virtual bool connect(const char* host, int port) = 0;
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

    virtual void Login(const std::string& server, int port) = 0;
    virtual void DisconnectFromServer() = 0;
    virtual void ConnectToPeer(int peer_id) = 0;
    virtual void DisconnectPeer(int peer_id) = 0;
    virtual void Logout() = 0;
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
SHARERTCCLIENT_API IShareRTCClient* CreateShareRTCClient();

/**
 * @brief 销毁 IShareRTCClient 实例
 * @param pClient 由 CreateShareRTCClient 返回的指针
 */
SHARERTCCLIENT_API void DestroyShareRTCClient(IShareRTCClient* pClient);

#ifdef __cplusplus
}
#endif

#endif // SHARERTCCLIENT_H