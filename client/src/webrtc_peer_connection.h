
#pragma once

class WebRTCPeerConnection {
public:
    WebRTCPeerConnection(
        const webrtc::PeerConnectionInterface::RTCConfiguration& config,
        signalingserverconn::SignalingServerConn* sig_conn,
        std::string connectedPeerID);
    ~WebRTCPeerConnection();

    WebRTCPeerConnection(const WebRTCPeerConnection&) = delete;
    WebRTCPeerConnection& operator=(const WebRTCPeerConnection&) = delete;

    int SendOffer();
    int SetLocalDescription(const Json::Value& input);
    int SetRemoteDescription(const Json::Value& input);
    int AddICECandidate(const Json::Value& input);
    int SendPendingICECandidates();
    int SendAnswer();
    int SendMessage(const uint8_t* data, size_t len);

    void SendMessageJS();

private:
    std::string peer_ids_[2];
    signalingserverconn::SignalingServerConn* signaling_server_conn_{nullptr};

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

    std::vector<webrtc::IceCandidateInterface*> pending_candidates_;
    std::mutex candidate_mux_;

    void OnConnectionStateChange(webrtc::PeerConnectionState state);
    void OnNewDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dc);
    void OnICECandidate(const webrtc::IceCandidateInterface* candidate);

    void OnDCOpen(webrtc::DataChannelInterface* dc);
    void OnDCClose(webrtc::DataChannelInterface* dc);
    void OnDCMessage(const webrtc::DataChannelInterface::DataBuffer& buf);

    void SendSignalMessage(const message::Message& msg);
};
