// AudioIO engine
#ifdef USE_RT_AUDIO
#include "audio/RtAudioIO.h"
#else
// Miniaudio
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio/MiniAudioIO.h"
#endif

#include <cmrc/cmrc.hpp>

#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/taskbar.h"

class App : public wxApp {
 public:
  virtual bool OnInit()
  wxOVERRIDE;
};

wxIMPLEMENT_APP(App);

bool App::OnInit() {
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

#ifdef __APPLE__
  if (!check_access()) {
    std::cerr << "No access" << std::endl;
    return false;
  }
#endif

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

  return true;
}