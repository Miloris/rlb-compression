#pragma once

#include "HybridBlockCache.hpp"
#include "IFileHandle.hpp"

typedef usize block_id;

class HybridFileHandle : public IFileHandle {
private:
  // cache definition
  HybridBlockCache *_cache;

  // file api impl
  FILE *_handle;
  usize _file_size;
  usize _ptr;
  bool _eof_flag; // whether user had attempted to read over eof
  char _hf_r_buffer[8];

  // common helper utils
  inline bool _is_using_block(block_id id);

  // low-level (raw) block operations. they are really un-smart so they
  // always do what you tell them to
  void _flush_block(block_id id);
  void _force_load_data_to_block(block_id id, usize offset);
  void _evict_block(block_id id);
  block_id _allocate_block();

  // block operations that are pretty smart and abstracts away the details
  block_id _get_block_r(usize offset);
  block_id _get_block_w(usize offset);

  // block-level i/o operations not yet abstracted
  usize _blk_read(usize offset, char *buffer, usize rel_offset, usize size);
  usize _blk_write(usize offset, char *buffer, usize rel_offset, usize size);

  // abstracted i/o operations
  usize _read(char *buffer, usize offset, usize size);
  void _write(char *buffer, usize offset, usize size);
  void _flush();

public:
  HybridFileHandle(HybridBlockCache *cache_impl, const char *path,
                   const char *mode);
  ~HybridFileHandle();

  void seek(isize offset, SeekOrigin origin);
  usize tell();
  usize read(void *dest, usize size, usize count);
  void write(void *buffer, usize size, usize count);
  bool eof();
  int getc();
};
