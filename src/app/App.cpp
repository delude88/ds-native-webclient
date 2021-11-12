//
// Created by Tobias Hegemann on 11.11.21.
//
#include <QApplication>
#include <deviceid/id.h>
#include "App.h"
#include "KeyStore.h"

#ifndef Q_MOC_RUN
#include <DigitalStage/Api/Client.h>
#include <audio/RtAudioIO.h>
#endif

App::App()
    : login_dialog_(new LoginDialog()),
      device_id_(std::to_string(deviceid::get())),
      tray_icon_(new TrayIcon(this)),
      key_store_(new KeyStore(this)),
      auth_service_(std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL)) {
  QApplication::setQuitOnLastWindowClosed(false);

  connect(tray_icon_, SIGNAL(loginClicked()), login_dialog_, SLOT(show()));
  connect(tray_icon_, SIGNAL(logoutClicked()), this, SLOT(logout()));
  connect(login_dialog_, SIGNAL(login(const QString&, const QString&)), SLOT(login(const QString &, const QString&)));
}

void App::show() {
  // Try to auto sign in
  token_ = autoLogin();
  tray_icon_->show();
  if (!token_) {
    tray_icon_->showLoginMenu();
    // Show login and wait for result
    login_dialog_->show();
  } else {
    // Start
    tray_icon_->showStatusMenu();
  }
}

std::optional<std::string> App::autoLogin() {
  std::optional<std::string> token;
  auto email = key_store_->restoreEmail();
  if (email) {
    // Try to login using stored credentials
    auto credentials = key_store_->restore(*email);
    if (credentials) {
      // Try to get token
      try {
        return auth_service_->signInSync(credentials->email, credentials->password);
      } catch (...) {
      }
    }
  }
  return std::nullopt;
}

void App::login(const QString &email, const QString &password) {
  auth_service_->signIn(email.toStdString(), password.toStdString())
      .then([=](const std::string &token) {
        token_ = token;
        start(token);
      });
}

void App::logout() {
  login_dialog_->setPassword("");
  if (token_)
    auth_service_->signOut(*token_)
        .then([=](bool) {
          token_ = std::nullopt;
          login_dialog_->show();
        });
}
void App::start(const std::string &token) {
  auto apiClient = std::make_unique<DigitalStage::Api::Client>(API_URL);
  auto audioIO = std::make_unique<RtAudioIO>(*apiClient);
  client_ = std::make_unique<Client>(*apiClient, *audioIO);// Describe this device
  nlohmann::json initialDeviceInformation;
  // - always use an UUID when you want Digital Stage to remember this device and its settings
  initialDeviceInformation["uuid"] = device_id_;
  initialDeviceInformation["type"] = "native";
  initialDeviceInformation["canAudio"] = true;
  initialDeviceInformation["canVideo"] = false;

  // And finally connect with the token and device description
  apiClient->connect(token, initialDeviceInformation);
}
void App::stop() {
  client_ = nullptr;
}
