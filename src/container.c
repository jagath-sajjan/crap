#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crap.h"
#include "container.h"

// CRC32 [IEEE 802.3]

static uint32_t crc32_table[256];
static int      crc32_ready = 0;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_ready = 1;
}

static uint32_t crc32(const uint8_t *data, size_t len) {
    if (!crc32_ready) crc32_init();
    uint32_t c = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFUL;
}

// Internal helpers

static int read_exact(FILE *fp, void *buf, size_t n) {
    return fread(buf, 1, n, fp) == n ? CRAP_OK : CRAP_ERR_IO;
}

static int write_exact(FILE *fp, const void *buf, size_t n) {
    return fwrite(buf, 1, n, fp) == n ? CRAP_OK : CRAP_ERR_IO;
}

static int write_chunk(FILE *fp, uint32_t id,
                       const void *payload, uint64_t size) {
    CrapChunkHeader hdr = { .id = id, .size = size };
    if (write_exact(fp, &hdr, sizeof(hdr)) != CRAP_OK) return CRAP_ERR_IO;
    if (size > 0)
        if (write_exact(fp, payload, (size_t)size) != CRAP_OK) return CRAP_ERR_IO;
    uint32_t crc = crc32((const uint8_t *)payload, (size_t)size);
    if (write_exact(fp, &crc, 4) != CRAP_OK) return CRAP_ERR_IO;
    return CRAP_OK;
}

static int read_chunk_header(FILE *fp, CrapChunkHeader *hdr) {
    return read_exact(fp, hdr, sizeof(*hdr));
}

// Public API

int crap_open_read(CrapContext *ctx, const char *path) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) return CRAP_ERR_IO;

    if (read_exact(ctx->fp, &ctx->header, sizeof(ctx->header)) != CRAP_OK)
        goto fail_io;

    if (ctx->header.magic != CRAP_MAGIC) {
        fclose(ctx->fp); return CRAP_ERR_MAGIC;
    }

    {
        uint32_t computed = crc32((const uint8_t *)&ctx->header,
                                   sizeof(ctx->header) - sizeof(uint32_t));
        if (computed != ctx->header.header_crc32) {
            fclose(ctx->fp); return CRAP_ERR_CRC;
        }
    }

    fseeko(ctx->fp, sizeof(CrapFileHeader), SEEK_SET);
    for (int i = 0; i < ctx->header.stream_count; i++) {
        CrapChunkHeader chdr;
        if (read_chunk_header(ctx->fp, &chdr) != CRAP_OK) goto fail_io;
        if (chdr.id != CHUNK_STREAMINFO) goto fail_corrupt;

        CrapStreamInfo si;
        if (read_exact(ctx->fp, &si, sizeof(si)) != CRAP_OK) goto fail_io;

        uint32_t s_stored = 0;
        uint32_t s_computed = crc32((const uint8_t *)&si, sizeof(si));
        if (read_exact(ctx->fp, &s_stored, 4) != CRAP_OK) goto fail_io;
        if (s_stored != s_computed) { fclose(ctx->fp); return CRAP_ERR_CRC; }

        if (si.stream_id < CRAP_MAX_STREAMS)
            ctx->streams[si.stream_id] = si;
    }

    if (ctx->header.index_offset == 0) return CRAP_OK;

    fseeko(ctx->fp, (off_t)ctx->header.index_offset, SEEK_SET);

    {
        CrapChunkHeader ichdr;
        if (read_chunk_header(ctx->fp, &ichdr) != CRAP_OK) goto fail_io;
        if (ichdr.id != CHUNK_FRAMEINDEX) goto fail_corrupt;

        uint32_t entry_count = 0;
        if (read_exact(ctx->fp, &entry_count, 4) != CRAP_OK) goto fail_io;

        size_t index_bytes = entry_count * sizeof(CrapIndexEntry);
        ctx->index = malloc(index_bytes);
        if (!ctx->index) { fclose(ctx->fp); return CRAP_ERR_OOM; }

        if (read_exact(ctx->fp, ctx->index, index_bytes) != CRAP_OK) {
            free(ctx->index); goto fail_io;
        }

        uint8_t *crc_buf = malloc(4 + index_bytes);
        if (!crc_buf) { free(ctx->index); fclose(ctx->fp); return CRAP_ERR_OOM; }
        memcpy(crc_buf, &entry_count, 4);
        memcpy(crc_buf + 4, ctx->index, index_bytes);
        uint32_t i_computed = crc32(crc_buf, 4 + index_bytes);
        free(crc_buf);

        uint32_t i_stored = 0;
        if (read_exact(ctx->fp, &i_stored, 4) != CRAP_OK) {
            free(ctx->index); goto fail_io;
        }
        if (i_stored != i_computed) {
            free(ctx->index); fclose(ctx->fp); return CRAP_ERR_CRC;
        }

        ctx->index_count = entry_count;
    }

    return CRAP_OK;

fail_io:      fclose(ctx->fp); return CRAP_ERR_IO;
fail_corrupt: fclose(ctx->fp); return CRAP_ERR_CORRUPT;
}

int crap_open_write(CrapContext *ctx, const char *path) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->fp = fopen(path, "wb");
    if (!ctx->fp) return CRAP_ERR_IO;

    CrapFileHeader hdr = {
        .magic         = CRAP_MAGIC,
        .version_major = CRAP_VERSION_MAJ,
        .version_minor = CRAP_VERSION_MIN,
        .flags         = 0,
    };
    ctx->header = hdr;
    if (write_exact(ctx->fp, &hdr, sizeof(hdr)) != CRAP_OK) {
        fclose(ctx->fp); return CRAP_ERR_IO;
    }
    return CRAP_OK;
}

void crap_close(CrapContext *ctx) {
    if (ctx->fp)    fclose(ctx->fp);
    if (ctx->index) free(ctx->index);
    memset(ctx, 0, sizeof(*ctx));
}

int crap_write_frame(CrapContext *ctx, const CrapFrameHeader *fh,
                     const uint8_t *data) {
    size_t payload_size = sizeof(CrapFrameHeader) + fh->data_size;
    uint8_t *payload = malloc(payload_size);
    if (!payload) return CRAP_ERR_OOM;
    memcpy(payload, fh, sizeof(CrapFrameHeader));
    memcpy(payload + sizeof(CrapFrameHeader), data, fh->data_size);
    int ret = write_chunk(ctx->fp, CHUNK_FRAMEDATA, payload,
                          (uint64_t)payload_size);
    free(payload);
    return ret;
}

int crap_read_frame(CrapContext *ctx, CrapFrameHeader *fh,
                    uint8_t *buf, uint32_t buf_size) {
    CrapChunkHeader chdr;
    if (read_chunk_header(ctx->fp, &chdr) != CRAP_OK) return CRAP_ERR_IO;
    if (chdr.id != CHUNK_FRAMEDATA) return CRAP_ERR_CORRUPT;

    if (read_exact(ctx->fp, fh, sizeof(*fh)) != CRAP_OK) return CRAP_ERR_IO;
    if (fh->data_size > buf_size) return CRAP_ERR_CORRUPT;
    if (read_exact(ctx->fp, buf, fh->data_size) != CRAP_OK) return CRAP_ERR_IO;

    size_t payload_size = sizeof(CrapFrameHeader) + fh->data_size;
    uint8_t *tmp = malloc(payload_size);
    if (!tmp) return CRAP_ERR_OOM;
    memcpy(tmp, fh, sizeof(CrapFrameHeader));
    memcpy(tmp + sizeof(CrapFrameHeader), buf, fh->data_size);
    uint32_t computed = crc32(tmp, payload_size);
    free(tmp);

    uint32_t stored = 0;
    if (read_exact(ctx->fp, &stored, 4) != CRAP_OK) return CRAP_ERR_IO;
    if (stored != computed) return CRAP_ERR_CRC;

    return CRAP_OK;
}

int crap_seek_pts(CrapContext *ctx, uint8_t stream_id, int64_t pts) {
    if (!ctx->index || ctx->index_count == 0) return CRAP_ERR_CORRUPT;

    int lo = 0, hi = (int)ctx->index_count - 1, best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        CrapIndexEntry *e = &ctx->index[mid];
        if (e->stream_id != stream_id) { lo = mid + 1; continue; }
        if (e->pts <= pts && e->frame_type == FRAME_I) {
            best = mid; lo = mid + 1;
        } else if (e->pts > pts) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    if (best < 0) return CRAP_ERR_CORRUPT;
    fseeko(ctx->fp, (off_t)ctx->index[best].byte_offset, SEEK_SET);
    return CRAP_OK;
}
