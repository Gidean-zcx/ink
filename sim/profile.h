/*
 * Device profile: every number that makes the simulator behave like a given
 * device. Loaded from profiles/<id>.json. Values marked "pessimistic" in the
 * JSON are upper-bound estimates until calibrated on hardware or the ISS.
 */
#ifndef INK_SIM_PROFILE_H
#define INK_SIM_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/hal_input.h"

#define PROFILE_MAX_COSTS 96

typedef struct {
    bool present;
    double rtt_p50_ms, rtt_p99_ms; /* round trip to the hub */
    double mbps;                   /* usable throughput (LTE: min(radio, UART link)) */
    double loss;                   /* message loss probability -> retransmission delay */
    double resume_after_s;         /* radio drops to a low-power state after this much silence */
    double resume_extra_ms;        /* ... and the next packet pays this (LTE RRC resume / Wi-Fi DTIM) */
    double tail_s;                 /* radio stays in its high-power state this long after traffic */
    double outages_per_hour, outage_min_s, outage_max_s;
} profile_link_t;

typedef struct {
    char name[40];
    double cycles; /* target CPU cycles per unit */
} profile_cost_t;

typedef struct {
    int phases;
    bool flashing;
    double ghost; /* ghosting gain per transition (heuristic, 0 = none) */
    bool supported;
} profile_waveform_t;

typedef struct {
    char id[64];
    char name[128];
    hal_device_class_t device_class;

    /* cpu & kernel */
    int cores;
    int mhz;
    double coremark_per_mhz_core;   /* reference performance per core */
    double host_to_target;          /* fallback when self-calibration is disabled */
    uint64_t sched_overhead_ns;
    uint64_t ctx_switch_ns;
    uint64_t queue_op_ns;
    uint64_t long_burst_warn_ns;
    double host_stack_ratio;
    uint32_t tcb_bytes;
    uint32_t sync_obj_bytes;
    int tick_hz;              /* RTOS tick: delays/timeouts are quantized to it */
    uint32_t main_stack_bytes; /* stack of the task that runs ink_main() */
    uint64_t now_cost_ns;     /* charged per osal_now_us() so busy-wait loops make progress */

    /* memory budgets available to the system */
    size_t internal_heap;
    size_t dma_heap;
    size_t external_heap;
    size_t always_internal_max;

    /* display */
    hal_panel_info_t panel;
    uint32_t frame_time_us;     /* one waveform phase = one full-panel scan */
    uint32_t power_up_us;
    uint32_t power_down_us;
    uint32_t touch_pause_us;
    uint32_t lcd_frame_us;      /* LCD: time until visible */
    uint32_t keep_power_ms;     /* keep panel power on this long after an update (0 = off after each) */
    double prep_cpu_duty;       /* fraction of each core used by line feeders during waveform playback */
    double submit_copy_mbps;    /* framebuffer copy bandwidth for submitted areas */
    uint32_t line_queue_us;     /* time the prepared-line queue bridges when the feeders freeze (= lines x line time) */
    int line_count;             /* lines scanned per phase */
    uint32_t line_ns;           /* scan time of one line */
    int line_queue_lines;       /* prepared-line queue depth (32 design target, 127 upstream) */
    uint32_t line_prep_ns;      /* one feeder prepares one line in this long (2 feeders) */
    uint32_t phase_min_us;      /* shortest phase the driver allows; feeders spin for most of it */
    int gc16_full_phases;       /* complete GC16 table for full-screen flashing updates */
    int clear_frames;           /* epd_clear frames used to recover from an underrun */
    double lcd_isr_duty;        /* LCD bounce-buffer refill interrupts on core 0 during playback */
    int diff_pass;              /* CPU difference image + back-buffer copy: 0 none (EPD controller), 1 area, 2 full-width rows (upstream) */
    profile_waveform_t wf[HAL_WF_COUNT];
    size_t panel_buffers_external; /* bytes the panel driver keeps in external RAM */
    size_t panel_buffers_internal; /* bytes the panel driver keeps in internal RAM (line queues, DMA) */

    /* input */
    bool has_touch;
    int touch_points;
    int i2c_khz;
    int touch_read_bytes;
    uint32_t keys_mask;
    uint32_t i2c_background_poll_ms; /* other devices on the shared bus (fuel gauge, RTC ...) */
    int i2c_background_bytes;
    uint32_t touch_read_us;
    uint32_t touch_poll_ms;
    uint32_t touch_detect_us;  /* finger down -> controller raises INT */

    /* internal flash (user-state partition) */
    size_t flash_user_bytes;
    uint32_t flash_sector;
    uint32_t flash_page;
    uint32_t erase_us_typ, erase_us_max;
    uint32_t prog_us_typ, prog_us_max;
    int flash_pessimism; /* 0 = typical, 1 = max, 2 = tail (mostly ~typ, 10% up to max) */
    bool flash_stalls_cpu; /* MCU: caches off while erasing/programming internal flash */
    uint32_t flash_write_chunk;
    size_t assets_partition_bytes; /* memory-mapped read-only assets (fonts...) */
    double flash_read_mbps;
    uint32_t flash_op_overhead_us;

    /* sd card (content/cache volumes) */
    double sd_read_mbps, sd_write_mbps;
    uint32_t sd_op_latency_us;
    uint32_t sd_write_busy_spike_ms;
    uint32_t sd_spike_every_kib;
    double sd_capacity_gib;

    /* network */
    profile_link_t wifi, lte;
    char default_link[8];          /* "lte" | "wifi" | "none" */
    size_t tls_internal_bytes, tls_external_bytes;
    uint32_t ws_stack, sys_evt_stack;
    uint32_t dead_detect_ms;       /* a dead connection is noticed after this (ping timeout) */

    /* audio: I2S DMA rings (16 kHz), codec memory */
    int audio_dma_desc, audio_dma_frames; /* per direction: descriptors x frames each */
    int audio_slot_bytes;                 /* bytes per I2S frame on the wire (stereo 32-bit slots = 8) */
    uint32_t audio_start_us, audio_stop_us; /* output path up (I2S + amplifier settle) / down */
    uint32_t audio_task_stack;
    int audio_task_prio;
    size_t opus_enc_bytes, opus_dec_bytes; /* codec state (external RAM) */

    /* power (estimates until measured) */
    double batt_mah;
    double ma_base_awake, ma_per_core_busy, ma_display_rails, ma_display_refresh;
    double ma_lte_idle, ma_lte_active, ma_wifi_idle, ma_wifi_active;
    double ma_audio_out, ma_audio_in, ma_light_full, ma_sleep;

    /* features */
    hal_device_info_t info;

    profile_cost_t costs[PROFILE_MAX_COSTS];
    int ncosts;
} sim_profile_t;

extern sim_profile_t g_profile;

int profile_load(const char *path, sim_profile_t *out, char *err, size_t errlen);
double profile_cost(const char *name); /* cycles per unit; 0 if unknown */

#endif
