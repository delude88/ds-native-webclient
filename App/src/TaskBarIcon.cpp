#include <wx/stdpaths.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include "TaskBarIcon.h"

enum {
  kPuRestart = 10001,
  kPuOpenStage,
  kPuOpenSettings,
  kPuAddBox,
  kPuLogout,
  kPuClose,
  kPuLogin
};

wxBEGIN_EVENT_TABLE(TaskBarIcon, wxTaskBarIcon)
        EVT_MENU(kPuRestart, TaskBarIcon::onRestartClicked)
        EVT_MENU(kPuOpenStage, TaskBarIcon::onOpenStageClicked)
        EVT_MENU(kPuOpenSettings, TaskBarIcon::onOpenSettingsClicked)
        EVT_MENU(kPuAddBox, TaskBarIcon::onAddBoxClicked)
        EVT_MENU(kPuLogout, TaskBarIcon::onLogoutClicked)
        EVT_MENU(kPuClose, TaskBarIcon::onCloseClicked)
        EVT_MENU(kPuLogin, TaskBarIcon::onLoginClicked)
wxEND_EVENT_TABLE()

TaskBarIcon::~TaskBarIcon() {
  if (IsIconInstalled()) {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"
    RemoveIcon();
#pragma clang diagnostic pop
  }
}

wxMenu *TaskBarIcon::CreatePopupMenu() {
  auto *menu = new wxMenu;
  if (isLoggedIn_) {
    menu->Append(kPuRestart, "&Neustarten");
    menu->AppendSeparator();
    menu->Append(kPuOpenStage, "Öffne &Bühne");
    menu->AppendSeparator();
    menu->Append(kPuOpenSettings, "Öffne Ein&stellungen");
    menu->AppendSeparator();
    menu->Append(kPuAddBox, "&Neue Box hinzufügen");
    menu->AppendSeparator();
    menu->Append(kPuLogout, "&Abmelden");
    menu->AppendSeparator();
#ifdef __WXOSX__
    if (OSXIsStatusItem())
#endif
    {
      menu->AppendSeparator();
      menu->Append(kPuClose, "B&eenden");
    }
  } else {
    menu->Append(kPuLogin, "&Anmelden");
    menu->AppendSeparator();
#ifdef __WXOSX__
    if (OSXIsStatusItem())
#endif
    {
      menu->AppendSeparator();
      menu->Append(kPuClose, "B&eenden");
    }
  }
  return menu;
}

void TaskBarIcon::showLoginMenu() {
  isLoggedIn_ = false;
}
void TaskBarIcon::showStatusMenu() {
  isLoggedIn_ = true;
}
void TaskBarIcon::onLoginClicked(wxCommandEvent & WXUNUSED(event)) {
  loginClicked();
}
void TaskBarIcon::onRestartClicked(wxCommandEvent & WXUNUSED(event)) {
  restartClicked();
}
void TaskBarIcon::onOpenStageClicked(wxCommandEvent & WXUNUSED(event)) {
  openStageClicked();
}
void TaskBarIcon::onOpenSettingsClicked(wxCommandEvent & WXUNUSED(event)) {
  openSettingsClicked();
}
void TaskBarIcon::onAddBoxClicked(wxCommandEvent & WXUNUSED(event)) {
  addBoxClicked();
}
void TaskBarIcon::onLogoutClicked(wxCommandEvent & WXUNUSED(event)) {
  logoutClicked();
}
void TaskBarIcon::onCloseClicked(wxCommandEvent & WXUNUSED(event)) {
  closeClicked();
}
