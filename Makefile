CC      = clang
OBJC    = clang
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
OBJCFLAGS = -fobjc-arc -O2 -Wall -Iinclude \
            -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
LDFLAGS_COMMON = -L/opt/homebrew/opt/sdl2-compat/lib \
                 -I/opt/homebrew/opt/sdl2-compat/include \
                 -lSDL2
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

.PHONY: all clean tools tests

all: libcrap.a tools

libcrap.a: $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tools: craprec crapplay

craprec: tools/craprec.m capture/capture.m $(OBJ)
	$(OBJC) $(OBJCFLAGS) -Iinclude \
		tools/craprec.m capture/capture.m $(OBJ) \
		$(FRAMEWORKS) \
		-o craprec

crapplay: player/player.c $(OBJ)
	$(CC) $(CFLAGS) \
		-I/opt/homebrew/opt/sdl2-compat/include \
		player/player.c $(OBJ) \
		$(LDFLAGS_COMMON) \
		-o crapplay

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

test_e2e: tests/test_e2e.c $(OBJ)
	$(CC) $(CFLAGS) tests/test_e2e.c $(OBJ) -o test_e2e

clean:
	rm -f $(OBJ) libcrap.a \
		craprec crapplay \
		test_frame test_container test_bitstream \
		test_colorspace test_dct test_quant test_e2e \
		test_e2e_asan
	rm -rf *.dSYM
