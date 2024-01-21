#pragma once

// #include "HybridFileHandle.hpp"
// #include "common.hpp"
// #include <sys/stat.h>
#include "IFileHandle.hpp"
#include "common.hpp"

void print_record(Record &rec);

bool compare_record(const Record &rec1, const Record &rec2);

uint32_t checkpoint_occ(Checkpoint *cp, char c);
/* Helper Functions */

bool check_file_exist(const char *path);

bool isChar(uint8_t c);
bool isNegative(uint32_t num);
/* first used I/Os*/
void write_uint32(uint32_t pos, IFileHandle *ifile, uint32_t num);
uint32_t read_uint32(uint32_t pos, IFileHandle *ifile);

void write_C(uint32_t pos, IFileHandle *ifile, uint32_t *C);
void read_C(uint32_t pos, IFileHandle *ifile, uint32_t *C);

void write_checkpoint(uint32_t pos, IFileHandle *ifile, Checkpoint &cp);

Checkpoint read_checkpoint(uint32_t pos, IFileHandle *ifile);