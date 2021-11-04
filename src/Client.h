//
// Created by Tobias Hegemann on 26.10.21.
//

#pragma once

#include <DigitalStage/Api/Client.h>
#include "webrtc/ConnectionService.h"
#include "audio/MiniAudioIO.h"
#include "audio/AudioMixer.h"
#include "audio/AudioRenderer.h"
#include "utils/ringbuffer.h"
#include <mutex>
#include <shared_mutex>
#include <memory>

class Client : public MiniAudioIO {
 public:
  explicit Client(DigitalStage::Api::Client &client);
  ~Client();

 protected:
  void onChannelCallback(const std::string &audio_track_id, const float *data, std::size_t frame_count) override;
  void onPlaybackCallback(float **data, std::size_t num_channels, std::size_t frame_count) override;

 private:
  std::atomic<unsigned int> output_buffer_;
  std::map<std::string, std::shared_ptr<RingBuffer<float>>> channels_;
  std::shared_mutex channels_mutex_;
  AudioMixer<float> audio_mixer_;
  AudioRenderer<float> audio_renderer_;
  ConnectionService connection_service_;
};