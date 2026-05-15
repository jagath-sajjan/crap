# CRAPPPPPPPPPPPPPPPP

## STAGE 3 P2 TEST

```bash
crap on  main [!?] took 38s
❯ make clean && make test_colorspace && ./test_colorspace

rm -f src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o libcrap.a test_frame test_container
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/container.c -o src/container.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/bitstream.c -o src/bitstream.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/utils.c -o src/utils.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/frame.c -o src/frame.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/pool.c -o src/pool.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/framequeue.c -o src/framequeue.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/colorspace.c -o src/colorspace.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/test_colorspace.c src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o -o test_colorspace
YUV444 max roundtrip error: 1

[PASS] YUV444P roundtrip
[PASS] YUV420P chroma dims
[PASS] YUV422P chroma dims
[PASS] pure color BT.601 full-swing values
[PASS] RGBA alpha ignored

Colorspace tests passed.
```
