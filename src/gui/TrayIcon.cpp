//
// Created by Tobias Hegemann on 04.11.21.
//

#include "TrayIcon.h"

wxIMPLEMENT_APP(App);

bool App::OnInit() {
  if (!wxApp::OnInit())
    return false;

  if (!wxTaskBarIcon::IsAvailable()) {
    wxMessageBox
        (
            "There appears to be no system tray support in your current environment. This sample may not behave as expected.",
            "Warning",
            wxOK | wxICON_EXCLAMATION
        );
  }

  // Create the main window
  gs_dialog = new MyDialog("wxTaskBarIcon Test Dialog");

  gs_dialog->Show(true);

  return true;
}
