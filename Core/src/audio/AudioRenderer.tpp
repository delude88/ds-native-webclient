//
// Created by Tobias Hegemann on 04.11.21.
//

#include "../utils/CMRCFileBuffer.h"
#include <DigitalStage/Audio/AudioMixer.h>

template<class T>
AudioRenderer<T>::AudioRenderer(std::shared_ptr<DigitalStage::Api::Client> client, bool autostart)
    : fs_(cmrc::clientres::get_filesystem()),
      client_(std::move(client)),
      audio_mixer_(std::make_unique<DigitalStage::Audio::AudioMixer<float>>(client_)),
      initialized_(false),
      current_frame_size_(0),
      token_(std::make_shared<DigitalStage::Api::Client::Token>()) {
  PLOGD << "AudioRenderer";
  ERRORHANDLER3DTI.SetVerbosityMode(VERBOSITY_MODE_ONLYERRORS);
  ERRORHANDLER3DTI.SetErrorLogStream(&std::cerr, true);
  ERRORHANDLER3DTI.SetAssertMode(ASSERT_MODE_CONTINUE);

  attachHandlers(autostart);
}

template<class T>
AudioRenderer<T>::~AudioRenderer() {
  initialized_ = false;
  PLOGD << "Destructed";
}

template<class T>
std::string AudioRenderer<T>::to_string(AudioRenderer::RoomSize room_size) {
  switch (room_size) {
    case AudioRenderer::RoomSize::kSmall:return "small";
    case AudioRenderer::RoomSize::kLarge:return "large";
    default:return "medium";
  }
}

template<class T>
bool AudioRenderer<T>::isValid(unsigned int sample_rate, unsigned int buffer_size) {
  std::string brir_path("3DTI_BRIR_small_" + std::to_string(sample_rate) + "Hz.3dti-brir");
  std::string hrtf_path
      ("3DTI_HRTF_IRC1008_" + std::to_string(buffer_size) + "s_" + std::to_string(sample_rate) + "Hz.3dti-hrtf");
  return fs_.is_file(brir_path) && fs_.is_file(hrtf_path);
}

template<class T>
void AudioRenderer<T>::start(unsigned int sample_rate,
                             unsigned int buffer_size,
                             AudioRenderer::RoomSize room_size,
                             int hrtf_resampling_steps) {
  PLOGD << "start(sample_rate=" << sample_rate << ", buffer_size=" << buffer_size << ",...)";
  // Load HRTF and BRIR from compiled resources
  if (!isValid(sample_rate, buffer_size)) {
    throw std::invalid_argument(
        "Invalid sample rate and/or buffer size. Hint: use sample rates of 41000, 48000 or 96000 and buffer sizes of 128, 256, 512 or 1024");
  }

  std::lock_guard<std::mutex> guard{mutex_};
  initialized_ = false;
  std::string brir_path("3DTI_BRIR_" + to_string(room_size) + "_" + std::to_string(sample_rate) + "Hz.3dti-brir");
  if (!fs_.is_file(brir_path)) {
    throw std::runtime_error(
        "BRIR for room size " + to_string(room_size) + " not available. Check your build settings. Was looking for "
            + brir_path);
  }
  auto brir_file = fs_.open(brir_path);
  CMRCFileBuffer brir_file_buffer(brir_file);
  std::istream brir_stream(&brir_file_buffer);
  std::string hrtf_path
      ("3DTI_HRTF_IRC1008_" + std::to_string(buffer_size) + "s_" + std::to_string(sample_rate) + "Hz.3dti-hrtf");
  auto hrtf_file = fs_.open(hrtf_path);
  CMRCFileBuffer hrtf_file_buffer(hrtf_file);
  std::istream hrtf_stream(&hrtf_file_buffer);
  audio_tracks_.clear();
  // Init core
  core_ = std::make_shared<Binaural::CCore>(Common::TAudioStateStruct{static_cast<int>(sample_rate),
                                                                      static_cast<int>(buffer_size)},
                                            hrtf_resampling_steps);
  current_frame_size_ = buffer_size;
  // Init environment (used for reverb)
  environment_ = core_->CreateEnvironment();
  environment_->SetReverberationOrder(TReverberationOrder::BIDIMENSIONAL);
  if (!BRIR::CreateFrom3dtiStream(brir_stream, environment_)) {
    throw std::runtime_error("Could not create BRIR");
  }
  if (!environment_->GetBRIR()->IsBRIRready()) {
    throw std::runtime_error("Created BRIR but it is not ready");
  }
  PLOGI << "Loaded BRIR " << brir_path;

  listener_ = core_->CreateListener();
  listener_->DisableCustomizedITD();

  if (!HRTF::CreateFrom3dtiStream(hrtf_stream, listener_)) {
    throw std::runtime_error("Could not create HRTF");
  }
  auto *hrtf = listener_->GetHRTF();
  if(!hrtf) {
    throw std::runtime_error("HRTF has not been created");
  }
  if (!hrtf->IsHRTFLoaded()) {
    throw std::runtime_error("Created HRTF but it is not loaded");
  }
  PLOGI << "Loaded HRTF " << hrtf_path;

  // Listener
  auto store_ptr = client_->getStore();
  if(store_ptr.expired()) {
    return;
  }
  auto store = store_ptr.lock();

  auto stage_member_id = store->getStageMemberId();
  if (!stage_member_id) {
    throw std::runtime_error("Local stage member not available");
  }
  auto stage_member = store->stageMembers.get(*stage_member_id);
  setListenerPosition(calculatePosition(*stage_member, store));

  // Other remote audio tracks
  for (const auto &audio_track: store->audioTracks.getAll()) {
#ifdef USE_ONLY_NATIVE_DEVICES
    if (audio_track.type == "native") {
      PLOGI << "Found an existing native audio_track";
#endif
    audio_tracks_[audio_track._id] = core_->CreateSingleSourceDSP();
    audio_tracks_[audio_track._id]->SetSpatializationMode(Binaural::TSpatializationMode::HighQuality);
    audio_tracks_[audio_track._id]->DisableNearFieldEffect();
    audio_tracks_[audio_track._id]->EnableAnechoicProcess();
    audio_tracks_[audio_track._id]->EnableDistanceAttenuationAnechoic();
    setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
#ifdef USE_ONLY_NATIVE_DEVICES
    } else {
      PLOGW << "Ignoring existing but non-native audio track";
    }
#endif
  }
  initialized_ = true;
  PLOGI << "Started audio renderer";
}

template<class T>
void AudioRenderer<T>::stop() {
  const std::lock_guard<std::mutex> lock(mutex_);
  initialized_ = false;
  PLOGI << "Stopped audio renderer";
}

template<class T>
void AudioRenderer<T>::autoInit(const DigitalStage::Types::Stage &stage,
                                const DigitalStage::Types::SoundCard &sound_card) {
  if (isValid(sound_card.sampleRate, sound_card.bufferSize)) {
    // Get room size
    auto room_volume = stage.width * stage.length * stage.height;
    auto room_size = AudioRenderer::RoomSize::kSmall;
    if (room_volume > 10000) {
      room_size = AudioRenderer::RoomSize::kLarge;
    } else if (room_volume > 1000) {
      room_size = AudioRenderer::RoomSize::kMedium;
    }
    try {
      start(sound_card.sampleRate, sound_card.bufferSize, room_size);
    } catch (std::exception &e) {
      PLOGE << "Could not auto start: " << e.what();
    }
  } else {
    PLOGW << "Current values of output sound card are not supported by 3D audio engine";
  }
}

template<class T>
void AudioRenderer<T>::attachHandlers(bool autostart) {
  // Automatically init means listening to the selected sound card and init each time the sampleRate and bufferSize seems valid
  if (autostart) {
    client_->ready.connect([this](const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
      if(store_ptr.expired()) {
        return;
      }
      auto store = store_ptr.lock();
      auto stage_id = store->getStageId();
      if (stage_id) {
        PLOGD << "ready";
        auto stage = store->stages.get(*stage_id);
        auto output_sound_card = store->getOutputSoundCard();
        if (stage && output_sound_card && output_sound_card->online) {
          autoInit(*stage, *output_sound_card);
        }
      }
    }, token_);
    client_->stageJoined.connect([this](const DigitalStage::Types::ID_TYPE &stage_id, const optional<DigitalStage::Types::ID_TYPE> &,
                                        const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
      if(store_ptr.expired()) {
        return;
      }
      auto store = store_ptr.lock();
      if (store->isReady()) {
        PLOGD << "stageJoined";
        auto stage = store->stages.get(stage_id);
        auto output_sound_card = store->getOutputSoundCard();
        if (stage && output_sound_card && output_sound_card->online) {
          autoInit(*stage, *output_sound_card);
        }
      }
    }, token_);
    client_->stageLeft.connect([this](const std::weak_ptr<DigitalStage::Api::Store>& /*store*/) {
      stop();
    }, token_);
    client_->outputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                                   const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
      if(store_ptr.expired()) {
        return;
      }
      auto store = store_ptr.lock();
      if (store->isReady() &&
          (update.contains("sampleRate") || update.contains("bufferSize") || update.contains("online"))) {
        auto stage_id = store->getStageId();
        if (stage_id) {
          auto stage = store->stages.get(*stage_id);
          auto output_sound_card = store->getOutputSoundCard();
          if (stage && output_sound_card && output_sound_card->online) {
            autoInit(*stage, *output_sound_card);
          }
        }
      }
    }, token_);
    client_->outputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                                    const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
      if(store_ptr.expired()) {
        return;
      }
      auto store = store_ptr.lock();
      if (store->isReady()) {
        auto stage_id = store->getStageId();
        if (stage_id) {
          auto stage = store->stages.get(*stage_id);
          auto output_sound_card = store->getOutputSoundCard();
          if (stage && output_sound_card && output_sound_card->online) {
            autoInit(*stage, *output_sound_card);
          }
        }
      }
    }, token_);
  }

  client_->stageChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                       const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady() && initialized_) {
      auto current_stage = store->getStage();
      if (current_stage && current_stage->_id == _id && (update.contains("width") || update.contains("length")
          || update.contains("height"))) {
        PLOGD << "stageChanged";
        auto output_sound_card = store->getOutputSoundCard();
        if (current_stage && output_sound_card && output_sound_card->online) {
          autoInit(*current_stage, *output_sound_card);
        }
      }
    }
  }, token_);
  client_->audioTrackAdded.connect([this](const DigitalStage::Types::AudioTrack &audio_track, const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady() && initialized_) {
      PLOGD << "audioTrackAdded";
#ifdef USE_ONLY_NATIVE_DEVICES
      if (audio_track.type != "native") {
        PLOGW << "Ignoring non-native audio track, which has been published right now";
        return;
      }
      PLOGI << "A native audio track has been added";
#endif
      mutex_.lock();
      if (!audio_tracks_.count(audio_track._id)) {
        audio_tracks_[audio_track._id] = core_->CreateSingleSourceDSP();
        audio_tracks_[audio_track._id]->SetSpatializationMode(Binaural::TSpatializationMode::HighQuality);
        audio_tracks_[audio_track._id]->DisableNearFieldEffect();
        audio_tracks_[audio_track._id]->EnableAnechoicProcess();
        audio_tracks_[audio_track._id]->EnableDistanceAttenuationAnechoic();
      }
      setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
      mutex_.unlock();
    }
  }, token_);
  client_->audioTrackChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                            const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady() && initialized_) {
      PLOGD << "audioTrackChanged";
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto audio_track = store->audioTracks.get(_id);
        mutex_.lock();
        setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, store));
        mutex_.unlock();
      }
    }
  }, token_);
  client_->audioTrackRemoved.connect([this](const DigitalStage::Types::AudioTrack &audio_track, const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if (!store_ptr.expired() && store_ptr.lock()->isReady() && initialized_) {
      mutex_.lock();
      PLOGD << "audioTrackRemoved";
      audio_tracks_.erase(audio_track._id);
      mutex_.unlock();
    }
  }, token_);
  client_->stageMemberChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                             const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        // Update all related audio tracks
        auto audio_tracks = store->getAudioTracksByStageMember(_id);
        for (const auto &audio_track: audio_tracks) {
          mutex_.lock();
          setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
          mutex_.unlock();
        }
        // Is this listener assigned to the stage member?
        auto stage_member_id = store->getStageMemberId();
        if (stage_member_id && *stage_member_id == _id) {
          auto stage_member = store->stageMembers.get(_id);
          // Also update this listener
          if (stage_member) {
            mutex_.lock();
            setListenerPosition(calculatePosition(*stage_member, store));
            mutex_.unlock();
          } else {
            PLOGE << "Stage member not found";
          }
        }
      }
    }
  }, token_);
  client_->groupChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                       const std::weak_ptr<DigitalStage::Api::Store>& store_ptr) {
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        // Update all related audio tracks
        auto audio_tracks = store->getAudioTracksByGroup(_id);
        for (const auto &audio_track: audio_tracks) {
          mutex_.lock();
          setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
          mutex_.unlock();
        }
        // Is this the group of this listener?
        auto group_id = store->getGroupId();
        if (group_id && *group_id == _id) {
          // Also update this listener
          auto stage_member_id = store->getStageMemberId();
          if(stage_member_id) {
            auto stage_member = store->stageMembers.get(*stage_member_id);
            if (stage_member) {
              mutex_.lock();
              setListenerPosition(calculatePosition(*stage_member, store));
              mutex_.unlock();
            } else {
              PLOGE << "Stage member not found";
            }
          } else {
            PLOGE << "No active stage member";
          }
        }
      }
    }
  }, token_);
}

template<class T>
void AudioRenderer<T>::setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setListenerPosition";
  Common::CTransform transform = Common::CTransform();
  transform.SetPosition(Common::CVector3(static_cast<float>(position.x),
                                         static_cast<float>(position.y),
                                         static_cast<float>(position.z)));
  transform.SetOrientation(Common::CQuaternion::FromYawPitchRoll(static_cast<float>(position.rZ),
                                                                 static_cast<float>(position.rY),
                                                                 static_cast<float>(position.rX)));
  listener_->SetListenerTransform(transform);
}
template<class T>
void AudioRenderer<T>::setAudioTrackPosition(const string &audio_track_id,
                                             const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setAudioTrackPosition";
  if (audio_tracks_.count(audio_track_id)) {
    Common::CTransform transform = Common::CTransform();
    transform.SetPosition(Common::CVector3(static_cast<float>(position.x),
                                           static_cast<float>(position.y),
                                           static_cast<float>(position.z)));
    transform.SetOrientation(Common::CQuaternion::FromYawPitchRoll(static_cast<float>(position.rZ),
                                                                   static_cast<float>(position.rY),
                                                                   static_cast<float>(position.rX)));
    audio_tracks_[audio_track_id]->SetSourceTransform(transform);
  }
}
template<class T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::StageMember &stage_member,
                                                                                    const std::shared_ptr<DigitalStage::Api::Store>& store) {
  PLOGD << "calculatePosition of StageMember";
  // Get related group
  auto group = stage_member.groupId ? store->groups.get(*stage_member.groupId) : std::nullopt;
  auto target_group_id = store->getGroupId();
  auto custom_group = group && target_group_id ? store->getCustomGroupByGroupAndTargetGroup(group->_id, *target_group_id) : std::nullopt;

  // Calculate coordinates
  double pos_x = stage_member.x;
  if (group) {
    pos_x += custom_group ? custom_group->x : group->x;
  }
  double pos_y = stage_member.y;
  if (group) {
    pos_y += custom_group ? custom_group->y : group->y;
  }
  double pos_z = stage_member.z;
  if (group) {
    pos_z += custom_group ? custom_group->z : group->z;
  }
  double r_x = stage_member.rX;
  if (group) {
    r_x += custom_group ? custom_group->rX : group->rX;
  }
  double r_y = stage_member.rY;
  if (group) {
    r_y += custom_group ? custom_group->rY : group->rY;
  }
  double r_z = stage_member.rZ;
  if (group) {
    r_z += custom_group ? custom_group->rZ : group->rZ;
  }

  return {"cardoid", pos_x, pos_y, pos_z, r_x, r_y, r_z};
}
template<class T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::AudioTrack &audio_track,
                                                                                    std::shared_ptr<DigitalStage::Api::Store> store) {
  PLOGD << "calculatePosition of AudioTrack";
  // Get this device ID
  auto local_device_id = store->getLocalDeviceId();
  if (!local_device_id) {
    PLOGE << "Local device ID not set";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }
  auto stage_member = store->stageMembers.get(audio_track.stageMemberId);
  if (!stage_member) {
    PLOGE << "Stage member not available";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }

  auto stage_member_position = calculatePosition(*stage_member, store);

  // Calculate coordinates
  double pos_x = audio_track.x + stage_member_position.x;
  double pos_y = audio_track.y + stage_member_position.y;
  double pos_z = audio_track.z + stage_member_position.z;
  double r_x = audio_track.rX + stage_member_position.rX;
  double r_y = audio_track.rY + stage_member_position.rY;
  double r_z = audio_track.rZ + stage_member_position.rZ;

  return {"cardoid", pos_x, pos_y, pos_z, r_x, r_y, r_z};
}
template<class T>
void AudioRenderer<T>::render(const std::string &audio_track_id,
                              T *input,
                              T *outLeft,
                              T *outRight,
                              std::size_t frame_size) {
  // Get volume and mute state (if available)
  auto volume_info = audio_mixer_ ? audio_mixer_->getGain(audio_track_id) : std::nullopt;
  if (volume_info && volume_info->second) {
    // Muted, do nothing
    memset(outLeft, 0, sizeof(float) * frame_size);
    return;
  }
  // Not muted so far ... now render:
  if (initialized_ && frame_size == current_frame_size_) {
    if (mutex_.try_lock()) {
      if (audio_tracks_.count(audio_track_id) != 0) {
        if(falling_back_) {
          falling_back_ = false;
          //PLOGD << "Disabling fallback since all requirements are fulfilled";
        }
        try {
          CMonoBuffer<float> input_buffer(frame_size);
          input_buffer.Feed(input, frame_size, 1);

          Common::CEarPair<CMonoBuffer<float>> buffer_processed;

          input_buffer.ApplyGain(volume_info->first);

          audio_tracks_[audio_track_id]->SetBuffer(input_buffer);
          audio_tracks_[audio_track_id]->ProcessAnechoic(buffer_processed.left, buffer_processed.right);

          if (!buffer_processed.left.empty()) {
            for (int frame = 0; frame < frame_size; frame++) {
              outLeft[frame] += buffer_processed.left[frame];
              outRight[frame] += buffer_processed.right[frame];
            }
          }
        } catch (std::exception &err) {
          PLOGE << err.what();
          mutex_.unlock();
        }
      } else {
        if(!falling_back_) {
          falling_back_ = true;
          //PLOGD << "No render information for audio track - falling back to simple mixing";
        }
        renderFallback(input, outLeft, outRight, frame_size, volume_info);
      }
      mutex_.unlock();
    } else {
      if(!falling_back_) {
        falling_back_ = true;
        PLOGD << "Falling back to simple mixing to avoid blocking";
      }
      renderFallback(input, outLeft, outRight, frame_size, volume_info);
    }
  } else {
    if(!falling_back_) {
      falling_back_ = true;
      PLOGD << "Falling back to simple mixing since 3D audio is not supported";
    }
    renderFallback(input, outLeft, outRight, frame_size, volume_info);
  }
}
template<class T>
void AudioRenderer<T>::renderReverb(T *outLeft, T *outRight, std::size_t frame_size) { // NOLINT(bugprone-easily-swappable-parameters)
  if (initialized_ && frame_size == current_frame_size_) {
    if (mutex_.try_lock()) {
      //TODO: Pull initialized fully to here
      if(!initialized_)
        return;
      Common::CEarPair<CMonoBuffer<float>> buffer_reverb;

      environment_->ProcessVirtualAmbisonicReverb(buffer_reverb.left, buffer_reverb.right);

      if (!buffer_reverb.left.empty()) {
        for (int frame = 0; frame < frame_size; frame++) {
          outLeft[frame] += buffer_reverb.left[frame];
          outRight[frame] += buffer_reverb.right[frame];
        }
      }
      mutex_.unlock();
    }
  }
}

template<class T>
void AudioRenderer<T>::renderFallback(
    T *input,
    T *outLeft, // NOLINT(bugprone-easily-swappable-parameters)
    T *outRight,
    std::size_t frame_size,
    std::optional<DigitalStage::Audio::VolumeInfo<T>> volume_info) {
  if (volume_info) {
    for (
        int frame = 0;
frame < frame_size;
frame++) {
      outLeft[frame] += (input[frame] * volume_info->first);
      outRight[frame] += (input[frame] * volume_info->first);
    }
  } else {
    for (
        int frame = 0;
frame < frame_size;
frame++) {
// Just mixing
      outLeft[frame] += input[frame];
      outRight[frame] += input[frame];
    }
  }
}