#include <wx/stdpaths.h>
#include "LoginDialog.h"

LoginDialog::LoginDialog(wxWindow *parent) : UILoginFrame(parent, wxID_ANY) {
  error_message_->Hide();
  auto logo = wxBitmap(wxStandardPaths::Get().GetResourcesDir() + "/logo-full.png", wxBITMAP_TYPE_PNG);
  logo_->SetBitmap(logo);
}

LoginDialog::~LoginDialog() = default;

void LoginDialog::setEmail(const std::string &email) {
  email_entry_->SetValue(email);
}

void LoginDialog::setPassword(const std::string &password) {
  password_entry_->SetValue(password);
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
