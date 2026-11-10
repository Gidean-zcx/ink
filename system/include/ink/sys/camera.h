/*
 * Camera service: the camera.capture capability (architecture v1.0 §4.2). Registered only on devices with a camera.
 */
#ifndef INK_SYS_CAMERA_H
#define INK_SYS_CAMERA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_camera_init(void);
/** MEDIA only. Capture RGB888 in PSRAM; caller osal_free()s it. NULL on failure. */
uint8_t *ink_camera_read_rgb(int *w, int *h);
bool ink_camera_restricted(void);
int ink_camera_open_locked(void); /* UI only; app.open uses the same camera. */
bool ink_camera_close_locked(void); /* UI only. */

#ifdef __cplusplus
}
#endif
#endif
