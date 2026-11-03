/* host_camera.h on macOS: AVFoundation, 640x480 BGRA frames on a serial queue. */
#import <AVFoundation/AVFoundation.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "host_camera.h"

static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static uint8_t *g_frame;
static int g_w, g_h;
static bool g_have;
static const char *g_status = "未启动";

@interface InkCameraSink : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@end

@implementation InkCameraSink
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sb fromConnection:(AVCaptureConnection *)c {
    CVImageBufferRef img = CMSampleBufferGetImageBuffer(sb);
    if (!img) return;
    CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
    int w = (int)CVPixelBufferGetWidth(img), h = (int)CVPixelBufferGetHeight(img);
    size_t stride = CVPixelBufferGetBytesPerRow(img);
    const uint8_t *base = CVPixelBufferGetBaseAddress(img);
    pthread_mutex_lock(&g_m);
    if (w != g_w || h != g_h) {
        free(g_frame);
        g_frame = malloc((size_t)w * (size_t)h * 4);
        g_w = g_frame ? w : 0;
        g_h = g_frame ? h : 0;
    }
    if (g_frame && base) {
        for (int y = 0; y < h; y++) memcpy(g_frame + (size_t)y * (size_t)w * 4, base + (size_t)y * stride, (size_t)w * 4);
        g_have = true;
    }
    pthread_mutex_unlock(&g_m);
    CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
}
@end

/* Session and status change only on one serial control queue; a generation number makes a late
 * permission answer or a start that races a close do nothing. */
static AVCaptureSession *g_session;
static InkCameraSink *g_sink;
static dispatch_queue_t g_frames, g_ctl;
static unsigned g_gen;

static dispatch_queue_t ctl(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ g_ctl = dispatch_queue_create("ink.camera.control", DISPATCH_QUEUE_SERIAL); });
    return g_ctl;
}

static void start_session(unsigned gen) { /* on the control queue */
    if (gen != g_gen || g_session) return;
    AVCaptureDevice *dev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!dev) {
        g_status = "这台电脑没有摄像头（用测试图案）";
        return;
    }
    NSError *err = nil;
    AVCaptureDeviceInput *in = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
    if (!in) {
        g_status = "摄像头打不开（用测试图案）";
        return;
    }
    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    if ([session canSetSessionPreset:AVCaptureSessionPreset640x480]) session.sessionPreset = AVCaptureSessionPreset640x480;
    if ([session canAddInput:in]) [session addInput:in];
    AVCaptureVideoDataOutput *out = [[AVCaptureVideoDataOutput alloc] init];
    out.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
    out.alwaysDiscardsLateVideoFrames = YES;
    g_sink = [[InkCameraSink alloc] init];
    if (!g_frames) g_frames = dispatch_queue_create("ink.camera.frames", DISPATCH_QUEUE_SERIAL);
    [out setSampleBufferDelegate:g_sink queue:g_frames];
    if ([session canAddOutput:out]) [session addOutput:out];
    [session startRunning];
    g_session = session;
    g_status = "电脑摄像头工作中";
}

void host_camera_open(void) {
    dispatch_async(ctl(), ^{
      unsigned gen = ++g_gen;
      switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
      case AVAuthorizationStatusAuthorized: start_session(gen); break;
      case AVAuthorizationStatusNotDetermined:
          g_status = "等待授权使用摄像头";
          [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                   completionHandler:^(BOOL granted) {
                                     dispatch_async(ctl(), ^{
                                       if (gen != g_gen) return; /* closed or reopened meanwhile */
                                       if (granted) start_session(gen);
                                       else g_status = "没有摄像头权限（系统设置 → 隐私与安全性 → 摄像头）";
                                     });
                                   }];
          break;
      default: g_status = "没有摄像头权限（系统设置 → 隐私与安全性 → 摄像头）"; break;
      }
    });
}

bool host_camera_frame(uint8_t *bgra, size_t cap, int *w, int *h) {
    pthread_mutex_lock(&g_m);
    bool ok = g_have && (size_t)g_w * (size_t)g_h * 4 <= cap;
    if (ok) {
        memcpy(bgra, g_frame, (size_t)g_w * (size_t)g_h * 4);
        *w = g_w;
        *h = g_h;
    }
    pthread_mutex_unlock(&g_m);
    return ok;
}

void host_camera_close(void) {
    dispatch_sync(ctl(), ^{
      g_gen++; /* a pending permission answer starts nothing */
      if (g_session) [g_session stopRunning];
      g_session = nil;
      g_sink = nil;
      g_status = "未启动";
    });
    if (g_frames) dispatch_sync(g_frames, ^{}); /* the last frame callback has finished */
    pthread_mutex_lock(&g_m);
    free(g_frame);
    g_frame = NULL;
    g_w = g_h = 0;
    g_have = false;
    pthread_mutex_unlock(&g_m);
}

const char *host_camera_status(void) { return g_status; }
