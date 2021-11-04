//
// Created by Tobias Hegemann on 22.10.21.
//

#ifndef CLIENT_SRC_WEBRTC_PEERCONNECTION_H_
#define CLIENT_SRC_WEBRTC_PEERCONNECTION_H_

#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <condition_variable>
#include <api/jsep.h>
#include <DigitalStage/Types.h>
#include <api/create_peerconnection_factory.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <system_wrappers/include/field_trial.h>
#include <sigslot/signal.hpp>

class WebRTCPeerConnection {
 public:
  PeerConnection(
      const std::string &stage_device_id,
      bool polite
  );
  ~PeerConnection();

  void connect(
      const rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> &factory,
      const webrtc::PeerConnectionInterface::RTCConfiguration &configuration
  );

  std::string getStageDeviceId();

  void addIceCandidate(const DigitalStage::Types::IceCandidateInit &ice_candidate);

  void setSessionDescription(const DigitalStage::Types::SessionDescriptionInit &session_description);

  void send(const webrtc::DataBuffer &);

  Pal::sigslot::signal<> onAcceptIce;
  Pal::sigslot::signal<const webrtc::DataBuffer &> onData;
  Pal::sigslot::signal<const DigitalStage::Types::IceCandidateInit &> onIceCandidate;
  Pal::sigslot::signal<const DigitalStage::Types::SessionDescriptionInit &> onSessionDescription;
  Pal::sigslot::signal<> onSuccess;
 private:
  void handleStateChanged();
  void handleSessionDescription(webrtc::SessionDescriptionInterface *desc);
  void handleIceCandidate(const webrtc::IceCandidateInterface *candidate);

  void makeOffer();

  const std::string &stage_device_id_;
  bool polite_;
  bool making_offer_;
  bool ignore_offer_;
  bool srd_answer_pending_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  // Listener sub classes
  class ConnectionObserver : public webrtc::PeerConnectionObserver {
   public:
    explicit ConnectionObserver(PeerConnection &parent) : parent(parent) {
    }
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;

    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;

    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::RemoveStream" << std::endl;
    };

    inline void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::DataChannel(" << data_channel << ", " << parent.data_channel_.get() << ")"
                << std::endl;
      parent.data_channel_ = data_channel;
      parent.data_channel_->RegisterObserver(&parent.data_channel_observer_);
    };

    inline void OnRenegotiationNeeded() override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::RenegotiationNeeded" << std::endl;
      parent.makeOffer();
    };

    inline void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceConnectionChange(" << new_state << ")" << std::endl;
    };

    inline void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceGatheringChange(" << new_state << ")" << std::endl;
    };

    inline void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "PeerConnectionObserver::IceCandidate" << std::endl;
      parent.handleIceCandidate(candidate);
    };
   private:
    PeerConnection &parent;
  };
  class DataChannelObserver : public webrtc::DataChannelObserver {
   private:
    PeerConnection &parent;

   public:
    explicit DataChannelObserver(PeerConnection &parent) : parent(parent) {
    }

    void OnStateChange() override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "DataChannelObserver::StateChange" << std::endl;
      parent.handleStateChanged();
    };

    // Message receipt
    void OnMessage(const webrtc::DataBuffer &buffer) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "DataChannelObserver::Message" << std::endl;
      assert(buffer.binary);
      parent.onData(buffer);
    };

    void OnBufferedAmountChange(uint64_t previous_amount) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "DataChannelObserver::BufferedAmountChange(" << previous_amount << ")" << std::endl;
    };
  };

  class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
   private:
    PeerConnection &parent;

   public:
    explicit CreateSessionDescriptionObserver(PeerConnection &parent) : parent(parent) {
    }

    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "CreateSessionDescriptionObserver::OnSuccess" << std::endl;
      parent.handleSessionDescription(desc);
      parent.making_offer_ = false;
    };

    void OnFailure(webrtc::RTCError error) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "CreateSessionDescriptionObserver::OnFailure" << std::endl
                << error.message() << std::endl;
      parent.making_offer_ = false;
    };
  };
  class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
   private:
    PeerConnection &parent;

   public:
    explicit SetSessionDescriptionObserver(PeerConnection &parent) : parent(parent) {
    }

    void OnSuccess() override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "SetSessionDescriptionObserver::OnSuccess" << std::endl;
      parent.onAcceptIce();
    };

    void OnFailure(webrtc::RTCError error) override {
      std::cout << parent.getStageDeviceId() << ":" << std::this_thread::get_id() << ":"
                << "SetSessionDescriptionObserver::OnFailure" << std::endl
                << error.message() << std::endl;
    };
  };

  ConnectionObserver connection_observer_;
  DataChannelObserver data_channel_observer_;
  rtc::scoped_refptr<CreateSessionDescriptionObserver> create_session_description_observer_;
  rtc::scoped_refptr<SetSessionDescriptionObserver> set_session_description_observer_;
};

#endif //CLIENT_SRC_WEBRTC_PEERCONNECTION_H_
