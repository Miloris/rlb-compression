#pragma once

#include "common.hpp"

class HybridBlockCache {
  friend class HybridFileHandle;

private:
  // cache definition
  usize _block_size;  // cache block size, in bytes
  usize _block_count; // how many cache blocks allocated
  uint8_t _flag_using;
  uint8_t _flag_dirty;
  // cache impl
  char *_cache;
  uint8_t *_cache_flags; // default should always be 0
  usize *_cache_offset;
  usize *_cache_blksize;
  // lru
  usize _lru_oracle;
  usize *_cache_last_ts;

public:
  HybridBlockCache(usize block_size = 4096, usize block_count = 1024) {
    this->_block_size = block_size;   // default 4096
    this->_block_count = block_count; // default 1024
    this->_flag_using = 0x01;
    this->_flag_dirty = 0x02;
    // cache impl
    this->_cache = new char[this->_block_size * this->_block_count];
    this->_cache_flags = new uint8_t[this->_block_count];
    this->_cache_offset = new usize[this->_block_count];
    this->_cache_blksize = new usize[this->_block_count];
    // lru
    this->_lru_oracle = 0;
    this->_cache_last_ts = new usize[this->_block_count];
  }
  ~HybridBlockCache() {
    delete[] this->_cache;
    delete[] this->_cache_flags;
    delete[] this->_cache_offset;
    delete[] this->_cache_blksize;
    delete[] this->_cache_last_ts;
    return;
  }
};
