//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <wx/taskbar.h>

class TaskBarIcon : public wxTaskBarIcon {

 public:
  explicit TaskBarIcon();

 public:
  void showLoginMenu();
  void showStatusMenu();

 private:
  void loginClicked();
  void restartClicked();
  void openStageClicked();
  void openSettingsClicked();
  void addBoxClicked();
  void logoutClicked();

};

