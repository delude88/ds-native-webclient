//
// Created by Tobias Hegemann on 26.10.21.
//

#include <plog/Log.h>
#include "AudioService.h"
#include "../utils/miniaudio.h"

AudioService::AudioService(DigitalStage::Api::Client &client) :
    initialized_(false),
    output_channel_mapping_({false}),
    context_(),
    backend_(),
    input_device_(),
    output_device_() {
  PLOGD << "AudioService::AudioService";
  attachHandlers(client);
}
AudioService::~AudioService() {
  PLOGD << "AudioService::~AudioService";
  ma_device_uninit(&input_device_);
  ma_device_uninit(&output_device_);
  ma_context_uninit(&context_);
}
void AudioService::attachHandlers(DigitalStage::Api::Client &client) {
  PLOGD << "AudioService::attachHandlers";
  client.ready.connect([this, &client](const DigitalStage::Api::Store *store) {
    PLOGD << "AudioService::handle::ready";
    auto local_device = store->getLocalDevice();
    if (local_device) {
      // Now we can set the audio driver, input and output sound cards IF they are set already
      if (local_device->audioDriver) {
        assert(!local_device->audioDriver->empty());
        setAudioDriver(*local_device->audioDriver);
        if (local_device->inputSoundCardId) {
          auto sound_card = store->soundCards.get(*local_device->inputSoundCardId);
          assert(sound_card);
          setInputSoundCard(*sound_card, local_device->sendAudio, client);
        }
        if (local_device->outputSoundCardId) {
          auto sound_card = store->soundCards.get(*local_device->outputSoundCardId);
          assert(sound_card);
          setOutputSoundCard(*sound_card, local_device->receiveAudio);
        }
      }
    }
  });
  client.audioDriverSelected.connect([this, &client](std::optional<std::string> audio_driver,
                                                     const DigitalStage::Api::Store *store) {
    if (store->isReady() && audio_driver && !audio_driver->empty()) {
      auto local_device = store->getLocalDevice();
      assert(local_device);
      setAudioDriver(*audio_driver);
      // Also re-init the input and output devices
      auto input_sound_card = store->getInputSoundCard();
      if (input_sound_card) {
        setInputSoundCard(*input_sound_card, local_device->sendAudio, client);
      }
      auto output_sound_card = store->getInputSoundCard();
      if (output_sound_card) {
        setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
      }
    }
  });
  client.soundCardChanged.connect([this, &client](const std::string &_id, const nlohmann::json &update,
                                                  const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "AudioService::handle::soundCardChanged";
      auto local_device = store->getLocalDevice();
      assert(local_device);
      // Is the sound card the current input or output sound card?
      // Are the changes relevant, so that we should react here?
      auto input_sound_card = store->getInputSoundCard();
      if (input_sound_card && input_sound_card->_id == _id) {
        if (update.contains("channels") ||
            update.contains("sampleRate") ||
            update.contains("periodSize") ||
            update.contains("numPeriods")) {
          setInputSoundCard(*input_sound_card, local_device->sendAudio, client);
        }
      }
      auto output_sound_card = store->getOutputSoundCard();
      if (output_sound_card && output_sound_card->_id == _id) {
        if (update.contains("channels") ||
            update.contains("sampleRate") ||
            update.contains("periodSize") ||
            update.contains("numPeriods")) {
          setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
        }
      }
    }
  });
  client.deviceChanged.connect([&, this](const std::string &id, const nlohmann::json &update,
                                         const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "AudioService::handle::deviceChanged";
      // Did the local device changed? (so this device needs an update)
      auto local_device = store->getLocalDevice();
      if (local_device && local_device->_id == id) {
        if (update.contains("audioDriver") && !update["audioDriver"].empty()) {
          // This handles audioDriver INCLUDING inputSoundCardId, outputSoundCardId, sendAudio and receiveAudio
          setAudioDriver(update["audioDriver"]);
          auto input_sound_card = store->getInputSoundCard();
          if (input_sound_card) {
            setInputSoundCard(*input_sound_card, local_device->sendAudio, client);
          }
          auto output_sound_card = store->getOutputSoundCard();
          if (output_sound_card) {
            setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
          }
        } else if (update.contains("inputSoundCardId") || update.contains("outputSoundCardId")) {
          // This handles inputSoundCardId, outputSoundCardId INCLUDING sendAudio and receiveAudio
          // Did the input sound card change?
          if (update.contains("inputSoundCardId")) {
            setInputSoundCard(*store->soundCards.get(update["inputSoundCardId"]), local_device->sendAudio, client);
          }
          // Did the output sound card change?
          if (update.contains("outputSoundCardId")) {
            setOutputSoundCard(*store->soundCards.get(update["outputSoundCardId"]), local_device->receiveAudio);
          }
        } else {
          // This only handles sendAudio and receiveAudio
          if (update.contains("sendAudio")) {
            if (local_device->sendAudio) {
              startSending();
            } else {
              stopSending();
            }
          }
          if (update.contains("receiveAudio")) {
            if (local_device->receiveAudio) {
              startReceiving();
            } else {
              stopReceiving();
            }
          }
        }
      }
    }
  });
  client.audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        PLOGD << "Adding local track for channel " << *audio_track.sourceChannel;
        input_channel_audio_track_mapping_[*audio_track.sourceChannel] = audio_track._id;
      }
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        PLOGD << "Removing local track for channel " << *audio_track.sourceChannel;
        input_channel_audio_track_mapping_.erase(*audio_track.sourceChannel);
      }
    }
  });
}
void AudioService::setAudioDriver(const std::string &audio_driver) {
  PLOGD << "AudioService::setAudioDriver";
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

void AudioService::setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                                     bool start,
                                     DigitalStage::Api::Client &client) {
  if (initialized_) {
    PLOGD << "AudioService::setInputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&input_device_);

    // Map and publish input channels as audio tracks
    publishChannelsAsAudioTracks(client);

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
          auto context = static_cast<AudioService *>(pDevice->pUserData);
          const ma_uint32 channel_count = pDevice->capture.channels;
          for (ma_uint32 channel = 0; channel < channel_count; channel++) {
            if (context->input_channel_audio_track_mapping_.count(channel) != 0) {
              // Extract channel
              float channel_input[frame_count];
              for (ma_uint32 frame = 0; frame < frame_count; frame++) {
                channel_input[frame] = ((float *) pInput)[frame * channel_count + channel];
              }
              context->callOnWrite(context->input_channel_audio_track_mapping_[channel], channel_input, frame_count);
            }
          }
          (void) pOutput;
        };
    ma_result result;
    result = ma_device_init(nullptr, &input_device_config, &input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize capture device. Error code " + std::to_string(result));
    }
    if (start) {
      startSending();
    }
  }
}
void AudioService::setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) {
  if (initialized_) {
    PLOGD << "AudioService::setOutputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&output_device_);

    // Map channels
    output_channel_mapping_.fill(false);
    for (unsigned int i = 0; i < sound_card.channels.size(); i++) {
      output_channel_mapping_[i] = sound_card.channels[i].active;
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
    playback_callback = [this](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
      const ma_uint32 channel_count = pDevice->playback.channels;
      for (ma_uint32 channel = 0; channel < channel_count; channel++) {
        // We have to differentiate between the native sound card channel count AND the model channel count
        // Here we omit non enabled channels simply
        if (output_channel_mapping_[channel]) {
          float *channel_output;
          channel_output = new float[frame_count];
          callOnRead(channel, channel_output, frame_count, channel_count);
          for (ma_uint32 frame = 0; frame < frame_count; frame++) {
            ((float *) pOutput)[frame * channel_count + channel] = channel_output[frame];
          }
          delete[] channel_output;
        }
      }
      (void) pInput;
    };
    output_device_config.pUserData = &playback_callback;
    output_device_config.dataCallback =
        [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
          auto context =
              static_cast<std::function<void(ma_device *, void *, const void *, ma_uint32)>*>(pDevice->pUserData);
          (*context)(pDevice, pOutput, pInput, frame_count);
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
void AudioService::publishChannelsAsAudioTracks(DigitalStage::Api::Client &client) {
  PLOGD << "AudioService::publishChannelsAsAudioTracks";
  auto store = client.getStore();
  auto input_sound_card = store->getInputSoundCard();
  auto stageId = store->getStageId();
  if (store->isReady() && input_sound_card && stageId) {
    for (const auto &item: input_channel_audio_track_mapping_) {
      if (item.first >= input_sound_card->channels.size() || !input_sound_card->channels[item.first].active) {
        // Un-publish this track
        PLOGD << "AudioService::publishChannelsAsAudioTracks -> Un-publishing audio track " << item.second
              << " of channel " << item.first;
        client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item.second);
      }
    }
    // Request a new audio track for all channels if not existing yet
    for (auto i = 0; i < input_sound_card->channels.size(); i++) {
      if (input_sound_card->channels[i].active && input_channel_audio_track_mapping_.count(i) == 0) {
        PLOGD << "AudioService::publishChannelsAsAudioTracks -> Publishing channel " << i;
        nlohmann::json payload;
        payload["stageId"] = *stageId;
        payload["type"] = "native";
        payload["sourceChannel"] = i;
        PLOGD << "Publishing audio track";
        client.send(DigitalStage::Api::SendEvents::CREATE_AUDIO_TRACK,
                    payload);
      }
    }
  } else {
    // Un-publish all tracks
    for (const auto &item: input_channel_audio_track_mapping_) {
      PLOGD << "AudioService::publishChannelsAsAudioTracks -> Un-publishing all including audio track "
            << item.second
            << " of channel " << item.first;
      client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item.second);
    }
  }
}
void AudioService::callOnWrite(const std::string &audio_track_id, const float *data, size_t frame_size) {
  onWrite(audio_track_id, data, frame_size);
}
void AudioService::callOnRead(uint8_t channel, float *data, size_t frame_size, size_t num_channels) {
  onRead(channel, data, frame_size, num_channels);
}
void AudioService::startReceiving() {
  if (initialized_ && !ma_device_is_started(&output_device_)) {
    ma_result result = ma_device_start(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start playback device. Error code " + std::to_string(result));
    }
  }
}
void AudioService::stopReceiving() {
  if (initialized_ && ma_device_is_started(&output_device_)) {
    ma_result result = ma_device_stop(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop playback device. Error code " + std::to_string(result));
    }
  }
}
void AudioService::startSending() {
  if (initialized_ && !ma_device_is_started(&input_device_)) {
    ma_result result = ma_device_start(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start capture device. Error code " + std::to_string(result));
    }
  }
}
void AudioService::stopSending() {
  if (initialized_ && ma_device_is_started(&input_device_)) {
    ma_result result = ma_device_stop(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop capture device. Error code " + std::to_string(result));
    }
  }
}
