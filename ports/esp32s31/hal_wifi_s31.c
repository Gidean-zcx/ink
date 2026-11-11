/* S31 station only. Credentials and PHY calibration never touch NVS. */
#include <string.h>
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "s31_cell.h"

#define INK_KNOWN_MAX 8
#define INK_SCAN_BIT 1
#define INK_JOIN_BIT 2
#define INK_DOWN_BIT 4
enum { INK_AUTO, INK_SCAN, INK_JOIN, INK_PAUSE };
ESP_EVENT_DEFINE_BASE(INK_WIFI_CHANGED);
static hal_wifi_known_t ink_known[INK_KNOWN_MAX] EXT_RAM_BSS_ATTR;
static struct {
    osal_mutex_t *lock;
    EventGroupHandle_t events;
    esp_netif_t *netif;
    hal_link_status_t link;
    int n, next, mode, result, count, max, listeners;
    bool ready, available, joining;
    hal_wifi_ap_t *out;
    hal_link_cb cb[4];
    void *ctx[4], *time_ctx;
    void (*time_cb)(void *, int64_t);
} ink_wifi;
#define INK_LOCK() osal_mutex_lock(ink_wifi.lock, OSAL_WAIT_FOREVER)
#define INK_UNLOCK() osal_mutex_unlock(ink_wifi.lock)

static void ink_join(const hal_wifi_known_t *k) {
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, k->ssid, strnlen(k->ssid, sizeof cfg.sta.ssid));
    memcpy(cfg.sta.password, k->password, strnlen(k->password, sizeof cfg.sta.password));
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ink_wifi.joining = esp_wifi_set_config(WIFI_IF_STA, &cfg) == ESP_OK && esp_wifi_connect() == ESP_OK;
}
static void ink_rejoin(void) {
    if (ink_wifi.link.type == HAL_LINK_WIFI && ink_wifi.n && !ink_wifi.link.up && !ink_wifi.joining)
        ink_join(&ink_known[ink_wifi.next++ % ink_wifi.n]);
}
static void ink_notify(void) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    for (int i = 0; i < ink_wifi.listeners; i++) ink_wifi.cb[i](ink_wifi.ctx[i], &st);
}
void ink_s31_net_changed(void) {
    if (ink_wifi.ready) esp_event_post(INK_WIFI_CHANGED, 0, NULL, 0, 0);
}
static void ink_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base == INK_WIFI_CHANGED) ink_s31_cell_sync();
    INK_LOCK();
    if (base == NETIF_SNTP_EVENT) {
        void (*cb)(void *, int64_t) = ink_wifi.time_cb;
        void *ctx = ink_wifi.time_ctx;
        int64_t utc = ((esp_netif_sntp_time_sync_t *)data)->tv.tv_sec;
        INK_UNLOCK();
        if (cb && utc > 0) cb(ctx, utc);
        ESP_LOGI("ink_wifi", "sntp received");
        return;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP && ink_wifi.link.type == HAL_LINK_WIFI) {
        ink_wifi.link.up = ink_wifi.available = true;
        ink_wifi.joining = false;
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ink_wifi.link.signal_dbm = ap.rssi;
        ink_wifi.result = HAL_WIFI_OK;
        xEventGroupSetBits(ink_wifi.events, INK_JOIN_BIT);
        if (ink_wifi.time_cb) ESP_ERROR_CHECK(esp_netif_sntp_start());
        ESP_LOGI("ink_wifi", "link up (DHCP)");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        ink_wifi.link.up = false;
        esp_wifi_disconnect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        int why = ((wifi_event_sta_disconnected_t *)data)->reason;
        ink_wifi.link.up = ink_wifi.joining = false;
        ink_wifi.link.signal_dbm = 0;
        ink_wifi.result = why == WIFI_REASON_AUTH_FAIL || why == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || why == WIFI_REASON_HANDSHAKE_TIMEOUT
            ? HAL_WIFI_ERR_AUTH : why == WIFI_REASON_NO_AP_FOUND ? HAL_WIFI_ERR_NOT_FOUND : HAL_WIFI_ERR_TIMEOUT;
        if (why == WIFI_REASON_NO_AP_FOUND) ink_wifi.available = false;
        xEventGroupSetBits(ink_wifi.events, INK_DOWN_BIT | INK_JOIN_BIT);
        if (ink_wifi.mode == INK_AUTO) ink_rejoin();
        ESP_LOGI("ink_wifi", "link down reason=%d", why);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        ink_wifi.count = 0;
        ink_wifi.available = ink_wifi.link.up;
        wifi_ap_record_t ap;
        int best = -128;
        while (esp_wifi_scan_get_ap_record(&ap) == ESP_OK) {
            for (int i = 0; i < ink_wifi.n; i++) if (!strncmp((const char *)ap.ssid, ink_known[i].ssid, 33)) {
                if (ap.rssi > best) { best = ap.rssi; ink_wifi.next = i; }
                ink_wifi.available = true;
            }
            if (ink_wifi.out) {
                bool full = ink_wifi.count == ink_wifi.max;
                int at = full ? ink_wifi.max - 1 : ink_wifi.count++;
                if (full && ap.rssi <= ink_wifi.out[at].rssi) continue;
                for (; at > 0 && ink_wifi.out[at - 1].rssi < ap.rssi; at--) ink_wifi.out[at] = ink_wifi.out[at - 1];
                hal_wifi_ap_t *out = &ink_wifi.out[at];
                memcpy(out->ssid, ap.ssid, 32);
                out->ssid[32] = 0;
                out->rssi = ap.rssi;
                out->secured = ap.authmode != WIFI_AUTH_OPEN;
            }
        }
        esp_wifi_clear_ap_list();
        if (((wifi_event_sta_scan_done_t *)data)->status) ink_wifi.count = HAL_WIFI_ERR_TIMEOUT;
        xEventGroupSetBits(ink_wifi.events, INK_SCAN_BIT);
        if (ink_wifi.mode == INK_AUTO) ink_rejoin();
        ESP_LOGI("ink_wifi", "scan done count=%d", ink_wifi.count);
    }
    bool cell_time = base == INK_WIFI_CHANGED && ink_wifi.link.type == HAL_LINK_LTE && ink_wifi.time_cb;
    INK_UNLOCK();
    if (cell_time) {
        hal_link_status_t st;
        if (ink_s31_cell_link(&st) && st.up) ESP_ERROR_CHECK(esp_netif_sntp_start());
    }
    ink_notify();
}
static void ink_init(void) {
    if (ink_wifi.ready) return;
    ink_wifi.lock = osal_mutex_create();
    ink_wifi.events = xEventGroupCreate();
    if (!ink_wifi.lock || !ink_wifi.events) osal_panic("wifi: no memory");
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t r = esp_event_loop_create_default();
    if (r != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(r);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ink_wifi.netif = esp_netif_create_default_wifi_sta();
    if (!ink_wifi.netif) osal_panic("wifi: no interface");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, ink_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ink_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(INK_WIFI_CHANGED, 0, ink_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, ink_event, NULL));
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    sntp.start = sntp.wait_for_sync = false;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ink_wifi.ready = true;
    ink_s31_cell_netif_init();
    ESP_LOGI("ink_wifi", "ready storage=RAM nvs=off ps=MIN_MODEM");
}
void hal_net_link_status(hal_link_status_t *out) {
    if (!ink_wifi.ready) { *out = (hal_link_status_t){0}; return; }
    INK_LOCK(); *out = ink_wifi.link; INK_UNLOCK();
    if (out->type == HAL_LINK_LTE && !ink_s31_cell_link(out)) *out = (hal_link_status_t){.type = HAL_LINK_LTE};
}
bool hal_net_available(hal_link_type_t type) {
    if (type == HAL_LINK_LTE) return ink_s31_cell_available();
    if (!ink_wifi.ready || type != HAL_LINK_WIFI) return false;
    INK_LOCK(); bool yes = ink_wifi.available; INK_UNLOCK();
    return yes;
}
void hal_net_use(hal_link_type_t type) {
    ink_init();
    INK_LOCK();
    if (type == ink_wifi.link.type) { INK_UNLOCK(); return; }
    ink_wifi.link = (hal_link_status_t){.type = type};
    if (type != HAL_LINK_WIFI) esp_wifi_disconnect();
    else { esp_netif_set_default_netif(ink_wifi.netif); ink_rejoin(); }
    INK_UNLOCK();
    ink_s31_cell_use(type == HAL_LINK_LTE);
    ESP_ERROR_CHECK(esp_event_post(INK_WIFI_CHANGED, 0, NULL, 0, 0));
}
void hal_net_set_link_callback(hal_link_cb cb, void *ctx) {
    ink_init();
    for (int i = 0; i < ink_wifi.listeners; i++) if (ink_wifi.cb[i] == cb && ink_wifi.ctx[i] == ctx) return;
    if (cb && ink_wifi.listeners < 4) { int i = ink_wifi.listeners++; ink_wifi.cb[i] = cb; ink_wifi.ctx[i] = ctx; }
}
void hal_net_set_time_callback(void (*cb)(void *, int64_t), void *ctx) {
    ink_init();
    INK_LOCK(); ink_wifi.time_cb = cb; ink_wifi.time_ctx = ctx;
    if (cb && ink_wifi.link.up) ESP_ERROR_CHECK(esp_netif_sntp_start());
    INK_UNLOCK();
}
void hal_wifi_set_known(const hal_wifi_known_t *list, int n) {
    ink_init();
    INK_LOCK();
    ink_wifi.n = list && n > 0 ? (n < INK_KNOWN_MAX ? n : INK_KNOWN_MAX) : 0;
    memset(ink_known, 0, sizeof ink_known);
    if (ink_wifi.n) memcpy(ink_known, list, (size_t)ink_wifi.n * sizeof *list);
    ink_wifi.next = 0;
    ink_wifi.available = false;
    if (ink_wifi.joining) esp_wifi_disconnect();
    wifi_ap_record_t ap;
    if (ink_wifi.link.up && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        for (int i = 0; i < ink_wifi.n; i++) if (!strncmp((const char *)ap.ssid, ink_known[i].ssid, 33)) ink_wifi.available = true;
        if (!ink_wifi.available) esp_wifi_disconnect();
    }
    if (!ink_wifi.link.up && !ink_wifi.joining && ink_wifi.n) ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, false));
    INK_UNLOCK();
}
/* Quiesce an earlier join before changing credentials or starting a scan. */
static bool ink_pause(uint32_t ms, bool disconnect_up) {
    INK_LOCK(); ink_wifi.mode = INK_PAUSE;
    bool wait = ink_wifi.joining || (disconnect_up && ink_wifi.link.up);
    xEventGroupClearBits(ink_wifi.events, INK_DOWN_BIT);
    esp_err_t r = wait ? esp_wifi_disconnect() : ESP_OK;
    INK_UNLOCK();
    return r == ESP_OK && (!wait || (xEventGroupWaitBits(ink_wifi.events, INK_DOWN_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(ms)) & INK_DOWN_BIT));
}
int hal_wifi_scan(hal_wifi_ap_t *out, int max, uint32_t timeout_ms) {
    ink_init();
    if (!out || max <= 0 || !ink_pause(timeout_ms, false)) return HAL_WIFI_ERR_TIMEOUT;
    INK_LOCK(); ink_wifi.mode = INK_SCAN; ink_wifi.out = out; ink_wifi.max = max;
    xEventGroupClearBits(ink_wifi.events, INK_SCAN_BIT);
    esp_err_t r = esp_wifi_scan_start(NULL, false);
    INK_UNLOCK();
    bool done = r == ESP_OK && (xEventGroupWaitBits(ink_wifi.events, INK_SCAN_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms)) & INK_SCAN_BIT);
    INK_LOCK(); ink_wifi.out = NULL;
    if (!done) { esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); }
    int result = done ? ink_wifi.count : HAL_WIFI_ERR_TIMEOUT;
    ink_wifi.mode = INK_AUTO; ink_rejoin(); INK_UNLOCK();
    return result;
}
int hal_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) {
    ink_init();
    if (!ssid || !*ssid || strlen(ssid) > 32 || !password || strlen(password) > 64 || !ink_pause(timeout_ms, true)) return HAL_WIFI_ERR_TIMEOUT;
    hal_wifi_known_t k = {0};
    memcpy(k.ssid, ssid, strlen(ssid)); memcpy(k.password, password, strlen(password));
    ink_s31_cell_use(false);
    ESP_ERROR_CHECK(esp_netif_set_default_netif(ink_wifi.netif));
    INK_LOCK(); ink_wifi.mode = INK_JOIN; ink_wifi.link.type = HAL_LINK_WIFI;
    xEventGroupClearBits(ink_wifi.events, INK_JOIN_BIT);
    ink_join(&k); memset(&k, 0, sizeof k);
    bool started = ink_wifi.joining;
    INK_UNLOCK();
    bool done = started && (xEventGroupWaitBits(ink_wifi.events, INK_JOIN_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms)) & INK_JOIN_BIT);
    INK_LOCK();
    int result = done ? ink_wifi.result : HAL_WIFI_ERR_TIMEOUT;
    if (!done) esp_wifi_disconnect();
    ink_wifi.mode = INK_AUTO; ink_rejoin(); INK_UNLOCK();
    return result;
}
