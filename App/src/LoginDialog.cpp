#include "LoginDialog.h"

LoginDialog::LoginDialog(wxWindow *parent) : UILoginDialog(parent) {
  error_message_->Hide();
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
  login_button_->Enable(!loading);
}
