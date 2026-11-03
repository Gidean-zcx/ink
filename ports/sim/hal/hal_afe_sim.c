/*
 * Voice front end, modelled (see ink/hal_afe.h). The simulator does not run
 * echo cancellation or a wake word network: it knows what the person in the
 * scenario does (sim_audio_mic_state: quiet, speaking, saying the wake word)
 * and answers like the front end would, with its delays (wake word and voice
 * activity), its residual-echo mistakes while the speaker plays, and its CPU
 * and memory cost, all from the profile ("audio.voice_frontend", estimates
 * until measured with ESP-SR on the S31).
 */
#include <stdlib.h>

#include "ink/hal_afe.h"
#include "ink/osal.h"
#include "sim.h"

static struct {
    bool on;
    void *ram;
    bool speech;
    uint64_t speech_since_ns;
    uint64_t wake_for_ns; /* the wake utterance already reported (its start) */
    uint64_t echo_until_ns;
} F;

bool hal_afe_available(void) { return g_profile.afe.present; }

int hal_afe_start(void) {
    if (!g_profile.afe.present) return -1;
    if (F.on) return 0;
    const char *prev = osal_mem_tag("afe");
    F.ram = osal_malloc(g_profile.afe.ram_bytes, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!F.ram) return -1;
    F.on = true;
    F.speech = false;
    F.echo_until_ns = 0;
    return 0;
}

void hal_afe_stop(void) {
    if (!F.on) return;
    osal_free(F.ram);
    F.ram = NULL;
    F.on = false;
}

static bool loud(const int16_t *ref, int n) {
    for (int i = 0; i < n; i += 16)
        if (abs(ref[i]) > 256) return true;
    return false;
}

void hal_afe_feed(const int16_t *mic, const int16_t *ref, int samples, uint64_t t_us, hal_afe_result_t *out) {
    (void)mic;
    (void)t_us;
    osal_cost_cycles((uint64_t)(samples * g_profile.afe.cycles_per_sample));
    uint64_t now = vtk_now(), since;
    int st = sim_audio_mic_state(&since);
    out->wake = false;
    if (st == 2 && now >= since + SIM_US(g_profile.afe.wake_delay_us) && F.wake_for_ns != since + 1) {
        out->wake = true;
        F.wake_for_ns = since + 1; /* +1: a start at 0 is a real one */
        sim_metric_add("afe.wakes", 1);
    }
    if (st) { /* speaking (the wake word too) */
        if (!F.speech && now >= since + SIM_US(g_profile.afe.vad_on_us)) {
            F.speech = true;
            F.speech_since_ns = since;
        }
        F.echo_until_ns = 0;
    } else if (F.speech && !F.echo_until_ns && now >= since + SIM_US(g_profile.afe.vad_off_us)) {
        F.speech = false;
    }
    /* residual echo: while the speaker plays, now and then heard as speech for a moment */
    double rate = g_profile.afe.echo_false_per_min;
    if (!st && !F.speech && rate > 0 && ref && loud(ref, samples) && sim_rand01() < rate * samples / (60.0 * 16000.0)) {
        F.speech = true;
        F.speech_since_ns = now;
        F.echo_until_ns = now + SIM_MS(200);
        sim_metric_add("afe.echo_false_speech", 1);
    }
    if (F.echo_until_ns && now >= F.echo_until_ns && !st) {
        F.speech = false;
        F.echo_until_ns = 0;
    }
    out->speech = F.speech;
    out->speech_since_us = F.speech_since_ns / 1000;
}
