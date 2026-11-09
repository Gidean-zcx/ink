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
    vtk_waitq_t wq, txwq, donewq;
    bool close_req, dead, freed_by_user, task_done;
    bool bridge_open;
    bool pinned;
    uint8_t expected_key[32], peer_key[32];
    sim_hub_bridge_t *bridge;
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
    hal_link_cb link_cb[4]; /* up to four listeners (hal_net.h) */
    void *link_ctx[4];
    int nlink;
    vtk_waitq_t evt_wq;
    bool link_changed;
    struct {
        void (*fn)(void *);
        void *ctx;
    } evt[4]; /* other work for the event task (sim_sys_evt) */
    bool started;                /* sim_net_init ran */
    struct hal_ws *conns;
    uint64_t gen;
    /* stats */
    uint64_t bytes_up, bytes_down, msgs_up, msgs_down, connects, connect_fail, resumes;
} N;

/* Synthetic SPKI digest; tests may change the peer independently of the device. */
static uint8_t g_peer_key[32] = {
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17};
void sim_net_peer_key(const uint8_t key[32]) { memcpy(g_peer_key, key, 32); }

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

void sim_sys_evt(void (*fn)(void *), void *ctx) { /* ISR or locked */
    for (int i = 0; i < 4; i++)
        if (!N.evt[i].fn || N.evt[i].fn == fn) {
            N.evt[i].fn = fn;
            N.evt[i].ctx = ctx;
            break;
        }
    if (vtk_in_isr()) vtk_isr_wake_one(&N.evt_wq);
    else vtk_wake_one_locked(&N.evt_wq);
}

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
 * send through hal_modem_at; how the system switches cards is driver code (drivers/chips/nt26).
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
    bool at_busy;              /* a command is running */
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

/* ------------------------------------------------------------- environment and the system's choice
 * The simulator only says what is around the device: a known Wi-Fi network in range (and how
 * strong), cellular coverage (and how strong), outages. Which link carries the traffic is the
 * system's choice (hal_net_use); until it chooses, the profile's default_link stands for the choice
 * it saved last time (upstream keeps it in NVS). Joining Wi-Fi takes WIFI_JOIN_MS (ESTIMATE: scan,
 * association, DHCP); LTE needs a registered card (cellular section). Signal strength is shown, not
 * yet used by the link model. */
#define WIFI_JOIN_MS 2500

static struct {
    bool wifi, cell;
    bool wifi_script, wifi_table; /* the script's "wifi on"; a known network from the table in range */
    bool wifi_broken; /* in range, but joining it never works */
    int wifi_rssi, cell_rsrp;
    bool outage;
    uint64_t joining_until;
} E;

static bool env_ok(hal_link_type_t t) {
    if (E.outage) return false;
    if (t == HAL_LINK_WIFI) return E.wifi && !E.wifi_broken && now_any() >= E.joining_until;
    if (t == HAL_LINK_LTE) return E.cell;
    return false;
}

static void relink(void) { /* ISR or locked */
    if (N.started) set_link(N.type, env_ok(N.type)); /* before sim_net_init: it starts from the environment */
}

static void outage_end_cb(void *arg) { /* ISR */
    (void)arg;
    E.outage = false;
    relink();
    sim_metric_add("net.outages_ended", 1);
}

static void joined_cb(void *arg) { /* ISR: Wi-Fi association done */
    (void)arg;
    relink();
}

void sim_net_set_link(const char *name) { /* ISR (script "net"): legacy shorthand for the environment */
    if (!strcmp(name, "none")) {
        E.wifi_script = E.cell = false;
        E.wifi = E.wifi_table;
    } else if (!strcmp(name, "wifi")) E.wifi_script = E.wifi = true;
    else if (!strcmp(name, "lte")) E.cell = true;
    relink();
}

void sim_net_env(int which, bool on, int level) { /* ISR (script, window): 0 Wi-Fi, 1 cellular */
    if (which == 0 || which == 2) {
        E.wifi_script = on;
        E.wifi = on || E.wifi_table;
        E.wifi_broken = which == 2;
        if (level) E.wifi_rssi = level;
    } else {
        E.cell = on;
        if (level) E.cell_rsrp = level;
    }
    relink();
}

/* ---- Wi-Fi management: the networks around, as the script makes them ---- */
#define MAX_APS 8
#define SCAN_US 2500000ull
#define JOIN_FAIL_US 3000000ull
static struct {
    hal_wifi_ap_t ap[MAX_APS];
    char password[MAX_APS][65];
    int n;
    hal_wifi_known_t known[8];
    int nknown;
} W;

/* A known network in range, and the strongest: the one automatic choosing joins. Locked or ISR. */
static int known_in_range(void) {
    int best = -1;
    for (int i = 0; i < W.n; i++)
        for (int k = 0; k < W.nknown; k++)
            if (!strcmp(W.ap[i].ssid, W.known[k].ssid) && !strcmp(W.password[i], W.known[k].password) && (best < 0 || W.ap[i].rssi > W.ap[best].rssi)) best = i;
    return best;
}

static void wifi_from_table(void) { /* locked or ISR */
    int i = known_in_range();
    E.wifi_table = i >= 0;
    if (i >= 0) E.wifi_rssi = W.ap[i].rssi;
    E.wifi = E.wifi_script || E.wifi_table;
    relink();
}

void sim_wifi_ap_add(const char *ssid, int rssi, const char *password) { /* ISR (script) */
    int at = -1;
    for (int i = 0; i < W.n; i++)
        if (!strcmp(W.ap[i].ssid, ssid)) at = i;
    if (at < 0 && W.n < MAX_APS) at = W.n++;
    if (at < 0) return;
    snprintf(W.ap[at].ssid, sizeof W.ap[at].ssid, "%s", ssid);
    W.ap[at].rssi = rssi;
    W.ap[at].secured = password && password[0];
    snprintf(W.password[at], sizeof W.password[at], "%s", password ? password : "");
    wifi_from_table();
}

void sim_wifi_ap_del(const char *ssid) { /* ISR (script) */
    for (int i = 0; i < W.n; i++)
        if (!strcmp(W.ap[i].ssid, ssid)) {
            memmove(&W.ap[i], &W.ap[i + 1], sizeof W.ap[0] * (size_t)(W.n - i - 1));
            memmove(&W.password[i], &W.password[i + 1], sizeof W.password[0] * (size_t)(W.n - i - 1));
            W.n--;
            break;
        }
    wifi_from_table();
}

int hal_wifi_scan(hal_wifi_ap_t *out, int max, uint32_t timeout_ms) {
    uint64_t wait = (uint64_t)timeout_ms * 1000u;
    osal_task_delay_us(wait < SCAN_US ? wait : SCAN_US);
    vtk_lock();
    int n = 0;
    for (int i = 0; i < W.n && n < max; i++) {
        int at = n++;
        out[at] = W.ap[i];
        while (at > 0 && out[at - 1].rssi < out[at].rssi) { /* strongest first */
            hal_wifi_ap_t t = out[at - 1];
            out[at - 1] = out[at];
            out[at] = t;
            at--;
        }
    }
    vtk_unlock();
    return n;
}

int hal_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) {
    osal_task_delay_us(WIFI_JOIN_MS * 1000ull < (uint64_t)timeout_ms * 1000u ? WIFI_JOIN_MS * 1000ull : (uint64_t)timeout_ms * 1000u);
    vtk_lock();
    int at = -1;
    for (int i = 0; i < W.n; i++)
        if (!strcmp(W.ap[i].ssid, ssid)) at = i;
    int r = at < 0 ? HAL_WIFI_ERR_NOT_FOUND : strcmp(W.password[at], password ? password : "") ? HAL_WIFI_ERR_AUTH : HAL_WIFI_OK;
    vtk_unlock();
    return r;
}

void hal_wifi_set_known(const hal_wifi_known_t *list, int n) {
    vtk_lock();
    W.nknown = n < 8 ? n : 8;
    if (W.nknown) memcpy(W.known, list, sizeof W.known[0] * (size_t)W.nknown);
    wifi_from_table();
    vtk_unlock();
}

void sim_net_env_state(bool *wifi, int *rssi, bool *cell, int *rsrp, int *chosen) { /* ISR */
    *wifi = E.wifi;
    *rssi = E.wifi_rssi;
    *cell = E.cell;
    *rsrp = E.cell_rsrp;
    *chosen = (int)N.type;
}

void hal_net_use(hal_link_type_t t) {
    vtk_lock();
    if (t != N.type) {
        if (t == HAL_LINK_WIFI) {
            E.joining_until = vtk_now() + SIM_MS(WIFI_JOIN_MS);
            vtk_event_at(E.joining_until, joined_cb, NULL);
        }
        set_link(t, env_ok(t)); /* from the old link to the new: connections on the old one are dead */
    }
    vtk_unlock();
}

bool hal_net_available(hal_link_type_t t) {
    vtk_lock();
    bool ok = !E.outage && (t == HAL_LINK_WIFI ? E.wifi : t == HAL_LINK_LTE && E.cell && (!g_profile.lte.present || cell_ok()));
    vtk_unlock();
    return ok;
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

static void relink(void);
static void cell_relink(void) { /* ISR or locked */
    if (N.type == HAL_LINK_LTE) relink();
    else notify_link_locked(); /* not in use, but what is around changed (hal_net_available) */
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

static void call_end(void);

/* The board holds the modem in reset (P12 low) or cuts its supply (P14 low): deaf, radio off; when
 * both are released it boots like after AT+ECRST. Any context. */
static void modem_gate(void) {
    bool down = CELL.held || CELL.unpowered;
    uint64_t now = now_any();
    if (down && CELL.deaf_until != VTK_TIME_INF) {
        if (CELL.boot_ev) vtk_event_cancel(CELL.boot_ev);
        CELL.boot_ev = 0;
        cell_radio(false, now);
        call_end();
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

/* ---- SMS, calls and unsolicited lines (3GPP TS 27.005 / 27.007 text mode) ----
 * Messages wait in the modem's store and are announced with +CMTI; AT+CMGR/AT+CMGD read and delete
 * them, in UCS2 hex after AT+CSCS="UCS2" (else as plain text). AT+CMGS takes the text after its
 * "> " prompt and answers +CMGS after SMS_SEND_MS (ESTIMATE). Calls: ATD<n>; ATA, ATH / AT+CHUP,
 * AT+CLCC; state changes are reported as +CLCC lines (stat 0 active, 2 dialling, 3 alerting,
 * 4 incoming), an incoming call rings (RING, +CLIP) every 3 s, the end is NO CARRIER. Which
 * unsolicited lines the EC718PM firmware really sends is an ASSUMPTION until its AT manual is
 * checked. SMS needs a card with SMS, calls one with voice (VoLTE), and both a registration.
 * Call audio injection below assumes a path through BH1098G; the board wiring is unverified. */
#define SMS_SEND_MS 2000
#define SMS_SLOTS 16
#define URC_SLOTS 16
#define CALL_RING_MS 3000
#define CALL_ANSWER_MS 4000  /* the far end picks up an outgoing call (auto-answer) */
#define CALL_RING_GIVEUP_MS 30000

static struct {
    bool text_mode, ucs2;
    struct { bool used, read; char number[24], text[400]; } sms[SMS_SLOTS];
    struct { char number[24], text[400]; } sent[8];
    int sent_n;
    char urc[URC_SLOTS][120];
    int urc_h, urc_n;
    vtk_waitq_t urc_wq;
    int call;        /* 0 idle, 2 dialling, 3 alerting, 4 incoming, 1 active */
    bool call_out;
    bool muted;
    char tones[65];
    char call_number[24];
    bool auto_answer;
    vtk_event_id_t call_ev;
    uint64_t ring_started;
    unsigned sms_in, sms_out, calls;
} MS = {.auto_answer = true};

static void urc_push(const char *line) { /* ISR or locked */
    if (MS.urc_n == URC_SLOTS) {
        MS.urc_h = (MS.urc_h + 1) % URC_SLOTS;
        MS.urc_n--;
    }
    snprintf(MS.urc[(MS.urc_h + MS.urc_n) % URC_SLOTS], sizeof MS.urc[0], "%s", line);
    MS.urc_n++;
    if (vtk_in_isr()) vtk_isr_wake_one(&MS.urc_wq);
    else vtk_wake_one_locked(&MS.urc_wq);
    /* the module signals data for the host on SRDY (GPIO3), a wake line when the sleep armed it
     * (ASSUMPTION: the EC718PM raises SRDY for unsolicited lines; upstream does not arm it) */
    sim_power_wake(HAL_WAKE_MODEM);
}

int hal_modem_urc(char *buf, size_t cap, uint32_t timeout_ms) {
    uint64_t deadline = vtk_now() + SIM_MS(timeout_ms);
    vtk_lock();
    while (MS.urc_n == 0) {
        uint64_t now = vtk_now();
        if (now >= deadline || !vtk_block_locked(&MS.urc_wq, deadline - now)) {
            if (MS.urc_n) break;
            vtk_unlock();
            return -1;
        }
    }
    snprintf(buf, cap, "%s", MS.urc[MS.urc_h]);
    MS.urc_h = (MS.urc_h + 1) % URC_SLOTS;
    MS.urc_n--;
    vtk_unlock();
    return (int)strlen(buf);
}

static bool can_sms(void) { return cell_ok() && CELL.sms[CELL.active]; }
static bool can_call(void) { return cell_ok() && CELL.voice[CELL.active]; }

/* text to the charset in use (UCS2 hex, BMP only, or as is) */
static void encode(const char *utf8, char *out, size_t cap) {
    if (!MS.ucs2) {
        snprintf(out, cap, "%s", utf8);
        return;
    }
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p && n + 5 < cap) {
        unsigned cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = (unsigned)(p[0] & 0x1F) << 6 | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
            cp = (unsigned)(p[0] & 0x0F) << 12 | (unsigned)(p[1] & 0x3F) << 6 | (p[2] & 0x3F);
            p += 3;
        } else {
            cp = '?';
            p++;
        }
        n += (size_t)snprintf(out + n, cap - n, "%04X", cp);
    }
    out[n] = 0;
}

static void decode(const char *in, char *utf8, size_t cap) {
    if (!MS.ucs2) {
        snprintf(utf8, cap, "%s", in);
        return;
    }
    size_t n = 0, len = strlen(in);
    for (size_t i = 0; i + 3 < len && n + 4 < cap; i += 4) {
        unsigned u = 0;
        for (int k = 0; k < 4; k++) {
            char c = in[i + (size_t)k];
            u = u << 4 | (unsigned)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
        }
        if (u < 0x80) utf8[n++] = (char)u;
        else if (u < 0x800) utf8[n++] = (char)(0xC0 | u >> 6), utf8[n++] = (char)(0x80 | (u & 0x3F));
        else utf8[n++] = (char)(0xE0 | u >> 12), utf8[n++] = (char)(0x80 | (u >> 6 & 0x3F)), utf8[n++] = (char)(0x80 | (u & 0x3F));
    }
    utf8[n] = 0;
}

static void call_report(void) { /* ISR or locked */
    char l[80];
    if (!MS.call) {
        urc_push("NO CARRIER");
        return;
    }
    snprintf(l, sizeof l, "+CLCC: 1,%d,%d,0,0,\"%s\",129", MS.call_out ? 0 : 1, MS.call == 1 ? 0 : MS.call, MS.call_number);
    urc_push(l);
}

static void call_end(void) { /* ISR or locked */
    if (MS.call_ev) vtk_event_cancel(MS.call_ev);
    MS.call_ev = 0;
    if (!MS.call) return;
    MS.call = 0;
    MS.muted = false;
    MS.tones[0] = 0;
    call_report();
}

static void call_tick(void *arg) { /* ISR: ring again, give up, or the far end answers */
    (void)arg;
    MS.call_ev = 0;
    uint64_t now = vtk_isr_now();
    if (MS.call == 4) {
        if (now - MS.ring_started >= SIM_MS(CALL_RING_GIVEUP_MS)) {
            call_end();
            return;
        }
        char l[60];
        urc_push("RING");
        snprintf(l, sizeof l, "+CLIP: \"%s\",129", MS.call_number);
        urc_push(l);
        MS.call_ev = vtk_event_at(now + SIM_MS(CALL_RING_MS), call_tick, NULL);
    } else if (MS.call == 3 && MS.auto_answer) {
        MS.call = 1;
        call_report();
    }
}

static hal_at_result_t at_run(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms);

hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t resp_cap, uint32_t timeout_ms) {
    return hal_modem_at_payload(cmd, NULL, resp, resp_cap, timeout_ms);
}

/* One command at a time: on the real link a second one would corrupt both (see hal_net.h). */
hal_at_result_t hal_modem_at_payload(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms) {
    vtk_lock();
    bool busy = CELL.at_busy;
    CELL.at_busy = true;
    vtk_unlock();
    if (busy) sim_warn("modem: %s sent while another AT command runs", cmd);
    hal_at_result_t r = at_run(cmd, payload, resp, resp_cap, timeout_ms);
    vtk_lock();
    if (!busy) CELL.at_busy = false;
    vtk_unlock();
    return r;
}

static hal_at_result_t at_run(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms) {
    if (resp && resp_cap) resp[0] = 0;
    if (!g_profile.lte.present) return HAL_AT_NO_MODEM;
    enum { K_AT, K_CFUN_Q, K_CFUN0, K_CFUN1, K_SLOT_Q, K_SLOT_SET, K_CPIN, K_CEREG, K_RST, K_CMGF, K_CSCS, K_CNMI, K_CMGS, K_CMGR,
           K_CMGD, K_ATD, K_ATA, K_CHUP, K_CLCC, K_CMUT, K_CMUT_Q, K_VTS, K_BAD } k = K_BAD;
    unsigned lat_ms = CELL_AT_MS;
    int slot = -1, idx = -1;
    char arg[100] = "";
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
    else if (!strncmp(cmd, "AT+CMGF=", 8)) k = K_CMGF;
    else if (!strncmp(cmd, "AT+CSCS=", 8)) k = K_CSCS;
    else if (!strncmp(cmd, "AT+CNMI=", 8)) k = K_CNMI;
    else if (sscanf(cmd, "AT+CMGS=\"%99[^\"]\"", arg) == 1) k = K_CMGS, lat_ms = SMS_SEND_MS;
    else if (sscanf(cmd, "AT+CMGR=%d", &idx) == 1) k = K_CMGR;
    else if (sscanf(cmd, "AT+CMGD=%d", &idx) == 1) k = K_CMGD;
    else if (sscanf(cmd, "ATD%23[^;];", arg) == 1) k = K_ATD;
    else if (!strcmp(cmd, "ATA")) k = K_ATA;
    else if (!strcmp(cmd, "ATH") || !strcmp(cmd, "AT+CHUP")) k = K_CHUP;
    else if (!strcmp(cmd, "AT+CLCC")) k = K_CLCC;
    else if (!strcmp(cmd, "AT+CMUT?")) k = K_CMUT_Q;
    else if (!strcmp(cmd, "AT+CMUT=0") || !strcmp(cmd, "AT+CMUT=1")) k = K_CMUT;
    else if (strlen(cmd) == 10 && !strncmp(cmd, "AT+VTS=\"", 8) && cmd[9] == '"' && strchr("0123456789*#ABCD", cmd[8])) k = K_VTS;
    uint64_t t0 = vtk_now(), limit = t0 + SIM_MS(timeout_ms);
    uint64_t done = CELL.deaf_until == VTK_TIME_INF ? VTK_TIME_INF : (t0 > CELL.deaf_until ? t0 : CELL.deaf_until) + SIM_MS(lat_ms);
    if (done > limit) { /* deaf (in reset, unpowered, rebooting) or slower than the caller waits: lost */
        vtk_sleep_ns(limit - t0);
        return HAL_AT_TIMEOUT;
    }
    vtk_sleep_ns(done - t0);
    hal_at_result_t r = HAL_AT_OK;
    char info[1100] = "";
    vtk_lock();
    switch (k) {
    case K_AT: break;
    case K_CFUN_Q: snprintf(info, sizeof info, "+CFUN: %d", CELL.radio ? 1 : 0); break;
    case K_CFUN0:
        cell_radio(false, vtk_now());
        call_end();
        break;
    case K_CFUN1:
        if (!CELL.radio) cell_radio(true, vtk_now());
        break;
    case K_SLOT_Q: snprintf(info, sizeof info, "+ECSIMCFG: \"SimSlot\",%d", CELL.selected); break;
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
        if (CELL.radio && CELL.present[CELL.active]) snprintf(info, sizeof info, "+CPIN: READY");
        else r = HAL_AT_ERROR;
        break;
    case K_CEREG: {
        int stat = cell_ok() ? 1 : CELL.radio && CELL.present[CELL.active] && CELL.reg_ev ? 2 : 0;
        snprintf(info, sizeof info, "+CEREG: 0,%d", stat);
        break;
    }
    case K_RST:
        cell_radio(false, vtk_now());
        call_end();
        CELL.deaf_until = vtk_now() + SIM_MS(CELL_RESET_MS);
        CELL.resets++;
        CELL.boot_ev = vtk_event_at(CELL.deaf_until, cell_reset_done_cb, NULL);
        break;
    case K_CMGF: MS.text_mode = cmd[8] == '1'; break;
    case K_CSCS: MS.ucs2 = strstr(cmd, "UCS2") != NULL; break;
    case K_CNMI: break;
    case K_CMGS:
        if (!payload || !MS.text_mode || !can_sms()) {
            snprintf(info, sizeof info, "+CMS ERROR: %d", !cell_ok() ? 331 : 500);
            r = HAL_AT_ERROR;
        } else {
            int i = MS.sent_n % 8;
            decode(arg, MS.sent[i].number, sizeof MS.sent[i].number);
            decode(payload, MS.sent[i].text, sizeof MS.sent[i].text);
            MS.sent_n++;
            MS.sms_out++;
            snprintf(info, sizeof info, "+CMGS: %d", MS.sms_out & 0xFF);
        }
        break;
    case K_CMGR:
        if (idx < 0 || idx >= SMS_SLOTS || !MS.sms[idx].used || !MS.text_mode) {
            r = HAL_AT_ERROR;
        } else {
            char num[100], text[900];
            encode(MS.sms[idx].number, num, sizeof num);
            encode(MS.sms[idx].text, text, sizeof text);
            snprintf(info, sizeof info, "+CMGR: \"%s\",\"%s\",,\"26/11/02,00:00:00+32\"\n%s", MS.sms[idx].read ? "REC READ" : "REC UNREAD", num, text);
            MS.sms[idx].read = true;
        }
        break;
    case K_CMGD:
        if (idx >= 0 && idx < SMS_SLOTS) MS.sms[idx].used = false;
        else r = HAL_AT_ERROR;
        break;
    case K_ATD:
        if (MS.call || !can_call()) {
            r = HAL_AT_ERROR;
            snprintf(info, sizeof info, "+CME ERROR: %d", !cell_ok() ? 30 : 4);
        } else {
            MS.call = 3; /* dialled, ringing at the far end */
            MS.call_out = true;
            snprintf(MS.call_number, sizeof MS.call_number, "%s", arg);
            MS.calls++;
            call_report();
            MS.call_ev = vtk_event_at(vtk_now() + SIM_MS(CALL_ANSWER_MS), call_tick, NULL);
        }
        break;
    case K_ATA:
        if (MS.call != 4) {
            r = HAL_AT_ERROR;
        } else {
            if (MS.call_ev) vtk_event_cancel(MS.call_ev);
            MS.call_ev = 0;
            MS.call = 1;
            call_report();
        }
        break;
    case K_CHUP: call_end(); break;
    case K_CMUT_Q: sim_copy_name(info, sizeof info, MS.muted ? "+CMUT: 1" : "+CMUT: 0"); break;
    case K_CMUT:
        if (MS.call != 1) r = HAL_AT_ERROR;
        else MS.muted = cmd[8] == '1';
        break;
    case K_VTS: {
        size_t n = strlen(MS.tones);
        if (MS.call != 1 || n == sizeof MS.tones - 1) r = HAL_AT_ERROR;
        else { MS.tones[n] = cmd[8]; MS.tones[n + 1] = 0; }
        break;
    }
    case K_CLCC:
        if (MS.call)
            snprintf(info, sizeof info, "+CLCC: 1,%d,%d,0,0,\"%s\",129", MS.call_out ? 0 : 1, MS.call == 1 ? 0 : MS.call, MS.call_number);
        break;
    case K_BAD: r = HAL_AT_ERROR; break;
    }
    vtk_unlock();
    if (resp && resp_cap) snprintf(resp, resp_cap, "%s", info);
    return r;
}

/* The outside world (window, scripts; ISR context) */
int sim_net_sms_sent(void) { return (int)MS.sms_out; }

bool sim_net_deliver_sms(const char *number, const char *utf8) {
    if (!can_sms()) return false;
    for (int i = 0; i < SMS_SLOTS; i++)
        if (!MS.sms[i].used) {
            MS.sms[i].used = true;
            MS.sms[i].read = false;
            snprintf(MS.sms[i].number, sizeof MS.sms[i].number, "%s", number);
            snprintf(MS.sms[i].text, sizeof MS.sms[i].text, "%s", utf8);
            MS.sms_in++;
            char l[40];
            snprintf(l, sizeof l, "+CMTI: \"SM\",%d", i);
            urc_push(l);
            return true;
        }
    return false; /* store full */
}

bool sim_net_incoming_call(const char *number) {
    if (MS.call || !can_call()) return false;
    MS.call = 4;
    MS.call_out = false;
    snprintf(MS.call_number, sizeof MS.call_number, "%s", number);
    MS.calls++;
    MS.ring_started = vtk_isr_now();
    call_report();
    call_tick(NULL); /* first RING now */
    return true;
}

void sim_net_remote_answer(void) {
    if (MS.call == 3) {
        if (MS.call_ev) vtk_event_cancel(MS.call_ev);
        MS.call_ev = 0;
        MS.call = 1;
        call_report();
    }
}

void sim_net_remote_hangup(void) { call_end(); }
void sim_net_auto_answer(bool on) { MS.auto_answer = on; }

int sim_net_call(char *number, size_t cap) {
    if (number && cap) snprintf(number, cap, "%s", MS.call_number);
    return MS.call;
}

/* Inject a call frame: bit 0 uplink, bit 1 speaker, bit 2 headset. No system policy. */
int sim_net_call_audio(unsigned samples) {
    if (MS.call != 1) return 0;
    int route = sim_bt_audio_mode() == 2 ? 4 : 2;
    sim_metric_add(MS.muted ? "cell.call.muted_samples" : "cell.call.uplink_samples", samples);
    sim_metric_add(route == 4 ? "cell.call.headset_samples" : "cell.call.speaker_samples", samples);
    return route | (MS.muted ? 0 : 1);
}
const char *sim_net_call_tones(void) { return MS.tones; }

int sim_net_sent_sms(int i, char *number, size_t ncap, char *text, size_t tcap) {
    if (i < 0 || i >= MS.sent_n || i < MS.sent_n - 8) return MS.sent_n;
    snprintf(number, ncap, "%s", MS.sent[i % 8].number);
    snprintf(text, tcap, "%s", MS.sent[i % 8].text);
    return MS.sent_n;
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
        call_end();
        cell_relink();
    }
}

void sim_net_outage(uint64_t ns) { /* ISR (script) */
    if (N.type == HAL_LINK_NONE || E.outage) return;
    E.outage = true;
    relink();
    sim_metric_add("net.outages", 1);
    vtk_event_after(ns, outage_end_cb, NULL);
}

void hal_net_link_status(hal_link_status_t *out) {
    out->type = N.type;
    out->up = N.up;
    out->signal_dbm = N.type == HAL_LINK_LTE ? (E.cell ? E.cell_rsrp : 0) : N.type == HAL_LINK_WIFI ? (E.wifi ? E.wifi_rssi : 0) : 0;
}

void hal_net_set_link_callback(hal_link_cb cb, void *ctx) {
    if (N.nlink >= 4) osal_panic("net: more than four link listeners");
    N.link_ctx[N.nlink] = ctx;
    N.link_cb[N.nlink++] = cb;
}

/* System event loop task (esp_event "sys_evt" on the device): delivers link changes. */
static void sys_evt_task(void *arg) {
    (void)arg;
    for (;;) {
        vtk_lock();
        bool other = false;
        while (!N.link_changed && !(other = N.evt[0].fn != NULL)) vtk_block_locked(&N.evt_wq, VTK_TIME_INF);
        bool link = N.link_changed;
        N.link_changed = false;
        typeof(N.evt) todo;
        memcpy(todo, N.evt, sizeof todo);
        if (other) memset(N.evt, 0, sizeof N.evt);
        vtk_unlock();
        sim_charge_ns_always(SIM_US(40));
        if (other)
            for (int i = 0; i < 4 && todo[i].fn; i++) todo[i].fn(todo[i].ctx);
        if (link && N.nlink) {
            hal_link_status_t st;
            hal_net_link_status(&st);
            for (int i = 0; i < N.nlink; i++) N.link_cb[i](N.link_ctx[i], &st);
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
/* A message the hub sends later (a reply after its think time, speech it is still synthesising):
 * it enters the TCP stream at that time, not now, so whatever the hub sends meanwhile is not
 * queued behind it. */
typedef struct {
    struct hal_ws *ws;
    uint64_t gen;
    bool binary;
    size_t len;
    uint8_t data[];
} later_t;

void sim_net_hub_send(struct hal_ws *w, uint64_t t_send, const void *data, size_t len, bool binary);

static void later_cb(void *arg) { /* ISR */
    later_t *l = arg;
    if (l->ws->gen == l->gen) sim_net_hub_send(l->ws, vtk_now(), l->data, l->len, l->binary);
    else sim_metric_add("net.dropped_downlink", 1);
    sim_host_free(l);
}

void sim_net_hub_send_at(struct hal_ws *w, uint64_t t_send, const void *data, size_t len, bool binary) {
    if (!w || t_send <= vtk_now()) {
        sim_net_hub_send(w, t_send, data, len, binary);
        return;
    }
    later_t *l = sim_host_malloc(sizeof *l + len);
    if (!l) abort();
    l->ws = w;
    l->gen = w->gen;
    l->binary = binary;
    l->len = len;
    memcpy(l->data, data, len);
    vtk_event_at(t_send, later_cb, l);
}

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
    if (u->ws->gen == u->gen && u->ws->state == 1 && !u->ws->dead && N.up) {
        if (g_sim.hub_url) {
            if (!sim_hub_bridge_send(u->ws->bridge, u->m->data, u->m->len, u->m->binary))
                sim_metric_add("bridge.dropped_uplink", 1);
        } else sim_hub_receive(u->ws, vtk_isr_now(), u->m->data, u->m->len, u->m->binary);
    }
    else
        sim_metric_add("net.dropped_uplink", 1);
    sim_host_free(u->m);
    sim_host_free(u);
}

void sim_net_bridge_event(void *owner, uint64_t generation, int event, const void *data, size_t len) {
    struct hal_ws *w = owner;
    if (w->gen != generation || w->close_req || w->state == 2) {
        sim_metric_add("bridge.stale_callbacks", 1);
        return;
    }
    if (event == SIM_BRIDGE_OPEN) {
        w->bridge_open = true;
        vtk_isr_wake_one(&w->wq);
    } else if (event == SIM_BRIDGE_CLOSED) {
        w->dead = true;
        w->dead_since = vtk_isr_now();
        vtk_isr_wake_one(&w->wq);
    } else if (w->state == 1 && !w->dead) {
        sim_net_hub_send(w, vtk_isr_now(), data, len, event == SIM_BRIDGE_BINARY);
        sim_metric_add("bridge.received", 1);
    }
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
    if (w->bridge) { sim_hub_bridge_close(w->bridge); w->bridge = NULL; }
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
    if (ok && g_sim.hub_url) {
        w->bridge = sim_hub_bridge_open(w, w->gen);
        vtk_lock();
        while (w->bridge && !w->bridge_open && !w->dead && !w->close_req && vtk_now() < deadline)
            vtk_block_locked(&w->wq, deadline - vtk_now());
        ok = w->bridge_open && !w->dead && !w->close_req;
        vtk_unlock();
    }
    memcpy(w->peer_key, g_peer_key, 32);
    if (ok && w->pinned && memcmp(w->expected_key, w->peer_key, 32)) {
        sim_metric_add("net.pin_rejected", 1);
        ok = false;
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
            size_t len = m->len; /* once it is on its way the message is the event's, which frees it */
            vtk_event_at(arrive, hub_arrive_cb, u);
            w->tx_bytes -= len;
            vtk_wake_all_locked(&w->txwq);
            vtk_unlock();
            N.bytes_up += len;
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
    vtk_lock();
    w->task_done = true; /* no callback runs from here on: hal_ws_free may return */
    vtk_wake_all_locked(&w->donewq);
    vtk_unlock();
    /* the struct itself is host memory and is never freed: link events may still refer to it */
}

/* ------------------------------------------------------------------ API */

hal_ws_t *hal_ws_open_pinned(const char *url, const uint8_t *sha256, hal_ws_cb cb, void *ctx) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_ws_open must be called from a task");
    struct hal_ws *w = sim_host_calloc(1, sizeof *w);
    if (!w) abort();
    snprintf(w->url, sizeof w->url, "%s", url);
    w->cb = cb;
    w->ctx = ctx;
    w->pinned = sha256 != NULL;
    if (sha256) memcpy(w->expected_key, sha256, 32);
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

hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx) { return hal_ws_open_pinned(url, NULL, cb, ctx); }
bool hal_ws_peer_key(hal_ws_t *w, uint8_t out[32]) {
    if (!w || w->state != 1) return false;
    memcpy(out, w->peer_key, 32);
    return true;
}

int hal_ws_send(hal_ws_t *w, bool binary, const void *data, size_t len, int64_t timeout_us) {
    if (!w || vtk_in_isr()) return -1;
    if (osal_task_self() == w->task) osal_panic("hal_ws_send from its connection's callback (see hal_net.h)");
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

int hal_ws_flush(hal_ws_t *w, int64_t timeout_us) {
    if (!w || vtk_in_isr() || osal_task_self() == w->task || timeout_us < 0) return -1;
    vtk_lock();
    uint64_t deadline = vtk_now() + (uint64_t)timeout_us * 1000;
    while (w->state == 1 && !w->dead && (w->tx_bytes || vtk_now() < w->up_last)) {
        uint64_t now = vtk_now();
        if (now >= deadline) { vtk_unlock(); return -1; }
        uint64_t until = !w->tx_bytes && w->up_last < deadline ? w->up_last : deadline;
        vtk_block_locked(&w->txwq, until - now);
    }
    bool ok = w->state == 1 && !w->dead;
    vtk_unlock();
    return ok ? 0 : -1;
}

void hal_ws_free(hal_ws_t *w) {
    if (!w) return;
    if (vtk_in_isr() || osal_task_self() == w->task) osal_panic("hal_ws_free from an ISR or its connection's callback (see hal_net.h)");
    vtk_lock();
    w->close_req = true;
    vtk_wake_one_locked(&w->wq);
    struct hal_ws **pp = &N.conns;
    while (*pp && *pp != w) pp = &(*pp)->next;
    if (*pp) *pp = w->next;
    while (!w->task_done) vtk_block_locked(&w->donewq, VTK_TIME_INF); /* a callback in progress finishes */
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

/* What is around the device and in its slots: set before anything runs, so that a script's first
 * lines (at 0) change it and are not overwritten when the boot task reaches sim_net_init. */
void sim_net_env_init(void) {
    hal_link_type_t t = !strcmp(g_profile.default_link, "wifi") ? HAL_LINK_WIFI
                        : !strcmp(g_profile.default_link, "lte") ? HAL_LINK_LTE : HAL_LINK_NONE;
    CELL.selected = g_profile.sim_selected;
    for (int i = 0; i < 2; i++) {
        CELL.present[i] = g_profile.sim_card[i].present;
        CELL.sms[i] = g_profile.sim_card[i].sms;
        CELL.voice[i] = g_profile.sim_card[i].voice;
    }
    cell_load_on_host(NULL); /* the modem kept its slot setting */
    CELL.active = CELL.selected;
    CELL.radio = true;
    CELL.registered = CELL.present[CELL.active]; /* registered at boot (no boot-time delay modelled) */
    /* around at the start: the network of the choice the system saved, and cellular coverage if there
     * is a modem; a known Wi-Fi network elsewhere comes with the script ("wifi on") */
    E.wifi = E.wifi_script = g_profile.wifi.present && t == HAL_LINK_WIFI;
    E.cell = g_profile.lte.present;
    E.wifi_rssi = -60;
    E.cell_rsrp = -100;
}

void sim_net_init(void) {
    hal_link_type_t t = !strcmp(g_profile.default_link, "wifi") ? HAL_LINK_WIFI
                        : !strcmp(g_profile.default_link, "lte") ? HAL_LINK_LTE : HAL_LINK_NONE;
    CELL.registered = CELL.present[CELL.active]; /* cards the script changed at 0 */
    N.type = t;
    N.want = env_ok(t);
    N.up = N.want && (t != HAL_LINK_LTE || !g_profile.lte.present || cell_ok());
    N.L = t == HAL_LINK_LTE ? &g_profile.lte : t == HAL_LINK_WIFI ? &g_profile.wifi : NULL;
    N.started = true;
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
