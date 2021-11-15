//
// Created by Tobias Hegemann on 26.10.21.
//

#pragma once

//#define USE_CIRCULAR_QUEUE

#include <DigitalStage/Api/Client.h>
#include "webrtc/ConnectionService.h"
#include "audio/AudioIO.h"
#include "audio/AudioMixer.h"
#include "audio/AudioRenderer.h"
#ifdef USE_CIRCULAR_QUEUE
#include "utils/CircularQueue.h"
#else
#include "utils/RingBuffer.h"
#endif
#include <mutex>
#include <shared_mutex>
#include <memory>

#define RECEIVER_BUFFER 2000

class Client {
 public:
  explicit Client(std::shared_ptr<DigitalStage::Api::Client> api_client);
  ~Client();

 protected:
  void onCaptureCallback(const std::string &audio_track_id, const float *data, std::size_t frame_count);
  void onPlaybackCallback(float **data, std::size_t num_channels, std::size_t frame_count);
  void onDuplexCallback(const std::unordered_map<std::string, float *> &,
                        float **data,
                        std::size_t num_channels,
                        std::size_t frame_count);

 private:
  void attachHandlers();
  void attachAudioHandlers();

  void changeReceiverSize(unsigned int receiver_buffer);

  std::atomic<unsigned int> receiver_buffer_;
#ifdef USE_CIRCULAR_QUEUE
  std::map<std::string, std::shared_ptr<CircularQueue<float>>> channels_;
#else
  std::map<std::string, std::shared_ptr<RingBuffer<float>>> channels_;
#endif
  std::shared_mutex channels_mutex_;

  std::shared_ptr<DigitalStage::Api::Client> api_client_;
  std::unique_ptr<AudioIO> audio_io_;
  std::unique_ptr<AudioRenderer<float>> audio_renderer_;
  std::shared_ptr<ConnectionService> connection_service_;
};