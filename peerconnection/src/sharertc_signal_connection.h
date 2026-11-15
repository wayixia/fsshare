#pragma once

#include <functional>
#include <map>
#include <string>

#include "sharertc/peerconnection/src/sharertc_signal_message.h"

// Encodes/decodes signaling messages and dispatches the incoming ones to a
// Delegate. The Delegate callbacks are invoked synchronously on the thread
// that calls HandleMessage().
class ShareRTCSignalConnection {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // The signaling server assigned us an id (reply to IdentifySelf).
    virtual void OnIdentified(const std::string& id) {}
    virtual void OnOffer(const std::string& sender, const std::string& sdp) {}
    virtual void OnAnswer(const std::string& sender, const std::string& sdp) {}
    virtual void OnRemoteIceCandidate(const std::string& sender,
                                      const ICECandidateContent& candidate) {}
    virtual void OnDisconnect(const std::string& sender) {}
    virtual void OnTextMessage(const std::string& sender,
                               const std::string& text) {}
    virtual void OnDisconnectionNotification(const std::string& peer_id) {}
  };

  ShareRTCSignalConnection(std::function<int(const std::string&)> sendmsg,
                           Delegate* delegate);
  ~ShareRTCSignalConnection();

  // ----- Outgoing messages -----
  // Registers this connection with the signaling server and asks for an id.
  int IdentifySelf();
  int SendOffer(const std::string& to, const std::string& sdp);
  int SendAnswer(const std::string& to, const std::string& sdp);
  int SendIceCandidate(const std::string& to,
                       const ICECandidateContent& candidate);
  int SendDisconnect(const std::string& to);

  // ----- Incoming messages -----
  // Parses `msg` and invokes the corresponding Delegate method.
  // Returns false and fills `err` when the message is malformed.
  bool HandleMessage(const std::string& msg, std::string& err);

  // Id assigned by the signaling server, empty until OnIdentified.
  const std::string& id() const { return id_; }

 private:
  using MessageHandler =
      std::function<bool(const Message&, BaseContent*)>;

  bool HandleMessageGetAllPeerIDs(const Message& msg, BaseContent* content);
  bool HandleMessageTextMessage(const Message& msg, BaseContent* content);
  bool HandleMessageDisconnect(const Message& msg, BaseContent* content);
  bool HandleMessageOffer(const Message& msg, BaseContent* content);
  bool HandleMessageAnswer(const Message& msg, BaseContent* content);
  bool HandleMessageICECandidate(const Message& msg, BaseContent* content);
  bool HandleMessageIdentifySelf(const Message& msg, BaseContent* content);
  bool HandleMessageDisconnectionNotification(const Message& msg,
                                              BaseContent* content);

  int SendMessage(MessageType kind,
                  ReachType reach,
                  const std::string& peer_id,
                  const Json::Value& content);

  std::map<MessageType, MessageHandler> msgmap_;
  Delegate* delegate_;
  std::string id_;
  std::function<int(const std::string&)> sendmsg_fn_;
};
