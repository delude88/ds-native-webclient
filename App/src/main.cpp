#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>
#include <QSplashScreen>

#ifdef __APPLE__
#include "utils/macos.h"
#endif

#include "App.h"

// Logger
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

int main(int argc, char *argv[]) {
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::info, &consoleAppender);

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

  QTranslator translator;
  if (translator.load(QLocale(), QLatin1String("DigitalStage"),
                      QLatin1String("_"), QLatin1String(":/i18n"))) {
    QCoreApplication::installTranslator(&translator);
  }

  QPixmap pixmap(":/assets/splash.png");
  QSplashScreen splash(pixmap);
  splash.show();
  QTimer::singleShot(1000, &splash,
                     &QWidget::close); // keep displayed for 5 seconds

  App app;
  app.show();

  return QApplication::exec();
}
