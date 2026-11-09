/*
 * Camera service: the camera.capture capability (architecture v1.0 §4.2). Registered only on devices with a camera.
 */
#ifndef INK_SYS_CAMERA_H
#define INK_SYS_CAMERA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_camera_init(void);
/** MEDIA only. Capture RGB888 in PSRAM; caller osal_free()s it. NULL on failure. */
uint8_t *ink_camera_read_rgb(int *w, int *h);

#ifdef __cplusplus
}
#endif
#endif
