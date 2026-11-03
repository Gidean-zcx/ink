/*
 * Bridge between the desktop simulator window (simulator/desktop/gui) and the simulation.
 *
 * The window runs on the host's main thread; the simulation runs sim_main() on another thread in
 * realtime mode. Everything here may be called from the window's thread: requests reach the
 * simulation as external events (vtk_inject_external), so simulated code still runs one piece at a
 * time, and results come back through a lock. Actions are the physical ones a person does to the
 * device (touch, keys, cable, network, power), in the same terms as the scenario scripts.
 */
#ifndef INK_SIM_GUI_H
#define INK_SIM_GUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The simulator (simulator/runtime/sim_main.c); returns its exit status. */
int sim_main(int argc, char **argv);
/** Before sim_main: interactive mode (realtime, no default end time, a panic keeps the window). */
void sim_gui_enable(void);

typedef enum {
    SIM_GUI_STARTING = 0,
    SIM_GUI_RUNNING,
    SIM_GUI_STOPPED, /* sim_main returned: end, stop, power cut, or a configuration error */
    SIM_GUI_CRASHED, /* the simulated device panicked; sim_gui_crash_reason() says why */
} sim_gui_phase_t;
sim_gui_phase_t sim_gui_phase(void);
const char *sim_gui_crash_reason(void);
/** Why the device lost power ("shutdown", power-off pulses...), or NULL. */
const char *sim_gui_off_reason(void);

typedef struct {
    unsigned seq;              /* bumped for every sample */
    uint64_t now_ns;           /* virtual time */
    int ncores;
    uint64_t core_busy_ns[4];  /* cumulative, for load over an interval */
    int battery_percent, battery_mv;
    int battery_ma, charge_ma;  /* + into the battery; from the charger */
    bool charging;
    int light_cold, light_warm; /* front light, permille (set by the system) */
    int power_state;            /* hal_power_state_t */
    bool sleeping;              /* light sleep */
    unsigned haptic_pulses;
    bool haptic_on;
    bool display_busy;          /* a refresh is being driven */
    char link[8];               /* "lte" | "wifi" | "none" */
    bool link_up;
    size_t internal_total, internal_free, internal_min_free;
    size_t external_total, external_free, external_min_free;
    /* cellular modem: slot 0 external card slot, 1 internal card */
    bool cell_modem;
    int sim_selected;           /* the modem's slot setting */
    int sim_active;             /* slot whose card the radio uses */
    bool modem_radio;
    int sim_card[2];            /* 0 none, 1 data, 2 data + SMS, 3 data + SMS + voice */
    bool sim_registered, sim_searching, volte;
    /* RTC and accelerometer */
    int64_t rtc_s;              /* UTC seconds the RTC shows */
    int accel_mg[3];            /* gravity the accelerometer reports */
    unsigned accel_motions;
    unsigned camera_captures;
    int bt_mode;                /* BH1098G mode, 0 = not set */
    bool bt_powered;
    /* calls and messages, seen from the network */
    int call;                   /* 0 idle, 1 active, 3 ringing at the far end, 4 ringing the device */
    char call_number[24];
    int sms_sent;               /* messages the device sent */
    char last_sms_number[24], last_sms_text[400];
    /* network environment and the system's choice */
    bool env_wifi, env_cell;
    int env_rssi, env_rsrp;
    int link_chosen;            /* hal_link_type_t */
} sim_gui_stats_t;

/** Ask for a fresh sample and, if the panel image changed, a new frame. At most one request is
 *  outstanding; while the simulation is paused or busy it is answered later. */
void sim_gui_poll(void);
/** Copy the newest frame (logical orientation, 8-bit gray) if it changed since the last call. */
bool sim_gui_take_frame(uint8_t *buf, size_t cap, int *w, int *h);
void sim_gui_get_stats(sim_gui_stats_t *out);

/* Physical actions. Coordinates are logical panel pixels; keys are hal_key_t values. */
void sim_gui_touch(bool down, int x, int y);
void sim_gui_key(int key, bool pressed);
void sim_gui_battery(int percent, bool charging);
void sim_gui_net(const char *link);  /* legacy: "lte" | "wifi" | "none" network appears */
void sim_gui_env(int which, bool on, int level_dbm); /* 0 Wi-Fi in range, 1 cellular coverage */
void sim_gui_outage(int ms);
void sim_gui_mic(const char *source); /* "speech" | "silence" | "noise" */
void sim_gui_sim_card(int slot, int kind); /* put a card in a slot or take it out (kind as sim_card) */
void sim_gui_orient(const char *name);     /* portrait, upside_down, landscape_left/right, face_up/down */
void sim_gui_shake(void);
/* the outside world: messages and calls to the device, the far end of a call */
void sim_gui_sms_to_device(const char *number, const char *text);
void sim_gui_call_device(const char *number);
void sim_gui_remote(bool answer); /* the far end answers / hangs up */
void sim_gui_auto_answer(bool on);
void sim_gui_snapshot(const char *name); /* <out>/snap-<name>.png */
void sim_gui_power_cut(void);         /* pull the battery: tears an in-flight flash write, stops */
void sim_gui_stop(void);              /* clean stop (saves the flash image) */
void sim_gui_set_speed(double speed); /* virtual seconds per wall second */
void sim_gui_set_paused(bool paused);
const char *sim_gui_out_dir(void);

/** Speaker samples the device played (16 kHz mono), for the computer's audio output; returns how
 *  many were copied (the rest is silence). Any thread. */
int sim_gui_audio_pull(int16_t *out, int n);
/** The computer's microphone (16 kHz mono) for the simulated mic; any thread. */
void sim_gui_mic_push(const int16_t *pcm, int n);
/** The device's camera takes its pictures from here (RGB565 high byte first, w x h); return false
 *  for the sensor's test pattern. Set before sim_main. */
typedef bool (*sim_gui_camera_fn)(uint8_t *rgb565be, int w, int h, void *ctx);
void sim_gui_set_camera_source(sim_gui_camera_fn fn, void *ctx);
/** Copy the last picture the device took (640x480 RGB565 high byte first) if it is new. */
bool sim_gui_take_picture(uint8_t *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
