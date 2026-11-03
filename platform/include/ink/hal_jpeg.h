/*
 * JPEG decoding (baseline, grayscale) for the remote screen's photo regions
 * (D-53). On the S31 the hardware JPEG codec (about 96 grayscale 720x1280
 * frames per second, Espressif; no light sleep while it runs); the simulator
 * uses a stand-in format with the same role and modelled cost.
 */
#ifndef INK_HAL_JPEG_H
#define INK_HAL_JPEG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Decode into w x h 8-bit gray. 0, or -1 (not a picture of that size). Task context. */
int hal_jpeg_decode_gray(const uint8_t *src, size_t n, uint8_t *dst, int w, int h);

#ifdef __cplusplus
}
#endif
#endif
