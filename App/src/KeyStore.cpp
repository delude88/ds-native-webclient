#include "KeyStore.h"
#include <iostream>
#include <wx/sysopt.h>

#include <keychain/keychain.h>

KeyStore::KeyStore() = default;

auto KeyStore::store(const Credentials &credentials) -> bool {
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
  if (wxSystemOptions::HasOption(EMAIL_IDENTIFIER)) {
    return wxSystemOptions::GetOption(EMAIL_IDENTIFIER).ToStdString();
  }
  return std::nullopt;
}

void KeyStore::storeEmail(const std::string &email) {
  wxSystemOptions::SetOption(EMAIL_IDENTIFIER, email);
}