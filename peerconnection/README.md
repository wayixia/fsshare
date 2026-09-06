
ShareRTC DLL 封装

定义以下类
ISignalSocket 底层网络库，用户网络层数据收发，处理网络连接状态， 由外部传入
SignalConnection 信令服务器登录链接, 用于收发信令消息，完成ice信息交换，以及其他业务逻辑
ShareRTCBase 包含SignalConnection实例，提供SignalConnection创建， 并监听SignalConnection相关事件
ShareRTCControl 继承ShareRTCBase，连接发起方，给定peerid相关信息，获取到iceservers，用于创建RTCSession（对webrtc peerconnection封装）
ShareRTCClient 继承ShareRTCBase，连接接收方，通过signalconnection登录信令服务器，申请peerid并将peerid跟用户自定义用户info绑定




