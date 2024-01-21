# This file shows how to run the compress and search program

make

# Compress the file dummy.txt and output to dummy.rlb
./bin/compress example/dummy.txt example/dummy.rlb

# Search for the word "in" in the compressed file dummy.rlb and output to dummy.idx
./bin/search example/dummy.rlb example/dummy.idx "in" > example/dummy.res
# ./bin/search example/dummy.rlb example/dummy.idx "in" 
