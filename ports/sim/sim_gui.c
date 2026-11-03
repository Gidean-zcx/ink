/*
 * Desktop simulator bridge (see sim_gui.h). The window's thread only takes G.m and injects
 * external events; the events run in the simulation (ISR context) and hand results back under
 * G.m, doing their host work through vtk_host_call like the rest of the port.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sim.h"
#include "sim_gui.h"

static pthread_mutex_t Gm = PTHREAD_MUTEX_INITIALIZER;
static struct {
    sim_gui_phase_t phase;
    char crash[256];
    bool poll_pending;
    uint8_t *frame;
    size_t frame_cap;
    int fw, fh;
    uint32_t frame_seq, taken_seq;
    uint32_t display_version; /* of the image in `frame` */
    sim_gui_stats_t stats;
} G;

void sim_gui_enable(void) { g_sim.interactive = true; }

static void set_phase(sim_gui_phase_t p) {
    pthread_mutex_lock(&Gm);
    G.phase = p;
    pthread_mutex_unlock(&Gm);
}

void sim_gui_started(void) { set_phase(SIM_GUI_RUNNING); }
void sim_gui_finished(void) { set_phase(SIM_GUI_STOPPED); }

void sim_gui_crashed(const char *reason) {
    pthread_mutex_lock(&Gm);
    G.phase = SIM_GUI_CRASHED;
    snprintf(G.crash, sizeof G.crash, "%s", reason ? reason : "panic");
    pthread_mutex_unlock(&Gm);
    for (;;) pause(); /* the device is dead: keep its last image and logs on screen */
}

sim_gui_phase_t sim_gui_phase(void) {
    pthread_mutex_lock(&Gm);
    sim_gui_phase_t p = G.phase;
    pthread_mutex_unlock(&Gm);
    return p;
}

const char *sim_gui_crash_reason(void) { return G.crash; }
const char *sim_gui_off_reason(void) { return sim_power_off_reason(); }
const char *sim_gui_out_dir(void) { return g_sim.out_dir; }

/* Inject only while the kernel runs (its queue exists before vtk_run and is never drained after). */
static bool inject(vtk_event_fn fn, void *arg) {
    pthread_mutex_lock(&Gm);
    bool ok = G.phase == SIM_GUI_RUNNING;
    if (ok) vtk_inject_external(fn, arg);
    pthread_mutex_unlock(&Gm);
    return ok;
}

/* ------------------------------------------------------------------ frames and stats */

static void heap_stats(uint32_t caps, size_t *total, size_t *free_now, size_t *min_free) {
    osal_mem_stats_t m;
    osal_mem_stats(caps, &m);
    *total = m.total;
    *free_now = m.free;
    *min_free = m.min_free;
}

static void pic_on_host(void *arg);

static void poll_on_host(void *arg) {
    (void)arg;
    sim_gui_stats_t st;
    memset(&st, 0, sizeof st);
    st.now_ns = vtk_isr_now();
    st.ncores = g_profile.cores < 4 ? g_profile.cores : 4;
    for (int c = 0; c < st.ncores; c++) st.core_busy_ns[c] = vtk_core_busy_ns(c);
    sim_power_gui_t pw;
    sim_power_gui_state(&pw);
    st.battery_percent = pw.percent;
    st.battery_mv = pw.millivolts;
    st.charging = pw.charging;
    st.battery_ma = pw.battery_ma;
    st.charge_ma = pw.charge_ma;
    st.light_cold = pw.light_cold;
    st.light_warm = pw.light_warm;
    st.power_state = pw.state;
    st.sleeping = pw.sleeping;
    st.haptic_pulses = pw.haptic_pulses;
    st.haptic_on = pw.haptic_on;
    st.display_busy = sim_display_busy();
    snprintf(st.link, sizeof st.link, "%s", sim_net_link(&st.link_up));
    heap_stats(OSAL_MEM_INTERNAL, &st.internal_total, &st.internal_free, &st.internal_min_free);
    heap_stats(OSAL_MEM_EXTERNAL, &st.external_total, &st.external_free, &st.external_min_free);
    sim_cell_state_t cs;
    sim_net_cell_state(&cs);
    st.cell_modem = cs.modem;
    st.sim_selected = cs.selected;
    st.sim_active = cs.active;
    st.modem_radio = cs.radio;
    for (int i = 0; i < 2; i++) st.sim_card[i] = !cs.present[i] ? 0 : cs.voice[i] ? 3 : cs.sms[i] ? 2 : 1;
    st.sim_registered = cs.registered;
    st.sim_searching = cs.searching;
    st.volte = cs.volte;
    st.call = sim_net_call(st.call_number, sizeof st.call_number);
    sim_net_env_state(&st.env_wifi, &st.env_rssi, &st.env_cell, &st.env_rsrp, &st.link_chosen);
    st.sms_sent = sim_net_sent_sms(-1, NULL, 0, NULL, 0);
    if (st.sms_sent > 0) sim_net_sent_sms(st.sms_sent - 1, st.last_sms_number, sizeof st.last_sms_number, st.last_sms_text, sizeof st.last_sms_text);
    if (sim_board_present()) {
        st.rtc_s = sim_rtc_now_s();
        sim_accel_orientation(st.accel_mg);
        st.accel_motions = sim_accel_motions();
        st.bt_mode = sim_bt_audio_mode();
        st.bt_powered = sim_bt_audio_powered();
    }

    pthread_mutex_lock(&Gm);
    st.seq = G.stats.seq + 1;
    G.stats = st;
    uint32_t v = sim_display_version();
    size_t need = (size_t)g_profile.panel.width * (size_t)g_profile.panel.height;
    if (v != G.display_version || !G.frame_seq) {
        if (G.frame_cap < need) {
            free(G.frame);
            G.frame = malloc(need);
            G.frame_cap = G.frame ? need : 0;
        }
        int w = 0, h = 0;
        if (G.frame && sim_display_visible(G.frame, G.frame_cap, &w, &h)) {
            G.fw = w;
            G.fh = h;
            G.frame_seq++;
            G.display_version = v;
        }
    }
    if (sim_board_present()) pic_on_host(NULL);
    G.poll_pending = false;
    pthread_mutex_unlock(&Gm);
}

static void poll_event(void *arg) { vtk_host_call(poll_on_host, arg); }

void sim_gui_poll(void) {
    pthread_mutex_lock(&Gm);
    bool go = G.phase == SIM_GUI_RUNNING && !G.poll_pending;
    if (go) {
        G.poll_pending = true;
        vtk_inject_external(poll_event, NULL);
    }
    pthread_mutex_unlock(&Gm);
}

bool sim_gui_take_frame(uint8_t *buf, size_t cap, int *w, int *h) {
    pthread_mutex_lock(&Gm);
    bool fresh = G.frame_seq != G.taken_seq && G.frame && (size_t)G.fw * (size_t)G.fh <= cap;
    if (fresh) {
        memcpy(buf, G.frame, (size_t)G.fw * (size_t)G.fh);
        *w = G.fw;
        *h = G.fh;
        G.taken_seq = G.frame_seq;
    }
    pthread_mutex_unlock(&Gm);
    return fresh;
}

void sim_gui_get_stats(sim_gui_stats_t *out) {
    pthread_mutex_lock(&Gm);
    *out = G.stats;
    pthread_mutex_unlock(&Gm);
}

/* ------------------------------------------------------------------ actions */

typedef enum { C_TOUCH, C_KEY, C_BATTERY, C_NET, C_OUTAGE, C_MIC, C_SNAP, C_POWERCUT, C_SIM_CARD, C_ORIENT, C_SHAKE, C_SMS_IN, C_CALL_IN, C_REMOTE, C_AUTO_ANSWER, C_ENV } cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    int a, b, c;
    char num[24];
    char text[400];
} cmd_t;

static void free_on_host(void *p) { free(p); }

static void cmd_event(void *arg) { /* ISR context, like the script's actions */
    cmd_t *c = arg;
    switch (c->kind) {
    case C_TOUCH: sim_input_touch(0, c->a != 0, c->b, c->c); break;
    case C_KEY: sim_input_key((hal_key_t)c->a, c->b != 0); break;
    case C_BATTERY: sim_power_set_battery(c->a, c->b != 0); break;
    case C_NET: sim_net_set_link(c->text); break;
    case C_OUTAGE: sim_net_outage(SIM_MS(c->a)); break;
    case C_MIC: sim_audio_mic_source(c->text); break;
    case C_SNAP: sim_display_snapshot(c->text); break;
    case C_POWERCUT: sim_storage_power_cut(); break;
    case C_SIM_CARD: sim_net_set_card(c->a, c->b > 0, c->b >= 2, c->b == 3); break;
    case C_ORIENT: sim_accel_orient(c->text); break;
    case C_SHAKE: sim_accel_shake(); break;
    case C_SMS_IN: c->a = sim_net_deliver_sms(c->num, c->text); break;
    case C_CALL_IN: sim_net_incoming_call(c->num); break;
    case C_REMOTE:
        if (c->a) sim_net_remote_answer();
        else sim_net_remote_hangup();
        break;
    case C_AUTO_ANSWER: sim_net_auto_answer(c->a != 0); break;
    case C_ENV: sim_net_env(c->a, c->b != 0, c->c); break;
    }
    vtk_host_call(free_on_host, c);
}

static void send(cmd_kind_t kind, int a, int b, int c, const char *text) {
    cmd_t *m = calloc(1, sizeof *m);
    if (!m) return;
    m->kind = kind;
    m->a = a;
    m->b = b;
    m->c = c;
    if (text) snprintf(m->text, sizeof m->text, "%s", text);
    if (!inject(cmd_event, m)) free(m);
}

void sim_gui_touch(bool down, int x, int y) { send(C_TOUCH, down, x, y, NULL); }
void sim_gui_key(int key, bool pressed) { send(C_KEY, key, pressed, 0, NULL); }
void sim_gui_battery(int percent, bool charging) { send(C_BATTERY, percent, charging, 0, NULL); }
void sim_gui_net(const char *link) { send(C_NET, 0, 0, 0, link); }
void sim_gui_outage(int ms) { send(C_OUTAGE, ms, 0, 0, NULL); }
void sim_gui_mic(const char *source) { send(C_MIC, 0, 0, 0, source); }
void sim_gui_snapshot(const char *name) { send(C_SNAP, 0, 0, 0, name); }
void sim_gui_power_cut(void) { send(C_POWERCUT, 0, 0, 0, NULL); }
void sim_gui_sim_card(int slot, int kind) { send(C_SIM_CARD, slot, kind, 0, NULL); }
void sim_gui_orient(const char *name) { send(C_ORIENT, 0, 0, 0, name); }
void sim_gui_shake(void) { send(C_SHAKE, 0, 0, 0, NULL); }

static void send_num(cmd_kind_t kind, const char *num, const char *text) {
    cmd_t *m = calloc(1, sizeof *m);
    if (!m) return;
    m->kind = kind;
    snprintf(m->num, sizeof m->num, "%s", num ? num : "");
    snprintf(m->text, sizeof m->text, "%s", text ? text : "");
    if (!inject(cmd_event, m)) free(m);
}

void sim_gui_sms_to_device(const char *number, const char *text) { send_num(C_SMS_IN, number, text); }
void sim_gui_call_device(const char *number) { send_num(C_CALL_IN, number, NULL); }
void sim_gui_remote(bool answer) { send(C_REMOTE, answer, 0, 0, NULL); }
void sim_gui_auto_answer(bool on) { send(C_AUTO_ANSWER, on, 0, 0, NULL); }
void sim_gui_env(int which, bool on, int level) { send(C_ENV, which, on, level, NULL); }

void sim_gui_stop(void) {
    if (sim_gui_phase() == SIM_GUI_RUNNING) vtk_stop();
}

void sim_gui_set_speed(double speed) { vtk_set_realtime_speed(speed); }
void sim_gui_set_paused(bool paused) { vtk_set_paused(paused); }

/* ------------------------------------------------------------------ speaker -> computer */

#define AUDIO_RING 16000 /* one second at 16 kHz */
static struct {
    int16_t buf[AUDIO_RING];
    int head, count;
} AU;

void sim_gui_audio_push(const int16_t *pcm, int n) { /* simulation side (play tick) */
    pthread_mutex_lock(&Gm);
    for (int i = 0; i < n; i++) {
        AU.buf[(AU.head + AU.count) % AUDIO_RING] = pcm[i];
        if (AU.count < AUDIO_RING) AU.count++;
        else AU.head = (AU.head + 1) % AUDIO_RING; /* the computer fell behind: drop the oldest */
    }
    pthread_mutex_unlock(&Gm);
}

int sim_gui_audio_pull(int16_t *out, int n) {
    pthread_mutex_lock(&Gm);
    int k = n < AU.count ? n : AU.count;
    for (int i = 0; i < k; i++) out[i] = AU.buf[(AU.head + i) % AUDIO_RING];
    AU.head = (AU.head + k) % AUDIO_RING;
    AU.count -= k;
    pthread_mutex_unlock(&Gm);
    return k;
}

/* ------------------------------------------------------------------ computer microphone -> mic */

static struct {
    int16_t buf[AUDIO_RING];
    int head, count;
} MI;

void sim_gui_mic_push(const int16_t *pcm, int n) {
    pthread_mutex_lock(&Gm);
    for (int i = 0; i < n; i++) {
        MI.buf[(MI.head + MI.count) % AUDIO_RING] = pcm[i];
        if (MI.count < AUDIO_RING) MI.count++;
        else MI.head = (MI.head + 1) % AUDIO_RING;
    }
    pthread_mutex_unlock(&Gm);
}

int sim_gui_mic_pull(int16_t *out, int n) {
    pthread_mutex_lock(&Gm);
    int k = n < MI.count ? n : MI.count;
    for (int i = 0; i < k; i++) out[i] = MI.buf[(MI.head + i) % AUDIO_RING];
    MI.head = (MI.head + k) % AUDIO_RING;
    MI.count -= k;
    pthread_mutex_unlock(&Gm);
    return k;
}

/* ------------------------------------------------------------------ camera */

void sim_gui_set_camera_source(sim_gui_camera_fn fn, void *ctx) { sim_camera_set_source(fn, ctx); }

static uint8_t *g_pic;
static uint32_t g_pic_seq, g_pic_taken;

static void pic_on_host(void *arg) { /* simulation side, at a poll */
    (void)arg;
    if (!g_pic) g_pic = malloc(640 * 480 * 2);
    uint32_t seq = g_pic_seq;
    if (g_pic && sim_camera_last(g_pic, 640 * 480 * 2, &seq)) g_pic_seq = seq;
}

bool sim_gui_take_picture(uint8_t *buf, size_t cap) {
    pthread_mutex_lock(&Gm);
    bool fresh = g_pic && g_pic_seq != g_pic_taken && cap >= 640 * 480 * 2;
    if (fresh) {
        memcpy(buf, g_pic, 640 * 480 * 2);
        g_pic_taken = g_pic_seq;
    }
    pthread_mutex_unlock(&Gm);
    return fresh;
}

unsigned sim_gui_camera_captures(void) { return sim_camera_captures(); }
