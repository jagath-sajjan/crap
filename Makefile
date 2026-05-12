CC =  clang
CFLAGS = -std=c11 -02 -Wall -Wextra -Wpendantic -Iinclude
SRC  = src/container.c src/utils.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: libcrap.a

libcrap: $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) libcrap.a

