//
// Created by Tobias Hegemann on 04.11.21.
//

#include "AudioRenderer.h"

template<typename T>
AudioRenderer<T>::AudioRenderer(DigitalStage::Api::Client &client) : initialized_(false) {
  ERRORHANDLER3DTI.SetVerbosityMode(VERBOSITYMODE_ERRORSANDWARNINGS);
  ERRORHANDLER3DTI.SetErrorLogStream(&std::cout, true);

  environment_ = core_.CreateEnvironment();
  environment_->SetReverberationOrder(TReverberationOrder::BIDIMENSIONAL);
  BRIR::CreateFrom3dti("./../Resources/3DTI_BRIR_large_48000Hz.3dti-brir", environment_);

  listener_ = core_.CreateListener();
  listener_->DisableCustomizedITD();
  //TODO: The path is only valid for MacOS
  HRTF::CreateFrom3dti("./../Resources/hrtf.3dti-hrtf", listener_);

  attachHandlers(client);
}

template<typename T>
void AudioRenderer<T>::attachHandlers(DigitalStage::Api::Client &client) {
  client.stageJoined.connect([this](const ID_TYPE &, const ID_TYPE &,
                                    const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      auto output_sound_card = store->getOutputSoundCard();
      if (output_sound_card && store->getStageDeviceId()) {
        PLOGD << "AudioRenderer::attachHandlers::stageJoined";
        init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
      }
    }
  });
  client.ready.connect([this](const DigitalStage::Api::Store *store) {
    if (store->getStageDeviceId()) {
      PLOGD << "AudioRenderer::attachHandlers::ready";
      auto output_sound_card = store->getOutputSoundCard();
      if (output_sound_card) {
        init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
      }
    }
  });
  client.outputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                               const DigitalStage::Api::Store *store) {
    if (store->isReady() && update.contains("sampleRate") && store->getStageDeviceId()) {
      PLOGD << "AudioRenderer::attachHandlers::outputSoundCardChanged";
      auto output_sound_card = store->getOutputSoundCard();
      init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
    }
  });
  client.outputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                                const DigitalStage::Api::Store *store) {
    if (store->isReady() && store->getStageDeviceId()) {
      PLOGD << "AudioRenderer::attachHandlers::outputSoundCardSelected";
      auto output_sound_card = store->getOutputSoundCard();
      init(output_sound_card->sampleRate, output_sound_card->frameSize, *store);
    }
  });
  client.audioTrackAdded.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "AudioRenderer::attachHandlers::audioTrackAdded";
      audio_tracks_[audio_track._id] = core_.CreateSingleSourceDSP();
      setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      PLOGD << "AudioRenderer::attachHandlers::audioTrackRemoved";
      audio_tracks_.erase(audio_track._id);
    }
  });
}
template<typename T>
void AudioRenderer<T>::init(int sample_rate, int buffer_size, const DigitalStage::Api::Store &store) {
  PLOGD << "AudioRenderer::init - " << sample_rate;
  audio_tracks_.clear();
  Common::TAudioStateStruct audioState;
  audioState.sampleRate = sample_rate;
  audioState.bufferSize = buffer_size;
  core_.SetAudioState(audioState);

  // Listener
  auto local_stage_device = store.getStageDevice();
  assert(local_stage_device);
  setListenerPosition(calculatePosition(*local_stage_device, store));

  // Other remote audio tracks
  for (const auto &audio_track: store.audioTracks.getAll()) {
    audio_tracks_[audio_track._id] = core_.CreateSingleSourceDSP();
    setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
  }

}

template<typename T>
void AudioRenderer<T>::setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "AudioRenderer::setListenerPosition";
  Common::CTransform transform = Common::CTransform();
  transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
  //TODO: transform.SetOrientation(...)
  listener_->SetListenerTransform(transform);
}
template<typename T>
void AudioRenderer<T>::setAudioTrackPosition(const string &audio_track_id,
                                             const DigitalStage::Types::ThreeDimensionalProperties &position) {
  if (audio_tracks_.count(audio_track_id)) {
    PLOGD << "AudioRenderer::setAudioTrackPosition";
    Common::CTransform transform = Common::CTransform();
    transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
    //TODO: transform.SetOrientation(...)
    audio_tracks_[audio_track_id]->SetSourceTransform(transform);
    audio_tracks_[audio_track_id]->SetSpatializationMode(Binaural::TSpatializationMode::HighPerformance);
    audio_tracks_[audio_track_id]->DisableNearFieldEffect();
    audio_tracks_[audio_track_id]->EnableAnechoicProcess();
    audio_tracks_[audio_track_id]->EnableDistanceAttenuationAnechoic();
  }
}
template<typename T>
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(const DigitalStage::Types::StageDevice &stage_device,
                                                                                    const DigitalStage::Api::Store &store) {
  PLOGD << "AudioRenderer::calculatePosition(stageDevice)";
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
  PLOGD << "AudioRenderer::calculatePosition(audioTrack)";
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
  if (audio_tracks_.count(audio_track_id)) {
    PLOGD << "AudioRenderer::renderA";

    CMonoBuffer<float> inputBuffer(frame_size);
    inputBuffer.Feed(in, frame_size, 1);

    Common::CEarPair<CMonoBuffer<float>> bufferProcessed;

    PLOGD << "inputBuffer" << frame_size << " " << inputBuffer.size() << " " << core_.GetAudioState().bufferSize;

    PLOGD << "AudioRenderer::renderB";
    audio_tracks_[audio_track_id]->ProcessAnechoic(inputBuffer,bufferProcessed.left, bufferProcessed.right);
    PLOGD << "AudioRenderer::renderB2";

    Common::CEarPair<CMonoBuffer<float>> bufferReverb;

    PLOGD << "AudioRenderer::renderC";
    //environment_->ProcessVirtualAmbisonicReverb(bufferReverb.left, bufferReverb.right);

    PLOGD << "AudioRenderer::renderD";
    for (int f = 0; f < frame_size; f++) {
      //float l = (bufferProcessed.left[f] + bufferReverb.left[f]);
      //float r = (bufferProcessed.right[f] + bufferReverb.right[f]);
      outLeft[f] += bufferProcessed.left[f];
      outRight[f] += bufferProcessed.right[f];
    }
  }
}
