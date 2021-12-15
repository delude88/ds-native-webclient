#include "KeyStore.h"

#include <keychain/keychain.h>
#include <plog/Log.h>

auto KeyStore::store(const Credentials &credentials) -> bool {
  PLOGD << "store";
  keychain::Error error{};
  keychain::setPassword(KEYSTORE_PACKAGE,
                        KEYSTORE_SERVICE,
                        credentials.email,
                        credentials.password,
                        error);
  if (error) {
    PLOGE << "Storing of password failed: "
              << error.message;
    return false;
  }
  return true;
}

auto KeyStore::restore(const std::string &email) -> std::optional<KeyStore::Credentials> {
  PLOGD << "restore";
  keychain::Error error{};
  auto password = keychain::getPassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    PLOGE << "Restore of password failed: "
              << error.message;
    return std::nullopt;
  }
  return Credentials{email, password};
}

auto KeyStore::remove(const std::string &email) -> bool {
  PLOGD << "remove";
  keychain::Error error{};
  keychain::deletePassword(KEYSTORE_PACKAGE, KEYSTORE_SERVICE, email, error);
  if (error) {
    PLOGE << "Reset of password failed: "
              << error.message;
    return false;
  }
  return true;
}

auto KeyStore::restoreEmail() -> std::optional<std::string> {
  PLOGD << "restoreEmail " << EMAIL_IDENTIFIER;
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
  PLOGD << "storeEmail" << email;
  auto* config = new wxConfig(KEYSTORE_PACKAGE);
  config->Write("email", wxString(email));
  delete config;
}
