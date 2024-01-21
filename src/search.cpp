#include <iostream>
#include <sys/stat.h>
#include <vector>
// #include <map>
#include <algorithm>
#include <cstring>
#include <set>
#include <tuple>

#define BLOCK_SIZE 600
#define BIG_RLB_THRESHOLD 30000
using namespace std;

/* Customised FileHandle to accelerate file I/O */

typedef uint64_t usize;
typedef int64_t isize;

enum SeekOrigin {
  cur,
  begin,
  end,
};

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

class HybridFileHandle : public IFileHandle {
private:
  // cache definition
  typedef usize block_id;
  HybridBlockCache *_cache;
  // file api impl
  FILE *_handle;
  usize _file_size;
  usize _ptr;
  bool _eof_flag; // whether user had attempted to read over eof
  char _hf_r_buffer[8];

  // common helper utils
  inline bool _is_using_block(block_id id) {
    uint8_t flag = this->_cache->_cache_flags[id];
    return (bool)(flag & this->_cache->_flag_using);
  }
  // low-level (raw) block operations. they are really un-smart so they
  // always do what you tell them to
  void _flush_block(block_id id) {
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
  void _force_load_data_to_block(block_id id, usize offset) {
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
  void _evict_block(block_id id) {
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
  block_id _allocate_block() {
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
  block_id _get_block_r(usize offset) {
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
  block_id _get_block_w(usize offset) {
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
  usize _blk_read(usize offset, char *buffer, usize rel_offset, usize size) {
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
  usize _blk_write(usize offset, char *buffer, usize rel_offset, usize size) {
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
  usize _read(char *buffer, usize offset, usize size) {
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
  void _write(char *buffer, usize offset, usize size) {
    usize bytes_written = 0;
    usize blk_offset =
        (offset / this->_cache->_block_size) * this->_cache->_block_size;
    while (size > 0) {
      usize rel_offset = offset >= blk_offset ? offset - blk_offset : 0;
      usize cur_written = this->_blk_write(blk_offset, buffer + bytes_written,
                                           rel_offset, size);
      size -= cur_written;
      bytes_written += cur_written;
      blk_offset += this->_cache->_block_size;
    }
    usize new_file_size = offset + bytes_written;
    if (new_file_size > this->_file_size)
      this->_file_size = new_file_size;
    return;
  }
  void _flush() {
    // flush all dirty blocks
    for (block_id i = 0; i < this->_cache->_block_count; i++) {
      this->_flush_block(i);
    }
    return;
  }

public:
  HybridFileHandle(HybridBlockCache *cache_impl, const char *path,
                   const char *mode) {
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
  ~HybridFileHandle() {
    this->_flush();
    fclose(this->_handle);
    return;
  }
  void seek(isize offset, SeekOrigin origin) {
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
  usize tell() { return this->_ptr; }
  usize read(void *dest, usize size, usize count) {
    usize tot = size * count;
    usize bytes_read = this->_read((char *)dest, this->_ptr, tot);
    this->_ptr += bytes_read;
    this->_eof_flag = bytes_read < tot;
    return bytes_read / size;
  }
  void write(void *buffer, usize size, usize count) {
    this->_write((char *)buffer, this->_ptr, size * count);
    this->_ptr += size * count;
    return;
  }
  bool eof() { return this->_eof_flag; }
  int getc() {
    usize bytes_read =
        this->_read(this->_hf_r_buffer, this->_ptr, sizeof(char));
    this->_ptr += bytes_read;
    this->_eof_flag = bytes_read < 1;
    if (bytes_read == 0)
      return EOF;
    int ch = (uint8_t)this->_hf_r_buffer[0];
    if (ch < 0)
      ch += 256;
    return ch;
  }
};

/* Checkpoint Operations */

struct Checkpoint {
  /* Containing cnt[c] for RLB[0...rlb_idx) & bwt[0...bwt_idx) */
  uint32_t bwt_idx; // bytes read / next pos
  uint32_t rlb_idx; // bytes read / next pos
  uint32_t cnt[128];
};

struct Record {
  // [9]abcdefg
  // search for "de"
  // ^^^^^^ rec1
  //       ^^^^ rec2
  vector<char> rec1;
  vector<char> rec2;
  uint32_t identifier;
};

void print_record(Record &rec) {
  printf("[%u", rec.identifier);
  for (int i = rec.rec1.size() - 1; i >= 0; --i)
    printf("%c", rec.rec1[i]);
  for (int i = rec.rec2.size() - 1; i >= 0; --i)
    printf("%c", rec.rec2[i]);
  printf("\n");
}

bool compare_record(const Record &rec1, const Record &rec2) {
  return rec1.identifier < rec2.identifier;
}

uint32_t checkpoint_occ(Checkpoint *cp, char c) { return cp->cnt[uint8_t(c)]; }

/* Helper Functions */

inline bool check_file_exist(const char *path) {
  struct stat buffer;
  return (stat(path, &buffer) == 0);
}

inline bool isChar(uint8_t c) { return (c & 0x80) == 0; }

inline bool isNegative(uint32_t num) { return (num > 2e9); }

/* first used I/Os*/
void write_uint32(uint32_t pos, IFileHandle *ifile, uint32_t num) {
  ifile->seek(pos, SeekOrigin::begin);
  ifile->write(&num, sizeof(uint32_t), 1);
}
uint32_t read_uint32(uint32_t pos, IFileHandle *ifile) {
  uint32_t num;
  ifile->seek(pos, SeekOrigin::begin);
  ifile->read(&num, sizeof(uint32_t), 1);
  return num;
}
void write_C(uint32_t pos, IFileHandle *ifile, uint32_t *C) {
  ifile->seek(pos, SeekOrigin::begin);
  ifile->write(C, sizeof(uint32_t), 128);
}
void read_C(uint32_t pos, IFileHandle *ifile, uint32_t *C) {
  ifile->seek(pos, SeekOrigin::begin);
  ifile->read(C, sizeof(uint32_t), 128);
}

void write_checkpoint(uint32_t pos, IFileHandle *ifile, Checkpoint &cp) {
  ifile->seek(pos, SeekOrigin::begin);
  ifile->write(&cp, sizeof(Checkpoint), 1);
}
Checkpoint read_checkpoint(uint32_t pos, IFileHandle *ifile) {
  Checkpoint cp;
  ifile->seek(pos, SeekOrigin::begin);
  ifile->read(&cp, sizeof(Checkpoint), 1);
  return cp;
}

class IndexFile {
private:
  IFileHandle *ifile_rlb, *ifile_idx;
  HybridBlockCache *cache_rlb, *cache_idx;
  uint32_t max_identifier, min_identifier;
  uint32_t C[128];
  bool big_flag;
  vector<Checkpoint> checkpoints;
  vector<tuple<uint32_t, uint32_t>> cp_idx; // bwt_idx, rlb_idx
  uint32_t idx_top = 0;

  /* Index File Management */

  void load_idx_file() {
    uint32_t n_cp = read_uint32(0, ifile_idx);
    max_identifier = read_uint32(4, ifile_idx);
    min_identifier = read_uint32(8, ifile_idx);
    read_C(12, ifile_idx, C);

    Checkpoint cp;
    for (uint32_t i = 0; i < n_cp; i++) {
      cp = read_checkpoint(524 + i * sizeof(Checkpoint), ifile_idx);
      cp_idx.push_back(make_tuple(cp.bwt_idx, cp.rlb_idx));
    }
  }

  // write initial 524 bytes
  void initialize_idx() {
    write_uint32(0, ifile_idx, 0);
    write_uint32(4, ifile_idx, 0);
    write_uint32(8, ifile_idx, 0);

    write_C(12, ifile_idx, C);
    idx_top = 524;
  }

  void finalize_idx() {
    // 0: n_cp
    // 4: max_id
    // 8: min_id
    // 12: C
    // 524+: cp s
    ifile_idx->seek(0, SeekOrigin::begin);
    write_uint32(0, ifile_idx, cp_idx.size());
    write_uint32(4, ifile_idx, max_identifier);
    write_uint32(8, ifile_idx, min_identifier);
    write_C(12, ifile_idx, C);
  }

  // return the largest i s.t.
  //      checkpoints[i].bwt_idx <= pos
  size_t find_checkpoint_small(uint32_t pos) {
    size_t n = checkpoints.size();
    size_t l = 0, r = n - 1;
    // binary search
    // keep: cp[l].bwt_idx <= pos < cp[r].bwt_idx
    while (l + 1 < r) {
      size_t mid = (l + r) / 2;
      if (checkpoints[mid].bwt_idx <= pos)
        l = mid;
      else
        r = mid;
    }
    return l;
  }

  // return the largest i s.t.
  //      checkpoints[i].bwt_idx <= pos
  size_t find_checkpoint_big(uint32_t pos) {
    size_t n = cp_idx.size();
    size_t l = 0, r = n - 1;
    // binary search
    // keep: cp_idx[l].bwt_idx <= pos < cp_idx[r].bwt_idx
    while (l + 1 < r) {
      size_t mid = (l + r) / 2;
      if (get<0>(cp_idx[mid]) <= pos)
        l = mid;
      else
        r = mid;
    }
    return l;
  }

  tuple<uint32_t, uint32_t, uint32_t> query_cp(uint32_t i, char c) {
    if (!big_flag) {
      Checkpoint *cp = &checkpoints[find_checkpoint_small(i)];
      uint32_t bwt_idx = cp->bwt_idx, rlb_idx = cp->rlb_idx;
      return make_tuple(bwt_idx, rlb_idx, checkpoint_occ(cp, c));
    } else {
      uint32_t bwt_idx, rlb_idx;
      uint32_t cp_i = find_checkpoint_big(i);
      tie(bwt_idx, rlb_idx) = cp_idx[cp_i];

      Checkpoint cp =
          read_checkpoint(524 + cp_i * sizeof(Checkpoint), ifile_idx);
      return make_tuple(bwt_idx, rlb_idx, checkpoint_occ(&cp, c));
    }
  }

  void add_new_checkpoint(uint32_t *local_occ, uint32_t bwt_cnt,
                          uint32_t rlb_cnt) {
    // currently holding bwt[0...bwt_cnt), rlb[0...rlb_cnt)
    Checkpoint cp;
    cp.bwt_idx = bwt_cnt;
    cp.rlb_idx = rlb_cnt;
    memcpy(cp.cnt, local_occ, 128 * sizeof(uint32_t));
    if (!big_flag) {
      checkpoints.push_back(cp);
    } else {
      write_checkpoint(idx_top, ifile_idx, cp);
      cp_idx.push_back(make_tuple(bwt_cnt, rlb_cnt));
      idx_top += sizeof(Checkpoint);
    }
  }

  /* Index File Operation */
  tuple<char, uint32_t, uint32_t> get_next_part() {
    uint8_t c = ifile_rlb->getc();

    if (ifile_rlb->eof()) {
      return make_tuple(c, 0, 0); // end of file
    }
    uint32_t bwt_num = 1, rlb_num = 1;
    uint8_t c_nxt = ifile_rlb->getc();

    if (ifile_rlb->eof()) {
      return make_tuple(c, bwt_num, rlb_num);
    }
    if (ifile_rlb->eof() || isChar(c_nxt)) {
      // no number involed
      ifile_rlb->seek(-1, SeekOrigin::cur);
      return make_tuple(c, bwt_num, rlb_num);
    }

    bwt_num = 0;
    uint32_t shift = 0;
    while (true) {
      // append current c_nxt as next part of num
      uint32_t cur = c_nxt ^ 0x80;
      bwt_num = bwt_num | (cur << shift);
      rlb_num += 1;
      shift += 7;
      c_nxt = ifile_rlb->getc();

      if (ifile_rlb->eof() || isChar(c_nxt)) {
        ifile_rlb->seek(-1, SeekOrigin::cur);
        break;
      }
    }
    return make_tuple(c, bwt_num + 3, rlb_num);
  }

  uint32_t get_C(char c) { return C[uint8_t(c)]; }

  char get_L(uint32_t i) {
    // get L[i]
    uint32_t bwt_idx, rlb_idx, _;
    tie(bwt_idx, rlb_idx, _) = query_cp(i, 0);
    ifile_rlb->seek(rlb_idx, SeekOrigin::begin);

    uint32_t bwt_num, rlb_num;
    char c;
    while (true) {
      // L[0...bwt_idx) in the previous turns
      tie(c, bwt_num, rlb_num) = get_next_part();
      if (bwt_idx + bwt_num <= i) {
        bwt_idx += bwt_num;
        continue;
      }
      return c;
    }
  }

  uint32_t get_occ(uint32_t i, char target_c) {
    if (isNegative(i))
      return 0;
    uint32_t bwt_idx, rlb_idx, res;
    tie(bwt_idx, rlb_idx, res) = query_cp(i, target_c);
    ifile_rlb->seek(rlb_idx, SeekOrigin::begin);
    uint32_t bwt_num, rlb_num;
    char c;
    while (bwt_idx <= i) {
      // L[0...bwt_idx) in the previous turns
      tie(c, bwt_num, rlb_num) = get_next_part();
      if (c == target_c && bwt_idx + bwt_num <= i + 1) {
        res += bwt_num;
      } else if (c == target_c && bwt_idx + bwt_num > i + 1) {
        res += i - bwt_idx + 1;
      }
      bwt_idx += bwt_num;
    }
    return res;
  }

  uint32_t get_LF(uint32_t i, char c) { return get_C(c) + get_occ(i, c) - 1; }

  /* Parse RLB file */

  // find the [id]xxxxxx
  // search id and store in rec
  bool search_id(uint32_t pos, Record &rec) {
    bool has_close = false;
    while (true) {
      char c = get_L(pos);
      rec.rec1.push_back(c);
      if (c == '[') {
        return false;
      }

      if (c == ']') {
        uint32_t _;
        tie(rec.identifier, _) = get_identifier(pos);
        return true;
      }
      uint32_t previous_pos = get_LF(pos, c);
      pos = previous_pos;
    }
  }

  /* Backward Search */

  // return (l, r) when perform backward search
  tuple<uint32_t, uint32_t> find_match(char *pattern) {
    int n = strlen(pattern);
    // backward search to find the matches
    uint8_t c = pattern[n - 1];
    uint32_t l = C[c], r = C[c + 1] - 1;

    for (int i = n - 2; i >= 0 && l <= r; i--) {
      c = pattern[i];
      l = C[c] + get_occ(l - 1, c);
      r = C[c] + get_occ(r, c) - 1;
    }
    return make_tuple(l, r);
  }

  // given the position of the close bracket
  // search backward and return the identifier and pos of [
  // e.g.    [9]Data compression[10]
  //          ^ return id
  //         ^  and pos of '['  -> (9, pos of '[')
  tuple<uint32_t, uint32_t> get_identifier(const uint32_t close_bracket_pos) {
    uint32_t id = 0, base = 1;
    uint32_t pos = close_bracket_pos;
    while (true) {
      char c = get_L(pos);
      if (c == '[') {
        return make_tuple(id, pos);
      }
      if (c != ']') {
        id += (c - '0') * base;
        base *= 10;
      }
      pos = get_LF(pos, c);
    }
  }

  // find if "[id]" exists
  // return t/f, and pos
  tuple<bool, uint32_t> find_identifier(uint32_t id) {
    char buffer[20]; // Ensure enough space to store the number as a string
    snprintf(buffer, sizeof(buffer), "[%d]", id);
    uint32_t l, r;
    tie(l, r) = find_match(buffer);
    if (l <= r) {
      return make_tuple(true, l);
    } else {
      return make_tuple(false, 0);
    }
  }

  // analyze to initialize id_to_pos[id] = pos;
  void analyze_max_min_id(uint32_t any_id) {
    // find min identifier
    uint32_t pos, l, r, mid;
    bool flag;
    tie(flag, pos) = find_identifier(0);

    if (flag)
      min_identifier = 0;
    else {
      // use binary search to find min
      // keep l not exists, r exists
      l = 0, r = any_id;
      while (l + 1 < r) {
        mid = (l + r) / 2;
        tie(flag, pos) = find_identifier(mid);
        if (flag)
          r = mid;
        else
          l = mid;
      }
      min_identifier = r;
    }

    // use binary search to find max
    // keep l exists, r not exists
    l = any_id, r = 0x7fffffff;
    while (l + 1 < r) {
      mid = (l + r) / 2;
      tie(flag, pos) = find_identifier(mid);

      if (flag)
        l = mid;
      else
        r = mid;
    }
    max_identifier = l;
  }

  void load_rlb_file(const char *idx_path) {
    char c;
    uint32_t bwt_num, rlb_num;
    uint32_t bwt_cnt = 0, rlb_cnt = 0;
    uint32_t last_record_rlb_cnt = 0;
    uint32_t local_occ[128];
    uint32_t start_point_open_bracket = 0;
    memset(local_occ, 0, sizeof(local_occ));

    add_new_checkpoint(local_occ, bwt_cnt, rlb_cnt);
    uint32_t any_close_bracket_pos;
    // scan through rlb and load all

    while (true) {
      // e.g. next part "a{8}" ->
      // bwt_num = 8, rlb_num = 2, c = 'a'
      tie(c, bwt_num, rlb_num) = get_next_part();
      if (bwt_num == 0)
        break;
      if (c == ']') {
        any_close_bracket_pos = bwt_cnt;
      }
      local_occ[uint8_t(c)] += bwt_num;
      bwt_cnt += bwt_num;
      rlb_cnt += rlb_num;

      if (rlb_cnt - last_record_rlb_cnt >= BLOCK_SIZE) {
        add_new_checkpoint(local_occ, bwt_cnt, rlb_cnt);
        last_record_rlb_cnt = rlb_cnt;
      }
    }
    add_new_checkpoint(local_occ, bwt_cnt, rlb_cnt);
    C[0] = 0;
    for (uint8_t c = 1; c < 128; c++) {
      C[c] = C[c - 1] + local_occ[c - 1];
    }

    // Analyse for bracket / identifier
    uint32_t any_id, _;
    tie(any_id, _) = get_identifier(any_close_bracket_pos);
    analyze_max_min_id(any_id);
  }

  void fill_rec(Record &rec, uint32_t found_pos) {
    uint32_t id = rec.identifier, pos;
    uint32_t next_id = (id == max_identifier) ? min_identifier : (id + 1);
    bool __;
    tie(__, pos) = find_identifier(next_id);

    while (true) {
      if (pos == found_pos) {
        break;
      }
      char c = get_L(pos);
      rec.rec2.push_back(c);

      uint32_t previous_pos = get_LF(pos, c);
      pos = previous_pos;
    }
  }

public:
  IndexFile(const char *rlb_path, const char *idx_path) {
    cache_rlb = new HybridBlockCache(4096, 128);
    ifile_rlb = new HybridFileHandle(cache_rlb, rlb_path, "rb");
    uint32_t rlb_size = ifile_rlb->get_size();
    big_flag = rlb_size > BIG_RLB_THRESHOLD;
    max_identifier = 0;
    min_identifier = 0xffffffff;
    if (check_file_exist(idx_path)) {
      cache_idx = new HybridBlockCache(4096, 64);
      ifile_idx = new HybridFileHandle(cache_idx, idx_path, "rb+");
      load_idx_file();
    } else {
      if (big_flag) {
        cache_idx = new HybridBlockCache(4096, 64);
        ifile_idx = new HybridFileHandle(cache_idx, idx_path, "wb+");
        initialize_idx();
      }
      load_rlb_file(idx_path);
      if (big_flag) {
        finalize_idx();
        ifile_idx->flush();
      }
    }
  }

  ~IndexFile() {
    delete ifile_rlb;
    delete cache_rlb;
    if (ifile_idx) {
      delete ifile_idx;
      delete cache_idx;
    }
  }

  void search(char *pattern) {
    uint32_t l, r;
    tie(l, r) = find_match(pattern);
    set<uint32_t> visited_id;
    vector<Record> ans;
    for (uint32_t pos = l; pos <= r; pos++) {
      Record rec;
      bool valid_res =
          search_id(pos, rec); // in case we search for an identifier
      if (!valid_res)
        continue;
      if (visited_id.find(rec.identifier) == visited_id.end()) {
        fill_rec(rec, pos);
        ans.push_back(rec);
        visited_id.insert(rec.identifier);
      }
    }
    sort(ans.begin(), ans.end(), compare_record);
    for (auto it1 = ans.begin(); it1 != ans.end(); ++it1) {
      print_record(*it1);
    }
  }
};

int main(int argc, char **argv) {

  char *rlb_path = argv[1], *index_path = argv[2], *pattern = argv[3];

  IndexFile index_file = IndexFile(rlb_path, index_path);
  index_file.search(pattern);
}
