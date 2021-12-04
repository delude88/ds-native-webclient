#include "KeyStore.h"
#include <iostream>

#include <keychain/keychain.h>
#include <plog/Log.h>

auto KeyStore::store(const Credentials &credentials) -> bool {
  PLOGI << "store";
  keychain::Error error{};
  keychain::setPassword(KEYSTORE_PACKAGE,
                        KEYSTORE_SERVICE,
                        credentials.email,
                        credentials.password,
                        error);
  if (error) {
    std::cerr << "Storing of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

auto KeyStore::restore(const std::string &email) -> std::optional<KeyStore::Credentials> {
  PLOGI << "restore";
  keychain::Error error{};
  auto password = keychain::getPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    std::cerr << "Restore of password failed: "
              << error.message << std::endl;
    return std::nullopt;
  }
  return Credentials{email, password};
}

auto KeyStore::remove(const std::string &email) -> bool {
  PLOGI << "remove";
  keychain::Error error{};
  keychain::deletePassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    std::cerr << "Reset of password failed: "
              << error.message << std::endl;
    return false;
  }
  return true;
}

auto KeyStore::restoreEmail() -> std::optional<std::string> {
  PLOGI << "restoreEmail " << EMAIL_IDENTIFIER;
  auto* config = new wxConfig(KEYSTORE_PACKAGE);
  if (config->HasEntry("email")) {
    auto value = config->Read("email").ToStdString();
    delete config;
    return value;
  }
  delete config;
  return std::nullopt;
}

void KeyStore::storeEmail(const std::string &email) {
  PLOGI << "storeEmail" << email;
  auto* config = new wxConfig(KEYSTORE_PACKAGE);
  config->Write("email", wxString(email));
  delete config;
}
