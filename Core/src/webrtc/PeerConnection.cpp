//
// Created by Tobias Hegemann on 26.10.21.
//

#include "PeerConnection.h"

PeerConnection::PeerConnection(const rtc::Configuration &configuration, bool polite) :
    peer_connection_(std::make_unique<rtc::PeerConnection>(configuration)),
    polite_(polite),
    making_offer_(false),
    ignore_offer_(false),
    srd_answer_pending_(false) {
  PLOGD << "PeerConnection";

  peer_connection_->onLocalCandidate([this](const rtc::Candidate &candidate) {
    PLOGD << "onLocalCandidate";
    // Just forward it to the other remote peer
    DigitalStage::Types::IceCandidateInit ice_candidate_init;
    ice_candidate_init.sdpMid = candidate.mid();
    ice_candidate_init.candidate = candidate.candidate();
    onLocalIceCandidate(ice_candidate_init);
  });

  peer_connection_->onLocalDescription([this](const rtc::Description &description) {
    PLOGD << "onLocalDescription";
    handleLocalSessionDescription(description);
  });

  peer_connection_->onStateChange([](rtc::PeerConnection::State state) {
    switch (state) {
      case rtc::PeerConnection::State::Connecting:PLOGD << "onStateChange -> Connecting";
        break;
      case rtc::PeerConnection::State::Connected:PLOGD << "onStateChange -> Connected";
        break;
      case rtc::PeerConnection::State::Disconnected:PLOGD << "onStateChange -> Disconnected";
        break;
      case rtc::PeerConnection::State::Closed:PLOGD << "onStateChange -> Closed";
        break;
      case rtc::PeerConnection::State::Failed:PLOGD << "onStateChange -> Failed";
        break;
      default:break;
    }
  });

  peer_connection_->onDataChannel([this](const std::shared_ptr<rtc::DataChannel> &incoming) {
    auto label = incoming->label();
    std::unique_lock<std::mutex> lock(receivers_mutex_);
    receivers_[label] = incoming;
    receivers_[label]->onMessage([this, label](const rtc::message_variant &message_variant) {
      try {
        auto binary = std::get<rtc::binary>(message_variant);
        onData(label, binary);
      }
      catch (const std::bad_variant_access &) {
      }
    });
  });
}

/*
void PeerConnection::makeOffer() {
  PLOGD << "PeerConnection::makeOffer" ;
  try {
    if (peer_connection_->signalingState() != rtc::PeerConnection::SignalingState::Stable) {
      throw std::runtime_error("Was not in stable state. Check your makeOffer calls!");
    }
    if (making_offer_) {
      throw std::runtime_error("Already making an offer. Check your makeOffer calls!");
    }
    making_offer_ = true;
    peer_connection_->setLocalDescription(rtc::Description::Type::Offer);  // <-- this is sync (!)
    if (peer_connection_->signalingState() != rtc::PeerConnection::SignalingState::HaveLocalOffer) {
      throw std::runtime_error("Make offer not racing with handlers");
    }
    if (peer_connection_->localDescription()->type() != rtc::Description::Type::Offer) {
      throw std::runtime_error("SLD of offer did not work");
    }
    PLOGD << "PeerConnection::makeOffer -> made offer?!?" ;
  } catch (std::runtime_error &error) {
    PLOGE << "PeerConnection: Could not create offer, reason: " << error.what() ;
    making_offer_ = false;
  }
}*/

void PeerConnection::addRemoteIceCandidate(const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
  PLOGD << "addRemoteIceCandidate";
  try {
    peer_connection_->addRemoteCandidate(rtc::Candidate(ice_candidate_init.candidate, ice_candidate_init.sdpMid));
  } catch (const std::logic_error &logic_error) {
    if (!ignore_offer_) {
      PLOGE << "Could not add remote ICE candidate: " << logic_error.what();
    } else {
      PLOGD << "Ignoring ice error - for sure!";
    }
  }
}
void PeerConnection::setRemoteSessionDescription(const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
  PLOGD << "setRemoteSessionDescription";
  rtc::Description description(session_description_init.sdp, session_description_init.type);

  bool is_stable = peer_connection_->signalingState() == rtc::PeerConnection::SignalingState::Stable ||
      (peer_connection_->signalingState() == rtc::PeerConnection::SignalingState::HaveLocalOffer
          && srd_answer_pending_);
  ignore_offer_ = description.type() == rtc::Description::Type::Offer && !polite_ && (making_offer_ || !is_stable);
  if (ignore_offer_) {
    PLOGD << "glare - ignoring offer";
    return;
  }
  srd_answer_pending_ = description.type() == rtc::Description::Type::Answer;
  peer_connection_->setRemoteDescription(description);
  srd_answer_pending_ = false;
  if (description.type() == rtc::Description::Type::Offer) {
    peer_connection_->setLocalDescription();
  }
}

void PeerConnection::handleLocalSessionDescription(const rtc::Description &description) {
  PLOGD << "handleLocalSessionDescription";
  if (description.type() == rtc::Description::Type::Offer) {
    try {
      // Always send offers, but make some checks first
      if (peer_connection_->signalingState() != rtc::PeerConnection::SignalingState::HaveLocalOffer) {
        throw std::runtime_error("Make offer not racing with handlers");
      }
      if (peer_connection_->localDescription()->type() != rtc::Description::Type::Offer) {
        throw std::runtime_error("SLD of offer did not work");
      }
      DigitalStage::Types::SessionDescriptionInit session_description_init;
      session_description_init.type = description.typeString();
      session_description_init.sdp = std::string(description);
      onLocalSessionDescription(session_description_init);
    } catch (std::runtime_error &error) {
      PLOGE << "Could not send offer, reason: " << error.what();
    }
    making_offer_ = false;
  } else if (description.type() == rtc::Description::Type::Answer) {
    try {
      if (peer_connection_->signalingState() != rtc::PeerConnection::SignalingState::Stable) {
        throw std::runtime_error("Handler not racing with makeOffer");
      }
      if (peer_connection_->localDescription()->type() != rtc::Description::Type::Answer) {
        throw std::runtime_error("SLD of answer did not work");
      }
      DigitalStage::Types::SessionDescriptionInit session_description_init;
      session_description_init.type = description.typeString();
      session_description_init.sdp = std::string(description);
      onLocalSessionDescription(session_description_init);
    } catch (std::runtime_error &error) {
      PLOGE << "Could not send answer, reason: " << error.what();
    }
  }
}
void PeerConnection::send(const std::string &audio_track_id, const std::byte *data, const size_t size) {
  std::unique_lock<std::mutex> lock(senders_mutex_);
  try {
    if (senders_.count(audio_track_id) == 0) {
      PLOGD << "Creating send data channel";
      senders_[audio_track_id] = peer_connection_->createDataChannel(audio_track_id);
    }
    // fire and forget
    if (senders_[audio_track_id]->isOpen()) {
      senders_[audio_track_id]->send(data, size);
    }
  } catch (std::exception &err) {
    PLOGW << "Could not send: " << err.what();
  }
}
void PeerConnection::close(const std::string &audio_track_id) {
  std::unique_lock<std::mutex> lock(senders_mutex_);
  if (senders_.count(audio_track_id) != 0 && senders_[audio_track_id]->isOpen()) {
    try {
      PLOGD << "Closing send data channel";
      senders_[audio_track_id]->close();
    } catch (std::exception &err) {
      PLOGW << "Could not close: " << err.what();
    }
  }
}

std::optional<std::chrono::milliseconds> PeerConnection::getRoundTripTime() {
  return peer_connection_->rtt();
}