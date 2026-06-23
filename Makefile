CC      = clang
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
SRC     = src/container.c src/bitstream.c src/utils.c \
          src/frame.c src/pool.c src/framequeue.c \
          src/colorspace.c src/dct.c \
		  src/quant.c
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

test_bitstream: tests/test_bitstream.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_bitstream.c $(OBJ) -o test_bitstream

test_colorspace: tests/test_colorspace.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_colorspace.c $(OBJ) -o test_colorspace

test_dct: tests/test_dct.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_dct.c $(OBJ) -o test_dct test_quant

test_quant: tests/test_quant.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_quant.c $(OBJ) -o test_quant

clean:
	rm -f $(OBJ) libcrap.a test_frame test_container test_bitstream test_colorspace test_dct test_quant
