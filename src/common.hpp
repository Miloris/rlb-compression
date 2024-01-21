#pragma once

#include <cstdint>
#include <vector>

#define BLOCK_SIZE 600
#define BIG_RLB_THRESHOLD 30000

typedef uint64_t usize;
typedef int64_t isize;

enum SeekOrigin {
  cur,
  begin,
  end,
};

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
  std::vector<char> rec1;
  std::vector<char> rec2;
  uint32_t identifier;
};