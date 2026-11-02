/*
 * ink HAL — power, battery, front light, haptics.
 */
#ifndef INK_HAL_POWER_H
#define INK_HAL_POWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int percent;        /* 0..100, from the fuel gauge when available */
    int millivolts;
    bool charging;
    bool external_power;
} hal_battery_t;

void hal_power_battery(hal_battery_t *out);

/* Power resources that share rails on some boards (Plus: modem, BT audio
 * chip and amplifier share one rail). The power service reference-counts
 * needs; the platform maps them onto rails. */
typedef enum {
    HAL_PWR_DISPLAY = 0,
    HAL_PWR_AUDIO_OUT,
    HAL_PWR_AUDIO_IN,
    HAL_PWR_MODEM,
    HAL_PWR_WIFI,
    HAL_PWR_COUNT
} hal_power_res_t;

void hal_power_request(hal_power_res_t res, bool on);

/* Front light (cold/warm), 0..1000 each; no-op where absent. */
void hal_light_set(int cold_permille, int warm_permille);

/* Haptics: short pulse for first feedback (<=150 ms requirement). No-op where absent. */
bool hal_haptic_available(void);
void hal_haptic_pulse(int ms);

#ifdef __cplusplus
}
#endif
#endif
