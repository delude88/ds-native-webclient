//
// Created by Tobias Hegemann on 08.11.21.
//
#pragma once
#include <cmrc/cmrc.hpp>

class CMRCFileBuffer : public std::streambuf {
  const char *current_;
  cmrc::file file_;
 public:
  explicit CMRCFileBuffer(cmrc::file file) : file_(file), current_(file.begin()) {
  }
  inline int_type underflow() override {
    if (current_ == file_.end()) {
      return traits_type::eof();
    }
    return traits_type::to_int_type(*current_);
  }
  inline int_type uflow() override {
    if (current_ == file_.end()) {
      return traits_type::eof();
    }
    return traits_type::to_int_type(*current_++);
  }

  inline std::streamsize showmanyc() override {
    return file_.end() - file_.begin();
  }
};