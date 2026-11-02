/*
 * Stand-in Opus codec for the simulator. Packets have Opus sizes (bitrate x
 * 60 ms, tiny packets for silence as with DTX) and carry a summary of the
 * frame (level and pitch) so the decoder can turn them back into an audible
 * tone pattern for speaker.wav. CPU per frame and the codec state's memory
 * come from the device profile (opus.enc_frame / opus.dec_frame; state in
 * external RAM, sizes from the esp_audio_codec table).
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_audio.h"
#include "ink/hal_codec.h"
#include "sim.h"

#define MAGIC 0xA5

struct hal_opus_enc {
    int bitrate;
    void *state;
};

struct hal_opus_dec {
    void *state;
    double phase, freq, level;
};

/* Packet layout: MAGIC, level (u16), pitch in 0.1 Hz (u16), filler. */
int sim_opus_fake_packet(uint8_t *out, int cap, double freq, double level, int bytes, uint32_t seed) {
    if (cap < 5) return -1;
    if (bytes > cap) bytes = cap;
    if (bytes < 5) bytes = 5;
    uint16_t lv = (uint16_t)(level < 0 ? 0 : level > 32767 ? 32767 : level);
    uint16_t fq = (uint16_t)(freq * 10 < 0 ? 0 : freq * 10 > 65535 ? 65535 : freq * 10);
    out[0] = MAGIC;
    memcpy(out + 1, &lv, 2);
    memcpy(out + 3, &fq, 2);
    for (int i = 5; i < bytes; i++) {
        seed = seed * 1103515245u + 12345u;
        out[i] = (uint8_t)(seed >> 16);
    }
    return bytes;
}

hal_opus_enc_t *hal_opus_enc_new(int bitrate, int complexity) {
    (void)complexity;
    hal_opus_enc_t *e = calloc(1, sizeof *e);
    if (!e) abort();
    const char *prev = osal_mem_tag("opus");
    e->state = osal_malloc(g_profile.opus_enc_bytes ? g_profile.opus_enc_bytes : 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!e->state) {
        free(e);
        return NULL;
    }
    e->bitrate = bitrate > 0 ? bitrate : 24000;
    return e;
}

int hal_opus_encode(hal_opus_enc_t *e, const int16_t *pcm, uint8_t *out, int cap) {
    if (!e || !pcm || !out) return -1;
    osal_cost_units("opus.enc_frame", 1);
    double sum = 0;
    int crossings = 0;
    for (int i = 0; i < HAL_OPUS_FRAME_SAMPLES; i++) {
        sum += (double)pcm[i] * pcm[i];
        if (i && ((pcm[i] >= 0) != (pcm[i - 1] >= 0))) crossings++;
    }
    double rms = sqrt(sum / HAL_OPUS_FRAME_SAMPLES);
    double freq = crossings * (double)HAL_AUDIO_RATE / (2.0 * HAL_OPUS_FRAME_SAMPLES);
    int bytes = rms < 200 ? 8 : e->bitrate * 60 / 1000 / 8; /* DTX-like tiny packets for silence */
    return sim_opus_fake_packet(out, cap, freq, rms, bytes, (uint32_t)sum);
}

void hal_opus_enc_free(hal_opus_enc_t *e) {
    if (!e) return;
    osal_free(e->state);
    free(e);
}

hal_opus_dec_t *hal_opus_dec_new(void) {
    hal_opus_dec_t *d = calloc(1, sizeof *d);
    if (!d) abort();
    const char *prev = osal_mem_tag("opus");
    d->state = osal_malloc(g_profile.opus_dec_bytes ? g_profile.opus_dec_bytes : 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!d->state) {
        free(d);
        return NULL;
    }
    return d;
}

int hal_opus_decode(hal_opus_dec_t *d, const uint8_t *pkt, int len, int16_t *pcm) {
    if (!d || !pcm) return -1;
    osal_cost_units("opus.dec_frame", 1);
    if (pkt && len >= 5 && pkt[0] == MAGIC) {
        uint16_t lv, fq;
        memcpy(&lv, pkt + 1, 2);
        memcpy(&fq, pkt + 3, 2);
        d->level = lv;
        d->freq = fq / 10.0;
    } else {
        d->level *= 0.5; /* concealment: fade the previous frame out */
    }
    double amp = d->level * 1.41421356;
    for (int i = 0; i < HAL_OPUS_FRAME_SAMPLES; i++) {
        d->phase += 2 * M_PI * d->freq / HAL_AUDIO_RATE;
        if (d->phase > 2 * M_PI) d->phase -= 2 * M_PI;
        double v = amp * sin(d->phase);
        pcm[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return HAL_OPUS_FRAME_SAMPLES;
}

void hal_opus_dec_free(hal_opus_dec_t *d) {
    if (!d) return;
    osal_free(d->state);
    free(d);
}
