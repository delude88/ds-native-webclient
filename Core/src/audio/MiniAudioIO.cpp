//
// Created by Tobias Hegemann on 03.11.21.
//

#include "MiniAudioIO.h"
#include "../utils/miniaudio.h"
#include <plog/Log.h>

MiniAudioIO::MiniAudioIO(std::shared_ptr<DigitalStage::Api::Client> client) : AudioIO(std::move(client)) {
  PLOGD << "MiniAudioIO::MiniAudioIO";
}

MiniAudioIO::~MiniAudioIO() {
  PLOGD << "MiniAudioIO::~MiniAudioIO";
  ma_device_uninit(&input_device_);
  ma_device_uninit(&output_device_);
  ma_context_uninit(&context_);
}

void MiniAudioIO::setAudioDriver(const std::string &audio_driver) {
  PLOGD << "AudioIO::setAudioDriver";
  initialized_ = false;
  ma_device_uninit(&input_device_);
  ma_device_uninit(&output_device_);
  ma_context_uninit(&context_);
  backend_ = convert_string_to_backend(audio_driver);
  ma_context_config config = ma_context_config_init();
  config.threadPriority = ma_thread_priority_realtime;
  if (ma_context_init(&backend_, 1, &config, &context_) != MA_SUCCESS) {
    throw std::runtime_error("Could not initialize audio context");
  }
  initialized_ = true;
}
void MiniAudioIO::setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                                    bool start) {
  if (initialized_) {
    PLOGD << "AudioService::setInputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&input_device_);

    unPublishAll();
    for (int channel = 0; channel < sound_card.channels.size(); channel++) {
      if (sound_card.channels[channel].active) {
        publishChannel(channel);
      }
    }

    // And init output device
    assert(!sound_card.uuid.empty());
    assert(sound_card.sampleRate != 0);
    assert(sound_card.periodSize != 0);
    assert(sound_card.numPeriods != 0);
    auto uuid = std::make_unique<ma_device_id>(convert_device_id(backend_, sound_card.uuid));
    ma_device_config input_device_config = ma_device_config_init(ma_device_type_capture);
    input_device_config.capture.pDeviceID = uuid.get();
    input_device_config.capture.format = ma_format_f32;
    //input_device_config.capture.shareMode = ma_share_mode_exclusive;
    input_device_config.capture.channels = 0;
    input_device_config.performanceProfile = ma_performance_profile_low_latency;
    input_device_config.sampleRate = sound_card.sampleRate;
    input_device_config.periodSizeInFrames = sound_card.periodSize;
    input_device_config.periods = sound_card.numPeriods;
    input_device_config.pUserData = this;
    input_device_config.dataCallback =
        [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
          auto context = static_cast<MiniAudioIO *>(pDevice->pUserData);
          const ma_uint32 channel_count = pDevice->capture.channels;
          for (const auto &item: context->input_channel_mapping_) {
            float buf[frame_count];
            for (int frame = 0; frame < frame_count; frame++) {
              buf[frame] = ((float *) pInput)[frame * channel_count + item.first];
            }
            context->onCapture(item.second, (float *) buf, frame_count);
            //context->onCaptureCallback(item.second, buf, frame_count);
          }
          (void) pOutput;
        };
    ma_result result = ma_device_init(nullptr, &input_device_config, &input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize capture device. Error code " + std::to_string(result));
    }
    if (start) {
      startSending();
    }
  }
}

void MiniAudioIO::setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) {
  if (initialized_) {
    PLOGD << "AudioIO::setOutputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&output_device_);

    // Map channels (enable/disable)
    output_channels_.fill(false);
    num_output_channels_ = 0;
    for (auto i = 0; i < sound_card.channels.size(); i++) {
      if (sound_card.channels.at(i).active) {
        output_channels_[i] = true;
        num_output_channels_++;
      }
    }

    // And init output device
    assert(!sound_card.uuid.empty());
    assert(sound_card.sampleRate != 0);
    assert(sound_card.periodSize != 0);
    assert(sound_card.numPeriods != 0);
    auto uuid = std::make_unique<ma_device_id>(convert_device_id(backend_, sound_card.uuid));
    ma_device_config output_device_config = ma_device_config_init(ma_device_type_playback);
    output_device_config.playback.pDeviceID = uuid.get();
    //output_device_config.playback.shareMode = ma_share_mode_exclusive;
    output_device_config.playback.format = ma_format_f32;
    output_device_config.playback.channels = 0; // Use all channels
    output_device_config.sampleRate = sound_card.sampleRate;
    output_device_config.periodSizeInFrames = sound_card.periodSize;
    output_device_config.periods = sound_card.numPeriods;
    output_device_config.performanceProfile = ma_performance_profile_low_latency;
    output_device_config.pUserData = this;
    output_device_config.dataCallback =
        [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
          // We know the output channel mapping, so only read from the enabled channels
          auto context = static_cast<MiniAudioIO *>(pDevice->pUserData);
          const ma_uint32 channel_count = pDevice->playback.channels;

          auto **buff = (float **) malloc(context->num_output_channels_ * sizeof(float *));
          for (int output_channel = 0; output_channel < context->num_output_channels_; output_channel++) {
            buff[output_channel] = (float *) malloc(frame_count * sizeof(float));
          }
          context->onPlayback(buff, context->num_output_channels_, frame_count);
          //context->onPlaybackCallback(buff, context->num_output_channels_, frame_count);

          unsigned int relative_channel = 0;
          for (ma_uint32 channel = 0; channel < channel_count; channel++) {
            if (context->output_channels_[channel]) {
              for (int frame = 0; frame < frame_count; frame++) {
                ((float *) pOutput)[frame * channel_count + channel] = buff[relative_channel][frame];
              }
              relative_channel++;
            }
          }
          free(buff);
          (void) pInput;
        };
    ma_result result = ma_device_init(&context_, &output_device_config, &output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize playback device. Error code " + std::to_string(result));
    }
    if (start) {
      startReceiving();
    }
  }
}
void MiniAudioIO::startSending() {
  if (initialized_ && !ma_device_is_started(&input_device_)) {
    PLOGD << "AudioIO::startSending";
    ma_result result = ma_device_start(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start capture device. Error code " + std::to_string(result));
    }
  }
}
void MiniAudioIO::stopSending() {
  if (initialized_ && ma_device_is_started(&input_device_)) {
    PLOGD << "AudioIO::stopSending";
    ma_result result = ma_device_stop(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop capture device. Error code " + std::to_string(result));
    }
  }
}
void MiniAudioIO::startReceiving() {
  if (initialized_ && !ma_device_is_started(&output_device_)) {
    PLOGD << "AudioIO::startReceiving";
    ma_result result = ma_device_start(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start playback device. Error code " + std::to_string(result));
    }
  }
}
void MiniAudioIO::stopReceiving() {
  if (initialized_ && ma_device_is_started(&output_device_)) {
    PLOGD << "AudioIO::stopReceiving";
    ma_result result = ma_device_stop(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop playback device. Error code " + std::to_string(result));
    }
  }
}
std::vector<nlohmann::json> MiniAudioIO::enumerateDevices(const DigitalStage::Api::Store &store) {
  auto sound_cards = std::vector<nlohmann::json>();

  // Here we enumerate using miniaudio
  ma_result result;
  ma_context context;
  ma_device_info *p_playback_device_infos;
  ma_uint32 playback_device_count;
  ma_device_info *p_capture_device_infos;
  ma_uint32 capture_device_count;
  ma_uint32 i_device;

  if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
    PLOGE << "Failed to initialize context.";
    return sound_cards;
  }

  result = ma_context_get_devices(&context,
                                  &pPlaybackDeviceInfos,
                                  &playbackDeviceCount,
                                  &pCaptureDeviceInfos,
                                  &captureDeviceCount);
  if (result != MA_SUCCESS) {
    PLOGE << "Failed to retrieve device information.";
    ma_context_uninit(&context);
    return sound_cards;
  }

  // Playback devices
  for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
    if (ma_context_get_device_info(&context,
                                   ma_device_type_playback,
                                   &pPlaybackDeviceInfos[iDevice].id,
                                   ma_share_mode_shared,
                                   &pPlaybackDeviceInfos[iDevice]) == MA_SUCCESS) {
      auto sound_card = convert_device_to_sound_card(pPlaybackDeviceInfos[iDevice],
                                                     &context,
                                                     store,
                                                     false);
      if (!sound_card.is_null()) {
        sound_cards.push_back(sound_card);
      } else {
        //TODO: Use default sound card
      }
    } else {
      PLOGE << "Failed to retrieve detailed device information.";
      ma_context_uninit(&context);
      return sound_cards;
    }
  }

  // Capture devices
  for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {

    if (ma_context_get_device_info(&context,
                                   ma_device_type_capture,
                                   &pCaptureDeviceInfos[iDevice].id,
                                   ma_share_mode_shared,
                                   &pCaptureDeviceInfos[iDevice]) == MA_SUCCESS) {
      auto sound_card = convert_device_to_sound_card(pCaptureDeviceInfos[iDevice],
                                                     &context,
                                                     store,
                                                     true);
      if (!sound_card.is_null())
        sound_cards.push_back(sound_card);
    } else {
      PLOGE << "Failed to retrieve detailed device information.";
      ma_context_uninit(&context);
      return sound_cards;
    }
  }

  ma_context_uninit(&context);

  return sound_cards;
}

nlohmann::json convert_device_to_sound_card(ma_device_info  /*device_info*/,
                                            ma_context * /*context*/,
                                            const DigitalStage::Api::Store &store,
                                            const bool is_input
) {
  const std::string type = is_input ? "input" : "output";
  const auto driver = convert_backend_to_string(context->backend);
  // Maybe the sound card already exists and we might just update it here?
  const auto local_device_id = store.getLocalDeviceId();
  const auto existing = localDeviceId ? store.getSoundCardByDeviceAndDriverAndTypeAndLabel(
      *store.getLocalDeviceId(),
      driver,
      type,
      device_info.name
  ) : std::nullopt;

  nlohmann::json sound_card {
      {"uuid", convert_device_id(context->backend, device_info.id)},
      {"label", device_info.name},
      {"audioEngine", "miniaudio"},
      {"audioDriver", driver},
      {"type", type},
      {"isDefault", device_info.isDefault == 1},
  };

  // We have to initialize the device to get more details
  ma_device_config device_config;
  ma_device device;
  device_config = ma_device_config_init(is_input ? ma_device_type_capture : ma_device_type_playback);
  device_config.performanceProfile = ma_performance_profile_low_latency;
  if (is_input) { // NOLINT(bugprone-branch-clone)
    device_config.capture.pDeviceID = &device_info.id;
  } else {
    device_config.playback.pDeviceID = &device_info.id;
  }
  if (ma_device_init(nullptr, &device_config, &device) != MA_SUCCESS) {
    PLOGE << "Failed to initialize device.";
    return {nullptr};
  }

  if (!existing) {
    sound_card["frameSize"] = 256;
    sound_card["sampleRate"] = device.sampleRate;
    sound_card["periodSize"] =
        is_input ? device.capture.internalPeriodSizeInFrames : device.playback.internalPeriodSizeInFrames;
    sound_card["numPeriods"] = is_input ? device.capture.internalPeriods : device.playback.internalPeriods;
  }
  sound_card["sampleRates"] = get_sample_rates(device_info);
  sound_card["online"] = true;

  std::vector<DigitalStage::Types::Channel> channels;
  const ma_uint32 num_channels = is_input ? device.capture.channels : device.playback.channels;
  for (int i = 0; i < numChannels; i++) {
    std::string label = "Kanal " + std::to_string(i + 1);
    if (existing && existing->channels.size() >= i) {
      channels.push_back(existing->channels[i]);
    } else {
      DigitalStage::Types::Channel channel;
      channel.label = label;
      channel.active = false;
      channels.push_back(channel);
    }
  }
  sound_card["channels"] = channels;

  ma_device_uninit(&device);

  return sound_card;
}