//
// Created by Tobias Hegemann on 04.11.21.
//
#pragma once

#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Api/Store.h>
#include <DigitalStage/Types.h>
#include <string>
#include <optional>
#include <unordered_map>
#include <utility>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <HRTF/HRTFCereal.h>
#include <BRIR/BRIRCereal.h>
#include <BinauralSpatializer/3DTI_BinauralSpatializer.h>
#include <plog/Log.h>

template<typename T>
class AudioRenderer {
 public:
  AudioRenderer(DigitalStage::Api::Client &client);

  void render(const std::string &audio_track_id, T *in, T *outLeft, T *outRight, std::size_t frame_size);

 private:
  void attachHandlers(DigitalStage::Api::Client &client);
  DigitalStage::Types::ThreeDimensionalProperties calculatePosition(const DigitalStage::Types::StageDevice &stage_device,
                                                                    const DigitalStage::Api::Store &store);
  DigitalStage::Types::ThreeDimensionalProperties calculatePosition(const DigitalStage::Types::AudioTrack &audio_track,
                                                                    const DigitalStage::Api::Store &store);



  void init(int sample_rate, int buffer_size, const DigitalStage::Api::Store &store);
  void setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position);
  void setAudioTrackPosition(const std::string &audio_track_id,
                             const DigitalStage::Types::ThreeDimensionalProperties &position);

  std::shared_ptr<Binaural::CCore> core_;
  std::shared_ptr<Binaural::CListener> listener_;
  std::shared_ptr<Binaural::CEnvironment> environment_;
  std::unordered_map<std::string, std::shared_ptr<Binaural::CSingleSourceDSP>> audio_tracks_;
  std::recursive_mutex mutex_;
};

#include "AudioRenderer.tpp"