//
// Created by Tobias Hegemann on 26.10.21.
//

#include "Client.h"

#include <utility>
#include "utils/conversion.h"

// AudioIO engine
#ifdef USE_RT_AUDIO
#include "audio/RtAudioIO.h"
#else
// Miniaudio
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio/MiniAudioIO.h"
#endif

Client::Client(std::shared_ptr<DigitalStage::Api::Client> api_client) :
    api_client_(std::move(api_client)),
    audio_renderer_(std::make_unique<AudioRenderer<float>>(api_client_, true)),
    connection_service_(std::make_unique<ConnectionService>(api_client_)),
    receiver_buffer_(RECEIVER_BUFFER) {
#ifdef USE_RT_AUDIO
  audio_io_ = std::make_unique<RtAudioIO>(api_client_);
#else
  audio_io_ = std::make_unique<MiniAudioIO>(api_client_);
#endif

  connection_service_->onData.connect([this](const std::string &audio_track_id, std::vector<std::byte> data) {
    if (channels_.count(audio_track_id) == 0) {
      std::unique_lock lock(channels_mutex_);
#ifdef USE_CIRCULAR_QUEUE
      channels_[audio_track_id] = std::make_shared<CircularQueue<float>>(receiver_buffer_);
#else
      channels_[audio_track_id] = std::make_shared<RingBuffer<float>>(receiver_buffer_);
#endif
      lock.unlock();
    }
    auto values_size = data.size() / 4;
    auto* values = new float[values_size];
    deserialize(data.data(), data.size(), values);
    std::shared_lock lock(channels_mutex_);
#ifdef USE_CIRCULAR_QUEUE

#else
    for (int v = 0; v < values_size; v++) {
      channels_[audio_track_id]->put(values[v]);
    }
#endif
    delete [] values;
    lock.unlock();  // May be useless here
  });
  attachHandlers();
  attachAudioHandlers();

  is_ready_ = true;
}

Client::~Client() {
  is_ready_ = false;
  audio_io_.reset();
  connection_service_.reset();
  audio_renderer_.reset();
  api_client_.reset();
}

void Client::onCaptureCallback(const std::string &audio_track_id, const float *data, const std::size_t frame_count) {
  // Write to channels
  if (channels_.count(audio_track_id) == 0) {
    std::unique_lock lock(channels_mutex_);
#ifdef USE_CIRCULAR_QUEUE
    channels_[audio_track_id] = std::make_shared<CircularQueue<float>>(receiver_buffer_);
#else
    channels_[audio_track_id] = std::make_shared<RingBuffer<float>>(receiver_buffer_);
#endif
    lock.unlock();
  }
  std::shared_lock lock(channels_mutex_);
#ifdef USE_CIRCULAR_QUEUE
  channels_[audio_track_id]->enqueue_many(data, 0, frame_count);
#else
  for (int f = 0; f < frame_count; f++) {
    channels_[audio_track_id]->put(data[f]);
  }
#endif
  lock.unlock();

  // Send to webRTC
  connection_service_->broadcastFloats(audio_track_id, data, frame_count);
}
void Client::onPlaybackCallback(float *out[], std::size_t num_output_channels, const std::size_t frame_count) {
  auto *left = new float[frame_count];
  auto *right = new float[frame_count];
  memset(left, 0, frame_count * sizeof(float));
  memset(right, 0, frame_count * sizeof(float));

  if (audio_renderer_) {
    for (const auto &item: channels_) {
      if (item.second) {
        auto *buf = static_cast<float *>(malloc(frame_count * sizeof(float)));
#ifdef USE_CIRCULAR_QUEUE
        for (int f = 0; f < frame_count; f++) {
        buf[f] = item.second->dequeue();
      }
#else
        for (int f = 0; f < frame_count; f++) {
          buf[f] = item.second->get();
        }
#endif
        audio_renderer_->render(item.first, buf, left, right, frame_count);
        free(buf);
      }
    }
    audio_renderer_->renderReverb(left, right, frame_count);
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

  //delete [] left;
  //delete [] right;
}
void Client::attachHandlers() {
  api_client_->ready.connect([this](const DigitalStage::Api::Store *store) {
    PLOGD << "ready";
    auto local_device = store->getLocalDevice();
    if (local_device) {
      changeReceiverSize(local_device->buffer);
    }
  });
  api_client_->localDeviceChanged.connect([this](const std::string &, const nlohmann::json &update,
                                                 const DigitalStage::Api::Store * /*store*/) {
    if (update.contains("buffer")) {
      changeReceiverSize(update["buffer"]);
    }
  });
}
void Client::onDuplexCallback(const std::unordered_map<std::string, float *> &audio_tracks,
                              float **out,
                              std::size_t num_output_channels,
                              std::size_t frame_count) {
  if(!is_ready_)
    return;

  // Mix to L / R
  auto *left = new float[frame_count];
  auto *right = new float[frame_count];
  memset(left, 0, frame_count * sizeof(float));
  memset(right, 0, frame_count * sizeof(float));

  // Forward local capture stream
  for (const auto &item: audio_tracks) {
    if (item.second) {
      // Send to webRTC
      connection_service_->broadcastFloats(item.first, item.second, frame_count);

      audio_renderer_->render(item.first, item.second, left, right, frame_count);
    } else {
      PLOGE << "Audio track item is null";
    }
  }

  // Forward remote streams
  for (const auto &item: channels_) {
    if (item.second) {
      auto *buf = static_cast<float *>(malloc(frame_count * sizeof(float)));
#ifdef USE_CIRCULAR_QUEUE
      for (int f = 0; f < frame_count; f++) {
        buf[f] = item.second->dequeue();
      }
#else
      for (int f = 0; f < frame_count; f++) {
        buf[f] = item.second->get();
      }
#endif
      audio_renderer_->render(item.first, buf, left, right, frame_count);
      free(buf);
    } else {
      std::cerr << "Channel item is null" << std::endl;
    }
  }

  audio_renderer_->renderReverb(left, right, frame_count);

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

  //delete [] left;
  //delete [] right;
}
void Client::attachAudioHandlers() {
  audio_io_->onPlayback.connect(&Client::onPlaybackCallback, this);
  audio_io_->onCapture.connect(&Client::onCaptureCallback, this);
  audio_io_->onDuplex.connect(&Client::onDuplexCallback, this);
}
void Client::changeReceiverSize(unsigned int receiver_buffer) {
  PLOGD << "changeReceiverSize to" << receiver_buffer;
  if (receiver_buffer > 0 && receiver_buffer_ != receiver_buffer) {
    std::unique_lock lock(channels_mutex_);
    receiver_buffer_ = receiver_buffer;
    // Recreate buffers with
    for (auto &item: channels_) {
#ifdef USE_CIRCULAR_QUEUE
      item.second = std::make_shared<CircularQueue<float>>(receiver_buffer_);
#else
      item.second = std::make_shared<RingBuffer<float>>(receiver_buffer_);
#endif
    }
    lock.unlock();
  }
}