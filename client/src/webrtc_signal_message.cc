#include "webrtc_signal_message.h"
#include "compatible.h"
#include <sstream>

// ========== ReachType ==========
std::string reachTypeToString(ReachType rt)
{
    switch (rt)
    {
    case ReachType::Self:      return "Self";
    case ReachType::OnePeer:   return "OnePeer";
    case ReachType::AllPeers:  return "AllPeers";
    case ReachType::None:      return "None";
    default:
        return "Unknown(" + std::to_string(static_cast<int>(rt)) + ")";
    }
}

Json::Value reachTypeToJsonValue(ReachType rt)
{
    return Json::Value(reachTypeToString(rt));
}

bool jsonValueToReachType(const Json::Value& jv, ReachType& outVal, std::string& err)
{
    err.clear();
    if (!jv.isString())
    {
        err = "ReachType expect string value";
        return false;
    }
    std::string s = jv.asString();
    if (s == "Self")
    {
        outVal = ReachType::Self;
    }
    else if (s == "OnePeer")
    {
        outVal = ReachType::OnePeer;
    }
    else if (s == "AllPeers")
    {
        outVal = ReachType::AllPeers;
    }
    else if (s == "None")
    {
        outVal = ReachType::None;
    }
    else
    {
        err = "unknown ReachType string: " + s;
        return false;
    }
    return true;
}

// ========== MessageType ==========
std::string messageTypeToString(MessageType t) {
    switch (t) {
    case MessageType::GetAllPeerIDs: return "GetAllPeerIDs";
    case MessageType::TextMessage: return "TextMessage";
    case MessageType::Disconnect: return "Disconnect";
    case MessageType::Offer: return "Offer";
    case MessageType::Answer: return "Answer";
    case MessageType::ICECandidate: return "ICECandidate";
    case MessageType::IdentifySelf: return "IdentifySelf";
    case MessageType::DisconnectionNotification: return "DisconnectionNotification";
    default:
        return "Unknown(" + std::to_string(static_cast<int>(t)) + ")";
    }
}

// ========== 工厂函数 ==========
std::unique_ptr<BaseContent> createContentByKind(MessageType kind) {
    switch (kind) {
    case MessageType::GetAllPeerIDs:
        return std::make_unique<GetAllPeerIDsContent>();
    case MessageType::TextMessage:
        return std::make_unique<TextMessageContent>();
    case MessageType::Disconnect:
        return std::make_unique<DisconnectContent>();
    case MessageType::Offer:
        return std::make_unique<OfferContent>();
    case MessageType::Answer:
        return std::make_unique<AnswerContent>();
    case MessageType::ICECandidate:
        return std::make_unique<ICECandidateContent>();
    case MessageType::IdentifySelf:
        return std::make_unique<IdentifySelfContent>();
    case MessageType::DisconnectionNotification:
        return std::make_unique<DisconnectionNotificationContent>();
    default:
        return nullptr;
    }
}

// -------- GetAllPeerIDsContent --------
Json::Value GetAllPeerIDsContent::toJson() const {
    return Json::Value(Json::objectValue);
}
bool GetAllPeerIDsContent::fromJson(const Json::Value& /*jv*/, std::string& /*err*/) {
    return true;
}

// -------- DisconnectContent --------
Json::Value DisconnectContent::toJson() const {
    return Json::Value(Json::objectValue);
}
bool DisconnectContent::fromJson(const Json::Value& /*jv*/, std::string& /*err*/) {
    return true;
}

// -------- TextMessageContent --------
Json::Value TextMessageContent::toJson() const {
    Json::Value jv;
    jv["text"] = text;
    return jv;
}
bool TextMessageContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("text")) {
        err = "missing field text";
        return false;
    }
    text = jv["text"].asString();
    return true;
}

// -------- OfferContent --------
Json::Value OfferContent::toJson() const {
    Json::Value jv;
    jv["sdp"] = sdp;
    return jv;
}
bool OfferContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("sdp")) {
        err = "missing field sdp";
        return false;
    }
    sdp = jv["sdp"].asString();
    return true;
}

// -------- AnswerContent --------
Json::Value AnswerContent::toJson() const {
    Json::Value jv;
    jv["sdp"] = sdp;
    return jv;
}
bool AnswerContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("sdp")) {
        err = "missing field sdp";
        return false;
    }
    sdp = jv["sdp"].asString();
    return true;
}

// -------- ICECandidateContent --------
Json::Value ICECandidateContent::toJson() const {
    Json::Value jv;
    jv["candidate"] = candidate;
    jv["sdpMLineIndex"] = sdpMLineIndex;
    return jv;
}
bool ICECandidateContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("candidate") || !jv.isMember("sdpMLineIndex")) {
        err = "missing candidate / sdpMLineIndex";
        return false;
    }
    candidate = jv["candidate"].asString();
    sdpMLineIndex = jv["sdpMLineIndex"].asInt();
    return true;
}

// -------- IdentifySelfContent --------
Json::Value IdentifySelfContent::toJson() const {
    Json::Value jv;
    jv["ID"] = ID;
    return jv;
}
bool IdentifySelfContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("ID")) {
        err = "missing field name";
        return false;
    }
    ID = jv["ID"].asString();
    return true;
}

// -------- DisconnectionNotificationContent --------
Json::Value DisconnectionNotificationContent::toJson() const {
    Json::Value jv;
    jv["peerID"] = peerID;
    return jv;
}
bool DisconnectionNotificationContent::fromJson(const Json::Value& jv, std::string& err) {
    if (!jv.isMember("peerID")) {
        err = "missing field peerID";
        return false;
    }
    peerID = jv["peerID"].asString();
    return true;
}

// ========= Message 成员 =========
std::unique_ptr<BaseContent> Message::unmarshalContent(std::string& outErr) {
    outErr.clear();
    if (content.isNull()) {
        outErr = "content is null";
        return nullptr;
    }
    auto payload = createContentByKind(kind);
    if (!payload) {
        outErr = "invalid message kind: " + messageTypeToString(kind);
        return nullptr;
    }
    if (!payload->fromJson(content, outErr)) {
        return nullptr;
    }
    return payload;
}

bool Message::marshalContent(const BaseContent& payload, std::string& outErr) {
    outErr.clear();
    try {
        content = payload.toJson();
    } catch (...) {
        outErr = "marshal content exception";
        return false;
    }
    return true;
}

// ========= 顶层消息序列化 =========
Json::Value messageToJson(const Message& msg)
{
    Json::Value root;
    root["kind"]  = messageTypeToString(msg.kind);
    root["reach"] = reachTypeToJsonValue(msg.reach);
    root["sender"] = msg.sender;
    root["peerID"] = msg.peerID;
    root["content"] = msg.content;
    return root;
}

bool jsonToMessage(const Json::Value& root, Message& outMsg, std::string& outErr)
{
    outErr.clear();
    try
    {
        outMsg.kind = static_cast<MessageType>(root["kind"].asInt());
        if (!jsonValueToReachType(root["reach"], outMsg.reach, outErr))
        {
            return false;
        }
        outMsg.sender = root["sender"].asString();
        outMsg.peerID  = root["peerID"].asString();
        outMsg.content = root["content"];
    }
    catch (const std::exception& e)
    {
        outErr = std::string("parse message root: ") + e.what();
        return false;
    }
    return true;
}
