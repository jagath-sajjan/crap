#include "colorspace.h"

#define FP_SHIFT 16
#define FP_HALF  (1 << (FP_SHIFT - 1))

#define COEF_YR   19595 
#define COEF_YG   38470   
#define COEF_YB   7471    
#define COEF_CBR  11056   
#define COEF_CBG  21712   
#define COEF_CBB  32768   
#define COEF_CRR  32768   
#define COEF_CRG  27440   
#define COEF_CRB  5328    

#define COEF_RCR  91881 
#define COEF_GCB  22554   
#define COEF_GCR  46802   
#define COEF_BCB 116130 

static inline uint8_t clamp_u8(int v) {
    v &= ~(v >> 31);
    return (uint8_t)(v | ((255 - v) >> 31));
}

static inline void rgb_to_ycbcr_pixel(uint8_t r, uint8_t g, uint8_t b,
                                       uint8_t *y, uint8_t *cb, uint8_t *cr) {
    *y  = clamp_u8((int)(( COEF_YR  * r + COEF_YG  * g + COEF_YB  * b + FP_HALF) >> FP_SHIFT));
    *cb = clamp_u8(128 + (int)((-COEF_CBR * r - COEF_CBG * g + COEF_CBB * b + FP_HALF) >> FP_SHIFT));
    *cr = clamp_u8(128 + (int)(( COEF_CRR * r - COEF_CRG * g - COEF_CRB * b + FP_HALF) >> FP_SHIFT));
}

static inline void ycbcr_to_rgb_pixel(uint8_t y, uint8_t cb, uint8_t cr,
                                       uint8_t *r, uint8_t *g, uint8_t *b) {
    int yy  = (int)y << FP_SHIFT;
    int pbr = ((int)cr - 128) * COEF_RCR;
    int pbg = ((int)cb - 128) * COEF_GCB + ((int)cr - 128) * COEF_GCR;
    int pbb = ((int)cb - 128) * COEF_BCB;
    *r = clamp_u8((yy + pbr + FP_HALF) >> FP_SHIFT);
    *g = clamp_u8((yy - pbg + FP_HALF) >> FP_SHIFT);
    *b = clamp_u8((yy + pbb + FP_HALF) >> FP_SHIFT);
}

static int do_rgb_to_yuv(const uint8_t *src, int bpp, CrapFrame *dst,
                          uint32_t width, uint32_t height) {
    uint8_t *yp  = dst->plane[0].data;
    uint8_t *cbp = dst->plane[1].data;
    uint8_t *crp = dst->plane[2].data;
    uint32_t y_stride  = dst->plane[0].stride;
    uint32_t cb_stride = dst->plane[1].stride;
    uint32_t cr_stride = dst->plane[2].stride;
    CrapPixFmt fmt = dst->format;

    if (fmt != PIX_FMT_YUV444P &&
        fmt != PIX_FMT_YUV422P &&
        fmt != PIX_FMT_YUV420P) return -1;

    for (uint32_t row = 0; row < height; row++) {
        const uint8_t *srow = src + row * width * (uint32_t)bpp;
        uint8_t *yrow = yp + row * y_stride;
        for (uint32_t col = 0; col < width; col++) {
            uint8_t r = srow[col * (uint32_t)bpp + 0];
            uint8_t g = srow[col * (uint32_t)bpp + 1];
            uint8_t b = srow[col * (uint32_t)bpp + 2];
            uint8_t yv, cbv, crv;
            rgb_to_ycbcr_pixel(r, g, b, &yv, &cbv, &crv);
            yrow[col] = yv;
            if (fmt == PIX_FMT_YUV444P) {
                (cbp + row * cb_stride)[col] = cbv;
                (crp + row * cr_stride)[col] = crv;
            } else if (fmt == PIX_FMT_YUV422P) {
                if (col % 2 == 0) {
                    uint32_t cx = col / 2;
                    (cbp + row * cb_stride)[cx] = cbv;
                    (crp + row * cr_stride)[cx] = crv;
                }
            } else {
                if (col % 2 == 0 && row % 2 == 0) {
                    uint32_t cx = col / 2;
                    uint32_t cy = row / 2;
                    (cbp + cy * cb_stride)[cx] = cbv;
                    (crp + cy * cr_stride)[cx] = crv;
                }
            }
        }
    }
    return 0;
}

int rgb24_to_yuv(const uint8_t *src, CrapFrame *dst,
                 uint32_t width, uint32_t height) {
    return do_rgb_to_yuv(src, 3, dst, width, height);
}

int rgba32_to_yuv(const uint8_t *src, CrapFrame *dst,
                  uint32_t width, uint32_t height) {
    return do_rgb_to_yuv(src, 4, dst, width, height);
}

int yuv_to_rgb24(const CrapFrame *src, uint8_t *dst,
                 uint32_t width, uint32_t height) {
    const uint8_t *yp  = src->plane[0].data;
    const uint8_t *cbp = src->plane[1].data;
    const uint8_t *crp = src->plane[2].data;
    uint32_t y_stride  = src->plane[0].stride;
    uint32_t cb_stride = src->plane[1].stride;
    uint32_t cr_stride = src->plane[2].stride;
    CrapPixFmt fmt = src->format;

    if (fmt != PIX_FMT_YUV444P &&
        fmt != PIX_FMT_YUV422P &&
        fmt != PIX_FMT_YUV420P) return -1;

    for (uint32_t row = 0; row < height; row++) {
        const uint8_t *yrow = yp + row * y_stride;
        uint8_t *drow = dst + row * width * 3;
        for (uint32_t col = 0; col < width; col++) {
            uint8_t yv = yrow[col];
            uint8_t cbv, crv;
            if (fmt == PIX_FMT_YUV444P) {
                cbv = (cbp + row * cb_stride)[col];
                crv = (crp + row * cr_stride)[col];
            } else if (fmt == PIX_FMT_YUV422P) {
                uint32_t cx = col / 2;
                cbv = (cbp + row * cb_stride)[cx];
                crv = (crp + row * cr_stride)[cx];
            } else {
                uint32_t cx = col / 2;
                uint32_t cy = row / 2;
                cbv = (cbp + cy * cb_stride)[cx];
                crv = (crp + cy * cr_stride)[cx];
            }
            uint8_t r, g, b;
            ycbcr_to_rgb_pixel(yv, cbv, crv, &r, &g, &b);
            drow[col * 3 + 0] = r;
            drow[col * 3 + 1] = g;
            drow[col * 3 + 2] = b;
        }
    }
    return 0;
}
