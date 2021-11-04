//
// Created by Tobias Hegemann on 22.10.21.
//

#include "WebRTCPeerConnection.h"
#include "../audio/AudioProvider.h"
#include "../../webrtc/WebRTCPeerConnection.h"

WebRTCPeerConnection::WebRTCPeerConnection(
    const std::string &stage_device_id,
    bool polite
) :
    stage_device_id_(stage_device_id),
    connection_observer_(*this),
    making_offer_(false),
    ignore_offer_(false),
    srd_answer_pending_(false),
    polite_(polite),
    data_channel_observer_(*this),
    create_session_description_observer_(new rtc::RefCountedObject<CreateSessionDescriptionObserver>(*this)),
    set_session_description_observer_(new rtc::RefCountedObject<SetSessionDescriptionObserver>(*this)) {
}

WebRTCPeerConnection::~WebRTCPeerConnection() {
  if (peer_connection_)
    peer_connection_->Close();
}

void WebRTCPeerConnection::connect(
    const rtc::scoped_refptr<webrtc::WebRTCPeerConnectionFactoryInterface> &factory,
    const webrtc::WebRTCPeerConnectionInterface::RTCConfiguration &configuration
) {
  assert(!peer_connection_);
  auto connection_result = factory->CreateWebRTCPeerConnectionOrError(
      configuration,
      webrtc::WebRTCPeerConnectionDependencies(&connection_observer_)
  );
  if (!connection_result.ok()) {
    throw std::runtime_error(connection_result.error().message());
  }
  // Store peer connection
  peer_connection_ = connection_result.MoveValue();

  // Create data channel
  webrtc::DataChannelInit config;
  auto data_channel_result = peer_connection_->CreateDataChannelOrError("data_channel", &config);
  if (!data_channel_result.ok()) {
    throw std::runtime_error(data_channel_result.error().message());
  }
  data_channel_ = data_channel_result.MoveValue();
  data_channel_->RegisterObserver(&data_channel_observer_);

  // And finally make an offer
  makeOffer();
}

void WebRTCPeerConnection::makeOffer() {
  if (peer_connection_->signaling_state() != webrtc::WebRTCPeerConnectionInterface::SignalingState::kStable) {
    return;
  }
  if (making_offer_) {
    return;
  }
  making_offer_ = true;
  peer_connection_->CreateOffer(
      create_session_description_observer_,
      webrtc::WebRTCPeerConnectionInterface::RTCOfferAnswerOptions()
  );
}

void WebRTCPeerConnection::handleIceCandidate(const webrtc::IceCandidateInterface *candidate) {
  DigitalStage::Types::IceCandidateInit ice_candidate_init;
  std::string ice_candidate;
  candidate->ToString(&ice_candidate);
  ice_candidate_init.candidate = ice_candidate;
  ice_candidate_init.sdpMid = candidate->sdp_mid();
  ice_candidate_init.sdpMLineIndex = candidate->sdp_mline_index();
  onIceCandidate(ice_candidate_init);
}

void WebRTCPeerConnection::addIceCandidate(const DigitalStage::Types::IceCandidateInit &ice_candidate_init) {
  assert(peer_connection_);
  webrtc::SdpParseError err_sdp;
  webrtc::IceCandidateInterface *ice_candidate = CreateIceCandidate(
      ice_candidate_init.sdpMid,
      ice_candidate_init.sdpMLineIndex,
      ice_candidate_init.candidate, &err_sdp);
  if (!err_sdp.line.empty() && !err_sdp.description.empty()) {
    std::cout << stage_device_id_ << ":" << std::this_thread::get_id() << ":"
              << "Error on CreateIceCandidate" << std::endl
              << err_sdp.line << std::endl
              << err_sdp.description << std::endl;
    if (!ignore_offer_) {
      exit(EXIT_FAILURE);
    } else {
      std::cout << "Ignoring error on ice candidate while ignoring offer" << std::endl;
    }
  }
  peer_connection_->AddIceCandidate(ice_candidate);
}

void WebRTCPeerConnection::setSessionDescription(const DigitalStage::Types::SessionDescriptionInit &session_description_init) {
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface *session_description(
      webrtc::CreateSessionDescription("answer", session_description_init.sdp, &error));
  if (session_description == nullptr) {
    std::cout << stage_device_id_ << ":" << std::this_thread::get_id() << ":"
              << "Error on CreateSessionDescription." << std::endl
              << error.line << std::endl
              << error.description << std::endl;
    std::cout << stage_device_id_ << ":" << std::this_thread::get_id() << ":"
              << "Answer SDP:begin" << std::endl
              << session_description_init.sdp << std::endl
              << "Answer SDP:end" << std::endl;
    exit(EXIT_FAILURE);
  }

  bool is_stable = peer_connection_->signaling_state() == webrtc::WebRTCPeerConnectionInterface::SignalingState::kStable ||
      (peer_connection_->signaling_state() == webrtc::WebRTCPeerConnectionInterface::SignalingState::kHaveLocalOffer
          && srd_answer_pending_);
  ignore_offer_ =
      session_description->GetType() == webrtc::SdpType::kOffer && !polite_ && (making_offer_ || !is_stable);
  if (ignore_offer_) {
    std::cout << "glare - ignoring offer" << std::endl;
    return;
  }
  if (session_description->GetType() == webrtc::SdpType::kAnswer) {
    srd_answer_pending_ = true;
    peer_connection_->SetRemoteDescription(set_session_description_observer_, session_description);
    // Follow answer handling of handleSessionDescription() for further steps
  } else if (session_description->GetType() == webrtc::SdpType::kOffer) {
    if (peer_connection_->signaling_state() != webrtc::WebRTCPeerConnectionInterface::SignalingState::kHaveRemoteOffer) {
      throw std::runtime_error("Not a remote offer");
    }
    if (peer_connection_->remote_description()->GetType() != webrtc::SdpType::kOffer) {
      throw std::runtime_error("SRD did not work");
    }
    std::cout << "SLD to get back to stable" << std::endl;
    peer_connection_->CreateAnswer(create_session_description_observer_,
                                   webrtc::WebRTCPeerConnectionInterface::RTCOfferAnswerOptions());
    // Follow offer handling of handleSessionDescription() for further steps
  } else {
    std::cout << "Unknown session description of type " << SdpTypeToString(session_description->GetType()) << std::endl;
  }
}
void WebRTCPeerConnection::handleSessionDescription(webrtc::SessionDescriptionInterface *desc) {
  peer_connection_->SetLocalDescription(set_session_description_observer_, desc);

  if (desc->GetType() == webrtc::SdpType::kOffer) {
    if (peer_connection_->signaling_state() != webrtc::WebRTCPeerConnectionInterface::SignalingState::kHaveLocalOffer) {
      throw std::runtime_error("negotiationneeded racing with onmessage");
    }
    if (peer_connection_->local_description()->GetType() != webrtc::SdpType::kOffer) {
      throw std::runtime_error("negotiationneeded SLD did not work");
    }
    std::string sdp;
    desc->ToString(&sdp);
    DigitalStage::Types::SessionDescriptionInit session_description_init;
    session_description_init.sdp = sdp;
    session_description_init.type = SdpTypeToString(desc->GetType());
    onSessionDescription(session_description_init);
    making_offer_ = false;  // Possible duplicate, done also inside the create_session_description_observer_
  } else if (desc->GetType() == webrtc::SdpType::kAnswer) {
    if (peer_connection_->signaling_state() != webrtc::WebRTCPeerConnectionInterface::SignalingState::kStable) {
      throw std::runtime_error("onmessage racing with negotiationneeded");
    }
    if (peer_connection_->local_description()->GetType() != webrtc::SdpType::kAnswer) {
      throw std::runtime_error("negotiationneeded SLD did not work");
    }
    // Send answer to other peer
    std::string sdp;
    desc->ToString(&sdp);
    DigitalStage::Types::SessionDescriptionInit session_description_init;
    session_description_init.sdp = sdp;
    session_description_init.type = SdpTypeToString(desc->GetType());
    onSessionDescription(session_description_init);
  }

}
void WebRTCPeerConnection::handleStateChanged() {
  std::cout << "state: " << data_channel_->state() << std::endl;
  if (data_channel_->state() == webrtc::DataChannelInterface::kOpen) {
    onSuccess();
  }
}

std::string WebRTCPeerConnection::getStageDeviceId() {
  return stage_device_id_;
}

void WebRTCPeerConnection::send(const webrtc::DataBuffer &buffer) {
  if (data_channel_->buffered_amount() < data_channel_->MaxSendQueueSize()) {
    data_channel_->Send(buffer);
  } else {
    //TODO: Ensure, rtc buffer is not full
    std::cout << "We definitely need a dynamic buffer here" << std::endl;
  }
}
void WebRTCPeerConnection::ConnectionObserver::OnSignalingChange(webrtc::WebRTCPeerConnectionInterface::SignalingState new_state) {
  std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
            << "WebRTCPeerConnectionObserver::SignalingChange(" << new_state << ")" << std::endl;
}
void WebRTCPeerConnection::ConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
            << "WebRTCPeerConnectionObserver::AddStream" << std::endl;
}
