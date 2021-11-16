//
// Created by Tobias Hegemann on 26.10.21.
//

#include "ConnectionService.h"
#include "../utils/conversion.h"

ConnectionService::ConnectionService(std::shared_ptr<DigitalStage::Api::Client> client)
    : client_(std::move(client)), configuration_(rtc::Configuration()) {
  attachHandlers();
}

void ConnectionService::attachHandlers() {
  PLOGD << "attachHandlers()";
  client_->ready.connect([this](const DigitalStage::Api::Store *store) {
    auto turn_urls = store->getTurnServers();
    if (!turn_urls.empty()) {
      auto turn_user = store->getTurnUsername();
      auto turn_secret = store->getTurnPassword();
      if (turn_user && turn_secret) {
        for (const auto &turn_url: turn_urls) {
          std::string delimiter = ":";
          auto host = turn_url.substr(0, turn_url.find(delimiter));
          auto port = turn_url.substr(turn_url.find(delimiter));
          configuration_.iceServers.emplace_back(rtc::IceServer(host, port, *turn_user, *turn_secret));
          PLOGI << "Using turn server:" << *turn_user << ":" << *turn_secret << "@" << host << ":" << port;
        }
      }
    } else {
      PLOGI << "Using public google STUN servers as fallback";
      configuration_.iceServers.emplace_back("stun:stun.l.google.com:19302");
    }
    onStageChanged();
  });
  client_->stageJoined.connect([this](const ID_TYPE &, const ID_TYPE &,
                                      const DigitalStage::Api::Store *store) {
    onStageChanged();
  });
  client_->stageLeft.connect([this](const DigitalStage::Api::Store *store) {
    onStageChanged();
  });
  client_->stageDeviceAdded.connect([this](const StageDevice &device, const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      // We safely can ignore here, if this is the local stage device, since we wait for ready
#if USE_ONLY_NATIVE_DEVICES
      if (device.active && device.type == "native") {
#else
      if (device.active) {
#endif
        std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // WRITE
        auto local_stage_device_id = store->getStageDeviceId();
        createPeerConnection(device._id, *local_stage_device_id);
      }
    }
  });
  client_->stageDeviceChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                             const DigitalStage::Api::Store *store) {
    if (store->isReady()) {
      // We safely can ignore here, if this is the local stage device, since we wait for ready
      auto local_stage_device_id = store->getStageDeviceId();
      if (_id != *local_stage_device_id) {
        if (update.contains("active")) {
          bool is_active = update["active"];
#if USE_ONLY_NATIVE_DEVICES
          auto device = store->devices.get(_id);
          if (device->type != "native") {
            return;
          }
#endif
          std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // maybe WRITE
          if (is_active) {
            if (!peer_connections_.count(_id)) {
              createPeerConnection(_id, *local_stage_device_id);
            }
          } else {
            if (peer_connections_.count(_id)) {
              closePeerConnection(_id);
            }
          }
        }
      }
    }
  });
  client_->p2pOffer.connect([this](const P2POffer &offer, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(offer.to == *local_stage_device_id);
    assert(offer.from != *local_stage_device_id);
#if USE_ONLY_NATIVE_DEVICES
    if(store->devices.get(offer.from)->type != "native") {
      return;
    }
#endif
    std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
    if (!peer_connections_.count(offer.from)) {
      createPeerConnection(offer.from, offer.to);
    }
    assert(peer_connections_[offer.from]);
    peer_connections_[offer.from]->setRemoteSessionDescription(offer.offer);
  });
  client_->p2pAnswer.connect([this](const P2PAnswer &answer, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(answer.to == *local_stage_device_id);
    assert(answer.from != *local_stage_device_id);
    std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_); // READ
    if (peer_connections_.count(answer.from)) {
      peer_connections_.at(answer.from)->setRemoteSessionDescription(answer.answer);
    }
  });
  client_->iceCandidate.connect([this](const IceCandidate &ice, const DigitalStage::Api::Store *store) {
    auto local_stage_device_id = store->getStageDeviceId();
    assert(ice.to == *local_stage_device_id);
    assert(ice.from != *local_stage_device_id);
    std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_); // READ
    if (ice.iceCandidate && peer_connections_.count(ice.from)) {
      peer_connections_.at(ice.from)->addRemoteIceCandidate(*ice.iceCandidate);
    }
  });
}

void ConnectionService::onStageChanged() {
  PLOGD << "handleStageChanged()";
  auto store = client_->getStore();
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
#if USE_ONLY_NATIVE_DEVICES
          if (item._id != *local_stage_device_id && item.type == "native") {
#else
          if (item._id != *local_stage_device_id) {
#endif
            std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
            if (item.active) {
              // Active
              if (!peer_connections_.count(item._id)) {
                // But no connection yet
                createPeerConnection(item._id, *local_stage_device_id);
              }
            } else {
              // Not active
              if (peer_connections_.count(item._id)) {
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
                                             const std::string &local_stage_device_id) {
  // And (double) check if the connection has not been created by a thread we had to probably wait for
  if (peer_connections_.count(stage_device_id)) {
    return;
  }
  bool polite = local_stage_device_id.compare(stage_device_id) > 0;
  peer_connections_[stage_device_id] = std::make_shared<PeerConnection>(configuration_, polite);
  peer_connections_[stage_device_id]->onLocalIceCandidate = [this, local_stage_device_id, stage_device_id](
      const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
    DigitalStage::Types::IceCandidate ice_candidate;
    ice_candidate.to = stage_device_id;
    ice_candidate.from = local_stage_device_id;
    ice_candidate.iceCandidate = ice_candidate_init;
    client_->send(DigitalStage::Api::SendEvents::SEND_ICE_CANDIDATE, ice_candidate);
  };
  peer_connections_[stage_device_id]->onLocalSessionDescription = [this, local_stage_device_id, stage_device_id](
      const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
    if (session_description_init.type == "offer") {
      DigitalStage::Types::P2POffer offer;
      offer.to = stage_device_id;
      offer.from = local_stage_device_id;
      offer.offer = session_description_init;
      client_->send(DigitalStage::Api::SendEvents::SEND_P2P_OFFER, offer);
    } else if (session_description_init.type == "answer") {
      DigitalStage::Types::P2PAnswer answer;
      answer.to = stage_device_id;
      answer.from = local_stage_device_id;
      answer.answer = session_description_init;
      client_->send(DigitalStage::Api::SendEvents::SEND_P2P_ANSWER, answer);
    }
  };
  peer_connections_[stage_device_id]->onData = [this](const std::string &audio_track_id,
                                                      const std::vector<std::byte> &values) {
    onData(audio_track_id, values);
  };
  PLOGI << "Connected to " << peer_connections_.size() << " peers";
}
void ConnectionService::closePeerConnection(const std::string &stage_device_id) {
  peer_connections_.erase(stage_device_id);
  assert(!peer_connections_.count(stage_device_id));
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
