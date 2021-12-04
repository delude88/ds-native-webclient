#pragma once

#include "forms/UI.h"
#include <sigslot/signal.hpp>

class LoginDialog : public UILoginFrame {
 public:
  explicit LoginDialog(wxWindow *parent = nullptr);
  ~LoginDialog() override;

  std::string getEmail() const;
  std::string getPassword() const;

  void setLoading(bool
                  loading);
  void setEmail(const std::string &user);
  void setPassword(const std::string &password);
  void setError(const std::string &error);
  void resetError();

  sigslot::signal<std::string, std::string> onLoginClicked;
 protected:
  void onLogin(wxCommandEvent &event) override;
};