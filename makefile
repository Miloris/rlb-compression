CXX = g++
CXXFLAGS = -std=c++11 -Wall -Iinclude -Isrc
SRC_DIR = src
BIN_DIR = bin
INCLUDE_DIR = include

TARGETS = $(BIN_DIR)/search $(BIN_DIR)/compress
OBJS = $(BIN_DIR)/HybridFileHandle.o $(BIN_DIR)/IndexFile.o $(BIN_DIR)/utils.o

all: $(TARGETS)

$(BIN_DIR)/search: $(SRC_DIR)/search.cpp $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_DIR)/compress: $(SRC_DIR)/compress.cpp
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
