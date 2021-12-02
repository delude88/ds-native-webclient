#include "KeyStore.h"
#include <QString>
#include <QSettings>
#include <iostream>

#include <keychain/keychain.h>

KeyStore::KeyStore(QObject *parent) : QObject(parent) {}

auto KeyStore::store(const Credentials &credentials) -> bool {
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

auto KeyStore::restore(const QString &email) -> std::optional<KeyStore::Credentials> {
  keychain::Error error{};
  auto password = keychain::getPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email.toStdString(), error);
  if (error) {
    std::cerr << "Restore of password failed: "
              << error.message << std::endl;
    return std::nullopt;
  }
  return Credentials{email, QString::fromStdString(password)};
}

auto KeyStore::remove(const QString &email) -> bool {
  keychain::Error error{};
  keychain::deletePassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email.toStdString(), error);
  if (error) {
    std::cerr << "Reset of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

auto KeyStore::restoreEmail() -> std::optional<QString> {
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