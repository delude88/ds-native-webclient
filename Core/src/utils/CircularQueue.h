#pragma once

#include <tbb/cache_aligned_allocator.h>
#include <cstring>

template<typename T>
struct alignas(64) CircularQueue {
  int m_front;
  int m_rear;
  std::size_t m_size;
  std::size_t m_counter;
  std::vector<T, tbb::cache_aligned_allocator < T>> m_arr;

  explicit CircularQueue(std::size_t size)
      : m_front(-1), m_rear(-1), m_size(size), m_counter(0), m_arr(size) {};

  [[maybe_unused]] inline void enqueue(T val) {
    if ((m_front == 0 && m_rear == m_size - 1)) {
      printf("Queue is Full \n");
      //throw std::runtime_error("Queue is Full \n");
      return;
    }
    if (m_front == -1) { /* Insert First Element */
      m_front = 0;
    }
    m_rear++;
    if (m_rear == m_size) m_rear = 0;
    m_arr[m_rear] = val;

    m_counter++;
  }

  inline void enqueue_many(const T *vals, int start, int end) {
    if (m_counter + (end - start) > m_size) {
      printf("Queue is Full \n");
      //throw std::runtime_error("Queue is Full \n");
      return;
    }

    if (m_front == -1) { /* Insert First Element */
      m_front = 0;
    }

    m_rear++;
    int diff = (((end - start) + m_rear) < m_size) ? (end - start) : m_size - m_rear;
    std::memcpy(&m_arr[m_rear], &vals[start], sizeof(T) * diff);
    if (diff != (end - start)) {
      int diff1 = (end - start) - diff;
      std::memcpy(&m_arr[0], &vals[start + diff], sizeof(int) * diff1);
    }

    m_rear = (m_rear + (end - start) - 1);
    if (m_rear >= m_size) m_rear -= m_size;
    m_counter += (end - start);
  }

  inline T dequeue() {
    if (m_front == -1 || m_counter == 0) {
      printf("Queue is Empty \n");
      return 0;
    }
    auto data = m_arr[m_front];
    m_arr[m_front] = -1;
    if (m_front == m_rear) {
      m_front = -1;
      m_rear = -1;
    } else {
      m_front = (m_front + 1) % m_size;
    }
    m_counter--;
    return data;
  }

  [[maybe_unused]] inline void dequeue_many(int numOfItems) {
    m_front += numOfItems;
    if (m_front > m_size) m_front -= m_size;
    m_counter -= numOfItems;
  }

  [[maybe_unused]] inline void reset() {
    m_front = -1;
    m_rear = -1;
    m_counter = 0;
  }
};