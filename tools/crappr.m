#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <AudioToolbox/AudioToolbox.h>
#import <QuartzCore/QuartzCore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "encoder.h"
#include "decoder.h"
#include "audio.h"
#include "colorspace.h"
#include "entropy.h"
#include "dct.h"
#include "quant.h"

#define AUDIO_RING_BYTES (48000 * 2 * 2 * 4) // ~4 seconds buffer
#define NUM_AQ_BUFFERS 3
#define AQ_BUF_SIZE 4096

@interface AudioOutputEngine : NSObject
@property (nonatomic, readonly) BOOL isRunning;
- (instancetype)initWithSampleRate:(double)rate channels:(UInt32)channels;
- (void)playSamples:(const void *)data bytes:(UInt32)bytes;
- (void)stop;
@end

@implementation AudioOutputEngine {
    AudioQueueRef _queue;
    AudioQueueBufferRef _buffers[NUM_AQ_BUFFERS];
    uint8_t _ringBuf[AUDIO_RING_BYTES];
    uint32_t _rpos;
    uint32_t _wpos;
    pthread_mutex_t _mutex;
    BOOL _running;
}

static void AQOutputCallback(void *userData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    AudioOutputEngine *engine = (__bridge AudioOutputEngine *)userData;
    [engine fillBuffer:inBuffer queue:inAQ];
}

- (instancetype)initWithSampleRate:(double)rate channels:(UInt32)channels {
    self = [super init];
    if (self) {
        pthread_mutex_init(&_mutex, NULL);
        _rpos = 0;
        _wpos = 0;

        if (rate <= 0) rate = 44100.0;
        if (channels == 0) channels = 2;

        AudioStreamBasicDescription asbd;
        memset(&asbd, 0, sizeof(asbd));
        asbd.mFormatID         = kAudioFormatLinearPCM;
        asbd.mSampleRate       = rate;
        asbd.mChannelsPerFrame = channels;
        asbd.mBitsPerChannel   = 16;
        asbd.mBytesPerFrame    = channels * 2;
        asbd.mFramesPerPacket  = 1;
        asbd.mBytesPerPacket   = channels * 2;
        asbd.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

        OSStatus st = AudioQueueNewOutput(&asbd, AQOutputCallback, (__bridge void *)self, NULL, NULL, 0, &_queue);
        if (st == noErr) {
            _running = YES;
            for (int i = 0; i < NUM_AQ_BUFFERS; i++) {
                if (AudioQueueAllocateBuffer(_queue, AQ_BUF_SIZE, &_buffers[i]) == noErr) {
                    memset(_buffers[i]->mAudioData, 0, AQ_BUF_SIZE);
                    _buffers[i]->mAudioDataByteSize = AQ_BUF_SIZE;
                    AudioQueueEnqueueBuffer(_queue, _buffers[i], 0, NULL);
                }
            }
            AudioQueueStart(_queue, NULL);
        }
    }
    return self;
}

- (BOOL)isRunning {
    return _running;
}

- (void)fillBuffer:(AudioQueueBufferRef)buf queue:(AudioQueueRef)queue {
    pthread_mutex_lock(&_mutex);
    uint32_t avail = (_wpos - _rpos + AUDIO_RING_BYTES) % AUDIO_RING_BYTES;
    uint32_t to_copy = buf->mAudioDataBytesCapacity < avail ? buf->mAudioDataBytesCapacity : avail;

    for (uint32_t i = 0; i < to_copy; i++) {
        ((uint8_t *)buf->mAudioData)[i] = _ringBuf[(_rpos + i) % AUDIO_RING_BYTES];
    }
    if (to_copy < buf->mAudioDataBytesCapacity) {
        memset((uint8_t *)buf->mAudioData + to_copy, 0, buf->mAudioDataBytesCapacity - to_copy);
    }
    _rpos = (_rpos + to_copy) % AUDIO_RING_BYTES;
    buf->mAudioDataByteSize = buf->mAudioDataBytesCapacity;
    pthread_mutex_unlock(&_mutex);

    if (_running && queue) {
        AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
    }
}

- (void)playSamples:(const void *)data bytes:(UInt32)bytes {
    if (!_running || bytes == 0 || !data) return;
    pthread_mutex_lock(&_mutex);
    const uint8_t *src = (const uint8_t *)data;
    for (UInt32 i = 0; i < bytes; i++) {
        _ringBuf[_wpos] = src[i];
        _wpos = (_wpos + 1) % AUDIO_RING_BYTES;
    }
    pthread_mutex_unlock(&_mutex);
}

- (void)stop {
    if (_running) {
        _running = NO;
        if (_queue) {
            AudioQueueStop(_queue, YES);
            AudioQueueDispose(_queue, YES);
            _queue = NULL;
        }
    }
}

- (void)dealloc {
    [self stop];
    pthread_mutex_destroy(&_mutex);
}
@end

@interface VideoCanvasView : NSView
- (void)renderRGB:(const uint8_t *)rgb width:(int)w height:(int)h;
- (void)clear;
- (void)setPlaceholderText:(NSString *)text;
@end

@implementation VideoCanvasView {
    CALayer *_videoLayer;
    CATextLayer *_placeholderLayer;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.06 green:0.07 blue:0.09 alpha:1.0].CGColor;
        self.layer.cornerRadius = 10.0;
        self.layer.masksToBounds = YES;
        self.layer.borderColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.8].CGColor;
        self.layer.borderWidth = 1.0;

        _videoLayer = [CALayer layer];
        _videoLayer.contentsGravity = kCAGravityResizeAspect;
        _videoLayer.frame = self.bounds;
        _videoLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [self.layer addSublayer:_videoLayer];

        _placeholderLayer = [CATextLayer layer];
        _placeholderLayer.string = @"No video signal\n\nClick Record to start camera or select a clip to Play";
        _placeholderLayer.font = (__bridge CFTypeRef)[NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
        _placeholderLayer.fontSize = 14;
        _placeholderLayer.alignmentMode = kCAAlignmentCenter;
        _placeholderLayer.foregroundColor = [NSColor colorWithCalibratedWhite:0.6 alpha:1.0].CGColor;
        _placeholderLayer.contentsScale = [NSScreen mainScreen].backingScaleFactor ?: 2.0;
        _placeholderLayer.frame = CGRectMake(16, (frame.size.height - 60) / 2, frame.size.width - 32, 60);
        [self.layer addSublayer:_placeholderLayer];
    }
    return self;
}

- (void)layout {
    [super layout];
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _videoLayer.frame = self.bounds;
    _placeholderLayer.frame = CGRectMake(16, (self.bounds.size.height - 60) / 2, self.bounds.size.width - 32, 60);
    [CATransaction commit];
}

- (void)renderRGB:(const uint8_t *)rgb width:(int)w height:(int)h {
    if (!rgb || w <= 0 || h <= 0) return;

    size_t bytesPerRow = (size_t)w * 3;
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, rgb, (CFIndex)(bytesPerRow * h));
    if (!data) return;

    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    if (!provider) return;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef cg = CGImageCreate(
        (size_t)w, (size_t)h, 8, 24, bytesPerRow,
        cs, kCGBitmapByteOrderDefault | kCGImageAlphaNone,
        provider, NULL, false, kCGRenderingIntentDefault
    );
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(provider);
    if (!cg) return;

    dispatch_async(dispatch_get_main_queue(), ^{
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        self->_placeholderLayer.hidden = YES;
        self->_videoLayer.contents = (__bridge id)cg;
        [CATransaction commit];
        CGImageRelease(cg);
    });
}

- (void)clear {
    dispatch_async(dispatch_get_main_queue(), ^{
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        self->_videoLayer.contents = nil;
        self->_placeholderLayer.hidden = NO;
        [CATransaction commit];
    });
}

- (void)setPlaceholderText:(NSString *)text {
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_placeholderLayer.string = text;
        self->_placeholderLayer.hidden = NO;
    });
}
@end

@interface CrapprCaptureSession : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate, AVCaptureAudioDataOutputSampleBufferDelegate>
@property (nonatomic, copy) void (^onPreviewFrame)(const uint8_t *rgb, int w, int h);
@property (nonatomic, readonly) BOOL isRecording;
- (void)startRecordingPath:(NSString *)path width:(uint32_t)w height:(uint32_t)h quality:(int)quality completion:(void (^)(BOOL success, NSString *errorMsg))completion;
- (void)stopRecording;
@end

@implementation CrapprCaptureSession {
    AVCaptureSession *_session;
    CrapEncoder _encoder;
    volatile int _running;
    int64_t _startUs;
    dispatch_queue_t _encodeQ;
    dispatch_queue_t _vq;
    dispatch_queue_t _aq;
    BOOL _hasAudio;
    uint32_t _encW;
    uint32_t _encH;
}

- (BOOL)isRecording {
    return _running != 0;
}

- (int64_t)nowUs {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

- (void)startRecordingPath:(NSString *)path width:(uint32_t)w height:(uint32_t)h quality:(int)quality completion:(void (^)(BOOL success, NSString *errorMsg))completion {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        AVAuthorizationStatus vstat = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        if (vstat == AVAuthorizationStatusNotDetermined) {
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL g) {
                (void)g; dispatch_semaphore_signal(sem);
            }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            vstat = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        }

        if (vstat == AVAuthorizationStatusDenied || vstat == AVAuthorizationStatusRestricted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(NO, @"Camera access denied. Enable camera in System Settings.");
            });
            return;
        }

        AVAuthorizationStatus astat = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        if (astat == AVAuthorizationStatusNotDetermined) {
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL g) {
                (void)g; dispatch_semaphore_signal(sem);
            }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        }

        AVCaptureDevice *vdev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (!vdev) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(NO, @"No camera device found.");
            });
            return;
        }

        NSError *err = nil;
        AVCaptureDeviceInput *vin = [AVCaptureDeviceInput deviceInputWithDevice:vdev error:&err];
        if (!vin) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(NO, [NSString stringWithFormat:@"Failed to open camera: %@", err.localizedDescription]);
            });
            return;
        }

        AVCaptureDevice *adev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
        AVCaptureDeviceInput *ain = adev ? [AVCaptureDeviceInput deviceInputWithDevice:adev error:nil] : nil;

        self->_session = [[AVCaptureSession alloc] init];
        if ([self->_session canSetSessionPreset:AVCaptureSessionPreset640x480]) {
            self->_session.sessionPreset = AVCaptureSessionPreset640x480;
        } else if ([self->_session canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
            self->_session.sessionPreset = AVCaptureSessionPreset1280x720;
        }

        if ([self->_session canAddInput:vin]) [self->_session addInput:vin];
        if (ain && [self->_session canAddInput:ain]) [self->_session addInput:ain];

        self->_encW = w;
        self->_encH = h;
        AVCaptureVideoDataOutput *vout = [[AVCaptureVideoDataOutput alloc] init];
        vout.videoSettings = @{
            (NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (NSString *)kCVPixelBufferWidthKey: @(w),
            (NSString *)kCVPixelBufferHeightKey: @(h),
        };
        vout.alwaysDiscardsLateVideoFrames = YES;

        AVCaptureAudioDataOutput *aout = ain ? [[AVCaptureAudioDataOutput alloc] init] : nil;

        self->_vq = dispatch_queue_create("crappr.vcap", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
        self->_aq = dispatch_queue_create("crappr.acap", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
        self->_encodeQ = dispatch_queue_create("crappr.encode", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, 0));

        [vout setSampleBufferDelegate:self queue:self->_vq];
        if (aout) [aout setSampleBufferDelegate:self queue:self->_aq];

        if ([self->_session canAddOutput:vout]) [self->_session addOutput:vout];
        if (aout && [self->_session canAddOutput:aout]) [self->_session addOutput:aout];

        int ret = encoder_open(&self->_encoder, [path UTF8String], w, h, 30, 1, quality);
        if (ret != CRAP_OK) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(NO, [NSString stringWithFormat:@"Failed to create file (%d)", ret]);
            });
            return;
        }

        self->_hasAudio = (ain && aout);
        if (self->_hasAudio) {
            CrapStreamInfo asi;
            memset(&asi, 0, sizeof(asi));
            asi.stream_id     = 1;
            asi.stream_type   = STREAM_AUDIO;
            asi.codec_id      = 0x0002;
            asi.sample_rate   = 44100;
            asi.channels      = 2;
            asi.bit_depth     = 16;
            asi.time_base_num = 1;
            asi.time_base_den = 1000000;
            crap_write_streaminfo(&self->_encoder.ctx, &asi);
        }

        [self->_session startRunning];
        self->_startUs = [self nowUs];
        self->_running = 1;

        dispatch_async(dispatch_get_main_queue(), ^{
            completion(YES, nil);
        });
    });
}

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    (void)connection;
    if (!self->_running) return;
    int64_t pts = [self nowUs] - _startUs;

    if ([output isKindOfClass:[AVCaptureVideoDataOutput class]]) {
        CVImageBufferRef img = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!img) return;

        CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        size_t w = CVPixelBufferGetWidth(img);
        size_t h = CVPixelBufferGetHeight(img);
        size_t stride = CVPixelBufferGetBytesPerRow(img);
        uint8_t *base = (uint8_t *)CVPixelBufferGetBaseAddress(img);

        uint8_t *rgb = malloc(w * h * 3);
        if (rgb) {
            for (size_t row = 0; row < h; row++) {
                uint8_t *src = base + row * stride;
                uint8_t *dst = rgb + row * w * 3;
                for (size_t col = 0; col < w; col++) {
                    dst[col*3+0] = src[col*4+2];
                    dst[col*3+1] = src[col*4+1];
                    dst[col*3+2] = src[col*4+0];
                }
            }
        }
        CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
        if (!rgb) return;

        if (self.onPreviewFrame) {
            self.onPreviewFrame(rgb, (int)w, (int)h);
        }

        uint32_t fw = (uint32_t)w, fh = (uint32_t)h;
        int64_t fpts = pts;
        dispatch_async(_encodeQ, ^{
            if (self->_running) {
                encoder_write_iframe_rgb(&self->_encoder, rgb, fw, fh, fpts);
            }
            free(rgb);
        });

    } else if ([output isKindOfClass:[AVCaptureAudioDataOutput class]] && self->_hasAudio) {
        CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sampleBuffer);
        if (!block) return;
        size_t length = 0;
        uint8_t *data = NULL;
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
            int64_t apts = pts;
            dispatch_async(_encodeQ, ^{
                if (self->_running) {
                    audio_write_frame(&self->_encoder.ctx, abuf, (uint32_t)sample_count, apts);
                }
                free(abuf);
            });
        } else if (abuf) {
            free(abuf);
        }
    }
}

- (void)stopRecording {
    if (!_running) return;
    _running = 0;
    [_session stopRunning];
    dispatch_sync(_encodeQ, ^{});
    encoder_close(&_encoder);
}
@end

@interface CrapprPlaybackEngine : NSObject
@property (nonatomic, copy) void (^onVideoFrame)(const uint8_t *rgb, int w, int h, int64_t pts);
@property (nonatomic, copy) void (^onComplete)(void);
@property (nonatomic, readonly) BOOL isPlaying;
- (BOOL)startPlaybackPath:(NSString *)path quality:(int)quality;
- (void)stopPlayback;
@end

static void decode_plane_cb(BSReader *r, uint8_t *plane, uint32_t stride, uint32_t mb_w, uint32_t mb_h, const uint16_t qtable[64]) {
    for (uint32_t by = 0; by < mb_h; by++) {
        for (uint32_t bx = 0; bx < mb_w; bx++) {
            int16_t block[64];
            if (entropy_decode_block(r, block) != 0) return;
            quant_decode(block, qtable);
            idct8x8(block);
            uint8_t *dst = plane + by * 8 * stride + bx * 8;
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int v = block[row*8+col] + 128;
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    dst[row * stride + col] = (uint8_t)v;
                }
            }
        }
    }
}

@implementation CrapprPlaybackEngine {
    CrapDecoder _decoder;
    AudioOutputEngine *_audioEngine;
    volatile int _playing;
    dispatch_queue_t _playQ;
}

- (BOOL)isPlaying {
    return _playing != 0;
}

- (BOOL)startPlaybackPath:(NSString *)path quality:(int)quality {
    [self stopPlayback];

    int ret = decoder_open(&_decoder, [path UTF8String], quality);
    if (ret != CRAP_OK) return NO;

    uint32_t W = _decoder.width;
    uint32_t H = _decoder.height;
    if (W == 0 || H == 0) {
        decoder_close(&_decoder);
        return NO;
    }

    double audio_rate = 44100.0;
    UInt32 audio_channels = 2;
    for (int i = 0; i < CRAP_MAX_STREAMS; i++) {
        if (_decoder.ctx.streams[i].stream_type == STREAM_AUDIO) {
            if (_decoder.ctx.streams[i].sample_rate > 0)
                audio_rate = (double)_decoder.ctx.streams[i].sample_rate;
            if (_decoder.ctx.streams[i].channels > 0)
                audio_channels = _decoder.ctx.streams[i].channels;
            break;
        }
    }

    _audioEngine = [[AudioOutputEngine alloc] initWithSampleRate:audio_rate channels:audio_channels];
    _playing = 1;
    _playQ = dispatch_queue_create("crappr.play", DISPATCH_QUEUE_SERIAL);

    dispatch_async(_playQ, ^{
        uint32_t scratch_size = W * H * 4 + 1024 * 1024;
        uint8_t *scratch = malloc(scratch_size);
        uint8_t *yuv_buf = malloc(frame_buffer_size(PIX_FMT_YUV420P, W, H));
        uint8_t *rgb_buf = malloc((size_t)W * H * 3);

        CrapFrame yf;
        frame_init(&yf);
        frame_attach_buffer(&yf, PIX_FMT_YUV420P, W, H, yuv_buf);

        uint32_t mb_w = (W + 7) / 8;
        uint32_t mb_h = (H + 7) / 8;
        uint32_t mb_w_c = (W/2 + 7) / 8;
        uint32_t mb_h_c = (H/2 + 7) / 8;

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t start_wall = (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
        int64_t start_pts = -1;

        while (self->_playing) {
            CrapFrameHeader fh;
            int ret2 = crap_read_frame(&self->_decoder.ctx, &fh, scratch, scratch_size);
            if (ret2 != CRAP_OK) break;

            if (fh.stream_id == 0 || (fh.stream_id < CRAP_MAX_STREAMS && self->_decoder.ctx.streams[fh.stream_id].stream_type == STREAM_VIDEO)) {
                if (start_pts < 0) {
                    start_pts = fh.pts;
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    start_wall = (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
                }

                BSReader r;
                bsr_init(&r, scratch, fh.data_size);

                decode_plane_cb(&r, yf.plane[0].data, yf.plane[0].stride, mb_w, mb_h, self->_decoder.qtable_luma);
                decode_plane_cb(&r, yf.plane[1].data, yf.plane[1].stride, mb_w_c, mb_h_c, self->_decoder.qtable_chroma);
                decode_plane_cb(&r, yf.plane[2].data, yf.plane[2].stride, mb_w_c, mb_h_c, self->_decoder.qtable_chroma);

                yuv_to_rgb24(&yf, rgb_buf, W, H);

                clock_gettime(CLOCK_MONOTONIC, &ts);
                int64_t now_us = (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
                int64_t target_us = start_wall + (fh.pts - start_pts);
                int64_t wait = target_us - now_us;
                if (wait > 1000 && wait < 5000000LL) {
                    usleep((useconds_t)wait);
                }

                if (self.onVideoFrame) {
                    self.onVideoFrame(rgb_buf, (int)W, (int)H, fh.pts);
                }

            } else if (fh.stream_id == 1 || (fh.stream_id < CRAP_MAX_STREAMS && self->_decoder.ctx.streams[fh.stream_id].stream_type == STREAM_AUDIO)) {
                [self->_audioEngine playSamples:scratch bytes:fh.data_size];
            }
        }

        free(scratch);
        free(yuv_buf);
        free(rgb_buf);
        [self->_audioEngine stop];
        self->_audioEngine = nil;
        decoder_close(&self->_decoder);

        BOOL completedNaturally = (self->_playing != 0);
        self->_playing = 0;

        if (completedNaturally && self.onComplete) {
            dispatch_async(dispatch_get_main_queue(), ^{
                self.onComplete();
            });
        }
    });

    return YES;
}

- (void)stopPlayback {
    if (_playing) {
        _playing = 0;
        if (_playQ) {
            dispatch_sync(_playQ, ^{});
        }
    }
}
@end

@interface CrapprRecordItem : NSObject
@property (nonatomic, copy) NSString *filename;
@property (nonatomic, copy) NSString *fullPath;
@property (nonatomic, copy) NSString *formattedDate;
@property (nonatomic, copy) NSString *formattedSize;
@property (nonatomic, assign) uint64_t sizeBytes;
@end

@implementation CrapprRecordItem
@end

@interface CrapprWindowController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation CrapprWindowController {
    CrapprCaptureSession *_captureSession;
    CrapprPlaybackEngine *_playbackEngine;
    NSMutableArray<CrapprRecordItem *> *_records;
    NSString *_recordsDir;

    NSTableView *_tableView;
    VideoCanvasView *_videoCanvas;
    NSButton *_recordBtn;
    NSButton *_playBtn;
    NSButton *_deleteBtn;
    NSButton *_revealBtn;
    NSTextField *_statusLabel;
    NSTextField *_countLabel;
    NSTimer *_recTimer;
    int _recElapsedSec;
}

- (instancetype)init {
    NSRect frame = NSMakeRect(120, 120, 1024, 640);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:(NSWindowStyleMaskTitled |
                                                              NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskMiniaturizable |
                                                              NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"crappr — Recording & Playback Studio";
    window.minSize = NSMakeSize(800, 520);
    self = [super initWithWindow:window];
    if (self) {
        _records = [NSMutableArray array];
        _captureSession = [[CrapprCaptureSession alloc] init];
        _playbackEngine = [[CrapprPlaybackEngine alloc] init];

        NSString *home = NSHomeDirectory();
        _recordsDir = [home stringByAppendingPathComponent:@"Movies/crappr"];
        [[NSFileManager defaultManager] createDirectoryAtPath:_recordsDir withIntermediateDirectories:YES attributes:nil error:nil];

        [self setupUI];
        [self refreshRecords];
    }
    return self;
}

- (void)setupUI {
    NSView *content = self.window.contentView;
    content.wantsLayer = YES;

    NSSplitView *split = [[NSSplitView alloc] initWithFrame:content.bounds];
    split.vertical = YES;
    split.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    split.dividerStyle = NSSplitViewDividerStyleThin;

    // LEFT SIDEBAR
    NSView *leftPanel = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 340, content.bounds.size.height)];
    leftPanel.autoresizingMask = NSViewHeightSizable;

    NSView *leftHeader = [[NSView alloc] initWithFrame:NSMakeRect(0, content.bounds.size.height - 48, 340, 48)];
    leftHeader.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;

    NSTextField *histTitle = [NSTextField labelWithString:@"Recordings"];
    histTitle.font = [NSFont systemFontOfSize:15 weight:NSFontWeightBold];
    histTitle.frame = NSMakeRect(14, 14, 150, 22);
    [leftHeader addSubview:histTitle];

    _countLabel = [NSTextField labelWithString:@"0 clips"];
    _countLabel.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightRegular];
    _countLabel.textColor = [NSColor secondaryLabelColor];
    _countLabel.alignment = NSTextAlignmentRight;
    _countLabel.frame = NSMakeRect(180, 14, 144, 20);
    _countLabel.autoresizingMask = NSViewMinXMargin;
    [leftHeader addSubview:_countLabel];

    [leftPanel addSubview:leftHeader];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(12, 54, 316, content.bounds.size.height - 110)];
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;

    _tableView = [[NSTableView alloc] initWithFrame:scroll.bounds];
    _tableView.dataSource = self;
    _tableView.delegate = self;
    _tableView.target = self;
    _tableView.doubleAction = @selector(onTableDoubleClicked:);
    _tableView.rowHeight = 30;
    _tableView.usesAlternatingRowBackgroundColors = YES;
    _tableView.headerView = [[NSTableHeaderView alloc] initWithFrame:NSMakeRect(0, 0, scroll.bounds.size.width, 24)];

    NSTableColumn *colName = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    colName.title = @"File Name";
    colName.width = 150;
    [_tableView addTableColumn:colName];

    NSTableColumn *colSize = [[NSTableColumn alloc] initWithIdentifier:@"size"];
    colSize.title = @"Size";
    colSize.width = 65;
    [_tableView addTableColumn:colSize];

    NSTableColumn *colDate = [[NSTableColumn alloc] initWithIdentifier:@"date"];
    colDate.title = @"Date";
    colDate.width = 100;
    [_tableView addTableColumn:colDate];

    scroll.documentView = _tableView;
    [leftPanel addSubview:scroll];

    NSView *leftFooter = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 340, 50)];
    leftFooter.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;

    _deleteBtn = [NSButton buttonWithTitle:@"Delete" target:self action:@selector(onDeleteClicked:)];
    _deleteBtn.frame = NSMakeRect(12, 10, 85, 30);
    _deleteBtn.bezelStyle = NSBezelStyleRounded;
    [leftFooter addSubview:_deleteBtn];

    _revealBtn = [NSButton buttonWithTitle:@"Reveal" target:self action:@selector(onRevealClicked:)];
    _revealBtn.frame = NSMakeRect(104, 10, 85, 30);
    _revealBtn.bezelStyle = NSBezelStyleRounded;
    [leftFooter addSubview:_revealBtn];

    NSButton *refreshBtn = [NSButton buttonWithTitle:@"Refresh" target:self action:@selector(refreshRecords)];
    refreshBtn.frame = NSMakeRect(196, 10, 85, 30);
    refreshBtn.bezelStyle = NSBezelStyleRounded;
    [leftFooter addSubview:refreshBtn];

    [leftPanel addSubview:leftFooter];

    // RIGHT CANVAS & CONTROLS
    NSView *rightPanel = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, content.bounds.size.width - 340, content.bounds.size.height)];
    rightPanel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _videoCanvas = [[VideoCanvasView alloc] initWithFrame:NSMakeRect(14, 68, rightPanel.bounds.size.width - 28, rightPanel.bounds.size.height - 82)];
    _videoCanvas.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [rightPanel addSubview:_videoCanvas];

    NSView *controlBar = [[NSView alloc] initWithFrame:NSMakeRect(14, 10, rightPanel.bounds.size.width - 28, 48)];
    controlBar.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;

    _recordBtn = [NSButton buttonWithTitle:@"● Record" target:self action:@selector(onRecordToggle:)];
    _recordBtn.frame = NSMakeRect(0, 4, 150, 40);
    _recordBtn.bezelStyle = NSBezelStyleRounded;
    _recordBtn.font = [NSFont systemFontOfSize:13 weight:NSFontWeightBold];
    [controlBar addSubview:_recordBtn];

    _playBtn = [NSButton buttonWithTitle:@"▶ Play" target:self action:@selector(onPlayToggle:)];
    _playBtn.frame = NSMakeRect(160, 4, 130, 40);
    _playBtn.bezelStyle = NSBezelStyleRounded;
    _playBtn.font = [NSFont systemFontOfSize:13 weight:NSFontWeightBold];
    [controlBar addSubview:_playBtn];

    _statusLabel = [NSTextField labelWithString:@"Ready"];
    _statusLabel.frame = NSMakeRect(304, 12, controlBar.bounds.size.width - 304, 24);
    _statusLabel.autoresizingMask = NSViewWidthSizable;
    _statusLabel.textColor = [NSColor labelColor];
    _statusLabel.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    [controlBar addSubview:_statusLabel];

    [rightPanel addSubview:controlBar];

    [split addSubview:leftPanel];
    [split addSubview:rightPanel];
    [split setPosition:340 ofDividerAtIndex:0];
    [content addSubview:split];

    __weak typeof(self) weakSelf = self;
    _captureSession.onPreviewFrame = ^(const uint8_t *rgb, int w, int h) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_videoCanvas renderRGB:rgb width:w height:h];
        }
    };

    _playbackEngine.onVideoFrame = ^(const uint8_t *rgb, int w, int h, int64_t pts) {
        (void)pts;
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_videoCanvas renderRGB:rgb width:w height:h];
        }
    };

    _playbackEngine.onComplete = ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf->_playBtn.title = @"▶ Play";
            strongSelf->_recordBtn.enabled = YES;
            strongSelf->_statusLabel.stringValue = @"Playback finished";
            [strongSelf->_videoCanvas setPlaceholderText:@"Playback finished • Click Play to replay"];
        }
    };
}

- (void)refreshRecords {
    [_records removeAllObjects];
    NSFileManager *fm = [NSFileManager defaultManager];

    NSString *currDir = [fm currentDirectoryPath];
    NSString *currRecDir = [currDir stringByAppendingPathComponent:@"recordings"];
    NSArray<NSString *> *dirs = @[_recordsDir, currRecDir, currDir];
    NSMutableSet *seen = [NSMutableSet set];

    for (NSString *dir in dirs) {
        NSArray *files = [fm contentsOfDirectoryAtPath:dir error:nil];
        for (NSString *f in files) {
            if ([f hasSuffix:@".crap"]) {
                NSString *path = [dir stringByAppendingPathComponent:f];
                if ([seen containsObject:path]) continue;
                [seen addObject:path];

                NSDictionary *attrs = [fm attributesOfItemAtPath:path error:nil];
                CrapprRecordItem *item = [[CrapprRecordItem alloc] init];
                item.filename = f;
                item.fullPath = path;
                item.sizeBytes = [attrs fileSize];

                double kb = (double)item.sizeBytes / 1024.0;
                if (kb > 1024.0) {
                    item.formattedSize = [NSString stringWithFormat:@"%.1f MB", kb / 1024.0];
                } else {
                    item.formattedSize = [NSString stringWithFormat:@"%.0f KB", kb];
                }

                NSDate *date = [attrs fileModificationDate];
                NSDateFormatter *df = [[NSDateFormatter alloc] init];
                [df setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
                item.formattedDate = [df stringFromDate:date];

                [_records addObject:item];
            }
        }
    }

    [_records sortUsingComparator:^NSComparisonResult(CrapprRecordItem *a, CrapprRecordItem *b) {
        return [b.formattedDate compare:a.formattedDate];
    }];

    _countLabel.stringValue = [NSString stringWithFormat:@"%lu clips", (unsigned long)_records.count];
    [_tableView reloadData];

    if (_records.count > 0 && _tableView.selectedRow < 0) {
        [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    (void)tableView;
    return (NSInteger)_records.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    if (row < 0 || row >= (NSInteger)_records.count) return nil;
    CrapprRecordItem *item = _records[(NSUInteger)row];

    NSTextField *tf = [tableView makeViewWithIdentifier:tableColumn.identifier owner:self];
    if (!tf) {
        tf = [NSTextField labelWithString:@""];
        tf.identifier = tableColumn.identifier;
    }

    if ([tableColumn.identifier isEqualToString:@"name"]) {
        tf.stringValue = item.filename;
        tf.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
        tf.textColor = [NSColor labelColor];
    } else if ([tableColumn.identifier isEqualToString:@"size"]) {
        tf.stringValue = item.formattedSize;
        tf.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
        tf.textColor = [NSColor secondaryLabelColor];
    } else if ([tableColumn.identifier isEqualToString:@"date"]) {
        tf.stringValue = item.formattedDate;
        tf.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
        tf.textColor = [NSColor secondaryLabelColor];
    }
    return tf;
}

- (void)onTableDoubleClicked:(id)sender {
    (void)sender;
    NSInteger row = _tableView.selectedRow;
    if (row >= 0 && row < (NSInteger)_records.count) {
        if ([_playbackEngine isPlaying]) {
            [_playbackEngine stopPlayback];
        }
        [self startPlayItem:_records[(NSUInteger)row]];
    }
}

- (void)onRecordToggle:(id)sender {
    (void)sender;
    if (![_captureSession isRecording]) {
        if ([_playbackEngine isPlaying]) {
            [_playbackEngine stopPlayback];
            _playBtn.title = @"▶ Play";
        }

        NSDateFormatter *df = [[NSDateFormatter alloc] init];
        [df setDateFormat:@"yyyy-MM-dd_HH-mm-ss"];
        NSString *fn = [NSString stringWithFormat:@"rec_%@.crap", [df stringFromDate:[NSDate date]]];
        NSString *outPath = [_recordsDir stringByAppendingPathComponent:fn];

        _statusLabel.stringValue = @"Initializing camera...";
        _recordBtn.enabled = NO;

        __weak typeof(self) weakSelf = self;
        [_captureSession startRecordingPath:outPath width:640 height:480 quality:85 completion:^(BOOL success, NSString *errorMsg) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;

            strongSelf->_recordBtn.enabled = YES;
            if (!success) {
                strongSelf->_statusLabel.stringValue = errorMsg ?: @"Camera init failed";
                return;
            }

            strongSelf->_recordBtn.title = @"■ Stop Recording";
            strongSelf->_playBtn.enabled = NO;
            strongSelf->_deleteBtn.enabled = NO;
            strongSelf->_recElapsedSec = 0;
            strongSelf->_statusLabel.stringValue = @"🔴 Recording: 00:00";

            strongSelf->_recTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:strongSelf selector:@selector(onRecTimerTick:) userInfo:nil repeats:YES];
        }];

    } else {
        [_recTimer invalidate];
        _recTimer = nil;
        [_captureSession stopRecording];

        _recordBtn.title = @"● Record";
        _playBtn.enabled = YES;
        _deleteBtn.enabled = YES;
        _statusLabel.stringValue = @"Recording saved";

        [self refreshRecords];
        if (_records.count > 0) {
            [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
        }
    }
}

- (void)onRecTimerTick:(NSTimer *)timer {
    (void)timer;
    _recElapsedSec++;
    int m = _recElapsedSec / 60;
    int s = _recElapsedSec % 60;
    _statusLabel.stringValue = [NSString stringWithFormat:@"🔴 Recording: %02d:%02d", m, s];
}

- (void)startPlayItem:(CrapprRecordItem *)item {
    BOOL ok = [_playbackEngine startPlaybackPath:item.fullPath quality:85];
    if (!ok) {
        _statusLabel.stringValue = [NSString stringWithFormat:@"Failed to open: %@", item.filename];
        return;
    }
    _playBtn.title = @"■ Stop";
    _recordBtn.enabled = NO;
    _statusLabel.stringValue = [NSString stringWithFormat:@"Playing: %@", item.filename];
}

- (void)onPlayToggle:(id)sender {
    (void)sender;
    if ([_playbackEngine isPlaying]) {
        [_playbackEngine stopPlayback];
        [_videoCanvas setPlaceholderText:@"Playback stopped • Click Play to resume"];
        _playBtn.title = @"▶ Play";
        _recordBtn.enabled = YES;
        _statusLabel.stringValue = @"Playback stopped";
    } else {
        NSInteger row = _tableView.selectedRow;
        if (row < 0 || row >= (NSInteger)_records.count) {
            _statusLabel.stringValue = @"No recording selected";
            return;
        }
        [self startPlayItem:_records[(NSUInteger)row]];
    }
}

- (void)onDeleteClicked:(id)sender {
    (void)sender;
    NSInteger row = _tableView.selectedRow;
    if (row < 0 || row >= (NSInteger)_records.count) return;
    CrapprRecordItem *item = _records[(NSUInteger)row];

    if ([_playbackEngine isPlaying]) {
        [_playbackEngine stopPlayback];
        _playBtn.title = @"▶ Play";
        _recordBtn.enabled = YES;
    }

    [[NSFileManager defaultManager] removeItemAtPath:item.fullPath error:nil];
    [self refreshRecords];
    [_videoCanvas clear];
    _statusLabel.stringValue = @"Deleted recording";
}

- (void)onRevealClicked:(id)sender {
    (void)sender;
    NSInteger row = _tableView.selectedRow;
    if (row < 0 || row >= (NSInteger)_records.count) {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:_recordsDir]];
        return;
    }
    CrapprRecordItem *item = _records[(NSUInteger)row];
    [[NSWorkspace sharedWorkspace] selectFile:item.fullPath inFileViewerRootedAtPath:@""];
}
@end

@interface CrapprAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) CrapprWindowController *wc;
@end

@implementation CrapprAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    self.wc = [[CrapprWindowController alloc] init];
    [self.wc showWindow:nil];
    [self.wc.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}
@end

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSMenu *menubar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];

        NSMenu *appMenu = [[NSMenu alloc] init];
        NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit crappr" action:@selector(terminate:) keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];

        CrapprAppDelegate *delegate = [[CrapprAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
