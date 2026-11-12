/* CloudZao/Metalio-E-INK4-Plus@874538e, MIT; see LICENSE-CloudZao. */
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"
#include <cstdio>
#include <cstring>
extern "C" void ink_transfer_wifi_pause(void), ink_transfer_wifi_restore(bool);
static esp_netif_t *s_ap_netif;
static bool s_we_inited, s_paused_sta;
static const char *TAG = "transfer";
constexpr int8_t kMetalioWifiMaxTxPower = 48;
struct WifiStation { static WifiStation &GetInstance() { static WifiStation s; return s; }
    void PauseForLp() { ink_transfer_wifi_pause(); } bool IsLpPaused() { return true; } };
namespace Lang { namespace Strings { static const char *CLOUD_AP_FAIL_INIT = "init", *CLOUD_AP_FAIL_START = "start"; } }
const char* StartRadio(const char* ssid) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif init %s", esp_err_to_name(err));
        return Lang::Strings::CLOUD_AP_FAIL_INIT;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop %s", esp_err_to_name(err));
        return Lang::Strings::CLOUD_AP_FAIL_INIT;
    }
    if (s_ap_netif == nullptr) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == nullptr) {
            ESP_LOGE(TAG, "ap netif create failed");
            return Lang::Strings::CLOUD_AP_FAIL_INIT;
        }
    }
    esp_netif_ip_info_t ip = {};
    IP4_ADDR(&ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(s_ap_netif);
    esp_netif_set_ip_info(s_ap_netif, &ip);
    esp_netif_dhcps_start(s_ap_netif);

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        cfg.nvs_enable = false;
        if (esp_wifi_init(&cfg) != ESP_OK) {
            ESP_LOGE(TAG, "wifi init failed");
            return Lang::Strings::CLOUD_AP_FAIL_INIT;
        }
        s_we_inited = true;
    } else {
        WifiStation::GetInstance().PauseForLp();
        s_paused_sta = WifiStation::GetInstance().IsLpPaused();
    }

    wifi_config_t ap = {};
    std::snprintf(reinterpret_cast<char*>(ap.ap.ssid), sizeof(ap.ap.ssid), "%s", ssid);
    ap.ap.ssid_len = static_cast<uint8_t>(std::strlen(ssid));
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK || esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "softap start failed");
        return Lang::Strings::CLOUD_AP_FAIL_START;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);
    #if 1 /* This port is the Plus board. */
    if (esp_wifi_set_max_tx_power(kMetalioWifiMaxTxPower) != ESP_OK) {
        return Lang::Strings::CLOUD_AP_FAIL_START;
    }
    #endif
    return nullptr;
}

void FillSsid(char* out, size_t n) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    std::snprintf(out, n, "Metalio-%02X%02X", mac[4], mac[5]);
}
extern "C" int ink_pico_radio_start(void) {
    char ssid[33];
    FillSsid(ssid, sizeof ssid);
    return StartRadio(ssid) ? -1 : 0;
}
extern "C" void ink_pico_radio_stop(void) {
    if (s_paused_sta || s_we_inited) {
        esp_wifi_stop(); esp_wifi_set_mode(WIFI_MODE_STA); ink_transfer_wifi_restore(true);
        s_paused_sta = s_we_inited = false;
    }
}
