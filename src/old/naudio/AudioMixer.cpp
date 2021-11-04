//
// Created by Tobias Hegemann on 30.10.21.
//

#include "AudioMixer.h"
#include "../utils/thread.h"

#include <utility>

AudioMixer::AudioMixer(AudioIO &audio_io, ConnectionService &connection_service)
    : local_io_(audio_io), remote_io_(connection_service), mixing_(false), distributing_(false) {
  audio_io.onChannelMappingChanged.connect([this](ChannelMap map) {
    input_channel_mapping_ = std::move(map);
  });
}
void AudioMixer::start() {
  mixing_ = true;
  mixing_thread_ = std::thread(&AudioMixer::mix, this);
  //sched_param sch_params;
  //sch_params.sched_priority = 1;
  //pthread_setschedparam(mixing_thread_.native_handle(), SCHED_RR, &sch_params);
  set_realtime_prio(mixing_thread_);
}
void AudioMixer::stop() {
  distributing_ = false;
  mixing_ = false;
  //distribution_thread_.join();
  mixing_thread_.join();
}

void AudioMixer::attachHandlers(DigitalStage::Api::Client &client) {
}
void AudioMixer::mix() {
  std::size_t frame_size = 1000;
  while (mixing_) {
    float left[frame_size];

    // Fetch values from local channels
    for (int channel = 0; channel < 32; channel++) {
      if (input_channel_mapping_[channel]) {
        local_io_.read(channel, left, frame_size);
        //remote_io_.broadcast(*input_channel_mapping_[channel], left, frame_size);
      }
    }
    local_io_.writeLeft(left, frame_size);
    local_io_.writeRight(left, frame_size);
  }
}
void AudioMixer::distribute() {
  while (distributing_) {
    // Fetch values from audio IO and distribute them to the connection_service_ and the internal channel mapping
  }
}
