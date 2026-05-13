#ifndef FRAME_H
#define FRAME_H

#include "crap.h"
#include <stdint.h>
#include <stddef.h>

// pixel formats
typedef enum {
  PIX_FMT_NONE    = 0, 
  PIX_FMT_RGB24   = 1,
  PIX_FMT_RGBA32  = 2, 
  PIX_FMT_YUV420P = 3, 
  PIX_FMT_YUV422P = 4, 
  PIX_FMT_YUV444P = 5,
} CrapPixFmt;

/*
 * plane descriptor one per channel
 * for packed formats (RGB, RBGA), only plane[0] is used
 * for planar YUV, plane[0] = Y, plane[1]=Cb, plane[2]=Cr
*/

typedef struct {
  uint8_t  *data;   // pointer into pool block or raw buffer
  uint32_t width;  // plane width in pixel
  uint32_t height; // plane height in pixel
  uint32_t stride; // bytes per row
} CrapPlane;

/*
 * in mem frame, owns no memory itself, data pointers
 * refrence pool blocks or externally managed buffers 
 * caller is responsible for lifetime
*/

typedef struct {
  CrapPixFmt format;
  uint32_t   width;  //coded width
  uint32_t   height; // coded height
  
  CrapPlane plane[3]; // up to 3 planes
  
  int64_t  pts; // display timestamp
  int64_t  dts; // decode timestamp 
  uint64_t frame_number; // display order index
  uint64_t decode_order; // encoded order index 

  CrapFrameType frame_type; // I, P, B
  
  /*
   * dependancy tracking for P/B frames 
   * ref[0] = backward ref frame_no (P & B)
   * ref[1] = forward ref frame_no (B only) 
   * -1 = unused
  */ 
  int64_t ref[2];

  uint8_t stream_id;
  uint8_t keyframe; // 1 if independantly decodeable
  uint8_t _pad[2];
} CrapFrame;

// Pixel format helpers

/*
 * returns the no of planes for a given format
*/
static inline int pixfmt_plane_count(CrapPixFmt fmt) {
  switch (fmt) {
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:  return 1;
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUV444P: return 3;
    default:              return 0;
  }
}

/*
 * returns bytes per pixel for packed formats; 0 for planar 
 * (planar formats compute plane sizes individually)
*/

static inline int pixfmt_bytes_per_pixel(CrapPixFmt fmt) {
  switch (fmt) {
    case PIX_FMT_RGB24:  return 3;
    case PIX_FMT_RGBA32: return 4;
    default:             return 0;
  }
}

/*
 * compute plane diamensions for a given format and plane index 
 * fills out *pw & *ph with the plane's width & height 
*/

static inline void pixfmt_plane_dims(CrapPixFmt fmt, uint32_t frame_w, uint32_t frame_h, int plane_idx, uint32_t *pw, uint32_t *ph ) {
  switch (fmt) {
    case PIX_FMT_YUV420P:
      *pw = (plane_idx == 0) ? frame_w: (frame_w + 1) / 2;
      *ph = (plane_idx == 0) ? frame_h: (frame_h + 1) / 2;
      break;
    case PIX_FMT_YUV422P:
      *pw = (plane_idx == 0) ? frame_w : (frame_w + 1) / 2;
      *ph = frame_h;
      break;
    case PIX_FMT_YUV444P:
      *pw = frame_w;
      *ph = frame_h;
      break;
    case PIX_FMT_RGB24:
    case PIX_FMT_RGBA32:
      *pw = frame_w;
      *ph = frame_h;
      break;
    default:
      *pw = 0; *ph = 0;
      break;
  }
}

/*
 * total bytes needed to store one frame of given format 
 * Stride = width (pool handles alignment)
*/

static inline size_t frame_buffer_size(CrapPixFmt fmt, uint32_t w, uint32_t h) {
  switch (fmt) {
    case PIX_FMT_RGB24:   return (size_t)w * h * 3;
    case PIX_FMT_RGBA32:  return (size_t)w * h * 4;
    case PIX_FMT_YUV420P: return (size_t)w * h + 2 * ((w+1)/2) * ((h+1)/2);
    case PIX_FMT_YUV422P: return (size_t)w * h + 2 * ((w+1)/2) * h;
    case PIX_FMT_YUV444P: return (size_t)w * h * 3;
    default:              return 0;
  }
}

/*
 * attach a flat buffer to a CrapFrame, setting up plane pointers
 * & strides correctly for the given format
 * buf must be at least frame_buffer_size(fmt, w, h) bytes
*/

void frame_attach_buffer(CrapFrame *f, CrapPixFmt fmt, uint32_t w, uint32_t h, uint8_t *buf);

/*
 * zero initialise a CrapFrame struct, does not free or touch plane data 
*/

void frame_init(CrapFrame *f);

#endif // FRAME_H
