CXX = g++
LIBS = 
LFLAGS = 
CXXFLAGS = -std=c++11 -Wall -Dhome -O3  # -DDEBUG # -g  -DNDEBUG 

SRCS=$(wildcard *.cc)

OBJS=$(SRCS:.cc=.o)

BINS=${OBJS:.o=}

all: $(BINS)

clean:
	rm -f $(BINS)