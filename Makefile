# MMSP 2025 Final Project Makefile

CC = gcc
CFLAGS = -Wall -O2
LIBS = -lm

TARGETS = encoder decoder

all: $(TARGETS)

encoder: encoder.c bmp.h
	$(CC) $(CFLAGS) -o encoder encoder.c $(LIBS)

decoder: decoder.c bmp.h
	$(CC) $(CFLAGS) -o decoder decoder.c $(LIBS)

clean:
	rm -f $(TARGETS) *.o *.txt *.raw Res*.bmp Rec*.bmp psnr.txt

.PHONY: all clean