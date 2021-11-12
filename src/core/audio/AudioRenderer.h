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

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(clientres);

template<class T>
class AudioRenderer {
 public:
  enum RoomSize {
    SMALL,
    MEDIUM,
    LARGE
  };

  explicit AudioRenderer(DigitalStage::Api::Client &client, bool autostart = false);

  /**
   * Init this audio renderer manually with the given sample rate, buffer size and room size;
   * Sample Rate has to be 41000, 48000 or 96000 and buffer size 128, 256, 512, or 1024.
   * Otherwise an exception will be thrown.
   * @param sample_rate 41000, 48000 or 96000
   * @param buffer_size 128, 256, 512, or 1024
   * @param HRTF resampling step, in degrees, default is 5
   */
  void start(unsigned int sample_rate,
             unsigned int buffer_size,
             AudioRenderer::RoomSize room_size = AudioRenderer::RoomSize::MEDIUM,
             int hrtf_resampling_steps = 5);

  void stop();

  void render(const std::string &audio_track_id, T *in, T *outLeft, T *outRight, std::size_t frame_size);

  void renderReverb(T *outLeft, T *outRight, std::size_t frame_size);

 private:
  static std::string to_string(AudioRenderer::RoomSize room_size);

  bool isValid(unsigned int sample_rate, unsigned int buffer_size);

  void autoInit(const DigitalStage::Types::Stage &stage, const DigitalStage::Types::SoundCard &sound_card);

  void attachHandlers(DigitalStage::Api::Client &client, bool autostart);
  static DigitalStage::Types::ThreeDimensionalProperties calculatePosition(const DigitalStage::Types::StageDevice &stage_device,
                                                                           const DigitalStage::Api::Store &store);
  static DigitalStage::Types::ThreeDimensionalProperties calculatePosition(const DigitalStage::Types::AudioTrack &audio_track,
                                                                           const DigitalStage::Api::Store &store);

  void setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position);
  void setAudioTrackPosition(const std::string &audio_track_id,
                             const DigitalStage::Types::ThreeDimensionalProperties &position);

  std::atomic<std::size_t> current_frame_size_;
  const DigitalStage::Api::Store &store_;
  std::atomic<bool> initialized_;
  cmrc::embedded_filesystem fs_;
  std::shared_ptr<Binaural::CCore> core_;
  std::shared_ptr<Binaural::CListener> listener_;
  std::shared_ptr<Binaural::CEnvironment> environment_;
  std::unordered_map<std::string, std::shared_ptr<Binaural::CSingleSourceDSP>> audio_tracks_;
  std::mutex mutex_;
};

#include "AudioRenderer.tpp"