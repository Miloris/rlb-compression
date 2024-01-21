CXX = g++
CXXFLAGS = -std=c++11 -Wall -Isrc -Iinclude

TARGETS = bin/search bin/compress

OBJS = bin/HybridFileHandle.o bin/IndexFile.o bin/utils.o

all: $(TARGETS)

bin/search: src/search.cpp $(OBJS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

bin/compress: src/compress.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $<

bin/HybridFileHandle.o: src/HybridFileHandle.cpp include/HybridFileHandle.hpp include/HybridBlockCache.hpp include/IFileHandle.hpp include/common.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/IndexFile.o: src/IndexFile.cpp include/IndexFile.hpp include/HybridFileHandle.hpp include/IFileHandle.hpp include/utils.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/utils.o: src/utils.cpp include/utils.hpp include/common.hpp include/IFileHandle.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf bin
