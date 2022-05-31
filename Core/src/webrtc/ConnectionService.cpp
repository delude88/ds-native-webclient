//
// Created by Tobias Hegemann on 26.10.21.
//

#include "ConnectionService.h"
#include "../utils/conversion.h"
#include <optional>
#include <utility>
#include <DigitalStage/Api/Events.h>          // for PeerConnection
#include <plog/Log.h>

ConnectionService::ConnectionService(std::weak_ptr<DigitalStage::Api::Client> client_ptr)
    : client_ptr_(std::move(client_ptr)),
      configuration_(rtc::Configuration()),
      is_fetching_statistics_(true),
      token_(std::make_shared<DigitalStage::Api::Client::Token>()) {
  attachHandlers();
  statistics_thread_ = std::thread(&ConnectionService::fetchStatistics, this);
}
ConnectionService::~ConnectionService() {
  PLOGD << "Stopping connection service";
  is_fetching_statistics_ = false;
  if (statistics_thread_.joinable())
    statistics_thread_.join();

}

void ConnectionService::attachHandlers() {
  if(client_ptr_.expired()) {
    return;
  }
  auto client = client_ptr_.lock();
  PLOGD << "attachHandlers()";
  client->ready.connect([this](const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    PLOGD << "ready";
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    auto turn_urls = store->getTurnServers();
    if (!turn_urls.empty()) {
      auto turn_user = store->getTurnUsername();
      auto turn_secret = store->getTurnPassword();

      for (const auto &turn_url: turn_urls) {
        std::string url(turn_url);
        if (turn_user && turn_secret) {
          if (url.find("turn:") || url.find("turns:")) {
            url.insert(turn_url.find(':') + 1, *turn_user + ":" + *turn_secret + "@");
          } else {
            url = *turn_user + ":" + *turn_secret + "@" + url;
          }
          PLOGI << "Using TURN server: " << url;
        } else {
          PLOGI << "Using STUN server: " << url;
        }
        configuration_.iceServers.emplace_back(url);
      }
    } else {
      PLOGI << "Using public google STUN servers as fallback";
      configuration_.iceServers.emplace_back("stun:stun.l.google.com:19302");
    }
    onStageChanged();
  }, token_);
  client->stageJoined.connect([this](const DigitalStage::Types::ID_TYPE &,
                                     const std::optional<DigitalStage::Types::ID_TYPE> &,
                                     const std::weak_ptr<DigitalStage::Api::Store> & /*store_ptr*/) {
    onStageChanged();
  }, token_);
  client->stageLeft.connect([this](const std::weak_ptr<DigitalStage::Api::Store> & /*store_ptr*/) {
    onStageChanged();
  }, token_);
  client->stageDeviceAdded.connect([this](const DigitalStage::Types::StageDevice &device,
                                          const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady()) {
      // We safely can ignore here, if this is the local stage device, since we wait for ready
      if (device.active) {
#if USE_ONLY_NATIVE_DEVICES
        if (device.type != "native") {
          PLOGW << "Ignoring recently added non-native stage device";
          return;
        }
#elif USE_ONLY_WEBRTC_DEVICES
        if (device.type != "native" && device.type != "browser") {
          PLOGW << "Ignoring recently added non-webrtc stage device";
          return;
        }
#endif
        PLOGI << "Connecting to recently added native stage device " << device._id;

        std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // WRITE
        auto local_stage_device_id = store->getStageDeviceId();
        createPeerConnection(device._id, *local_stage_device_id);
      }
    }
  }, token_);
  client->stageDeviceChanged.connect([this](const std::string &_id, const nlohmann::json &update,
                                            const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    if (store->isReady()) {
      auto local_stage_device_id = store->getStageDeviceId();
      if (local_stage_device_id && _id != *local_stage_device_id) {
        if (update.contains("active")) {
          bool is_active = update["active"];
#if USE_ONLY_NATIVE_DEVICES
          auto device = store->stageDevices.get(_id);
          if (device->type != "native") {
            PLOGW << "Ignoring non-native device " << _id;
            return;
          }
          PLOGI << "Connecting to recently activated native stage device " << _id;
#elif USE_ONLY_WEBRTC_DEVICES
          auto device = store->stageDevices.get(_id);
          if (device->type != "native" && device->type != "browser") {
            PLOGW << "Ignoring non-native device " << _id;
            return;
          }
          PLOGI << "Connecting to recently activated webrtc stage device " << _id;
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
  }, token_);
  client->p2pOffer.connect([this](const DigitalStage::Types::P2POffer &offer,
                                  const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    auto local_stage_device_id = store->getStageDeviceId();
    if (local_stage_device_id) {
      assert(offer.to == *local_stage_device_id);
      assert(offer.from != *local_stage_device_id);
      //TODO: Assertion failed: (offer.from != *local_stage_device_id), function operator(), file ConnectionService.cpp, line 144.
#if USE_ONLY_NATIVE_DEVICES
      if (store->stageDevices.get(offer.from)->type != "native") {
      PLOGW << "Ignoring offer from non-native stage device" << offer.from;
      return;
    }
    PLOGI << "Accepting offer from native stage device " << offer.from;
#elif USE_ONLY_WEBRTC_DEVICES
      if (store->stageDevices.get(offer.from)->type != "native"
          && store->stageDevices.get(offer.from)->type != "browser") {
        PLOGW << "Ignoring offer from non-native stage device" << offer.from;
        return;
      }
      PLOGI << "Accepting offer from webrtc stage device " << offer.from;
#endif
      std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
      if (!peer_connections_.count(offer.from)) {
        createPeerConnection(offer.from, offer.to);
      }
      assert(peer_connections_[offer.from]);
      peer_connections_[offer.from]->setRemoteSessionDescription(offer.offer);
    }
  }, token_);
  client->p2pAnswer.connect([this](const DigitalStage::Types::P2PAnswer &answer,
                                   const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    auto local_stage_device_id = store->getStageDeviceId();
    if (local_stage_device_id) {
      assert(answer.to == *local_stage_device_id);
      assert(answer.from != *local_stage_device_id);
      std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_); // READ
      if (peer_connections_.count(answer.from)) {
        peer_connections_.at(answer.from)->setRemoteSessionDescription(answer.answer);
      }
    }
  }, token_);
  client->iceCandidate.connect([this](const DigitalStage::Types::IceCandidate &ice,
                                      const std::weak_ptr<DigitalStage::Api::Store> &store_ptr) {
    if (store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    auto local_stage_device_id = store->getStageDeviceId();
    if (local_stage_device_id) {
      assert(ice.to == *local_stage_device_id);
      assert(ice.from != *local_stage_device_id);
      std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_); // READ
      if (ice.iceCandidate && peer_connections_.count(ice.from)) {
        peer_connections_.at(ice.from)->addRemoteIceCandidate(*ice.iceCandidate);
      }
    }
  }, token_);
}

void ConnectionService::onStageChanged() {
  PLOGD << "handleStageChanged()";
  if (client_ptr_.expired()) {
    return;
  }
  auto store_ptr = client_ptr_.lock()->getStore();
  if (store_ptr.expired()) {
    return;
  }
  auto store = store_ptr.lock();
  if (store->isReady()) {
    auto stage_id = store->getStageId();
    if (stage_id) {
      auto stage = store->stages.get(*stage_id);
      assert(stage);
      if (stage->audioType == "browser") {
        auto local_stage_device_id = store->getStageDeviceId();
        assert(local_stage_device_id);
        auto stage_devices = store->stageDevices.getAll();
        for (const auto &item: stage_devices) {
          if (item._id != *local_stage_device_id) {
#if USE_ONLY_NATIVE_DEVICES
            if (item.type != "native") {
              PLOGW << "Ignoring existing non-native stage device " << item._id;
              return;
            }
            PLOGI << "Found existing native stage device " << item._id;
#elif USE_ONLY_WEBRTC_DEVICES
            if (item.type != "native" && item.type != "browser") {
              PLOGW << "Ignoring existing non-native stage device " << item._id;
              return;
            }
            PLOGI << "Found existing webrtc stage device " << item._id;
#endif
            PLOGD << "Request token";
            std::lock_guard<std::shared_mutex> lock_guard(peer_connections_mutex_); // READ and maybe WRITE
            PLOGD << "GOT token";
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
    } else {
      PLOGE << "Not inside any stage";
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
  peer_connections_[stage_device_id] = std::make_unique<PeerConnection>(configuration_, polite);
  peer_connections_[stage_device_id]->onLocalIceCandidate = [local_stage_device_id, stage_device_id, this](
      const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
    if(client_ptr_.expired()) {
      return;
    }
    auto client = client_ptr_.lock();
    DigitalStage::Types::IceCandidate ice_candidate;
    ice_candidate.to = stage_device_id;
    ice_candidate.from = local_stage_device_id;
    ice_candidate.iceCandidate = ice_candidate_init;
    client->send(DigitalStage::Api::SendEvents::SEND_ICE_CANDIDATE, ice_candidate);
  };
  peer_connections_[stage_device_id]->onLocalSessionDescription = [local_stage_device_id, stage_device_id, this](
      const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
    if(client_ptr_.expired()) {
      return;
    }
    auto client = client_ptr_.lock();
    if (session_description_init.type == "offer") {
      DigitalStage::Types::P2POffer offer;
      offer.to = stage_device_id;
      offer.from = local_stage_device_id;
      offer.offer = session_description_init;
      client->send(DigitalStage::Api::SendEvents::SEND_P2P_OFFER, offer);
    } else if (session_description_init.type == "answer") {
      DigitalStage::Types::P2PAnswer answer;
      answer.to = stage_device_id;
      answer.from = local_stage_device_id;
      answer.answer = session_description_init;
      client->send(DigitalStage::Api::SendEvents::SEND_P2P_ANSWER, answer);
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
void ConnectionService::broadcastBytes(const std::string &audio_track_id,
                                       const std::byte *data,
                                       const std::size_t size) {
  std::shared_lock<std::shared_mutex> shared_lock(peer_connections_mutex_);
  for (const auto &item: peer_connections_) {
    if (item.second) {
      item.second->send(audio_track_id, data, size);
    }
  }
}

void ConnectionService::broadcastFloats(const std::string &audio_track_id, const float *data, const std::size_t size) {
  std::size_t buffer_size = size * 4;
  auto *buffer = new std::byte[buffer_size];
  serialize(data, size, buffer);
  broadcastBytes(audio_track_id, buffer, buffer_size);
}

void ConnectionService::close(const std::string &audio_track_id) {
  for (const auto &item: peer_connections_) {
    item.second->close(audio_track_id);
  }
}

void ConnectionService::fetchStatistics() {
  while (is_fetching_statistics_) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if(client_ptr_.expired()) {
      return;
    }
    auto store_ptr = client_ptr_.lock()->getStore();
    if(store_ptr.expired()) {
      return;
    }
    auto store = store_ptr.lock();
    for (const auto &item: peer_connections_) {
      auto time = item.second->getRoundTripTime();
      if (time) {
        auto stage_device = store->stageDevices.get(item.first);
        if (stage_device) {
          auto user = store->users.get(stage_device->userId);
          if (user) {
            PLOGI << user->name << "@" << item.first << ": " << time->count() << "ms RTT";
          } else {
            PLOGI << "" << stage_device->userId << "/" << item.first << ": " << time->count() << "ms RTT";
          }
        } else {
          PLOGI << "?/" << item.first << ": " << time->count() << "ms RTT";
        }
      }
    }
  }
}