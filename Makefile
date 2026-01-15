BINARY = hash
CC = gcc
CFLAGS = -Wall -Wextra -g -O0

all: $(BINARY)
	@$(info Compiling and creating the binary)

$(BINARY): hash.c main.c
	@$(CC) -o $@ $^

clean:
	@rm -rf $(BINARY)

diff:
	@$(info The status of the repository, and the volume of per-file changes:)
	@git status
	@git diff --stat

.PHONY: all deepclean clean diff distribute

-include $(DEPFILES)
