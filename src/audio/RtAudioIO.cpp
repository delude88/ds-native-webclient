//
// Created by Tobias Hegemann on 06.11.21.
//

#include "RtAudioIO.h"
#include "../utils/cp1252_to_utf8.h"
#include <memory>
#include <plog/Log.h>

RtAudioIO::RtAudioIO(DigitalStage::Api::Client &client)
    : AudioIO(client), client_(client) {
  PLOGD << "RtAudioIO::RtAudioIO";
}

std::vector<nlohmann::json> RtAudioIO::enumerateRtDevices(RtAudio::Api rt_api,
                                                          const DigitalStage::Api::Store &store) {
  PLOGD << "RtAudioIO::enumerateRtDevices";
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
  sound_card["type"] = type;
  sound_card["uuid"] = id;
  sound_card["label"] = CP1252_to_UTF8(info.name);
  sound_card["isDefault"] = type == "input" ? info.isDefaultInput : info.isDefaultOutput;
  sound_card["sampleRates"] = info.sampleRates;

  const auto localDeviceId = store.getLocalDeviceId();
  const auto existing = localDeviceId ? store.getSoundCardByDeviceAndDriverAndTypeAndLabel(
      *store.getLocalDeviceId(),
      driver,
      type,
      info.name
  ) : std::nullopt;

  if (!existing) {
    sound_card["frameSize"] = 256;
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

std::vector<json> RtAudioIO::enumerateDevices(const DigitalStage::Api::Store &store) {
  PLOGD << "RtAudioIO::enumerateDevices";
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
  // Capture all dependencies
  auto store = client.getStore();
  auto local_device = store->getLocalDevice();
  auto input_sound_card = store->getInputSoundCard();
  auto output_sound_card = store->getOutputSoundCard();
  if (local_device &&
      /*local_device->audioEngine &&
      *local_device->audioEngine == "rtaudio" &&*/
      input_sound_card &&
      output_sound_card &&
      local_device->audioDriver) {
    PLOGD << "RtAudioIO::initAudio";
    RtAudio::Api api = RtAudio::getCompiledApiByName(*local_device->audioDriver);
    if (api != RtAudio::Api::UNSPECIFIED) {
      // Stop rt audio if running
      if (rt_audio_ && rt_audio_->isStreamOpen()) {
        rt_audio_->closeStream();
      }
      // Re-init rt audio if necessary
      if (!rt_audio_ || rt_audio_->getCurrentApi() != api) {
        rt_audio_ = std::make_unique<RtAudio>(api);
      }
      // Configure stream using sound card objects
      RtAudio::StreamParameters inputParameters;
      inputParameters.deviceId = std::stoi(input_sound_card->uuid);
      inputParameters.nChannels = input_sound_card->channels.size();  // take all
      unPublishAll(client);
      for (int channel = 0; channel < input_sound_card->channels.size(); channel++) {
        if (input_sound_card->channels[channel].active) {
          publishChannel(client, channel);
        }
      }

      RtAudio::StreamParameters outputParameters;
      outputParameters.deviceId = std::stoi(output_sound_card->uuid);
      outputParameters.nChannels = output_sound_card->channels.size();  // take all
      num_total_output_channels_ = output_sound_card->channels.size();
      output_channels_.fill(false);
      num_output_channels_ = 0;
      for (auto i = 0; i < output_sound_card->channels.size(); i++) {
        if (output_sound_card->channels.at(i).active) {
          output_channels_[i] = true;
          num_output_channels_++;
        }
      }

      RtAudio::StreamOptions options;
      //options.flags = RTAUDIO_MINIMIZE_LATENCY | RTAUDIO_NONINTERLEAVED | RTAUDIO_SCHEDULE_REALTIME;
      options.flags = RTAUDIO_NONINTERLEAVED | RTAUDIO_SCHEDULE_REALTIME;
      options.priority = 1;
      try {
        auto callback = [](void *output,
                           void *input,
                           unsigned int bufferSize,
                           double streamTime,
                           RtAudioStreamStatus status,
                           void *userData) {
          auto context = static_cast<RtAudioIO *>(userData);
          auto *outputBuffer = (float *) output;
          auto *inputBuffer = (float *) input;

          std::cout << bufferSize << std::endl;

          if (status) { PLOGE << "stream over/underflow detected"; }

          std::unordered_map<std::string, float *> input_channels;
          for (const auto &item: context->input_channel_mapping_) {
            // Just assign pointer reference
            input_channels[item.second] = &inputBuffer[item.first * bufferSize];
          }

          // Create empty output buffer
          auto **out = (float **) malloc(bufferSize * context->num_output_channels_ * sizeof(float *));

          // Now let the listeners use the input channels and fill the output buffers
          context->onDuplex(input_channels, out, context->num_output_channels_, bufferSize);

          unsigned int relative_channel = 0;
          for (int channel = 0; channel < context->num_total_output_channels_; channel++) {
            if (context->output_channels_[channel]) {
              memcpy(&outputBuffer[channel * bufferSize], out[relative_channel], bufferSize * sizeof(float));
              relative_channel++;
            }
          }
          free(out);

          return 0;
        };

        PLOGD << "Open RT Audio Stream with " << output_sound_card->sampleRate << " Sample rate and "
              << output_sound_card->frameSize << " frame size";
        rt_audio_->openStream(
            &outputParameters,
            &inputParameters,
            RTAUDIO_FLOAT32,
            // Always prefer the output sound card settings
            output_sound_card->sampleRate,
            &output_sound_card->frameSize,
            callback,
            this,
            &options
        );
      } catch (RtAudioError &e) {
        PLOGE << e.getMessage() << '\n';
        if (rt_audio_ && rt_audio_->isStreamOpen()) {
          rt_audio_->closeStream();
        }
        //exit(0);
      }

    }
  } else {

  }
}

void RtAudioIO::setAudioDriver(const std::string &audio_driver) {
  PLOGD << "RtAudioIO::setAudioDriver()";
  initAudio(client_);
}

void RtAudioIO::setInputSoundCard(const SoundCard &sound_card, bool start, DigitalStage::Api::Client &client) {
  PLOGD << "RtAudioIO::setInputSoundCard()";
  initAudio(client);
  if (start) {
    startSending();
  }
}
void RtAudioIO::setOutputSoundCard(const SoundCard &sound_card, bool start) {
  PLOGD << "RtAudioIO::setOutputSoundCard()";
  initAudio(client_);
  if (start) {
    startReceiving();
  }
}
void RtAudioIO::startSending() {
  PLOGD << "RtAudioIO::startSending()";
  if (rt_audio_ && !rt_audio_->isStreamRunning())
    rt_audio_->startStream();
}
void RtAudioIO::stopSending() {
  PLOGD << "RtAudioIO::stopSending()";
  if (rt_audio_ && rt_audio_->isStreamRunning())
    rt_audio_->stopStream();
}
void RtAudioIO::startReceiving() {
  PLOGD << "RtAudioIO::startReceiving()";
  if (rt_audio_ && !rt_audio_->isStreamRunning())
    rt_audio_->startStream();
}
void RtAudioIO::stopReceiving() {
  PLOGD << "RtAudioIO::stopReceiving()";
  if (rt_audio_ && rt_audio_->isStreamRunning())
    rt_audio_->stopStream();
}