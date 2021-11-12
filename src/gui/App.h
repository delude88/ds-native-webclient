//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QObject>
#include <DigitalStage/Auth/AuthService.h>
#include "LoginDialog.h"
#include "TrayIcon.h"
#include "KeyStore.h"
#include "../Client.h"
#include <optional>
#include <string>

class App : public QObject {
 Q_OBJECT

 public:
  App();
  void show();

 private slots:
  std::optional<std::string> autoLogin();
  void login(const QString &email, const QString &password);
  void logout();

 private:
  void start(const std::string &token);
  void stop();

  std::string device_id_;
  std::optional<std::string> token_;
  std::unique_ptr<Client> client_;
  std::unique_ptr<KeyStore> key_store_;
  std::unique_ptr<DigitalStage::Auth::AuthService> auth_service_;
  TrayIcon *tray_icon_;
  LoginDialog *login_dialog_;
};
