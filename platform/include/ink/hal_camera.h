/*
 * ink HAL — camera. One still picture at a time (the Plus: GC2145 on the S31's DVP port, powered by
 * TCA9555 P05, reset shared on P12; upstream takes 640x480 RGB565 after 8 warm-up frames).
 */
#ifndef INK_HAL_CAMERA_H
#define INK_HAL_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;   /* of a picture */
    size_t frame_bytes;  /* RGB565, high byte first */
} hal_camera_info_t;

/** False if this device has no camera. */
bool hal_camera_info(hal_camera_info_t *out);
/** Take one picture into buf (>= frame_bytes), which the camera's DMA fills directly: allocate it
 *  with osal_malloc(frame_bytes, OSAL_MEM_DMA | OSAL_MEM_EXTERNAL) (upstream: a PSRAM frame buffer
 *  from esp_cam_ctlr_alloc_buffer). Loads the sensor's sequence, drops the warm-up frames, keeps
 *  one, stops. timeout_ms covers the whole call. Returns the bytes written, or < 0: no camera or
 *  wrong buffer (-1), sensor not answering (-2, -3), timeout (-4), power or reset lost meanwhile
 *  (-5), sensor not set up for VGA RGB565 (-6). Task context; about a second. */
int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms);
/** Viewfinder session: while *active is non-zero, captures keep the sensor set up between pictures
 *  (only the first loads the sequence); when it falls, the next call ends the session. Same task as
 *  the captures. Optional: the camera service has a no-op fallback. */
void hal_camera_session(const int *active);

#ifdef __cplusplus
}
#endif

#endif
