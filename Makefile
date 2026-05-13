CC      = clang
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
SRC     = src/container.c src/bitstream.c src/utils.c \
          src/frame.c src/pool.c src/framequeue.c
OBJ     = $(SRC:.c=.o)

.PHONY: all clean test

all: libcrap.a

libcrap.a: $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test_frame: tests/test_frame.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_frame.c $(OBJ) -o test_frame

test_container: tests/test_container.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_container.c $(OBJ) -o test_container

clean:
	rm -f $(OBJ) libcrap.a test_frame test_container
