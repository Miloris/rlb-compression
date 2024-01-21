CXX = g++
CXXFLAGS = -std=c++11 -Wall -Isrc

TARGETS = bin/search bin/compress

OBJS = bin/HybridFileHandle.o bin/IndexFile.o bin/utils.o

all: $(TARGETS)

bin/search: src/search.cpp $(OBJS)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

bin/compress: src/compress.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $<

bin/HybridFileHandle.o: src/HybridFileHandle.cpp src/HybridFileHandle.hpp src/HybridBlockCache.hpp src/IFileHandle.hpp src/common.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/IndexFile.o: src/IndexFile.cpp src/IndexFile.hpp src/HybridFileHandle.hpp src/IFileHandle.hpp src/utils.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/utils.o: src/utils.cpp src/utils.hpp src/common.hpp src/IFileHandle.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf bin
