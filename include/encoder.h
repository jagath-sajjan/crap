#ifndef ENCODER_H
#define ENCODER_H

#include "container.h"
#include "frame.h"
#include "quant.h"
#include <stdint.h>


/*
 * encoder context
 * owns the output CrapContext and quantization tables
 * not thread safe ~ one encoder per file
 */
typedef struct {
    CrapContext ctx;
    uint16_t    qtable_luma[64];
    uint16_t    qtable_chroma[64];
    int         quality;
    uint64_t    frame_index;
    uint8_t     stream_id;
} CrapEncoder;

/*
 * open a .crap file for writing
 * quality: 1..100
 * width/height: frame dimensions (must be multiples of 8)
 * fps_num/fps_den: framerate as rational
 */
int  encoder_open(CrapEncoder *enc, const char *path,
                  uint32_t width, uint32_t height,
                  uint8_t fps_num, uint8_t fps_den,
                  int quality);

/*
 * encode one iframe from a packed RGB24 buffer
 * rgb: width*height*3 bytes, row major
 * pts: presentation timestamp in microseconds
 */
int  encoder_write_iframe_rgb(CrapEncoder *enc,
                              const uint8_t *rgb,
                              uint32_t width, uint32_t height,
                              int64_t pts);

/*
 * finalise and close the file
 * writes updated header with correct offsets and CRC
 */
int  encoder_close(CrapEncoder *enc);


#endif /* ENCODER_H */
