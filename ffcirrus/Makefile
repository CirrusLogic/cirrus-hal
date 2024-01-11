# Compiler
# CC=gcc
# CC=arm-linux-gnueabihf-gcc
CC=aarch64-linux-gnu-gcc

# Compiler flags:
# -I. use current directory
# -Wall turns on most, but not all, compiler warnings
# -lm link with standard C math library
CFLAGS=-I. -Wall -lm

# Build Dependencies
DEPS = ffcirrus.h owt.h owt.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

pre-build:
	make -C ./../owt/
	mv ./../owt/owt.o .

ffcirrus: ffcirrus.o owt.o main.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *~ *.sh