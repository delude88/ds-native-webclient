//
// Created by Tobias Hegemann on 26.10.21.
//

#ifndef CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
#define CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_

#include "rtc/rtc.hpp"
#include "PeerConnection.h"
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Api/Store.h>
#include <DigitalStage/Types.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <plog/Log.h>
#include <sigslot/signal.hpp>
#include <mutex>
#include <shared_mutex>

class ConnectionService {
 public:
  explicit ConnectionService(DigitalStage::Api::Client &client);
  ~ConnectionService();

  void broadcast(const std::string &audio_track_id, float data);
  void broadcast(const std::string &audio_track_id, const std::byte *data, size_t size);
  void broadcast(const std::string &audio_track_id, const float *data, size_t size);

  Pal::sigslot::signal<std::string, std::vector<std::byte>> onData;
 private:
  void attachHandlers(DigitalStage::Api::Client &client);
  void onStageChanged(DigitalStage::Api::Client &client);
  void createPeerConnection(const std::string &stage_device_id,
                            const std::string &local_stage_device_id,
                            DigitalStage::Api::Client &client);
  void closePeerConnection(const std::string &stage_device_id);

  std::unordered_map<std::string, std::shared_ptr<PeerConnection>> peer_connections_;
  std::shared_mutex peer_connections_mutex_;

  rtc::Configuration configuration_;
};

#endif //CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
