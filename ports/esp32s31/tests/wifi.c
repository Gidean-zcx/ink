/* Real station HAL, deterministic IDF events. All credentials below are synthetic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifi_idf.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#ifndef INK_TEST_WIFI_SOURCE
#define INK_TEST_WIFI_SOURCE "../hal_wifi_s31.c"
#endif
#include INK_TEST_WIFI_SOURCE
const char ink_test_wifi_base[] = "wifi", ink_test_ip_base[] = "ip", ink_test_sntp_base[] = "sntp";

#define INK_TEST_EVENTS 64
#define INK_TEST_HANDLERS 8
static struct { esp_event_base_t base; int id; esp_event_handler_t cb; void *ctx; } handlers[INK_TEST_HANDLERS];
static struct { esp_event_base_t base; int id; union { wifi_event_sta_disconnected_t down; wifi_event_sta_scan_done_t scan; esp_netif_sntp_time_sync_t sync; } data; } pending[INK_TEST_EVENTS];
static bool cell_available, cell_up, cell_selected;
static unsigned nhandlers, head, tail, connect_calls, sntp_starts, scan_stops, clears;
static int response, scan_status;
static bool scan_timeout, radio_busy, scan_drop;
static bool radio_on, ap_visible = true;
static unsigned radio_starts, radio_stops, scan_calls, ps_calls, protocol_calls, power_calls;
struct ink_test_timer { esp_timer_create_args_t args; bool active; uint64_t us; };
static struct ink_test_timer retry_timer;
static esp_netif_t *default_netif;
static int fake_cell_netif;
static wifi_config_t config;
static wifi_ap_record_t aps[] = {{{"FAKE-AP-TWO"}, -65, 0}, {{"FAKE-NEIGHBOR"}, -30, 1}, {{"FAKE-AP-ONE"}, -42, 1}};
static unsigned ap_next;
static unsigned init_step;
struct osal_mutex { bool held; };
static struct osal_mutex mutex;
osal_mutex_t *osal_mutex_create(void) { return &mutex; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) { assert(m == &mutex && !m->held && us == OSAL_WAIT_FOREVER); m->held = true; return OSAL_OK; }
void osal_mutex_unlock(osal_mutex_t *m) { assert(m == &mutex && m->held); m->held = false; }
void osal_panic(const char *fmt, ...) { (void)fmt; abort(); }

static void pump_one(void) {
    assert(!mutex.held && head < tail);
    unsigned i = head++ % INK_TEST_EVENTS;
    for (unsigned h = 0; h < nhandlers; h++) if (!strcmp(handlers[h].base, pending[i].base) &&
            (handlers[h].id == ESP_EVENT_ANY_ID || handlers[h].id == pending[i].id))
        handlers[h].cb(handlers[h].ctx, pending[i].base, pending[i].id, &pending[i].data);
}
static void pump_all(void) { for (unsigned limit = 0; head < tail; limit++) { assert(limit < INK_TEST_EVENTS); pump_one(); } }
esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void *data, size_t n, unsigned ticks) {
    (void)ticks;
    assert(tail - head < INK_TEST_EVENTS && n <= sizeof pending[0].data);
    unsigned i = tail++ % INK_TEST_EVENTS;
    pending[i].base = base; pending[i].id = id;
    memset(&pending[i].data, 0, sizeof pending[i].data);
    if (n) memcpy(&pending[i].data, data, n);
    return ESP_OK;
}
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id, esp_event_handler_t cb, void *ctx) {
    assert(nhandlers < INK_TEST_HANDLERS);
    handlers[nhandlers].base = base; handlers[nhandlers].id = id; handlers[nhandlers].cb = cb; handlers[nhandlers++].ctx = ctx;
    return ESP_OK;
}
EventGroupHandle_t xEventGroupCreate(void) { static struct { unsigned bits; } group; return (EventGroupHandle_t)&group; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits) { return group->bits |= bits; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits) { unsigned old = group->bits; group->bits &= ~bits; return old; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits, int clear, int all, unsigned ticks) {
    assert(!all && ticks <= 1000 && !mutex.held);
    while (head < tail) pump_one(); /* the higher-priority IDF task drains its ready events first */
    unsigned got = group->bits;
    if (clear) group->bits &= ~bits;
    return got;
}
esp_err_t esp_netif_init(void) { assert(init_step++ == 0); return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { assert(init_step++ == 1); return ESP_OK; }
esp_err_t esp_wifi_init(const wifi_init_config_t *cfg) { assert(init_step++ == 3 && !cfg->nvs_enable); return ESP_OK; }
esp_err_t esp_wifi_set_storage(int storage) { assert(init_step++ == 4 && storage == WIFI_STORAGE_RAM); return ESP_OK; }
void *esp_netif_create_default_wifi_sta(void) { assert(init_step++ == 2); return &config; }
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *cfg) { assert(!cfg->start && !cfg->wait_for_sync && !strcmp(cfg->servers[0], "pool.ntp.org")); return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(init_step++ == 5 && mode == WIFI_MODE_STA); return ESP_OK; }
esp_err_t esp_wifi_get_mode(wifi_mode_t *mode) { *mode = WIFI_MODE_STA; return ESP_OK; }
esp_err_t esp_wifi_start(void) {
    assert(init_step == 6 && !radio_on && ps_calls > radio_starts && protocol_calls > radio_starts);
    radio_on = true; radio_starts++;
    return esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_START, NULL, 0, 0);
}
esp_err_t esp_wifi_stop(void) {
    assert(radio_on);
    radio_on = radio_busy = false; radio_stops++;
    return ESP_OK;
}
esp_err_t esp_wifi_set_ps(int ps) { assert(ps == WIFI_PS_NONE); ps_calls++; return ESP_OK; }
esp_err_t esp_wifi_set_protocol(int iface, unsigned protocol) {
    assert(iface == WIFI_IF_STA && protocol == (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    protocol_calls++; return ESP_OK;
}
esp_err_t esp_wifi_set_max_tx_power(int8_t power) { assert(radio_on && power == 48); power_calls++; return ESP_OK; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *timer) {
    assert(!retry_timer.args.callback && args->skip_unhandled_events && args->dispatch_method == ESP_TIMER_TASK);
    retry_timer.args = *args; *timer = &retry_timer; return ESP_OK;
}
esp_err_t esp_timer_stop(esp_timer_handle_t timer) { assert(timer == &retry_timer); timer->active = false; return ESP_OK; }
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t us) {
    assert(timer == &retry_timer && !timer->active && us == 10000000);
    timer->active = true; timer->us = us; return ESP_OK;
}
static void retry_due(void) {
    assert(retry_timer.active && !mutex.held);
    retry_timer.active = false;
    retry_timer.args.callback(retry_timer.args.arg);
    pump_all();
}
esp_err_t esp_netif_sntp_start(void) { sntp_starts++; return ESP_OK; }
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *cfg) {
    assert(radio_on && iface == WIFI_IF_STA && cfg->sta.scan_method == WIFI_ALL_CHANNEL_SCAN && cfg->sta.sort_method == WIFI_CONNECT_AP_BY_SIGNAL);
    assert(power_calls == radio_starts);
    assert(!cfg->sta.bssid_set && !cfg->sta.channel);
    for (size_t i = 0; i < sizeof cfg->sta.bssid; i++) assert(!cfg->sta.bssid[i]);
    config = *cfg; return ESP_OK;
}
esp_err_t esp_wifi_connect(void) {
    assert(radio_on);
    connect_calls++; radio_busy = true;
    int r = response; response = 0;
    if (r == 1) return esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0, 0);
    if (r) { wifi_event_sta_disconnected_t ev = {.reason = r}; return esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &ev, sizeof ev, 0); }
    return ESP_OK;
}
esp_err_t esp_wifi_disconnect(void) {
    if (radio_busy) { wifi_event_sta_disconnected_t ev = {.reason = 8}; radio_busy = false; return esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &ev, sizeof ev, 0); }
    return ESP_OK;
}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap) { *ap = aps[2]; return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *ap) {
    if (!ap_visible || ap_next == sizeof aps / sizeof *aps) return ESP_FAIL;
    *ap = aps[ap_next++]; return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list(void) { clears++; ap_next = sizeof aps / sizeof *aps; return ESP_OK; }
esp_err_t esp_wifi_scan_start(const void *cfg, bool block) {
    assert(radio_on && !cfg && !block);
    scan_calls++;
    ap_next = 0;
    if (scan_timeout) return ESP_OK;
    wifi_event_sta_scan_done_t ev = {.status = scan_status};
    esp_event_post(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &ev, sizeof ev, 0);
    if (scan_drop) {
        scan_drop = false; radio_busy = false;
        wifi_event_sta_disconnected_t down = {.reason = 8};
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0);
    }
    return ESP_OK;
}
esp_err_t esp_wifi_scan_stop(void) { scan_stops++; return ESP_OK; }
static int changes[4];
static hal_link_status_t observed;
static void on_link(void *ctx, const hal_link_status_t *st) { assert(!mutex.held); (*(int *)ctx)++; observed = *st; }
static int64_t utc_seen;
static void on_time(void *ctx, int64_t utc) { assert(!mutex.held && ctx == &utc_seen); utc_seen = utc; }
static void link_is(hal_link_type_t type, bool up) {
    hal_link_status_t st; hal_net_link_status(&st); assert(st.type == type && st.up == up);
    if (up) assert(default_netif == (type == HAL_LINK_WIFI ? (void *)&config : (void *)&fake_cell_netif));
}
int main(void) {
    link_is(HAL_LINK_NONE, false);
    for (int i = 0; i < 4; i++) hal_net_set_link_callback(on_link, &changes[i]);
    hal_net_set_link_callback(on_link, &changes[0]);
    assert(init_step == 6 && !radio_on && !hal_net_available(HAL_LINK_LTE));
    hal_wifi_known_t known[] = {{"FAKE-AP-ONE", "fake-password-one"}, {"FAKE-AP-TWO", "fake-password-two"}};
    hal_wifi_set_known(known, 2);
    memset(known, 0, sizeof known);
    pump_all();
    assert(!hal_net_available(HAL_LINK_WIFI)); /* the enabled system asks for discovery */
    pump_all();
    assert(hal_net_available(HAL_LINK_WIFI));
    assert(!radio_on && radio_starts == 1 && radio_stops == 1 && power_calls == 1);
    response = 1; hal_net_use(HAL_LINK_WIFI); pump_all();
    link_is(HAL_LINK_WIFI, true);
    assert(observed.up && observed.signal_dbm == -42 && changes[0] == changes[3] && sntp_starts == 0);
    assert(!strcmp((char *)config.sta.ssid, "FAKE-AP-ONE") && !strcmp((char *)config.sta.password, "fake-password-one"));
    hal_net_set_time_callback(on_time, &utc_seen);
    assert(sntp_starts == 1);
    esp_netif_sntp_time_sync_t sync = {.tv = {.tv_sec = 1794268800}};
    esp_event_post(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, &sync, sizeof sync, 0); pump_all();
    assert(utc_seen == 1794268800);
    wifi_event_sta_disconnected_t down = {.reason = WIFI_REASON_NO_AP_FOUND};
    unsigned calls = connect_calls;
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0); pump_all();
    assert(connect_calls == calls + 1 && !strcmp((char *)config.sta.ssid, "FAKE-AP-ONE"));
    assert(!hal_net_available(HAL_LINK_WIFI));
    response = 1;
    esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0); pump_all();
    link_is(HAL_LINK_WIFI, true);
    assert(sntp_starts == 2 && !strcmp((char *)config.sta.ssid, "FAKE-AP-ONE"));
    response = 1;
    calls = connect_calls;
    esp_event_post(IP_EVENT, IP_EVENT_STA_LOST_IP, NULL, 0, 0);
    pump_one();
    link_is(HAL_LINK_WIFI, false);
    pump_all();
    link_is(HAL_LINK_WIFI, true);
    assert(connect_calls == calls + 1 && sntp_starts == 3);
    calls = connect_calls;
    response = WIFI_REASON_AUTH_FAIL;
    assert(hal_wifi_connect("FAKE-WRONG", "fake-wrong-password", 100) == HAL_WIFI_ERR_AUTH);
    assert(connect_calls == calls + 1 && retry_timer.active); /* resume discovery after the manual failure */
    esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL, 0, 0); pump_all();
    assert(sntp_starts == 4);
    hal_wifi_ap_t seen[2];
    assert(hal_wifi_scan(seen, 2, 100) == 2 && !strcmp(seen[0].ssid, "FAKE-NEIGHBOR") && seen[0].rssi > seen[1].rssi && seen[1].secured);
    scan_drop = true; response = 1;
    assert(hal_wifi_scan(seen, 2, 100) == 2); /* disconnect must not replace the scan's result */
    pump_all(); link_is(HAL_LINK_WIFI, true);
    scan_timeout = true;
    unsigned stopped_scans = scan_stops;
    assert(hal_wifi_scan(seen, 2, 100) == HAL_WIFI_ERR_TIMEOUT && scan_stops == stopped_scans + 1 && clears > 0);
    scan_timeout = false;
    hal_net_use(HAL_LINK_LTE); pump_all(); link_is(HAL_LINK_LTE, false);
    int notices = changes[0];
    hal_net_use(HAL_LINK_LTE); pump_all();
    assert(changes[0] == notices); /* unavailable selection must not feed a link-event loop */
    calls = connect_calls; esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0); pump_all();
    assert(connect_calls == calls);
    hal_wifi_set_known(NULL, 0);
    response = WIFI_REASON_AUTH_FAIL;
    assert(hal_wifi_connect("FAKE-AP-ONE", "fake-wrong-password", 100) == HAL_WIFI_ERR_AUTH);
    response = WIFI_REASON_NO_AP_FOUND;
    assert(hal_wifi_connect("FAKE-ABSENT", "", 100) == HAL_WIFI_ERR_NOT_FOUND);
    assert(hal_wifi_connect("FAKE-TIMEOUT", "", 100) == HAL_WIFI_ERR_TIMEOUT); pump_all();
    response = 1;
    assert(hal_wifi_connect("FAKE-AP-TWO", "", 100) == HAL_WIFI_OK);
    link_is(HAL_LINK_WIFI, true);
    assert(sntp_starts == 6);
    hal_net_set_time_callback(NULL, NULL);
    sync.tv.tv_sec++;
    esp_event_post(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, &sync, sizeof sync, 0); pump_all();
    assert(utc_seen == 1794268800);
    calls = connect_calls;
    hal_wifi_set_known(NULL, 0); pump_all();
    link_is(HAL_LINK_WIFI, false);
    assert(connect_calls == calls && !hal_net_available(HAL_LINK_WIFI));
    hal_wifi_known_t retry = {"FAKE-AP-ONE", "fake-password-one"};
    hal_wifi_set_known(&retry, 1); pump_all();
    calls = connect_calls;
    hal_wifi_set_known(NULL, 0); pump_all();
    assert(connect_calls == calls && !radio_busy); /* forgetting also cancels an in-flight automatic join */
    /* W03: five retries per AP, then a delayed scan; LTE discovery never joins. */
    hal_wifi_set_known(&retry, 1); pump_all();
    calls = connect_calls;
    ap_visible = false;
    for (unsigned i = 0; i < 6; i++) {
        radio_busy = false;
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0);
        pump_all();
    }
    assert(connect_calls == calls + 5 && retry_timer.active && !hal_net_available(HAL_LINK_WIFI));
    unsigned scans = scan_calls;
    retry_due();
    /* Selected Wi-Fi also retries when fixed Wi-Fi bypasses auto_choice. */
    assert(scan_calls == scans + 1 && retry_timer.active && !hal_net_available(HAL_LINK_WIFI));
    cell_available = cell_up = true;
    assert(hal_net_available(HAL_LINK_LTE));
    hal_net_use(HAL_LINK_LTE); pump_all(); link_is(HAL_LINK_LTE, true); assert(cell_selected);
    assert(!radio_on && retry_timer.active);
    ap_visible = true;
    calls = connect_calls; scans = scan_calls;
    retry_due();
    assert(!radio_on && scan_calls == scans); /* Wi-Fi disabled or fixed LTE: no availability query */
    assert(!hal_net_available(HAL_LINK_WIFI)); pump_all();
    assert(hal_net_available(HAL_LINK_WIFI) && scan_calls == scans + 1 && connect_calls == calls && !radio_on);
    hal_link_status_t cellular; hal_net_link_status(&cellular); assert(cellular.signal_dbm == -83);
    unsigned before_time = sntp_starts;
    hal_net_set_time_callback(on_time, &utc_seen); assert(sntp_starts == before_time + 1);
    ink_s31_net_changed(); pump_all(); assert(sntp_starts == before_time + 1);
    esp_event_post(IP_EVENT, IP_EVENT_ETH_GOT_IP, NULL, 0, 0); pump_all(); assert(sntp_starts == before_time + 2);
    response = 1;
    hal_net_use(HAL_LINK_WIFI); pump_all(); link_is(HAL_LINK_WIFI, true);
    assert(radio_on && !cell_selected && sntp_starts == before_time + 3);
    assert(power_calls == radio_starts && ps_calls == protocol_calls && ps_calls == radio_starts + 1);
    hal_net_use(HAL_LINK_NONE); pump_all(); link_is(HAL_LINK_NONE, false); assert(!cell_selected);
    assert(!radio_on);
    scans = scan_calls; retry_due();
    assert(!radio_on && scan_calls == scans); /* airplane/off: a timer never restarts RF */
    response = 1;
    hal_net_use(HAL_LINK_WIFI); pump_all(); link_is(HAL_LINK_WIFI, true);
    hal_net_use(HAL_LINK_LTE); pump_all(); link_is(HAL_LINK_LTE, true);
    assert(!radio_on && power_calls == radio_starts && radio_stops == radio_starts);
    /* The original candidate queue tries the strongest AP before the next one. */
    hal_wifi_known_t candidates[] = {{"FAKE-AP-TWO", "fake-password-two"}, {"FAKE-AP-ONE", "fake-password-one"}};
    hal_wifi_set_known(candidates, 2);
    hal_net_use(HAL_LINK_WIFI); pump_all();
    calls = connect_calls;
    assert(!strcmp((char *)config.sta.ssid, "FAKE-AP-ONE"));
    for (unsigned i = 0; i < 6; i++) {
        radio_busy = false;
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0); pump_all();
        assert(!strcmp((char *)config.sta.ssid, i < 5 ? "FAKE-AP-ONE" : "FAKE-AP-TWO"));
    }
    assert(connect_calls == calls + 6);
    for (unsigned i = 0; i < 6; i++) {
        radio_busy = false;
        esp_event_post(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &down, sizeof down, 0); pump_all();
    }
    assert(connect_calls == calls + 11 && retry_timer.active);
    /* A retry queued just before switching off cannot restart the radio. */
    retry_timer.active = false;
    retry_timer.args.callback(retry_timer.args.arg);
    unsigned starts = radio_starts;
    hal_net_use(HAL_LINK_LTE); pump_all(); assert(!radio_on && radio_starts == starts);
    puts("wifi: W02/W03/W04, RAM, scan, DHCP, failures, callbacks, dual routes and SNTP passed");
    return 0;
}

/* The cellular transport is independently exercised by cell.c. */
void ink_s31_cell_netif_init(void) { }
void ink_s31_cell_sync(void) { }
void ink_s31_cell_use(bool on) { cell_selected = on; if (on) default_netif = &fake_cell_netif; }
bool ink_s31_cell_link(hal_link_status_t *out) { *out = (hal_link_status_t){.type = HAL_LINK_LTE, .up = cell_up && cell_selected, .signal_dbm = -83}; return true; }
bool ink_s31_cell_available(void) { return cell_available; }
esp_err_t esp_netif_set_default_netif(esp_netif_t *n) { default_netif = n; return ESP_OK; }
