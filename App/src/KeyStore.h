//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <string>
#include <optional>

#ifndef KEYSTORE_PACKAGE
#define KEYSTORE_PACKAGE "org.digital-stage.client"
#endif
#ifndef KEYSTORE_SERVICE
#define KEYSTORE_SERVICE "auth"
#endif
#ifndef EMAIL_IDENTIFIER
#define EMAIL_IDENTIFIER "org.digital-stage.client.email"
#endif

class KeyStore {

 public:
  struct Credentials {
    std::string email;
    std::string password;
  };

  explicit KeyStore();

  static bool store(const Credentials& credentials);
  static std::optional<Credentials> restore(const std::string& email) ;
  [[nodiscard]] static bool remove(const std::string& email) ;

  static std::optional<std::string> restoreEmail();
  static void storeEmail(const std::string& email);
};

