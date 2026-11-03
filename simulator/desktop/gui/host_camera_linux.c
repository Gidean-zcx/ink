/* host_camera.h on Linux: V4L2 /dev/video0, 640x480 YUYV converted to BGRA on a reader thread.
 * Every setup step is checked and undone on failure; frames are converted with the driver's row
 * stride and only from buffers that were mapped, and only whole frames (a short one is skipped). A
 * capture error on the reader thread stops it and says so in the status. */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "host_camera.h"

#define NBUF 4
static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static uint8_t *g_frame;
static int g_w, g_h, g_stride, g_fd = -1, g_nmap;
static bool g_have, g_thread;
static atomic_bool g_run;
static _Atomic(const char *) g_status = "未启动"; /* written by the reader thread too, read by the window */
static void *g_map[NBUF];
static size_t g_len[NBUF];
static pthread_t g_th;

static uint8_t clamp(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static bool whole_frame(size_t len) { return len >= (size_t)(g_h - 1) * (size_t)g_stride + (size_t)g_w * 2; }

static void yuyv_to_bgra(const uint8_t *src, uint8_t *dst, int w, int h, int stride) { /* a whole frame */
    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + (size_t)y * (size_t)stride;
        for (int x = 0; x + 1 < w; x += 2) {
            int y0 = row[2 * x], u = row[2 * x + 1] - 128, y1 = row[2 * x + 2], v = row[2 * x + 3] - 128;
            for (int k = 0; k < 2; k++) {
                int yy = k ? y1 : y0;
                uint8_t *p = dst + 4 * ((size_t)y * (size_t)w + (size_t)(x + k));
                p[0] = clamp(yy + (1814 * u >> 10));
                p[1] = clamp(yy - ((352 * u + 731 * v) >> 10));
                p[2] = clamp(yy + (1436 * v >> 10));
                p[3] = 255;
            }
        }
    }
}

static void stopped(void) { /* reader thread: the device failed */
    pthread_mutex_lock(&g_m);
    g_have = false;
    pthread_mutex_unlock(&g_m);
    atomic_store(&g_status, "摄像头出错，已停止（用测试图案）");
}

static void *reader(void *arg) {
    (void)arg;
    while (atomic_load(&g_run)) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (ioctl(g_fd, VIDIOC_DQBUF, &b) < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                usleep(5000);
                continue;
            }
            stopped();
            break;
        }
        size_t len = b.bytesused ? b.bytesused : ((int)b.index < g_nmap ? g_len[b.index] : 0);
        if ((int)b.index < g_nmap && len <= g_len[b.index] && whole_frame(len)) {
            pthread_mutex_lock(&g_m);
            if (g_frame) {
                yuyv_to_bgra(g_map[b.index], g_frame, g_w, g_h, g_stride);
                g_have = true;
            }
            pthread_mutex_unlock(&g_m);
        }
        while (ioctl(g_fd, VIDIOC_QBUF, &b) < 0) { /* the buffer goes back, or the stream runs dry */
            if (errno == EINTR) continue;
            stopped();
            return NULL;
        }
    }
    return NULL;
}

static void cleanup(void) {
    if (g_thread) {
        atomic_store(&g_run, false);
        pthread_join(g_th, NULL);
        g_thread = false;
    }
    if (g_fd >= 0) {
        enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(g_fd, VIDIOC_STREAMOFF, &t);
    }
    for (int i = 0; i < g_nmap; i++) munmap(g_map[i], g_len[i]);
    g_nmap = 0;
    if (g_fd >= 0) close(g_fd);
    g_fd = -1;
    pthread_mutex_lock(&g_m);
    free(g_frame);
    g_frame = NULL;
    g_have = false;
    pthread_mutex_unlock(&g_m);
}

static void fail(const char *status) {
    cleanup();
    atomic_store(&g_status, status);
}

void host_camera_open(void) {
    if (g_fd >= 0) return;
    g_fd = open("/dev/video0", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (g_fd < 0) {
        atomic_store(&g_status, "这台电脑没有摄像头（用测试图案）");
        return;
    }
    struct v4l2_format f;
    memset(&f, 0, sizeof f);
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f.fmt.pix.width = 640;
    f.fmt.pix.height = 480;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(g_fd, VIDIOC_S_FMT, &f) < 0 || f.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV || f.fmt.pix.width < 2 || f.fmt.pix.height < 1)
        return fail("摄像头不支持 YUYV（用测试图案）");
    g_w = (int)f.fmt.pix.width;
    g_h = (int)f.fmt.pix.height;
    g_stride = f.fmt.pix.bytesperline ? (int)f.fmt.pix.bytesperline : g_w * 2;
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof rb);
    rb.count = NBUF;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    if (ioctl(g_fd, VIDIOC_REQBUFS, &rb) < 0 || rb.count < 1) return fail("摄像头打不开（用测试图案）");
    for (unsigned i = 0; i < rb.count && i < NBUF; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (ioctl(g_fd, VIDIOC_QUERYBUF, &b) < 0) return fail("摄像头打不开（用测试图案）");
        void *m = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, b.m.offset);
        if (m == MAP_FAILED) return fail("摄像头打不开（用测试图案）");
        g_map[g_nmap] = m;
        g_len[g_nmap] = b.length;
        g_nmap++;
        if (ioctl(g_fd, VIDIOC_QBUF, &b) < 0) return fail("摄像头打不开（用测试图案）");
    }
    pthread_mutex_lock(&g_m);
    g_frame = calloc((size_t)g_w * (size_t)g_h, 4);
    pthread_mutex_unlock(&g_m);
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!g_frame || ioctl(g_fd, VIDIOC_STREAMON, &t) < 0) return fail("摄像头打不开（用测试图案）");
    atomic_store(&g_status, "电脑摄像头工作中"); /* before the reader, which may report an error at once */
    atomic_store(&g_run, true);
    if (pthread_create(&g_th, NULL, reader, NULL) != 0) {
        atomic_store(&g_run, false);
        return fail("摄像头打不开（用测试图案）");
    }
    g_thread = true;
}

bool host_camera_frame(uint8_t *bgra, size_t cap, int *w, int *h) {
    pthread_mutex_lock(&g_m);
    bool ok = g_have && g_frame && (size_t)g_w * (size_t)g_h * 4 <= cap;
    if (ok) {
        memcpy(bgra, g_frame, (size_t)g_w * (size_t)g_h * 4);
        *w = g_w;
        *h = g_h;
    }
    pthread_mutex_unlock(&g_m);
    return ok;
}

void host_camera_close(void) {
    cleanup();
    atomic_store(&g_status, "未启动");
}

const char *host_camera_status(void) { return atomic_load(&g_status); }
