//
// Created by Tobias Hegemann on 11.11.21.
//
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <deviceid/id.h>
#include "App.h"
#include "KeyStore.h"
#include <DigitalStage/Api/Client.h>
#include <plog/Log.h>

App::App()
    : login_dialog_(new LoginDialog()),
      device_id_(std::to_string(deviceid::get())),
      tray_icon_(new TrayIcon()),
      auth_service_(std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL)) {
  //QApplication::setQuitOnLastWindowClosed(false);

  connect(tray_icon_, SIGNAL(loginClicked()), login_dialog_, SLOT(show()));
  connect(tray_icon_, SIGNAL(restartClicked()), this, SLOT(restart()));
  connect(tray_icon_, SIGNAL(logoutClicked()), this, SLOT(logOut()));
  connect(tray_icon_, SIGNAL(openStageClicked()), this, SLOT(openStage()));
  connect(tray_icon_, SIGNAL(openSettingsClicked()), this, SLOT(openSettings()));
  // clang-format off
  connect(login_dialog_, SIGNAL(login(QString,QString)), SLOT(logIn(QString,QString)));
  // clang-format on
}

void App::show() {
  tray_icon_->show();
  // Try to auto sign in
  auto email = KeyStore::restoreEmail();
  if (email) {
    login_dialog_->setEmail(*email);
    token_ = tryAutoLogin(*email);
  }
  if (!token_.has_value()) {
    // Show login menu and dialog
    tray_icon_->showLoginMenu();
    login_dialog_->show();
  } else {
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->hide();
    start();
  }
}

std::optional<QString> App::tryAutoLogin(const QString &email) {
  // Try to log in using stored credentials
  auto credentials = KeyStore::restore(email);
  if (credentials) {
    // Try to get token
    auto token = auth_service_->signInSync(credentials->email.toStdString(), credentials->password.toStdString());
    if (token.has_value()) {
      return QString::fromStdString(*token);
    }
    if (!KeyStore::remove(email)) {
      PLOGE << "Could not reset credentials for " << email;
    }
  }
  return std::nullopt;
}

void App::logIn(const QString &email, const QString &password) {
  login_dialog_->setLoading(true);
  login_dialog_->resetError();
  try {
    auto strEmail = email.toStdString();
    auto stdPassword = email.toStdString();
    auto token = auth_service_->signInSync(strEmail, stdPassword);
    if (token.has_value()) {
      token_ = QString::fromStdString(*token);
    }
    KeyStore::storeEmail(email);
    KeyStore::store({email, password});
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->setLoading(false);
    login_dialog_->hide();
    start();
  } catch (std::exception &ex) {
    login_dialog_->setLoading(false);
    login_dialog_->setError(ex.what());
  }
}

void App::logOut() {
  stop();
  tray_icon_->showLoginMenu();
  login_dialog_->setPassword("");
  if (token_.has_value()) {
    auto token = token_->toStdString();
    auth_service_->signOutSync(token);
    token_ = std::nullopt;
  }
  login_dialog_->show();
}

void App::start() {
  PLOGI << "Starting";
  if (!token_.has_value()) {
    throw std::logic_error("No token available");
  }

  api_client_ = std::make_shared<DigitalStage::Api::Client>(API_URL);
  client_ = std::make_unique<Client>(api_client_);// Describe this device
  nlohmann::json initialDeviceInformation;
  // - always use a UUID when you want Digital Stage to remember this device and its settings
  initialDeviceInformation["uuid"] = device_id_;
  initialDeviceInformation["type"] = "native";
  initialDeviceInformation["canAudio"] = true;
  initialDeviceInformation["sendAudio"] = false;
  initialDeviceInformation["receiveAudio"] = true;
  initialDeviceInformation["canVideo"] = false;
#ifdef USE_RT_AUDIO
  initialDeviceInformation["audioEngine"] = "rtaudio";
#else
  initialDeviceInformation["audioEngine"] = "miniaudio";
#endif

  // And finally connect with the token and device description
  api_client_->disconnected.connect([=](bool expected) {
    if (!expected) {
      PLOGE << "Disconnected unexpected - calling stop()";
      stop();
    }
  });
  auto token = token_->toStdString();
  PLOGE << "Connecting ...";
  api_client_->connect(token, initialDeviceInformation);
}
void App::stop() {
  PLOGE << "Stopping";
  client_ = nullptr;
  api_client_ = nullptr;
}
void App::restart() {
  PLOGE << "Restarting";
  stop();
  start();
}

void App::openStage() {
  QDesktopServices::openUrl(QUrl(STAGE_URL));
}

void App::openSettings() {
  auto uri = std::string(SETTINGS_URL);
  uri += "/" + device_id_;
  std::cout << "Opening" << uri << std::endl;
  QDesktopServices::openUrl(QUrl(SETTINGS_URL));
}