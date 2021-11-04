#ifndef LIBDS_EXAMPLE_PEER_HANDLER_H_
#define LIBDS_EXAMPLE_PEER_HANDLER_H_

#include "WebRTCPeerConnection.h"
#include "../audio/AudioMixer.h"
#include <DigitalStage/Api/Client.h>
#include <DigitalStage/Types.h>
#include <api/create_peerconnection_factory.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <system_wrappers/include/field_trial.h>

class PeerHandler {
 public:
  explicit PeerHandler(AudioMixer &);
  ~PeerHandler();

  void attachHandlers(DigitalStage::Api::Client &client);

 private:
  void init();
  void onStageChanged(DigitalStage::Api::Client &client,
                      const DigitalStage::Api::Store &store);
  void handleStageDevice(DigitalStage::Api::Client &client,
                         const DigitalStage::Types::StageDevice &stage_device,
                         const std::string &local_stage_device_id);
  void handleLocalChannel(const std::string &audioTrackId, float* dataPtr, unsigned int dataSize);

  bool ready_;
  bool sending_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<rtc::Thread> signaling_thread_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
  std::map<std::string, std::unique_ptr<WebRTCPeerConnection>> peer_connections_;
  webrtc::PeerConnectionInterface::RTCConfiguration configuration_;
  AudioMixer &audio_mixer_;
};

#endif