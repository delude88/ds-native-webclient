#include <wx/stdpaths.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include "TaskBarIcon.h"

enum {
  PU_RESTART = 10001,
  PU_OPEN_STAGE,
  PU_OPEN_SETTINGS,
  PU_ADD_BOX,
  PU_LOGOUT,
  PU_CLOSE,
  PU_LOGIN
};

wxBEGIN_EVENT_TABLE(TaskBarIcon, wxTaskBarIcon)
        EVT_MENU(PU_RESTART, TaskBarIcon::onRestartClicked)
        EVT_MENU(PU_OPEN_STAGE, TaskBarIcon::onOpenStageClicked)
        EVT_MENU(PU_OPEN_SETTINGS, TaskBarIcon::onOpenSettingsClicked)
        EVT_MENU(PU_ADD_BOX, TaskBarIcon::onAddBoxClicked)
        EVT_MENU(PU_LOGOUT, TaskBarIcon::onLogoutClicked)
        EVT_MENU(PU_CLOSE, TaskBarIcon::onCloseClicked)
        EVT_MENU(PU_LOGIN, TaskBarIcon::onLoginClicked)
wxEND_EVENT_TABLE()

TaskBarIcon::~TaskBarIcon() {
  if (IsIconInstalled()) {
    RemoveIcon();
  }
}

wxMenu *TaskBarIcon::CreatePopupMenu() {
  auto menu = new wxMenu;
  if (isLoggedIn_) {
    menu->Append(PU_RESTART, "&Neustarten");
    menu->AppendSeparator();
    menu->Append(PU_OPEN_STAGE, "Öffne &Bühne");
    menu->AppendSeparator();
    menu->Append(PU_OPEN_SETTINGS, "Öffne Ein&stellungen");
    menu->AppendSeparator();
    menu->Append(PU_ADD_BOX, "&Neue Box hinzufügen");
    menu->AppendSeparator();
    menu->Append(PU_LOGOUT, "&Abmelden");
    menu->AppendSeparator();
#ifdef __WXOSX__
    if (OSXIsStatusItem())
#endif
    {
      menu->AppendSeparator();
      menu->Append(PU_CLOSE, "B&eenden");
    }
  } else {
    menu->Append(PU_LOGIN, "&Anmelden");
    menu->AppendSeparator();
#ifdef __WXOSX__
    if (OSXIsStatusItem())
#endif
    {
      menu->AppendSeparator();
      menu->Append(PU_CLOSE, "B&eenden");
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
