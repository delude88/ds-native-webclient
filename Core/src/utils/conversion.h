//
// Created by Tobias Hegemann on 28.10.21.
//

#ifndef CLIENT_SRC_UTILS_CONVERSION_H_
#define CLIENT_SRC_UTILS_CONVERSION_H_

#include <vector>

// unpack method for retrieving data in network byte,
//   big endian, order (MSB first)
// increments index i by the number of bytes unpacked
// usage:
//   int i = 0;
//   float x = unpackFloat(&buffer[i], &i);
//   float y = unpackFloat(&buffer[i], &i);
//   float z = unpackFloat(&buffer[i], &i);
static float unpackFloat(const void *buf, int *i) {
  const auto *b = (const unsigned char *) buf;
  uint32_t temp = 0;
  *i += 4;
  temp = ((b[0] << 24) |
      (b[1] << 16) |
      (b[2] << 8) |
      b[3]);
  return *((float *) &temp);
}

static int unpackFloatArray(void **buf, float *arr) {
  int arr_size = sizeof(buf);
  int buf_index = 0;
  for (auto arr_index = 0; arr_index < arr_size; arr_index++) {
    float f = unpackFloat(&buf[buf_index], &buf_index);
    arr[arr_index] = f;
  }
  return arr_size;
}

// pack method for storing data in network,
//   big endian, byte order (MSB first)
// returns number of bytes packed
// usage:
//   float x, y, z;
//   int i = 0;
//   i += packFloat(&buffer[i], x);
//   i += packFloat(&buffer[i], y);
//   i += packFloat(&buffer[i], z);
static int packFloat(void *buf, float x) {
  auto *b = (unsigned char *) buf;
  auto *p = (unsigned char *) &x;
#if defined (DS_LITTLE_ENDIAN)
  b[0] = p[3];
  b[1] = p[2];
  b[2] = p[1];
  b[3] = p[0];
#else
  b[0] = p[0];
  b[1] = p[1];
  b[2] = p[2];
  b[3] = p[3];
#endif
  return 4;
}

static int packFloatArray(void **buffer, const float *arr, const size_t arr_size) {
  int buffer_size = 0;
  for (auto arr_index = 0; arr_index < arr_size; arr_index++) {
    buffer_size += packFloat(&buffer[buffer_size], arr[arr_index]);
  }
  return buffer_size;
}

static size_t serializeFloat(const float f, std::byte out[4]) {
  auto *p = (unsigned char *) &f;
  auto *b = (unsigned char *) &out[0];
#if defined (DS_LITTLE_ENDIAN)
  b[0] = p[3];
  b[1] = p[2];
  b[2] = p[1];
  b[3] = p[0];
#else
  b[0] = p[0];
    b[1] = p[1];
    b[2] = p[2];
    b[3] = p[3];
#endif
  return 4;
}

static size_t serialize(const float *in, const size_t size, std::byte *out) {
  size_t out_size = size * 4;
  for (size_t i = 0; i < size; i++) {
    auto *p = (unsigned char *) &in[i]; // get pointer to first byte of current index
    auto *b = (unsigned char *) &out[i * 4];  // access out using uchar ptr
#if defined (DS_LITTLE_ENDIAN)
    b[0] = p[3];
    b[1] = p[2];
    b[2] = p[1];
    b[3] = p[0];
#else
    b[0] = p[0];
    b[1] = p[1];
    b[2] = p[2];
    b[3] = p[3];
#endif
  }
  return out_size;
}

static size_t deserialize(const std::byte *in, const size_t size, float *out) {
  size_t out_size = size / 4;
  uint32_t temp = 0;
  for (size_t i = 0; i < out_size; i++) {
    const auto *b = (const unsigned char *) &in[i * 4];
    temp = ((b[0] << 24) |
        (b[1] << 16) |
        (b[2] << 8) |
        b[3]);
    out[i] = *((float *) &temp);
  }
  return out_size;
}

#endif //CLIENT_SRC_UTILS_CONVERSION_H_
