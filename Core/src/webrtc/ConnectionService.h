//
// Created by Tobias Hegemann on 26.10.21.
//

#ifndef CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
#define CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
#include <DigitalStage/Api/Client.h>  // for Client
#include <memory>                     // for shared_ptr
#include <cstddef>                    // for byte, size_t
#include <iosfwd>                     // for string
#include <shared_mutex>               // for shared_mutex
#include <sigslot/signal.hpp>         // for signal
#include <string>                     // for operator==, hash
#include <unordered_map>              // for unordered_map
#include <vector>                     // for vector
#include "rtc/configuration.hpp"      // for Configuration
class PeerConnection;

class ConnectionService {
 public:
  explicit ConnectionService(std::shared_ptr<DigitalStage::Api::Client> client);
  ~ConnectionService();

  [[maybe_unused]] void broadcastFloat(const std::string &audio_track_id, float data);
  void broadcastBytes(const std::string &audio_track_id, const std::byte *data, size_t size);
  void broadcastFloats(const std::string &audio_track_id, const float *data, size_t size);

  sigslot::signal<std::string, std::vector<std::byte>> onData;
 private:
  void attachHandlers();
  void onStageChanged();
  void createPeerConnection(const std::string &stage_device_id,
                            const std::string &local_stage_device_id);
  void closePeerConnection(const std::string &stage_device_id);

  std::shared_ptr<DigitalStage::Api::Client> client_;
  std::unordered_map<std::string, std::shared_ptr<PeerConnection>> peer_connections_;
  std::shared_mutex peer_connections_mutex_;

  rtc::Configuration configuration_;

  std::shared_ptr<DigitalStage::Api::Client::Token> token_;

  bool running_;
};

#endif //CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
