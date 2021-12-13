//
// Created by Tobias Hegemann on 18.10.21.
//
#pragma once

#include <iostream>
#include <optional>
#include <DigitalStage/Auth/AuthService.h>
#include <plog/Log.h>
#include "AuthIO.h"

inline std::pair<std::string, std::string> sign_in() {
  std::string email;
  std::string password;
  std::cout << "Please enter your email: ";
  std::cin >> email;
  std::cout << "Please enter your password: ";
  std::cin >> password;
  return {email, password};
}

inline std::string authenticate_user() {
  auto *service = new DigitalStage::Auth::AuthService(AUTH_URL);
  // Read last token
  auto token = AuthIO::readToken();
  if (!token.empty()) {
    // Validate token
    if (service->verifyTokenSync(token)) {
      return token;
    }
  }
  std::cout << "Don't have an account? Go get one at " << SIGNUP_URL << std::endl << std::endl;
  do {
    auto credentials = sign_in();
    try {
      auto received_token = service->signInSync(credentials.first, credentials.second);
      AuthIO::writeToken(received_token);
      return received_token;
    } catch (DigitalStage::Auth::AuthError &error) {
      PLOGE << error.getCode() << ": " << error.what();
    }
  } while (true);
}
