all: bin/search bin/compress

bin/search: src/search.cpp
	mkdir -p bin
	g++ -std=c++11 src/search.cpp -o bin/search

bin/compress: src/compress.cpp
	mkdir -p bin
	g++ -std=c++11 src/compress.cpp -o bin/compress

clean:
	rm -rf bin
