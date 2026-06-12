# CRAPPPPPPPPPPPPPPPP

## STAGE 4 DCT + IDCT TEST

```bash
crap on  main [!?]
❯ make clean && make test_dct && ./test_dct

rm -f src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o src/dct.o libcrap.a test_frame test_container test_bitstream test_colorspace test_dct
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/container.c -o src/container.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/bitstream.c -o src/bitstream.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/utils.c -o src/utils.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/frame.c -o src/frame.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/pool.c -o src/pool.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/framequeue.c -o src/framequeue.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/colorspace.c -o src/colorspace.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/dct.c -o src/dct.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/test_dct.c src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o src/dct.o -o test_dct

DCT/IDCT max roundtrip error: 2

[PASS] DCT/IDCT roundtrip
[PASS] DCT zero block
[PASS] ZigZag bijection
[PASS] DCT energy compaction

DCT tests passed yoo.
```
