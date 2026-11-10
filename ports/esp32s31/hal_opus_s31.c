/* Same libopus API and VOIP/DTX parameters as 78/esp-opus-encoder 2.4.1. */
#include "ink/hal_codec.h"
#include "ink/hal_audio.h"
#include "ink/osal.h"
#include "opus.h"

hal_opus_enc_t *hal_opus_enc_new(int bitrate, int complexity) {
    OpusEncoder *e = osal_malloc((size_t)opus_encoder_get_size(1), OSAL_MEM_EXTERNAL);
    if (!e) return NULL;
    if (opus_encoder_init(e, HAL_AUDIO_RATE, 1, OPUS_APPLICATION_VOIP) != OPUS_OK ||
        opus_encoder_ctl(e, OPUS_SET_BITRATE(bitrate)) != OPUS_OK ||
        opus_encoder_ctl(e, OPUS_SET_COMPLEXITY(complexity)) != OPUS_OK ||
        opus_encoder_ctl(e, OPUS_SET_DTX(1)) != OPUS_OK) { osal_free(e); return NULL; }
    return (hal_opus_enc_t *)e;
}
int hal_opus_encode(hal_opus_enc_t *e, const int16_t *pcm, uint8_t *out, int cap) {
    if (!e || !pcm || !out || cap <= 0) return -1;
    int n = (int)opus_encode((OpusEncoder *)e, pcm, HAL_OPUS_FRAME_SAMPLES, out, cap);
    return n < 0 ? -1 : n;
}
void hal_opus_enc_free(hal_opus_enc_t *e) { osal_free(e); }
hal_opus_dec_t *hal_opus_dec_new(void) {
    OpusDecoder *d = osal_malloc((size_t)opus_decoder_get_size(1), OSAL_MEM_EXTERNAL);
    if (!d) return NULL;
    if (opus_decoder_init(d, HAL_AUDIO_RATE, 1) != OPUS_OK) { osal_free(d); return NULL; }
    return (hal_opus_dec_t *)d;
}
int hal_opus_decode(hal_opus_dec_t *d, const uint8_t *pkt, int len, int16_t *pcm) {
    if (!d || !pcm || len < 0 || (!pkt && len)) return -1;
    int n = opus_decode((OpusDecoder *)d, pkt, len, pcm, HAL_OPUS_FRAME_SAMPLES, 0);
    return n < 0 ? -1 : n;
}
void hal_opus_dec_free(hal_opus_dec_t *d) { osal_free(d); }
