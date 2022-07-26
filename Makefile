CC = gcc

all: bin/sort bin/tests

CFLAGS = -g3 -Wall -Wextra -Wpedantic -Wconversion -std=c89 -pedantic

INCLUDES = src/*.h

COMMON_DEPS = $(INCLUDES) Makefile

build/%.o: src/%.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f build/* bin/*