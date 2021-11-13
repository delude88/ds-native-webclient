//
// Created by Tobias Hegemann on 11.11.21.
//

#include "TrayIcon.h"
#include <QApplication>
#include <QAction>

TrayIcon::TrayIcon(QObject *parent) : QSystemTrayIcon(parent) {
  QIcon icon = QIcon(":/resources/icon.png");
  icon.setIsMask(true);
  this->setIcon(icon);
  this->setToolTip(tr("Digital Stage"));
  auto quitAction = new QAction(tr("Close"), this);
  connect(quitAction, &QAction::triggered, QCoreApplication::instance(), &QApplication::exit);

  // Login menu
  login_menu_ = new QMenu();
  auto viewLoginAction = new QAction(tr("Login"), this);
  login_menu_->addAction(viewLoginAction);
  connect(viewLoginAction, &QAction::triggered, [=]() {
    emit loginClicked();
  });
  login_menu_->addAction(quitAction);

  // Digital stage status menu
  status_menu_ = new QMenu();
  auto openStageAction = new QAction(tr("Open stage"), this);
  status_menu_->addAction(openStageAction);
  connect(openStageAction, &QAction::triggered, [=]() {
    emit openStageClicked();
  });
  auto openSettingsAction = new QAction(tr("Open settings"), this);
  status_menu_->addAction(openSettingsAction);
  connect(openSettingsAction, &QAction::triggered, [=]() {
    emit openSettingsClicked();
  });
  auto registerBoxAction = new QAction(tr("Add Box"), this);
  status_menu_->addAction(registerBoxAction);
  connect(registerBoxAction, &QAction::triggered, [=]() {
    emit addBoxClicked();
  });
  auto logoutAction = new QAction(tr("Logout"), this);
  status_menu_->addAction(logoutAction);
  connect(logoutAction, &QAction::triggered, [=]() {
    emit logoutClicked();
  });
  status_menu_->addSeparator();
  status_menu_->addAction(quitAction);
}

void TrayIcon::showStatusMenu() {
  this->setContextMenu(status_menu_);
}

void TrayIcon::showLoginMenu() {
  this->setContextMenu(login_menu_);
}
