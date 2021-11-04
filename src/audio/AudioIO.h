//
// Created by Tobias Hegemann on 03.11.21.
//
#pragma once
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Types.h>
#include <sigslot/signal.hpp>
#include <unordered_map>
#include <string>

typedef std::unordered_map<unsigned int, std::string> ChannelMap;

class AudioIO {
 public:
  explicit AudioIO(DigitalStage::Api::Client &client);

  Pal::sigslot::signal<std::string /* audio_track_id */, const float * /* data */, std::size_t /* frame_count */>
      onChannel;
  Pal::sigslot::signal<float **/* data */, std::size_t /* num_channels */, std::size_t /* frame_count */> onPlayback;
 protected:
  void publishChannel(DigitalStage::Api::Client &client, int channel);
  void unPublishChannel(DigitalStage::Api::Client &client, int channel);
  void unPublishAll(DigitalStage::Api::Client &client);

  virtual std::vector<json> enumerateDevices(const DigitalStage::Api::Store &store) = 0;

  virtual void onChannelCallback(const std::string &audio_track_id,
                                 const float *data,
                                 std::size_t frame_count) = 0;
  virtual void onPlaybackCallback(float **data, std::size_t num_channels, std::size_t frame_count) = 0;

  virtual void setAudioDriver(const std::string &audio_driver) = 0;
  virtual void setInputSoundCard(const DigitalStage::Types::SoundCard &sound_card,
                                 bool start,
                                 DigitalStage::Api::Client &client) = 0;
  virtual void setOutputSoundCard(const DigitalStage::Types::SoundCard &sound_card, bool start) = 0;
  virtual void startSending() = 0;
  virtual void stopSending() = 0;
  virtual void startReceiving() = 0;
  virtual void stopReceiving() = 0;

 private:
  void attachHandlers(DigitalStage::Api::Client &client);

 protected:
  /**
   * Mapping of input channels.
   * Source channel <-> audio_track_id (online)
   */
  ChannelMap input_channel_mapping_;
};