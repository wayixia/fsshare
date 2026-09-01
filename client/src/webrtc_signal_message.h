#pragma once
#include <string>
#include <memory>
#include <json/json.h>

// ---------------- ReachType ----------------
enum class ReachType : int {
    Self = 0,
    OnePeer,
    AllPeers,
    None
};

bool jsonValueToReachType(const Json::Value& jv, ReachType& outVal, std::string& err);
Json::Value reachTypeToJsonValue(ReachType rt);
std::string reachTypeToString(ReachType rt);

// ---------------- MessageType ----------------
enum class MessageType : int {
    GetAllPeerIDs = 0,
    TextMessage,
    Disconnect,
    Offer,
    Answer,
    ICECandidate,
    IdentifySelf,
    DisconnectionNotification
};

std::string messageTypeToString(MessageType t);



// ---------------- BaseContent 抽象基类 ----------------
struct IdentifySelfContent;


struct BaseContent {
  virtual ~BaseContent() = default;
  virtual Json::Value toJson() const = 0;
  virtual bool fromJson(const Json::Value& jv, std::string& err) = 0;
  
  virtual IdentifySelfContent* ToIdentifySelfContent() {
    return nullptr;
  }
  
};

// ---------------- Content 子类 ----------------
struct GetAllPeerIDsContent : public BaseContent {
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct TextMessageContent : public BaseContent {
    std::string text;
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct DisconnectContent : public BaseContent {
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct OfferContent : public BaseContent {
    std::string sdp;
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct AnswerContent : public BaseContent {
    std::string sdp;
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct ICECandidateContent : public BaseContent {
    std::string candidate;
    int sdpMLineIndex{0};
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

struct IdentifySelfContent : public BaseContent {
  std::string ID;
  Json::Value toJson() const override;
  bool fromJson(const Json::Value& jv, std::string& err) override;
  IdentifySelfContent* ToIdentifySelfContent() override;
};

struct DisconnectionNotificationContent : public BaseContent {
    std::string peerID;
    Json::Value toJson() const override;
    bool fromJson(const Json::Value& jv, std::string& err) override;
};

// 工厂函数
std::unique_ptr<BaseContent> createContentByKind(MessageType kind);

// ---------------- Message 主结构体 ----------------
struct Message {
    MessageType kind;
    ReachType reach;
    std::string sender;
    std::string peerID;
    Json::Value content;

    std::unique_ptr<BaseContent> unmarshalContent(std::string& outErr);
    bool marshalContent(const BaseContent& payload, std::string& outErr);
};

// 消息整体序列化/反序列化
Json::Value messageToJson(const Message& msg);
bool jsonToMessage(const Json::Value& root, Message& outMsg, std::string& outErr);
