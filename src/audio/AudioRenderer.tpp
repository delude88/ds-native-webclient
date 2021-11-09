//
// Created by Tobias Hegemann on 04.11.21.
//

#include <cmrc/cmrc.hpp>
#include "../utils/CMRCFileBuffer.h"
#include "AudioRenderer.h"

CMRC_DECLARE(clientres);

template<typename T>
AudioRenderer<T>::AudioRenderer(DigitalStage::Api::Client &client)
    : core_(std::make_shared<Binaural::CCore>(Common::TAudioStateStruct{48000, 256}, 5)) {
  PLOGD << "AudioRenderer";
  ERRORHANDLER3DTI.SetVerbosityMode(VERBOSITYMODE_ERRORSANDWARNINGS);
  ERRORHANDLER3DTI.SetErrorLogStream(&std::cerr, true);

  auto fs = cmrc::clientres::get_filesystem();

  auto brirFile = fs.open("3DTI_BRIR_large_48000Hz.3dti-brir");
  CMRCFileBuffer brirFileBuffer(brirFile);
  std::istream brirFileStream(&brirFileBuffer);
  auto hrtfFile = fs.open("3DTI_HRTF_IRC1008_128s_48000Hz.3dti-hrtf");
  CMRCFileBuffer hrtfFileBuffer(hrtfFile);
  std::istream hrtfFileStream(&hrtfFileBuffer);

  environment_ = core_->CreateEnvironment();
  environment_->SetReverberationOrder(TReverberationOrder::BIDIMENSIONAL);
  BRIR::CreateFrom3dtiStream(brirFileStream, environment_);
  assert(environment_->GetBRIR()->IsBRIRready());
  PLOGD << "Loaded BRIR";

  listener_ = core_->CreateListener();
  listener_->DisableCustomizedITD();

  assert(HRTF::CreateFrom3dtiStream(hrtfFileStream, listener_));
  assert(listener_->GetHRTF()->IsHRTFLoaded());
  PLOGD << "Loaded HRTF";

  attachHandlers(client);
}

template<typename T>
void AudioRenderer<T>::attachHandlers(DigitalStage::Api::Client &client) {
  client.stageJoined.connect([this](const ID_TYPE &, const ID_TYPE &,
                                    const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto output_sound_card = store->getOutputSoundCard();
      if (output_sound_card && store->getStageDeviceId()) {
        PLOGD << "stageJoined";
        init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
      }
    }
  });
  client.ready.connect([this](const DigitalStage::Api::Store *store) {
    if (store->getStageDeviceId()) {
      PLOGD << "ready";
      auto output_sound_card = store->getOutputSoundCard();
      if (output_sound_card) {
        init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
      }
    }
  });
  client.outputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                               const DigitalStage::Api::Store *store) {
    if (store->isReady() && update.contains("sampleRate") && store->getStageDeviceId()) {
      PLOGD << "outputSoundCardChanged";
      auto output_sound_card = store->getOutputSoundCard();
      init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
    }
  });
  client.outputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                                const DigitalStage::Api::Store *store) {
    if (store->isReady() && store->getStageDeviceId()) {
      PLOGD << "outputSoundCardSelected";
      auto output_sound_card = store->getOutputSoundCard();
      init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
    }
  });
  client.audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "audioTrackAdded";
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
  client.audioTrackChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                          const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      mutex_.lock();
      PLOGD << "audioTrackRemoved";
      audio_tracks_.erase(audio_track._id);
      mutex_.unlock();
    }
  });
  client.customAudioTrackPositionAdded.connect([this](const CustomAudioTrackPosition &position,
                                                      const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto audio_track = store->audioTracks.get(position.audioTrackId);
      assert(audio_track);
      mutex_.lock();
      setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
      mutex_.unlock();
    }
  });
  client.customAudioTrackPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                        const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto position = store->customAudioTrackPositions.get(id);
        assert(position);
        auto audio_track = store->audioTracks.get(position->audioTrackId);
        assert(audio_track);
        mutex_.lock();
        setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
        mutex_.unlock();
      }
    }
  });
  client.customAudioTrackPositionRemoved.connect([this](const CustomAudioTrackPosition &position,
                                                        const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto audio_track = store->audioTracks.get(position.audioTrackId);
      assert(audio_track);
      mutex_.lock();
      setAudioTrackPosition(audio_track->_id, calculatePosition(*audio_track, *store));
      mutex_.unlock();
    }
  });
  client.stageDeviceChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                           const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customStageDevicePositionAdded.connect([this](const CustomStageDevicePosition &position,
                                                       const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customStageDevicePositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto custom_position = store->customStageDevicePositions.get(id);
        if (custom_position) {
          return;
        }
        assert(custom_position);
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
      }
    }
  });
  client.customStageDevicePositionRemoved.connect([this](const CustomStageDevicePosition &position,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.stageMemberChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                           const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customStageMemberPositionAdded.connect([this](const CustomStageMemberPosition &position,
                                                       const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customStageMemberPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customStageMemberPositionRemoved.connect([this](const CustomStageMemberPosition &position,
                                                         const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.groupChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                     const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customGroupPositionAdded.connect([this](const CustomGroupPosition &position,
                                                 const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
  client.customGroupPositionChanged.connect([this](const std::string &id, const nlohmann::json &update,
                                                   const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      if (update.contains("x") || update.contains("y") || update.contains("z") || update.contains("rX")
          || update.contains("rY") || update.contains("rZ")) {
        auto custom_position = store->customGroupPositions.get(id);
        assert(custom_position);
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
      }
    }
  });
  client.customGroupPositionRemoved.connect([this](const CustomGroupPosition &position,
                                                   const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
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
template<typename T>
void AudioRenderer<T>::init(int sample_rate, int buffer_size, const DigitalStage::Api::Store &store) {
  mutex_.lock();
  PLOGD << "nit - " << sample_rate;
  audio_tracks_.clear();
  Common::TAudioStateStruct audioState;
  audioState.sampleRate = sample_rate;
  audioState.bufferSize = buffer_size;
  core_->SetAudioState(audioState);

  // Listener
  auto local_stage_device = store.getStageDevice();
  assert(local_stage_device);
  setListenerPosition(calculatePosition(*local_stage_device, store));

  // Other remote audio tracks
  for (const auto &audio_track: store.audioTracks.getAll()) {
    if (!audio_tracks_.count(audio_track._id)) {
      audio_tracks_[audio_track._id] = core_->CreateSingleSourceDSP();
      audio_tracks_[audio_track._id]->SetSpatializationMode(Binaural::TSpatializationMode::HighQuality);
      audio_tracks_[audio_track._id]->DisableNearFieldEffect();
      audio_tracks_[audio_track._id]->EnableAnechoicProcess();
      audio_tracks_[audio_track._id]->EnableDistanceAttenuationAnechoic();
    }
    setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
  }
  mutex_.unlock();
}

template<typename T>
void AudioRenderer<T>::setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setListenerPosition";
  Common::CTransform transform = Common::CTransform();
  transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
  //TODO: transform.SetOrientation(...)
  listener_->SetListenerTransform(transform);
}
template<typename T>
void AudioRenderer<T>::setAudioTrackPosition(const string &audio_track_id,
                                             const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setAudioTrackPosition";
  if (audio_tracks_.count(audio_track_id)) {
    Common::CTransform transform = Common::CTransform();
    transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
    //TODO: transform.SetOrientation(...)
    audio_tracks_[audio_track_id]->SetSourceTransform(transform);
  }
}
template<typename T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::StageDevice &stage_device,
                                                                                    const DigitalStage::Api::Store &store) {
  PLOGD << "calculatePosition of StageDevice";
  // Get this device ID
  auto local_device_id = store.getLocalDeviceId();
  assert(local_device_id);
  auto custom_stage_device_position =
      store.getCustomStageDevicePositionByStageDeviceAndDevice(stage_device._id, *local_device_id);
  // Get related stage member
  auto stage_member = store.stageMembers.get(stage_device.stageMemberId);
  assert(stage_member);
  auto custom_stage_member_position =
      store.getCustomStageMemberPositionByStageMemberAndDevice(stage_member->_id, *local_device_id);
  // Get related group
  auto group = store.groups.get(stage_member->groupId);
  assert(group);
  auto custom_group_position = store.getCustomGroupPositionByGroupAndDevice(group->_id, *local_device_id);

  // Calculate coordinates
  double x = custom_stage_device_position ? custom_stage_device_position->x : stage_device.x;
  x += custom_stage_member_position ? custom_stage_member_position->x : stage_member->x;
  x *= custom_group_position ? custom_group_position->x : group->x;
  double y = custom_stage_device_position ? custom_stage_device_position->y : stage_device.y;
  y += custom_stage_member_position ? custom_stage_member_position->y : stage_member->y;
  y *= custom_group_position ? custom_group_position->y : group->y;
  double z = custom_stage_device_position ? custom_stage_device_position->z : stage_device.z;
  z += custom_stage_member_position ? custom_stage_member_position->z : stage_member->z;
  z *= custom_group_position ? custom_group_position->z : group->z;
  double rX = custom_stage_device_position ? custom_stage_device_position->rX : stage_device.rX;
  rX += custom_stage_member_position ? custom_stage_member_position->rX : stage_member->rX;
  rX *= custom_group_position ? custom_group_position->rX : group->rX;
  double rY = custom_stage_device_position ? custom_stage_device_position->rY : stage_device.rY;
  rY += custom_stage_member_position ? custom_stage_member_position->rY : stage_member->rY;
  rY *= custom_group_position ? custom_group_position->rY : group->rY;
  double rZ = custom_stage_device_position ? custom_stage_device_position->rZ : stage_device.rZ;
  rZ += custom_stage_member_position ? custom_stage_member_position->rZ : stage_member->rZ;
  rZ *= custom_group_position ? custom_group_position->rZ : group->rZ;

  return {"cardoid", x, y, z, rX, rY, rZ};
}
template<typename T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::AudioTrack &audio_track,
                                                                                    const DigitalStage::Api::Store &store) {
  PLOGD << "calculatePosition of AudioTrack";
  // Get this device ID
  auto local_device_id = store.getLocalDeviceId();
  assert(local_device_id);
  auto custom_audio_track_position =
      store.getCustomAudioTrackPositionByAudioTrackAndDevice(audio_track._id, *local_device_id);
  auto stage_device = store.stageDevices.get(audio_track.stageDeviceId);
  assert(stage_device);

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
template<typename T>
void AudioRenderer<T>::render(const std::string &audio_track_id,
                              T *in,
                              T *outLeft,
                              T *outRight,
                              std::size_t frame_size) {
  if (mutex_.try_lock()) {
    if (audio_tracks_.count(audio_track_id) != 0 && listener_->GetHRTF()->IsHRTFLoaded()) {
      // Compare frame size
      try {
        if (frame_size != core_->GetAudioState().bufferSize) {
          PLOGD << "Correcting buffer size of audio renderer from " << core_->GetAudioState().bufferSize << " to "
                << frame_size;
          //TODO: Optimize this by comparing member variable instead of calling GetAudioState() twice each time
          auto audio_state = core_->GetAudioState();
          audio_state.bufferSize = frame_size;
          core_->SetAudioState(audio_state);
          environment_->ResetReverbBuffers();
        }

        CMonoBuffer<float> inputBuffer(frame_size);
        inputBuffer.Feed(in, frame_size, 1);

        Common::CEarPair<CMonoBuffer<float>> bufferProcessed;

        audio_tracks_[audio_track_id]->SetBuffer(inputBuffer);
        audio_tracks_[audio_track_id]->ProcessAnechoic(bufferProcessed.left, bufferProcessed.right);


        for (int f = 0; f < frame_size; f++) {
          outLeft[f] += bufferProcessed.left[f];
          outRight[f] += bufferProcessed.right[f];
        }
      } catch (std::exception &err) {
        PLOGE << err.what();
      }
    } else {
      for (int f = 0; f < frame_size; f++) {
        // Just mixing
        outLeft[f] += in[f];
        outRight[f] += in[f];
      }
    }
    mutex_.unlock();
  }
}
template<typename T>
void AudioRenderer<T>::renderReverb(T *outLeft, T *outRight, std::size_t frame_size) {
  mutex_.lock();
  Common::CEarPair<CMonoBuffer<float>> bufferReverb;
  environment_->ProcessVirtualAmbisonicReverb(bufferReverb.left, bufferReverb.right);

  PLOGD << "bufferReverb.left.size = " << bufferReverb.left.size();
  PLOGD << "bufferReverb.right.size = " << bufferReverb.right.size();
  for (int f = 0; f < frame_size; f++) {
    std::cout << outLeft[f] << " += " << bufferReverb.left[f];
    outLeft[f] += bufferReverb.left[f];
    outRight[f] += bufferReverb.right[f];
  }
  mutex_.unlock();
}
