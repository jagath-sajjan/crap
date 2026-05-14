# CRAPPPPPPPPPPPPPPPP

## STAGE 3 P1 TEST

```bash
crap on  main took 2s
❯ make clean && make test_bitstream && ./test_bitstream

rm -f src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o libcrap.a test_frame test_container

clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/container.c -o src/container.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/bitstream.c -o src/bitstream.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/utils.c -o src/utils.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/frame.c -o src/frame.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/pool.c -o src/pool.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/framequeue.c -o src/framequeue.o

clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude \
tests/test_bitstream.c \
src/container.o \
src/bitstream.o \
src/utils.o \
src/frame.o \
src/pool.o \
src/framequeue.o \
-o test_bitstream

[PASS] single bits
[PASS] multi bits
[PASS] byte aligned
[PASS] u32 roundtrip
[PASS] align
[PASS] overflow guard

Bitstream tests passed.
```
