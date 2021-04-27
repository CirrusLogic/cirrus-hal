# Compiler
# CC=gcc
CC=arm-linux-gnueabihf-gcc

# Compiler flags:
# -I. use current directory
# -Wall turns on most, but not all, compiler warnings
CFLAGS=-I. -Wall -lm

# Build Dependencies
DEPS = owt.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

owt: main.o owt.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *~ *.sh
