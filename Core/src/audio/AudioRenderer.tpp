//
// Created by Tobias Hegemann on 04.11.21.
//

#include "../utils/CMRCFileBuffer.h"
#include "AudioRenderer.h"

template<class T>
AudioRenderer<T>::AudioRenderer(std::shared_ptr<DigitalStage::Api::Client> client, bool autostart)
    : fs_(cmrc::clientres::get_filesystem()),
      client_(std::move(client)),
      audio_mixer_(std::make_unique<AudioMixer<float>>(client_)),
      initialized_(false),
      current_frame_size_(0) {
  PLOGD << "AudioRenderer";
  ERRORHANDLER3DTI.SetVerbosityMode(VERBOSITY_MODE_ONLYERRORS);
  ERRORHANDLER3DTI.SetErrorLogStream(&std::cerr, true);
  ERRORHANDLER3DTI.SetAssertMode(ASSERT_MODE_CONTINUE);

  attachHandlers(autostart);
}

template<class T>
AudioRenderer<T>::~AudioRenderer() {
  initialized_ = false;
}

template<class T>
std::string AudioRenderer<T>::to_string(AudioRenderer::RoomSize room_size) {
  switch (room_size) {
    case AudioRenderer::RoomSize::SMALL:return "small";
    case AudioRenderer::RoomSize::LARGE:return "large";
    default:return "medium";
  }
}

template<class T>
bool AudioRenderer<T>::isValid(unsigned int sample_rate, unsigned int buffer_size) {
  std::string brirPath("3DTI_BRIR_small_" + std::to_string(sample_rate) + "Hz.3dti-brir");
  std::string hrtfPath
      ("3DTI_HRTF_IRC1008_" + std::to_string(buffer_size) + "s_" + std::to_string(sample_rate) + "Hz.3dti-hrtf");
  return fs_.is_file(brirPath) && fs_.is_file(hrtfPath);
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
  std::string brirPath("3DTI_BRIR_" + to_string(room_size) + "_" + std::to_string(sample_rate) + "Hz.3dti-brir");
  if (!fs_.is_file(brirPath)) {
    throw std::runtime_error(
        "BRIR for room size " + to_string(room_size) + " not available. Check your build settings. Was looking for "
            + brirPath);
  }
  auto brirFile = fs_.open(brirPath);
  CMRCFileBuffer brirFileBuffer(brirFile);
  std::istream brirStream(&brirFileBuffer);
  std::string hrtfPath
      ("3DTI_HRTF_IRC1008_" + std::to_string(buffer_size) + "s_" + std::to_string(sample_rate) + "Hz.3dti-hrtf");
  auto hrtfFile = fs_.open(hrtfPath);
  CMRCFileBuffer hrtfFileBuffer(hrtfFile);
  std::istream hrtfStream(&hrtfFileBuffer);
  audio_tracks_.clear();
  // Init core
  core_ = std::make_shared<Binaural::CCore>(Common::TAudioStateStruct{static_cast<int>(sample_rate),
                                                                      static_cast<int>(buffer_size)},
                                            hrtf_resampling_steps);
  current_frame_size_ = buffer_size;
  // Init environment (used for reverb)
  environment_ = core_->CreateEnvironment();
  environment_->SetReverberationOrder(TReverberationOrder::BIDIMENSIONAL);
  if (!BRIR::CreateFrom3dtiStream(brirStream, environment_)) {
    throw std::runtime_error("Could not create BRIR");
  }
  if (!environment_->GetBRIR()->IsBRIRready()) {
    throw std::runtime_error("Created BRIR but it is not ready");
  }
  PLOGI << "Loaded BRIR " << brirPath;

  listener_ = core_->CreateListener();
  listener_->DisableCustomizedITD();

  if (!HRTF::CreateFrom3dtiStream(hrtfStream, listener_)) {
    throw std::runtime_error("Could not create HRTF");
  }
  if (!listener_->GetHRTF()->IsHRTFLoaded()) {
    throw std::runtime_error("Created HRTF but it is not loaded");
  }
  PLOGI << "Loaded HRTF " << hrtfPath;

  // Listener
  auto store = client_->getStore();
  auto local_stage_device = store->getStageDevice();
  if (!local_stage_device) {
    throw std::runtime_error("Local stage device not available");
  }
  setListenerPosition(calculatePosition(*local_stage_device, *store));

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
    setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
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
    auto room_size = AudioRenderer::RoomSize::SMALL;
    if (room_volume > 10000) {
      room_size = AudioRenderer::RoomSize::LARGE;
    } else if (room_volume > 1000) {
      room_size = AudioRenderer::RoomSize::MEDIUM;
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
    client_->ready.connect([this](const DigitalStage::Api::Store *store) {
      auto stageId = store->getStageId();
      if (stageId) {
        PLOGD << "ready";
        auto stage = store->stages.get(*stageId);
        auto output_sound_card = store->getOutputSoundCard();
        if (stage && output_sound_card && output_sound_card->online) {
          autoInit(*stage, *output_sound_card);
        }
      }
    });
    client_->stageJoined.connect([this](const ID_TYPE &stage_id, const optional<ID_TYPE> &,
                                        const DigitalStage::Api::Store *store) {
      if (store->isReady()) {
        PLOGD << "stageJoined";
        auto stage = store->stages.get(stage_id);
        auto output_sound_card = store->getOutputSoundCard();
        if (stage && output_sound_card && output_sound_card->online) {
          autoInit(*stage, *output_sound_card);
        }
      }
    });
    client_->stageLeft.connect([this](const DigitalStage::Api::Store *store) {
      stop();
    });
    client_->outputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                                   const DigitalStage::Api::Store *store) {
      if (store->isReady() &&
          (update.contains("sampleRate") || update.contains("bufferSize") || update.contains("online"))) {
        auto stageId = store->getStageId();
        if (stageId) {
          auto stage = store->stages.get(*stageId);
          auto output_sound_card = store->getOutputSoundCard();
          if (stage && output_sound_card && output_sound_card->online) {
            autoInit(*stage, *output_sound_card);
          }
        }
      }
    });
    client_->outputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                                    const DigitalStage::Api::Store *store) {
      if (store->isReady()) {
        auto stageId = store->getStageId();
        if (stageId) {
          auto stage = store->stages.get(*stageId);
          auto output_sound_card = store->getOutputSoundCard();
          if (stage && output_sound_card && output_sound_card->online) {
            autoInit(*stage, *output_sound_card);
          }
        }
      }
    });
  }

  client_->stageChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                       const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto current_stage = store->getStage();
      if (current_stage && current_stage->_id == id && (update.contains("width") || update.contains("length")
          || update.contains("height"))) {
        PLOGD << "stageChanged";
        auto output_sound_card = store->getOutputSoundCard();
        if (current_stage && output_sound_card && output_sound_card->online) {
          autoInit(*current_stage, *output_sound_card);
        }
      }
    }
  });
  client_->audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
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
      setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
      mutex_.unlock();
    }
  });
  client_->audioTrackChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                            const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      PLOGD << "audioTrackChanged";
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto audio_track = store->audioTracks.get(id);
        mutex_.lock();
        setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
        mutex_.unlock();
      }
    }
  });
  client_->audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      mutex_.lock();
      PLOGD << "audioTrackRemoved";
      audio_tracks_.erase(audio_track._id);
      mutex_.unlock();
    }
  });
  client_->customAudioTrackPositionAdded.connect([this](const CustomAudioTrackPosition &position,
                                                        const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_track = store->audioTracks.get(position.audioTrackId);
      if (audio_track) {
        mutex_.lock();
        setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
        mutex_.unlock();
      } else {
        PLOGE << "Audio track not found";
      }
    }
  });
  client_->customAudioTrackPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                          const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto position = store->customAudioTrackPositions.get(id);
        if (position) {
          auto audio_track = store->audioTracks.get(position->audioTrackId);
          if (audio_track) {
            mutex_.lock();
            setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
            mutex_.unlock();
          } else {
            PLOGE << "Audio track not found";
          }
        } else {
          PLOGE << "Custom audio track position not found";
        }
      }
    }
  });
  client_->customAudioTrackPositionRemoved.connect([this](const CustomAudioTrackPosition &position,
                                                          const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_track = store->audioTracks.get(position.audioTrackId);
      if (audio_track) {
        mutex_.lock();
        setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
        mutex_.unlock();
      } else {
        PLOGE << "Audio track not found";
      }
    }
  });
  client_->stageDeviceChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                             const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        // Did the local stage device and so this listener change?
        auto stage_device = store->stageDevices.get(id);
        auto local_stage_device_id = store->getStageDeviceId();
        if (id == *local_stage_device_id) {
          // This listener changed
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          // Recalculate audio track position
          auto audio_tracks = store->getAudioTracksByStageDevice(id);
          for (const auto &audio_track: audio_tracks) {
            mutex_.lock();
            setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
            mutex_.unlock();
          }
        }
      }
    }
  });
  client_->customStageDevicePositionAdded.connect([this](const CustomStageDevicePosition &position,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByStageDevice(position.stageDeviceId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this stageDevice?
      auto stageDevice_id = store->getStageDeviceId();
      if (stageDevice_id && *stageDevice_id == position.stageDeviceId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
  client_->customStageDevicePositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                           const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto custom_position = store->customStageDevicePositions.get(id);

        if (custom_position) {
          auto audio_tracks = store->getAudioTracksByStageDevice(custom_position->stageDeviceId);
          for (const auto &audio_track: audio_tracks) {
            mutex_.lock();
            setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
            mutex_.unlock();
          }
          // Is this listener assigned to this stageDevice?
          auto stageDevice_id = store->getStageDeviceId();
          if (stageDevice_id && *stageDevice_id == custom_position->stageDeviceId) {
            // Also update this listener
            auto stage_device = store->getStageDevice();
            if (stage_device) {
              mutex_.lock();
              setListenerPosition(calculatePosition(*stage_device, *store));
              mutex_.unlock();
            } else {
              PLOGE << "Stage device not found";
            }
          }
        } else {
          PLOGE << "Custom stage device position not found";
        }
      }
    }
  });
  client_->customStageDevicePositionRemoved.connect([this](const CustomStageDevicePosition &position,
                                                           const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByStageDevice(position.stageDeviceId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this stageDevice?
      auto stageDevice_id = store->getStageDeviceId();
      if (stageDevice_id && *stageDevice_id == position.stageDeviceId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
  client_->stageMemberChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                             const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        // Update all related audio tracks
        auto audio_tracks = store->getAudioTracksByStageMember(id);
        for (const auto &audio_track: audio_tracks) {
          mutex_.lock();
          setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
          mutex_.unlock();
        }
        // Is this listener assigned to the stage member?
        auto stage_member_id = store->getStageMemberId();
        if (stage_member_id && *stage_member_id == id) {
          // Also update this listener
          auto stage_device = store->getStageDevice();
          if (stage_device) {
            mutex_.lock();
            setListenerPosition(calculatePosition(*stage_device, *store));
            mutex_.unlock();
          } else {
            PLOGE << "Stage device not found";
          }
        }
      }
    }
  });
  client_->customStageMemberPositionAdded.connect([this](const CustomStageMemberPosition &position,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByStageMember(position.stageMemberId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this stageMember?
      auto stageMember_id = store->getStageMemberId();
      if (stageMember_id && *stageMember_id == position.stageMemberId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
  client_->customStageMemberPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                           const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto custom_position = store->customStageMemberPositions.get(id);
        if (!custom_position) {
          PLOGE << "Custom position not found";
          return;
        }
        auto audio_tracks = store->getAudioTracksByStageMember(custom_position->stageMemberId);
        for (const auto &audio_track: audio_tracks) {
          mutex_.lock();
          setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
          mutex_.unlock();
        }
        // Is this listener assigned to this stageMember?
        auto stageMember_id = store->getStageMemberId();
        if (stageMember_id && *stageMember_id == custom_position->stageMemberId) {
          // Also update this listener
          auto stage_device = store->getStageDevice();
          if (stage_device) {
            mutex_.lock();
            setListenerPosition(calculatePosition(*stage_device, *store));
            mutex_.unlock();
          } else {
            PLOGE << "Stage device not found";
          }
        }
      }
    }
  });
  client_->customStageMemberPositionRemoved.connect([this](const CustomStageMemberPosition &position,
                                                           const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByStageMember(position.stageMemberId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this stageMember?
      auto stageMember_id = store->getStageMemberId();
      if (stageMember_id && *stageMember_id == position.stageMemberId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
  client_->groupChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                       const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        // Update all related audio tracks
        auto audio_tracks = store->getAudioTracksByGroup(id);
        for (const auto &audio_track: audio_tracks) {
          mutex_.lock();
          setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
          mutex_.unlock();
        }
        // Is this the group of this listener?
        auto group_id = store->getGroupId();
        if (group_id && *group_id == id) {
          // Also update this listener
          auto stage_device = store->getStageDevice();
          if (stage_device) {
            mutex_.lock();
            setListenerPosition(calculatePosition(*stage_device, *store));
            mutex_.unlock();
          } else {
            PLOGE << "Stage device not found";
          }
        }
      }
    }
  });
  client_->customGroupPositionAdded.connect([this](const CustomGroupPosition &position,
                                                   const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByGroup(position.groupId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this group?
      auto group_id = store->getGroupId();
      if (group_id && *group_id == position.groupId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
  client_->customGroupPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                     const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto custom_position = store->customGroupPositions.get(id);
        if (custom_position) {
          auto audio_tracks = store->getAudioTracksByGroup(custom_position->groupId);
          for (const auto &audio_track: audio_tracks) {
            mutex_.lock();
            setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
            mutex_.unlock();
          }
          // Is this listener assigned to this group?
          auto group_id = store->getGroupId();
          if (group_id && *group_id == custom_position->groupId) {
            // Also update this listener
            auto stage_device = store->getStageDevice();
            if (stage_device) {
              mutex_.lock();
              setListenerPosition(calculatePosition(*stage_device, *store));
              mutex_.unlock();
            } else {
              PLOGE << "Stage device not found";
            }
          }
        } else {
          PLOGE << "Custom group position not found";
        }
      }
    }
  });
  client_->customGroupPositionRemoved.connect([this](const CustomGroupPosition &position,
                                                     const DigitalStage::Api::Store *store) {
    if (store->isReady() && initialized_) {
      auto audio_tracks = store->getAudioTracksByGroup(position.groupId);
      for (const auto &audio_track: audio_tracks) {
        mutex_.lock();
        setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
        mutex_.unlock();
      }
      // Is this listener assigned to this group?
      auto group_id = store->getGroupId();
      if (group_id && *group_id == position.groupId) {
        // Also update this listener
        auto stage_device = store->getStageDevice();
        if (stage_device) {
          mutex_.lock();
          setListenerPosition(calculatePosition(*stage_device, *store));
          mutex_.unlock();
        } else {
          PLOGE << "Stage device not found";
        }
      }
    }
  });
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
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::StageDevice &stage_device,
                                                                                    const DigitalStage::Api::Store &store) {
  PLOGD << "calculatePosition of StageDevice";
  // Get this device ID
  auto local_device_id = store.getLocalDeviceId();
  if (!local_device_id) {
    PLOGE << "Local device ID not set";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }
  auto custom_stage_device_position =
      store.getCustomStageDevicePositionByStageDeviceAndDevice(stage_device._id, *local_device_id);
  // Get related stage member
  auto stage_member = store.stageMembers.get(stage_device.stageMemberId);
  if (!local_device_id) {
    PLOGE << "Stage member not found";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }
  auto custom_stage_member_position =
      store.getCustomStageMemberPositionByStageMemberAndDevice(stage_member->_id, *local_device_id);
  // Get related group
  auto group = stage_member->groupId ? store.groups.get(*stage_member->groupId) : std::nullopt;
  auto custom_group_position = store.getCustomGroupPositionByGroupAndDevice(group->_id, *local_device_id);

  // Calculate coordinates
  double x = custom_stage_device_position ? custom_stage_device_position->x : stage_device.x;
  x += custom_stage_member_position ? custom_stage_member_position->x : stage_member->x;
  if (group) {
    x += custom_group_position ? custom_group_position->x : group->x;
  }
  double y = custom_stage_device_position ? custom_stage_device_position->y : stage_device.y;
  y += custom_stage_member_position ? custom_stage_member_position->y : stage_member->y;
  if (group) {
    y += custom_group_position ? custom_group_position->y : group->y;
  }
  double z = custom_stage_device_position ? custom_stage_device_position->z : stage_device.z;
  z += custom_stage_member_position ? custom_stage_member_position->z : stage_member->z;
  if (group) {
    z += custom_group_position ? custom_group_position->z : group->z;
  }
  double rX = custom_stage_device_position ? custom_stage_device_position->rX : stage_device.rX;
  rX += custom_stage_member_position ? custom_stage_member_position->rX : stage_member->rX;
  if (group) {
    rX += custom_group_position ? custom_group_position->rX : group->rX;
  }
  double rY = custom_stage_device_position ? custom_stage_device_position->rY : stage_device.rY;
  rY += custom_stage_member_position ? custom_stage_member_position->rY : stage_member->rY;
  if (group) {
    rY += custom_group_position ? custom_group_position->rY : group->rY;
  }
  double rZ = custom_stage_device_position ? custom_stage_device_position->rZ : stage_device.rZ;
  rZ += custom_stage_member_position ? custom_stage_member_position->rZ : stage_member->rZ;
  if (group) {
    rZ += custom_group_position ? custom_group_position->rZ : group->rZ;
  }

  return {"cardoid", x, y, z, rX, rY, rZ};
}
template<class T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::AudioTrack &audio_track,
                                                                                    const DigitalStage::Api::Store &store) {
  PLOGD << "calculatePosition of AudioTrack";
  // Get this device ID
  auto local_device_id = store.getLocalDeviceId();
  if (!local_device_id) {
    PLOGE << "Local device ID not set";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }
  auto custom_audio_track_position =
      store.getCustomAudioTrackPositionByAudioTrackAndDevice(audio_track._id, *local_device_id);
  auto stage_device = store.stageDevices.get(audio_track.stageDeviceId);
  if (!stage_device) {
    PLOGE << "Stage device not available";
    return {"cardoid", 0, 0, 0, 0, 0, 0};
  }

  auto stage_device_position = calculatePosition(*stage_device, store);

  // Calculate coordinates
  double x = custom_audio_track_position ? custom_audio_track_position->x : audio_track.x;
  x += stage_device_position.x;
  double y = custom_audio_track_position ? custom_audio_track_position->y : audio_track.y;
  y += stage_device_position.y;
  double z = custom_audio_track_position ? custom_audio_track_position->z : audio_track.z;
  z += stage_device_position.z;
  double rX = custom_audio_track_position ? custom_audio_track_position->rX : audio_track.rX;
  rX += stage_device_position.rX;
  double rY = custom_audio_track_position ? custom_audio_track_position->rY : audio_track.rY;
  rY += stage_device_position.rY;
  double rZ = custom_audio_track_position ? custom_audio_track_position->rZ : audio_track.rZ;
  rZ += stage_device_position.rZ;

  return {"cardoid", x, y, z, rX, rY, rZ};
}
template<class T>
void AudioRenderer<T>::render(const std::string &audio_track_id,
                              T *in,
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
        try {
          CMonoBuffer<float> inputBuffer(frame_size);
          inputBuffer.Feed(in, frame_size, 1);

          Common::CEarPair<CMonoBuffer<float>> bufferProcessed;

          inputBuffer.ApplyGain(volume_info->first);

          audio_tracks_[audio_track_id]->SetBuffer(inputBuffer);
          audio_tracks_[audio_track_id]->ProcessAnechoic(bufferProcessed.left, bufferProcessed.right);

          if (!bufferProcessed.left.empty()) {
            for (int f = 0; f < frame_size; f++) {
              outLeft[f] += bufferProcessed.left[f];
              outRight[f] += bufferProcessed.right[f];
            }
          }
        } catch (std::exception &err) {
          PLOGE << err.what();
          mutex_.unlock();
        }
      } else {
        PLOGD << "No render information for audio track - falling back to simple mixing";
        renderFallback(in, outLeft, outRight, frame_size, volume_info);
      }
      mutex_.unlock();
    } else {
      PLOGD << "Falling back to simple mixing to avoid blocking";
      renderFallback(in, outLeft, outRight, frame_size, volume_info);
    }
  } else {
    PLOGD << "Falling back to simple mixing since 3D audio is not supported";
    renderFallback(in, outLeft, outRight, frame_size, volume_info);
  }
}
template<class T>
void AudioRenderer<T>::renderReverb(T *outLeft, T *outRight, std::size_t frame_size) {
  if (initialized_ && frame_size == current_frame_size_) {
    if (mutex_.try_lock()) {
      Common::CEarPair<CMonoBuffer<float>> bufferReverb;

      environment_->ProcessVirtualAmbisonicReverb(bufferReverb.left, bufferReverb.right);

      if (!bufferReverb.left.empty()) {
        for (int f = 0; f < frame_size; f++) {
          outLeft[f] += bufferReverb.left[f];
          outRight[f] += bufferReverb.right[f];
        }
      }
      mutex_.unlock();
    }
  }
}

template<class T>
void AudioRenderer<T>::renderFallback(
    T *in,
    T *outLeft,
    T *outRight,
    std::size_t frame_size,
    std::optional<VolumeInfo<T>> volume_info) {
  if (volume_info) {
    for (
        int f = 0;
        f < frame_size;
        f++) {
      outLeft[f] += (in[f] * volume_info->first);
      outRight[f] += (in[f] * volume_info->first);
    }
  } else {
    for (
        int f = 0;
        f < frame_size;
        f++) {
// Just mixing
      outLeft[f] += in[f];
      outRight[f] += in[f];
    }
  }
}