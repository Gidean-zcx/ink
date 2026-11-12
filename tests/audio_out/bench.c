/* Reuse the host decoder/file/PSRAM stubs, with real SpeexDSP. */
#define main formats_reference_main
#include "../music/formats.c"
#undef main
#include "speex/speex_resampler.h"
#include "speex/ink_quality.h"
static int prefix(const char *path, unsigned ms, const char *output) {
    ink_format_t *r; int err;
    assert(!ink_format_open_for("test", path, &r, &err));
    if (ms) assert(ink_format_seek(r, ms) == ms);
    unsigned rate = ink_format_rate(r), ch = ink_format_channels(r);
    size_t count = rate * 60 / 1000;
    int16_t *raw = osal_malloc(count * ch * 2, 0), *mono = osal_malloc(count * 2, 0);
    int16_t *out = osal_malloc((HAL_OPUS_FRAME_SAMPLES + 1) * 2, 0);
    assert(raw && mono && out);
    size_t n = ink_format_pcm(r, raw, count);
    for (size_t i = 0; i < n; i++) mono[i] = ch == 1 ? raw[i] : (int16_t)(((int)raw[2 * i] + raw[2 * i + 1]) / 2);
    SpeexResamplerState *s = speex_resampler_init(1, rate, 16000, INK_AUDIO_PCM_QUALITY, &err);
    spx_uint32_t in = (spx_uint32_t)n, len = HAL_OPUS_FRAME_SAMPLES + 1;
    assert(s && !speex_resampler_process_int(s, 0, mono, &in, out, &len) && in == n);
    FILE *f = fopen(output, "wb"); assert(f);
    assert(fwrite(out, 2, len, f) == len && !fclose(f));
    speex_resampler_destroy(s); ink_format_close(r);
    osal_free(raw); osal_free(mono); osal_free(out);
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 4) return prefix(argv[1], (unsigned)strtoul(argv[2], NULL, 10), argv[3]);
    assert(argc == 2);
    const char *names[] = {"tone-44100-2.wav", "tone-44100-2.flac", "tone-44100-2.mp3", "tone-44100-2.ogg"};
    for (unsigned k = 0; k < sizeof names / sizeof *names; k++) {
        char path[1024]; snprintf(path, sizeof path, "%s/%s", argv[1], names[k]);
        ink_format_t *r; int err;
        size_t before = live; peak = live;
        assert(!ink_format_open_for("test", path, &r, &err));
        unsigned rate = ink_format_rate(r), channels = ink_format_channels(r);
        size_t count = rate * 60 / 1000;
        int16_t *raw = osal_malloc(count * channels * 2, 0), *mono = osal_malloc(count * 2, 0);
        int16_t *out = osal_malloc((HAL_OPUS_FRAME_SAMPLES + 1) * 2, 0);
        size_t pre_filter = live;
        SpeexResamplerState *s = speex_resampler_init(1, rate, 16000, INK_AUDIO_PCM_QUALITY, &err);
        assert(s && raw && mono && out);
        size_t filter_bytes = live - pre_filter;
        clock_t decode = 0, resample = 0, worst = 0;
        size_t total = 0;
        for (;;) {
            clock_t t = clock();
            size_t n = ink_format_pcm(r, raw, count);
            decode += clock() - t;
            if (!n) break;
            t = clock();
            for (size_t i = 0; i < n; i++) mono[i] = (int16_t)(((int)raw[2 * i] + raw[2 * i + 1]) / 2);
            size_t offset = 0;
            while (offset < n) {
                spx_uint32_t in = (spx_uint32_t)(n - offset), len = HAL_OPUS_FRAME_SAMPLES / 4;
                assert(!speex_resampler_process_int(s, 0, mono + offset, &in, out, &len) && (in || len));
                offset += in;
            }
            clock_t dt = clock() - t;
            resample += dt;
            if (dt > worst) worst = dt;
            total += n;
        }
        double scale = 1000.0 * rate / total / CLOCKS_PER_SEC;
        printf("%s decode_ms_per_s=%.4f resample_ms_per_s=%.4f total_ms_per_s=%.4f max_resample_60ms=%.4f filter_psram=%zu peak_psram=%zu\n",
            names[k], decode * scale, resample * scale, (decode + resample) * scale,
            worst * 1000.0 / CLOCKS_PER_SEC, filter_bytes, peak - before);
        speex_resampler_destroy(s); ink_format_close(r);
        osal_free(raw); osal_free(mono); osal_free(out);
        assert(live == before);
    }
    return 0;
}
