//
// Created by Tobias Hegemann on 03.11.21.
//

#include "AudioIO.h"
#include <utility>                       // for move, pair
#include <plog/Log.h>                    // for PLOGD, PLOGE
#include <chrono>                        // for seconds
//#include <map>                           // for operator!=
#include <nlohmann/detail/json_ref.hpp>  // for json_ref
//#include <nlohmann/json.hpp>             // for basic_json<>::object_t, basi...
#include <optional>                      // for optional
//#include <stdexcept>                     // for out_of_range
#include <string>                        // for operator==, basic_string
#include <type_traits>                   // for remove_reference<>::type
#include "DigitalStage/Api/Client.h"     // for Client::Token, Client
#include "DigitalStage/Api/Events.h"     // for REMOVE_AUDIO_TRACK, SET_SOUN...
#include "DigitalStage/Api/Store.h"      // for Store, StoreEntry
#include "DigitalStage/Types.h"          // for SoundCard, json, AudioTrack
#include "plog/Record.h"                 // for Record

AudioIO::AudioIO(std::shared_ptr<DigitalStage::Api::Client> client)
    : client_(std::move(client)),
      num_devices_(0),
      watching_device_updates_(false),
      published_channels_(),
      token_(std::make_shared<DigitalStage::Api::Client::Token>()) {
  attachHandlers();
}

void AudioIO::attachHandlers() {
  PLOGD << "Attaching handlers";
  client_->ready.connect([this](const DigitalStage::Api::Store *store) {
    PLOGD << "ready";
    auto local_device = store->getLocalDevice();
    if (local_device) {
      // Read all available sound cards (and update with existing ones from store if available)
      auto sound_cards = enumerateDevices(*store);
      num_devices_ = sound_cards.size();

      // Emit all sound cards using the set request (will overwrite existing ones)
      // For a reference of all events and payloads, see:
      // https://github.com/digital-stage/api-types/blob/main/src/ClientDeviceEvents.ts
      // https://github.com/digital-stage/api-types/blob/main/src/ClientDevicePayloads.ts
      for (auto &sound_card: sound_cards) {
        sound_card["online"] = true;
        client_->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, sound_card);
      }

      // Now we can set the audio driver, input and output sound cards IF they are set already
      if (local_device->audioDriver && !local_device->audioDriver->empty()) {
        setAudioDriver(*local_device->audioDriver);
        if (local_device->inputSoundCardId) {
          auto sound_card = store->soundCards.get(*local_device->inputSoundCardId);
          if (sound_card) {
            setInputSoundCard(*sound_card, local_device->sendAudio);
          } else {
            PLOGE << "No sound card found for selected input sound card";
          }
        }
        if (local_device->outputSoundCardId) {
          auto sound_card = store->soundCards.get(*local_device->outputSoundCardId);
          if (sound_card) {
            setOutputSoundCard(*sound_card, local_device->receiveAudio);
          } else {
            PLOGE << "No sound card found for selected output sound card";
          }
        }
      }
      // Also start live device update watcher
      watchDeviceUpdates();
    }
  }, token_);
  client_->disconnected.connect([this](bool /*normal_exit*/) {
    stopWatchingDeviceUpdates();
  }, token_);
  client_->audioDriverSelected.connect([this](std::optional<std::string> audio_driver,
                                              const DigitalStage::Api::Store *store) {
    if (store->isReady() && audio_driver && !audio_driver->empty()) {
      PLOGD << "audioDriverSelected";
      auto local_device = store->getLocalDevice();
      if (local_device) {
        setAudioDriver(*audio_driver);
        // Also re-init the input and output devices
        auto input_sound_card = store->getInputSoundCard();
        if (input_sound_card && input_sound_card->online) {
          setInputSoundCard(*input_sound_card, local_device->sendAudio);
        }
        auto output_sound_card = store->getInputSoundCard();
        if (output_sound_card && output_sound_card->online) {
          setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
        }
      } else {
        PLOGE << "Local device not available when ready";
      }
    }
  }, token_);
  client_->soundCardChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                           const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "soundCardChanged";
      auto local_device = store->getLocalDevice();
      if (local_device) {
        // Are the changes relevant, so that we should react here?
        if (update.contains("channels") ||
            update.contains("sampleRate") ||
            update.contains("bufferSize") ||
            update.contains("periodSize") ||
            update.contains("numPeriods") ||
            update.contains("online")) {
          // Is the sound card the current input or output sound card?
          auto input_sound_card = store->getInputSoundCard();
          if (input_sound_card && input_sound_card->_id == _id && input_sound_card->online) {
            setInputSoundCard(*input_sound_card, local_device->sendAudio);
          }
          auto output_sound_card = store->getOutputSoundCard();
          if (output_sound_card && output_sound_card->_id == _id && output_sound_card->online) {
            setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
          }
        }
      } else {
        PLOGE << "Local device not available when ready";
      }
    }
  }, token_);
  client_->deviceChanged.connect([this](const std::string &id, const nlohmann::json &update,
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
          if (input_sound_card && input_sound_card->online) {
            setInputSoundCard(*input_sound_card, local_device->sendAudio);
          }
          auto output_sound_card = store->getOutputSoundCard();
          if (output_sound_card && output_sound_card->online) {
            setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
          }
        } else if (update.contains("inputSoundCardId") || update.contains("outputSoundCardId")) {
          // This handles inputSoundCardId, outputSoundCardId INCLUDING sendAudio and receiveAudio
          // Did the input sound card change?
          if (update.contains("inputSoundCardId")) {
            auto input_sound_card = store->getInputSoundCard();
            if (input_sound_card) {
              if (input_sound_card->online) {
                setInputSoundCard(*input_sound_card, local_device->sendAudio);
              }
            } else {
              PLOGE << "Could not locate sound card for inputSoundCardId";
            }
          }
          // Did the output sound card change?
          if (update.contains("outputSoundCardId")) {
            auto output_sound_card = store->getInputSoundCard();
            if (output_sound_card) {
              if (output_sound_card->online) {
                setOutputSoundCard(*output_sound_card, local_device->receiveAudio);
              }
            } else {
              PLOGE << "Could not locate sound card for outputSoundCardId";
            }
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
  }, token_);
  client_->audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "audioTrackAdded";
      auto local_device_id = store->getLocalDeviceId();
      auto input_sound_card = store->getInputSoundCard();
      if (local_device_id && audio_track.deviceId == *local_device_id && audio_track.sourceChannel) {
        // This is a local track, so map the channels to this track
        mutex_.lock();
        if (input_channel_mapping_.count(*audio_track.sourceChannel) == 0U) {
          input_channel_mapping_[*audio_track.sourceChannel] = audio_track._id;
          PLOGD << "Added local track for channel " << *audio_track.sourceChannel;
        }
        mutex_.unlock();
      }
    }
  }, token_);
  client_->audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "audioTrackRemoved";
      auto local_device_id = store->getLocalDeviceId();
      if (local_device_id && audio_track.deviceId == *local_device_id && audio_track.sourceChannel) {
        mutex_.lock();
        if (input_channel_mapping_.count(*audio_track.sourceChannel) != 0U) {
          input_channel_mapping_.erase(*audio_track.sourceChannel);
          PLOGD << "Removed local track for channel " << *audio_track.sourceChannel;
        }
        mutex_.unlock();
      }
    }
  }, token_);
}

void AudioIO::publishChannel(int channel) {
  if (!published_channels_[channel] && (input_channel_mapping_.count(channel) == 0U)) {
    published_channels_[channel] = true;
    PLOGD << "publishChannel " << channel;
    auto *store = client_->getStore();
    auto stage_id = store->getStageId();
    nlohmann::json payload {
        {"stageId", *stage_id},
        {"type", "native"},
        {"sourceChannel", channel}
    };
    PLOGD << "Publishing audio track";
    client_->send(DigitalStage::Api::SendEvents::CREATE_AUDIO_TRACK,
                  payload);
  } else {
    PLOGD << "publishChannel " << channel << " NOT ";
  }
}

void AudioIO::unPublishChannel(int channel) {
  PLOGD << "unPublishChannel " << channel;
  if (input_channel_mapping_.count(channel) != 0U) {
    published_channels_[channel] = false;
    client_->send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, input_channel_mapping_[channel]);
  } else {
    PLOGD << "unPublishChannel " << channel << " NOT ";
  }
}

void AudioIO::unPublishAll() {
  for (const auto &item: input_channel_mapping_) {
    PLOGD << "unPublishAll";
    published_channels_[item.first] = false;
    client_->send(DigitalStage::Api::SendEvents::REMOVE_AUDIO_TRACK, item.second);
  }
}
AudioIO::~AudioIO() {
  token_ = nullptr; // Don't listen to the unpublished event
  PLOGD << "Unpublish all";
  unPublishAll();
  PLOGD << "Stopping device watcher";
  stopWatchingDeviceUpdates();
  PLOGD << "Destructor finished";
}
void AudioIO::watchDeviceUpdates() {
  if (!watching_device_updates_) {
    watching_device_updates_ = true;
    device_watcher_ = std::thread([this]() {
      while (watching_device_updates_) {
        auto sound_cards = enumerateDevices(*client_->getStore());
        if (num_devices_ != sound_cards.size()) {
          for (auto &sound_card: sound_cards) {
            sound_card["online"] = true;
            client_->send(DigitalStage::Api::SendEvents::SET_SOUND_CARD, sound_card);
          }
        }
        num_devices_ = sound_cards.size();
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        if (watching_device_updates_)
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      PLOGD << "Stopping AudioIO watcher thread";
    });
  }
}
void AudioIO::stopWatchingDeviceUpdates() {
  watching_device_updates_ = false;
  if (device_watcher_.joinable())
    device_watcher_.join();
}
