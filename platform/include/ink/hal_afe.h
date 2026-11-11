/*
 * Voice front end (architecture v1.0 §9.4): echo cancellation against the
 * speaker reference, noise suppression, voice activity and the wake word.
 * On the device this wraps ESP-SR (AFE and WakeNet; its S31 support is
 * preliminary and unverified on hardware); the simulator models its results
 * and costs. Called from the audio service's task only.
 */
#ifndef INK_HAL_AFE_H
#define INK_HAL_AFE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool speech;              /* someone speaks (after echo cancellation) */
    bool wake;                /* the wake word was just heard (once per utterance) */
    uint64_t speech_since_us; /* when the current speech began (valid while speech) */
    int command;              /* a command word just recognised: its index in hal_afe_set_commands, else -1 */
} hal_afe_result_t;

/** This device can listen hands-free (front end and wake word model present). */
bool hal_afe_available(void);
/** NULL-terminated supported names; an empty list when unavailable. */
const char *const *hal_afe_names(void);
int hal_afe_select_name(int index);
int hal_afe_start(void);
/** The command words to recognise on the device (ESP-SR MultiNet; the strings must outlive the use). */
int hal_afe_set_commands(const char *const *phrases, int n);
void hal_afe_stop(void);
/** One block of capture: the microphone and what the speaker played, `t_us` its start. */
void hal_afe_feed(const int16_t *mic, const int16_t *ref, int samples, uint64_t t_us, hal_afe_result_t *out);

#ifdef __cplusplus
}
#endif
#endif
