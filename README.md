# CRAPPPPPPPPPPPPPPPP

## STAGE 5 QUANT TEST

```bash
ccrap on  main [!?]
❯ make clean && make test_quant && ./test_quant

rm -f src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o src/dct.o src/quant.o libcrap.a test_frame test_container test_bitstream test_colorspace test_dct test_quant

clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/container.c -o src/container.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/bitstream.c -o src/bitstream.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/utils.c -o src/utils.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/frame.c -o src/frame.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/pool.c -o src/pool.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/framequeue.c -o src/framequeue.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/colorspace.c -o src/colorspace.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/dct.c -o src/dct.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude -c src/quant.c -o src/quant.o
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/test_quant.c src/container.o src/bitstream.o src/utils.o src/frame.o src/pool.o src/framequeue.o src/colorspace.o src/dct.o src/quant.o -o test_quant

[PASS] quant table quality extremes
 quant q95 max pixel error: 0
[PASS] quant encode decode q95
  zero AC coeffs at q5: 63/63
[PASS] quant zeroes AC at low quality
[PASS] zigzag order preserved through encode decode

Quantization tests passed.
```
