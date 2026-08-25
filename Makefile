CC      = clang
OBJC    = clang
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
OBJCFLAGS = -fobjc-arc -O2 -Wall -Iinclude \
            -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk

SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null || echo "-I/opt/homebrew/include/SDL2 -I/opt/homebrew/opt/sdl2-compat/include")
SDL_LIBS   ?= $(shell pkg-config --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null || echo "-L/opt/homebrew/lib -L/opt/homebrew/opt/sdl2-compat/lib -lSDL2")

FRAMEWORKS = -framework AVFoundation \
             -framework CoreMedia    \
             -framework CoreVideo    \
             -framework Foundation   \
             -framework CoreAudio    \
             -framework AudioToolbox

SRC = src/container.c src/bitstream.c src/utils.c \
      src/frame.c src/pool.c src/framequeue.c \
      src/colorspace.c src/dct.c src/quant.c \
      src/entropy.c src/audio.c \
      encoder/encoder.c \
      decoder/decoder.c
OBJ = $(SRC:.c=.o)

TEST_BINS = test_frame test_bitstream test_colorspace test_dct test_quant test_container test_e2e

.PHONY: all clean tools tests test run_tests

all: libcrap.a tools tests

libcrap.a: $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tools: craprec crapplay crappr

craprec: tools/craprec.m capture/capture.m $(OBJ)
	$(OBJC) $(OBJCFLAGS) \
		tools/craprec.m capture/capture.m $(OBJ) \
		$(FRAMEWORKS) \
		-o craprec

crapplay: player/player.c $(OBJ)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) \
		player/player.c $(OBJ) \
		$(SDL_LIBS) \
		-o crapplay

crappr: tools/crappr.m $(OBJ)
	$(OBJC) $(OBJCFLAGS) \
		tools/crappr.m $(OBJ) \
		$(FRAMEWORKS) -framework Cocoa -framework QuartzCore \
		-o crappr

test_frame: tests/test_frame.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_frame.c $(OBJ) -o test_frame

test_bitstream: tests/test_bitstream.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_bitstream.c $(OBJ) -o test_bitstream

test_colorspace: tests/test_colorspace.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_colorspace.c $(OBJ) -o test_colorspace

test_dct: tests/test_dct.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_dct.c $(OBJ) -o test_dct

test_quant: tests/test_quant.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_quant.c $(OBJ) -o test_quant

test_container: tests/test_container.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_container.c $(OBJ) -o test_container

test_e2e: tests/test_e2e.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_e2e.c $(OBJ) -o test_e2e

tests: $(TEST_BINS)

test: tests
	./test_frame
	./test_bitstream
	./test_colorspace
	./test_dct
	./test_quant
	./test_container
	./test_e2e

run_tests: test

clean:
	rm -f $(OBJ) libcrap.a \
		craprec crapplay crappr \
		$(TEST_BINS) \
		test_e2e_asan
	rm -rf *.dSYM
