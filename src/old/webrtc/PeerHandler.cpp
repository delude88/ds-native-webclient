
#include "PeerHandler.h"


// unpack method for retrieving data in network byte,
//   big endian, order (MSB first)
// increments index i by the number of bytes unpacked
// usage:
//   int i = 0;
//   float x = unpackFloat(&buffer[i], &i);
//   float y = unpackFloat(&buffer[i], &i);
//   float z = unpackFloat(&buffer[i], &i);
float unpackFloat(const void *buf, int *i) {
  const unsigned char *b = (const unsigned char *)buf;
  uint32_t temp = 0;
  *i += 4;
  temp = ((b[0] << 24) |
      (b[1] << 16) |
      (b[2] <<  8) |
      b[3]);
  return *((float *) &temp);
}

// pack method for storing data in network,
//   big endian, byte order (MSB first)
// returns number of bytes packed
// usage:
//   float x, y, z;
//   int i = 0;
//   i += packFloat(&buffer[i], x);
//   i += packFloat(&buffer[i], y);
//   i += packFloat(&buffer[i], z);
int packFloat(void *buf, float x) {
  unsigned char *b = (unsigned char *)buf;
  unsigned char *p = (unsigned char *) &x;
#if defined (_M_IX86) || (defined (CPU_FAMILY) && (CPU_FAMILY == I80X86))
  b[0] = p[3];
    b[1] = p[2];
    b[2] = p[1];
    b[3] = p[0];
#else
  b[0] = p[0];
  b[1] = p[1];
  b[2] = p[2];
  b[3] = p[3];
#endif
  return 4;
}


PeerHandler::PeerHandler(AudioMixer &audio_mixer) : ready_(false), audio_mixer_(audio_mixer) {
  webrtc::field_trial::InitFieldTrialsFromString("");
  rtc::InitializeSSL();
  audio_mixer_.onLocalChannel.connect(&PeerHandler::handleLocalChannel, this);
}

PeerHandler::~PeerHandler() {
  rtc::CleanupSSL();
  network_thread_->Quit();
  network_thread_->Quit();
  signaling_thread_->Quit();
  peer_connection_factory_ = nullptr;
}

void PeerHandler::handleStageDevice(DigitalStage::Api::Client &client,
                                    const DigitalStage::Types::StageDevice &stage_device,
                                    const std::string &local_stage_device_id) {

  assert(ready_);
  if (stage_device.active) {
    if (peer_connections_.count(stage_device._id) == 0) {
      bool polite = local_stage_device_id.compare(stage_device._id) > 0;
      peer_connections_[stage_device._id] = std::make_unique<PeerConnection>(
          stage_device._id,
          polite
      );
      peer_connections_[stage_device._id]->onAcceptIce.connect([]() {
        std::cout << "onSuccess" << std::endl;
      });
      peer_connections_[stage_device._id]->onData.connect([](const webrtc::DataBuffer &data_buffer) {
        std::cout << "onSuccess" << std::endl;
      });
      peer_connections_[stage_device._id]->onIceCandidate.connect([&client, local_stage_device_id, stage_device](const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
        std::cout << "onIceCandidate" << std::endl;
        DigitalStage::Types::IceCandidate ice_candidate;
        ice_candidate.to = stage_device._id;
        ice_candidate.from = local_stage_device_id;
        ice_candidate.iceCandidate = ice_candidate_init;
        const nlohmann::json payload = ice_candidate;
        client.send(DigitalStage::Api::SendEvents::SEND_ICE_CANDIDATE, payload);
      });
      peer_connections_[stage_device._id]->onSessionDescription.connect([&client, local_stage_device_id, stage_device](
          const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
        std::cout << "onSessionDescription" << std::endl;
        if (session_description_init.type == "offer") {
          DigitalStage::Types::P2POffer offer;
          offer.to = stage_device._id;
          offer.from = local_stage_device_id;
          offer.offer = session_description_init;
          const nlohmann::json payload = offer;
          client.send(DigitalStage::Api::SendEvents::SEND_P2P_OFFER, payload);
        } else if (session_description_init.type == "answer") {
          DigitalStage::Types::P2PAnswer answer;
          answer.to = stage_device._id;
          answer.from = local_stage_device_id;
          answer.answer = session_description_init;
          const nlohmann::json payload = answer;
          client.send(DigitalStage::Api::SendEvents::SEND_P2P_ANSWER, payload);
        } else {
          std::cout << "Unknown session description type" << std::endl;
        }
      });
      peer_connections_[stage_device._id]->onSuccess.connect([]() {
        std::cout << "onSuccess" << std::endl;
      });
      peer_connections_[stage_device._id]->connect(peer_connection_factory_, configuration_);
    }
  }
}
void PeerHandler::onStageChanged(DigitalStage::Api::Client &client, const DigitalStage::Api::Store &store) {
  if (!ready_)
    return;
  // Does the current stage support webRTC?
  auto stage_id_ptr = store.getStageId();
  if (stage_id_ptr) {
    auto stage_ptr = store.stages.get(*stage_id_ptr);
    assert(stage_ptr);
    if (stage_ptr) {
      auto stage = *stage_ptr;
      if (stage.audioType == "mediasoup") {
        auto local_stage_device_id_ptr = store.getStageDeviceId();
        assert(local_stage_device_id_ptr);
        // Yes, this stage can handle webRTC audio
        // Now we want to create peer connections to all stage members, so fetch them
        std::vector<DigitalStage::Types::StageDevice> stageDevices = store.stageDevices.getAll();
        for (const auto &item: stageDevices) {
          std::cout << *local_stage_device_id_ptr << std::endl;
          handleStageDevice(client, item, *local_stage_device_id_ptr);
        }
        return;
      }
    }
  }
  // Stage is not available or capable of webRTC, so close all connections if there are any

}

void PeerHandler::attachHandlers(DigitalStage::Api::Client &client) {
  std::cout << "PeerHandler::attachHandlers()" << std::endl;
  client.ready.connect([this, &client](const DigitalStage::Api::Store *store) {
    ready_ = true;
    onStageChanged(client, *store);
  });
  client.stageJoined.connect([this, &client](const ID_TYPE &, const ID_TYPE &,
                                             const DigitalStage::Api::Store *store) {
    onStageChanged(client, *store);
  });
  client.stageLeft.connect([this, &client](const DigitalStage::Api::Store *store) {
    onStageChanged(client, *store);
  });
  client.p2pOffer.connect([this](const P2POffer &offer, const DigitalStage::Api::Store *store) {
    if (peer_connections_.count(offer.to)) {
      peer_connections_.at(offer.to)->setSessionDescription(offer.offer);
    }
  });
  client.p2pAnswer.connect([this](const P2PAnswer &answer, const DigitalStage::Api::Store *store) {
    if (peer_connections_.count(answer.to)) {
      peer_connections_.at(answer.to)->setSessionDescription(answer.answer);
    }
  });
  client.iceCandidate.connect([this](const IceCandidate &ice, const DigitalStage::Api::Store *store) {
    if (ice.iceCandidate && peer_connections_.count(ice.to)) {
      peer_connections_[ice.to]->addIceCandidate(*ice.iceCandidate);
    }
  });

  init();
}
void PeerHandler::init() {
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = "stun:stun.l.google.com:19302";
  configuration_.servers.push_back(ice_server);
  std::cout << "PeerHandler::init()" << std::endl;
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->Start();
  worker_thread_ = rtc::Thread::Create();
  worker_thread_->Start();
  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->Start();

  webrtc::PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread_.get();
  dependencies.worker_thread = worker_thread_.get();
  dependencies.signaling_thread = signaling_thread_.get();
  peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));

  if (peer_connection_factory_.get() == nullptr) {
    std::cout << std::this_thread::get_id() << ":"
              << "Error on CreateModularPeerConnectionFactory." << std::endl;
    exit(EXIT_FAILURE);
  }
}

void PeerHandler::handleLocalChannel(const std::string &audioTrackId, float *dataPtr, unsigned int dataSize) {
  size_t packet_size = 0;
  uint8_t* packet;
  for(int i = 0; i < dataSize; i++) {
    packet_size += packFloat(packet, dataPtr[i]);
  }
  auto data = new rtc::CopyOnWriteBuffer(packet, packet_size);
  webrtc::DataBuffer buffer(*data, true);
  for (auto &item: peer_connections_) {
    item.second->send(buffer);
  }
}
