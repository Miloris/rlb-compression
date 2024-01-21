#include "HybridFileHandle.hpp"

// common helper utils
inline bool HybridFileHandle::_is_using_block(block_id id) {
  uint8_t flag = this->_cache->_cache_flags[id];
  return (bool)(flag & this->_cache->_flag_using);
}
// low-level (raw) block operations. they are really un-smart so they
// always do what you tell them to
void HybridFileHandle::_flush_block(block_id id) {
  uint8_t flag = this->_cache->_cache_flags[id];
  if (!(flag & this->_cache->_flag_using))
    return;
  if (flag & this->_cache->_flag_dirty) {
    // write back to file
    fseek(this->_handle, this->_cache->_cache_offset[id], SEEK_SET);
    fwrite(this->_cache->_cache + id * this->_cache->_block_size, 1,
           this->_cache->_cache_blksize[id], this->_handle);
    // clear dirty flag
    flag ^= this->_cache->_flag_dirty;
  }
  this->_cache->_cache_flags[id] = flag;
  return;
}
void HybridFileHandle::_force_load_data_to_block(block_id id, usize offset) {
  // assuming block is unused
  uint8_t flag = this->_cache->_cache_flags[id];
  if (!(flag & this->_cache->_flag_using))
    return;
  // read from file
  fseek(this->_handle, offset, SEEK_SET);
  usize bytes_read =
      fread(this->_cache->_cache + id * this->_cache->_block_size, 1,
            this->_cache->_block_size, this->_handle);
  // set block info
  flag |= this->_cache->_flag_using;
  this->_cache->_cache_flags[id] = flag;
  this->_cache->_cache_offset[id] = offset;
  this->_cache->_cache_blksize[id] = bytes_read;
  return;
}
void HybridFileHandle::_evict_block(block_id id) {
  // skip if block is not used
  uint8_t flag = this->_cache->_cache_flags[id];
  if (!(flag & this->_cache->_flag_using))
    return;
  // flush data
  this->_flush_block(id);
  // clear block info
  flag ^= this->_cache->_flag_using;
  this->_cache->_cache_flags[id] = flag;
  return;
}
block_id HybridFileHandle::_allocate_block() {
  // LOOK HERE: change cache eviction logic from here
  // find a block to evict, and there's always at least one
  block_id to_kick = 0;
  usize min_ts = -1; // max usize
  for (block_id i = 0; i < this->_cache->_block_count; i++) {
    if (!this->_is_using_block(i)) {
      to_kick = i;
      break;
    }
    usize cur_ts = this->_cache->_cache_last_ts[i];
    if (cur_ts < min_ts) {
      min_ts = cur_ts;
      to_kick = i;
    }
  }
  // now kicking it
  this->_evict_block(to_kick);
  // assigning it a new life
  uint8_t flag = 0 | this->_cache->_flag_using;
  memset(this->_cache->_cache + to_kick * this->_cache->_block_size, 0,
         this->_cache->_block_size);
  this->_cache->_cache_flags[to_kick] = flag;
  this->_cache->_cache_offset[to_kick] = 0;
  this->_cache->_cache_blksize[to_kick] = 0;
  this->_cache->_cache_last_ts[to_kick] = ++this->_cache->_lru_oracle;
  return to_kick;
}
// block operations that are pretty smart and abstracts away the details
block_id HybridFileHandle::_get_block_r(usize offset) {
  // find existing block (offset must be exact multiples of the block size)
  for (block_id i = 0; i < this->_cache->_block_count; i++) {
    if (!this->_is_using_block(i))
      continue;
    if (this->_cache->_cache_offset[i] == offset) {
      this->_cache->_cache_last_ts[i] = ++this->_cache->_lru_oracle;
      return i;
    }
  }
  // we need to load this from file
  block_id blk = this->_allocate_block();
  this->_force_load_data_to_block(blk, offset);
  return blk;
}
block_id HybridFileHandle::_get_block_w(usize offset) {
  // find existing block in cache
  for (block_id i = 0; i < this->_cache->_block_count; i++) {
    if (!this->_is_using_block(i))
      continue;
    if (this->_cache->_cache_offset[i] == offset) {
      this->_cache->_cache_flags[i] |= this->_cache->_flag_dirty;
      this->_cache->_cache_last_ts[i] = ++this->_cache->_lru_oracle;
      return i;
    }
  }
  // creating a new block that should be appended to the end of the file
  block_id blk = this->_allocate_block();
  if (offset < this->_file_size) {
    this->_force_load_data_to_block(blk, offset);
  } else {
    this->_cache->_cache_offset[blk] = offset;
  }
  this->_cache->_cache_flags[blk] |= this->_cache->_flag_dirty;
  return blk;
}
// block-level i/o operations not yet abstracted
usize HybridFileHandle::_blk_read(usize offset, char *buffer, usize rel_offset,
                                  usize size) {
  // find block
  block_id blk = this->_get_block_r(offset);
  // read from block (offset in params is multiple of _block_size)
  usize blk_size = this->_cache->_cache_blksize[blk];
  usize bytes_read = blk_size - rel_offset;
  if (bytes_read > size)
    bytes_read = size;
  memcpy(buffer,
         this->_cache->_cache + blk * this->_cache->_block_size + rel_offset,
         bytes_read);
  return bytes_read;
}
usize HybridFileHandle::_blk_write(usize offset, char *buffer, usize rel_offset,
                                   usize size) {
  // find block
  block_id blk = this->_get_block_w(offset);
  // write to block (similar to _blk_read)
  usize blk_size = this->_cache->_cache_blksize[blk];
  usize bytes_written = this->_cache->_block_size - rel_offset;
  if (bytes_written > size)
    bytes_written = size;
  memcpy(this->_cache->_cache + blk * this->_cache->_block_size + rel_offset,
         buffer, bytes_written);
  // update block info
  usize new_blk_size = rel_offset + bytes_written;
  if (new_blk_size > blk_size)
    this->_cache->_cache_blksize[blk] = new_blk_size;
  return bytes_written;
}
// abstracted i/o operations
usize HybridFileHandle::_read(char *buffer, usize offset, usize size) {
  usize bytes_read = 0;
  usize blk_offset =
      (offset / this->_cache->_block_size) * this->_cache->_block_size;
  while (size > 0 && offset + bytes_read < this->_file_size) {
    usize rel_offset = offset >= blk_offset ? offset - blk_offset : 0;
    usize cur_read =
        this->_blk_read(blk_offset, buffer + bytes_read, rel_offset, size);
    size -= cur_read;
    bytes_read += cur_read;
    blk_offset += this->_cache->_block_size;
  }
  return bytes_read;
}
void HybridFileHandle::_write(char *buffer, usize offset, usize size) {
  usize bytes_written = 0;
  usize blk_offset =
      (offset / this->_cache->_block_size) * this->_cache->_block_size;
  while (size > 0) {
    usize rel_offset = offset >= blk_offset ? offset - blk_offset : 0;
    usize cur_written =
        this->_blk_write(blk_offset, buffer + bytes_written, rel_offset, size);
    size -= cur_written;
    bytes_written += cur_written;
    blk_offset += this->_cache->_block_size;
  }
  usize new_file_size = offset + bytes_written;
  if (new_file_size > this->_file_size)
    this->_file_size = new_file_size;
  return;
}
void HybridFileHandle::_flush() {
  // flush all dirty blocks
  for (block_id i = 0; i < this->_cache->_block_count; i++) {
    this->_flush_block(i);
  }
  return;
}

HybridFileHandle::HybridFileHandle(HybridBlockCache *cache_impl,
                                   const char *path, const char *mode) {
  this->_cache = cache_impl;
  // file api impl
  this->_handle = fopen(path, mode);
  fseek(this->_handle, 0, SEEK_END);
  this->_file_size = ftell(this->_handle);
  this->_ptr = 0;
  this->_eof_flag = false;
  memset(this->_hf_r_buffer, 0, sizeof(char) * 8);
  return;
}
HybridFileHandle::~HybridFileHandle() {
  this->_flush();
  fclose(this->_handle);
  return;
}
void HybridFileHandle::seek(isize offset, SeekOrigin origin) {
  isize new_ptr = 0;
  if (origin == SeekOrigin::cur) {
    new_ptr = (isize)this->_ptr;
  } else if (origin == SeekOrigin::begin) {
    new_ptr = 0;
  } else if (origin == SeekOrigin::end) {
    new_ptr = (isize)this->_file_size;
  }
  new_ptr += offset;
  if (new_ptr < 0)
    new_ptr = 0;
  this->_ptr = (usize)new_ptr;
  return;
}
usize HybridFileHandle::tell() { return this->_ptr; }
usize HybridFileHandle::read(void *dest, usize size, usize count) {
  usize tot = size * count;
  usize bytes_read = this->_read((char *)dest, this->_ptr, tot);
  this->_ptr += bytes_read;
  this->_eof_flag = bytes_read < tot;
  return bytes_read / size;
}
void HybridFileHandle::write(void *buffer, usize size, usize count) {
  this->_write((char *)buffer, this->_ptr, size * count);
  this->_ptr += size * count;
  return;
}
bool HybridFileHandle::eof() { return this->_eof_flag; }
int HybridFileHandle::getc() {
  usize bytes_read = this->_read(this->_hf_r_buffer, this->_ptr, sizeof(char));
  this->_ptr += bytes_read;
  this->_eof_flag = bytes_read < 1;
  if (bytes_read == 0)
    return EOF;
  int ch = (uint8_t)this->_hf_r_buffer[0];
  if (ch < 0)
    ch += 256;
  return ch;
}
