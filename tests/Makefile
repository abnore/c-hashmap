CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -O2 -I..

HASHSRC = ../hash.c
TESTS   = test heavy_test bench

all: $(TESTS)

test: test_hash.c $(HASHSRC)
	$(CC) $(CFLAGS) $^ -o $@

heavy_test: heavy_test.c $(HASHSRC)
	$(CC) $(CFLAGS) $^ -o $@

bench: bench_hash.c $(HASHSRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TESTS)

.PHONY: all clean
