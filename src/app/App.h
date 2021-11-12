//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QObject>

#include "LoginDialog.h"
#include "TrayIcon.h"
#include "KeyStore.h"
#include <optional>
#include <string>

#ifndef Q_MOC_RUN
#include <Client.h>
#include <DigitalStage/Auth/AuthService.h>
#endif

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
  KeyStore* key_store_;
  std::unique_ptr<DigitalStage::Auth::AuthService> auth_service_;
  TrayIcon *tray_icon_;
  LoginDialog *login_dialog_;
};
