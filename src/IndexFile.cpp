#include "IndexFile.hpp"
#include "utils.hpp"

/* Index File Management */

void IndexFile::load_idx_file() {
  uint32_t n_cp = read_uint32(0, ifile_idx);
  max_identifier = read_uint32(4, ifile_idx);
  min_identifier = read_uint32(8, ifile_idx);
  read_C(12, ifile_idx, C);

  Checkpoint cp;
  for (uint32_t i = 0; i < n_cp; i++) {
    cp = read_checkpoint(524 + i * sizeof(Checkpoint), ifile_idx);
    cp_idx.push_back(std::make_tuple(cp.bwt_idx, cp.rlb_idx));
  }
}

// write initial 524 bytes
void IndexFile::initialize_idx() {
  write_uint32(0, ifile_idx, 0);
  write_uint32(4, ifile_idx, 0);
  write_uint32(8, ifile_idx, 0);

  write_C(12, ifile_idx, C);
  idx_top = 524;
}

void IndexFile::finalize_idx() {
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
size_t IndexFile::find_checkpoint_small(uint32_t pos) {
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
size_t IndexFile::find_checkpoint_big(uint32_t pos) {
  size_t n = cp_idx.size();
  size_t l = 0, r = n - 1;
  // binary search
  // keep: cp_idx[l].bwt_idx <= pos < cp_idx[r].bwt_idx
  while (l + 1 < r) {
    size_t mid = (l + r) / 2;
    if (std::get<0>(cp_idx[mid]) <= pos)
      l = mid;
    else
      r = mid;
  }
  return l;
}

std::tuple<uint32_t, uint32_t, uint32_t> IndexFile::query_cp(uint32_t i,
                                                             char c) {
  if (!big_flag) {
    Checkpoint *cp = &checkpoints[find_checkpoint_small(i)];
    uint32_t bwt_idx = cp->bwt_idx, rlb_idx = cp->rlb_idx;
    return std::make_tuple(bwt_idx, rlb_idx, checkpoint_occ(cp, c));
  } else {
    uint32_t bwt_idx, rlb_idx;
    uint32_t cp_i = find_checkpoint_big(i);
    std::tie(bwt_idx, rlb_idx) = cp_idx[cp_i];

    Checkpoint cp = read_checkpoint(524 + cp_i * sizeof(Checkpoint), ifile_idx);
    return std::make_tuple(bwt_idx, rlb_idx, checkpoint_occ(&cp, c));
  }
}

void IndexFile::add_new_checkpoint(uint32_t *local_occ, uint32_t bwt_cnt,
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
    cp_idx.push_back(std::make_tuple(bwt_cnt, rlb_cnt));
    idx_top += sizeof(Checkpoint);
  }
}

/* Index File Operation */
std::tuple<char, uint32_t, uint32_t> IndexFile::get_next_part() {
  uint8_t c = ifile_rlb->getc();

  if (ifile_rlb->eof()) {
    return std::make_tuple(c, 0, 0); // end of file
  }
  uint32_t bwt_num = 1, rlb_num = 1;
  uint8_t c_nxt = ifile_rlb->getc();

  if (ifile_rlb->eof()) {
    return std::make_tuple(c, bwt_num, rlb_num);
  }
  if (ifile_rlb->eof() || isChar(c_nxt)) {
    // no number involed
    ifile_rlb->seek(-1, SeekOrigin::cur);
    return std::make_tuple(c, bwt_num, rlb_num);
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
  return std::make_tuple(c, bwt_num + 3, rlb_num);
}

uint32_t IndexFile::get_C(char c) { return C[uint8_t(c)]; }

char IndexFile::get_L(uint32_t i) {
  // get L[i]
  uint32_t bwt_idx, rlb_idx, _;
  std::tie(bwt_idx, rlb_idx, _) = query_cp(i, 0);
  ifile_rlb->seek(rlb_idx, SeekOrigin::begin);

  uint32_t bwt_num, rlb_num;
  char c;
  while (true) {
    // L[0...bwt_idx) in the previous turns
    std::tie(c, bwt_num, rlb_num) = get_next_part();
    if (bwt_idx + bwt_num <= i) {
      bwt_idx += bwt_num;
      continue;
    }
    return c;
  }
}

uint32_t IndexFile::get_occ(uint32_t i, char target_c) {
  if (isNegative(i))
    return 0;
  uint32_t bwt_idx, rlb_idx, res;
  std::tie(bwt_idx, rlb_idx, res) = query_cp(i, target_c);
  ifile_rlb->seek(rlb_idx, SeekOrigin::begin);
  uint32_t bwt_num, rlb_num;
  char c;
  while (bwt_idx <= i) {
    // L[0...bwt_idx) in the previous turns
    std::tie(c, bwt_num, rlb_num) = get_next_part();
    if (c == target_c && bwt_idx + bwt_num <= i + 1) {
      res += bwt_num;
    } else if (c == target_c && bwt_idx + bwt_num > i + 1) {
      res += i - bwt_idx + 1;
    }
    bwt_idx += bwt_num;
  }
  return res;
}

uint32_t IndexFile::get_LF(uint32_t i, char c) {
  return get_C(c) + get_occ(i, c) - 1;
}

/* Parse RLB file */

// find the [id]xxxxxx
// search id and store in rec
bool IndexFile::search_id(uint32_t pos, Record &rec) {
  while (true) {
    char c = get_L(pos);
    rec.rec1.push_back(c);
    if (c == '[') {
      return false;
    }

    if (c == ']') {
      uint32_t _;
      std::tie(rec.identifier, _) = get_identifier(pos);
      return true;
    }
    uint32_t previous_pos = get_LF(pos, c);
    pos = previous_pos;
  }
}

/* Backward Search */

// return (l, r) when perform backward search
std::tuple<uint32_t, uint32_t> IndexFile::find_match(char *pattern) {
  int n = strlen(pattern);
  // backward search to find the matches
  uint8_t c = pattern[n - 1];
  uint32_t l = C[c], r = C[c + 1] - 1;

  for (int i = n - 2; i >= 0 && l <= r; i--) {
    c = pattern[i];
    l = C[c] + get_occ(l - 1, c);
    r = C[c] + get_occ(r, c) - 1;
  }
  return std::make_tuple(l, r);
}

// given the position of the close bracket
// search backward and return the identifier and pos of [
// e.g.    [9]Data compression[10]
//          ^ return id
//         ^  and pos of '['  -> (9, pos of '[')
std::tuple<uint32_t, uint32_t>
IndexFile::get_identifier(const uint32_t close_bracket_pos) {
  uint32_t id = 0, base = 1;
  uint32_t pos = close_bracket_pos;
  while (true) {
    char c = get_L(pos);
    if (c == '[') {
      return std::make_tuple(id, pos);
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
std::tuple<bool, uint32_t> IndexFile::find_identifier(uint32_t id) {
  char buffer[20]; // Ensure enough space to store the number as a string
  snprintf(buffer, sizeof(buffer), "[%d]", id);
  uint32_t l, r;
  std::tie(l, r) = find_match(buffer);
  if (l <= r) {
    return std::make_tuple(true, l);
  } else {
    return std::make_tuple(false, 0);
  }
}

// analyze to initialize id_to_pos[id] = pos;
void IndexFile::analyze_max_min_id(uint32_t any_id) {
  // find min identifier
  uint32_t pos, l, r, mid;
  bool flag;
  std::tie(flag, pos) = find_identifier(0);

  if (flag)
    min_identifier = 0;
  else {
    // use binary search to find min
    // keep l not exists, r exists
    l = 0, r = any_id;
    while (l + 1 < r) {
      mid = (l + r) / 2;
      std::tie(flag, pos) = find_identifier(mid);
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
    std::tie(flag, pos) = find_identifier(mid);

    if (flag)
      l = mid;
    else
      r = mid;
  }
  max_identifier = l;
}

void IndexFile::load_rlb_file(const char *idx_path) {
  char c;
  uint32_t bwt_num, rlb_num;
  uint32_t bwt_cnt = 0, rlb_cnt = 0;
  uint32_t last_record_rlb_cnt = 0;
  uint32_t local_occ[128];
  memset(local_occ, 0, sizeof(local_occ));

  add_new_checkpoint(local_occ, bwt_cnt, rlb_cnt);
  uint32_t any_close_bracket_pos;
  // scan through rlb and load all

  while (true) {
    // e.g. next part "a{8}" ->
    // bwt_num = 8, rlb_num = 2, c = 'a'
    std::tie(c, bwt_num, rlb_num) = get_next_part();
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
  std::tie(any_id, _) = get_identifier(any_close_bracket_pos);
  analyze_max_min_id(any_id);
}

void IndexFile::fill_rec(Record &rec, uint32_t found_pos) {
  uint32_t id = rec.identifier, pos;
  uint32_t next_id = (id == max_identifier) ? min_identifier : (id + 1);
  bool __;
  std::tie(__, pos) = find_identifier(next_id);

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

IndexFile::IndexFile(const char *rlb_path, const char *idx_path) {
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

IndexFile::~IndexFile() {
  delete ifile_rlb;
  delete cache_rlb;
  if (ifile_idx) {
    delete ifile_idx;
    delete cache_idx;
  }
}

void IndexFile::search(char *pattern) {
  uint32_t l, r;
  std::tie(l, r) = find_match(pattern);
  std::set<uint32_t> visited_id;
  std::vector<Record> ans;
  for (uint32_t pos = l; pos <= r; pos++) {
    Record rec;
    bool valid_res = search_id(pos, rec); // in case we search for an identifier
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
