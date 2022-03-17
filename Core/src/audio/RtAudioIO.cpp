//
// Created by Tobias Hegemann on 06.11.21.
//

#include "RtAudioIO.h"
#include "../utils/cp1252_to_utf8.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <plog/Log.h>

[[maybe_unused]] RtAudioIO::RtAudioIO(std::shared_ptr<DigitalStage::Api::Client> client) : AudioIO(std::move(client)), is_running_(true) {
}

RtAudioIO::~RtAudioIO() {
  PLOGD << "Stopping rt audio";
  is_running_ = false;
  rt_audio_.reset();
}

std::vector<nlohmann::json> RtAudioIO::enumerateRtDevices(RtAudio::Api rt_api,
                                                          const std::shared_ptr<DigitalStage::Api::Store>& store) {
  PLOGD << "enumerateRtDevices";
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
nlohmann::json RtAudioIO::getDevice(const std::string &uuid, // NOLINT(bugprone-easily-swappable-parameters)
                                    const std::string &driver,
                                    const std::string &type,
                                    const RtAudio::DeviceInfo &info,
                                    const std::shared_ptr<DigitalStage::Api::Store>& store) {
  nlohmann::json sound_card;
  sound_card["audioDriver"] = driver;
  sound_card["audioEngine"] = "rtaudio";
  sound_card["type"] = type;
  sound_card["uuid"] = uuid;
  sound_card["label"] = CP1252_to_UTF8(info.name);
  sound_card["isDefault"] = type == "input" ? info.isDefaultInput : info.isDefaultOutput;
  sound_card["sampleRates"] = info.sampleRates;
  sound_card["online"] = true;

  const auto local_device_id = store->getLocalDeviceId();
  const auto existing = local_device_id ? store->getSoundCardByDeviceAndDriverAndTypeAndLabel(
      *store->getLocalDeviceId(),
      driver,
      type,
      info.name
  ) : std::nullopt;

  if (!existing) {
    sound_card["sampleRate"] = info.preferredSampleRate;
    sound_card["bufferSize"] = 512;
  }

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

std::vector<nlohmann::json> RtAudioIO::enumerateDevices(std::shared_ptr<DigitalStage::Api::Store> store) {
  PLOGD << "enumerateDevices";
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

void RtAudioIO::initAudio() {
  std::lock_guard<std::mutex> guard{mutex_};
  if (!is_running_) {
    return;
  }

  // Capture all dependencies
  auto store_ptr = client_->getStore();
  if(store_ptr.expired()) {
    return;
  }
  auto store = store_ptr.lock();
  auto local_device = store->getLocalDevice();

  if (local_device && local_device->audioDriver) {

    auto input_sound_card = store->getInputSoundCard();
    auto output_sound_card = store->getOutputSoundCard();
    unsigned int sample_rate = 48000;
    buffer_size_ = 512;

    /**
     * Input sound card handling
     */
    bool has_input = false;
    if (input_sound_card && input_sound_card->audioEngine == "rtaudio" && input_sound_card->online
        && local_device->sendAudio) {
      has_input = true;
      PLOGD << "Got input sound card";
      sample_rate = input_sound_card->sampleRate;
      buffer_size_ = input_sound_card->bufferSize;
      input_parameters_.deviceId = std::stoi(input_sound_card->uuid);
      input_parameters_.nChannels = input_sound_card->channels.size();
      // Sync enabled channels
      for (int channel = 0; channel < input_sound_card->channels.size(); channel++) {
        if (input_sound_card->channels[channel].active) {
          publishChannel(channel);
        } else if (published_channels_[channel]) {
          unPublishChannel(channel);
        }
      }
    } else {
      unPublishAll();
    }

    /**
     * Output sound card handling
     */
    num_total_output_channels_ = 0;
    output_channels_.fill(false);
    bool has_output = false;
    if (output_sound_card && output_sound_card->audioEngine == "rtaudio" && output_sound_card->online
        && local_device->receiveAudio) {
      has_output = true;
      PLOGD << "Got output sound card";
      sample_rate = output_sound_card->sampleRate;
      buffer_size_ = output_sound_card->bufferSize;
      output_parameters_.deviceId = std::stoi(output_sound_card->uuid);
      output_parameters_.nChannels = output_sound_card->channels.size();
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

      auto callback = [](void *output, // NOLINT(bugprone-easily-swappable-parameters)
                         void *input,
                         unsigned int bufferSize,
                         double  /*streamTime*/,
                         RtAudioStreamStatus status,
                         void *userData) {
        auto *context = static_cast<RtAudioIO *>(userData);
        if (context->mutex_.try_lock()) {
          auto *output_buffer = static_cast<float *>(output);
          auto *input_buffer = static_cast<float *>(input);

          if (status) {
            if (status & RTAUDIO_INPUT_OVERFLOW) {
              PLOGW << "Input data was discarded because of an overflow condition at the driver";
            }
            if (status & RTAUDIO_OUTPUT_UNDERFLOW) {
              PLOGW << "The output buffer ran low, likely causing a gap in the output sound";
            }
          }

          if (input_buffer && output_buffer) {
            // Duplex
            std::unordered_map<std::string, float *> input_channels;
            for (const auto &item: context->input_channel_mapping_) {
              input_channels[item.second] = &input_buffer[static_cast<size_t>(item.first) * bufferSize];
            }

            auto **out = static_cast<float **>(malloc(bufferSize * context->num_output_channels_ * sizeof(float *)));
            for (int output_channel = 0; output_channel < context->num_output_channels_; output_channel++) {
              out[output_channel] = static_cast<float *>(malloc(bufferSize * sizeof(float)));
            }

            context->onDuplex(input_channels, out, context->num_output_channels_, bufferSize);

            unsigned int relative_channel = 0;
            for (std::size_t channel = 0; channel < context->num_total_output_channels_; channel++) {
              if (context->output_channels_[channel]) {
                memcpy(&output_buffer[channel * bufferSize], out[relative_channel], bufferSize * sizeof(float));
                relative_channel++;
              }
            }
            free(out);
          } else if (input_buffer) {
            // Capture only
            for (const auto &item: context->input_channel_mapping_) {
              context->onCapture(item.second, &input_buffer[item.first * bufferSize], bufferSize);
            }
          } else if (output_buffer) {
            // Playback only
            auto **out = static_cast<float **>(malloc(bufferSize * context->num_output_channels_ * sizeof(float *)));
            for (int output_channel = 0; output_channel < context->num_output_channels_; output_channel++) {
              out[output_channel] = static_cast<float *>(malloc(bufferSize * sizeof(float)));
            }
            context->onPlayback(out, context->num_output_channels_, bufferSize);
            unsigned int relative_channel = 0;
            for (std::size_t channel = 0; channel < context->num_total_output_channels_; channel++) {
              if (context->output_channels_[channel]) {
                memcpy(&output_buffer[channel * bufferSize], out[relative_channel], bufferSize * sizeof(float));
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

      if (is_running_) {
        try {
          /**
           * Audio driver handling
           */
          RtAudio::Api api = RtAudio::getCompiledApiByName(*local_device->audioDriver);
          PLOGD << "(Re)init with audio driver " << *local_device->audioDriver;
          rt_audio_ = std::make_unique<RtAudio>(std::move(api));
          rt_audio_->openStream(
              has_output ? &output_parameters_ : nullptr,
              has_input ? &input_parameters_ : nullptr,
              RTAUDIO_FLOAT32,
              // Always prefer the output sound card settings
              sample_rate,
              &buffer_size_,
              callback,
              this,
              &options
          );
          rt_audio_->startStream();
          PLOGD << "Started Audio IO";
        } catch (RtAudioError &e) {
          PLOGE << e.getMessage();
          rt_audio_.reset();
        }
      }
    } else {
      PLOGI << "Stopped Audio IO - no input or output sound card selected";
    }
  } else {
    PLOGD << "Got invalid local_device or audio driver is missing";
  }
}

void RtAudioIO::setAudioDriver(const std::string & /*audio_driver*/) {
  PLOGD << "setAudioDriver()";
  initAudio();
}

void RtAudioIO::setInputSoundCard(const DigitalStage::Types::SoundCard & /*sound_card*/, bool  /*start*/) {
  PLOGD << "setInputSoundCard()";
  initAudio();
}
void RtAudioIO::setOutputSoundCard(const DigitalStage::Types::SoundCard & /*sound_card*/, bool  /*start*/) {
  PLOGD << "setOutputSoundCard()";
  initAudio();
}
void RtAudioIO::startSending() {
  PLOGD << "startSending()";
  initAudio();
}
void RtAudioIO::stopSending() {
  PLOGD << "RtAudstopSending()";
  initAudio();
}
void RtAudioIO::startReceiving() {
  PLOGD << "startReceiving()";
  initAudio();
}
void RtAudioIO::stopReceiving() {
  PLOGD << "stopReceiving()";
  initAudio();
}
void RtAudioIO::restart() {
  PLOGD << "restart()";
  initAudio();
}
[[maybe_unused]] unsigned int RtAudioIO::getLowestBufferSize(std::optional<RtAudio::StreamParameters> inputParameters,
                                                             std::optional<RtAudio::StreamParameters> outputParameters,
                                                             unsigned int sample_rate) {
  PLOGD << "getLowestBufferSize";
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