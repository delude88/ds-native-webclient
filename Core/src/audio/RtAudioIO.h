//
// Created by Tobias Hegemann on 06.11.21.
//
#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include <RtAudio.h>
#include "AudioIO.h"
#include <optional>

class [[maybe_unused]] RtAudioIO :
    public AudioIO {

 public:
  [[maybe_unused]] explicit RtAudioIO(std::shared_ptr<DigitalStage::Api::Client> client);
  ~RtAudioIO() override;
 protected:
  std::vector<json> enumerateDevices(const DigitalStage::Api::Store &store) override;

  void setAudioDriver(const std::string &audio_driver) override;
  void setInputSoundCard(const SoundCard &sound_card, bool start) override;
  void setOutputSoundCard(const SoundCard &sound_card, bool start) override;
  void startSending() override;
  void stopSending() override;
  void startReceiving() override;
  void stopReceiving() override;

 private:
  [[maybe_unused]] unsigned int getLowestBufferSize(std::optional<RtAudio::StreamParameters> inputParameters,
                                                    std::optional<RtAudio::StreamParameters> outputParameters,
                                                    unsigned int sample_rate);

  void initAudio();
  static std::vector<nlohmann::json> enumerateRtDevices(RtAudio::Api rt_api, const DigitalStage::Api::Store &store);
  static nlohmann::json getDevice(const std::string &id,
                                  const std::string &driver,
                                  const std::string &type,
                                  const RtAudio::DeviceInfo &info,
                                  const DigitalStage::Api::Store &store);

  std::atomic<bool> is_running_;

  RtAudio::StreamParameters input_parameters_;
  RtAudio::StreamParameters output_parameters_;
  unsigned int buffer_size_;

  std::unique_ptr<RtAudio> rt_audio_;
  std::atomic<std::size_t> num_output_channels_{};
  std::atomic<std::size_t> num_total_output_channels_{};
  std::array<bool, 64> output_channels_{};
};
