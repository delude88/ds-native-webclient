//
// Created by Tobias Hegemann on 06.11.21.
//

#include "RtAudioIO.h"
#include "../utils/cp1252_to_utf8.h"
#include <memory>
#include <plog/Log.h>

RtAudioIO::RtAudioIO(DigitalStage::Api::Client &client)
    : AudioIO(client), client_(client) {
  PLOGV << "RtAudioIO";
}

std::vector<nlohmann::json> RtAudioIO::enumerateRtDevices(RtAudio::Api rt_api,
                                                          const DigitalStage::Api::Store &store) {
  PLOGV << "enumerateRtDevices";
  auto sound_cards = std::vector<nlohmann::json>();
  auto rt_audio = std::make_unique<RtAudio>(rt_api);
  const std::string driver = RtAudio::getApiName(rt_api);
  unsigned int devices = rt_audio->getDeviceCount();
  RtAudio::DeviceInfo info;
  for (int i = 0; i < devices; i++) {
    info = rt_audio->getDeviceInfo(i);
    if (info.inputChannels > 0) {
      sound_cards.push_back(getDevice(std::to_string(i), driver, "input", info, store));
    }
    if (info.outputChannels > 0) {
      sound_cards.push_back(getDevice(std::to_string(i), driver, "output", info, store));
    }
  }
  return sound_cards;
}
nlohmann::json RtAudioIO::getDevice(const std::string &id,
                                    const std::string &driver,
                                    const std::string &type,
                                    const RtAudio::DeviceInfo &info,
                                    const DigitalStage::Api::Store &store) {
  nlohmann::json sound_card;
  sound_card["audioDriver"] = driver;
  sound_card["audioEngine"] = "rtaudio";
  sound_card["type"] = type;
  sound_card["uuid"] = id;
  sound_card["label"] = CP1252_to_UTF8(info.name);
  sound_card["isDefault"] = type == "input" ? info.isDefaultInput : info.isDefaultOutput;
  sound_card["sampleRates"] = info.sampleRates;
  sound_card["online"] = true;

  const auto localDeviceId = store.getLocalDeviceId();
  const auto existing = localDeviceId ? store.getSoundCardByDeviceAndDriverAndTypeAndLabel(
      *store.getLocalDeviceId(),
      driver,
      type,
      info.name
  ) : std::nullopt;

  if (!existing) {
    sound_card["sampleRate"] = info.preferredSampleRate;
    sound_card["bufferSize"] = 512;
  }
  //TODO: REMOVE ME:
  sound_card["bufferSize"] = 128;

  std::vector<DigitalStage::Types::Channel> channels;
  auto channel_count = type == "input" ? info.inputChannels : info.outputChannels;
  for (int i = 0; i < channel_count; ++i) {
    if (existing && existing->channels.size() >= i) {
      channels.push_back(existing->channels[i]);
    } else {
      DigitalStage::Types::Channel channel;
      channel.label = "Kanal " + std::to_string(i + 1);
      channel.active = false;
      channels.push_back(channel);
    }
  }
  sound_card["channels"] = channels;

  return sound_card;
}

std::vector<json> RtAudioIO::enumerateDevices(const DigitalStage::Api::Store &store) {
  PLOGV << "enumerateDevices";
  auto sound_cards = std::vector<nlohmann::json>();
  // Fetch existing

  std::vector<RtAudio::Api> compiled_apis;
  RtAudio::getCompiledApi(compiled_apis);
  for (const auto &item: compiled_apis) {
    auto found_sound_cards = enumerateRtDevices(item, store);
    sound_cards.insert(sound_cards.end(), found_sound_cards.begin(), found_sound_cards.end());
  }

  return sound_cards;
}

void RtAudioIO::initAudio(DigitalStage::Api::Client &client) {
  std::lock_guard<std::mutex> guard{mutex_};

  // Capture all dependencies
  auto store = client.getStore();
  auto local_device = store->getLocalDevice();

  // Stop and close rt audio if open
  if (rt_audio_) {
    if (rt_audio_->isStreamRunning()) {
      PLOGV << "RtAudiostop stream";
      rt_audio_->stopStream();
    }
    if (rt_audio_->isStreamOpen()) {
      PLOGV << "close stream";
      rt_audio_->closeStream();
    }
  }

  if (local_device && local_device->audioDriver) {
    PLOGV << "Got valid audio driver";
    auto input_sound_card = store->getInputSoundCard();
    auto output_sound_card = store->getOutputSoundCard();
    unsigned int sampleRate = 48000;
    unsigned int bufferSize = 512;

    /**
     * Audio driver handling
     */
    RtAudio::Api api = RtAudio::getCompiledApiByName(*local_device->audioDriver);
    if (!rt_audio_ || rt_audio_->getCurrentApi() != api) {
      PLOGV << "(Re)init with audio driver " << *local_device->audioDriver;
      rt_audio_ = std::make_unique<RtAudio>(api);
    }

    /**
     * Input sound card handling
     */
    bool has_input = false;
    RtAudio::StreamParameters inputParameters;
    if (input_sound_card && input_sound_card->audioEngine == "rtaudio" && input_sound_card->online
        && local_device->sendAudio) {
      has_input = true;
      PLOGV << "Got input sound card";
      sampleRate = input_sound_card->sampleRate;
      bufferSize = input_sound_card->bufferSize;
      inputParameters = RtAudio::StreamParameters();
      inputParameters.deviceId = std::stoi(input_sound_card->uuid);
      inputParameters.nChannels = input_sound_card->channels.size();
      // Sync enabled channels
      for (int channel = 0; channel < input_sound_card->channels.size(); channel++) {
        if (input_sound_card->channels[channel].active) {
          publishChannel(client, channel);
        } else if (input_channel_mapping_.count(channel)) {
          unPublishChannel(client, channel);
        }
      }
    } else {
      unPublishAll(client);
    }

    /**
     * Output sound card handling
     */
    num_total_output_channels_ = 0;
    output_channels_.fill(false);
    bool has_output = false;
    RtAudio::StreamParameters outputParameters;
    if (output_sound_card && output_sound_card->audioEngine == "rtaudio" && output_sound_card->online
        && local_device->receiveAudio) {
      has_output = true;
      PLOGV << "Got output sound card";
      sampleRate = output_sound_card->sampleRate;
      bufferSize = output_sound_card->bufferSize;
      outputParameters = RtAudio::StreamParameters();
      outputParameters.deviceId = std::stoi(output_sound_card->uuid);
      outputParameters.nChannels = output_sound_card->channels.size();
      num_total_output_channels_ = output_sound_card->channels.size();
      num_output_channels_ = 0;
      for (auto i = 0; i < output_sound_card->channels.size(); i++) {
        if (output_sound_card->channels.at(i).active) {
          output_channels_[i] = true;
          num_output_channels_++;
        }
      }
    }

    if (has_input || has_output) {
      RtAudio::StreamOptions options;
      options.flags = RTAUDIO_NONINTERLEAVED | RTAUDIO_SCHEDULE_REALTIME;
      options.priority = 1;

      auto callback = [](void *output,
                         void *input,
                         unsigned int bufferSize,
                         double streamTime,
                         RtAudioStreamStatus status,
                         void *userData) {
        auto context = static_cast<RtAudioIO *>(userData);
        if (context->mutex_.try_lock()) {
          auto *outputBuffer = (float *) output;
          auto *inputBuffer = (float *) input;

          if (status) {
            if (status & RTAUDIO_INPUT_OVERFLOW) {
              PLOGW << "Input data was discarded because of an overflow condition at the driver";
            }
            if (status & RTAUDIO_OUTPUT_UNDERFLOW) {
              PLOGW << "The output buffer ran low, likely causing a gap in the output sound";
            }
          }

          if (inputBuffer && outputBuffer) {
            // Duplex
            std::unordered_map<std::string, float *> input_channels;
            for (const auto &item: context->input_channel_mapping_) {
              input_channels[item.second] = &inputBuffer[item.first * bufferSize];
            }

            auto **out = (float **) malloc(bufferSize * context->num_output_channels_ * sizeof(float *));
            for (int output_channel = 0; output_channel < context->num_output_channels_; output_channel++) {
              out[output_channel] = (float *) malloc(bufferSize * sizeof(float));
            }

            context->onDuplex(input_channels, out, context->num_output_channels_, bufferSize);

            unsigned int relative_channel = 0;
            for (int channel = 0; channel < context->num_total_output_channels_; channel++) {
              if (context->output_channels_[channel]) {
                memcpy(&outputBuffer[channel * bufferSize], out[relative_channel], bufferSize * sizeof(float));
                relative_channel++;
              }
            }
            free(out);
          } else if (inputBuffer) {
            // Capture only
            for (const auto &item: context->input_channel_mapping_) {
              context->onCapture(item.second, &inputBuffer[item.first * bufferSize], bufferSize);
            }
          } else if (outputBuffer) {
            // Playback only
            auto **out = (float **) malloc(bufferSize * context->num_output_channels_ * sizeof(float *));
            for (int output_channel = 0; output_channel < context->num_output_channels_; output_channel++) {
              out[output_channel] = (float *) malloc(bufferSize * sizeof(float));
            }
            context->onPlayback(out, context->num_output_channels_, bufferSize);
            unsigned int relative_channel = 0;
            for (int channel = 0; channel < context->num_total_output_channels_; channel++) {
              if (context->output_channels_[channel]) {
                memcpy(&outputBuffer[channel * bufferSize], out[relative_channel], bufferSize * sizeof(float));
                relative_channel++;
              }
            }
            free(out);
          }
          context->mutex_.unlock();
        } else {
          PLOGW << "Not owning lock";
        }
        return 0;
      };

      try {
        PLOGV << "open stream";
        rt_audio_->openStream(
            has_output ? &outputParameters : nullptr,
            has_input ? &inputParameters : nullptr,
            RTAUDIO_FLOAT32,
            // Always prefer the output sound card settings
            sampleRate,
            &bufferSize,
            callback,
            this,
            &options
        );
        PLOGV << "start stream";
        rt_audio_->startStream();
        PLOGI << "Started Audio IO";
      } catch (RtAudioError &e) {
        PLOGE << e.getMessage();
        if (rt_audio_ && rt_audio_->isStreamOpen()) {
          rt_audio_->closeStream();
          PLOGI << "Stopped Audio IO";
        }
        //exit(0);
      }
    } else {
      PLOGI << "Stopped Audio IO - no input or output sound card selected";
    }
  } else {
    PLOGV << "Got invalid local_device or audio driver is missing";
  }
}

void RtAudioIO::setAudioDriver(const std::string &audio_driver) {
  PLOGV << "setAudioDriver()";
  initAudio(client_);
}

void RtAudioIO::setInputSoundCard(const SoundCard &sound_card, bool start, DigitalStage::Api::Client &client) {
  PLOGV << "setInputSoundCard()";
  initAudio(client);
}
void RtAudioIO::setOutputSoundCard(const SoundCard &sound_card, bool start) {
  PLOGV << "setOutputSoundCard()";
  initAudio(client_);
}
void RtAudioIO::startSending() {
  PLOGV << "startSending()";
  initAudio(client_);
}
void RtAudioIO::stopSending() {
  PLOGV << "RtAudstopSending()";
  initAudio(client_);
}
void RtAudioIO::startReceiving() {
  PLOGV << "startReceiving()";
  initAudio(client_);
}
void RtAudioIO::stopReceiving() {
  PLOGV << "stopReceiving()";
  initAudio(client_);
}
unsigned int RtAudioIO::getLowestBufferSize(std::optional<RtAudio::StreamParameters> inputParameters,
                                            std::optional<RtAudio::StreamParameters> outputParameters,
                                            unsigned int sample_rate) {
  PLOGV << "getLowestBufferSize";
  unsigned int buffer_size = 256; // = default
  if (inputParameters || outputParameters) {
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_MINIMIZE_LATENCY | RTAUDIO_SCHEDULE_REALTIME;
    options.priority = 1;
    try {
      rt_audio_->openStream(
          outputParameters ? &(*outputParameters) : nullptr,
          inputParameters ? &(*inputParameters) : nullptr,
          RTAUDIO_FLOAT32,
          // Always prefer the output sound card settings
          sample_rate,
          &buffer_size,  // Will be ignored, since RTAUDIO_SCHEDULE_REALTIME is set
          nullptr,
          nullptr,
          &options
      );
      rt_audio_->closeStream();
    } catch (RtAudioError &e) {
      PLOGE << "Error occurred while opening stream to determine lowest buffer size" << e.getMessage();
    }
    buffer_size--;
    buffer_size |= buffer_size >> 1;
    buffer_size |= buffer_size >> 2;
    buffer_size |= buffer_size >> 4;
    buffer_size |= buffer_size >> 8;
    buffer_size |= buffer_size >> 16;
    buffer_size++;
  }
  return buffer_size;
}
