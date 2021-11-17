//
// Created by Tobias Hegemann on 03.11.21.
//
#pragma once

#include "miniaudio.h"

#include "AudioIO.h"
#include <miniaudio.h>

class MiniAudioIO : public AudioIO {
 public:
  explicit MiniAudioIO(std::shared_ptr<DigitalStage::Api::Client> client);
  ~MiniAudioIO();
 protected:
  std::vector<json> enumerateDevices(const DigitalStage::Api::Store &store) override;

  void setAudioDriver(const std::string &audio_driver) override;
  void setInputSoundCard(const SoundCard &sound_card, bool start) override;
  void setOutputSoundCard(const SoundCard &sound_card, bool start) override;
  void startSending() override;
  void stopSending() override;
  void startReceiving() override;
  void stopReceiving() override;
  void shutdown() override;

 private:
  std::atomic<bool> initialized_{};
  std::atomic<unsigned int> num_output_channels_{};
  std::array<bool, 64> output_channels_{};
  ma_backend backend_;
  ma_context context_{};
  ma_device input_device_{};
  ma_device output_device_{};
};

nlohmann::json convert_device_to_sound_card(ma_device_info,
                                            ma_context *,
                                            const DigitalStage::Api::Store &,
                                            bool is_input
);