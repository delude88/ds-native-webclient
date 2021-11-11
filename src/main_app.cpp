#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

#ifdef __APPLE__
#include "gui/utils/macos.h"
#endif

int main(int argc, char *argv[]) {
#ifdef __APPLE__
  // Special macOS routine (get microphone access rights)
  if (!check_access()) {
    return -1;
  }
#endif

  QApplication qApplication(argc, argv);

  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    QMessageBox::critical(nullptr, QObject::tr("Systray"),
                          QObject::tr("I couldn't detect any system tray "
                                      "on this system."));
    return 1;
  }

  QMessageBox::warning(nullptr, QObject::tr("Hello"), QObject::tr("World!"));

  return 0;
}
