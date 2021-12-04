//
// Created by Tobias Hegemann on 11.11.21.
//
#ifdef __APPLE__
#include "utils/macos.h"
#endif

#include <deviceid/id.h>
#include "App.h"
#include "KeyStore.h"
#include <DigitalStage/Api/Client.h>
#include <plog/Log.h>
#include <wx/taskbar.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <wx/file.h>
#include <wx/image.h>

wxIMPLEMENT_APP(App);

Frame::Frame() : wxFrame(nullptr, wxID_ANY, "You should never see me") {
  tray_icon_ = new TaskBarIcon();
}

bool App::OnInit() {
  static plog::ConsoleAppender<plog::TxtFormatter> console_appender;
  plog::init(plog::debug, &console_appender);

#ifdef __APPLE__
  // Special macOS routine (get microphone access rights)
  if (!check_access()) {
    return false;
  }
#endif

  // Core
  device_id_ = std::to_string(deviceid::get());
  auth_service_ = std::make_unique<DigitalStage::Auth::AuthService>(AUTH_URL);

  // wxWidgets widgets
  if (!wxApp::OnInit())
    return false;

  wxInitAllImageHandlers();

  wxApp::SetExitOnFrameDelete(true);
  auto *menubar = new wxMenuBar;
  wxMenuBar::MacSetCommonMenuBar(menubar);

  // Login Dialog (also our main frame)
  login_dialog_ = new LoginDialog(nullptr);
  login_dialog_->onLoginClicked.connect([this](const std::string &email, const std::string &password) {
    logIn(email, password);
  });
  SetTopWindow(login_dialog_);

  if (!wxTaskBarIcon::IsAvailable()) {
    wxMessageBox
        (
            "There appears to be no system tray support in your current environment.",
            "Error",
            wxOK | wxICON_EXCLAMATION
        );
    return false;
  }

  // Show splash screen
  wxBitmap bitmap;
  if (bitmap.LoadFile(wxStandardPaths::Get().GetResourcesDir() + "/splash.png", wxBITMAP_TYPE_PNG)) {
    PLOGI << "splash";
    new wxSplashScreen(bitmap,
                       wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                       2500, nullptr, wxID_ANY,
                       wxDefaultPosition, wxDefaultSize,
                       wxSIMPLE_BORDER | wxSTAY_ON_TOP);
#if !defined(__WXGTK20__)
    wxYield();
#endif
  }

  tray_icon_ = new TaskBarIcon();
  tray_icon_->loginClicked.connect([this]() { login_dialog_->SetFocus(); });
  tray_icon_->restartClicked.connect([this]() { restart(); });
  tray_icon_->openStageClicked.connect([]() { openStage(); });
  tray_icon_->openSettingsClicked.connect([this]() { openSettings(); });
  //tray_icon_->addBoxClicked.connect([this](){ });
  tray_icon_->logoutClicked.connect([this]() { logOut(); });
  tray_icon_->closeClicked.connect([this]() { login_dialog_->Close(true); });

  // Try to auto sign in
  auto email = KeyStore::restoreEmail();
  if (email) {
    login_dialog_->setEmail(*email);
    token_ = tryAutoLogin(*email);
  }
  if (!token_.has_value()) {
    // Show login menu and dialog
    tray_icon_->showLoginMenu();
    login_dialog_->Show(true);
  } else {
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->Show(false);
    //login_dialog_->Hide();
    start();
  }
  return true;
}

std::optional<std::string> App::tryAutoLogin(const std::string &email) {
  // Try to log in using stored credentials
  auto credentials = KeyStore::restore(email);
  if (credentials) {
    // Try to get token
    try {
      token_ = auth_service_->signInSync(credentials->email, credentials->password);
      return token_;
    } catch (DigitalStage::Auth::AuthError &error) {
      login_dialog_->setError(std::to_string(error.getCode()) + ": " + error.what());
      if (!KeyStore::remove(email)) {
        PLOGE << "Could not reset credentials for " << email;
      }
    }
  }
  return std::nullopt;
}

void App::logIn(const std::string &email, const std::string &password) {
  login_dialog_->setLoading(true);
  login_dialog_->resetError();
  try {
    PLOGI << "Login";
    token_ = auth_service_->signInSync(email, password);
    KeyStore::storeEmail(email);
    KeyStore::store({email, password});
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->setLoading(false);
    login_dialog_->Hide();
    start();
  } catch (DigitalStage::Auth::AuthError &ex) {
    login_dialog_->setLoading(false);
    login_dialog_->setError(std::to_string(ex.getCode()) + ": " + ex.what());
  } catch (std::exception &ex) {
    login_dialog_->setLoading(false);
    login_dialog_->setError(ex.what());
  }
}

void App::logOut() {
  stop();
  tray_icon_->showLoginMenu();
  login_dialog_->setPassword("");
  if (token_.has_value()) {
    auth_service_->signOutSync(*token_);
    token_ = std::nullopt;
  }
  login_dialog_->Show();
}

void App::start() {
  PLOGI << "Starting";
  if (!token_.has_value()) {
    throw std::logic_error("No token available");
  }

  api_client_ = std::make_shared<DigitalStage::Api::Client>(API_URL);
  client_ = std::make_unique<Client>(api_client_);// Describe this device
  nlohmann::json initial_device_information;
  // - always use a UUID when you want Digital Stage to remember this device and its settings
  initial_device_information["uuid"] = device_id_;
  initial_device_information["type"] = "native";
  initial_device_information["canAudio"] = true;
  initial_device_information["sendAudio"] = false;
  initial_device_information["receiveAudio"] = true;
  initial_device_information["canVideo"] = false;
#ifdef USE_RT_AUDIO
  initial_device_information["audioEngine"] = "rtaudio";
#else
  initial_device_information["audioEngine"] = "miniaudio";
#endif

  // And finally connect with the token and device description
  api_client_->disconnected.connect([=](bool expected) {
    if (!expected) {
      PLOGE << "Disconnected unexpected - calling stop()";
      stop();
    }
  });
  PLOGE << "Connecting ...";
  api_client_->connect(*token_, initial_device_information);
}
void App::stop() {
  PLOGE << "Stopping";
  client_ = nullptr;
  api_client_ = nullptr;
}
void App::restart() {
  PLOGE << "Restarting";
  stop();
  start();
}

void App::openStage() {
  wxLaunchDefaultBrowser(STAGE_URL);
}

void App::openSettings() {
  auto uri = std::string(SETTINGS_URL);
  uri += "/" + device_id_;
  wxLaunchDefaultBrowser(uri);
}