#include "LoginDialog.h"
#include "ui_LoginDialog.h"

LoginDialog::LoginDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f), ui(new Ui::LoginDialog) {
  ui->setupUi(this);
}

LoginDialog::~LoginDialog() {
  delete ui;
}

void LoginDialog::setEmail(const QString &email) {
  ui->emailEdit->setText(email);
}

void LoginDialog::setPassword(const QString &password) {
  ui->passwordEdit->setText(password);
}

QString LoginDialog::getEmail() {
  return ui->emailEdit->text();
}

QString LoginDialog::getPassword() {
  return ui->passwordEdit->text();
}

void LoginDialog::setError(const QString &error) {
  ui->labelError->setText(error);
}

void LoginDialog::resetError() {
  ui->labelError->setText("");
}

void LoginDialog::on_buttonSignIn_clicked() {
  login(getEmail(), getPassword());
}
