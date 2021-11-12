//
// Created by Tobias Hegemann on 26.10.21.
//

#pragma once

#include <DigitalStage/Api/Client.h>
#include "webrtc/ConnectionService.h"
#include "audio/AudioIO.h"
#include "audio/AudioMixer.h"
#include "audio/AudioRenderer.h"
#include "audio/RingBuffer.h"
#include <mutex>
#include <shared_mutex>
#include <memory>

#define RECEIVER_BUFFER 2000

class Client {
 public:
  explicit Client(DigitalStage::Api::Client &client, AudioIO &audio_io);
  ~Client();

 protected:
  void onCaptureCallback(const std::string &audio_track_id, const float *data, std::size_t frame_count);
  void onPlaybackCallback(float **data, std::size_t num_channels, std::size_t frame_count);
  void onDuplexCallback(const std::unordered_map<std::string, float *>&,
                        float **data,
                        std::size_t num_channels,
                        std::size_t frame_count);

 private:
  void attachHandlers(DigitalStage::Api::Client &client);
  void attachAudioHandlers(AudioIO &audio_io);

  void changeReceiverSize(unsigned int receiver_buffer);

  std::atomic<unsigned int> receiver_buffer_;
  std::map<std::string, std::shared_ptr<RingBuffer<float>>> channels_;
  std::shared_mutex channels_mutex_;
  AudioMixer<float> audio_mixer_;
  AudioRenderer<float> audio_renderer_;
  ConnectionService connection_service_;
};