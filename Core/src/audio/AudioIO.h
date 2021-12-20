//
// Created by Tobias Hegemann on 03.11.21.
//
#pragma once
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Types.h>
#include <sigslot/signal.hpp>
#include <unordered_map>
#include <string>
#include <mutex>
#include <array>
#include <memory>
#include <atomic>
#include <thread>

using ChannelMap = std::unordered_map<std::size_t, std::string>;

class AudioIO {
 public:
  explicit AudioIO(std::shared_ptr<DigitalStage::Api::Client> client);
  virtual ~AudioIO();

  sigslot::signal<
      /* audio_track_id */ std::string,
      /* input */ const float *,
      /* frame_count */ std::size_t>
      onCapture;
  sigslot::signal<
      /* output */ float **,
                   std::size_t /* num_output_channels */,
      /* frame_count */ std::size_t>
      onPlayback;
  sigslot::signal<
      /* audio_tracks */ const std::unordered_map<std::string /* audio_track_id */, float *> &,
      /* output */ float **,
      /* num_output_channels */ std::size_t,
      /* frame_count */ std::size_t>
      onDuplex;
 protected:
  /*
  virtual void onCaptureCallback(const std::string &audio_track_id,
                                 const float *data,
                                 std::size_t frame_count) = 0;
  virtual void onPlaybackCallback(float **output, std::size_t num_output_channels, std::size_t frame_count) = 0;
  virtual void onDuplexCallback(std::unordered_map<std::string, float *> audio_tracks,
                                float **output_channels,
                                std::size_t num_output_channels,
                                std::size_t frame_count
  ) = 0;*/
  virtual std::vector<json> enumerateDevices(const DigitalStage::Api::Store &store) = 0;
  virtual void setAudioDriver(const std::string &audio_driver) = 0;
  virtual void setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                                 bool start) = 0;
  virtual void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) = 0;
  virtual void startSending() = 0;
  virtual void stopSending() = 0;
  virtual void startReceiving() = 0;
  virtual void stopReceiving() = 0;
  void publishChannel(int channel);
  void unPublishChannel(int channel);
  void unPublishAll();

  std::mutex mutex_;
  /**
   * Indicates if a channel has been or is being published.
   */
  std::array<bool, 64> published_channels_;
  /**
   * Mapping of successful published input channels.
   * Source channel <-> audio_track_id (online)
   */
  ChannelMap input_channel_mapping_;
  std::shared_ptr<DigitalStage::Api::Client> client_;

 private:
  void attachHandlers();
  void watchDeviceUpdates();
  void stopWatchingDeviceUpdates();

  std::atomic<bool> watching_device_updates_;
  std::thread device_watcher_;
  std::atomic<std::size_t> num_devices_;
  std::shared_ptr<DigitalStage::Api::Client::Token> token_;
};