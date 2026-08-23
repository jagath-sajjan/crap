#include <stdio.h>
#include <stdlib.h>

extern int capture_run(const char *path, int quality,
                       unsigned int width, unsigned int height,
                       int duration_sec);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: craprec <output.crap> [seconds=10] [quality=85]\n");
        return 1;
    }
    const char *path  = argv[1];
    int duration      = argc > 2 ? atoi(argv[2]) : 10;
    int quality       = argc > 3 ? atoi(argv[3]) : 85;

    return capture_run(path, quality, 640, 480, duration);
}
