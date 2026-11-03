/*
 * Internal interfaces shared by the simulator port (ports/sim). Not visible to
 * system or app code, which only sees the headers in platform/include/ink.
 *
 * Execution model reminder (see sim/vtk/vtk.h): all simulated code — tasks and
 * event callbacks — runs strictly one at a time, so module state needs no host
 * locks. Event callbacks run in ISR-like context: they must not call OSAL
 * functions or block; they use vtk_isr_* and the sim_* helpers marked ISR-safe.
 */
#ifndef INK_PORT_SIM_H
#define INK_PORT_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/hal_display.h"
#include "ink/hal_input.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "pools.h"
#include "profile.h"
#include "vtk.h"

#define SIM_MS(x) ((uint64_t)(x) * 1000000ull)
#define SIM_US(x) ((uint64_t)(x) * 1000ull)

typedef struct {
    const char *profile_path;
    const char *script_path;
    const char *out_dir;      /* frames, trace, metrics, logs */
    const char *budget_path;  /* optional CI budgets */
    const char *flash_image;  /* persist /user flash across runs (power-cut tests) */
    const char *sd_dir;       /* host directory backing /content and /cache */
    const char *asset_dirs[4]; /* read-only assets (fonts...), searched in order */
    int nasset_dirs;
    const char *seed_content;  /* host directory copied into /content at boot (books for scenarios) */
    const char *hub_script;    /* scripted hub rules (JSON); built-in defaults if NULL */
    bool no_random_outages;    /* disable the profile's random link outages */
    const char *flash_timing;  /* override the profile's flash timing: "typ" | "max" | "tail" */
    double i2c_faults;         /* share of hal_i2c transfers that time out (fault injection) */
    unsigned panel_vcom_mv;    /* override the panel's VCOM (0 = profile): a panel with another label */
    bool trace;               /* write Chrome/Perfetto trace JSON */
    bool frames;              /* write a PNG after every completed display update */
    bool quiet;
    bool realtime;
    double realtime_speed;
    bool interactive;         /* desktop simulator window (sim_gui.h): realtime, no default end */
    vtk_cpu_mode_t cpu_mode;
    double host_to_target;    /* MEASURED mode factor (0 = profile default) */
    uint64_t end_ns;          /* stop time (0 = when idle / script end) */
    uint32_t seed;            /* deterministic pseudo-randomness (flash timing tails, network) */
    int64_t wall_start_us;    /* RTC time at boot */
    const char *reset_reason;
} sim_options_t;

extern sim_options_t g_sim;

/* ---- time & cost ---- */
extern int g_sim_mhz; /* current CPU clock (frequency scaling; 0 = the profile's maximum) */
static inline uint64_t sim_cycles_ns(double cycles) {
    int mhz = g_sim_mhz > 0 ? g_sim_mhz : g_profile.mhz;
    return (uint64_t)(cycles * 1000.0 / (double)(mhz > 0 ? mhz : 1));
}
/** Charge CPU work to the calling task (task context; ignored in MEASURED mode). */
void sim_charge_cycles(double cycles);
/** Charge CPU work regardless of mode (hardware-ish costs that the host does not reproduce, e.g. PSRAM copies). */
void sim_charge_ns_always(uint64_t ns);
/** Deterministic PRNG (xorshift), seeded from g_sim.seed. ISR-safe. */
uint32_t sim_rand(void);
double sim_rand01(void);

/* ---- host work in task context ----
 * The stack estimate measures what a task's code uses on its host stack. Simulator work that the
 * device does not do (host heap for the simulator's own objects, host files and directories,
 * metrics, traces, messages) goes through vtk_host_call, which runs it outside that part: the host
 * allocator and libc then neither inflate the estimate nor make it vary between runs. ISR-safe. */
void *sim_host_malloc(size_t size);
void *sim_host_calloc(size_t n, size_t size);
void sim_host_free(void *p);
/** Bounded string copy without the host's printf machinery (names on hot paths). */
void sim_copy_name(char *dst, size_t n, const char *src);

/* ---- metrics (ISR-safe) ---- */
void sim_metric_sample(const char *name, double v); /* distribution */
void sim_metric_add(const char *name, double v);    /* counter */
void sim_metric_max(const char *name, double v);    /* max gauge */
void sim_metric_min(const char *name, double v);    /* min gauge */
void sim_metric_set(const char *name, double v);    /* gauge */
/** Warning: printed, counted in "warnings.count", listed in the report. */
void sim_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void sim_note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/** Write metrics JSON; evaluate budgets. Returns number of violated budgets. */
int sim_metrics_write(const char *path, const char *budget_path, const char *status);
void sim_metrics_print_summary(void);

/* ---- trace (ISR-safe; no-ops unless enabled) ---- */
void sim_trace_open(const char *path);
void sim_trace_close(void);
bool sim_trace_enabled(void);
/** Complete span on a named track (track = e.g. "display", "flash", "input"). */
void sim_trace_span(const char *track, const char *name, uint64_t t0_ns, uint64_t t1_ns, const char *args_json);
void sim_trace_instant(const char *track, const char *name, uint64_t t_ns, const char *args_json);
void sim_trace_counter(const char *name, uint64_t t_ns, double value);
void sim_trace_switch_hook(int core, const vtk_task_t *t, bool running, uint64_t t_ns);

/* ---- OSAL sim internals ---- */
void sim_osal_init(void);
void sim_osal_report(void);           /* per-task stats into metrics */
const char *sim_current_owner(void);  /* mem tag or task name, for attribution */
bool sim_task_stack_external(void);   /* current task's stack is in PSRAM (task context) */

/* ---- device models ---- */
void sim_display_init(void);          /* task context (boot) */
void sim_display_report(void);
/** Save the current physical panel image (logical orientation) as PNG. */
void sim_display_snapshot(const char *name);
/** Mean absolute residual ghosting over the panel (0..255), for budgets. */
double sim_display_ghost_level(void);
/* Desktop simulator (ISR context or host work): change counter, refresh in progress, and a copy of
 * the visible image in logical orientation (cap >= panel pixels; false before the display exists). */
uint32_t sim_display_version(void);
bool sim_display_busy(void);
bool sim_display_visible(uint8_t *out, size_t cap, int *lw, int *lh);

void sim_input_init(void);            /* task context (boot) */
/* Physical actions in logical (UI-orientation) coordinates; called from script events (ISR ctx). */
void sim_input_touch(int id, bool down, int lx, int ly);
void sim_input_key(hal_key_t key, bool pressed);
void sim_input_report(void);

void sim_storage_init(void);          /* task context (boot): mounts /user, prepares SD dirs */
void sim_storage_report(void);
/** Power cut now (ISR ctx): tear any in-flight flash operation and save the image. */
void sim_storage_power_cut(void);
/** Cut power in the middle of the n-th flash program/erase that starts from now (ISR ctx). */
void sim_storage_arm_power_cut(int nth_write);
void sim_storage_shutdown(void);      /* clean shutdown: save image */

/* Mapped-asset D-cache model (hal_assets_sim.c) */
void sim_assets_cache_pollute(void);  /* a refresh streamed PSRAM through the shared D-cache */
void sim_assets_report(void);

void sim_power_init(void);
void sim_power_report(void);
void sim_power_set_battery(int percent, bool charging);
/** Track a power consumer: name, current in mA while on. ISR-safe. */
void sim_power_consumer(const char *name, bool on, double ma);
typedef struct {
    int percent, millivolts;
    bool charging;
    int light_cold, light_warm; /* front light, permille */
    int state;                  /* hal_power_state_t */
    bool sleeping;              /* light sleep */
    unsigned haptic_pulses;     /* vibration pulses so far */
    bool haptic_on;             /* motor running now */
} sim_power_gui_t;
void sim_power_gui_state(sim_power_gui_t *out); /* ISR context (desktop simulator) */

/* Shared I2C bus model (touch, PMIC, fuel gauge, RTC). Task context. */
void sim_i2c_init(void);
void sim_i2c_transfer(const char *who, int bytes);       /* blocks for the bus + transfer time */
void sim_i2c_hold(const char *who, uint64_t ns);         /* occupy the bus (e.g. PMIC power-up sequence) */

/* Register-level chip models answering hal_i2c transfers (hal_i2c_sim.c, dev_*.c) */
typedef struct {
    /* One transfer: write wlen bytes, then (repeated start) read rlen bytes. HAL_I2C_OK or HAL_I2C_NACK. */
    int (*xfer)(void *state, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen);
} sim_i2c_model_t;
void sim_i2c_attach(int bus, uint8_t addr7, const sim_i2c_model_t *m, void *state);
void sim_i2c_set_faults(double timeout_rate); /* --i2c-faults: share of hal_i2c transfers that time out */

/* Panel power chips of the Plus (dev_epd_power.c): TCA9555 port 0 drives the TPS65185 pins */
void sim_epd_chips_init(void);
bool sim_tps65185_panel_ready(void); /* rails good and VCOM driven; checks VCOM against the panel's */
void sim_tps65185_gate(bool oe);     /* EPD XOE level, for the power-down order check */
void sim_tps65185_report(void);

/* ---- audio (I2S rings) + stand-in codec ---- */
void sim_display_plan_info(const hal_rect_t *area, uint64_t *phase_ns, uint64_t *busy0_ns, uint64_t *busy1_ns,
                           double *slack_lines); /* task context */
bool sim_display_playing(void);
uint64_t sim_display_ipc_wait_ns(uint64_t now); /* task context */
bool sim_power_sleeping(void);
bool sim_power_waking(void);  /* asleep, but a wake source already fired */
void sim_power_input_lost(void);
void sim_input_set_power_state(hal_power_state_t s);
bool sim_power_wake(uint32_t cause); /* ISR: a wake source fired; true if it woke the chip */
hal_power_state_t sim_power_state(void);
bool sim_audio_busy(void);
void sim_audio_init(void);            /* task context (boot) */
void sim_audio_report(void);
void sim_audio_mic_source(const char *name); /* ISR (script): "speech" | "silence" | "noise" */
void sim_audio_speaker_counters(uint64_t *underruns, double *underrun_ms, double *idle_ms); /* self-test */
/** Build a stand-in Opus packet (level, pitch) the simulated decoder turns into a tone. */
int sim_opus_fake_packet(uint8_t *out, int cap, double freq, double level, int bytes, uint32_t seed);

/* ---- network + scripted hub ---- */
struct hal_ws;
void sim_net_init(void);              /* task context (boot) */
void sim_net_report(void);
void sim_net_set_link(const char *name); /* ISR (script): "lte" | "wifi" | "none" */
void sim_net_outage(uint64_t ns);        /* ISR (script) */
const char *sim_net_link(bool *up);      /* ISR: "lte" | "wifi" | "none" */
typedef struct {
    bool modem;
    int selected;                        /* slot setting: 0 external card slot, 1 internal card */
    int active;                          /* slot whose card the radio uses */
    bool radio;
    bool present[2], sms[2], voice[2];   /* each slot's card */
    bool registered, searching, volte;
} sim_cell_state_t;
void sim_net_cell_state(sim_cell_state_t *out);                    /* ISR */
void sim_net_set_card(int slot, bool present, bool sms, bool voice); /* ISR (script, window) or locked */
void sim_net_hub_send(struct hal_ws *ws, uint64_t t_send, const void *data, size_t len, bool binary); /* ISR */
int sim_hub_load(const char *path, char *err, size_t errlen);
void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary); /* ISR */

/* ---- script ---- */
int sim_script_load(const char *path, char *err, size_t errlen);
void sim_script_start(void);          /* schedule script events (before vtk_run) */
bool sim_script_has_end(void);

/* ---- runner ---- */
/* Desktop simulator bridge (sim_gui.c), called by the runner */
void sim_gui_started(void);
void sim_gui_finished(void);
void sim_gui_crashed(const char *reason) __attribute__((noreturn));
void sim_fatal(const char *reason) __attribute__((noreturn));
/** Entry point of the simulated system (provided by the system/app under test). */
void ink_main(void);

#endif
