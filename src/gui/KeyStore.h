//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
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
    QString email;
    QString password;
  };

  KeyStore();
  ~KeyStore() override;

  bool store(const Credentials& credentials);
  static std::optional<Credentials> restore(const QString& email) ;
  bool remove(const QString& email) const;

  std::optional<QString> restoreEmail();
  void storeEmail(const QString& email);
};

