//
// Created by Tobias Hegemann on 26.10.21.
//

#include "Client.h"
#include "utils/conversion.h"

Client::Client(DigitalStage::Api::Client &client, AudioIO &audio_io)
    :
    connection_service_(client),
    audio_mixer_(client),
    audio_renderer_(client),
    receiver_buffer_(RECEIVER_BUFFER) {
  connection_service_.onData.connect([this](const std::string &audio_track_id, std::vector<std::byte> data) {
    if (channels_.count(audio_track_id) == 0) {
      std::unique_lock lock(channels_mutex_);
      channels_[audio_track_id] = std::make_shared<RingBuffer<float>>(receiver_buffer_);
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
  attachHandlers(client);
  attachAudioHandlers(audio_io);
}

Client::~Client() {
}

void Client::onCaptureCallback(const std::string &audio_track_id, const float *data, std::size_t frame_count) {
  // Write to channels
  if (channels_.count(audio_track_id) == 0) {
    std::unique_lock lock(channels_mutex_);
    channels_[audio_track_id] = std::make_shared<RingBuffer<float>>(receiver_buffer_);
    lock.unlock();
  }
  std::shared_lock lock(channels_mutex_);
  for (int f = 0; f < frame_count; f++) {
    channels_[audio_track_id]->put(data[f]);
  }
  lock.unlock();

  // Send to webRTC
  connection_service_.broadcast(audio_track_id, data, frame_count);
}
void Client::onPlaybackCallback(float *out[], std::size_t num_output_channels, const std::size_t frame_count) {
  float left[frame_count];
  float right[frame_count];
  memset(left, 0, frame_count * sizeof(float));
  memset(right, 0, frame_count * sizeof(float));

  for (const auto &item: channels_) {
    auto gain = audio_mixer_.getGain(item.first);
    for (int frame = 0; frame < frame_count; frame++) {
      // Read next float from channel
      float f = item.second->get();
      // And apply gain
      if (gain) {
        f *= gain->second ? 0 : gain->first;
      }

      // Now mix with existing
      left[frame] += f;
      right[frame] += f;
    }
  }

  if (num_output_channels % 2 == 0) {
    // Use stereo for all
    for (int output_channel = 0; output_channel < num_output_channels; output_channel++) {
      if (output_channel % 2 == 0) {
        out[output_channel] = &left[0];
      } else {
        out[output_channel] = &right[0];
      }
    }
  } else {
    // Use mono for all
    for (int output_channel = 0; output_channel < num_output_channels; output_channel++) {
      out[output_channel] = &left[0];
    }
  }
}
void Client::attachHandlers(DigitalStage::Api::Client &client) {
  client.ready.connect([this](const DigitalStage::Api::Store *store) {
    auto local_device = store->getLocalDevice();
    if (local_device) {
      changeReceiverSize(local_device->buffer);
    }
  });
  client.localDeviceChanged.connect([this](const std::string &, const nlohmann::json &update,
                                           const DigitalStage::Api::Store *store) {
    if (update.contains("buffer")) {
      changeReceiverSize(update["buffer"]);
    }
  });
}
void Client::onDuplexCallback(const std::unordered_map<std::string, float *> &audio_tracks,
                              float **out,
                              std::size_t num_output_channels,
                              std::size_t frame_count) {
  // Mix to L / R
  float left[frame_count];
  float right[frame_count];
  memset(left, 0, frame_count * sizeof(float));
  memset(right, 0, frame_count * sizeof(float));

  // Forward local capture stream
  for (const auto &item: audio_tracks) {
    if (item.second) {
      // Send to webRTC
      connection_service_.broadcast(item.first, item.second, frame_count);

      audio_renderer_.render(item.first, item.second, left, right, frame_count);
    } else {
      std::cerr << "Audio track item is null" << std::endl;
    }
  }

  // Forward remote streams
  for (const auto &item: channels_) {
    if (item.second) {
      auto buf = (float *) malloc(frame_count * sizeof(float));
      for (int f = 0; f < frame_count; f++) {
        buf[f] = item.second->get();
      }
      audio_renderer_.render(item.first, buf, left, right, frame_count);
      free(buf);
    } else {
      std::cerr << "Channel item is null" << std::endl;
    }
  }

  if (num_output_channels % 2 == 0) {
    // Use stereo for all
    for (int output_channel = 0; output_channel < num_output_channels; output_channel++) {
      // Just mixdown to mono for now.
      if (output_channel % 2 == 0) {
        out[output_channel] = &left[0];
      } else {
        out[output_channel] = &right[0];
      }
    }
  } else {
    // Use mono for all
    for (int output_channel = 0; output_channel < num_output_channels; output_channel++) {
      out[output_channel] = &left[0];
    }
  }
}
void Client::attachAudioHandlers(AudioIO &audio_io) {
  audio_io.onPlayback.connect(&Client::onPlaybackCallback, this);
  audio_io.onCapture.connect(&Client::onCaptureCallback, this);
  audio_io.onDuplex.connect(&Client::onDuplexCallback, this);
}
void Client::changeReceiverSize(unsigned int receiver_buffer) {
  PLOGD << "changeReceiverSize to" << receiver_buffer;
  if (receiver_buffer > 0 && receiver_buffer_ != receiver_buffer) {
    std::unique_lock lock(channels_mutex_);
    receiver_buffer_ = receiver_buffer;
    // Recreate buffers with
    for (auto &item: channels_) {
      item.second = std::make_shared<RingBuffer<float>>(receiver_buffer_);
    }
    lock.unlock();
  }
}
