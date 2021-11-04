//
// Created by Tobias Hegemann on 26.10.21.
//

#ifndef CLIENT_SRC_AUDIOSERVICE_H_
#define CLIENT_SRC_AUDIOSERVICE_H_

#include "miniaudio.h"
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Types.h>
#include <string>
#include <cstdint>
#include <map>
#include <array>

class AudioService {
 public:
  explicit AudioService(DigitalStage::Api::Client &client);
  virtual ~AudioService();

 protected:
  virtual void onWrite(const std::string &audio_track_id, const float *data, size_t frame_size) = 0;
  virtual void onRead(uint8_t channel, float *data, size_t frame_size, size_t num_channels) = 0;

 private:
  void callOnWrite(const std::string &audio_track_id, const float *data, size_t frame_size);
  void callOnRead(uint8_t channel, float *data, size_t frame_size, size_t num_channels);
  void attachHandlers(DigitalStage::Api::Client &client);
  void setAudioDriver(const std::string &audio_driver);
  void setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                         bool start,
                         DigitalStage::Api::Client &client);
  void publishChannelsAsAudioTracks(DigitalStage::Api::Client &client);
  void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start);
  void startSending();
  void stopSending();
  void startReceiving();
  void stopReceiving();

  std::function<void(ma_device *, void *, const void *, ma_uint32)> capture_callback;
  std::function<void(ma_device *, void *, const void *, ma_uint32)> playback_callback;

  bool initialized_;
  ma_backend backend_;
  ma_context context_;
  ma_device input_device_;
  ma_device output_device_;
  std::map<uint8_t, std::string> input_channel_audio_track_mapping_;
  std::array<bool, 32> output_channel_mapping_;
};

#endif //CLIENT_SRC_AUDIOSERVICE_H_
