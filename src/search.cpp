#include "IndexFile.hpp"

using namespace std;

int main(int argc, char **argv) {

  char *rlb_path = argv[1], *index_path = argv[2], *pattern = argv[3];

  IndexFile index_file = IndexFile(rlb_path, index_path);
  index_file.search(pattern);
}
