/* host_camera.h on Linux: V4L2 /dev/video0, 640x480 YUYV converted to BGRA on a reader thread. */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "host_camera.h"

#define NBUF 4
static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static uint8_t *g_frame;
static int g_w, g_h, g_fd = -1;
static bool g_have, g_run;
static const char *g_status = "未启动";
static void *g_map[NBUF];
static size_t g_len[NBUF];
static pthread_t g_th;

static uint8_t clamp(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void yuyv_to_bgra(const uint8_t *src, uint8_t *dst, int w, int h) {
    for (int i = 0; i < w * h / 2; i++) {
        int y0 = src[4 * i], u = src[4 * i + 1] - 128, y1 = src[4 * i + 2], v = src[4 * i + 3] - 128;
        for (int k = 0; k < 2; k++) {
            int y = k ? y1 : y0;
            uint8_t *p = dst + 8 * i + 4 * k;
            p[0] = clamp(y + (1814 * u >> 10));
            p[1] = clamp(y - ((352 * u + 731 * v) >> 10));
            p[2] = clamp(y + (1436 * v >> 10));
            p[3] = 255;
        }
    }
}

static void *reader(void *arg) {
    (void)arg;
    while (g_run) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (ioctl(g_fd, VIDIOC_DQBUF, &b) < 0) {
            if (errno == EAGAIN) {
                usleep(5000);
                continue;
            }
            break;
        }
        pthread_mutex_lock(&g_m);
        if (g_frame) {
            yuyv_to_bgra(g_map[b.index], g_frame, g_w, g_h);
            g_have = true;
        }
        pthread_mutex_unlock(&g_m);
        ioctl(g_fd, VIDIOC_QBUF, &b);
    }
    return NULL;
}

void host_camera_open(void) {
    g_fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (g_fd < 0) {
        g_status = "这台电脑没有摄像头（用测试图案）";
        return;
    }
    struct v4l2_format f;
    memset(&f, 0, sizeof f);
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f.fmt.pix.width = 640;
    f.fmt.pix.height = 480;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof rb);
    rb.count = NBUF;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    if (ioctl(g_fd, VIDIOC_S_FMT, &f) < 0 || f.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV || ioctl(g_fd, VIDIOC_REQBUFS, &rb) < 0) {
        g_status = "摄像头不支持 YUYV（用测试图案）";
        close(g_fd);
        g_fd = -1;
        return;
    }
    for (unsigned i = 0; i < rb.count && i < NBUF; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (ioctl(g_fd, VIDIOC_QUERYBUF, &b) < 0) break;
        g_len[i] = b.length;
        g_map[i] = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, b.m.offset);
        ioctl(g_fd, VIDIOC_QBUF, &b);
    }
    g_w = (int)f.fmt.pix.width;
    g_h = (int)f.fmt.pix.height;
    g_frame = malloc((size_t)g_w * (size_t)g_h * 4);
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!g_frame || ioctl(g_fd, VIDIOC_STREAMON, &t) < 0) {
        g_status = "摄像头打不开（用测试图案）";
        return;
    }
    g_run = true;
    pthread_create(&g_th, NULL, reader, NULL);
    g_status = "电脑摄像头工作中";
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
    if (g_run) {
        g_run = false;
        pthread_join(g_th, NULL);
    }
    if (g_fd >= 0) {
        enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(g_fd, VIDIOC_STREAMOFF, &t);
        for (int i = 0; i < NBUF; i++)
            if (g_map[i]) munmap(g_map[i], g_len[i]);
        close(g_fd);
        g_fd = -1;
    }
}

const char *host_camera_status(void) { return g_status; }
