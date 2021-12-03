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
#include <wx/imagpng.h>
#include <wx/file.h>
#include <wx/image.h>

wxIMPLEMENT_APP(App);

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

  //wxImage::AddHandler(new wxPNGHandler);
  wxInitAllImageHandlers();

  if (!wxTaskBarIcon::IsAvailable()) {
    wxMessageBox
        (
            "There appears to be no system tray support in your current environment.",
            "Error",
            wxOK | wxICON_EXCLAMATION
        );
    return false;
  }

  login_dialog_ = new LoginDialog();
  tray_icon_ = new TaskBarIcon();

  // Show splash screen
  /*assert(wxFile::Exists(wxStandardPaths::Get().GetResourcesDir() + "/splash.png"));
  auto *splash = new wxSplashScreen(wxBitmap(wxStandardPaths::Get().GetResourcesDir() + "/splash.png", wxBITMAP_TYPE_PNG_RESOURCE),
                                    wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT,
                                    2500,
                                    nullptr,
                                    wxID_ANY,
                                    wxDefaultPosition,
                                    wxDefaultSize,
                                    wxFRAME_NO_TASKBAR | wxSIMPLE_BORDER | wxSTAY_ON_TOP);
  wxYield();*/

  // Try to auto sign in
  auto email = KeyStore::restoreEmail();
  if (email) {
    login_dialog_->setEmail(*email);
    token_ = tryAutoLogin(*email);
  }
  if (!token_.has_value()) {
    // Show login menu and dialog
    tray_icon_->showLoginMenu();
    login_dialog_->Show();
  } else {
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->Hide();
    start();
  }
  return true;
}

std::optional<std::string> App::tryAutoLogin(const std::string &email) {
  // Try to log in using stored credentials
  auto credentials = KeyStore::restore(email);
  if (credentials) {
    // Try to get token
    auto token = auth_service_->signInSync(credentials->email, credentials->password);
    if (token.has_value()) {
      return *token;
    }
    if (!KeyStore::remove(email)) {
      PLOGE << "Could not reset credentials for " << email;
    }
  }
  return std::nullopt;
}

void App::logIn(const std::string &email, const std::string &password) {
  login_dialog_->setLoading(true);
  login_dialog_->resetError();
  try {
    token_ = auth_service_->signInSync(email, password);
    KeyStore::storeEmail(email);
    KeyStore::store({email, password});
    // Show status menu, hide login and start
    tray_icon_->showStatusMenu();
    login_dialog_->setLoading(false);
    login_dialog_->Hide();
    start();
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