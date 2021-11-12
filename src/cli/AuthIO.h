//
// Created by Tobias Hegemann on 05.11.21.
//
#pragma once
#include <iostream>

class AuthIO {
 public:
  static inline std::string readToken() {
    std::ifstream file;
    file.open("token");
    if (file.is_open()) {
      std::string line;
      if (getline(file, line)) {
        return line;
      }
    }
    return "";
  }

  static inline void writeToken(const std::string &token) {
    std::ofstream file;
    file.open("token");
    file << token;
    file.close();
  }

  static inline void resetToken() {
    std::remove("token");
  }
};


