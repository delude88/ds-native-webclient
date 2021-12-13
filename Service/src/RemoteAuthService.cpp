//
// Created by Tobias Hegemann on 05.11.21.
//
#include <ixwebsocket/IXHttpServer.h>
#include "RemoteAuthService.h"
#include "AuthIO.h"
#include <plog/Log.h>

RemoteAuthService::RemoteAuthService() : auth_service_(std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL)) {
  init();
}

RemoteAuthService::~RemoteAuthService() {
  stop();
}

void RemoteAuthService::start() {
  stop();
  utility::string_t port = U("3000");
  utility::string_t address = U("http://127.0.0.1:");
  address.append(port);
  web::uri_builder uri(address);
  auto addr = uri.to_uri().to_string();
  listener_ = std::make_unique<web::http::experimental::listener::http_listener>(addr);
  listener_->support(web::http::methods::POST, [this](auto &&PH1) { handlePost(std::forward<decltype(PH1)>(PH1)); });
  listener_->open();
}

void RemoteAuthService::stop() {
  if (listener_)
    listener_->close();
}

void RemoteAuthService::init() {
  auto token = AuthIO::readToken();
  if (!token.empty()) {
    if (auth_service_->verifyTokenSync(token)) {
      AuthIO::writeToken(token);
      token_ = token;
      onLogin(token);
    }
  }
}

void RemoteAuthService::handlePost(const web::http::http_request & /*message*/) {
  auto paths = web::http::uri::split_path(web::http::uri::decode(message.relative_uri().path()));
  if (!paths.empty()) {
    const std::string &path = paths[0];
    PLOGD << path << std::endl;
    if (path == "login") {
      auto json = message.extract_json().get();
      if (json.has_field("email") && json.has_field("password")) {
        auto email = json["email"].as_string();
        auto password = json["password"].as_string();
        try {
          auto token = auth_service_->signInSync(email, password);
          AuthIO::writeToken(token);
          token_ = token;
          onLogin(token);
        } catch (DigitalStage::Auth::AuthError &error) {
          token_ = std::nullopt;
          PLOGE << error.getCode() << ": " << error.what();
        }
      }
    } else if (path == "logout") {
      logout();
    }
  }
  message.reply(web::http::status_codes::OK, message.to_string());
}
void RemoteAuthService::logout() {
  AuthIO::resetToken();
  if (token_)
    auth_service_->signOut(U(*token_));
  onLogout();
}
