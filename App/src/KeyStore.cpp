#include "KeyStore.h"
#include <QString>
#include <QSettings>
#include <iostream>

#include <keychain/keychain.h>

KeyStore::KeyStore(QObject *parent) : QObject(parent) {}

bool KeyStore::store(const Credentials &credentials) {
  keychain::Error error{};
  keychain::setPassword(KEYSTORE_PACKAGE,
                        KEYSTORE_SERVICE,
                        credentials.email.toStdString(),
                        credentials.password.toStdString(),
                        error);
  if (error) {
    std::cerr << "Storing of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

std::optional<KeyStore::Credentials> KeyStore::restore(const QString &email) {
  keychain::Error error{};
  auto password = keychain::getPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email.toStdString(), error);
  if (error) {
    std::cerr << "Restore of password failed: "
              << error.message << std::endl;
    return std::nullopt;
  }
  return Credentials{email, QString::fromStdString(password)};
}

bool KeyStore::remove(const QString &email) {
  keychain::Error error{};
  keychain::deletePassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email.toStdString(), error);
  if (error) {
    std::cerr << "Reset of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

std::optional<QString> KeyStore::restoreEmail() {
  QSettings settings(KEYSTORE_PACKAGE, "Client");
  if (settings.contains("email")) {
    return settings.value("email", "").toString();
  }
  return std::nullopt;
}

void KeyStore::storeEmail(const QString &email) {
  QSettings settings(KEYSTORE_PACKAGE, "Client");
  settings.setValue("email", email);
}