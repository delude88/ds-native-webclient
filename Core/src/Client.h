//
// Created by Tobias Hegemann on 26.10.21.
//

#pragma once

//#define USE_CIRCULAR_QUEUE

#include <memory>                 // for shared_ptr, unique_ptr
#include <cstddef>                // for size_t
#include <atomic>                 // for atomic
#include <iosfwd>                 // for string
#include <map>                    // for map
#include <shared_mutex>           // for shared_mutex
#include <unordered_map>          // for unordered_map
#include <DigitalStage/Api/Client.h>
#include "audio/AudioIO.h"
#include "audio/AudioRenderer.h"
#include "webrtc/ConnectionService.h"
#ifdef USE_CIRCULAR_QUEUE
#include "utils/CircularQueue.h"
#else
#include "utils/RingBuffer.h"
template<class T>
class RingBuffer;
#endif

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

  std::atomic<bool> is_ready_;
  std::shared_ptr<DigitalStage::Api::Client> api_client_;
  std::unique_ptr<AudioIO> audio_io_;
  std::unique_ptr<AudioRenderer<float>> audio_renderer_;
  std::shared_ptr<ConnectionService> connection_service_;
};