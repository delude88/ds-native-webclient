#ifdef __APPLE__
#include "utils/macos.h"
#endif

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/taskbar.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>




// Logger
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

int main(int argc, char *argv[]) {
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

#ifdef __APPLE__
  // Special macOS routine (get microphone access rights)
  if (!check_access()) {
    return -1;
  }
#endif

  if (!wxTaskBarIcon::IsAvailable()) {
    wxMessageBox("Error", "I couldn't detect any system tray on this system.", wxCANCEL);
    return 1;
  }

  wxEntry(argc, argv);

  wxImage::AddHandler(new wxXPMHandler);
  wxBitmap bitmap(wxStandardPaths::Get().GetResourcesDir() + "/splash.png");
  auto *splash = new wxSplashScreen(bitmap,
                                    wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                                    2500,
                                    nullptr,
                                    wxID_ANY,
                                    wxDefaultPosition,
                                    wxDefaultSize,
                                    wxFRAME_NO_TASKBAR | wxSIMPLE_BORDER | wxSTAY_ON_TOP);
  wxYield();
}
