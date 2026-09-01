
#include "webrtcpeerconn.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ---------------- jsoncpp serialize impl ----------------
namespace message {

bool OfferContentToJson(const OfferContent& src, Json::Value& out) {
    out = Json::Value(Json::objectValue);
    out["type"] = src.type;
    out["SDP"] = src.SDP;
    return true;
}

bool JsonToOfferContent(const Json::Value& in, OfferContent& out) {
    if (!in.isMember("type") || !in.isMember("SDP")) return false;
    out.type = in["type"].asInt();
    out.SDP = in["SDP"].asString();
    return true;
}

bool ICECandidateContentToJson(const ICECandidateContent& src, Json::Value& out) {
    out = Json::Value(Json::objectValue);
    out["Candidate"] = src.Candidate;
    out["SdpMid"] = src.SdpMid;
    out["SdpMLineIndex"] = src.SdpMLineIndex;
    out["UsernameFragment"] = src.UsernameFragment;
    return true;
}

bool JsonToICECandidateContent(const Json::Value& in, ICECandidateContent& out) {
    if (!in.isMember("Candidate")) return false;
    out.Candidate = in["Candidate"].asString();
    out.SdpMid = in["SdpMid"].asString();
    out.SdpMLineIndex = in["SdpMLineIndex"].asInt();
    out.UsernameFragment = in["UsernameFragment"].asString();
    return true;
}

bool MessageToJson(const Message& src, Json::Value& out) {
    out = Json::Value(Json::objectValue);
    int kind_val = 0;
    switch(src.Kind) {
        case Kind::Offer: kind_val = 0; break;
        case Kind::Answer: kind_val = 1; break;
        case Kind::ICECandidate: kind_val = 2; break;
    }
    out["Kind"] = kind_val;
    out["PeerID"] = src.PeerID;
    out["Sender"] = src.Sender;
    out["Reach"] = static_cast<int>(src.Reach);
    out["Content"] = src.Content;
    return true;
}

bool JsonToMessage(const Json::Value& in, Message& out) {
    if(!in.isMember("Kind") || !in.isMember("PeerID") || !in.isMember("Sender") || !in.isMember("Content"))
        return false;
    int kv = in["Kind"].asInt();
    switch(kv) {
        case 0: out.Kind = Kind::Offer; break;
        case 1: out.Kind = Kind::Answer; break;
        case 2: out.Kind = Kind::ICECandidate; break;
        default: return false;
    }
    out.PeerID = in["PeerID"].asString();
    out.Sender = in["Sender"].asString();
    out.Reach = static_cast<Message::Reach>(in["Reach"].asInt());
    out.Content = in["Content"];
    return true;
}

} // namespace message

void Log(const std::string& txt) {
#ifdef __EMSCRIPTEN__
    EM_ASM({ console.log(UTF8ToString($0)); }, txt.c_str());
#else
    std::cerr << txt << "\n";
#endif
}

// WebRTC Observer
class PeerConnObserver : public webrtc::PeerConnectionObserver {
public:
    explicit PeerConnObserver(WebRTCPeerConnection* outer) : outer_(outer) {}
    void OnConnectionChange(webrtc::PeerConnectionState state) override {
        outer_->OnConnectionStateChange(state);
    }
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dc) override {
        outer_->OnNewDataChannel(dc);
    }
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        outer_->OnICECandidate(candidate);
    }
private:
    WebRTCPeerConnection* outer_;
};

class DataChannelObserver : public webrtc::DataChannelObserver {
public:
    explicit DataChannelObserver(WebRTCPeerConnection* outer) : outer_(outer) {}
    void OnStateChange() override {}
    void OnMessage(const webrtc::DataChannelInterface::DataBuffer& buf) override {
        outer_->OnDCMessage(buf);
    }
    void OnOpen() override { outer_->OnDCOpen(nullptr); }
    void OnClose() override { outer_->OnDCClose(nullptr); }
private:
    WebRTCPeerConnection* outer_;
};

WebRTCPeerConnection::WebRTCPeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    signalingserverconn::SignalingServerConn* sig_conn,
    std::string connectedPeerID)
: signaling_server_conn_(sig_conn)
{
    peer_ids_[0] = sig_conn->PeerID();
    peer_ids_[1] = std::move(connectedPeerID);

    webrtc::PeerConnectionDependencies deps(nullptr);
    deps.observer = std::make_unique<PeerConnObserver>(this);
    auto create_res = webrtc::CreatePeerConnectionOrError(config, std::move(deps));
    if (!create_res.ok()) {
        Log("CreatePeerConnection failed: " + create_res.error().message());
        return;
    }
    peer_connection_ = create_res.MoveValue();

    std::string dc_label = "chan" + peer_ids_[0] + peer_ids_[1];
    webrtc::DataChannelInit dc_init;
    auto dc_res = peer_connection_->CreateDataChannelOrError(dc_label, &dc_init);
    if (!dc_res.ok()) {
        Log("Problem creating dataChannel: " + dc_res.error().message());
        return;
    }
    data_channel_ = dc_res.MoveValue();
    data_channel_->RegisterObserver(std::make_unique<DataChannelObserver>(this));
}

WebRTCPeerConnection::~WebRTCPeerConnection() {
    {
        std::lock_guard<std::mutex> lk(candidate_mux_);
        for(auto* c : pending_candidates_) delete c;
        pending_candidates_.clear();
    }
    if(data_channel_) data_channel_->UnregisterObserver();
    if(peer_connection_) peer_connection_->Close();
}

int WebRTCPeerConnection::SendOffer() {
    auto offer_res = peer_connection_->CreateOffer();
    if (!offer_res.ok()) {
        Log("CreateOffer error: " + offer_res.error().message());
        return -1;
    }
    auto offer = offer_res.MoveValue();
    auto set_local = peer_connection_->SetLocalDescription(offer->Copy());
    if (!set_local.ok()) {
        Log("SetLocalDescription error: " + set_local.error().message());
        return -1;
    }

    message::OfferContent oc;
    oc.type = static_cast<int>(offer->GetType());
    oc.SDP = offer->sdp();

    Json::Value oc_json;
    message::OfferContentToJson(oc, oc_json);

    message::Message msg;
    msg.Kind = message::Kind::Offer;
    msg.Sender = peer_ids_[0];
    msg.PeerID = peer_ids_[1];
    msg.Reach = message::Message::OnePeer;
    msg.Content = oc_json;

    SendSignalMessage(msg);
    return 0;
}

int WebRTCPeerConnection::SetLocalDescription(const Json::Value& input) {
    message::OfferContent sdp;
    if (!message::JsonToOfferContent(input, sdp)) {
        Log("Unmarshal error for OfferContent");
        return -1;
    }

    webrtc::SdpType sdp_type{};
    switch (sdp.type) {
        case 1: sdp_type = webrtc::SdpType::kOffer; break;
        case 3: sdp_type = webrtc::SdpType::kAnswer; break;
        default:
            Log("sdp type neither offer nor answer");
            return -1;
    }

    auto desc = webrtc::CreateSessionDescription(sdp_type, sdp.SDP);
    auto err = peer_connection_->SetRemoteDescription(std::move(desc));
    if (!err.ok()) {
        Log("SetRemoteDescription error: " + err.error().message());
    } else {
        Log("Successfully set remote description using sdp");
    }
    return 0;
}

int WebRTCPeerConnection::SetRemoteDescription(const Json::Value& input) {
    return SetLocalDescription(input);
}

int WebRTCPeerConnection::AddICECandidate(const Json::Value& input) {
    message::ICECandidateContent ic;
    if (!message::JsonToICECandidateContent(input, ic)) {
        Log("Unmarshal ICE candidate error");
        return -1;
    }

    std::unique_ptr<webrtc::IceCandidateInterface> candidate;
    bool ok = webrtc::CreateIceCandidate(
        ic.SdpMid,
        ic.SdpMLineIndex,
        ic.Candidate,
        &candidate);
    if (!ok) {
        Log("CreateIceCandidate parse failed");
        return -1;
    }

    auto err = peer_connection_->AddIceCandidate(std::move(candidate));
    if (!err.ok()) {
        Log("AddIceCandidate error: " + err.error().message());
    } else {
        Log("Successfully added ICE Candidate");
    }
    return 0;
}

int WebRTCPeerConnection::SendPendingICECandidates() {
    std::lock_guard<std::mutex> lk(candidate_mux_);
    for(auto* c : pending_candidates_) {
        auto err = peer_connection_->AddIceCandidate(c->Copy());
        if (!err.ok()) {
            Log("Error sending ICE candidate: " + err.error().message());
        }
        delete c;
    }
    pending_candidates_.clear();
    return 0;
}

int WebRTCPeerConnection::SendAnswer() {
    auto ans_res = peer_connection_->CreateAnswer();
    if (!ans_res.ok()) {
        Log("CreateAnswer error: " + ans_res.error().message());
        return -1;
    }
    auto answer = ans_res.MoveValue();
    auto set_local = peer_connection_->SetLocalDescription(answer->Copy());
    if (!set_local.ok()) {
        Log("SetLocalDescription error: " + set_local.error().message());
        return -1;
    }

    message::OfferContent oc;
    oc.type = static_cast<int>(answer->GetType());
    oc.SDP = answer->sdp();
    Json::Value oc_json;
    message::OfferContentToJson(oc, oc_json);

    message::Message msg;
    msg.Kind = message::Kind::Offer; // Go原版bug：这里应当改为 message::Kind::Answer
    msg.Sender = peer_ids_[0];
    msg.PeerID = peer_ids_[1];
    msg.Reach = message::Message::OnePeer;
    msg.Content = oc_json;

    SendSignalMessage(msg);
    return 0;
}

int WebRTCPeerConnection::SendMessage(const uint8_t* data, size_t len) {
    if (!data_channel_) {
        Log("dataChannel null");
        return -1;
    }
    webrtc::DataChannelInterface::DataBuffer buf(data, len, false);
    bool ok = data_channel_->Send(buf);
    if (!ok) {
        Log("Error sending message on dataChannel " + data_channel_->label());
        return -1;
    }
    return 0;
}

void WebRTCPeerConnection::SendMessageJS() {
#ifdef __EMSCRIPTEN__
    EM_JS(void, do_send_msg_js, (WebRTCPeerConnection* self), {
        const v = document.getElementById("message").value;
        const bytes = new TextEncoder().encode(v);
        const ptr = Module._malloc(bytes.length);
        Module.HEAPU8.set(bytes, ptr);
        Module._Z20WebRTCPeerConnection_SendMessageP17WebRTCPeerConnectionPKhm(self, ptr, bytes.length);
        Module._free(ptr);
    });
    do_send_msg_js(this);
#endif
}

void WebRTCPeerConnection::SendSignalMessage(const message::Message& msg) {
    Json::Value root;
    if (!message::MessageToJson(msg, root)) {
        Log("MessageToJson failed");
        return;
    }
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "";
    std::string json_str = Json::writeString(wbuilder, root);

    int ret = signaling_server_conn_->Send(json_str);
    if (ret != 0) {
        Log("Error sending signal message");
    }
}

void WebRTCPeerConnection::OnConnectionStateChange(webrtc::PeerConnectionState state) {
    Log("Peer connection state has changed: " + webrtc::PeerConnectionStateToString(state));
    if (state == webrtc::PeerConnectionState::kFailed) {
        Log("Peer connection has gone to failed exiting");
        std::exit(0);
    }
    if (state == webrtc::PeerConnectionState::kClosed) {
        Log("Peer connection has gone to closed exiting");
        std::exit(0);
    }
}

void WebRTCPeerConnection::OnNewDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dc) {
    Log("New data channel '" + dc->label() + "'");
    dc->RegisterObserver(std::make_unique<DataChannelObserver>(this));
}

void WebRTCPeerConnection::OnICECandidate(const webrtc::IceCandidateInterface* candidate) {
    if (!candidate) return;
    std::lock_guard<std::mutex> lk(candidate_mux_);

    auto remote_desc = peer_connection_->remote_description();
    if (!remote_desc) {
        pending_candidates_.push_back(candidate->Copy());
    } else {
        message::ICECandidateContent ic;
        ic.Candidate = candidate->candidate();
        ic.SdpMid = candidate->sdp_mid();
        ic.SdpMLineIndex = candidate->sdp_mline_index();
        ic.UsernameFragment = candidate->username_fragment();

        Json::Value ic_json;
        message::ICECandidateContentToJson(ic, ic_json);

        message::Message msg;
        msg.Kind = message::Kind::ICECandidate;
        msg.PeerID = peer_ids_[1];
        msg.Reach = message::Message::OnePeer;
        msg.Sender = peer_ids_[0];
        msg.Content = ic_json;

        SendSignalMessage(msg);
    }
}

void WebRTCPeerConnection::OnDCOpen(webrtc::DataChannelInterface* dc) {
    (void)dc;
    Log("Data channel open.");
}

void WebRTCPeerConnection::OnDCClose(webrtc::DataChannelInterface* dc) {
    (void)dc;
    Log("Data channel closed.");
}

void WebRTCPeerConnection::OnDCMessage(const webrtc::DataChannelInterface::DataBuffer& buf) {
    std::string text(buf.data.data<char>(), buf.data.size());
    Log("Message from DataChannel: " + text);
}
