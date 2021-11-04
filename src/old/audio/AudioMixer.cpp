//
// Created by Tobias Hegemann on 25.10.21.
//

#include "AudioMixer.h"
#include "../../new/AudioMixer.h"
#include "../../utils/thread.h"

AudioMixer::AudioMixer() {
}

AudioMixer::~AudioMixer() {
}

void AudioMixer::attachHandlers(DigitalStage::Api::Client &client) {
  std::cout << "AudioMixer::attachHandlers" << std::endl;
  client.ready.connect([this, &client](const DigitalStage::Api::Store *) {
    handleInputSoundCardChange(client);
  });
  client.inputSoundCardSelected.connect([this, &client](const std::optional<std::string> &,
                                                        const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      handleInputSoundCardChange(client);
    }
  });
  client.inputSoundCardChanged.connect([this, &client](const std::string &, const nlohmann::json &,
                                                       const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      handleInputSoundCardChange(client);
    }
  });
  client.audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        std::cout << "Adding local track for channel " << *audio_track.sourceChannel << std::endl;
        local_channel_mapping_[*audio_track.sourceChannel] = audio_track._id;
      }
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        std::cout << "Removing local track for channel " << *audio_track.sourceChannel << std::endl;
        local_channel_mapping_.erase(*audio_track.sourceChannel);
      }
    }
  });
}

void AudioMixer::write(const std::string &audioTrackId, const float value) {
  if (channels_.count(audioTrackId) == 0) {
    channels_[audioTrackId] = std::make_unique<RingBuffer<float >>(BUFFER_SIZE_IN_FRAMES);
  }
  channels_[audioTrackId]->put(value);
}

void AudioMixer::writeLocal(const float *pInput, unsigned int numChannels, unsigned int frameSize) {
  for (const auto &item: local_channel_mapping_) {
    float *arrPtr[frameSize];
    for (unsigned int frame = 0; frame < frameSize; frame++) {
      float f = pInput[frame * numChannels + item.first];
      arrPtr[frame] = &f;
      write(item.second, f);
    }
    // TODO: Pass to a different thread
    onLocalChannel(item.second, *arrPtr, frameSize);
  }
}

void AudioMixer::handleInputSoundCardChange(DigitalStage::Api::Client &client) {
  std::cout << "AudioMixer::handleInputSoundCardChange" << std::endl;
  // Un-publish all existing audio tracks and clear map
  for (const auto &item: local_channel_mapping_) {
    std::cout << "Un-publishing audio track" << std::endl;
    client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item.second);
  }
  auto store = client.getStore();
  auto inputSoundCard = store->getInputSoundCard();
  auto stageId = store->getStageId();
  if (inputSoundCard && stageId) {
    // Request a new audio track for all channels if not existing yet
    for (size_t i = 0; i < inputSoundCard->channels.size(); i++) {
      nlohmann::json payload;
      payload["stageId"] = *stageId;
      payload["type"] = "native";
      payload["sourceChannel"] = i;
      std::cout << "Publishing audio track" << std::endl;
      client.send(DigitalStage::Api::SendEvents::CREATE_AUDIO_TRACK,
                  payload);
    }
  }
}
void AudioMixer::mixDown(float *pOutput, unsigned int numChannels, unsigned int frameSize) {
  for (unsigned int frame = 0; frame < frameSize; frame++) {
    float f = .0;
    // MIXING FROM MONO TO MONO HERE
    for (const auto &item: channels_) {
      /*if (item.second->empty()) {
        std::cout << "Buffer underrun" << std::endl;
      }*/
      f += item.second->get();
    }
    for (unsigned int channel = 0; channel < numChannels; channel++) {
      pOutput[frame * numChannels + channel] = f;
    }
  }
}
