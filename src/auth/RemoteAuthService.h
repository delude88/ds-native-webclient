//
// Created by Tobias Hegemann on 05.11.21.
//
#pragma once

#include <sigslot/signal.hpp>
#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <DigitalStage/Auth/AuthService.h>
#include <memory>

class RemoteAuthService {
 public:
  RemoteAuthService();
  ~RemoteAuthService();

  void start();
  void stop();

  Pal::sigslot::signal<std::string> onLogin;
  Pal::sigslot::signal<> onLogout;
 private:
  void init();

  void handlePost(const web::http::http_request& message);

  void logout();

  std::optional<std::string> token_;
  std::unique_ptr<web::http::experimental::listener::http_listener> listener_;
  std::unique_ptr<DigitalStage::Auth::AuthService> auth_service_;
};
