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

/* ---- CPU clock and sleep ----
 *
 * System power state hint: the platform adapts what only it controls — the
 * CPU clock floor (idle frequency), the touch controller's scan mode and how
 * often other bus devices are polled. Standby itself is entered explicitly
 * with hal_power_sleep(). */
typedef enum { HAL_PWR_STATE_ACTIVE = 0, HAL_PWR_STATE_IDLE, HAL_PWR_STATE_LOCKED } hal_power_state_t;
void hal_power_set_state(hal_power_state_t s);

/* Keep the CPU at full speed while held (ESP-IDF esp_pm CPU_FREQ_MAX lock;
 * no-op where the OS scales the clock itself). Hold one around work that
 * must not run at the idle frequency: waveforms, audio codecs, TLS. */
typedef struct hal_cpu_boost hal_cpu_boost_t;
hal_cpu_boost_t *hal_cpu_boost_create(const char *name);
void hal_cpu_boost_acquire(hal_cpu_boost_t *b);
void hal_cpu_boost_release(hal_cpu_boost_t *b);

/* Wake sources while the chip sleeps. */
enum {
    HAL_WAKE_TIMER = 1u << 0,
    HAL_WAKE_POWER_KEY = 1u << 1, /* Plus: TCA9555 P17 -> shared INT -> EXT1 */
    HAL_WAKE_AI_KEY = 1u << 2,    /* Plus: BOOT (GPIO61); upstream does not arm it */
    HAL_WAKE_RTC = 1u << 3,       /* RTC alarm (reminders) */
    HAL_WAKE_MODEM = 1u << 4,     /* SMS / call / data from the 4G modem (needs its rail kept up) */
};

typedef struct {
    uint64_t max_us;      /* slice length: wake by timer after this long (0 = no timer) */
    uint32_t wake_mask;   /* HAL_WAKE_* */
    bool keep_modem;      /* keep the 4G modem powered and registered ("可接听待机") */
    /* Asked after the entry sequence, right before the chip sleeps (the real port: with the
     * scheduler suspended, so it must not block): true cancels the sleep, e.g. a wake key was
     * pressed and handled during the entry sequence. Optional. */
    bool (*cancel)(void *ctx);
    void *cancel_ctx;
} hal_sleep_cfg_t;

/**
 * Light sleep (task context): parks the panel PMIC, the SD card and the bus,
 * cuts the rails that may be cut, sleeps until a wake source or the timer,
 * then restores the hardware. All other tasks are suspended meanwhile and
 * input from sources not in wake_mask is lost. The caller must have
 * quiesced the system (no refresh, flash write, audio or SD access in
 * progress). Returns the HAL_WAKE_* bit that woke it, or 0 if the sleep was
 * refused (something still busy) or cancelled.
 */
uint32_t hal_power_sleep(const hal_sleep_cfg_t *cfg);

/* Front light (cold/warm), 0..1000 each; no-op where absent. */
void hal_light_set(int cold_permille, int warm_permille);

/* Haptics: short pulse for first feedback (<=150 ms requirement). No-op where absent. */
bool hal_haptic_available(void);
void hal_haptic_pulse(int ms);

#ifdef __cplusplus
}
#endif
#endif
