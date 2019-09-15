# Compiler options
CXX = g++
CXXFLAGS = --std=c++11 -Wall -g -pthread

# Source code files, object files and the final binary name.
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
BIN = binary

.PHONY: clean

all: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(OBJS)

clean:
	$(RM) $(OBJS) $(BIN)
