#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include <memory>

#ifdef __APPLE__
#include "gui/utils/macos.h"
#endif

//#include "gui/KeyStore.h"
#include "gui/LoginDialog.h"
#include "gui/Dummy.h"

#include <DigitalStage/Auth/AuthService.h>

int main(int argc, char *argv[]) {
#ifdef __APPLE__
  // Special macOS routine (get microphone access rights)
  if (!check_access()) {
    return -1;
  }
#endif

  QApplication qApplication(argc, argv);

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    QMessageBox::critical(nullptr, QObject::tr("Systray"),
                          QObject::tr("I couldn't detect any system tray "
                                      "on this system."));
    return 1;
  }

  auto dummy = std::make_unique<Dummy>();

  /*
  std::optional<std::string> token;
  auto auth_service = std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL);
  auto key_store = std::make_unique<KeyStore>();
  auto email = key_store->restoreEmail();
  if (email) {
    // Try to login using stored credentials
    auto credentials = key_store->restore(*email);
    if (credentials) {
      // Try to get token
      try {
        token = auth_service->signInSync(credentials->email, credentials->password);
      } catch (...) {
      }
    }
  }
  if (!token) {
    // Show login panel
    auto login_pane = std::make_unique<LoginDialog>();
    login_pane->show();
  }*/

  std::cout << "OK, let's go" << std::endl;

  return 0;
}
