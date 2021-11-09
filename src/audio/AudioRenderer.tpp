//
// Created by Tobias Hegemann on 04.11.21.
//

#include "AudioRenderer.h"
#include <cmrc/cmrc.hpp>
#include "../utils/CMRCFileBuffer.h"
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
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      PLOGD << "audioTrackAdded";
      if (!audio_tracks_.count(audio_track._id)) {
        audio_tracks_[audio_track._id] = core_->CreateSingleSourceDSP();
        audio_tracks_[audio_track._id]->SetSpatializationMode(Binaural::TSpatializationMode::HighQuality);
        audio_tracks_[audio_track._id]->DisableNearFieldEffect();
        audio_tracks_[audio_track._id]->EnableAnechoicProcess();
        audio_tracks_[audio_track._id]->EnableDistanceAttenuationAnechoic();
      }
      setAudioTrackPosition(audio_track._id, calculatePosition(audio_track, *store));
    }
  });
  client.audioTrackRemoved.connect([this](const AudioTrack &audio_track, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      PLOGD << "audioTrackRemoved";
      audio_tracks_.erase(audio_track._id);
    }
  });
}
template<typename T>
void AudioRenderer<T>::init(int sample_rate, int buffer_size, const DigitalStage::Api::Store &store) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
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
}

template<typename T>
void AudioRenderer<T>::setListenerPosition(const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setListenerPosition";
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  Common::CTransform transform = Common::CTransform();
  transform.SetPosition(Common::CVector3(position.x, position.y, position.z));
  //TODO: transform.SetOrientation(...)
  listener_->SetListenerTransform(transform);
}
template<typename T>
void AudioRenderer<T>::setAudioTrackPosition(const string &audio_track_id,
                                             const DigitalStage::Types::ThreeDimensionalProperties &position) {
  PLOGD << "setAudioTrackPosition";
  std::lock_guard<std::recursive_mutex> lock(mutex_);
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
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (audio_tracks_.count(audio_track_id) != 0 && listener_->GetHRTF()->IsHRTFLoaded()) {
    // Compare frame size
    if (frame_size != core_->GetAudioState().bufferSize) {
      PLOGD << "Correcting buffer size of audio renderer from " << core_->GetAudioState().bufferSize << " to " << frame_size;
      //TODO: Optimize this by comparing member variable instead of calling GetAudioState() twice each time
      auto audio_state = core_->GetAudioState();
      audio_state.bufferSize = frame_size;
      core_->SetAudioState(audio_state);
    }

    CMonoBuffer<float> inputBuffer(frame_size);
    inputBuffer.Feed(in, frame_size, 1);

    CMonoBuffer<float> leftBuffer(frame_size);
    CMonoBuffer<float> rightBuffer(frame_size);
    leftBuffer.Fill(frame_size, 0);
    rightBuffer.Fill(frame_size, 0);

    if(inputBuffer.size() != core_->GetAudioState().bufferSize) {
      PLOGE << "Not matching";
    }
    if(leftBuffer.size() != core_->GetAudioState().bufferSize) {
      PLOGE << "Not matching";
    }
    if(rightBuffer.size() != core_->GetAudioState().bufferSize) {
      PLOGE << "Not matching";
    }

    auto track = audio_tracks_[audio_track_id];
    //track->ProcessAnechoic(inputBuffer, leftBuffer, rightBuffer);

    for (int f = 0; f < frame_size; f++) {
      // Just mixing
      outLeft[f] += in[f];
      outRight[f] += in[f];
    }
    /*
    bufferProcessed.left.Fill(frame_size, 0);
    bufferProcessed.right.Fill(frame_size, 0);

    auto track = audio_tracks_[audio_track_id];
    assert(track);
    track->ProcessAnechoic(inputBuffer, bufferProcessed.left, bufferProcessed.right);

    Common::CEarPair<CMonoBuffer<float>> bufferReverb;
    bufferProcessed.left.Fill(frame_size, 0);
    bufferProcessed.right.Fill(frame_size, 0);

    //environment_->ProcessVirtualAmbisonicReverb(bufferReverb.left, bufferReverb.right);

    for (int f = 0; f < frame_size; f++) {
      outLeft[f] += (bufferProcessed.left[f]);// + bufferReverb.left[f]);
      outRight[f] += (bufferProcessed.right[f]);// + bufferReverb.right[f]);
    }*/
  } else {
    for (int f = 0; f < frame_size; f++) {
      // Just mixing
      outLeft[f] += in[f];
      outRight[f] += in[f];
    }
  }
}
