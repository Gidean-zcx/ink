/* C/HAL adaptation of CloudZao/Metalio-E-INK4-Plus@874538e, MIT. See SOURCE.md. */
#define MAX_RECONNECT_COUNT 5
static const uint64_t kScanRetryUs = 10ULL * 1000 * 1000;
static const int8_t kMetalioWifiMaxTxPower = 48;

static void ApplyApSelection(wifi_config_t *config) {
    config->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config->sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    config->sta.bssid_set = false;
    memset(config->sta.bssid, 0, sizeof(config->sta.bssid));
    config->sta.channel = 0;
}

static void ApplyWifiProfile() {
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
}

static esp_err_t NetifInitOnce()
{
    esp_err_t err = esp_netif_init();
    return (err == ESP_ERR_INVALID_STATE) ? ESP_OK : err;
}

/* PauseForLp: retain the STA netif and handlers, stop scan and radio. */
static void ink_station_pause(void) {
    if (!ink_wifi.running) return;
    if (ink_wifi.netif == NULL) return;
    wifi_mode_t mode = WIFI_MODE_NULL;
    const bool driver_ready = esp_wifi_get_mode(&mode) == ESP_OK;
    if (driver_ready && (mode & WIFI_MODE_AP) != 0) return;
    if (ink_wifi.timer != NULL) esp_timer_stop(ink_wifi.timer);
    esp_wifi_scan_stop();
    xEventGroupClearBits(ink_wifi.events, INK_JOIN_BIT);
    if (driver_ready) {
        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK) {
            ESP_LOGW("ink_wifi", "LP pause wifi stop: %s", esp_err_to_name(err));
            return;
        }
    }
    ink_wifi.running = ink_wifi.joining = ink_wifi.link.up = false;
    ESP_LOGI("ink_wifi", "STA paused (radio off, netif kept)");
}

/* ResumeFromLp: apply the same profile before every restart. */
static void ink_station_resume(void) {
    if (ink_wifi.running) return;
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        ESP_LOGW("ink_wifi", "LP resume skipped (wifi not init)");
        return;
    }
    if ((mode & WIFI_MODE_STA) == 0) {
        ESP_LOGW("ink_wifi", "LP resume skipped (no STA, mode=%d)", (int)mode);
        return;
    }
    ink_wifi.reconnect_count = 0;
    ink_wifi.running = true;
    ApplyWifiProfile();
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        ink_wifi.running = false;
        ESP_LOGE("ink_wifi", "LP resume wifi start: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI("ink_wifi", "STA resumed");
}

/* The system's availability query gates discovery while using LTE. */
static void ink_scan_retry(void *arg) {
    (void)arg;
    INK_LOCK();
    ink_wifi.scan_due = true;
    INK_UNLOCK();
    ink_s31_net_changed();
}

static void ink_scan_later(void) {
    esp_timer_stop(ink_wifi.timer);
    if (ink_wifi.n) ESP_ERROR_CHECK(esp_timer_start_once(ink_wifi.timer, kScanRetryUs));
}

/* StartConnect: consume the strongest remaining known AP, with RAM credentials. */
static void ink_rejoin(void) {
    if (!ink_wifi.running || ink_wifi.link.type != HAL_LINK_WIFI || ink_wifi.link.up || ink_wifi.joining) return;
    if (ink_wifi.queued) {
        int next = ink_wifi.queue[0];
        memmove(ink_wifi.queue, ink_wifi.queue + 1, (size_t)--ink_wifi.queued * sizeof *ink_wifi.queue);
        ink_wifi.reconnect_count = 0;
        ink_join(&ink_known[next]);
    } else {
        ink_scan_later();
    }
}

/* WifiEventHandler: retry this AP five times, then the rest, then rescan. */
static void ink_station_disconnected(void) {
    if (!ink_wifi.running || !ink_wifi.n || ink_wifi.link.type != HAL_LINK_WIFI) return;
    if (ink_wifi.reconnect_count < MAX_RECONNECT_COUNT) {
        ink_wifi.joining = esp_wifi_connect() == ESP_OK;
        ink_wifi.reconnect_count++;
        return;
    }
    ink_rejoin();
}
