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
  explicit ConnectionService(std::shared_ptr<DigitalStage::Api::Client> client);
  ~ConnectionService();

  void broadcastBytes(const std::string &audio_track_id, const std::byte *data, size_t size);
  void broadcastFloats(const std::string &audio_track_id, const float *data, size_t size);

  void close(const std::string &audio_track_id);

  sigslot::signal<std::string, std::vector<std::byte>> onData;
 private:
  static bool IsSupported(const DigitalStage::Api::StageDevice& stage_device);
  void attachHandlers();
  void syncPeerConnections();
  void createPeerConnection(const std::string &stage_device_id,
                            const std::string &local_stage_device_id);
  void closePeerConnection(const std::string &stage_device_id);
  void fetchStatistics();

  std::shared_ptr<DigitalStage::Api::Client> client_;
  std::unordered_map<std::string, std::shared_ptr<PeerConnection>> peer_connections_;
  std::shared_mutex peer_connections_mutex_;

  rtc::Configuration configuration_;

  std::shared_ptr<DigitalStage::Api::Client::Token> token_;

  std::thread statistics_thread_;
  std::atomic<bool> is_fetching_statistics_;
};

#endif //CLIENT_SRC_WEBRTC_CONNECTIONSERVICE_H_
