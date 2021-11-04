//
// Created by Tobias Hegemann on 25.10.21.
//

#ifndef CLIENT_SRC_AUDIO_AUDIOMIXER_H_
#define CLIENT_SRC_AUDIO_AUDIOMIXER_H_

#include <DigitalStage/Api/Store.h>
#include <DigitalStage/Api/Client.h>
#include <sigslot/signal.hpp>
#include <thread>
#include <string>
#include "../../utils/ringbuffer.h"

#define BUFFER_SIZE_IN_MS 5
#define SAMPLE_RATE 48000
#define BUFFER_SIZE_IN_FRAMES SAMPLE_RATE / 1000 * BUFFER_SIZE_IN_MS

class AudioMixer {
 public:
  AudioMixer();
  ~AudioMixer();

  void attachHandlers(DigitalStage::Api::Client &client);

  void writeLocal(const float *pInput, unsigned int numChannels, unsigned int frameSize);

  void write(const std::string &audioTrackId, float f);

  void mixDown(float *pOutput, unsigned int numChannels, unsigned int frameSize);

  /**
   * Emitted, when new data is available for a local track, identified by its audioTrackId as the first parameter.
   */
  Pal::sigslot::signal<const std::string &, float*, unsigned int> onLocalChannel;
 private:
  void handleInputSoundCardChange(DigitalStage::Api::Client &client);

  std::map<size_t, std::string> local_channel_mapping_;
  std::map<std::string, std::unique_ptr<RingBuffer<float>>> channels_;
};

#endif //CLIENT_SRC_AUDIO_AUDIOMIXER_H_
