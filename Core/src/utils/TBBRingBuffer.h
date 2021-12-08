#pragma once

#include <cstdio>
#include <memory>
#include <tbb/cache_aligned_allocator.h>

template<class T>
class RingBuffer {
 public:
  explicit RingBuffer(std::size_t size) : buf_(size), max_size_(size) {
  }

  inline void put(T item) {
    buf_[head_] = item;

    if (full_) {
      tail_ = (tail_ + 1) % max_size_;
    }

    head_ = (head_ + 1) % max_size_;

    full_ = head_ == tail_;
  }

  inline T get() {
    if (empty()) {
      return T();
    }

    // Read data and advance the tail (we now have a free space)
    auto val = buf_[tail_];
    full_ = false;
    tail_ = (tail_ + 1) % max_size_;

    return val;
  }

  inline void reset() {
    head_ = tail_;
    full_ = false;
  }

  [[nodiscard]] inline bool empty() const {
    // if head and tail are equal, we are empty
    return (!full_ && (head_ == tail_));
  }

  [[maybe_unused]] [[maybe_unused]] [[nodiscard]] inline bool full() const {
    // If tail is ahead the head by 1, we are full
    return full_;
  }

  [[nodiscard]] inline size_t capacity() const {
    return max_size_;
  }

  [[nodiscard]] inline size_t size() const {
    size_t size = max_size_;

    if (!full_) {
      if (head_ >= tail_) {
        size = head_ - tail_;
      } else {
        size = max_size_ + head_ - tail_;
      }
    }

    return size;
  }

 private:
  std::vector<T, tbb::cache_aligned_allocator<T>> buf_;
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  const std::size_t max_size_;
  bool full_ = false;
};