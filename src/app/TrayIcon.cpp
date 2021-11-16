//
// Created by Tobias Hegemann on 11.11.21.
//

#include "TrayIcon.h"
#include <QCoreApplication>
#include <QAction>

TrayIcon::TrayIcon(QObject *parent) : QSystemTrayIcon(parent) {
  QIcon icon = QIcon(":/resources/icon.png");
  icon.setIsMask(true);
  this->setIcon(icon);
  this->setToolTip(tr("Digital Stage"));
  this->showLoginMenu();
}

void TrayIcon::showStatusMenu() {
  auto menu = new QMenu();
  auto restartAction = new QAction(tr("Restart"), this);
  menu->addAction(restartAction);
  connect(restartAction, &QAction::triggered, [=]() {
    emit restartClicked();
  });
  auto openStageAction = new QAction(tr("Open stage"), this);
  menu->addAction(openStageAction);
  connect(openStageAction, &QAction::triggered, [=]() {
    emit openStageClicked();
  });
  auto openSettingsAction = new QAction(tr("Open settings"), this);
  menu->addAction(openSettingsAction);
  connect(openSettingsAction, &QAction::triggered, [=]() {
    emit openSettingsClicked();
  });
  auto registerBoxAction = new QAction(tr("Add Box"), this);
  menu->addAction(registerBoxAction);
  connect(registerBoxAction, &QAction::triggered, [=]() {
    emit addBoxClicked();
  });
  auto logoutAction = new QAction(tr("Logout"), this);
  menu->addAction(logoutAction);
  connect(logoutAction, &QAction::triggered, [=]() {
    emit logoutClicked();
  });
  menu->addSeparator();
  auto quitAction = new QAction(tr("Close"), this);
  connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
  menu->addAction(quitAction);
  this->setContextMenu(menu);
}

void TrayIcon::showLoginMenu() {
  auto menu = new QMenu();
  auto viewLoginAction = new QAction(tr("Login"), this);
  menu->addAction(viewLoginAction);
  connect(viewLoginAction, &QAction::triggered, [=]() {
    emit loginClicked();
  });
  auto quitAction = new QAction(tr("Close"), this);
  connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
  menu->addAction(quitAction);
  this->setContextMenu(menu);
}
