//
// Created by Tobias Hegemann on 26.10.21.
//

#include "Client.h"
#include "utils/conversion.h"

Client::Client(DigitalStage::Api::Client &client)
    : MiniAudioIO(client), connection_service_(client), output_buffer_(5000) {
  connection_service_.onData.connect([this](const std::string &audio_track_id, std::vector<std::byte> data) {
    if (channels_.count(audio_track_id) == 0) {
      std::unique_lock lock(channels_mutex_);
      channels_[audio_track_id] = std::make_shared<RingBuffer2<float>>(output_buffer_);
      lock.unlock();
    }
    auto values_size = data.size() / 4;
    float values[values_size];
    deserialize(data.data(), data.size(), values);
    std::shared_lock lock(channels_mutex_);
    //channels_[audio_track_id]->write(values, values_size);
    for (int v = 0; v < values_size; v++) {
      channels_[audio_track_id]->put(values[v]);
    }
    lock.unlock();  // May be useless here
  });
  client.ready.connect([this](const DigitalStage::Api::Store *store) {
    auto input_sound_card = store->getInputSoundCard();
    if (input_sound_card) {
      output_buffer_ = input_sound_card->outputBuffer * 1000;
    }
  });
  client.inputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                              const DigitalStage::Api::Store *) {
    if (update.contains("outputBuffer")) {
      output_buffer_ = update["outputBuffer"].get<unsigned int>() * 1000;
    }
  });
  client.inputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                               const DigitalStage::Api::Store *store) {
    auto input_sound_card = store->getInputSoundCard();
    if (input_sound_card) {
      output_buffer_ = input_sound_card->outputBuffer * 1000;
    }
  });
}

Client::~Client() {
}

void Client::onChannelCallback(const std::string &audio_track_id, const float *data, std::size_t frame_count) {
  // Write to channels
  if (channels_.count(audio_track_id) == 0) {
    std::unique_lock lock(channels_mutex_);
    channels_[audio_track_id] = std::make_shared<RingBuffer2<float>>(output_buffer_);
    lock.unlock();
  }
  std::shared_lock lock(channels_mutex_);
  //channels_[audio_track_id]->pwriteut(data, frame_count);
  for (int f = 0; f < frame_count; f++) {
    channels_[audio_track_id]->put(data[f]);
  }
  lock.unlock();

  // Send to webRTC
  connection_service_.broadcast(audio_track_id, data, frame_count);
}
void Client::onPlaybackCallback(float *data[], std::size_t num_output_channels, std::size_t frame_count) {
  for (int frame = 0; frame < frame_count; frame++) {
    for (const auto &item: channels_) {
      float f = item.second->get();
      //TODO: Use item.first as auto_track_id and mix the values here down

      for (int output_channel = 0; output_channel < num_output_channels; output_channel++) {
        // Just mixdown to mono for now. TODO: Replace with logic (l,r when num_output_channels % 2 == 0, otherwise l/mono for all)
        data[output_channel][frame] = f;
      }
    }
  }
}
