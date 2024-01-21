#include "utils.hpp"
#include <sys/stat.h>

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

bool check_file_exist(const char *path) {
  struct stat buffer;
  return (stat(path, &buffer) == 0);
}

bool isChar(uint8_t c) { return (c & 0x80) == 0; }

bool isNegative(uint32_t num) { return (num > 2e9); }

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
