//
// Created by Tobias Hegemann on 02.11.21.
//

#ifndef CLIENT_SRC_NEW_IAUDIOIO_H_
#define CLIENT_SRC_NEW_IAUDIOIO_H_

#include <sigslot/signal.hpp>

typedef std::array<std::optional<std::string>, 32> ChannelMap;

class IAudioIO {
 public:
  virtual const ChannelMap &getChannelMapping() const = 0;
  virtual void read(std::size_t channel, float *buff, size_t size) = 0;
  virtual void writeLeft(const float *buf, size_t size) = 0;
  virtual void writeRight(const float *buf, size_t size) = 0;

};

#endif //CLIENT_SRC_NEW_IAUDIOIO_H_
