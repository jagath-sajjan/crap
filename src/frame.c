#include "frame.h"
#include <string.h>

void frame_init(CrapFrame *f) {
  memset(f, 0, sizeof(*f));
  f->ref[0] = -1;
  f->ref[1] = -1;
}

void frame_attach_buffer(CrapFrame *f, CrapPixFmt fmt, uint32_t w, uint32_t h, uint8_t *buf) {
  f->format = fmt;
  f->width  = w;
  f->height = h;

  int n = pixfmt_plane_count (fmt);
  uint8_t *ptr = buf;

  if (n == 1) {
    // packed format
    f->plane[0].data   = ptr;
    f->plane[0].width  = w;
    f->plane[0].height = h;
    f->plane[0].stride = w * (uint32_t)pixfmt_bytes_per_pixel(fmt);
  } else {
    // planar YUV
    for (int i = 0; i < n; i++) {
      uint32_t pw, ph;
      pixfmt_plane_dims(fmt, w, h, i, &pw, &ph);
      f->plane[i].data   = ptr;
      f->plane[i].width  = pw;
      f->plane[i].height = ph;
      f->plane[i].stride = pw; // 1 byte per sample
      ptr += (size_t)pw * ph;
    }
  }
}
