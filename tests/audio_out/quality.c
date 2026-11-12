/* Manual quality/cost sweep; real upstream FIR, synthetic signals only. */
#define main formats_reference_main
#include "../music/formats.c"
#undef main
#include <math.h>
#include "speex/speex_resampler.h"

static void measure(int q, int hz) {
    const unsigned rate = 44100, seconds = 3, count = rate * seconds;
    int16_t *in = osal_malloc(count * 2, OSAL_MEM_EXTERNAL);
    int16_t *out = osal_malloc(16000 * seconds * 2, OSAL_MEM_EXTERNAL);
    assert(in && out);
    for (unsigned i = 0; i < count; i++) in[i] = (int16_t)(16000 * sin(2 * M_PI * hz * i / rate));
    int err;
    size_t before = live;
    SpeexResamplerState *s = speex_resampler_init(1, rate, 16000, q, &err);
    assert(s);
    size_t bytes = live - before;
    spx_uint32_t n = count, len = 16000 * seconds;
    clock_t t = clock();
    assert(!speex_resampler_process_int(s, 0, in, &n, out, &len) && n == count);
    double ms = (double)(clock() - t) * 1000 / CLOCKS_PER_SEC / seconds;
    double a = 0, b = 0, mean = 0, power = 0, residual = 0;
    int freq = hz == 1000 ? 1000 : 6000;
    for (int i = 0; i < 16000; i++) {
        double x = out[16000 + i];
        mean += x / 16000;
        a += 2 * x * sin(2 * M_PI * freq * i / 16000) / 16000;
        b += 2 * x * cos(2 * M_PI * freq * i / 16000) / 16000;
    }
    for (int i = 0; i < 16000; i++) {
        double x = out[16000 + i] - mean;
        double e = x - a * sin(2 * M_PI * freq * i / 16000) - b * cos(2 * M_PI * freq * i / 16000);
        power += x * x / 16000;
        residual += e * e / 16000;
    }
    printf("q=%d hz=%d taps=%d psram=%zu cpu_ms_per_s=%.4f snr_db=%.4f attenuation_db=%.4f\n",
           q, hz, 2 * speex_resampler_get_input_latency(s), bytes, ms,
           10 * log10(fmax((a * a + b * b) / 2, 1e-20) / fmax(residual, 1e-20)),
           10 * log10(16000.0 * 16000 / 2 / fmax(power, 1e-20)));
    speex_resampler_destroy(s);
    osal_free(in);
    osal_free(out);
}
int main(void) {
    for (int q = 10; q >= 0; q--) {
        measure(q, 1000);
        measure(q, 10000);
    }
    assert(!live);
    return 0;
}
