//
// Created by Tobias Hegemann on 24.10.21.
//

#include "AudioProvider.h"
#include "../../utils/miniaudio.h"

AudioProvider::AudioProvider()
    : initialized_(false),
      context_(),
      backend_(),
      input_device_(),
      output_device_(),
      mixer_(std::make_unique<AudioMixer>()) {
};

AudioProvider::~AudioProvider() {
  ma_device_uninit(&input_device_);
  ma_device_uninit(&output_device_);
  ma_context_uninit(&context_);
}

void AudioProvider::setAudioDriver(const std::string &audio_driver) {
  std::cout << "AudioProvider::setAudioDriver" << std::endl;
  initialized_ = false;
  backend_ = convert_string_to_backend(audio_driver);
  ma_context_config config = ma_context_config_init();
  config.threadPriority = ma_thread_priority_realtime;
  if (ma_context_init(&backend_, 1, &config, &context_) != MA_SUCCESS) {
    throw std::runtime_error("Could not initialize audio context");
  }
  initialized_ = true;
  initInputSoundCard();
  initOutputSoundCard();
}

void AudioProvider::setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card) {
  std::cout << "AudioProvider::setInputSoundCard" << std::endl;
  input_sound_card_ = sound_card;
  initInputSoundCard();
}

void AudioProvider::setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card) {
  std::cout << "AudioProvider::setOutputSoundCard" << std::endl;
  output_sound_card_ = sound_card;
  initOutputSoundCard();
}

void AudioProvider::initInputSoundCard() {
  if (initialized_ && input_sound_card_) {
    std::cout << "AudioProvider::initInputSoundCard" << std::endl;
    ma_device_config inputDeviceConfig;
    auto uuid = convert_device_id(backend_, input_sound_card_->uuid);
    inputDeviceConfig = ma_device_config_init(ma_device_type_capture);
    inputDeviceConfig.capture.pDeviceID = &uuid;
    inputDeviceConfig.capture.format = FORMAT;
    inputDeviceConfig.capture.shareMode = ma_share_mode_exclusive;
    inputDeviceConfig.capture.channels = 0;
    inputDeviceConfig.performanceProfile = ma_performance_profile_low_latency;
    inputDeviceConfig.sampleRate = input_sound_card_->sampleRate;
    inputDeviceConfig.periodSizeInMilliseconds = input_sound_card_->periodSize;
    inputDeviceConfig.periods = input_sound_card_->numPeriods;
    inputDeviceConfig.pUserData = mixer_.get();
    inputDeviceConfig.dataCallback = [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
      auto mixer = static_cast<AudioMixer *>(pDevice->pUserData);
      mixer->writeLocal((float *) pInput, pDevice->capture.channels, frameCount);
      (void) pOutput;
    };
    ma_result result;
    result = ma_device_init(nullptr, &inputDeviceConfig, &input_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize capture device. Error code " + std::to_string(result));
    }

    result = ma_device_start(&input_device_);
    if (result != MA_SUCCESS) {
      ma_device_uninit(&input_device_);
      throw std::runtime_error("Failed to start capture device. Error code " + std::to_string(result));
    }
    std::cout << "Initialized capture device." << std::endl;
  }
}

void AudioProvider::initOutputSoundCard() {
  if (initialized_ && output_sound_card_) {
    std::cout << "AudioProvider::initOutputSoundCard" << std::endl;
    ma_device_config outputDeviceConfig;
    outputDeviceConfig = ma_device_config_init(ma_device_type_playback);
    outputDeviceConfig.playback.format = FORMAT;
    outputDeviceConfig.playback.channels = 0;
    outputDeviceConfig.sampleRate = output_sound_card_->sampleRate;
    outputDeviceConfig.performanceProfile = ma_performance_profile_low_latency;
    outputDeviceConfig.pUserData = mixer_.get();
    outputDeviceConfig.dataCallback = [](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
      auto mixer = static_cast<AudioMixer *>(pDevice->pUserData);
      mixer->mixDown((float *) pOutput, pDevice->playback.channels, frameCount);
      (void) pInput;
    };
    ma_result result;
    result = ma_device_init(&context_, &outputDeviceConfig, &output_device_);
    if (result != MA_SUCCESS) {
      throw std::runtime_error("Failed to initialize playback device. Error code " + std::to_string(result));
    }

    result = ma_device_start(&output_device_);
    if (result != MA_SUCCESS) {
      ma_device_uninit(&output_device_);
      throw std::runtime_error("Failed to start playback device. Error code " + std::to_string(result));
    }
  }
}

void AudioProvider::attachHandlers(DigitalStage::Api::Client &client) {
  std::cout << "AudioProvider::attachHandlers" << std::endl;
  bool is_api_ready = false;
  client.ready.connect([this, &is_api_ready](const DigitalStage::Api::Store *store) {
    is_api_ready = true;
    std::cout << "AudioProvider handle ready" << std::endl;
    auto localDevicePtr = store->getLocalDevice();
    if (localDevicePtr) {
      auto localDevice = *localDevicePtr;
      // Now we can set the audio driver, input and output sound cards IF they are set already
      if (localDevice.audioDriver) {
        setAudioDriver(*localDevice.audioDriver);
        if (localDevice.inputSoundCardId) {
          setInputSoundCard(*store->soundCards.get(*localDevice.inputSoundCardId));
        }
        if (localDevice.outputSoundCardId) {
          setOutputSoundCard(*store->soundCards.get(*localDevice.outputSoundCardId));
        }
      }
    }
  });
  client.soundCardChanged.connect([this, &is_api_ready](const std::string &_id, const nlohmann::json &update,
                                                        const DigitalStage::Api::Store *store) {
    if (!is_api_ready) {
      return;
    }
    std::cout << "AudioProvider handle soundCardChanged" << std::endl;
    // Is the sound card the current input or output sound card?
    // Are the changes relevant, so that we should react here?
    if (input_sound_card_ && input_sound_card_->_id == _id) {
      if (update.contains("channels")) {
        setInputSoundCard(*store->soundCards.get(_id));
      }
    } else if (output_sound_card_ && output_sound_card_->_id == _id) {
      if (update.contains("channels")) {
        setOutputSoundCard(*store->soundCards.get(_id));
      }
    }
  });
  client.deviceChanged.connect([this, &is_api_ready](const std::string &id, const nlohmann::json &update,
                                                     const DigitalStage::Api::Store *store) {
    if (!is_api_ready) {
      return;
    }
    std::cout << "AudioProvider handle deviceChanged" << std::endl;
    // Did the local device changed? (so this device needs an update)
    auto localDeviceId = store->getLocalDeviceId();
    if (localDeviceId && *localDeviceId == id) {
      if (update.contains("audioDriver")) {
        setAudioDriver(update["audioDriver"]);
      }
      // Did the input sound card change?
      if (update.contains("inputSoundCardId")) {
        setInputSoundCard(*store->soundCards.get(update["inputSoundCardId"]));
      }
      // Did the output sound card change?
      if (update.contains("outputSoundCardId")) {
        setOutputSoundCard(*store->soundCards.get(update["outputSoundCardId"]));
      }
    }
    std::cout << "AudioProvider handle deviceChanged" << std::endl;
  });
  mixer_->attachHandlers(client);
}

AudioMixer &AudioProvider::getMixer() {
  return *mixer_;
}