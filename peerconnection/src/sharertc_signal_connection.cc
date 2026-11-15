#include "sharertc/peerconnection/src/sharertc_signal_connection.h"

#include <iostream>
#include <utility>

#include "json/writer.h"
#include "rtc_base/logging.h"

namespace {

std::string SerializeMessage(const Message& msg) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, messageToJson(msg));
}

}  // namespace

ShareRTCSignalConnection::ShareRTCSignalConnection(
    std::function<int(const std::string&)> sendmsg,
    Delegate* delegate)
    : delegate_(delegate), sendmsg_fn_(std::move(sendmsg)) {
  // Dispatch Message map
  msgmap_[MessageType::GetAllPeerIDs] = std::bind(
      &ShareRTCSignalConnection::HandleMessageGetAllPeerIDs, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::TextMessage] = std::bind(
      &ShareRTCSignalConnection::HandleMessageTextMessage, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::Disconnect] = std::bind(
      &ShareRTCSignalConnection::HandleMessageDisconnect, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::Offer] = std::bind(
      &ShareRTCSignalConnection::HandleMessageOffer, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::Answer] = std::bind(
      &ShareRTCSignalConnection::HandleMessageAnswer, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::ICECandidate] = std::bind(
      &ShareRTCSignalConnection::HandleMessageICECandidate, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::IdentifySelf] = std::bind(
      &ShareRTCSignalConnection::HandleMessageIdentifySelf, this,
      std::placeholders::_1, std::placeholders::_2);
  msgmap_[MessageType::DisconnectionNotification] = std::bind(
      &ShareRTCSignalConnection::HandleMessageDisconnectionNotification, this,
      std::placeholders::_1, std::placeholders::_2);
}

ShareRTCSignalConnection::~ShareRTCSignalConnection() = default;

int ShareRTCSignalConnection::SendMessage(MessageType kind,
                                          ReachType reach,
                                          const std::string& peer_id,
                                          const Json::Value& content) {
  Message msg;
  msg.kind = kind;
  msg.reach = reach;
  msg.sender = id_;
  msg.peerID = peer_id;
  msg.content = content;

  if (!sendmsg_fn_) {
    return -1;
  }
  return sendmsg_fn_(SerializeMessage(msg));
}

int ShareRTCSignalConnection::IdentifySelf() {
  IdentifySelfContent content;
  content.ID.clear();
  return SendMessage(MessageType::IdentifySelf, ReachType::Self, "",
                     content.toJson());
}

int ShareRTCSignalConnection::SendOffer(const std::string& to,
                                        const std::string& sdp) {
  OfferContent content;
  content.sdp = sdp;
  return SendMessage(MessageType::Offer, ReachType::OnePeer, to,
                     content.toJson());
}

int ShareRTCSignalConnection::SendAnswer(const std::string& to,
                                         const std::string& sdp) {
  AnswerContent content;
  content.sdp = sdp;
  return SendMessage(MessageType::Answer, ReachType::OnePeer, to,
                     content.toJson());
}

int ShareRTCSignalConnection::SendIceCandidate(
    const std::string& to,
    const ICECandidateContent& candidate) {
  return SendMessage(MessageType::ICECandidate, ReachType::OnePeer, to,
                     candidate.toJson());
}

int ShareRTCSignalConnection::SendDisconnect(const std::string& to) {
  DisconnectContent content;
  return SendMessage(MessageType::Disconnect, ReachType::OnePeer, to,
                     content.toJson());
}

bool ShareRTCSignalConnection::HandleMessage(const std::string& msg,
                                             std::string& err) {
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  std::string errors;
  bool success = reader->parse(msg.data(), msg.data() + msg.size(), &root,
                               &errors);
  if (!success) {
    err = "failed to parse signal message: " + errors;
    RTC_LOG(LS_WARNING) << err;
    return false;
  }

  Message out_msg;
  if (!jsonToMessage(root, out_msg, err)) {
    RTC_LOG(LS_WARNING) << "[signal] invalid message: " << err;
    return false;
  }

  std::unique_ptr<BaseContent> content = out_msg.unmarshalContent(err);
  if (!content) {
    RTC_LOG(LS_WARNING) << "[signal] invalid content: " << err;
    return false;
  }

  auto handler = msgmap_.find(out_msg.kind);
  if (handler == msgmap_.end() || !handler->second) {
    err = "[signal] unsupported message type: " +
          messageTypeToString(out_msg.kind);
    RTC_LOG(LS_WARNING) << err;
    return false;
  }
  return handler->second(out_msg, content.get());
}

bool ShareRTCSignalConnection::HandleMessageGetAllPeerIDs(
    const Message& msg,
    BaseContent* content) {
  return true;
}

bool ShareRTCSignalConnection::HandleMessageTextMessage(
    const Message& msg,
    BaseContent* content) {
  auto* text = static_cast<TextMessageContent*>(content);
  if (delegate_) {
    delegate_->OnTextMessage(msg.sender, text->text);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageDisconnect(
    const Message& msg,
    BaseContent* content) {
  if (delegate_) {
    delegate_->OnDisconnect(msg.sender);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageOffer(const Message& msg,
                                                  BaseContent* content) {
  auto* offer = static_cast<OfferContent*>(content);
  if (msg.sender.empty()) {
    RTC_LOG(LS_WARNING) << "[signal] offer without sender";
    return false;
  }
  if (delegate_) {
    delegate_->OnOffer(msg.sender, offer->sdp);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageAnswer(const Message& msg,
                                                   BaseContent* content) {
  auto* answer = static_cast<AnswerContent*>(content);
  if (msg.sender.empty()) {
    RTC_LOG(LS_WARNING) << "[signal] answer without sender";
    return false;
  }
  if (delegate_) {
    delegate_->OnAnswer(msg.sender, answer->sdp);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageICECandidate(
    const Message& msg,
    BaseContent* content) {
  auto* candidate = static_cast<ICECandidateContent*>(content);
  if (msg.sender.empty()) {
    RTC_LOG(LS_WARNING) << "[signal] ice candidate without sender";
    return false;
  }
  if (delegate_) {
    delegate_->OnRemoteIceCandidate(msg.sender, *candidate);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageIdentifySelf(
    const Message& msg,
    BaseContent* content) {
  auto* identify = static_cast<IdentifySelfContent*>(content);
  if (identify->ID.empty()) {
    RTC_LOG(LS_WARNING) << "[signal] identify self without id";
    return false;
  }
  id_ = identify->ID;
  RTC_LOG(LS_INFO) << "[signal] identified as " << id_;
  if (delegate_) {
    delegate_->OnIdentified(id_);
  }
  return true;
}

bool ShareRTCSignalConnection::HandleMessageDisconnectionNotification(
    const Message& msg,
    BaseContent* content) {
  auto* notification = static_cast<DisconnectionNotificationContent*>(content);
  if (delegate_) {
    delegate_->OnDisconnectionNotification(notification->peerID);
  }
  return true;
}
