//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

// Std
#include <optional>
#include <string>

// Logger
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

// wxWidgets
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/app.h>
#include "LoginDialog.h"
#include "TaskBarIcon.h"
#include "KeyStore.h"

// Core
#include <Client.h>
#include <DigitalStage/Auth/AuthService.h>

class App : public wxApp {
 public:
  bool OnInit() override;

 private:
  void logIn(const std::string &email, const std::string &password);
  void logOut();
  static void openStage();
  void openSettings();
  void restart();

  std::optional<std::string> tryAutoLogin(const std::string &email);
  void start();
  void stop();

  std::string device_id_;
  std::optional<std::string> token_;
  std::unique_ptr<DigitalStage::Auth::AuthService> auth_service_;
  std::shared_ptr<DigitalStage::Api::Client> api_client_;
  std::unique_ptr<Client> client_;

  TaskBarIcon *tray_icon_;
  LoginDialog *login_dialog_;
};
