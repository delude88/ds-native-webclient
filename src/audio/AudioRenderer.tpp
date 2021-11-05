//
// Created by Tobias Hegemann on 04.11.21.
//

#include "AudioRenderer.h"

template<typename T>
AudioRenderer<T>::AudioRenderer(DigitalStage::Api::Client &client) : initialized_(false) {
  ERRORHANDLER3DTI.SetVerbosityMode(VERBOSITYMODE_ERRORSANDWARNINGS);
  ERRORHANDLER3DTI.SetErrorLogStream(&std::cout, true);
  attachHandlers(client);
}

template<typename T>
void AudioRenderer<T>::attachHandlers(DigitalStage::Api::Client &client) {
  client.ready.connect([this](const DigitalStage::Api::Store *store) {
    auto output_sound_card = store->getOutputSoundCard();
    init(output_sound_card->sampleRate, *store);
  });
  client.outputSoundCardChanged.connect([this](const std::string &, const nlohmann::json &update,
                                               const DigitalStage::Api::Store *store) {
    if (update.contains("sampleRate")) {
      auto output_sound_card = store->getOutputSoundCard();
      init(output_sound_card->sampleRate, *store);
    }
  });
  client.outputSoundCardSelected.connect([this](const std::optional<std::string> &,
                                                const DigitalStage::Api::Store *store) {
    auto output_sound_card = store->getOutputSoundCard();
    init(output_sound_card->sampleRate, *store);
  });
}

template<typename T>
void AudioRenderer<T>::init(int sample_rate, const DigitalStage::Api::Store &store) {
  audio_tracks_.clear();
  Common::TAudioStateStruct audioState;
  audioState.sampleRate = sample_rate;
  //TODO: audioState.bufferSize = buffer_size;
  core_.SetAudioState(audioState);
  listener_ = core_.CreateListener();
  auto local_stage_device = store.getStageDevice();
  assert(local_stage_device);
  setListenerPosition(calculatePosition(*local_stage_device, store));
  for (const auto &audio_track: store.audioTracks.getAll()) {
    audio_tracks_[audio_track._id] = core_.CreateSingleSourceDSP();
    setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, store));
  }

}

template<typename T>
std::optional<std::pair<T, T>> AudioRenderer<T>::getRenderValue(const std::string &audio_track_id) const {
  if (render_map_.count(audio_track_id)) {
    return render_map_[audio_track_id];
  }
  return std::nullopt;
}
template<typename T>
void AudioRenderer<T>::setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position) {
  Common::CTransform transform = Common::CTransform();
  transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
  //TODO: transform.SetOrientation(...)
  listener_->SetListenerTransform(transform);
}
template<typename T>
void AudioRenderer<T>::setAudioTrackPosition(const string &audio_track_id,
                                             const DigitalStage::Types::ThreeDimensionalProperties &position) {
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
DigitalStage::Types::ThreeDimensionalProperties AudioRenderer<T>::calculatePosition(DigitalStage::Types::AudioTrack audio_track,
                                                                                    const DigitalStage::Api::Store &store) {
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
