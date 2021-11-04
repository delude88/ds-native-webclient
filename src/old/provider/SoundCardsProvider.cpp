//
// Created by Tobias Hegemann on 20.10.21.
//

#include <plog/Log.h>
#include "SoundCardsProvider.h"
#include "../utils/miniaudio.h"

std::vector<json> SoundCardsProvider::getSoundCards(const DigitalStage::Api::Store *store) {
  auto sound_cards = std::vector<nlohmann::json>();

  // Here we enumerate using miniaudio
  ma_result result;
  ma_context context;
  ma_device_info *pPlaybackDeviceInfos;
  ma_uint32 playbackDeviceCount;
  ma_device_info *pCaptureDeviceInfos;
  ma_uint32 captureDeviceCount;
  ma_uint32 iDevice;

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

nlohmann::json convert_device_to_sound_card(ma_device_info device_info,
                                            ma_context *context,
                                            const DigitalStage::Api::Store *store,
                                            const bool is_input
) {
  const std::string type = is_input ? "input" : "output";
  const auto driver = convert_backend_to_string(context->backend);
  // Maybe the sound card already exists and we might just update it here?
  const auto localDeviceId = store->getLocalDeviceId();
  const auto existing = localDeviceId ? store->getSoundCardByDeviceAndDriverAndTypeAndLabel(
      *store->getLocalDeviceId(),
      driver,
      type,
      device_info.name
  ) : std::nullopt;

  nlohmann::json sound_card;
  sound_card["uuid"] = convert_device_id(context->backend, device_info.id);
  sound_card["label"] = device_info.name;
  sound_card["audioDriver"] = driver;
  sound_card["type"] = type;
  sound_card["isDefault"] = device_info.isDefault == 1;

  // We have to initialize the device to get more details
  ma_device_config device_config;
  ma_device device;
  device_config = ma_device_config_init(is_input ? ma_device_type_capture : ma_device_type_playback);
  device_config.performanceProfile = ma_performance_profile_low_latency;
  if (is_input) {
    device_config.capture.pDeviceID = &device_info.id;
  } else {
    device_config.playback.pDeviceID = &device_info.id;
  }
  if (ma_device_init(nullptr, &device_config, &device) != MA_SUCCESS) {
    PLOGE << "Failed to initialize device.";
    return {nullptr};
  }

  sound_card["sampleRate"] = device.sampleRate;
  sound_card["sampleRates"] = get_sample_rates(device_info);
  sound_card["periodSize"] =
      is_input ? device.capture.internalPeriodSizeInFrames : device.playback.internalPeriodSizeInFrames;
  sound_card["numPeriods"] = is_input ? device.capture.internalPeriods : device.playback.internalPeriods;
  sound_card["online"] = true;

  std::vector<DigitalStage::Types::Channel> channels;
  const ma_uint32 numChannels = is_input ? device.capture.channels : device.playback.channels;
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