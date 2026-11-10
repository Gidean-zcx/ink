#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/hal_codec.h"
#include "ink/osal.h"
#include "opus.h"
static int allocations;
static bool no_mem;
void *osal_malloc(size_t n, uint32_t caps) {
    assert(caps == OSAL_MEM_EXTERNAL);
    if (no_mem) return NULL;
    void *p = malloc(n);
    assert(p);
    allocations++;
    return p;
}
void osal_free(void *p) { if (p) { allocations--; free(p); } }
int main(void) {
    int16_t pcm[HAL_OPUS_FRAME_SAMPLES], decoded[HAL_OPUS_FRAME_SAMPLES];
    uint8_t packet[HAL_OPUS_MAX_PACKET];
    hal_opus_enc_t *e = hal_opus_enc_new(24000, 0);
    hal_opus_dec_t *d = hal_opus_dec_new();
    assert(e && d);
    opus_int32 value;
    assert(opus_encoder_ctl((OpusEncoder *)e, OPUS_GET_APPLICATION(&value)) == 0 && value == OPUS_APPLICATION_VOIP);
    assert(opus_encoder_ctl((OpusEncoder *)e, OPUS_GET_DTX(&value)) == 0 && value == 1);
    assert(opus_encoder_ctl((OpusEncoder *)e, OPUS_GET_BITRATE(&value)) == 0 && value == 24000);
    assert(opus_encoder_ctl((OpusEncoder *)e, OPUS_GET_COMPLEXITY(&value)) == 0 && value == 0);
    for (int i = 0; i < HAL_OPUS_FRAME_SAMPLES; i++) pcm[i] = (int16_t)((i % 80 - 40) * 400);
    for (int f = 0; f < 3; f++) {
        int n = hal_opus_encode(e, pcm, packet, sizeof packet);
        assert(n > 0 && n <= HAL_OPUS_MAX_PACKET);
        assert(opus_packet_get_nb_samples(packet, n, 16000) == HAL_OPUS_FRAME_SAMPLES);
        memset(decoded, 0, sizeof decoded);
        assert(hal_opus_decode(d, packet, n, decoded) == HAL_OPUS_FRAME_SAMPLES);
        int64_t energy = 0;
        for (int i = 0; i < HAL_OPUS_FRAME_SAMPLES; i++) energy += (int64_t)decoded[i] * decoded[i];
        assert(energy > 1000000);
    }
    assert(hal_opus_decode(d, NULL, 0, decoded) == HAL_OPUS_FRAME_SAMPLES);
    assert(hal_opus_encode(NULL, pcm, packet, sizeof packet) == -1);
    assert(hal_opus_decode(d, NULL, 1, decoded) == -1);
    assert(!hal_opus_enc_new(24000, 11) && !hal_opus_enc_new(0, 0));
    hal_opus_enc_free(e);
    hal_opus_dec_free(d);
    no_mem = true;
    assert(!hal_opus_enc_new(24000, 0) && !hal_opus_dec_new());
    assert(!allocations);
    puts("opus: real fixed-point encode/decode, frame size, DTX, PLC and allocation checks passed");
    return 0;
}
