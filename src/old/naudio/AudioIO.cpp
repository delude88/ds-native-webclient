//
// Created by Tobias Hegemann on 30.10.21.
//

#include "AudioIO.h"

#include <memory>
#include "../utils/miniaudio.h"
#include "../audio/AudioIO.h"

AudioIO::AudioIO(DigitalStage::Api::Client &client) :
    initialized_(false),
    context_(),
    backend_(),
    input_device_(),
    output_device_() {
  PLOGD << "AudioService::AudioService";
  attachHandlers(client);
}
AudioIO::~AudioIO() {
  PLOGD << "AudioService::~AudioService";
  ma_device_uninit(&input_device_);
  ma_device_uninit(&output_device_);
  ma_context_uninit(&context_);
}
void AudioIO::attachHandlers(DigitalStage::Api::Client &client) {
  client.ready.connect([this, &client](const DigitalStage::Api::Store *store) {
    PLOGD << "AudioIO::handle::ready";
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
      PLOGD << "AudioIO::handle::audioDriverSelected";
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
      PLOGD << "AudioIO::handle::soundCardChanged";
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
      PLOGD << "AudioIO::handle::deviceChanged";
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
          if (update.contains("sendAudio") && local_device->inputSoundCardId) {
            if (local_device->sendAudio) {
              startSending();
            } else {
              stopSending();
            }
          }
          if (update.contains("receiveAudio") && local_device->outputSoundCardId) {
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
      PLOGD << "AudioIO::handle::audioTrackAdded";
      auto localDeviceId = store->getLocalDeviceId();
      auto inputSoundCard = store->getInputSoundCard();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        PLOGD << "Adding local track " << audio_track._id << " for channel " << *audio_track.sourceChannel;
        input_channels_[*audio_track.sourceChannel] =
            {audio_track._id, std::make_unique<NonBlockingRingBuffer>(input_buffer_)};
        PLOGD << "Added local track for channel " << *audio_track.sourceChannel;
        onChannelMappingChanged(getChannelMapping());
      }
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "AudioIO::handle::audioTrackRemoved";
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        //std::unique_lock lock(channels_mapping_mutex_);
        input_channels_[*audio_track.sourceChannel] = std::nullopt;
        PLOGD << "Removed local track for channel " << *audio_track.sourceChannel;
        onChannelMappingChanged(getChannelMapping());
      }
    }
  });
}
void AudioIO::setAudioDriver(const std::string &audio_driver) {
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
void AudioIO::setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                                bool start,
                                DigitalStage::Api::Client &client) {
  if (initialized_) {
    PLOGD << "AudioService::setInputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&input_device_);

    // Empty all input ring buffers and set buffer size
    //TODO: Replace with calculation
    input_buffer_ = sound_card.inputBuffer * 5000;
    publishChannelsAsTracks(client);

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
          auto context = static_cast<AudioIO *>(pDevice->pUserData);
          const ma_uint32 channel_count = pDevice->capture.channels;
          std::cout << channel_count << " and " << sizeof(pInput) << std::endl;
          //float channels[channel_count][frame_count];

          // Fastest way to get all channels?
          for(int frame = 0; frame < frame_count; frame++) {

          }

          const float* pSrcF32 = (const float*)pInterleavedPCMFrames;
          ma_uint64 iPCMFrame;
          for (iPCMFrame = 0; iPCMFrame < frameCount; ++iPCMFrame) {
            ma_uint32 iChannel;
            for (iChannel = 0; iChannel < channels; ++iChannel) {
              float* pDstF32 = (float*)ppDeinterleavedPCMFrames[iChannel];
              pDstF32[iPCMFrame] = pSrcF32[iPCMFrame*channels+iChannel];
            }
          }

          for (ma_uint32 channel = 0; channel < channel_count; channel++) {
            if (context->input_channels_[channel]) {
              // Miniaudio only supports INTERLEAVED streams
              for (int frame = 0; frame < frame_count; frame++) {
                context->input_channels_[channel]->buffer->write(((float *) pInput)[frame + channel_count * channel]);
              }
            }
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

void AudioIO::setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) {
  if (initialized_) {
    PLOGD << "AudioIO::setOutputSoundCard";
    // Un-init existing output device
    ma_device_uninit(&output_device_);

    // Map channels (enable/disable)
    output_channel_mapping_.fill(false);
    auto num_channels = 0;
    for (auto i = 0; i < sound_card.channels.size(); i++) {
      if (sound_card.channels.at(i).active) {
        PLOGD << "HAVE ACTIVE CHANNEL!";
        output_channel_mapping_[i] = true;
        num_channels++;
      }
    }
    bool stereo_playback = num_channels % 2 == 0;

    // Empty all output ring buffers
    //TODO: Replace with calculation in ms
    auto bufferSize = sound_card.outputBuffer * 10000;
    left_output_channel_ = std::make_unique<NonBlockingRingBuffer>(bufferSize);
    right_output_channel_ = std::make_unique<NonBlockingRingBuffer>(bufferSize);

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
    if (stereo_playback) {
      std::cout << "STEREO" << std::endl;
      output_device_config.dataCallback =
          [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
            // We know the output channel mapping, so only read from the enabled channels
            bool left_channel = true;
            auto context = static_cast<AudioIO *>(pDevice->pUserData);
            const ma_uint32 channel_count = pDevice->playback.channels;

            float left[frame_count];
            context->left_output_channel_->get(left, frame_count);
            float right[frame_count];
            context->left_output_channel_->get(right, frame_count);

            for (ma_uint32 channel = 0; channel < channel_count; channel++) {
              if (context->output_channel_mapping_[channel]) {
                if (left_channel) {
                  for (int frame = 0; frame < frame_count; frame++) {
                    ((float *) pOutput)[frame * channel_count + channel] = left[frame];
                  }
                } else {
                  for (int frame = 0; frame < frame_count; frame++) {
                    ((float *) pOutput)[frame * channel_count + channel] = right[frame];
                  }
                }
                left_channel = !left_channel;
              }
            }
            (void) pInput;
          };
    } else {
      output_device_config.dataCallback =
          [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frame_count) {
            // We know the output channel mapping, so only read from the enabled channels
            auto context = static_cast<AudioIO *>(pDevice->pUserData);
            const ma_uint32 channel_count = pDevice->playback.channels;
            float mono[frame_count];
            context->left_output_channel_->get(mono, frame_count);
            for (ma_uint32 channel = 0; channel < channel_count; channel++) {
              if (context->output_channel_mapping_[channel]) {
                // Deinterleave
                for (int frame = 0; frame < frame_count; frame++) {
                  ((float *) pOutput)[frame * channel_count + channel] = mono[frame];
                }
              }
            }
            (void) pInput;
          };
    }
    ma_result result = ma_device_init(&context_, &output_device_config, &output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize playback device. Error code " + std::to_string(result));
    }
    if (start) {
      startReceiving();
    }
  }
}
void AudioIO::startSending() {
  if (initialized_ && !ma_device_is_started(&input_device_)) {
    PLOGD << "AudioIO::startSending";
    ma_result result = ma_device_start(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start capture device. Error code " + std::to_string(result));
    }
  }
}
void AudioIO::stopSending() {
  if (initialized_ && ma_device_is_started(&input_device_)) {
    PLOGD << "AudioIO::stopSending";
    ma_result result = ma_device_stop(&input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop capture device. Error code " + std::to_string(result));
    }
  }
}
void AudioIO::startReceiving() {
  if (initialized_ && !ma_device_is_started(&output_device_)) {
    PLOGD << "AudioIO::startReceiving";
    ma_result result = ma_device_start(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to start playback device. Error code " + std::to_string(result));
    }
  }
}
void AudioIO::stopReceiving() {
  if (initialized_ && ma_device_is_started(&output_device_)) {
    PLOGD << "AudioIO::stopReceiving";
    ma_result result = ma_device_stop(&output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to stop playback device. Error code " + std::to_string(result));
    }
  }
}
const ChannelMap &AudioIO::getChannelMapping() const {
  ChannelMap map;
  for (int i = 0; i < input_channels_.size(); i++) {
    if (input_channels_[i]) {
      map[i] = input_channels_[i]->audio_track_id;
    } else {
      map[i] = std::nullopt;
    }
  }
  return std::move(map);
}

void AudioIO::read(std::size_t channel, float *buff, size_t size) {
  if (input_channels_[channel])
    input_channels_[channel]->buffer->get(buff, size);
}

void AudioIO::writeLeft(const float *buf, size_t size) {
  if (left_output_channel_)
    left_output_channel_->write(buf, size);
}
void AudioIO::writeRight(const float *buf, size_t size) {
  if (right_output_channel_)
    right_output_channel_->write(buf, size);
}
void AudioIO::publishChannelsAsTracks(DigitalStage::Api::Client &client) {
  PLOGD << "AudioIO::publishChannelsAsAudioTracks";
  auto store = client.getStore();
  auto input_sound_card = store->getInputSoundCard();
  auto stageId = store->getStageId();
  if (store->isReady() && input_sound_card && stageId) {
    for (int i = 0; i < input_channels_.size(); i++) {
      if (input_channels_[i]) {
        if (i >= input_sound_card->channels.size() || !input_sound_card->channels[i].active) {
          PLOGD << "AudioService::publishChannelsAsAudioTracks -> Un-publishing audio track "
                << input_channels_[i]->audio_track_id
                << " of channel " << i;
          client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, input_channels_[i]->audio_track_id);
        }
      }
    }
    // Request a new audio track for all channels if not existing yet
    for (auto i = 0; i < input_sound_card->channels.size(); i++) {
      if (input_sound_card->channels[i].active && !input_channels_[i]) {
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
    for (const auto &item: input_channels_) {
      if (item) {
        PLOGD << "AudioService::publishChannelsAsAudioTracks -> Un-publishing all including audio track "
              << item->audio_track_id;
        client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item->audio_track_id);
      }
    }
  }
}