#pragma once

#include "forms/UI.h"

class LoginDialog : public UILoginDialog {

 public:
  explicit LoginDialog(wxWindow* parent = nullptr);
  ~LoginDialog();

  std::string getEmail() const;
  std::string getPassword() const;

  void setLoading(bool
                  loading);
  void setEmail(const std::string &user);
  void setPassword(const std::string &password);
  void setError(const std::string &error);
  void resetError();
};