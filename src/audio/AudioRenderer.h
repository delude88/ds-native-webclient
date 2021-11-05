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
#include <HRTF/HRTFFactory.h>
#include <HRTF/HRTFCereal.h>
#include <BRIR/BRIRFactory.h>
#include <BRIR/BRIRCereal.h>
#include <BinauralSpatializer/3DTI_BinauralSpatializer.h>

template<typename T>
class AudioRenderer {
 public:
  AudioRenderer(DigitalStage::Api::Client &client);

  std::optional<std::pair<T, T>> getRenderValue(const std::string &audio_track_id) const;

 private:
  std::unordered_map<std::string, std::pair<T, T>> render_map_;
};

#include "AudioRenderer.tpp"