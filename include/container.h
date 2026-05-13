#ifndef CONTAINER_H
#define CONTAINER_H

#include "crap.h"
#include <stdio.h>

// CHUNK IDs
#define CHUNK_METADATA   0x4D455441UL /* "META" */
#define CHUNK_STREAMINFO 0x53494E46UL /* "SINF" */
#define CHUNK_FRAMEDATA  0x46524D44UL /* "FRMD" */
#define CHUNK_FRAMEINDEX 0x46524958UL /* "FRIX" */
#define CHUNK_EOF        0x454F4643UL /* "EOFC" */

// On disk structs (packed, explicit padding)

/*
 * File header > first bytes of every .crap file
 * Fixed size, no chunk wrapper. CRC32 covers bytes 0..27
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;           // CRAP_MAGIC
    uint8_t  version_major;
    uint8_t  version_minor;
    uint16_t flags;           // reserved, must be 0
    uint8_t  stream_count;    // total number of streams
    uint8_t  _pad[3];
    int64_t  duration_us;     // total duration in microseconds
    uint64_t metadata_offset; // byte offset to META chunk
    uint64_t index_offset;    // byte offset to FRIX chunk
    uint32_t header_crc32;    // CRC32 of bytes [0..27]
} CrapFileHeader;             // 40 bytes total

/*
 * Generic chunk header, precedes every chunk payload
 */
typedef struct __attribute__((packed)) {
    uint32_t id;   // chunk type ID
    uint64_t size; // payload size in bytes
} CrapChunkHeader; // 12 bytes

/*
 * Stream info payload, one per stream inside SINF chunk
 */
typedef struct __attribute__((packed)) {
    uint8_t  stream_id;      // unique per file, 0-based
    uint8_t  stream_type;    // CrapStreamType
    uint16_t codec_id;       // codec identifier

    uint32_t time_base_num;  // timebase numerator
    uint32_t time_base_den;  // timebase denominator

    /* video specific */
    uint16_t width;
    uint16_t height;
    uint8_t  fps_num;
    uint8_t  fps_den;

    /* audio specific */
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bit_depth;
    uint8_t  _pad[2];
} CrapStreamInfo; // 28 bytes

/*
 * Frame data chunk payload header
 * Actual encoded bytes follow immediately
 */
typedef struct __attribute__((packed)) {
    uint8_t  stream_id;
    uint8_t  frame_type; // CrapFrameType
    uint8_t  _pad[2];

    int64_t  pts;        // presentation timestamp
    int64_t  dts;        // decode timestamp

    uint32_t data_size;  // encoded payload bytes that follow
} CrapFrameHeader; // 24 bytes

/*
 * One entry in the frame index (FRIX chunk)
 * FRIX payload = array of this + uint32_t entry_count prefix
 */
typedef struct __attribute__((packed)) {
    int64_t  pts;
    uint64_t byte_offset; // absolute offset of FRMD chunk

    uint8_t  stream_id;
    uint8_t  frame_type;
    uint8_t  _pad[2];
} CrapIndexEntry; // 20 bytes

// In-memory context
#define CRAP_MAX_STREAMS 16

typedef struct {
    FILE           *fp;

    CrapFileHeader  header;
    CrapStreamInfo  streams[CRAP_MAX_STREAMS];

    CrapIndexEntry *index; // heap allocated array
    uint32_t        index_count;
} CrapContext;

// API
int  crap_open_read (CrapContext *ctx, const char *path);
int  crap_open_write(CrapContext *ctx, const char *path);
void crap_close     (CrapContext *ctx);

int  crap_read_frame (
    CrapContext     *ctx,
    CrapFrameHeader *fh,
    uint8_t         *buf,
    uint32_t         buf_size
);

int  crap_write_frame(
    CrapContext           *ctx,
    const CrapFrameHeader *fh,
    const uint8_t         *data
);

int  crap_seek_pts(
    CrapContext *ctx,
    uint8_t      stream_id,
    int64_t      pts
);

#endif // CONTAINER_H
