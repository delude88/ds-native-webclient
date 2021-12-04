#include <wx/stdpaths.h>
#include "LoginDialog.h"

LoginDialog::LoginDialog(wxWindow *parent) : UILoginFrame(parent, wxID_ANY) {
  error_message_->Hide();
#if defined(__WXOSX__) && wxOSX_USE_COCOA
  auto logo = wxBitmap(wxStandardPaths::Get().GetResourcesDir() + "/logo-full@2x.png", wxBITMAP_TYPE_PNG);
#else
  auto logo = wxBitmap(wxStandardPaths::Get().GetResourcesDir() + "/logo-full.png", wxBITMAP_TYPE_PNG);
#endif
  logo_->SetBitmap(logo);
}

LoginDialog::~LoginDialog() = default;

void LoginDialog::setEmail(const std::string &email) {
  email_entry_->SetValue(email);
}

void LoginDialog::setPassword(const std::string &password) {
  password_entry_->SetValue(password);
}

std::string LoginDialog::getEmail() const {
  return email_entry_->GetValue().ToStdString();
}

std::string LoginDialog::getPassword() const {
  return password_entry_->GetValue().ToStdString();
}

void LoginDialog::setError(const std::string &error) {
  error_message_->SetLabel(error);
  error_message_->Show();
}

void LoginDialog::resetError() {
  error_message_->Hide();
  error_message_->SetLabel("");
}

void LoginDialog::setLoading(bool loading) {
  if (loading) {
    login_button_->Disable();
    login_button_->SetLabel("Verbinde...");
  } else {
    login_button_->Enable();
    login_button_->SetLabel("Anmelden");
  }
}

void LoginDialog::onLogin(wxCommandEvent & /*event*/) {
  onLoginClicked(
      email_entry_->GetValue().ToStdString(),
      password_entry_->GetValue().ToStdString()
  );
}
