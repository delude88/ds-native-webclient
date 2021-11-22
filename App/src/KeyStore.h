//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QObject>
#include <string>
#include <optional>
#include <cpprest/details/basic_types.h>

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
    QString email;
    QString password;
  };

  explicit KeyStore(QObject *parent = nullptr);

  static bool store(const Credentials& credentials);
  static std::optional<Credentials> restore(const QString& email) ;
  [[nodiscard]] static bool remove(const QString& email) ;

  static std::optional<QString> restoreEmail();
  static void storeEmail(const QString& email);
};

