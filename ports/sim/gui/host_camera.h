/*
 * The computer's camera for the desktop simulator: what is in front of the simulated device's
 * GC2145. macOS: AVFoundation; Linux: V4L2; elsewhere: none (the sensor model then shows a test
 * pattern).
 */
#ifndef INK_HOST_CAMERA_H
#define INK_HOST_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Start capturing (asynchronous on macOS, which may ask the user for permission first). */
void host_camera_open(void);
/** Copy the newest frame as BGRA (4 bytes per pixel); false if there is none yet. */
bool host_camera_frame(uint8_t *bgra, size_t cap, int *w, int *h);
void host_camera_close(void);
/** One line for the window: running, waiting for permission, denied, no camera... */
const char *host_camera_status(void);

#ifdef __cplusplus
}
#endif

#endif
