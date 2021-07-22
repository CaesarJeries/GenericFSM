CC=g++
CFLAGS= -std=c++11 -Werror -Wall -pedantic -Wextra
TARGET=fsm

all: main.cpp
	$(CC) $(CFLAGS) -pthread -o $(TARGET) $^

clean:
	@rm -f $(TARGET) *.log

.PHONY: all clean
