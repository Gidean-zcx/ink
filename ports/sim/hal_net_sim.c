/*
 * Network model: Wi-Fi / LTE link to the hub plus WebSocket connections.
 *
 * Timing (virtual time, profile values):
 *   connect   [radio resume] + DNS (1 RTT, first time) + TCP (1 RTT)
 *             + TLS 1.3 (1 RTT + handshake CPU) + WebSocket upgrade (1 RTT)
 *   messages  serialisation at the link rate (LTE: limited by the UART link)
 *             + one-way latency sampled from a log-normal fitted to p50/p99
 *             + retransmission delay on loss; uplink and downlink queue separately
 *   radio     after resume_after_s of silence the radio is idle: the next
 *             packet in either direction pays resume_extra_ms; it stays in its
 *             high-power state tail_s after the last packet (energy)
 *   outages   scripted ("outage 5s") or random per the profile; a dead
 *             connection is only noticed after dead_detect_ms
 * CPU: every byte costs "net.byte" cycles (lwIP + TLS + framing) in the
 * connection task; the handshake costs "tls.handshake". Memory: TLS buffers
 * and the connection task's stack come out of the device heaps.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/hal_power.h"
#include "sim.h"

typedef struct sim_msg {
    struct sim_msg *next;
    uint64_t at;
    bool binary;
    size_t len;
    uint8_t data[];
} sim_msg_t;

struct hal_ws {
    char url[160];
    hal_ws_cb cb;
    void *ctx;
    int state; /* 0 connecting, 1 open, 2 closed */
    void *tls_int, *tls_ext;
    sim_msg_t *rx_head, *rx_tail;
    sim_msg_t *tx_head, *tx_tail;
    size_t tx_bytes;
    uint64_t gen;
    vtk_waitq_t wq, txwq;
    bool close_req, dead, freed_by_user, task_done;
    uint64_t dead_since;
    uint64_t down_last, up_last; /* latest arrival scheduled in each direction (TCP delivers in order) */
    osal_task_t *task;
    struct hal_ws *next;
};

#define TX_BUFFER_BYTES 16384
#define CONNECT_TIMEOUT_NS SIM_MS(15000)

static struct {
    hal_link_type_t type;
    bool up;
    bool want;                   /* the network is there (coverage, no outage); LTE also needs a registered card */
    const profile_link_t *L;
    uint64_t up_free, down_free; /* link busy until */
    uint64_t last_traffic;
    bool radio_active;
    vtk_event_id_t tail_ev;
    bool dns_cached;
    hal_link_cb link_cb;
    void *link_ctx;
    vtk_waitq_t evt_wq;
    bool link_changed;
    struct hal_ws *conns;
    uint64_t gen;
    /* stats */
    uint64_t bytes_up, bytes_down, msgs_up, msgs_down, connects, connect_fail, resumes;
} N;

/* ------------------------------------------------------------- helpers */

static double gauss(void) { /* Box-Muller on the deterministic PRNG */
    double u1 = sim_rand01(), u2 = sim_rand01();
    if (u1 < 1e-9) u1 = 1e-9;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

static uint64_t rtt_ns(void) {
    const profile_link_t *L = N.L;
    double sigma = L->rtt_p99_ms > L->rtt_p50_ms ? log(L->rtt_p99_ms / L->rtt_p50_ms) / 2.326 : 0;
    double ms = L->rtt_p50_ms * exp(gauss() * sigma);
    if (ms > L->rtt_p99_ms * 3) ms = L->rtt_p99_ms * 3;
    return (uint64_t)(ms * 1e6);
}

static uint64_t xfer_ns(size_t bytes) {
    double mbps = N.L && N.L->mbps > 0 ? N.L->mbps : 1;
    return (uint64_t)((double)(bytes + 60) * 8.0 * 1000.0 / mbps); /* + TCP/IP/TLS/WS headers */
}

static uint64_t now_any(void) { return vtk_in_isr() ? vtk_isr_now() : vtk_now(); }

static const char *link_name(hal_link_type_t t) {
    return t == HAL_LINK_LTE ? "lte" : t == HAL_LINK_WIFI ? "wifi" : "none";
}

static void tail_end_cb(void *arg) { /* ISR */
    (void)arg;
    N.tail_ev = 0;
    N.radio_active = false;
    sim_power_consumer("modem", true, N.type == HAL_LINK_LTE ? g_profile.ma_lte_idle : g_profile.ma_wifi_idle);
    sim_trace_instant("net", "radio idle", vtk_isr_now(), NULL);
}

/* Radio activity at time t: returns the extra latency for resuming from idle. */
static uint64_t radio_touch(uint64_t t) {
    if (!N.L) return 0;
    uint64_t extra = 0;
    bool idle = !N.radio_active && (N.last_traffic == 0 || t - N.last_traffic >= (uint64_t)(N.L->resume_after_s * 1e9));
    if (idle) {
        extra = (uint64_t)(N.L->resume_extra_ms * 1e6);
        N.resumes++;
        sim_trace_span("net", "radio resume", t, t + extra, NULL);
    }
    N.radio_active = true;
    N.last_traffic = t + extra;
    sim_power_consumer("modem", true, N.type == HAL_LINK_LTE ? g_profile.ma_lte_active : g_profile.ma_wifi_active);
    if (N.tail_ev) vtk_event_cancel(N.tail_ev);
    N.tail_ev = vtk_event_at(N.last_traffic + (uint64_t)(N.L->tail_s * 1e9), tail_end_cb, NULL);
    return extra;
}

/* ------------------------------------------------------------ link state */

static void notify_link_locked(void) {
    N.link_changed = true;
    if (vtk_in_isr()) vtk_isr_wake_one(&N.evt_wq);
    else vtk_wake_one_locked(&N.evt_wq);
}

static void wake_conns(void) {
    for (struct hal_ws *w = N.conns; w; w = w->next) {
        if (vtk_in_isr()) {
            vtk_isr_wake_one(&w->wq);
        } else {
            vtk_wake_one_locked(&w->wq);
        }
    }
}

/* ------------------------------------------------------------- cellular modem (AT level)
 * Plus: NT26 (EC718PM per the factory boot log) with an external card slot (SimSlot 0) and an
 * internal card (SimSlot 1). Only the modem is modelled here, answering the AT commands drivers
 * send through hal_modem_at; how the system switches cards is driver code (drivers/nt26).
 *  - AT+ECSIMCFG=SimSlot,n changes the slot setting, which the modem keeps across reboots (kept
 *    next to --flash-image). The card in use follows the setting when the radio next comes on
 *    (AT+CFUN=1, end of AT+ECRST): ASSUMPTION, upstream only sets the slot with the radio off.
 *  - AT+CFUN=0 drops the registration; after AT+CFUN=1 the card in use registers CELL_REG_MS later
 *    if it is there. AT+ECRST answers OK, then the modem is deaf for CELL_RESET_MS and comes back
 *    with the radio on.
 *  - Latencies are ESTIMATES within the timeouts upstream allows (CFUN=0 8 s, ECSIMCFG 5 s,
 *    CFUN=1 15 s, settings_network_tab.cc @690a023); the factory boot log registered ~2 s after
 *    the SIM check, and the modem answered ~1.5 s after AT+ECRST (uart_eth_modem.cc).
 * LTE carries data only while the card in use is present and registered. Taking out the card in
 * use drops the registration at once; a card put in is seen at the next radio-on (whether the
 * modem detects hot plug is not confirmed). */
#define CELL_AT_MS 20
#define CELL_CFUN0_MS 800
#define CELL_CFUN1_MS 500
#define CELL_SLOT_MS 200
#define CELL_REG_MS 2000
#define CELL_RESET_MS 1500

static struct {
    int selected;              /* slot setting (AT+ECSIMCFG) */
    int active;                /* slot whose card the radio uses: `selected` at the last radio-on */
    bool radio;                /* AT+CFUN=1 */
    bool present[2], sms[2], voice[2];
    bool registered;
    uint64_t deaf_until;       /* rebooting after AT+ECRST, or held down (VTK_TIME_INF) */
    vtk_event_id_t reg_ev;     /* registration pending */
    vtk_event_id_t boot_ev;    /* coming back from a reset or power-up */
    bool held, unpowered;      /* board: P12 shared reset low, P14 shared supply off */
    uint32_t slot_changes, resets;
} CELL;

static bool cell_ok(void) { return CELL.radio && CELL.registered && CELL.present[CELL.active]; }

static void set_link(hal_link_type_t t, bool up) { /* ISR or locked task context */
    hal_link_type_t old = N.type;
    bool was = N.up;
    N.type = t;
    N.want = up;
    N.up = up && t != HAL_LINK_NONE && (t != HAL_LINK_LTE || !g_profile.lte.present || cell_ok());
    N.L = t == HAL_LINK_LTE ? &g_profile.lte : t == HAL_LINK_WIFI ? &g_profile.wifi : NULL;
    if (old != t) {
        N.dns_cached = false;
        N.radio_active = false;
        N.last_traffic = 0;
    }
    if (was && (!N.up || old != t)) { /* in-flight connections are dead (noticed later) */
        for (struct hal_ws *w = N.conns; w; w = w->next)
            if (w->state != 2 && !w->dead) {
                w->dead = true;
                w->dead_since = now_any();
            }
    }
    sim_power_consumer("modem", N.up && t == HAL_LINK_LTE, g_profile.ma_lte_idle);
    if (t != HAL_LINK_LTE) sim_power_consumer("wifi", N.up && t == HAL_LINK_WIFI, g_profile.ma_wifi_idle);
    char args[64];
    snprintf(args, sizeof args, "{\"link\":\"%s\",\"up\":%s}", link_name(t), N.up ? "true" : "false");
    sim_trace_instant("net", N.up ? "link up" : "link down", now_any(), args);
    notify_link_locked();
    wake_conns();
}

static void outage_end_cb(void *arg) { /* ISR */
    hal_link_type_t t = (hal_link_type_t)(intptr_t)arg;
    set_link(t, true);
    sim_metric_add("net.outages_ended", 1);
}

void sim_net_set_link(const char *name) { /* ISR (script) */
    hal_link_type_t t = !strcmp(name, "lte") ? HAL_LINK_LTE : !strcmp(name, "wifi") ? HAL_LINK_WIFI : HAL_LINK_NONE;
    set_link(t, t != HAL_LINK_NONE);
}

const char *sim_net_link(bool *up) { /* ISR */
    if (up) *up = N.up;
    return N.type == HAL_LINK_LTE ? "lte" : N.type == HAL_LINK_WIFI ? "wifi" : "none";
}

static void cell_save_on_host(void *arg) {
    (void)arg;
    if (!g_sim.flash_image) return;
    char path[600];
    snprintf(path, sizeof path, "%s.modem", g_sim.flash_image);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SimSlot=%d\n", CELL.selected);
    fclose(f);
}

static void cell_load_on_host(void *arg) {
    (void)arg;
    if (!g_sim.flash_image) return;
    char path[600];
    snprintf(path, sizeof path, "%s.modem", g_sim.flash_image);
    FILE *f = fopen(path, "r");
    if (!f) return;
    int v;
    if (fscanf(f, "SimSlot=%d", &v) == 1 && (v == 0 || v == 1)) CELL.selected = v;
    fclose(f);
}

static void cell_relink(void) { /* ISR or locked */
    if (N.type == HAL_LINK_LTE) set_link(HAL_LINK_LTE, N.want);
}

static void cell_registered_cb(void *arg) { /* ISR: the card in use attached, or there is none */
    (void)arg;
    CELL.reg_ev = 0;
    CELL.registered = CELL.radio && CELL.present[CELL.active];
    sim_trace_instant("net", CELL.registered ? "lte registered" : "lte: no card", vtk_isr_now(), NULL);
    cell_relink();
}

static void cell_radio(bool on, uint64_t at) { /* ISR or locked */
    if (CELL.reg_ev) vtk_event_cancel(CELL.reg_ev);
    CELL.reg_ev = 0;
    CELL.radio = on;
    CELL.registered = false;
    if (on) {
        CELL.active = CELL.selected;
        CELL.reg_ev = vtk_event_at(at + SIM_MS(CELL_REG_MS), cell_registered_cb, NULL);
    }
    cell_relink();
}

static void cell_reset_done_cb(void *arg) { /* ISR: the modem rebooted, radio on */
    (void)arg;
    CELL.boot_ev = 0;
    cell_radio(true, vtk_isr_now());
}

/* The board holds the modem in reset (P12 low) or cuts its supply (P14 low): deaf, radio off; when
 * both are released it boots like after AT+ECRST. Any context. */
static void modem_gate(void) {
    bool down = CELL.held || CELL.unpowered;
    uint64_t now = now_any();
    if (down && CELL.deaf_until != VTK_TIME_INF) {
        if (CELL.boot_ev) vtk_event_cancel(CELL.boot_ev);
        CELL.boot_ev = 0;
        cell_radio(false, now);
        CELL.deaf_until = VTK_TIME_INF;
    } else if (!down && CELL.deaf_until == VTK_TIME_INF) {
        CELL.deaf_until = now + SIM_MS(CELL_RESET_MS);
        CELL.resets++;
        CELL.boot_ev = vtk_event_at(CELL.deaf_until, cell_reset_done_cb, NULL);
    }
}

static void modem_board_pin(bool *pin, bool value) {
    if (!g_profile.lte.present) return;
    bool isr = vtk_in_isr();
    if (!isr) vtk_lock();
    *pin = value;
    modem_gate();
    if (!isr) vtk_unlock();
}

void sim_net_modem_hold_reset(bool held) { modem_board_pin(&CELL.held, held); }
void sim_net_modem_power(bool on) { modem_board_pin(&CELL.unpowered, !on); }
bool sim_net_modem_registered(void) { return g_profile.lte.present && cell_ok(); }

hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t resp_cap, uint32_t timeout_ms) {
    if (resp && resp_cap) resp[0] = 0;
    if (!g_profile.lte.present) return HAL_AT_NO_MODEM;
    enum { K_AT, K_CFUN_Q, K_CFUN0, K_CFUN1, K_SLOT_Q, K_SLOT_SET, K_CPIN, K_CEREG, K_RST, K_BAD } k = K_BAD;
    unsigned lat_ms = CELL_AT_MS;
    int slot = -1;
    if (!strcmp(cmd, "AT")) k = K_AT;
    else if (!strcmp(cmd, "AT+CFUN?")) k = K_CFUN_Q;
    else if (!strcmp(cmd, "AT+CFUN=0")) k = K_CFUN0, lat_ms = CELL_CFUN0_MS;
    else if (!strcmp(cmd, "AT+CFUN=1")) k = K_CFUN1, lat_ms = CELL_CFUN1_MS;
    else if (!strcmp(cmd, "AT+ECSIMCFG?")) k = K_SLOT_Q;
    else if (sscanf(cmd, "AT+ECSIMCFG=SimSlot,%d", &slot) == 1 || sscanf(cmd, "AT+ECSIMCFG=\"SimSlot\",%d", &slot) == 1)
        k = K_SLOT_SET, lat_ms = CELL_SLOT_MS;
    else if (!strcmp(cmd, "AT+CPIN?")) k = K_CPIN;
    else if (!strcmp(cmd, "AT+CEREG?")) k = K_CEREG;
    else if (!strcmp(cmd, "AT+ECRST")) k = K_RST;
    uint64_t t0 = vtk_now(), limit = t0 + SIM_MS(timeout_ms);
    uint64_t done = CELL.deaf_until == VTK_TIME_INF ? VTK_TIME_INF : (t0 > CELL.deaf_until ? t0 : CELL.deaf_until) + SIM_MS(lat_ms);
    if (done > limit) { /* deaf (in reset, unpowered, rebooting) or slower than the caller waits: lost */
        vtk_sleep_ns(limit - t0);
        return HAL_AT_TIMEOUT;
    }
    vtk_sleep_ns(done - t0);
    hal_at_result_t r = HAL_AT_OK;
    char line[48] = "";
    vtk_lock();
    switch (k) {
    case K_AT: break;
    case K_CFUN_Q: snprintf(line, sizeof line, "+CFUN: %d", CELL.radio ? 1 : 0); break;
    case K_CFUN0: cell_radio(false, vtk_now()); break;
    case K_CFUN1:
        if (!CELL.radio) cell_radio(true, vtk_now());
        break;
    case K_SLOT_Q: snprintf(line, sizeof line, "+ECSIMCFG: \"SimSlot\",%d", CELL.selected); break;
    case K_SLOT_SET:
        if (slot != 0 && slot != 1) {
            r = HAL_AT_ERROR;
        } else if (slot != CELL.selected) {
            CELL.selected = slot;
            CELL.slot_changes++;
            vtk_host_call(cell_save_on_host, NULL);
        }
        break;
    case K_CPIN: /* the card is powered only with the radio on (ASSUMPTION) */
        if (CELL.radio && CELL.present[CELL.active]) snprintf(line, sizeof line, "+CPIN: READY");
        else r = HAL_AT_ERROR;
        break;
    case K_CEREG: {
        int stat = cell_ok() ? 1 : CELL.radio && CELL.present[CELL.active] && CELL.reg_ev ? 2 : 0;
        snprintf(line, sizeof line, "+CEREG: 0,%d", stat);
        break;
    }
    case K_RST:
        cell_radio(false, vtk_now());
        CELL.deaf_until = vtk_now() + SIM_MS(CELL_RESET_MS);
        CELL.resets++;
        CELL.boot_ev = vtk_event_at(CELL.deaf_until, cell_reset_done_cb, NULL);
        break;
    case K_BAD: r = HAL_AT_ERROR; break;
    }
    vtk_unlock();
    if (resp && resp_cap) snprintf(resp, resp_cap, "%s", line);
    return r;
}

void sim_net_cell_state(sim_cell_state_t *out) { /* ISR */
    memset(out, 0, sizeof *out);
    out->modem = g_profile.lte.present;
    out->selected = CELL.selected;
    out->active = CELL.active;
    out->radio = CELL.radio;
    for (int i = 0; i < 2; i++) {
        out->present[i] = CELL.present[i];
        out->sms[i] = CELL.sms[i];
        out->voice[i] = CELL.voice[i];
    }
    out->registered = cell_ok();
    out->searching = CELL.radio && CELL.present[CELL.active] && CELL.reg_ev;
    out->volte = out->registered && CELL.voice[CELL.active];
}

void sim_net_set_card(int slot, bool present, bool sms, bool voice) { /* ISR (script, window) or locked */
    if (slot < 0 || slot > 1) return;
    bool was = CELL.present[slot];
    CELL.present[slot] = present;
    CELL.sms[slot] = present && sms;
    CELL.voice[slot] = present && voice;
    if (slot == CELL.active && was && !present) {
        CELL.registered = false; /* stays off until the next radio-on, even if the card comes back */
        cell_relink();
    }
}

void sim_net_outage(uint64_t ns) { /* ISR (script) */
    hal_link_type_t t = N.type;
    if (t == HAL_LINK_NONE) return;
    set_link(t, false);
    sim_metric_add("net.outages", 1);
    vtk_event_after(ns, outage_end_cb, (void *)(intptr_t)t);
}

void hal_net_link_status(hal_link_status_t *out) {
    out->type = N.type;
    out->up = N.up;
    out->signal_dbm = N.type == HAL_LINK_LTE ? -100 : N.type == HAL_LINK_WIFI ? -60 : 0;
}

void hal_net_set_link_callback(hal_link_cb cb, void *ctx) {
    N.link_cb = cb;
    N.link_ctx = ctx;
}

/* System event loop task (esp_event "sys_evt" on the device): delivers link changes. */
static void sys_evt_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        while (!N.link_changed) vtk_block_locked(&N.evt_wq, VTK_TIME_INF);
        N.link_changed = false;
        vtk_unlock();
        sim_charge_ns_always(SIM_US(40));
        if (N.link_cb) {
            hal_link_status_t st;
            hal_net_link_status(&st);
            N.link_cb(N.link_ctx, &st);
        }
    }
}

/* ------------------------------------------------------------- messages */

typedef struct {
    struct hal_ws *ws;
    uint64_t gen;
    sim_msg_t *m;
} arrival_t;

static void deliver_cb(void *arg) { /* ISR */
    arrival_t *a = arg;
    struct hal_ws *w = a->ws;
    if (w->gen != a->gen || w->state != 1 || w->dead) {
        sim_host_free(a->m);
        sim_host_free(a);
        sim_metric_add("net.dropped_downlink", 1);
        return;
    }
    a->m->next = NULL;
    if (w->rx_tail) w->rx_tail->next = a->m;
    else w->rx_head = a->m;
    w->rx_tail = a->m;
    vtk_isr_wake_one(&w->wq);
    sim_host_free(a);
}

/* The hub sends `len` bytes at server time t_send (ISR context, from sim_hub). */
void sim_net_hub_send(struct hal_ws *w, uint64_t t_send, const void *data, size_t len, bool binary) {
    if (!w || w->state != 1 || w->dead || !N.up) {
        sim_metric_add("net.dropped_downlink", 1);
        return;
    }
    uint64_t t = t_send > N.down_free ? t_send : N.down_free;
    t += radio_touch(t);
    uint64_t x = xfer_ns(len);
    N.down_free = t + x;
    uint64_t arrive = t + x + rtt_ns() / 2;
    if (N.L && sim_rand01() < N.L->loss) {
        arrive += SIM_MS(300) + rtt_ns(); /* TCP retransmission */
        sim_metric_add("net.retransmissions", 1);
    }
    /* one TCP stream: nothing overtakes an earlier message (a retransmission holds back what follows) */
    if (arrive < w->down_last) arrive = w->down_last;
    w->down_last = arrive;
    sim_msg_t *m = sim_host_malloc(sizeof *m + len + 1);
    if (!m) abort();
    m->binary = binary;
    m->len = len;
    memcpy(m->data, data, len);
    m->data[len] = 0;
    arrival_t *a = sim_host_malloc(sizeof *a);
    if (!a) abort();
    a->ws = w;
    a->gen = w->gen;
    a->m = m;
    vtk_event_at(arrive, deliver_cb, a);
    N.bytes_down += len;
    N.msgs_down++;
}

typedef struct {
    struct hal_ws *ws;
    uint64_t gen;
    sim_msg_t *m;
} uplink_t;

static void hub_arrive_cb(void *arg) { /* ISR: an uplink message reached the hub */
    uplink_t *u = arg;
    if (u->ws->gen == u->gen && u->ws->state == 1 && !u->ws->dead && N.up)
        sim_hub_receive(u->ws, vtk_isr_now(), u->m->data, u->m->len, u->m->binary);
    else
        sim_metric_add("net.dropped_uplink", 1);
    sim_host_free(u->m);
    sim_host_free(u);
}

/* --------------------------------------------------------- connection task */

static bool wait_until(struct hal_ws *w, uint64_t t) { /* false if closed/dead meanwhile */
    for (;;) {
        uint64_t now = vtk_now();
        if (now >= t) return true;
        vtk_sleep_ns(t - now);
        if (w->close_req) return false;
    }
}

static void close_now(struct hal_ws *w, const char *why) {
    if (w->state == 2) return;
    w->state = 2;
    sim_trace_instant("net", why, vtk_now(), NULL);
    vtk_lock();
    vtk_wake_all_locked(&w->txwq);
    vtk_unlock();
    if (!w->close_req) w->cb(w->ctx, HAL_WS_CLOSED, NULL, 0);
}

static void ws_task(void *arg) {
    struct hal_ws *w = arg;
    uint64_t t0 = vtk_now(), deadline = t0 + CONNECT_TIMEOUT_NS;
    N.connects++;
    /* wait for a link */
    vtk_lock();
    while (!N.up && !w->close_req && vtk_now() < deadline) vtk_block_locked(&w->wq, deadline - vtk_now());
    bool ok = N.up && !w->close_req;
    vtk_unlock();
    if (ok) {
        uint64_t t = vtk_now();
        vtk_lock();
        t += radio_touch(t);
        vtk_unlock();
        if (!N.dns_cached) t += rtt_ns();
        t += rtt_ns(); /* TCP */
        ok = wait_until(w, t);
        if (ok) {
            static hal_cpu_boost_t *boost;
            if (!boost) boost = hal_cpu_boost_create("tls");
            hal_cpu_boost_acquire(boost); /* the TLS stack holds the CPU at full speed for the handshake */
            sim_charge_cycles(profile_cost("tls.handshake")); /* key exchange + certificate check */
            hal_cpu_boost_release(boost);
            t = vtk_now() + rtt_ns() /* TLS 1.3 */ + rtt_ns() /* WebSocket upgrade */;
            ok = wait_until(w, t) && N.up && !w->dead;
        }
        if (ok) N.dns_cached = true;
    }
    if (!ok) {
        N.connect_fail++;
        close_now(w, "connect failed");
    } else {
        w->state = 1;
        sim_metric_sample("net.connect_ms", (vtk_now() - t0) / 1e6);
        sim_trace_span("net", "connect", t0, vtk_now(), NULL);
        w->cb(w->ctx, HAL_WS_CONNECTED, NULL, 0);
    }
    while (w->state == 1) {
        vtk_lock();
        while (!w->rx_head && !w->tx_head && !w->close_req && !w->dead) vtk_block_locked(&w->wq, VTK_TIME_INF);
        vtk_unlock();
        if (w->close_req) {
            close_now(w, "closed by device");
            break;
        }
        if (w->dead) { /* noticed only after the keepalive/ping timeout */
            uint64_t noticed = w->dead_since + SIM_MS(g_profile.dead_detect_ms);
            if (vtk_now() < noticed) vtk_sleep_ns(noticed - vtk_now());
            close_now(w, "connection lost");
            break;
        }
        if (w->rx_head) {
            sim_msg_t *m = w->rx_head;
            w->rx_head = m->next;
            if (!w->rx_head) w->rx_tail = NULL;
            sim_charge_cycles(profile_cost("net.byte") * (double)m->len);
            w->cb(w->ctx, m->binary ? HAL_WS_BINARY : HAL_WS_TEXT, m->data, m->len);
            sim_host_free(m);
            continue;
        }
        if (w->tx_head) {
            sim_msg_t *m = w->tx_head;
            w->tx_head = m->next;
            if (!w->tx_head) w->tx_tail = NULL;
            sim_charge_cycles(profile_cost("net.byte") * (double)m->len);
            vtk_lock();
            uint64_t now = vtk_now();
            uint64_t t = now > N.up_free ? now : N.up_free;
            t += radio_touch(t);
            uint64_t x = xfer_ns(m->len);
            N.up_free = t + x;
            uint64_t arrive = t + x + rtt_ns() / 2;
            if (N.L && sim_rand01() < N.L->loss) {
                arrive += SIM_MS(300) + rtt_ns();
                sim_metric_add("net.retransmissions", 1);
            }
            if (arrive < w->up_last) arrive = w->up_last; /* in order, as above */
            w->up_last = arrive;
            uplink_t *u = sim_host_malloc(sizeof *u);
            if (!u) abort();
            u->ws = w;
            u->gen = w->gen;
            u->m = m;
            vtk_event_at(arrive, hub_arrive_cb, u);
            w->tx_bytes -= m->len;
            vtk_wake_all_locked(&w->txwq);
            vtk_unlock();
            N.bytes_up += m->len;
            N.msgs_up++;
        }
    }
    /* drop anything left */
    while (w->rx_head) {
        sim_msg_t *m = w->rx_head;
        w->rx_head = m->next;
        sim_host_free(m);
    }
    while (w->tx_head) {
        sim_msg_t *m = w->tx_head;
        w->tx_head = m->next;
        sim_host_free(m);
    }
    w->rx_tail = w->tx_tail = NULL;
    w->task_done = true;
    /* the struct itself is host memory and is never freed: link events may still refer to it */
}

/* ------------------------------------------------------------------ API */

hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_ws_open must be called from a task");
    struct hal_ws *w = sim_host_calloc(1, sizeof *w);
    if (!w) abort();
    snprintf(w->url, sizeof w->url, "%s", url);
    w->cb = cb;
    w->ctx = ctx;
    w->gen = ++N.gen;
    const char *prev = osal_mem_tag("tls");
    if (g_profile.tls_internal_bytes) w->tls_int = osal_malloc(g_profile.tls_internal_bytes, OSAL_MEM_INTERNAL);
    if (g_profile.tls_external_bytes) w->tls_ext = osal_malloc(g_profile.tls_external_bytes, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if ((g_profile.tls_internal_bytes && !w->tls_int) || (g_profile.tls_external_bytes && !w->tls_ext)) {
        sim_warn("hal_ws_open: no memory for the TLS session (%zu internal + %zu external bytes)",
                 g_profile.tls_internal_bytes, g_profile.tls_external_bytes);
        osal_free(w->tls_int);
        osal_free(w->tls_ext);
        sim_host_free(w);
        return NULL;
    }
    /* esp_websocket_client: CONFIG_ESP_WS_CLIENT_TASK_STACK_IN_EXT_RAM puts the stack in PSRAM (TCB stays internal) */
    osal_task_cfg_t cfg = {.name = "ws", .fn = ws_task, .arg = w, .stack_bytes = g_profile.ws_stack, .priority = 5, .core = -1,
                           .flags = g_profile.ws_stack_external ? OSAL_TASK_STACK_EXTERNAL : 0u};
    if (osal_task_create(&cfg, &w->task) != OSAL_OK) {
        osal_free(w->tls_int);
        osal_free(w->tls_ext);
        sim_host_free(w);
        return NULL;
    }
    vtk_lock();
    w->next = N.conns;
    N.conns = w;
    vtk_unlock();
    return w;
}

int hal_ws_send(hal_ws_t *w, bool binary, const void *data, size_t len, int64_t timeout_us) {
    if (!w || vtk_in_isr()) return -1;
    vtk_lock();
    uint64_t deadline = timeout_us < 0 ? VTK_TIME_INF : vtk_now() + (uint64_t)timeout_us * 1000ull;
    while (w->state == 1 && w->tx_bytes + len > TX_BUFFER_BYTES && w->tx_bytes > 0) {
        uint64_t now = vtk_now();
        if (now >= deadline || !vtk_block_locked(&w->txwq, deadline == VTK_TIME_INF ? VTK_TIME_INF : deadline - now)) {
            vtk_unlock();
            return -1;
        }
    }
    if (w->state != 1) {
        vtk_unlock();
        return -1;
    }
    sim_msg_t *m = sim_host_malloc(sizeof *m + len + 1);
    if (!m) abort();
    m->next = NULL;
    m->binary = binary;
    m->len = len;
    memcpy(m->data, data, len);
    if (w->tx_tail) w->tx_tail->next = m;
    else w->tx_head = m;
    w->tx_tail = m;
    w->tx_bytes += len;
    vtk_wake_one_locked(&w->wq);
    vtk_unlock();
    sim_charge_cycles(profile_cost("psram.copy_byte") * (double)len); /* copy into the send buffer */
    return 0;
}

void hal_ws_free(hal_ws_t *w) {
    if (!w) return;
    vtk_lock();
    w->close_req = true;
    vtk_wake_one_locked(&w->wq);
    struct hal_ws **pp = &N.conns;
    while (*pp && *pp != w) pp = &(*pp)->next;
    if (*pp) *pp = w->next;
    vtk_unlock();
    osal_free(w->tls_int);
    osal_free(w->tls_ext);
    w->tls_int = w->tls_ext = NULL;
    w->freed_by_user = true;
}

/* ------------------------------------------------------------ init/report */

static void random_outage_cb(void *arg) { /* ISR */
    (void)arg;
    if (N.up && N.L) {
        double s = N.L->outage_min_s + (N.L->outage_max_s - N.L->outage_min_s) * sim_rand01();
        sim_net_outage((uint64_t)(s * 1e9));
    }
    if (N.L && N.L->outages_per_hour > 0) {
        double mean_s = 3600.0 / N.L->outages_per_hour;
        double next = -log(1.0 - sim_rand01() * 0.999) * mean_s;
        vtk_event_after((uint64_t)(next * 1e9), random_outage_cb, NULL);
    }
}

static void idle_forever(void *arg) {
    (void)arg;
    vtk_lock();
    static vtk_waitq_t never;
    for (;;) vtk_block_locked(&never, VTK_TIME_INF);
}

void sim_net_init(void) {
    hal_link_type_t t = !strcmp(g_profile.default_link, "wifi") ? HAL_LINK_WIFI
                        : !strcmp(g_profile.default_link, "lte") ? HAL_LINK_LTE : HAL_LINK_NONE;
    CELL.selected = g_profile.sim_selected;
    for (int i = 0; i < 2; i++) {
        CELL.present[i] = g_profile.sim_card[i].present;
        CELL.sms[i] = g_profile.sim_card[i].sms;
        CELL.voice[i] = g_profile.sim_card[i].voice;
    }
    vtk_host_call(cell_load_on_host, NULL); /* the modem kept its slot setting */
    CELL.active = CELL.selected;
    CELL.radio = true;
    CELL.registered = CELL.present[CELL.active]; /* registered at boot (no boot-time delay modelled) */
    N.type = t;
    N.want = t != HAL_LINK_NONE;
    N.up = N.want && (t != HAL_LINK_LTE || !g_profile.lte.present || cell_ok());
    N.L = t == HAL_LINK_LTE ? &g_profile.lte : t == HAL_LINK_WIFI ? &g_profile.wifi : NULL;
    if (N.up) sim_power_consumer(t == HAL_LINK_LTE ? "modem" : "wifi", true,
                                 t == HAL_LINK_LTE ? g_profile.ma_lte_idle : g_profile.ma_wifi_idle);
    const char *prev = osal_mem_tag("net-driver");
    osal_task_cfg_t cfg = {.name = "sys_evt", .fn = sys_evt_task, .stack_bytes = g_profile.sys_evt_stack, .priority = 20, .core = -1};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("network event task failed");
    if (g_profile.lte.present && g_profile.uart_nic_dma_bytes) {
        /* NT26 "UART Ethernet": UHCI DMA rings (DMA-capable internal RAM) and the framing task,
         * installed while the modem is powered (upstream: 4G mode chosen at boot) */
        osal_mem_tag("uart-nic");
        if (!osal_malloc(g_profile.uart_nic_dma_bytes, OSAL_MEM_DMA)) osal_panic("4G UART NIC: no DMA memory for its rings");
        osal_task_cfg_t nic = {.name = "uart_nic", .fn = idle_forever, .stack_bytes = g_profile.uart_nic_stack, .priority = 18, .core = -1,
                               .flags = g_profile.uart_nic_stack_external ? OSAL_TASK_STACK_EXTERNAL : 0u};
        if (osal_task_create(&nic, NULL) != OSAL_OK) osal_panic("4G UART NIC task failed");
    }
    osal_mem_tag(prev);
    if (N.L && N.L->outages_per_hour > 0 && !g_sim.no_random_outages) {
        double mean_s = 3600.0 / N.L->outages_per_hour;
        vtk_event_after((uint64_t)(-log(1.0 - sim_rand01() * 0.999) * mean_s * 1e9), random_outage_cb, NULL);
    }
}

void sim_net_report(void) {
    if (g_profile.lte.present) {
        sim_metric_set("modem.slot_changes", (double)CELL.slot_changes);
        sim_metric_set("modem.resets", (double)CELL.resets);
    }
    sim_metric_set("net.bytes_up", (double)N.bytes_up);
    sim_metric_set("net.bytes_down", (double)N.bytes_down);
    sim_metric_set("net.messages_up", (double)N.msgs_up);
    sim_metric_set("net.messages_down", (double)N.msgs_down);
    sim_metric_set("net.connects", (double)N.connects);
    sim_metric_set("net.connect_failures", (double)N.connect_fail);
    sim_metric_set("net.radio_resumes", (double)N.resumes);
}
