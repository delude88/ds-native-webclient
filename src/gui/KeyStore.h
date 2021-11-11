//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QObject>
#include <string>
#include <optional>

#ifndef KEYSTORE_PACKAGE
#define KEYSTORE_PACKAGE "org.digital-stage.client"
#endif
#ifndef KEYSTORE_SERVICE
#define KEYSTORE_SERVICE "auth"
#endif

class KeyStore : QObject {
 Q_OBJECT

 public:
  struct Credentials {
    std::string email;
    std::string password;
  };

  KeyStore();
  ~KeyStore();

  bool store(const Credentials& credentials);
  static std::optional<Credentials> restore(const std::string& email) ;
  [[nodiscard]] bool remove(const std::string& email) const;

  std::optional<std::string> restoreEmail();
  void storeEmail(const std::string& email);
};

