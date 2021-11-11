//
// Created by Tobias Hegemann on 11.11.21.
//

#ifndef CLIENT_SRC_GUI_DUMMY_H_
#define CLIENT_SRC_GUI_DUMMY_H_

#include <QObject>
#include <QString>

class Dummy : QObject {
 Q_OBJECT

 public:
  Dummy();
  ~Dummy() override;

 private:
  QString bla_;
};

#endif //CLIENT_SRC_GUI_DUMMY_H_
