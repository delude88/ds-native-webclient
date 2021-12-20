//
// Created by Tobias Hegemann on 26.10.21.
//

#ifndef CLIENT_SRC_WEBRTC_PEERCONNECTION_H_
#define CLIENT_SRC_WEBRTC_PEERCONNECTION_H_

#include <functional>               // for function
#include <memory>                   // for shared_ptr, unique_ptr
#include <mutex>                    // for mutex
#include <cstddef>                  // for byte, size_t
#include <iosfwd>                   // for string
#include <map>                      // for map
#include <string>                   // for operator<
#include <vector>                   // for vector
#include <DigitalStage/Types.h>
#include "rtc/peerconnection.hpp"   // for PeerConnection

class PeerConnection {
 public:
  PeerConnection(const rtc::Configuration &configuration, bool polite);

  //void makeOffer();

  void send(const std::string &audio_track_id, const std::byte *data, size_t size);

  void addRemoteIceCandidate(const DigitalStage::Types::IceCandidateInit &ice_candidate_init);
  void setRemoteSessionDescription(const DigitalStage::Types::SessionDescriptionInit &session_description_init);

  std::function<void(const DigitalStage::Types::IceCandidateInit &)> onLocalIceCandidate;
  std::function<void(const DigitalStage::Types::SessionDescriptionInit &)> onLocalSessionDescription;
  std::function<void(std::string, std::vector<std::byte>)> onData;
 private:
  void handleLocalSessionDescription(const rtc::Description &description);

  std::unique_ptr<rtc::PeerConnection> peer_connection_;
  std::map<std::string, std::shared_ptr<rtc::DataChannel>> senders_;
  std::mutex senders_mutex_;
  std::map<std::string, std::shared_ptr<rtc::DataChannel>> receivers_;
  std::mutex receivers_mutex_;

  bool polite_;
  bool making_offer_;
  bool ignore_offer_;
  bool srd_answer_pending_;
};

#endif //CLIENT_SRC_WEBRTC_PEERCONNECTION_H_
