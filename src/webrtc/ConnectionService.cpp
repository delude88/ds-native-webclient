//
// Created by Tobias Hegemann on 26.10.21.
//

#include "ConnectionService.h"
#include "../utils/conversion.h"

ConnectionService::ConnectionService(DigitalStage::Api::Client &client)
    : configuration_(rtc::Configuration()) {
  configuration_.iceServers.emplace_back("stun:stun.l.google.com:19302");
  attachHandlers(client);
}

ConnectionService::~ConnectionService() {
}

void ConnectionService::attachHandlers(DigitalStage::Api::Client &client) {
  PLOGD << "ConnectionService::attachHandlers()";
  client.ready.connect([this, &client](const DigitalStage::Api::Store *store) {
    onStageChanged(client);
  });
  client.stageJoined.connect([this, &client](const ID_TYPE &, const ID_TYPE &,
                                             const DigitalStage::Api::Store *store) {
    onStageChanged(client);
  });
  client.stageLeft.connect([this, &client](const DigitalStage::Api::Store *store) {
    onStageChanged(client);
  });
  client.stageDeviceAdded.connect([this, &client](const StageDevice &device, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      // We safely can ignore here, if this is the local stage device, since we wait for ready
      if (device.active) {
        std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // WRITE
        auto local_stage_device_id = store->getStageDeviceId();
        createPeerConnection(device._id, *local_stage_device_id, client);
      }
    }
  });
  client.stageDeviceChanged.connect([this, &client](const std::string &_id, const nlohmann::json &update,
                                                    const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      // We safely can ignore here, if this is the local stage device, since we wait for ready
      auto local_stage_device_id = store->getStageDeviceId();
      if (_id != *local_stage_device_id) {
        if (update.contains("active")) {
          std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
          bool is_active = update["active"];
          auto item = peer_connections_[_id];
          if (is_active) {
            if (item) {
              createPeerConnection(_id, *local_stage_device_id, client);
            }
          } else {
            if (item) {
              closePeerConnection(_id);
            }
          }
        }
      }
    }
  });
  client.p2pOffer.connect([this, &client](const P2POffer &offer, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(offer.to == *local_stage_device_id);
    assert(offer.from != *local_stage_device_id);
    std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
    if (!peer_connections_.count(offer.from)) {
      createPeerConnection(offer.from, offer.to, client);
    }
    assert(peer_connections_[offer.from]);
    peer_connections_[offer.from]->setRemoteSessionDescription(offer.offer);
  });
  client.p2pAnswer.connect([this](const P2PAnswer &answer, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(answer.to == *local_stage_device_id);
    assert(answer.from != *local_stage_device_id);
    std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
    if (peer_connections_.count(answer.from)) {
      peer_connections_.at(answer.from)->setRemoteSessionDescription(answer.answer);
    }
  });
  client.iceCandidate.connect([this](const IceCandidate &ice, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(ice.to == *local_stage_device_id);
    assert(ice.from != *local_stage_device_id);
    std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
    if (ice.iceCandidate && peer_connections_.count(ice.from)) {
      peer_connections_.at(ice.from)->addRemoteIceCandidate(*ice.iceCandidate);
    }
  });
}

void ConnectionService::onStageChanged(DigitalStage::Api::Client &client) {
  PLOGD << "ConnectionService::handleStageChanged()";
  auto store = client.getStore();
  if (store->isReady()) {
    auto stage_id = store->getStageId();
    if (stage_id) {
      auto stage = store->stages.get(*stage_id);
      assert(stage);
      if (stage->audioType == "mediasoup") {
        auto local_stage_device_id = store->getStageDeviceId();
        assert(local_stage_device_id);
        auto stage_devices = store->stageDevices.getAll();
        for (const auto &item: stage_devices) {
          if (item._id != *local_stage_device_id) {
            std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
            if (item.active) {
              // Active
              if (peer_connections_.count(item._id) == 0) {
                // But no connection yet
                createPeerConnection(item._id, *local_stage_device_id, client);
              }
            } else {
              // Not active
              if (peer_connections_.count(item._id) != 0) {
                // But connection is still alive
                closePeerConnection(item._id);
              }
            }
          }
        }
      }
    }
  }
}
void ConnectionService::createPeerConnection(const std::string &stage_device_id,
                                             const std::string &local_stage_device_id,
                                             DigitalStage::Api::Client &client) {
  // And (double) check if the connection has not been created by a thread we had to probably wait for
  if (peer_connections_.count(stage_device_id)) {
    return;
  }
  bool polite = local_stage_device_id.compare(stage_device_id) > 0;
  peer_connections_[stage_device_id] = std::make_shared<PeerConnection>(configuration_, polite);
  peer_connections_[stage_device_id]->onLocalIceCandidate = [local_stage_device_id, stage_device_id, &client](
      const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
    DigitalStage::Types::IceCandidate ice_candidate;
    ice_candidate.to = stage_device_id;
    ice_candidate.from = local_stage_device_id;
    ice_candidate.iceCandidate = ice_candidate_init;
    client.send(DigitalStage::Api::SendEvents::SEND_ICE_CANDIDATE, ice_candidate);
  };
  peer_connections_[stage_device_id]->onLocalSessionDescription = [local_stage_device_id, stage_device_id, &client](
      const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
    if (session_description_init.type == "offer") {
      DigitalStage::Types::P2POffer offer;
      offer.to = stage_device_id;
      offer.from = local_stage_device_id;
      offer.offer = session_description_init;
      client.send(DigitalStage::Api::SendEvents::SEND_P2P_OFFER, offer);
    } else if (session_description_init.type == "answer") {
      DigitalStage::Types::P2PAnswer answer;
      answer.to = stage_device_id;
      answer.from = local_stage_device_id;
      answer.answer = session_description_init;
      client.send(DigitalStage::Api::SendEvents::SEND_P2P_ANSWER, answer);
    }
  };
  peer_connections_[stage_device_id]->onData = [this](const std::string &audio_track_id,
                                                      const std::vector<std::byte> &values) {
    onData(audio_track_id, values);
  };
}
void ConnectionService::closePeerConnection(const std::string &stage_device_id) {
  peer_connections_.erase(stage_device_id);
}
void ConnectionService::broadcast(const std::string &audio_track_id, const std::byte *data, const std::size_t size) {
  std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_);
  for (const auto &item: peer_connections_) {
    if (item.second) {
      item.second->send(audio_track_id, data, size);
    }
  }
}

void ConnectionService::broadcast(const std::string &audio_track_id, const float *data, const std::size_t size) {
  // float to std::byte
  std::size_t buffer_size = size * 4;
  std::byte buffer[buffer_size];
  serialize(data, size, buffer);
  broadcast(audio_track_id, buffer, buffer_size);
}
void ConnectionService::broadcast(const std::string &audio_track_id, const float data) {
  std::byte buffer[4];
  serializeFloat(data, buffer);
  broadcast(audio_track_id, buffer, 4);
}
