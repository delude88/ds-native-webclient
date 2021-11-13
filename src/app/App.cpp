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

App::App()
    : login_dialog_(new LoginDialog()),
      device_id_(std::to_string(deviceid::get())),
      tray_icon_(new TrayIcon(this)),
      key_store_(new KeyStore(this)),
      auth_service_(std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL)) {
  QApplication::setQuitOnLastWindowClosed(false);

  connect(tray_icon_, SIGNAL(loginClicked()), login_dialog_, SLOT(show()));
  connect(tray_icon_, SIGNAL(logoutClicked()), this, SLOT(logout()));
  connect(tray_icon_, &TrayIcon::openStageClicked, []() {
    QDesktopServices::openUrl(QUrl(STAGE_URL));
  });
  connect(login_dialog_, SIGNAL(login(QString, QString)), SLOT(login(QString, QString)));
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
    start(*token_);
  }
}

std::optional<std::string> App::autoLogin() {
  auto email = KeyStore::restoreEmail();
  if (email) {
    // Try to login using stored credentials
    auto credentials = KeyStore::restore(*email);
    if (credentials) {
      // Try to get token
      auto token = auth_service_->signInSync(credentials->email, credentials->password);
      if (!token.empty())
        return token;
    }
  }
  return std::nullopt;
}

void App::login(const QString &email, const QString &password) {
  auth_service_->signIn(email.toStdString(), password.toStdString())
      .then([=](const std::string &token) {
        token_ = token;
        KeyStore::storeEmail(email.toStdString());
        KeyStore::store({email.toStdString(), password.toStdString()});
        start(token);
      });
}

void App::logout() {
  login_dialog_->setPassword("");
  if (token_)
    auth_service_->signOut(*token_)
        .then([=](bool) {
          token_ = std::nullopt;
          stop();
        });
}
void App::start(const std::string &token) {
  login_dialog_->hide();
  tray_icon_->showStatusMenu();

  auto apiClient = std::make_shared<DigitalStage::Api::Client>(API_URL);
  client_ = std::make_unique<Client>(apiClient);// Describe this device
  nlohmann::json initialDeviceInformation;
  // - always use an UUID when you want Digital Stage to remember this device and its settings
  initialDeviceInformation["uuid"] = device_id_;
  initialDeviceInformation["type"] = "native";
  initialDeviceInformation["canAudio"] = true;
  initialDeviceInformation["canVideo"] = false;

  // And finally connect with the token and device description
  apiClient->disconnected.connect([=](bool) {
    stop();
  });
  apiClient->connect(token, initialDeviceInformation);
}
void App::stop() {
  client_ = nullptr;
  tray_icon_->showLoginMenu();
  login_dialog_->show();
}
