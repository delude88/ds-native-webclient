#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

#include <memory>

#ifdef __APPLE__
#include "gui/utils/macos.h"
#endif

#include "gui/KeyStore.h"
#include "gui/App.h"

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

  App app;
  app.show();

  return QApplication::exec();
}
