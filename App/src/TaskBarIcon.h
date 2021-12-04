//
// Created by Tobias Hegemann on 11.11.21.
//
#pragma once

#include <wx/taskbar.h>
#include <wx/stdpaths.h>
#include <sigslot/signal.hpp>

class TaskBarIcon : public wxTaskBarIcon {

 public:
#if defined(__WXOSX__) && wxOSX_USE_COCOA
  explicit TaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE)
      : wxTaskBarIcon(iconType), isLoggedIn_(false)
#else
  explicit TaskBarIcon() : isLoggedIn_(false)
#endif
  {
    auto icon = wxIcon(wxStandardPaths::Get().GetResourcesDir() + "/icon@2x.png", wxBITMAP_TYPE_PNG);
    this->SetIcon(icon);
  }
  virtual ~TaskBarIcon();

  void showLoginMenu();
  void showStatusMenu();

  sigslot::signal<> loginClicked;
  sigslot::signal<> restartClicked;
  sigslot::signal<> openStageClicked;
  sigslot::signal<> openSettingsClicked;
  sigslot::signal<> addBoxClicked;
  sigslot::signal<> logoutClicked;
  sigslot::signal<> closeClicked;

 protected:
  wxMenu *CreatePopupMenu() wxOVERRIDE;

 private:
  void onLoginClicked(wxCommandEvent &);
  void onRestartClicked(wxCommandEvent &);
  void onOpenStageClicked(wxCommandEvent &);
  void onOpenSettingsClicked(wxCommandEvent &);
  void onAddBoxClicked(wxCommandEvent &);
  void onLogoutClicked(wxCommandEvent &);
  void onCloseClicked(wxCommandEvent &);

  bool isLoggedIn_;

 wxDECLARE_EVENT_TABLE();
};
