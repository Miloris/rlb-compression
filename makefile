# all: bin/search bin/compress

# bin/search: src/search.cpp
# 	mkdir -p bin
# 	g++ -std=c++11 src/search.cpp -o bin/search

# bin/compress: src/compress.cpp
# 	mkdir -p bin
# 	g++ -std=c++11 src/compress.cpp -o bin/compress

# clean:
# 	rm -rf bin


# 定义编译器和编译选项
CXX = g++
CXXFLAGS = -std=c++11 -Wall

# 定义目标可执行文件
TARGETS = bin/search bin/compress

# 默认目标
all: $(TARGETS)

# 编译 search 可执行文件
bin/search: src/search.cpp src/IFileHandle.hpp src/common.hpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ src/search.cpp

# 编译 compress 可执行文件
bin/compress: src/compress.cpp
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ src/compress.cpp

# 清理目标
clean:
	rm -rf bin
