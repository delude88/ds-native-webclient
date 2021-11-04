//
// Created by Tobias Hegemann on 18.10.21.
//
#include <iostream>
#include <optional>
#include <DigitalStage/Auth/AuthService.h>

static std::string read_token() {
  std::ifstream file;
  file.open("token");
  if (file.is_open()) {
    std::string line;
    if (getline(file, line)) {
      return line;
    }
  }
  return "";
}

static void store_token(const std::string &token) {
  std::ofstream file;
  file.open("token");
  file << token;
  file.close();
}

static std::pair<std::string, std::string> sign_in() {
  std::string email, password;
  std::cout << "Please enter your email: ";
  std::cin >> email;
  std::cout << "Please enter your password: ";
  std::cin >> password;
  return {email, password};
}

static std::string authenticate_user() {
  auto service = new DigitalStage::Auth::AuthService(AUTH_URL);
  // Read last token
  auto token = read_token();
  if (!token.empty()) {
    // Validate token
    if (service->verifyTokenSync(token)) {
      return token;
    }
  }
  std::cout << "Don't have an account? Go get one at " << SIGNUP_URL << std::endl << std::endl;
  do {
    auto credentials = sign_in();
    auto receivedToken = service->signInSync(credentials.first, credentials.second);
    if (!receivedToken.empty()) {
      store_token(receivedToken);
      return receivedToken;
    }
  } while (true);
}
