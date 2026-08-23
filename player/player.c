#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#include "container.h"
#include "audio.h"
#include "bitstream.h"
#include "entropy.h"
#include "dct.h"
#include "quant.h"
#include "colorspace.h"

#define AUDIO_BUF_BYTES (44100 * 2 * 2 * 4)

static uint8_t   audio_ring[AUDIO_BUF_BYTES];
static uint32_t  audio_rpos = 0;
static uint32_t  audio_wpos = 0;
static SDL_mutex *audio_mtx;

static void audio_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    SDL_LockMutex(audio_mtx);
    uint32_t avail = (audio_wpos - audio_rpos + (uint32_t)AUDIO_BUF_BYTES)
                     % AUDIO_BUF_BYTES;
    uint32_t copy  = (uint32_t)len < avail ? (uint32_t)len : avail;
    for (uint32_t i = 0; i < copy; i++)
        stream[i] = audio_ring[(audio_rpos + i) % AUDIO_BUF_BYTES];
    if (copy < (uint32_t)len) memset(stream + copy, 0, (size_t)(len - (int)copy));
    audio_rpos = (audio_rpos + copy) % AUDIO_BUF_BYTES;
    SDL_UnlockMutex(audio_mtx);
}

static void push_audio(const uint8_t *data, uint32_t bytes) {
    SDL_LockMutex(audio_mtx);
    for (uint32_t i = 0; i < bytes; i++) {
        audio_ring[audio_wpos] = data[i];
        audio_wpos = (audio_wpos + 1) % AUDIO_BUF_BYTES;
    }
    SDL_UnlockMutex(audio_mtx);
}

static void decode_plane(BSReader *r, uint8_t *plane, uint32_t stride,
                         uint32_t mb_w, uint32_t mb_h,
                         const uint16_t qtable[64]) {
    for (uint32_t by = 0; by < mb_h; by++) {
        for (uint32_t bx = 0; bx < mb_w; bx++) {
            int16_t block[64];
            if (entropy_decode_block(r, block) != 0) return;
            quant_decode(block, qtable);
            idct8x8(block);
            uint8_t *dst = plane + by * 8 * stride + bx * 8;
            for (int row = 0; row < 8; row++)
                for (int col = 0; col < 8; col++) {
                    int v = block[row*8+col] + 128;
                    if (v <   0) v = 0;
                    if (v > 255) v = 255;
                    dst[row * stride + col] = (uint8_t)v;
                }
        }
    }
}

int player_run(const char *path, int quality) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    CrapDecoder dec;
    int ret = decoder_open(&dec, path, quality);
    if (ret != CRAP_OK) {
        fprintf(stderr, "decoder_open failed: %d\n", ret);
        SDL_Quit();
        return -1;
    }

    uint32_t W = dec.width, H = dec.height;
    fprintf(stderr, "Playing %ux%u\n", W, H);

    SDL_Window *win = SDL_CreateWindow("crapplay",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)W, (int)H, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *tex = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
        (int)W, (int)H);

    audio_mtx = SDL_CreateMutex();
    SDL_AudioSpec want = {0}, got;
    want.freq     = AUDIO_SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples  = AUDIO_FRAME_SAMPLES;
    want.callback = audio_callback;
    SDL_AudioDeviceID adev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (adev) SDL_PauseAudioDevice(adev, 0);

    /* pre-allocate all frame buffers — no malloc in hot loop */
    uint32_t scratch_size = W * H * 4 + 1024 * 1024;
    uint8_t *scratch  = malloc(scratch_size);
    uint8_t *yuv_buf  = malloc(frame_buffer_size(PIX_FMT_YUV420P, W, H));
    uint8_t *rgb_buf  = malloc((size_t)W * H * 3);

    if (!scratch || !yuv_buf || !rgb_buf) {
        fprintf(stderr, "OOM allocating frame buffers\n");
        return -1;
    }

    CrapFrame yf;
    frame_init(&yf);
    frame_attach_buffer(&yf, PIX_FMT_YUV420P, W, H, yuv_buf);

    uint32_t mb_w   = (W    + 7) / 8;
    uint32_t mb_h   = (H    + 7) / 8;
    uint32_t mb_w_c = (W/2  + 7) / 8;
    uint32_t mb_h_c = (H/2  + 7) / 8;

    int64_t start_wall = 0;
    int64_t start_pts  = -1;
    int running = 1;
    SDL_Event ev;

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT ||
               (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.sym == SDLK_ESCAPE))
                running = 0;
        }

        CrapFrameHeader fh;
        int ret2 = crap_read_frame(&dec.ctx, &fh, scratch, scratch_size);
        if (ret2 != CRAP_OK) break;

        if (fh.stream_id == 0 || (fh.stream_id < CRAP_MAX_STREAMS && dec.ctx.streams[fh.stream_id].stream_type == STREAM_VIDEO)) {
            if (start_pts < 0) {
                start_pts  = fh.pts;
                start_wall = (int64_t)SDL_GetTicks64() * 1000LL;
            }

            BSReader r;
            bsr_init(&r, scratch, fh.data_size);

            decode_plane(&r, yf.plane[0].data, yf.plane[0].stride,
                         mb_w, mb_h, dec.qtable_luma);
            decode_plane(&r, yf.plane[1].data, yf.plane[1].stride,
                         mb_w_c, mb_h_c, dec.qtable_chroma);
            decode_plane(&r, yf.plane[2].data, yf.plane[2].stride,
                         mb_w_c, mb_h_c, dec.qtable_chroma);

            yuv_to_rgb24(&yf, rgb_buf, W, H);

            int64_t now_us = (int64_t)SDL_GetTicks64() * 1000LL;
            int64_t target_us = start_wall + (fh.pts - start_pts);
            int64_t wait = target_us - now_us;
            if (wait > 1000 && wait < 10000000LL)
                SDL_Delay((uint32_t)(wait / 1000));

            void *pixels; int pitch;
            if (SDL_LockTexture(tex, NULL, &pixels, &pitch) == 0) {
                for (uint32_t row = 0; row < H; row++)
                    memcpy((uint8_t*)pixels + row*(uint32_t)pitch,
                           rgb_buf + row*W*3, W*3);
                SDL_UnlockTexture(tex);
            }
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);

        } else if ((fh.stream_id == 1 || (fh.stream_id < CRAP_MAX_STREAMS && dec.ctx.streams[fh.stream_id].stream_type == STREAM_AUDIO)) && adev) {
            push_audio(scratch, fh.data_size);
        }
    }

    if (adev) {
        SDL_LockMutex(audio_mtx);
        uint32_t avail = (audio_wpos - audio_rpos + (uint32_t)AUDIO_BUF_BYTES) % AUDIO_BUF_BYTES;
        SDL_UnlockMutex(audio_mtx);
        if (avail > 0) {
            uint32_t rem_ms = (avail * 1000) / (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS / 8));
            if (rem_ms > 0 && rem_ms < 5000) SDL_Delay(rem_ms);
        }
    }

    free(scratch); free(yuv_buf); free(rgb_buf);
    if (adev) SDL_CloseAudioDevice(adev);
    SDL_DestroyMutex(audio_mtx);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    decoder_close(&dec);
    SDL_Quit();
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: crapplay <input.crap> [quality=85]\n");
        return 1;
    }
    int quality = argc > 2 ? atoi(argv[2]) : 85;
    return player_run(argv[1], quality);
}
