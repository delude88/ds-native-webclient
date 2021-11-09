//
// Created by Tobias Hegemann on 03.11.21.
//

#include "AudioIO.h"
#include <plog/Log.h>

AudioIO::AudioIO(DigitalStage::Api::Client &client) {
  attachHandlers(client);
}

void AudioIO::attachHandlers(DigitalStage::Api::Client &client) {
  client.ready.connect([this, &client](const DigitalStage::Api::Store *store) {
    PLOGD << "ready";
    auto local_device = store->getLocalDevice();
    if (local_device) {
      // Read all available sound cards (and update with existing ones from store if available)
      auto sound_cards = enumerateDevices(*store);

      // Emit all sound cards using the set request (will overwrite exsiting ones)
      // For a reference of all events and payloads, see:
      // https://github.com/digital-stage/api-types/blob/main/src/ClientDeviceEvents.ts
      // https://github.com/digital-stage/api-types/blob/main/src/ClientDevicePayloads.ts
      for (auto &sound_card: sound_cards) {
//PLOGD << "Label:" << sound_card["label"].dump();
//        PLOGD << "SC:" << sound_card.dump();
        client.send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, sound_card);
      }

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
      PLOGD << "audioDriverSelected";
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
      PLOGD << "soundCardChanged";
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
      PLOGD << "deviceChanged";
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
      PLOGD << "audioTrackAdded";
      auto localDeviceId = store->getLocalDeviceId();
      auto inputSoundCard = store->getInputSoundCard();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        mutex_.lock();
        if (!input_channel_mapping_.count(*audio_track.sourceChannel)) {
          input_channel_mapping_[*audio_track.sourceChannel] = audio_track._id;
          PLOGD << "Added local track for channel " << *audio_track.sourceChannel;
        }
        mutex_.unlock();
      }
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "audioTrackRemoved";
      auto localDeviceId = store->getLocalDeviceId();
      if (localDeviceId && audio_track.deviceId == *localDeviceId && audio_track.sourceChannel) {
        mutex_.lock();
        if (input_channel_mapping_.count(*audio_track.sourceChannel)) {
          input_channel_mapping_.erase(*audio_track.sourceChannel);
          PLOGD << "Removed local track for channel " << *audio_track.sourceChannel;
        }
        mutex_.unlock();
      }
    }
  });
}

void AudioIO::publishChannel(DigitalStage::Api::Client &client, int channel) {
  if (!input_channel_mapping_.count(channel)) {
    PLOGD << "publishChannel " << channel;
    auto store = client.getStore();
    auto stageId = store->getStageId();
    nlohmann::json payload;
    payload["stageId"] = *stageId;
    payload["type"] = "native";
    payload["sourceChannel"] = channel;
    PLOGD << "Publishing audio track";
    client.send(DigitalStage::Api::SendEvents::CREATE_AUDIO_TRACK,
                payload);
  }
}

void AudioIO::unPublishChannel(DigitalStage::Api::Client &client, int channel) {
  PLOGD << "unPublishChannel " << channel;
  if (input_channel_mapping_.count(channel))
    client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, input_channel_mapping_[channel]);
}

void AudioIO::unPublishAll(DigitalStage::Api::Client &client) {
  for (const auto &item: input_channel_mapping_) {
    PLOGD << "unPublishAll";
    client.send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item.second);
  }
}