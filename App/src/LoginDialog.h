#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog {
 Q_OBJECT

 public:
  explicit LoginDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
  ~LoginDialog() override;

  QString getEmail();
  QString getPassword();

 public slots:
  void setLoading(bool loading);
  void setEmail(const QString &user);
  void setPassword(const QString &password);
  void setError(const QString &error);
  void resetError();

 protected slots:
  void on_buttonSignIn_clicked();

 signals:
  void login(const QString &email, const QString &password);

 private:
  Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H