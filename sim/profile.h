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
    /* optional per-waveform overrides (0 = the display-wide value): a refresh algorithm whose
     * scan path costs more or less per line, or whose table uses a different phase time */
    uint32_t line_prep_ns;
    uint32_t phase_min_us;
} profile_waveform_t;

typedef struct {
    char id[64];
    char name[128];
    hal_device_class_t device_class;

    /* cpu & kernel */
    int cores;
    int mhz;
    double host_to_target;          /* fallback when self-calibration is disabled */
    uint64_t sched_overhead_ns;
    uint64_t ctx_switch_ns;
    uint64_t queue_op_ns;
    uint64_t long_burst_warn_ns;
    double host_stack_ratio;
    double icache_penalty;    /* extra fraction on annotated system CPU: code runs from flash through a 16 KiB I-cache */
    uint32_t dcache_bytes;    /* shared D-cache in front of mapped flash (and PSRAM) */
    uint32_t dcache_line;
    uint32_t tcb_bytes;
    uint32_t sync_obj_bytes;
    int tick_hz;              /* RTOS tick: delays/timeouts are quantized to it */
    uint32_t main_stack_bytes; /* stack of the task that runs ink_main() */
    uint64_t now_cost_ns;     /* charged per osal_now_us() so busy-wait loops make progress */

    /* memory budgets available to the system */
    size_t internal_heap;           /* sum of internal_regions */
    size_t internal_regions[8];     /* separate internal heap regions (largest block <= largest region) */
    size_t heap_region_overhead;    /* device allocator control data per region */
    int n_internal_regions;
    size_t dma_heap;
    size_t external_heap;
    size_t always_internal_max;

    /* display */
    hal_panel_info_t panel;
    uint32_t frame_time_us;     /* hardware EPD controller (feeders "none"): one phase */
    uint32_t power_up_us;
    uint32_t power_down_us;
    uint32_t touch_pause_us;
    bool pmic_tps65185;         /* panel power by the board driver against chip models (else power_up_us) */
    unsigned panel_vcom_mv;     /* VCOM the panel needs (its label); the PMIC model checks against it */
    uint32_t lcd_frame_us;      /* LCD: time until visible */
    /* EPDiy-style CPU feeders (feeders "cpu"): two max-priority tasks convert lines into per-feeder
     * queues; the LCD bounce-buffer interrupt pulls them; the phase is planned line by line */
    bool feeders_cpu;
    bool feeder_dynamic;        /* next free feeder takes the next line (else fixed odd/even, upstream) */
    bool feeder_area_columns;   /* convert only the area's columns (else full lines, upstream) */
    bool feeder_block;          /* feeders block on a full queue (else spin, upstream) */
    /* display driver: 0 = EPDiy-class waveform tables (global lock: one update at a time),
     * 1 = PQ-BN direct drive (drivers/epd/pqbn_v4.c: per-pixel plans, updates overlap) */
    int display_driver;
    struct {
        int plan_budget;        /* pixels the planner may visit per phase */
        bool auto_motion;       /* tiles switch to the binary motion mode by themselves */
        bool plan_during_scan;  /* the planner overlaps the scan (needs a port that plans behind the scan) */
        double full_swing;      /* pushes for a full black<->white swing (panel model and planner tables) */
        double stiction, stiction_tau; /* first-push loss after a rest: s0, time constant (phases) */
        double mobility_sd;     /* per-pixel spread of the push size (panel model only) */
        /* CPU cycles (ISS, tools/iss/bench/run_pqbn.sh): scan rows and planner */
        double row_idle, row_band, row_tile;
        double plan_base, plan_px, plan_motion_tile, plan_exit_tile, plan_idle;
    } pqbn;
    int line_count;             /* lines scanned per phase (panel lines + blanking) */
    uint32_t line_ns;           /* scan time of one line */
    int line_queue_lines;       /* per feeder (upstream 127) */
    int prefill_lines;          /* the scan starts once these many lines are ready (upstream 192) */
    uint32_t line_prep_ns;      /* one feeder converts one full panel line (ISS-measured) */
    uint32_t line_skip_ns;      /* one feeder queues an untouched (no-op) line */
    uint32_t phase_min_us;      /* shortest phase the driver allows */
    uint32_t frame_overhead_us; /* frame-done interrupt -> next phase start (update task, LUT, notify) */
    int bounce_lines;           /* the LCD interrupt refills bounce buffers of this many lines (two buffers) */
    bool lcd_isr_iram_safe;     /* the refill interrupt keeps running during flash operations */
    int gc16_full_phases;       /* complete GC16 table for full-screen flashing updates */
    int clear_frames;           /* epd_clear frames used to recover from an underrun */
    int gc16_full_clear_frames; /* a full-screen flashing GC16 is preceded by these clear frames (upstream "clean": 69) */
    double lcd_isr_duty;        /* LCD bounce-buffer refill interrupts on core 0 during the scan */
    int diff_pass;              /* CPU difference image + back-buffer copy: 0 none (EPD controller), 1 area,
                                 * 2 full-width rows of the area, 3 always the full screen (upstream Draw()) */
    profile_waveform_t wf[HAL_WF_COUNT];
    size_t panel_buffers_external; /* bytes the panel driver keeps in external RAM */
    size_t panel_buffers_internal; /* bytes the panel driver keeps in internal RAM (line queues, LUT, bounce, state) */
    size_t driver_lut_bytes;       /* waveform LUT (part of panel_buffers_internal) */

    /* input */
    bool has_touch;
    int touch_points;
    int i2c_khz;
    int touch_read_bytes;
    uint32_t keys_mask;
    uint8_t key_source[HAL_KEY_COUNT]; /* 0 GPIO (debounced), 1 IO expander (INT + I2C reads), 2 touch-controller cover key */
    uint32_t key_debounce_us;
    uint32_t i2c_background_poll_ms; /* other devices on the shared bus (fuel gauge, RTC ...) */
    int i2c_background_bytes;
    uint32_t touch_poll_ms;
    uint32_t touch_detect_us;  /* finger down -> controller raises INT */
    uint32_t touch_monitor_after_ms;  /* the controller drops to its slow monitor scan after this long untouched */
    uint32_t touch_monitor_period_ms; /* monitor scan period: up to this much extra latency for the first touch */
    uint32_t i2c_background_idle_poll_ms; /* background bus polling while the system is idle */

    /* internal flash (user-state partition) */
    size_t flash_user_bytes;
    uint32_t flash_sector;
    uint32_t flash_page;
    uint32_t erase_us_typ, erase_us_max;
    uint32_t prog_us_typ, prog_us_max;
    int flash_pessimism; /* 0 = typical, 1 = max, 2 = tail (mostly ~typ, 10% up to max) */
    bool flash_stalls_cpu; /* MCU: caches off while erasing/programming internal flash */
    size_t assets_partition_bytes; /* memory-mapped read-only assets (fonts...) */
    double flash_read_mbps;       /* raw SPI1 data rate */
    uint32_t flash_op_overhead_us; /* per cache-off window (IPC to the other core, cache off/on) */
    uint32_t flash_write_chunk;    /* one esp_flash_write window covers this many bytes (CONFIG_SPI_FLASH_WRITE_CHUNK_SIZE) */
    uint32_t flash_spi_cmd_bytes;  /* SPI1 moves at most this much per command (program and read) */
    uint32_t flash_spi_cmd_ns;     /* per SPI1 command: opcode/address/dummy + driver setup */
    uint32_t prog_first_us;        /* program command: fixed part (datasheet first-byte time) */

    /* sd card (content/cache volumes) */
    double sd_read_mbps, sd_write_mbps;
    uint32_t sd_op_latency_us;
    uint32_t sd_write_busy_spike_ms;
    uint32_t sd_spike_every_kib;
    double sd_capacity_gib;

    /* network */
    profile_link_t wifi, lte;
    char default_link[8];          /* "lte" | "wifi" | "none" */
    /* cellular SIM: the modem's selected slot (0 external, 1 internal) and each slot's card */
    int sim_selected;
    struct { bool present, sms, voice; } sim_card[2];
    size_t tls_internal_bytes, tls_external_bytes;
    uint32_t ws_stack, sys_evt_stack;
    uint32_t dead_detect_ms;       /* a dead connection is noticed after this (ping timeout) */
    size_t uart_nic_dma_bytes;     /* 4G UART-Ethernet DMA rings */
    uint32_t uart_nic_stack;
    bool ws_stack_external, uart_nic_stack_external; /* port decisions: these tasks never touch flash */

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
    int idle_mhz;               /* CPU clock floor when the system is not active and nothing boosts it */
    double ma_base_awake_idle;  /* base current while awake at idle_mhz */
    double ma_standby_modem;    /* modem registered + shared-rail parts while the chip sleeps ("可接听待机") */
    uint32_t sleep_enter_us;    /* standby entry: panel PMIC to sleep (upstream 50 + 120 ms), SD park, rails */
    uint32_t sleep_exit_us;     /* after the chip wakes: rails back, key baseline (upstream sequence) */
    uint32_t sleep_wake_us;     /* chip light-sleep exit (clock/PLL restore) before any task runs */
    uint32_t sd_remount_us;     /* SD card re-mount after a non-timer wake */

    /* features */
    hal_device_info_t info;

    profile_cost_t costs[PROFILE_MAX_COSTS];
    int ncosts;
    /* values the JSON did not set (defaults used), for the run log */
    int ndefaults;
    char defaults_used[2048];
} sim_profile_t;

extern sim_profile_t g_profile;

int profile_load(const char *path, sim_profile_t *out, char *err, size_t errlen);
double profile_cost(const char *name); /* cycles per unit; 0 (and one warning) if the profile lacks it */
void profile_set_warn(void (*fn)(const char *msg));

#endif
