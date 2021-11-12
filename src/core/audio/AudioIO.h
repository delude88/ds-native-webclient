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

typedef std::unordered_map<unsigned int, std::string> ChannelMap;

class AudioIO {
 public:
  explicit AudioIO(DigitalStage::Api::Client &client);
  ~AudioIO();

  Pal::sigslot::signal<
      /* audio_track_id */ std::string,
      /* input */ const float *,
      /* frame_count */ std::size_t>
      onCapture;
  Pal::sigslot::signal<
      /* output */ float **,
                   std::size_t /* num_output_channels */,
      /* frame_count */ std::size_t>
      onPlayback;
  Pal::sigslot::signal<
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
                                 bool start,
                                 DigitalStage::Api::Client &client) = 0;
  virtual void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) = 0;
  virtual void startSending() = 0;
  virtual void stopSending() = 0;
  virtual void startReceiving() = 0;
  virtual void stopReceiving() = 0;
  void publishChannel(DigitalStage::Api::Client &client, int channel);
  void unPublishChannel(DigitalStage::Api::Client &client, int channel);
  void unPublishAll(DigitalStage::Api::Client &client);

  std::mutex mutex_;
  /**
   * Mapping of input channels.
   * Source channel <-> audio_track_id (online)
   */
  ChannelMap input_channel_mapping_;
  DigitalStage::Api::Client &client_;

 private:
  void attachHandlers();
  void watchDeviceUpdates();
  void stopWatchingDeviceUpdates();

  std::atomic<bool> watching_device_updates_;
  std::thread device_watcher_;
  std::atomic<std::size_t> num_devices_;
};