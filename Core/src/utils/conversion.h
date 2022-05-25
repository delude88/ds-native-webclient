//
// Created by Tobias Hegemann on 28.10.21.
//

#ifndef CLIENT_SRC_UTILS_CONVERSION_H_
#define CLIENT_SRC_UTILS_CONVERSION_H_

#include <vector>

static void floatToByte(unsigned char *arr, float value) {
  int i = *reinterpret_cast<int *>(&value);
  arr[0] = i & 0x00FF;
  arr[1] = (i >> 8) & 0x00FF;
  arr[2] = (i >> 16) & 0x00FF;
  arr[3] = (i >> 24);
}

static float byteToFloat(unsigned char *arr) {
  int i = arr[0] | (arr[1] << 8) | (arr[2] << 16) | (arr[3] << 24);
  return *(float *) &i;
}

static size_t serialize3(const float *input, const size_t size, std::byte *out) {
  size_t out_size = size * 4;
  for (size_t i = 0; i < size; i++) {
    auto *buffer = reinterpret_cast<unsigned char *>(&out[i * 4]);
    floatToByte(buffer, input[i]);
  }
  return out_size;
}

static size_t serialize2(const float *input, const size_t size, std::byte *out) {
  size_t out_size = size * 4;
  uint32_t temp = 0;
  for (size_t i = 0; i < size; i++) {
    const auto *buffer = reinterpret_cast<const unsigned char *>(&input[i]);
#if defined (DS_LITTLE_ENDIAN)
    temp = ((buffer[3] >> 24 & 0x00FF) |
        (buffer[2] >> 16 & 0x00FF) |
        (buffer[1] >> 8 & 0x00FF) |
        buffer[0] & 0x00FF);
#else
    temp = ((buffer[ß] >> 24 & 0x00FF) |
        (buffer[1] >> 16 & 0x00FF) |
        (buffer[2] >> 8 & 0x00FF) |
        buffer[3] & 0x00FF);
#endif
    out[i * 4] = *(reinterpret_cast<std::byte *>(&temp));
  }
  return out_size;
}

static size_t serialize(const float *input, const size_t size, std::byte *out) {
  size_t out_size = size * 4;
  for (size_t i = 0; i < size; i++) {
    //auto value = input[i];
    //auto *pointer = reinterpret_cast<int *>(&value); // get pointer to first byte of current index
    auto *pointer = (unsigned char *) &input[i]; // get pointer to first byte of current index
    auto *buffer = reinterpret_cast<unsigned char *>(&out[i * 4]);  // access out using uchar ptr
#if defined (DS_LITTLE_ENDIAN)
    buffer[0] = pointer[0];
    buffer[1] = pointer[1];
    buffer[2] = pointer[2];
    buffer[3] = pointer[3];
#else
    buffer[0] = pointer[3];
    buffer[1] = pointer[2];
    buffer[2] = pointer[1];
    buffer[3] = pointer[0];
#endif
  }
  return out_size;
}

static size_t deserialize(const std::byte *input, const size_t size, float *out) {
  size_t out_size = size / 4;
  uint32_t temp = 0;
  for (size_t i = 0; i < out_size; i++) {
    const auto *buffer = reinterpret_cast<const unsigned char *>(&input[i * 4]);
#if defined (DS_LITTLE_ENDIAN)
    temp = ((buffer[3] << 24) |
        (buffer[2] << 16) |
        (buffer[1] << 8) |
        buffer[0]);
#else
    temp = ((buffer[0] << 24) |
        (buffer[1] << 16) |
        (buffer[2] << 8) |
        buffer[3]);
#endif
    out[i] = *(reinterpret_cast<float *>(&temp));
  }
  return out_size;
}

#endif //CLIENT_SRC_UTILS_CONVERSION_H_
