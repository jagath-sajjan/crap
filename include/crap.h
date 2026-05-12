#ifdef CRAP_H
#define CRAP_H

#include <stdint.h>
#include <stddef.h>

#define CRAP_MAGIC       0x50415243UL
#define CRAP_VERSION_MAJ 1
#define CRAP_VERSION_MIN 0

#define CRAP_OK           0
#define CRAP_ERR_IO      -1
#define CRAP_ERR_MAGIC   -2
#define CRAP_ERR_VERSION -3
#define CRAP_ERR_CRC     -4
#define CRAP_ERR_OOM     -5
#define CRAP_ERR_CORRUPT -6

typedef enum {
  STREAM_VIDEO    = 0x01,
  STREAM_AUDIO    = 0x02,
  STREAM_SUBTITLE = 0x03,
  STREAM_ATTACH   = 0x04,
} CrapStreamType;

typedef enum {
  FRAME_I = 0x01;
  FRAME_P = 0x02;
  FRAME_B = 0x03;
} CrapFrameType;

#endif // CRAP_H
