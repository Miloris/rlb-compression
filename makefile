all:
	g++ -std=c++11 bwtsearch.cpp  -o bwtsearch

# run:
# 	time valgrind --tool=massif --pages-as-heap=yes  time ./bwtsearch test/dummy.rlb idx/dummy.idx "in " 



clean:
	rm -f bwtsearch 

