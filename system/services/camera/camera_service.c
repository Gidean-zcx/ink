/*
 * Camera service (see ink/sys/camera.h): "take a picture" as a capability.
 * The camera's supply, set-up and timeouts are the platform's. A capture runs
 * on the media executor; requests queue there one after another. The last
 * picture is kept in PSRAM as the object the receipt names (keeping photos on
 * the card comes with the media store).
 */
#include <stdio.h>

#include "ink/hal_camera.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/camera.h"
#include "ink/sys/cap.h"

static struct {
    hal_camera_info_t info;
    uint8_t *frame; /* the last picture (RGB565) */
    uint32_t seq;
} CAM;

static void cap_capture(ink_call_t *c) { /* exec.media */
    if (!CAM.frame) {
        const char *prev = osal_mem_tag("camera");
        CAM.frame = osal_malloc(CAM.info.frame_bytes, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
    }
    int n = CAM.frame ? hal_camera_capture(CAM.frame, CAM.info.frame_bytes, 3000) : -1;
    if (n <= 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, CAM.frame ? "the camera took no picture (%d)" : "out of memory", n);
        return;
    }
    CAM.seq++;
    ink_probe_count("camera.photos");
    snprintf(c->rc.object, sizeof c->rc.object, "photo:%u", (unsigned)CAM.seq);
    c->rc.revision = CAM.seq;
    snprintf(c->rc.say, sizeof c->rc.say, "拍好了");
}

static const ink_cap_t CAMERA_CAPS[] = {
    {.id = "camera.capture",
     .title = "拍一张照片",
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
    return ink_cap_register(CAMERA_CAPS, 1);
}
