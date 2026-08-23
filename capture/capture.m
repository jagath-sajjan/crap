#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "encoder.h"
#include "audio.h"

static volatile sig_atomic_t g_stop = 0;
static void handle_sig(int sig) {
    (void)sig;
    g_stop = 1;
}

@interface CrapCaptureDelegate : NSObject
    <AVCaptureVideoDataOutputSampleBufferDelegate,
     AVCaptureAudioDataOutputSampleBufferDelegate>
@property (nonatomic, assign) CrapEncoder *enc;
@property (nonatomic, assign) int          running;
@property (nonatomic, assign) int64_t      start_us;
@property (nonatomic, strong) dispatch_queue_t encode_q;
@end

@implementation CrapCaptureDelegate

- (int64_t)nowUs {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    (void)connection;
    if (!self.running) return;
    int64_t pts = [self nowUs] - self.start_us;

    if ([output isKindOfClass:[AVCaptureVideoDataOutput class]]) {
        CVImageBufferRef img = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!img) return;

        CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        size_t w      = CVPixelBufferGetWidth(img);
        size_t h      = CVPixelBufferGetHeight(img);
        size_t stride = CVPixelBufferGetBytesPerRow(img);
        uint8_t *base = (uint8_t *)CVPixelBufferGetBaseAddress(img);

        uint8_t *rgb = malloc(w * h * 3);
        if (rgb) {
            for (size_t row = 0; row < h; row++) {
                uint8_t *src = base + row * stride;
                uint8_t *dst = rgb  + row * w * 3;
                for (size_t col = 0; col < w; col++) {
                    dst[col*3+0] = src[col*4+2];
                    dst[col*3+1] = src[col*4+1];
                    dst[col*3+2] = src[col*4+0];
                }
            }
        }
        CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);

        if (!rgb) return;

        CrapEncoder *enc = self.enc;
        uint32_t fw = (uint32_t)w, fh2 = (uint32_t)h;
        int64_t fpts = pts;

        dispatch_async(self.encode_q, ^{
            encoder_write_iframe_rgb(enc, rgb, fw, fh2, fpts);
            free(rgb);
        });

    } else if ([output isKindOfClass:[AVCaptureAudioDataOutput class]]) {
        CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sampleBuffer);
        if (!block) return;
        size_t   length = 0;
        uint8_t *data   = NULL;
        CMBlockBufferGetDataPointer(block, 0, NULL, &length, (char **)&data);
        if (!data || length == 0) return;

        CMAudioFormatDescriptionRef fmtDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
        const AudioStreamBasicDescription *asbd = fmtDesc ? CMAudioFormatDescriptionGetStreamBasicDescription(fmtDesc) : NULL;

        int16_t *abuf = NULL;
        size_t sample_count = 0;

        if (asbd && (asbd->mFormatFlags & kAudioFormatFlagIsFloat)) {
            int num_floats = (int)(length / sizeof(float));
            const float *fsrc = (const float *)data;
            if (asbd->mChannelsPerFrame == 1) {
                abuf = malloc(num_floats * 2 * sizeof(int16_t));
                if (abuf) {
                    for (int i = 0; i < num_floats; i++) {
                        float s = fsrc[i];
                        if (s > 1.0f) s = 1.0f;
                        if (s < -1.0f) s = -1.0f;
                        int16_t v = (int16_t)(s * 32767.0f);
                        abuf[i * 2 + 0] = v;
                        abuf[i * 2 + 1] = v;
                    }
                    sample_count = (size_t)num_floats * 2;
                }
            } else {
                abuf = malloc(num_floats * sizeof(int16_t));
                if (abuf) {
                    for (int i = 0; i < num_floats; i++) {
                        float s = fsrc[i];
                        if (s > 1.0f) s = 1.0f;
                        if (s < -1.0f) s = -1.0f;
                        abuf[i] = (int16_t)(s * 32767.0f);
                    }
                    sample_count = (size_t)num_floats;
                }
            }
        } else if (asbd && asbd->mChannelsPerFrame == 1 && asbd->mBitsPerChannel == 16) {
            int num_s16 = (int)(length / sizeof(int16_t));
            const int16_t *isrc = (const int16_t *)data;
            abuf = malloc(num_s16 * 2 * sizeof(int16_t));
            if (abuf) {
                for (int i = 0; i < num_s16; i++) {
                    abuf[i * 2 + 0] = isrc[i];
                    abuf[i * 2 + 1] = isrc[i];
                }
                sample_count = (size_t)num_s16 * 2;
            }
        } else {
            abuf = malloc(length);
            if (abuf) {
                memcpy(abuf, data, length);
                sample_count = length / sizeof(int16_t);
            }
        }

        if (abuf && sample_count > 0) {
            CrapEncoder *enc = self.enc;
            int64_t apts = pts;
            dispatch_async(self.encode_q, ^{
                audio_write_frame(&enc->ctx, abuf, (uint32_t)sample_count, apts);
                free(abuf);
            });
        } else if (abuf) {
            free(abuf);
        }
    }
}
@end

int capture_run(const char *output_path, int quality,
                uint32_t width, uint32_t height,
                int duration_sec) {
    /* request video permission */
    dispatch_semaphore_t vsem = dispatch_semaphore_create(0);
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                           completionHandler:^(BOOL g) {
        (void)g; dispatch_semaphore_signal(vsem);
    }];
    dispatch_semaphore_wait(vsem, DISPATCH_TIME_FOREVER);

    dispatch_semaphore_t asem = dispatch_semaphore_create(0);
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                           completionHandler:^(BOOL g) {
        (void)g; dispatch_semaphore_signal(asem);
    }];
    dispatch_semaphore_wait(asem, DISPATCH_TIME_FOREVER);

    AVCaptureDevice *vdev = [AVCaptureDevice
        defaultDeviceWithMediaType:AVMediaTypeVideo];
    AVCaptureDevice *adev = [AVCaptureDevice
        defaultDeviceWithMediaType:AVMediaTypeAudio];

    if (!vdev) { fprintf(stderr, "no camera\n"); return -1; }
    if (!adev) { fprintf(stderr, "no mic\n");    return -1; }

    NSError *err = nil;
    AVCaptureDeviceInput *vin =
        [AVCaptureDeviceInput deviceInputWithDevice:vdev error:&err];
    AVCaptureDeviceInput *ain =
        [AVCaptureDeviceInput deviceInputWithDevice:adev error:&err];

    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    if ([session canSetSessionPreset:AVCaptureSessionPreset640x480]) {
        session.sessionPreset = AVCaptureSessionPreset640x480;
    } else if ([session canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
        session.sessionPreset = AVCaptureSessionPreset1280x720;
    } else if ([session canSetSessionPreset:AVCaptureSessionPresetHigh]) {
        session.sessionPreset = AVCaptureSessionPresetHigh;
    }

    if ([session canAddInput:vin]) [session addInput:vin];
    if ([session canAddInput:ain]) [session addInput:ain];

    AVCaptureVideoDataOutput *vout = [[AVCaptureVideoDataOutput alloc] init];
    vout.videoSettings = @{
        (NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (NSString *)kCVPixelBufferWidthKey: @(width),
        (NSString *)kCVPixelBufferHeightKey: @(height),
    };
    vout.alwaysDiscardsLateVideoFrames = YES;

    AVCaptureAudioDataOutput *aout = [[AVCaptureAudioDataOutput alloc] init];

    dispatch_queue_t vq = dispatch_queue_create("crap.vcap",
        dispatch_queue_attr_make_with_qos_class(
            DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
    dispatch_queue_t aq = dispatch_queue_create("crap.acap",
        dispatch_queue_attr_make_with_qos_class(
            DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));

    CrapEncoder enc;
    int ret = encoder_open(&enc, output_path, width, height, 30, 1, quality);
    if (ret != CRAP_OK) {
        fprintf(stderr, "encoder_open failed: %d\n", ret);
        return ret;
    }

    CrapStreamInfo asi;
    memset(&asi, 0, sizeof(asi));
    asi.stream_id    = 1;
    asi.stream_type  = STREAM_AUDIO;
    asi.codec_id     = 0x0002;
    asi.sample_rate  = 44100;
    asi.channels     = 2;
    asi.bit_depth    = 16;
    asi.time_base_num = 1;
    asi.time_base_den = 1000000;
    int aret = crap_write_streaminfo(&enc.ctx, &asi);
    if (aret != CRAP_OK) {
        fprintf(stderr, "crap_write_streaminfo failed: %d\n", aret);
        return aret;
    }

    CrapCaptureDelegate * __strong delegate =
        [[CrapCaptureDelegate alloc] init];
    delegate.enc      = &enc;
    delegate.running  = 0;
    delegate.encode_q = dispatch_queue_create("crap.encode",
        dispatch_queue_attr_make_with_qos_class(
            DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0));

    [vout setSampleBufferDelegate:delegate queue:vq];
    [aout setSampleBufferDelegate:delegate queue:aq];

    if ([session canAddOutput:vout]) [session addOutput:vout];
    if ([session canAddOutput:aout]) [session addOutput:aout];

    [session startRunning];

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    delegate.start_us = (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
    delegate.running  = 1;

    g_stop = 0;
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    fprintf(stderr, "Recording %ds to %s (640x480 q%d)...\n",
            duration_sec, output_path, quality);

    for (int i = 0; i < duration_sec * 10 && !g_stop; i++) {
        [NSThread sleepForTimeInterval:0.1];
    }

    delegate.running = 0;
    [session stopRunning];

    dispatch_sync(delegate.encode_q, ^{});

    encoder_close(&enc);
    fprintf(stderr, "Done.\n");
    return 0;
}
