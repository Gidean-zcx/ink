/*
 * Camera service (see ink/sys/camera.h): "take a picture" as a capability.
 * The camera's supply, set-up and timeouts are the platform's. A capture runs
 * on the media executor; requests queue there one after another. The last
 * picture is kept in PSRAM as the object the receipt names (keeping photos on
 * the card comes with the media store); under memory pressure it is given
 * back.
 */
#include <stdio.h>

#include "ink/sys/i18n.h"
#include "ink/hal_camera.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/camera.h"
#include "ink/sys/cap.h"
#include "ink/sys/res.h"

static struct {
    hal_camera_info_t info;
    uint8_t *frame; /* the last picture (RGB565), given back under memory pressure */
    uint32_t seq;
    osal_mutex_t *lock; /* capture, copy and reclaim share the frame */
    int busy; /* allocation may ask this task to reclaim its own frame */
} CAM;

/* Optional retained-session HAL hook; the simulator still supplies single frames. */
__attribute__((weak)) void hal_camera_session(const int *active) { (void)active; }
static int preview_active;
static ink_exec_job_t session_job;
static void session_update(void *arg) {
    (void)arg;
    hal_camera_session(&preview_active);
}
void ink_camera_session(bool active) { /* UI; cleanup follows pending MEDIA work. */
    __atomic_store_n(&preview_active, active, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_MEDIA, &session_job, session_update, NULL);
}

static size_t reclaim(void *ctx, size_t want) { /* the resource manager, any task */
    (void)ctx;
    (void)want;
    if (__atomic_load_n(&CAM.busy, __ATOMIC_ACQUIRE) || osal_mutex_lock(CAM.lock, 0) != OSAL_OK) return 0;
    size_t n = CAM.frame ? CAM.info.frame_bytes : 0;
    ink_mem_free(CAM.frame);
    CAM.frame = NULL;
    osal_mutex_unlock(CAM.lock);
    return n;
}

static int capture_frame(void) { /* under CAM.lock, exec.media */
    __atomic_store_n(&CAM.busy, 1, __ATOMIC_RELEASE);
    if (!CAM.frame) {
        const char *prev = osal_mem_tag("camera");
        CAM.frame = ink_mem_alloc("camera", INK_MEM_PSRAM, CAM.info.frame_bytes); /* admitted: a large block */
        osal_mem_tag(prev);
    }
    int n = CAM.frame ? hal_camera_capture(CAM.frame, CAM.info.frame_bytes, 3000) : -1;
    if (n > 0) { CAM.seq++; ink_probe_count("camera.photos"); }
    return n;
}

static void cap_capture(ink_call_t *c) { /* exec.media */
    osal_mutex_lock(CAM.lock, OSAL_WAIT_FOREVER);
    int n = capture_frame();
    if (n <= 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, CAM.frame ? "the camera took no picture (%d)" : "out of memory", n);
    } else {
        snprintf(c->rc.object, sizeof c->rc.object, "photo:%u", (unsigned)CAM.seq);
        c->rc.revision = CAM.seq;
        snprintf(c->rc.say, sizeof c->rc.say, TR("取好了，没有存到相册"));
    }
    __atomic_store_n(&CAM.busy, 0, __ATOMIC_RELEASE);
    osal_mutex_unlock(CAM.lock);
}

static const ink_cap_t CAMERA_CAPS[] = {
    {.id = "camera.capture", .locked_owner_only = true, .app = "camera",
     .title = "取一帧画面给 AI 看，不保存",
     .output = "{\"type\":\"object\",\"properties\":{\"object\":{\"type\":\"string\"}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .permission = "camera",
     .qos = INK_QOS_MEDIA,
     .timeout_ms = 5000,
     .exec = INK_EXEC_MEDIA,
     .run = cap_capture},
};

int ink_camera_init(void) {
    if (!hal_camera_info(&CAM.info)) return 0; /* no camera: no capability (apps check ink_cap_available) */
    if (CAM.lock) return 0;
    CAM.lock = osal_mutex_create();
    if (!CAM.lock) return -1;
    ink_mem_budget("camera", INK_MEM_PSRAM, CAM.info.frame_bytes);
    ink_mem_reclaimer("camera-last-photo", 20, reclaim, NULL);
    return ink_cap_register(CAMERA_CAPS, 1);
}

uint8_t *ink_camera_read_rgb(int *w, int *h) {
    if (!CAM.lock) { *w = *h = 0; return NULL; }
    osal_mutex_lock(CAM.lock, OSAL_WAIT_FOREVER);
    int n = capture_frame();
    size_t count = (size_t)CAM.info.width * CAM.info.height;
    uint8_t *rgb = n > 0 ? osal_malloc(count * 3, OSAL_MEM_EXTERNAL) : NULL;
    if (rgb) for (size_t i = 0; i < count; i++) {
        unsigned v = (unsigned)CAM.frame[i * 2] << 8 | CAM.frame[i * 2 + 1];
        rgb[i * 3] = ((v >> 11) & 31) * 255 / 31;
        rgb[i * 3 + 1] = ((v >> 5) & 63) * 255 / 63;
        rgb[i * 3 + 2] = (v & 31) * 255 / 31;
    }
    *w = CAM.info.width; *h = CAM.info.height;
    __atomic_store_n(&CAM.busy, 0, __ATOMIC_RELEASE);
    osal_mutex_unlock(CAM.lock);
    return rgb;
}
