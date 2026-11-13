/*
 * Internal interfaces shared by the simulator port (ports/sim). Not visible to
 * system or app code, which only sees the headers in platform/include/ink.
 *
 * Execution model reminder (see simulator/kernel/vtk/vtk.h): all simulated code — tasks and
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
#include "environment/hub_bridge.h"

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
    const char *hub_url;       /* opt-in loopback WebSocket bridge; fixed 1x realtime */
    bool no_random_outages;    /* disable the profile's random link outages */
    const char *flash_timing;  /* override the profile's flash timing: "typ" | "max" | "tail" */
    double i2c_faults;         /* share of hal_i2c transfers that time out (fault injection) */
    const char *i2c_fault_dev; /* only this device ("pmic", "touch"...), NULL = all */
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
double sim_metric_value(const char *name);         /* so far: a counter's or gauge's value, a distribution's count; 0 if none (tests) */
double sim_metric_last(const char *name);          /* a distribution's last sample (a counter's or gauge's value); 0 if none (tests) */
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
uint32_t sim_display_phase_seq(void); /* bumped for every waveform phase (self-tests) */
void sim_display_stall_next_scan(uint64_t ns); /* self-test: flash stall right after the next scan starts */
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
void sim_sd_set_present(bool present); /* ISR (script): the card taken out, put in */
void sim_sd_set_free(int64_t bytes);   /* ISR (script): what the card still takes; < 0: no limit */
void sim_sd_lend(bool lent, bool dirty); /* the USB disk model: unmounted while lent; dirty: the next mount fails */
const char *sim_sd_root(void);         /* host directory behind /content and /cache */
/* USB to a computer (hal_usb_sim.c), ISR (script): */
void sim_usb_plug(bool on);
void sim_usb_eject(void);
void sim_usb_copy_in(const char *host_path, const char *card_path);
void sim_usb_report(void);
void sim_sys_evt(void (*fn)(void *), void *ctx); /* run fn in the platform's event task; ISR-safe; one at a time per fn */
void sim_power_set_cable(bool on);    /* ISR or locked: the cable carries power */

/* Mapped-asset D-cache model (hal_assets_sim.c) */
void sim_assets_cache_pollute(void);  /* a refresh streamed PSRAM through the shared D-cache */
void sim_assets_report(void);

void sim_power_init(void);
void sim_power_report(void);
void sim_power_set_battery(int percent, bool charging);
/** Track a power consumer: name, current in mA while on. ISR-safe. */
void sim_power_consumer(const char *name, bool on, double ma);
/* Battery model (hal_power_sim.c) read by the gauge and charger models (dev_power_chips.c) */
double sim_battery_soc(void);             /* 0..1 */
int sim_battery_mv(void);
int sim_battery_ma(void);                 /* + charging */
double sim_power_load_ma(void);
bool sim_power_vbus(void);                /* USB plugged in */
void sim_power_set_charge_ma(double ma);  /* ISR or locked */
void sim_power_chips_init(void);          /* task context (boot): gauge + charger models, charger set up */
void sim_charger_update(void);            /* ISR or locked: input, settings or charge level changed */
bool sim_board_battery(hal_battery_t *out); /* task context: through the gauge and charger drivers */
void sim_power_off(const char *reason);   /* the battery is cut (power-off pulses, shutdown): the run ends */
const char *sim_power_off_reason(void);   /* NULL while powered */
typedef struct {
    int percent, millivolts;
    bool charging;
    int battery_ma;             /* + into the battery */
    int charge_ma;              /* from the charger */
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
void sim_i2c_set_faults(double timeout_rate, const char *device); /* --i2c-faults RATE[:DEVICE] */

/* Panel power chips of the Plus (dev_epd_power.c): TCA9555 port 0 drives the TPS65185 pins */
void sim_epd_chips_init(void);
bool sim_tps65185_panel_ready(void); /* rails good and VCOM driven; checks VCOM against the panel's */
void sim_tps65185_gate(bool oe);     /* EPD XOE level, for the power-down order check */
void sim_tps65185_report(void);
void sim_tps65185_pins(bool wakeup, bool pwrup, bool vcom_ctrl); /* from the TCA9555 model */
bool sim_tps65185_rails_good(void);

/* Board IO expander (dev_tca9555.c, pins per boards/metalio_plus/metalio_board.h) */
bool sim_board_present(void);             /* this profile has the Metalio board's expander */
void sim_board_init(void);                /* task context (boot): models + metalio_board_init() */
void sim_tca9555_init(void);
void sim_tca9555_report(void);
void sim_tca_input(int port, uint8_t mask, uint8_t value); /* physical input levels (keys, RTC INT); ISR or locked */
bool sim_tca_int_low(void);
void sim_tca_reeval(void);                /* ISR or locked: an input changed by itself (PWR_GOOD) */
bool sim_board_motor_on(void);
bool sim_board_amp_on(void);
bool sim_board_shared_reset_held(void);   /* P12 low: touch controller and modem in reset */
bool sim_board_cam_power(void);           /* P05: camera / screen power */
bool sim_board_shared_power(void);        /* P14: 4G + Bluetooth audio + amplifier supply */
uint8_t sim_tca_config(int port);         /* the expander's direction register (1 = input) */
uint8_t sim_tca_levels(int port);         /* the levels on its pins */
/* GC2145 camera (dev_camera.c). The window may give it pictures (RGB565, high byte first). */
typedef bool (*sim_camera_source_fn)(uint8_t *rgb565be, int w, int h, void *ctx);
void sim_camera_set_source(sim_camera_source_fn fn, void *ctx); /* before sim_main */
void sim_camera_init(void);               /* task context (boot) */
void sim_camera_power_lost(void);         /* board: P05 or P12 fell */
bool sim_camera_last(uint8_t *buf, size_t cap, uint32_t *seq); /* the last picture taken, if newer than *seq */
unsigned sim_camera_captures(void);
bool sim_camera_busy(void);               /* a capture is in progress */
/* BH1098G Bluetooth audio chip on UART1 (dev_bt_audio.c) */
void sim_bt_audio_init(void);             /* task context (boot) */
void sim_bt_audio_power(bool on);         /* board P14; any context */
void sim_bt_audio_report(void);
int sim_bt_audio_mode(void);              /* 0 = not set since power-up */
bool sim_bt_audio_clock(void);            /* it drives the I2S clocks (powered, booted, a mode set) */
bool sim_bt_audio_powered(void);
void sim_gpio2_update(void);              /* GPIO2 = expander INT AND accelerometer INT1 (wired); any context */
bool sim_gpio2_low(void);
/* RTC + accelerometer models (dev_rtc_accel.c) and their HALs (hal_sensors_sim.c) */
void sim_rtc_accel_init(void);
void sim_rtc_accel_report(void);
void sim_sensors_init(void);              /* task context (boot) */
void sim_sensors_service_gpio2(void);     /* task context: read the accelerometer's latch on GPIO2 */
void sim_sensors_sleep(void);             /* light sleep: accelerometer quiet, INT1 released; task context */
bool sim_sensors_sleep_watch(void);       /* light sleep with HAL_WAKE_MOTION: low-power motion watch on INT1 */
bool sim_sensors_take_motion(void);       /* after a GPIO2 wake: the accelerometer's latch (read and cleared) */
void sim_sensors_resume(void);            /* after the wake: set up again; task context */
void sim_rtc_lose_time(void);             /* ISR (script) */
void sim_rtc_line_stuck(void);            /* ISR (script): P16 held at its active level all the time */
int64_t sim_rtc_now_s(void);
unsigned sim_rtc_alarms(void);
bool sim_accel_int_low(void);
unsigned sim_accel_motions(void);
void sim_accel_orient(const char *name);  /* ISR (script, window): portrait, upside_down, landscape_left/right, face_up/down, reading */
void sim_accel_shake(void);               /* ISR (script, window) */
void sim_accel_orientation(int g[3]);

/* ---- audio (I2S rings) + stand-in codec ---- */
void sim_display_plan_info(const hal_rect_t *area, uint64_t *phase_ns, uint64_t *busy0_ns, uint64_t *busy1_ns,
                           double *slack_lines); /* task context */
bool sim_display_playing(void);
void sim_display_supply_low(void); /* PWR_GOOD read low while serving GPIO2 (input task) */
uint64_t sim_display_ipc_wait_ns(uint64_t now); /* task context */
bool sim_power_sleeping(void);
int sim_power_sleeps(void);    /* light sleeps so far (tests) */
bool sim_power_waking(void);  /* asleep, but a wake source already fired */
void sim_power_input_lost(void);
void sim_input_set_power_state(hal_power_state_t s);
bool sim_power_wake(uint32_t cause); /* a direct wake line fired (ISR or locked); true if it woke the chip */
void sim_power_gpio2_low(void);      /* GPIO2 is low (ISR or locked): wakes the chip if the sleep armed it */
void sim_power_lease(bool take);     /* a peripheral starting / stopped: no sleep transition meanwhile (task) */
/* the port's light-sleep sequence (hal_power_sleep) through the other HALs; task context */
void sim_display_park(void);         /* rails down, TPS65185 to SLEEP */
void sim_input_sleep_begin(void);    /* stop reading the expander's keys */
void sim_input_sleep_end(hal_key_t woke_by, uint64_t t_wake_ns, bool keys_back); /* report the wake key; baseline */
void sim_input_restore_later(bool keys, bool rails); /* the wake's restore failed: the input task retries */
hal_power_state_t sim_power_state(void);
bool sim_audio_busy(void);
double sim_audio_played_ms(void); /* what the speaker has played so far (tests) */
double sim_audio_bt_played_ms(void); /* what the headset has played so far (tests) */
void sim_headset_add(const char *addr, const char *name); /* a Bluetooth headset, switched on, in range */
void sim_headset_power(const char *addr, bool on);
void sim_headset_range(const char *addr, bool in);
void sim_audio_init(void);            /* task context (boot) */
void sim_audio_report(void);
void sim_audio_mic_source(const char *name); /* ISR (script): "speech" | "silence" | "noise" | "wake" | "host" (window) */
int sim_audio_mic_state(uint64_t *since_ns); /* 0 quiet, 1 speaks, 2 says the wake word; since when */
void sim_audio_mic_say(const char *phrase); /* ISR (script): speech with these words */
const char *sim_audio_mic_phrase(void);     /* the words being said, or NULL */
void sim_audio_speaker_counters(uint64_t *underruns, double *underrun_ms, double *idle_ms); /* self-test */
/** Build a stand-in Opus packet (level, pitch) the simulated decoder turns into a tone. */
int sim_opus_fake_packet(uint8_t *out, int cap, double freq, double level, int bytes, uint32_t seed);

/* ---- network + scripted hub ---- */
struct hal_ws;
void sim_net_init(void);              /* task context (boot) */
void sim_net_peer_key(const uint8_t key[32]); /* synthetic TLS peer, event context */
int sim_provision_submit(const char *password, const char *json); /* AP page, event context */
int sim_provision_status(void);
bool sim_provision_active(void);
const char *sim_provision_page(void);
enum { SIM_PAIR_DROP_BEGIN=1, SIM_PAIR_DROP_ACCEPT=2, SIM_PAIR_DROP_COMPLETE=4, SIM_PAIR_DROP_ACK=8, SIM_PAIR_DROP_HELLO=16,
       SIM_PAIR_BEGIN_ERROR=32, SIM_PAIR_ACCEPT_ERROR=64 };
void sim_hub_pair_faults(unsigned faults, int hello_error, bool wipe); /* event context */
void sim_hub_pair_expire(void);
const char *sim_hub_pair_nonce(void);
const char *sim_hub_pair_txid(void);
void sim_net_report(void);
void sim_net_set_link(const char *name); /* ISR (script): "lte" | "wifi" | "none" */
void sim_net_env_init(void); /* before the kernel runs: what is around and in the slots, from the profile */
void sim_net_outage(uint64_t ns);        /* ISR (script) */
const char *sim_net_link(bool *up);      /* ISR: "lte" | "wifi" | "none" */
void sim_net_env(int which, bool on, int level_dbm);  /* ISR: 0 Wi-Fi in range, 2 the same but it cannot be joined, 1 cellular coverage */
void sim_net_env_state(bool *wifi, int *rssi, bool *cell, int *rsrp, int *chosen);
typedef struct {
    bool modem;
    int selected;                        /* slot setting: 0 external card slot, 1 internal card */
    int active;                          /* slot whose card the radio uses */
    bool radio;
    bool airplane;                       /* the radio was turned off with AT+CFUN=4 */
    bool present[2], sms[2], voice[2];   /* each slot's card */
    bool registered, searching, volte;
} sim_cell_state_t;
void sim_net_cell_state(sim_cell_state_t *out);                    /* ISR */
void sim_net_roaming(bool on); /* ISR: the network's registration status */
void sim_net_lte_frame(bool tx, size_t bytes); /* ISR/locked: interface receives/transmits a frame */
void sim_net_set_card(int slot, bool present, bool sms, bool voice); /* ISR (script, window) or locked */
void sim_net_modem_hold_reset(bool held);  /* board P12 (shared touch + 4G reset); any context */
void sim_net_modem_power(bool on);         /* board P14 (shared 4G + BT audio + amplifier supply) */
bool sim_net_modem_registered(void);
/* The outside world for SMS and calls (window, scripts; ISR context) */
bool sim_net_deliver_sms(const char *number, const char *utf8); /* false: no SMS service or store full */
int sim_net_sms_sent(void); /* PDUs (messages and parts of long ones) the network took from the device so far (tests) */
/* One part of a long message (3GPP TS 23.040 §9.2.3.24.1): any order, again, or never; ref16 uses the 16-bit reference. */
bool sim_net_deliver_sms_part(const char *number, const char *utf8, int ref, int total, int seq, bool ref16);
void sim_net_sms_fail_part(int nth);                  /* the nth PDU the device sends from now is refused (+CMS ERROR 500) */
int sim_net_sent_pdu(int i, char *hex, size_t cap);   /* PDU i the device sent (hex, SMSC address first); returns how many */
void sim_wifi_ap_add(const char *ssid, int rssi, const char *password); /* ISR (script): a network appears */
void sim_wifi_ap_del(const char *ssid);
bool sim_net_incoming_call(const char *number);                 /* false: busy or no voice service */
void sim_net_remote_answer(void);
void sim_net_remote_hangup(void);
void sim_net_auto_answer(bool on);
int sim_net_call(char *number, size_t cap);                     /* 0 idle, 1 active, 3 ringing out, 4 ringing in */
int sim_net_call_audio(unsigned samples); /* Assumed call path; inject a frame and meter its destinations. */
const char *sim_net_call_tones(void);
int sim_net_sent_sms(int i, char *number, size_t ncap, char *text, size_t tcap); /* returns how many were sent */
void sim_net_hub_send(struct hal_ws *ws, uint64_t t_send, const void *data, size_t len, bool binary); /* ISR */
/* The hub's rendering capability "browser.session" (sim_remote.c), ISR: */
bool sim_remote_open(struct hal_ws *ws, uint64_t t, int sid, int w, int h);
/* What the hub believes the device shows in session `sid` (8-bit gray); NULL if it has no such session (tests). */
const uint8_t *sim_remote_shown(int sid, int *w, int *h);
bool sim_remote_frame(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len); /* false: not one of its sessions */
void sim_remote_close(struct hal_ws *ws, int sid);
/* The same, entering the stream at t_send (later sends by the hub: think time, speech being synthesised). ISR. */
void sim_net_hub_send_at(struct hal_ws *ws, uint64_t t_send, const void *data, size_t len, bool binary);
int sim_hub_load(const char *path, char *err, size_t errlen);
void sim_hub_clock_offset(int64_t seconds); /* ISR: the hub's clock is this far from the true time */
void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary); /* ISR */
/* Tests: a request to the device on the connection in use (ISR), and what it answered (NULL until then). */
void sim_hub_request_device(const char *method, const char *params_json);
void sim_hub_request_device_raw(const char *method, const char *params_json); /* preserve escapes; no fixture metadata */
const char *sim_hub_device_reply(void);
bool sim_hub_reply_matches_request(void);
/* Tests: the device/event messages so far (the first 32), and the last question with what came with it. */
int sim_hub_events_seen(void);
void sim_hub_event_acks(bool enabled); /* fault fixture: drop ACKs without dropping events */
const char *sim_hub_event_at(int i);
const char *sim_hub_event_audience(int i); /* connection identity at receipt, independent of event payload */
const char *sim_hub_last_ask(void);
/* Dictation (asr.stream): what is "said" (phrases separated by |); after that many frames it fails with that code
 * ("silent": it answers nothing at all). What it was opened with. */
void sim_hub_asr_script(const char *phrases, const char *fail_code, int fail_after_frames);
void sim_hub_asr_refuse(bool refuse); /* rt.open gets an error */
void sim_hub_asr_hide(bool hide);     /* not in the directory (this member may not), from the next caps/list */
const char *sim_hub_asr_params(void);

/* ---- script ---- */
int sim_script_load(const char *path, char *err, size_t errlen);
void sim_script_start(void);          /* schedule script events (before vtk_run) */
bool sim_script_has_end(void);

/* ---- runner ---- */
/* Desktop simulator bridge (sim_gui.c), called by the runner */
void sim_gui_started(void);
void sim_gui_finished(void);
void sim_gui_crashed(const char *reason) __attribute__((noreturn));
void sim_gui_audio_push(const int16_t *pcm, int n); /* speaker samples (16 kHz mono) for the window */
int sim_gui_mic_pull(int16_t *out, int n);           /* the computer's microphone (16 kHz mono), 0 if none */
void sim_fatal(const char *reason) __attribute__((noreturn));
/** Entry point of the simulated system (provided by the system/app under test). */
void ink_main(void);

/* ---- phone-3: the far end of an outgoing call that has not connected (state 3) refuses it, in event context ---- */
void sim_net_remote_busy(void);      /* the modem reports BUSY (busy, or the call was rejected) */
void sim_net_remote_no_answer(void); /* the modem reports NO ANSWER (nobody picked up) */
/* How the NT26 may report a call, unverified on the device (phone-3 review): no unsolicited +CLCC (AT+CLCC still
 * answers), BUSY or NO ANSWER as ATD's own final result, NO CARRIER ending AT+CHUP before its OK. */
enum { SIM_CALL_NO_CLCC_URC = 1, SIM_CALL_ATD_BUSY = 2, SIM_CALL_ATD_NO_ANSWER = 4, SIM_CALL_CHUP_NO_CARRIER = 8 };
void sim_net_call_quirks(unsigned quirks); /* SIM_CALL_* flags, 0 = the default model */

#endif
