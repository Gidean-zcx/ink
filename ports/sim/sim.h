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
    bool trace;               /* write Chrome/Perfetto trace JSON */
    bool frames;              /* write a PNG after every completed display update */
    bool quiet;
    bool realtime;
    double realtime_speed;
    vtk_cpu_mode_t cpu_mode;
    double host_to_target;    /* MEASURED mode factor (0 = profile default) */
    uint64_t end_ns;          /* stop time (0 = when idle / script end) */
    uint32_t seed;            /* deterministic pseudo-randomness (flash timing tails, network) */
    int64_t wall_start_us;    /* RTC time at boot */
    const char *reset_reason;
} sim_options_t;

extern sim_options_t g_sim;

/* ---- time & cost ---- */
static inline uint64_t sim_cycles_ns(double cycles) {
    return (uint64_t)(cycles * 1000.0 / (double)(g_profile.mhz > 0 ? g_profile.mhz : 1));
}
/** Charge CPU work to the calling task (task context; ignored in MEASURED mode). */
void sim_charge_cycles(double cycles);
/** Charge CPU work regardless of mode (hardware-ish costs that the host does not reproduce, e.g. PSRAM copies). */
void sim_charge_ns_always(uint64_t ns);
/** Deterministic PRNG (xorshift), seeded from g_sim.seed. ISR-safe. */
uint32_t sim_rand(void);
double sim_rand01(void);

/* ---- metrics (ISR-safe) ---- */
void sim_metric_sample(const char *name, double v); /* distribution */
void sim_metric_add(const char *name, double v);    /* counter */
void sim_metric_max(const char *name, double v);    /* max gauge */
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

/* Shared I2C bus model (touch, PMIC, fuel gauge, RTC). Task context. */
void sim_i2c_init(void);
void sim_i2c_transfer(const char *who, int bytes);       /* blocks for the bus + transfer time */
void sim_i2c_hold(const char *who, uint64_t ns);         /* occupy the bus (e.g. PMIC power-up sequence) */

/* ---- network + scripted hub ---- */
struct hal_ws;
void sim_net_init(void);              /* task context (boot) */
void sim_net_report(void);
void sim_net_set_link(const char *name); /* ISR (script): "lte" | "wifi" | "none" */
void sim_net_outage(uint64_t ns);        /* ISR (script) */
void sim_net_hub_send(struct hal_ws *ws, uint64_t t_send, const void *data, size_t len, bool binary); /* ISR */
int sim_hub_load(const char *path, char *err, size_t errlen);
void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary); /* ISR */

/* ---- script ---- */
int sim_script_load(const char *path, char *err, size_t errlen);
void sim_script_start(void);          /* schedule script events (before vtk_run) */
bool sim_script_has_end(void);

/* ---- runner ---- */
void sim_fatal(const char *reason) __attribute__((noreturn));
/** Entry point of the simulated system (provided by the system/app under test). */
void ink_main(void);

#endif
