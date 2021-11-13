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

  explicit KeyStore(QObject *parent = nullptr);

  static bool store(const Credentials& credentials);
  static std::optional<Credentials> restore(const std::string& email) ;
  [[nodiscard]] static bool remove(const std::string& email) ;

  static std::optional<std::string> restoreEmail();
  static void storeEmail(const std::string& email);
};

