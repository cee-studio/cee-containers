TOP = ../..
CC ?= gcc

SRC   = $(wildcard *.c)
EXES  = $(filter %.out, $(SRC:.c=.out))

SUITE_EXE        = test-cee-json.out
SUITE_DECODE_DIR = test_decode
SUITE_ENCODE_DIR = test_encode

CFLAGS= -fno-exceptions -g -I../ -I$(TOP)/ -I$(TOP)/cee-utils

# generic compilation
%.out: %.c ../json-one.o ../cee.o
	$(CC) $(CFLAGS) -std=c11 -g -o $@ $^

all: $(EXES) $(SUITE_DECODE_DIR) $(SUITE_ENCODE_DIR)
	./$(SUITE_EXE) -v -t expect_decoding -- $(SUITE_DECODE_DIR)/*
	./$(SUITE_EXE) -v -t expect_encoding -- $(SUITE_ENCODE_DIR)/*

clean:
	rm -f *.out

.PHONY : all clean
.ONESHELL :