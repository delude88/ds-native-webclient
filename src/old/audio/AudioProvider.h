//
// Created by Tobias Hegemann on 24.10.21.
//

#ifndef CLIENT_SRC_PROVIDER_AUDIOPROVIDER_H_
#define CLIENT_SRC_PROVIDER_AUDIOPROVIDER_H_

#include <miniaudio.h>
#include <string>
#include "AudioMixer.h"
#include <thread>
#include <DigitalStage/Types.h>
#include <DigitalStage/Api/Client.h>
#include <sigslot/signal.hpp>

#define FORMAT ma_format_f32
#define SAMPLE_RATE 48000

class AudioProvider {
 public:
  AudioProvider();

  ~AudioProvider();

  void attachHandlers(DigitalStage::Api::Client &client);

  void setAudioDriver(const std::string &audio_driver);

  void setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card);

  void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card);

  AudioMixer& getMixer();

 private:
  void initInputSoundCard();
  void initOutputSoundCard();


  std::unique_ptr<AudioMixer> mixer_;

  bool initialized_;
  ma_backend backend_;
  ma_context context_;
  std::optional<DigitalStage::Types::SoundCard> input_sound_card_;
  ma_device input_device_;
  std::optional<DigitalStage::Types::SoundCard> output_sound_card_;
  ma_device output_device_;
};

#endif //CLIENT_SRC_PROVIDER_AUDIOPROVIDER_H_
