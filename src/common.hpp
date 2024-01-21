#pragma once

#include <cstdint>
#define BLOCK_SIZE 600
#define BIG_RLB_THRESHOLD 30000

typedef uint64_t usize;
typedef int64_t isize;

enum SeekOrigin {
  cur,
  begin,
  end,
};