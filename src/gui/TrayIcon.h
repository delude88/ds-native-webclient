//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

class TrayIcon : public QSystemTrayIcon {
 Q_OBJECT

 public:
  explicit TrayIcon(QObject *parent = nullptr);

 public slots:
  void showLoginMenu();
  void showStatusMenu();

 signals:
  void loginClicked();
  void openStageClicked();
  void logoutClicked();

 private:
  QMenu *login_menu_;
  QMenu *status_menu_;
};

