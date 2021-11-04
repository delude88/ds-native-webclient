//
// Created by Tobias Hegemann on 30.10.21.
//

#ifndef CLIENT_SRC_NEW_AUDIOIO_H_
#define CLIENT_SRC_NEW_AUDIOIO_H_

#include <array>
#include <string>
#include <optional>
#include <utility>
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Types.h>
#include <plog/Log.h>
#include <miniaudio.h>
#include <mutex>
#include <shared_mutex>
#include <delude88/ringbuffer.h>
#include "IAudioIO.h"

class AudioIO : public IAudioIO {
 public:
  struct InputChannel {
    std::string audio_track_id;
    std::unique_ptr<RingBuffer> buffer;
  };

  explicit AudioIO(DigitalStage::Api::Client &client);
  ~AudioIO();

  const ChannelMap &getChannelMapping() const override;

  void read(std::size_t channel, float *buff, std::size_t size) override;

  void writeLeft(const float *buf, size_t size) override;
  void writeRight(const float *buf, size_t size) override;

 private:
  void attachHandlers(DigitalStage::Api::Client &client);
  void setAudioDriver(const std::string &audio_driver);
  void setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                         bool start,
                         DigitalStage::Api::Client &client);

  void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start);
  void startSending();
  void stopSending();
  void startReceiving();
  void stopReceiving();
  void publishChannelsAsTracks(DigitalStage::Api::Client &client);

  bool initialized_;
  ma_backend backend_;
  ma_context context_;
  ma_device input_device_;
  ma_device output_device_;
  std::atomic_uint input_buffer_;



  std::array<std::optional<InputChannel>, 32> input_channels_;

  std::unique_ptr<RingBuffer> left_output_channel_;
  std::unique_ptr<RingBuffer> right_output_channel_;

  std::array<bool, 32> output_channel_mapping_;
};

#endif //CLIENT_SRC_NEW_AUDIOIO_H_
