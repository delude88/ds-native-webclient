//
// Created by Tobias Hegemann on 30.10.21.
//

#ifndef CLIENT_SRC_NEW_AUDIOMIXER_H_
#define CLIENT_SRC_NEW_AUDIOMIXER_H_

#include "AudioIO.h"
#include "../webrtc/ConnectionService.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#define SEND_FRAME_SIZE 400
#define RECEIVE_FRAME_SIZE 2000

struct ChannelTrackMap {
  bool active;
  std::string audio_track_id;
};

class AudioMixer {
 public:
  AudioMixer(AudioIO &audio_io, ConnectionService &connection_service);

  void start();
  void stop();

 private:
  void attachHandlers(DigitalStage::Api::Client &client);

  // DISTRIBUTION
  /**
   * Distribute audio tracks published inside AudioIO to ConnectionService
   * and to the internal channels_ list.
   */
  void distribute();
  // atomic abstraction of channels
  std::array<ChannelTrackMap, 32> output_channel_mapping_;
  std::shared_mutex distribution_mutex_;
  bool distributing_;
  std::thread distribution_thread_;

  // MIXING
  ChannelMap input_channel_mapping_;
  void mix();
  bool mixing_;
  std::thread mixing_thread_;

  AudioIO &local_io_;
  ConnectionService &remote_io_;
};

#endif //CLIENT_SRC_NEW_AUDIOMIXER_H_
