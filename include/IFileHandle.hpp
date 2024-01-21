#pragma once

#include "common.hpp"

/* Customised FileHandle to accelerate file I/O */

class IFileHandle {
private:
  char __read_buffer[16];

public:
  virtual ~IFileHandle() {}

  virtual void seek(isize offset, SeekOrigin origin) {}
  virtual usize tell() { return 0; }
  virtual usize read(void *dest, usize size, usize count) { return 0; }
  virtual void write(void *buffer, usize size, usize count) {}
  virtual bool eof() { return true; }
  virtual int getc() { return 0; }
  void flush() {}

  virtual usize get_size() {
    usize current = this->tell();
    this->seek(0, SeekOrigin::end);
    usize size = this->tell();
    this->seek(current, SeekOrigin::begin);
    return size;
  }

  virtual char get_char() { return this->getc(); }

  virtual int32_t get_int32() {
    this->read(this->__read_buffer, sizeof(int32_t), 1);
    return *(int32_t *)this->__read_buffer;
  }

  virtual uint32_t get_uint32() {
    this->read(this->__read_buffer, sizeof(uint32_t), 1);
    return *(uint32_t *)this->__read_buffer;
  }

  virtual int64_t get_int64() {
    this->read(this->__read_buffer, sizeof(int64_t), 1);
    return *(int64_t *)this->__read_buffer;
  }

  virtual uint64_t get_uint64() {
    this->read(this->__read_buffer, sizeof(uint64_t), 1);
    return *(uint64_t *)this->__read_buffer;
  }
};