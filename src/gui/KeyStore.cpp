#include "KeyStore.h"
#include <QString>
#include <QSettings>
#include <iostream>

#include <../../include/keychain/include/keychain/keychain.h>

KeyStore::KeyStore() {}

KeyStore::~KeyStore() {}

bool KeyStore::store(const Credentials &credentials) {
  keychain::Error error{};
  keychain::setPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, credentials.email, "hunter2", error);
  if (error) {
    std::cerr << "Storing of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

std::optional<KeyStore::Credentials> KeyStore::restore(const std::string &email) {
  keychain::Error error{};
  auto password = keychain::getPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    std::cerr << "Restore of password failed: "
              << error.message << std::endl;
    return std::nullopt;
  }
  return Credentials{email, password};
}

bool KeyStore::remove(const std::string &email) const {
  keychain::Error error{};
  keychain::deletePassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    std::cerr << "Reset of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

std::optional<std::string> KeyStore::restoreEmail() {
  QSettings settings(KEYSTORE_PACKAGE, "Client");
  if (settings.contains("email")) {
    return settings.value("email", "").toString().toStdString();
  }
  return std::nullopt;
}

void KeyStore::storeEmail(const std::string &email) {
  QSettings settings(KEYSTORE_PACKAGE, "Client");
  settings.setValue("email", QString::fromStdString(email));
}