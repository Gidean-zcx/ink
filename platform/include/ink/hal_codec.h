/*
 * ink HAL — speech codec (Opus).
 *
 * Codecs are platform services because the fast implementations are
 * platform-specific: esp_audio_codec with RISC-V assembly on the ESP32-S31,
 * libopus on Linux. The system uses 16 kHz mono, 60 ms frames (960 samples)
 * for both directions (the hub sends TTS at 16 kHz to save data and CPU).
 *   simulator  a stand-in codec with Opus-sized packets, the target's CPU
 *              cost per frame and its memory footprint; decoded audio is a
 *              tone pattern so speaker.wav stays audible
 */
#ifndef INK_HAL_CODEC_H
#define INK_HAL_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_OPUS_FRAME_SAMPLES 960 /* 60 ms at 16 kHz */
#define HAL_OPUS_MAX_PACKET 400

typedef struct hal_opus_enc hal_opus_enc_t;
typedef struct hal_opus_dec hal_opus_dec_t;

/** bitrate in bit/s (e.g. 24000); complexity 0 (cheapest) .. 10. NULL if out of memory. */
hal_opus_enc_t *hal_opus_enc_new(int bitrate, int complexity);
/** Encode exactly HAL_OPUS_FRAME_SAMPLES samples. Returns packet bytes, or -1. */
int hal_opus_encode(hal_opus_enc_t *e, const int16_t *pcm, uint8_t *out, int cap);
void hal_opus_enc_free(hal_opus_enc_t *e);

hal_opus_dec_t *hal_opus_dec_new(void);
/**
 * Decode one packet into HAL_OPUS_FRAME_SAMPLES samples. `pkt` NULL / `len`
 * 0 = the packet was lost: conceal. Returns samples written, or -1.
 */
int hal_opus_decode(hal_opus_dec_t *d, const uint8_t *pkt, int len, int16_t *pcm);
void hal_opus_dec_free(hal_opus_dec_t *d);

#ifdef __cplusplus
}
#endif
#endif
