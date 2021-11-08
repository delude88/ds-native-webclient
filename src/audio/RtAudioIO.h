//
// Created by Tobias Hegemann on 06.11.21.
//
#pragma once

#include <RtAudio.h>
#include "AudioIO.h"
#include <optional>

#define COPY_BUFFERS false

class RtAudioIO :
    public AudioIO {

 public:
  explicit RtAudioIO(DigitalStage::Api::Client &client);
 protected:
  std::vector<json> enumerateDevices(const DigitalStage::Api::Store &store) override;

  void setAudioDriver(const std::string &audio_driver) override;
  void setInputSoundCard(const SoundCard &sound_card, bool start, DigitalStage::Api::Client &client) override;
  void setOutputSoundCard(const SoundCard &sound_card, bool start) override;
  void startSending() override;
  void stopSending() override;
  void startReceiving() override;
  void stopReceiving() override;

 private:
  unsigned int getLowestBufferSize(std::optional<RtAudio::StreamParameters> inputParameters, std::optional<RtAudio::StreamParameters> outputParameters, unsigned int sample_rate);

  void initAudio( DigitalStage::Api::Client &client);
  static std::vector<nlohmann::json> enumerateRtDevices(RtAudio::Api rt_api, const DigitalStage::Api::Store &store);
  static nlohmann::json getDevice(const std::string &id,
                                  const std::string &driver,
                                  const std::string &type,
                                  const RtAudio::DeviceInfo &info,
                                  const DigitalStage::Api::Store &store);

  DigitalStage::Api::Client& client_;
  std::unique_ptr<RtAudio> rt_audio_;
  std::atomic<unsigned int> num_output_channels_{};
  std::atomic<unsigned int> num_total_output_channels_{};
  std::array<bool, 64> output_channels_{};
};