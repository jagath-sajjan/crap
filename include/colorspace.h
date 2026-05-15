#ifndef COLORSPACE_H
#define COLORSPACE_H

#include "frame.h"
#include <stdint.h>

int rgb24_to_yuv  (const uint8_t *src, CrapFrame *dst, uint32_t width, uint32_t height);
int rgba32_to_yuv (const uint8_t *src, CrapFrame *dst, uint32_t width, uint32_t height);
int yuv_to_rgb24  (const CrapFrame *src, uint8_t *dst, uint32_t width, uint32_t height);

#endif /* COLORSPACE_H */
