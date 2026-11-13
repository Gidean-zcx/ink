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
#include "ink/hal_cellular.h"
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
    double u1 = sim_rand01(SIM_RNG_NET), u2 = sim_rand01(SIM_RNG_NET);
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
 *  - AT+CFUN=4 (airplane mode, 3GPP TS 27.007 §8.2: transmit and receive circuits off, the card stays
 *    powered) drops the registration and any call like AT+CFUN=0; AT+CFUN? then answers 4.
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
    bool airplane;             /* off by AT+CFUN=4, not AT+CFUN=0 */
    bool present[2], sms[2], voice[2];
    bool registered;
    uint64_t deaf_until;       /* rebooting after AT+ECRST, or held down (VTK_TIME_INF) */
    vtk_event_id_t reg_ev;     /* registration pending */
    vtk_event_id_t boot_ev;    /* coming back from a reset or power-up */
    bool held, unpowered;      /* board: P12 shared reset low, P14 shared supply off */
    bool at_busy;              /* a command is running */
    uint32_t slot_changes, resets;
    bool roaming, data_off;
    hal_cell_apn_t apn;
    uint64_t rx_bytes, tx_bytes;
} CELL;

bool hal_net_lte_bytes(uint64_t *rx, uint64_t *tx) {
    vtk_lock(); *rx = CELL.rx_bytes; *tx = CELL.tx_bytes; vtk_unlock(); return g_profile.lte.present;
}
void sim_net_lte_frame(bool tx, size_t bytes) {
    if (N.type == HAL_LINK_LTE && N.up) *(tx ? &CELL.tx_bytes : &CELL.rx_bytes) += bytes;
}

static bool cell_ok(void) { return CELL.radio && CELL.registered && CELL.present[CELL.active]; }

static void set_link(hal_link_type_t t, bool up) { /* ISR or locked task context */
    hal_link_type_t old = N.type;
    bool was = N.up;
    N.type = t;
    N.want = up;
    N.up = up && t != HAL_LINK_NONE && (t != HAL_LINK_LTE || !g_profile.lte.present || (cell_ok() && !CELL.data_off));
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
    CELL.airplane = false;
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

/* ---- SMS, calls and unsolicited lines (3GPP TS 27.005 / 27.007) ----
 * Messages wait in the modem's store and are announced with +CMTI; AT+CMGR/AT+CMGD read and delete
 * them. PDU mode (AT+CMGF=0, the default, TS 27.005 §3.2.3): +CMGR gives an SMS-DELIVER (TS 23.040
 * §9.2.2.1), AT+CMGS=<octets> takes an SMS-SUBMIT (§9.2.2.2) after its "> " prompt. Text mode
 * (AT+CMGF=1): UCS2 hex after AT+CSCS="UCS2" (else plain text). +CMGS comes after SMS_SEND_MS
 * (ESTIMATE). The network side: a long message to the device comes as concatenated parts
 * (§9.2.3.24.1, 153 GSM 7-bit or 67 UCS2 characters each, as phones split them); tests can send
 * parts one by one (any order, again, or never) and make a part from the device fail. What the
 * device sent is kept as the far phone puts it together. Calls: ATD<n>; ATA, ATH / AT+CHUP,
 * AT+CLCC; state changes are reported as +CLCC lines (stat 0 active, 2 dialling, 3 alerting,
 * 4 incoming), an incoming call rings (RING, +CLIP) every 3 s, the end is NO CARRIER. Which
 * unsolicited lines the EC718PM firmware really sends is an ASSUMPTION until its AT manual is
 * checked. SMS needs a card with SMS, calls one with voice (VoLTE), and both a registration.
 * Call audio injection below assumes a path through BH1098G; the board wiring is unverified. */
#define SMS_SEND_MS 2000
#define SMS_SLOTS 32
#define URC_SLOTS 16
#define CALL_RING_MS 3000
#define CALL_ANSWER_MS 4000  /* the far end picks up an outgoing call (auto-answer) */
#define CALL_RING_GIVEUP_MS 30000

static struct {
    bool text_mode, ucs2;
    struct { bool used, read, ref16; char number[24], text[400]; int ref, total, seq; } sms[SMS_SLOTS];
    struct { char number[24], text[1200]; } sent[8];
    int sent_n;
    char sent_pdu[16][360]; /* what the device sent, PDU by PDU (hex, the SMSC address included) */
    struct { bool used; char number[24], text[400]; int ref, total, seq; } out_parts[16]; /* waiting for the rest */
    unsigned fail_pdu;   /* the PDU (by sms_out + 1) the network refuses, 0 none */
    unsigned char ref8;  /* the next reference a long message to the device gets */
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

void sim_net_roaming(bool on) { CELL.roaming = on; urc_push(on ? "+CEREG: 5" : "+CEREG: 1"); }

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

/* PDU mode, as the network and the far phone see it (3GPP TS 23.040, TS 23.038) */
static const uint16_t GSM7_ALPHABET[128] = { /* TS 23.038 §6.2.1: septet -> code point; 0x1B (the escape) is 0 */
    '@', 0xA3, '$', 0xA5, 0xE8, 0xE9, 0xF9, 0xEC, 0xF2, 0xC7, '\n', 0xD8, 0xF8, '\r', 0xC5, 0xE5,
    0x394, '_', 0x3A6, 0x393, 0x39B, 0x3A9, 0x3A0, 0x3A8, 0x3A3, 0x398, 0x39E, 0, 0xC6, 0xE6, 0xDF, 0xC9,
    ' ', '!', '"', '#', 0xA4, '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
    0xA1, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xC4, 0xD6, 0xD1, 0xDC, 0xA7,
    0xBF, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xE4, 0xF6, 0xF1, 0xFC, 0xE0};

static int model_septet(unsigned cp) {
    for (int i = 0; cp && i < 128; i++) if (GSM7_ALPHABET[i] == cp) return i;
    return -1;
}

static unsigned next_cp(const char **s) { /* the text is the simulator's own: well-formed UTF-8 */
    const unsigned char *p = (const unsigned char *)*s;
    int n = p[0] < 0x80 ? 1 : p[0] < 0xE0 ? 2 : p[0] < 0xF0 ? 3 : 4;
    unsigned cp = n == 1 ? p[0] : p[0] & (0x7F >> n);
    for (int i = 1; i < n && p[i]; i++) cp = cp << 6 | (p[i] & 0x3F);
    *s += n;
    return cp;
}

static void put_utf8(char *out, size_t cap, size_t *n, unsigned cp) {
    char b[4];
    size_t m = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    if (m == 1) b[0] = (char)cp;
    else if (m == 2) b[0] = (char)(0xC0 | cp >> 6), b[1] = (char)(0x80 | (cp & 0x3F));
    else if (m == 3) b[0] = (char)(0xE0 | cp >> 12), b[1] = (char)(0x80 | (cp >> 6 & 0x3F)), b[2] = (char)(0x80 | (cp & 0x3F));
    else b[0] = (char)(0xF0 | cp >> 18), b[1] = (char)(0x80 | (cp >> 12 & 0x3F)), b[2] = (char)(0x80 | (cp >> 6 & 0x3F)), b[3] = (char)(0x80 | (cp & 0x3F));
    if (*n + m < cap) memcpy(out + *n, b, m), *n += m;
    out[*n] = 0;
}

static bool all_gsm7(const char *text, size_t len, int *units) { /* and the units it takes as UCS2 */
    bool g = true;
    *units = 0;
    for (const char *p = text; p < text + len;) {
        unsigned cp = next_cp(&p);
        g = g && model_septet(cp) >= 0;
        *units += cp >= 0x10000 ? 2 : 1;
    }
    return g;
}

/* Store slot i as an SMS-DELIVER: SMSC address, then the TPDU (§9.2.2.1). Returns the TPDU's octets. */
static int deliver_pdu(int i, char *hex, size_t cap) {
    uint8_t b[200] = {0};
    const char *num = MS.sms[i].number, *d = num + (num[0] == '+'), *text = MS.sms[i].text;
    int n = 0, digits = (int)strlen(d), units, hdr = MS.sms[i].total > 1 ? (MS.sms[i].ref16 ? 7 : 6) : 0, ref = MS.sms[i].ref;
    bool g = all_gsm7(text, strlen(text), &units);
    b[n++] = hdr ? 0x44 : 0x04; /* SMS-DELIVER, no more messages to send, TP-UDHI */
    b[n++] = (uint8_t)digits;
    b[n++] = num[0] == '+' ? 0x91 : 0x81;
    for (int k = 0; k < digits; k += 2) b[n++] = (uint8_t)((d[k] - '0') | (k + 1 < digits ? d[k + 1] - '0' : 0xF) << 4);
    b[n++] = 0;
    b[n++] = g ? 0x00 : 0x08;
    memcpy(b + n, (const uint8_t[]){0x62, 0x11, 0x02, 0x01, 0x00, 0x00, 0x23}, 7); /* 26-11-20 10:00:00 +08:00 */
    n += 7;
    uint8_t *udl = &b[n++], *ud = &b[n];
    if (hdr == 6) memcpy(ud, (const uint8_t[]){5, 0x00, 3, (uint8_t)ref, (uint8_t)MS.sms[i].total, (uint8_t)MS.sms[i].seq}, 6);
    if (hdr == 7) memcpy(ud, (const uint8_t[]){6, 0x08, 4, (uint8_t)(ref >> 8), (uint8_t)ref, (uint8_t)MS.sms[i].total, (uint8_t)MS.sms[i].seq}, 7);
    int k = g ? (hdr * 8 + 6) / 7 : hdr;
    for (const char *p = text; *p;) {
        unsigned cp = next_cp(&p);
        if (g) {
            unsigned s = (unsigned)model_septet(cp), bit = 7u * (unsigned)k++;
            ud[bit / 8] |= (uint8_t)(s << bit % 8);
            if (bit % 8 > 1) ud[bit / 8 + 1] |= (uint8_t)(s >> (8 - bit % 8));
        } else if (cp >= 0x10000) {
            unsigned hi = 0xD800 | ((cp - 0x10000) >> 10), lo = 0xDC00 | ((cp - 0x10000) & 0x3FF);
            ud[k++] = (uint8_t)(hi >> 8), ud[k++] = (uint8_t)hi, ud[k++] = (uint8_t)(lo >> 8), ud[k++] = (uint8_t)lo;
        } else ud[k++] = (uint8_t)(cp >> 8), ud[k++] = (uint8_t)cp;
    }
    *udl = (uint8_t)k;
    n += g ? (k * 7 + 7) / 8 : k;
    size_t h = (size_t)snprintf(hex, cap, "0891683108000000F0"); /* a synthetic SMSC, +8613800000000 */
    for (int j = 0; j < n && h + 3 <= cap; j++) h += (size_t)snprintf(hex + h, cap - h, "%02X", b[j]);
    return n;
}

static void sent_add(const char *number, const char *text) {
    int i = MS.sent_n % 8;
    snprintf(MS.sent[i].number, sizeof MS.sent[i].number, "%s", number);
    snprintf(MS.sent[i].text, sizeof MS.sent[i].text, "%s", text);
    MS.sent_n++;
}

/* An SMS-SUBMIT from the device (§9.2.2.2): 0 taken, else a +CMS ERROR code (TS 27.005 §3.2.5). The far phone puts
 * concatenated parts together. */
static int submit_take(const char *hex, int tpdu_len) {
    uint8_t b[200];
    int n = 0, len = (int)strlen(hex);
    for (; 2 * n + 1 < len && n < (int)sizeof b; n++) {
        unsigned v;
        if (sscanf(hex + 2 * n, "%2x", &v) != 1) return 304;
        b[n] = (uint8_t)v;
    }
    if (len % 2 || 2 * n != len || n < 1) return 304;
    int i = 1 + b[0];
    if (n - i != tpdu_len || n - i < 7) return 304; /* <length> is the TPDU's octets (TS 27.005 §3.5.1) */
    int fo = b[i], vpf = fo >> 3 & 3, digits = b[i + 2], toa = b[i + 3];
    if ((fo & 3) != 1) return 304;
    char number[24];
    size_t nn = 0;
    if ((toa & 0x70) == 0x10) number[nn++] = '+';
    for (int k = 0; k < digits && nn + 1 < sizeof number; k++) number[nn++] = (char)('0' + (b[i + 4 + k / 2] >> (k % 2 * 4) & 0xF));
    number[nn] = 0;
    i += 4 + (digits + 1) / 2;
    int dcs = b[i + 1];
    i += 2 + (vpf == 2 ? 1 : vpf ? 7 : 0);
    if (i >= n) return 304;
    int udl = b[i], hdr = 0, ref = 0, total = 1, seq = 1;
    const uint8_t *ud = b + i + 1;
    if (fo & 0x40) {
        hdr = ud[0] + 1;
        for (int k = 1; k + 1 < hdr; k += 2 + ud[k + 1]) {
            if (ud[k] == 0x00) ref = ud[k + 2], total = ud[k + 3], seq = ud[k + 4];
            if (ud[k] == 0x08) ref = ud[k + 2] << 8 | ud[k + 3], total = ud[k + 4], seq = ud[k + 5];
        }
    }
    char text[400];
    size_t tn = 0;
    text[0] = 0;
    if (dcs == 0x00) {
        if ((udl * 7 + 7) / 8 > n - i - 1) return 304;
        for (int k = (hdr * 8 + 6) / 7; k < udl; k++) {
            int bit = 7 * k;
            unsigned s = (unsigned)(ud[bit / 8] >> (bit % 8) | (bit % 8 > 1 ? ud[bit / 8 + 1] << (8 - bit % 8) : 0)) & 0x7F;
            put_utf8(text, sizeof text, &tn, GSM7_ALPHABET[s]);
        }
    } else if (dcs == 0x08) {
        if (udl > n - i - 1) return 304;
        for (int k = hdr; k + 1 < udl; k += 2) {
            unsigned u = (unsigned)ud[k] << 8 | ud[k + 1];
            if (u >= 0xD800 && u < 0xDC00 && k + 3 < udl) u = 0x10000 + ((u - 0xD800) << 10) + (((unsigned)ud[k + 2] << 8 | ud[k + 3]) - 0xDC00), k += 2;
            put_utf8(text, sizeof text, &tn, u);
        }
    } else return 303; /* not a text the far phone shows */
    snprintf(MS.sent_pdu[MS.sms_out % 16], sizeof MS.sent_pdu[0], "%s", hex);
    if (total <= 1) {
        sent_add(number, text);
        return 0;
    }
    int free_at = -1, have = 0;
    for (int k = 0; k < 16; k++) {
        if (!MS.out_parts[k].used) { if (free_at < 0) free_at = k; continue; }
        if (!strcmp(MS.out_parts[k].number, number) && MS.out_parts[k].ref == ref && MS.out_parts[k].total == total) have++;
    }
    if (free_at < 0) return 322; /* the far phone's memory is full */
    MS.out_parts[free_at].used = true;
    snprintf(MS.out_parts[free_at].number, sizeof MS.out_parts[0].number, "%s", number);
    snprintf(MS.out_parts[free_at].text, sizeof MS.out_parts[0].text, "%s", text);
    MS.out_parts[free_at].ref = ref, MS.out_parts[free_at].total = total, MS.out_parts[free_at].seq = seq;
    if (++have < total) return 0;
    char whole[1200] = "";
    for (int q = 1; q <= total; q++)
        for (int k = 0; k < 16; k++)
            if (MS.out_parts[k].used && !strcmp(MS.out_parts[k].number, number) && MS.out_parts[k].ref == ref && MS.out_parts[k].seq == q) {
                strncat(whole, MS.out_parts[k].text, sizeof whole - strlen(whole) - 1);
                MS.out_parts[k].used = false;
            }
    sent_add(number, whole);
    return 0;
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
    if (busy) sim_warn("modem: command sent while another AT command runs");
    hal_at_result_t r = at_run(cmd, payload, resp, resp_cap, timeout_ms);
    vtk_lock();
    if (!busy) CELL.at_busy = false;
    vtk_unlock();
    return r;
}

static hal_at_result_t at_run(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms) {
    if (resp && resp_cap) resp[0] = 0;
    if (!g_profile.lte.present) return HAL_AT_NO_MODEM;
    enum { K_AT, K_CFUN_Q, K_CFUN0, K_CFUN1, K_SLOT_Q, K_SLOT_SET, K_CPIN, K_CEREG, K_CSQ, K_RST, K_CMGF, K_CSCS, K_CNMI, K_CMGS, K_CMGR,
           K_CMGD, K_ATD, K_ATA, K_CHUP, K_CLCC, K_CMUT, K_CMUT_Q, K_VTS, K_CFUN4, K_CTX, K_AUTH, K_CTX_Q, K_AUTH_Q, K_ACT, K_BAD } k = K_BAD;
    unsigned lat_ms = CELL_AT_MS;
    int slot = -1, idx = -1;
    char arg[100] = "";
    if (!strcmp(cmd, "AT")) k = K_AT;
    else if (!strcmp(cmd, "AT+CFUN?")) k = K_CFUN_Q;
    else if (!strcmp(cmd, "AT+CFUN=0")) k = K_CFUN0, lat_ms = CELL_CFUN0_MS;
    else if (!strcmp(cmd, "AT+CFUN=1")) k = K_CFUN1, lat_ms = CELL_CFUN1_MS;
    else if (!strcmp(cmd, "AT+CFUN=4")) k = K_CFUN4, lat_ms = CELL_CFUN0_MS;
    else if (!strcmp(cmd, "AT+ECSIMCFG?")) k = K_SLOT_Q;
    else if (sscanf(cmd, "AT+ECSIMCFG=SimSlot,%d", &slot) == 1 || sscanf(cmd, "AT+ECSIMCFG=\"SimSlot\",%d", &slot) == 1)
        k = K_SLOT_SET, lat_ms = CELL_SLOT_MS;
    else if (!strcmp(cmd, "AT+CPIN?")) k = K_CPIN;
    else if (!strcmp(cmd, "AT+CEREG?")) k = K_CEREG;
    else if (!strcmp(cmd, "AT+CSQ")) k = K_CSQ;
    else if (!strcmp(cmd, "AT+CGDCONT?")) k = K_CTX_Q;
    else if (!strcmp(cmd, "AT+CGAUTH?")) k = K_AUTH_Q;
    else if (!strncmp(cmd, "AT+CGDCONT=1,\"IP\",\"", 19)) k = K_CTX;
    else if (!strncmp(cmd, "AT+CGAUTH=1,", 12)) k = K_AUTH;
    else if (!strcmp(cmd, "AT+CGACT=0,1") || !strcmp(cmd, "AT+CGACT=1,1")) k = K_ACT;
    else if (!strcmp(cmd, "AT+ECRST")) k = K_RST;
    else if (!strncmp(cmd, "AT+CMGF=", 8)) k = K_CMGF;
    else if (!strncmp(cmd, "AT+CSCS=", 8)) k = K_CSCS;
    else if (!strncmp(cmd, "AT+CNMI=", 8)) k = K_CNMI;
    else if (sscanf(cmd, "AT+CMGS=\"%99[^\"]\"", arg) == 1) k = K_CMGS, lat_ms = SMS_SEND_MS;
    else if (sscanf(cmd, "AT+CMGS=%d", &idx) == 1) k = K_CMGS, lat_ms = SMS_SEND_MS; /* PDU mode: the TPDU's octets */
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
    case K_CTX_Q: snprintf(info, sizeof info, "+CGDCONT: 1,\"IP\",\"%s\"", CELL.apn.name); break;
    case K_AUTH_Q: snprintf(info, sizeof info, "+CGAUTH: 1,%d,\"%s\",\"%s\"", CELL.apn.user[0] ? 1 : 0, CELL.apn.user, CELL.apn.password); break;
    case K_CTX:
    case K_AUTH: {
        char *fields[] = {k == K_CTX ? CELL.apn.name : CELL.apn.user, CELL.apn.password};
        const size_t caps[] = {k == K_CTX ? sizeof CELL.apn.name : sizeof CELL.apn.user, sizeof CELL.apn.password};
        const char *p = cmd + (k == K_CTX ? 18 : 12);
        for (int i = 0; i < (k == K_CTX ? 1 : 2); i++) {
            const char *q = strchr(p, '"'), *end = q ? strchr(q + 1, '"') : NULL;
            if (!end || (size_t)(end - q) > caps[i]) { r = HAL_AT_ERROR; break; }
            sim_copy_name(fields[i], caps[i], ""); memcpy(fields[i], q + 1, (size_t)(end - q - 1)); fields[i][end - q - 1] = 0; p = end + 1;
        }
        break;
    }
    case K_ACT: CELL.data_off = cmd[9] == '0'; cell_relink(); break;
    case K_AT: break;
    case K_CFUN_Q: snprintf(info, sizeof info, "+CFUN: %d", CELL.radio ? 1 : CELL.airplane ? 4 : 0); break;
    case K_CFUN0:
    case K_CFUN4:
        cell_radio(false, vtk_now());
        CELL.airplane = k == K_CFUN4;
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
    case K_CPIN: /* the card is powered only with the radio on or in airplane mode (ASSUMPTION) */
        if ((CELL.radio || CELL.airplane) && CELL.present[CELL.active]) snprintf(info, sizeof info, "+CPIN: READY");
        else r = HAL_AT_ERROR;
        break;
    case K_CSQ: {
        int csq = E.cell ? (E.cell_rsrp + 113) / 2 : 99;
        if (csq != 99) csq = csq < 0 ? 0 : csq > 31 ? 31 : csq;
        snprintf(info, sizeof info, "+CSQ: %d,99", csq);
        break;
    }
    case K_CEREG: {
        int stat = cell_ok() ? (CELL.roaming ? 5 : 1) : CELL.radio && CELL.present[CELL.active] && CELL.reg_ev ? 2 : 0;
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
    case K_CMGS: {
        int err = !payload || MS.text_mode != (arg[0] != 0) || !can_sms() ? (!cell_ok() ? 331 : 500) : 0;
        if (!err && MS.fail_pdu && MS.sms_out + 1 == MS.fail_pdu) err = 500, MS.fail_pdu = 0; /* the test's refused part */
        if (!err && !MS.text_mode) err = submit_take(payload, idx);
        if (err) {
            snprintf(info, sizeof info, "+CMS ERROR: %d", err);
            r = HAL_AT_ERROR;
        } else if (!MS.text_mode) {
            MS.sms_out++;
            snprintf(info, sizeof info, "+CMGS: %d", MS.sms_out & 0xFF);
        } else {
            int i = MS.sent_n % 8;
            decode(arg, MS.sent[i].number, sizeof MS.sent[i].number);
            decode(payload, MS.sent[i].text, sizeof MS.sent[i].text);
            MS.sent_n++;
            MS.sms_out++;
            snprintf(info, sizeof info, "+CMGS: %d", MS.sms_out & 0xFF);
        }
        break;
    }
    case K_CMGR:
        if (idx < 0 || idx >= SMS_SLOTS || !MS.sms[idx].used) {
            r = HAL_AT_ERROR;
        } else if (!MS.text_mode) {
            char pdu[400];
            int octets = deliver_pdu(idx, pdu, sizeof pdu);
            snprintf(info, sizeof info, "+CMGR: %d,,%d\n%s", MS.sms[idx].read ? 1 : 0, octets, pdu);
            MS.sms[idx].read = true;
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
    case K_ATD: {
        /* ASSUMPTION: emergency service without a SIM; NT26 support needs hardware verification. */
        bool emergency = !strcmp(arg, "110") || !strcmp(arg, "120") || !strcmp(arg, "119") || !strcmp(arg, "112");
        if (MS.call || !(emergency ? CELL.radio : can_call())) {
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
    }
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

static bool deliver_part(const char *number, const char *utf8, size_t len, int ref, int total, int seq, bool ref16) {
    for (int i = 0; i < SMS_SLOTS; i++)
        if (!MS.sms[i].used) {
            MS.sms[i].used = true;
            MS.sms[i].read = false;
            snprintf(MS.sms[i].number, sizeof MS.sms[i].number, "%s", number);
            snprintf(MS.sms[i].text, sizeof MS.sms[i].text, "%.*s", (int)len, utf8);
            MS.sms[i].ref = ref, MS.sms[i].total = total, MS.sms[i].seq = seq, MS.sms[i].ref16 = ref16;
            MS.sms_in++;
            char l[40];
            snprintf(l, sizeof l, "+CMTI: \"SM\",%d", i);
            urc_push(l);
            return true;
        }
    return false; /* store full */
}

bool sim_net_deliver_sms(const char *number, const char *utf8) { /* split as the sending phone does */
    if (!can_sms()) return false;
    int units, used = 0, parts = 1, in_part = 0;
    size_t len = strlen(utf8);
    bool g = all_gsm7(utf8, len, &units);
    if (units <= (g ? 160 : 70)) return deliver_part(number, utf8, len, 0, 1, 1, false);
    for (const char *p = utf8; *p;) {
        unsigned cp = next_cp(&p), u = g || cp < 0x10000 ? 1 : 2;
        if (in_part + (int)u > (g ? 153 : 67)) parts++, in_part = 0;
        in_part += (int)u;
    }
    for (int i = 0; i < SMS_SLOTS; i++) used += MS.sms[i].used;
    if (SMS_SLOTS - used < parts) return false;
    int ref = ++MS.ref8, seq = 1;
    const char *start = utf8;
    in_part = 0;
    for (const char *p = utf8; *p;) {
        const char *at = p;
        unsigned cp = next_cp(&p), u = g || cp < 0x10000 ? 1 : 2;
        if (in_part + (int)u > (g ? 153 : 67)) deliver_part(number, start, (size_t)(at - start), ref, parts, seq++, false), start = at, in_part = 0;
        in_part += (int)u;
    }
    return deliver_part(number, start, strlen(start), ref, parts, seq, false);
}

bool sim_net_deliver_sms_part(const char *number, const char *utf8, int ref, int total, int seq, bool ref16) {
    return can_sms() && deliver_part(number, utf8, strlen(utf8), ref, total, seq, ref16);
}

void sim_net_sms_fail_part(int nth) { MS.fail_pdu = nth > 0 ? MS.sms_out + (unsigned)nth : 0; }

int sim_net_sent_pdu(int i, char *hex, size_t cap) {
    if (i >= 0 && i < (int)MS.sms_out && i >= (int)MS.sms_out - 16) snprintf(hex, cap, "%s", MS.sent_pdu[i % 16]);
    return (int)MS.sms_out;
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
    out->airplane = CELL.airplane;
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

void hal_net_set_time_callback(void (*cb)(void *, int64_t), void *ctx) { (void)cb; (void)ctx; }

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
    bool held; /* by a stall: delivered at its end */
} arrival_t;

/* Script "stall": nothing reaches the device until stall_until; what was due meanwhile comes then, in order (a
 * delivery due exactly at the end waits behind the held ones). */
static uint64_t g_stall_until;
static int g_stall_held;

void sim_net_stall(uint64_t ns) { /* ISR (script) */
    uint64_t until = vtk_isr_now() + ns;
    if (until > g_stall_until) g_stall_until = until;
    sim_metric_add("net.stalls", 1);
    sim_trace_span("net", "downlink stall", vtk_isr_now(), until, NULL);
}

static void deliver_cb(void *arg) { /* ISR */
    arrival_t *a = arg;
    struct hal_ws *w = a->ws;
    if (a->held) {
        a->held = false;
        g_stall_held--;
    } else if (vtk_isr_now() < g_stall_until || g_stall_held) {
        a->held = true;
        g_stall_held++;
        vtk_event_at(vtk_isr_now() < g_stall_until ? g_stall_until : vtk_isr_now(), deliver_cb, a);
        return;
    }
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
    if (N.L && sim_rand01(SIM_RNG_NET) < N.L->loss) {
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
    a->held = false;
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
            vtk_lock(); sim_net_lte_frame(false, m->len); vtk_unlock();
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
            sim_net_lte_frame(true, m->len);
            uint64_t t = now > N.up_free ? now : N.up_free;
            t += radio_touch(t);
            uint64_t x = xfer_ns(m->len);
            N.up_free = t + x;
            uint64_t arrive = t + x + rtt_ns() / 2;
            if (N.L && sim_rand01(SIM_RNG_NET) < N.L->loss) {
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
        double s = N.L->outage_min_s + (N.L->outage_max_s - N.L->outage_min_s) * sim_rand01(SIM_RNG_NET);
        sim_net_outage((uint64_t)(s * 1e9));
    }
    if (N.L && N.L->outages_per_hour > 0) {
        double mean_s = 3600.0 / N.L->outages_per_hour;
        double next = -log(1.0 - sim_rand01(SIM_RNG_NET) * 0.999) * mean_s;
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
        vtk_event_after((uint64_t)(-log(1.0 - sim_rand01(SIM_RNG_NET) * 0.999) * mean_s * 1e9), random_outage_cb, NULL);
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
