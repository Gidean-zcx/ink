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
/** Take one picture into buf (>= frame_bytes): start the sensor, drop the warm-up frames, keep
 *  one, stop. Returns the bytes written, or < 0 (no camera, sensor not answering, timeout). Task
 *  context; takes about half a second. */
int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
