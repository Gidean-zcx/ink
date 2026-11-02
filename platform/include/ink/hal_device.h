/*
 * ink HAL — device description.
 *
 * Every platform exposes a device profile: what hardware exists and how big
 * the budgets are. The portable system adapts to it at runtime (cache sizes,
 * layout density, which apps appear) instead of being compiled for one board.
 * The simulator loads the same profile from profiles/<name>.json.
 */
#ifndef INK_HAL_DEVICE_H
#define INK_HAL_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_CLASS_MCU = 0,   /* FreeRTOS microcontroller (ESP32-S31) */
    HAL_CLASS_SOC = 1,   /* embedded Linux SoC (RK3566 ...) */
    HAL_CLASS_DESKTOP = 2,
} hal_device_class_t;

typedef struct {
    const char *profile_id;      /* e.g. "metalio-e-ink4-plus" */
    const char *display_name;
    hal_device_class_t device_class;
    int cpu_cores;
    int cpu_mhz;
    uint32_t internal_heap_bytes; /* budget available to the system after platform/static use */
    uint32_t dma_heap_bytes;
    uint32_t external_heap_bytes; /* PSRAM (MCU) or a RAM budget (SoC/desktop) */
    bool has_modem;
    bool has_wifi;
    bool has_audio_out;
    bool has_audio_in;
    bool has_frontlight;
    bool has_haptics;
    bool has_rtc;
} hal_device_info_t;

void hal_device_get_info(hal_device_info_t *out);

/** Reset reason of the last boot, e.g. "poweron", "panic", "watchdog". */
const char *hal_device_reset_reason(void);

/** Platform log sink (UART on device, stdout/file elsewhere). */
void hal_log_write(const char *line);

#ifdef __cplusplus
}
#endif
#endif
