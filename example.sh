# This file shows how to run the compress and search program

make

# Compress the file dummy.txt and output to dummy.rlb
./bin/compress dummy/dummy.txt dummy/dummy.rlb

# Search for the word "in" in the compressed file dummy.rlb and output to dummy.idx
# ./bin/search dummy/dummy.rlb dummy/dummy.idx "in" > dummy/dummy.res
./bin/search dummy/dummy.rlb dummy/dummy.idx "in" 
