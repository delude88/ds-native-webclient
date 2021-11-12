//
// Created by Tobias Hegemann on 04.11.21.
//
#pragma once

#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Api/Store.h>
#include <DigitalStage/Types.h>
#include <string>
#include <unordered_map>
#include <optional>
#include <utility>
#include <sigslot/signal.hpp>
#include <plog/Log.h>

template<class T>
class AudioMixer {
 public:
  explicit AudioMixer(DigitalStage::Api::Client &client);

  void applyGain(const std::string &audio_track_id, T *data, std::size_t frame_count);
  T applyGain(const std::string &audio_track_id, T data);
  std::optional<std::pair<T, bool>> getGain(const std::string &audio_track_id) const;

  Pal::sigslot::signal<std::string, std::pair<T, bool>> onGainChanged;
 private:
  void attachHandlers(DigitalStage::Api::Client &client);
  std::pair<T, bool> calculateVolume(const DigitalStage::Types::AudioTrack &audio_track,
                                     const DigitalStage::Api::Store &store);

  std::unordered_map<std::string, std::pair<T, bool>> volume_map_;
};

#include "AudioMixer.tpp"