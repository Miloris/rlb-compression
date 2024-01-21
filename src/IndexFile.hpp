#pragma once

#include "HybridFileHandle.hpp"
#include "IFileHandle.hpp"
#include "utils.hpp"
#include <set>

class IndexFile {
private:
  IFileHandle *ifile_rlb, *ifile_idx;
  HybridBlockCache *cache_rlb, *cache_idx;
  uint32_t max_identifier, min_identifier;
  uint32_t C[128];
  bool big_flag;
  std::vector<Checkpoint> checkpoints;
  std::vector<std::tuple<uint32_t, uint32_t>> cp_idx;
  uint32_t idx_top = 0;

  /* Index File Management */

  void load_idx_file();
  void initialize_idx();
  void finalize_idx();
  size_t find_checkpoint_small(uint32_t pos);
  size_t find_checkpoint_big(uint32_t pos);
  std::tuple<uint32_t, uint32_t, uint32_t> query_cp(uint32_t i, char c);
  void add_new_checkpoint(uint32_t *local_occ, uint32_t bwt_cnt,
                          uint32_t rlb_cnt);

  /* Index File Operation */
  std::tuple<char, uint32_t, uint32_t> get_next_part();
  uint32_t get_C(char c);
  char get_L(uint32_t i);
  uint32_t get_occ(uint32_t i, char target_c);
  uint32_t get_LF(uint32_t i, char c);

  /* Parse RLB file */
  bool search_id(uint32_t pos, Record &rec);

  /* Backward Search */
  std::tuple<uint32_t, uint32_t> find_match(char *pattern);
  std::tuple<uint32_t, uint32_t>
  get_identifier(const uint32_t close_bracket_pos);
  std::tuple<bool, uint32_t> find_identifier(uint32_t id);
  void analyze_max_min_id(uint32_t any_id);
  void load_rlb_file(const char *idx_path);
  void fill_rec(Record &rec, uint32_t found_pos);

public:
  IndexFile(const char *rlb_path, const char *idx_path);
  ~IndexFile();
  void search(char *pattern);
};
