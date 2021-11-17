//
// Created by Tobias Hegemann on 04.11.21.
//
#pragma once

#include "udp_discovery_peer.hpp"

class ServiceDiscovery {
 public:
  explicit ServiceDiscovery(const std::string &device_id, bool discover = false) {
    parameters_.set_port(12021);
    parameters_.set_application_id(497920112);
    parameters_.set_can_discover(discover);
    parameters_.set_can_be_discovered(true);
    peer_.Start(parameters_, device_id);
  }
  ~ServiceDiscovery() {
    peer_.Stop();
  }

  inline std::list<udpdiscovery::DiscoveredPeer> scan() {
    return peer_.ListDiscovered();
  }

 private:
  udpdiscovery::PeerParameters parameters_;
  udpdiscovery::Peer peer_;
};