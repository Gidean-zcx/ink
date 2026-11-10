#ifndef INK_WIFI_TEST_IDF_H
#define INK_WIFI_TEST_IDF_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
typedef int esp_err_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
typedef unsigned EventBits_t;
typedef struct { unsigned bits; } *EventGroupHandle_t;
typedef struct { bool nvs_enable; } wifi_init_config_t;
typedef struct { struct { uint8_t ssid[32], password[64]; int scan_method, sort_method; } sta; } wifi_config_t;
typedef struct { uint8_t ssid[33]; int rssi, authmode; } wifi_ap_record_t;
typedef struct { int reason; } wifi_event_sta_disconnected_t;
typedef struct { int status; } wifi_event_sta_scan_done_t;
typedef struct { struct timeval tv; } esp_netif_sntp_time_sync_t;
typedef struct { bool start, wait_for_sync; const char *servers[1]; } esp_sntp_config_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define ESP_EVENT_ANY_ID -1
extern const char ink_test_wifi_base[], ink_test_ip_base[], ink_test_sntp_base[];
#define WIFI_EVENT ink_test_wifi_base
#define IP_EVENT ink_test_ip_base
#define NETIF_SNTP_EVENT ink_test_sntp_base
#define WIFI_EVENT_STA_DISCONNECTED 1
#define WIFI_EVENT_SCAN_DONE 2
#define IP_EVENT_STA_GOT_IP 3
#define IP_EVENT_STA_LOST_IP 5
#define NETIF_SNTP_TIME_SYNC 4
#define WIFI_REASON_AUTH_FAIL 202
#define WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT 15
#define WIFI_REASON_HANDSHAKE_TIMEOUT 204
#define WIFI_REASON_NO_AP_FOUND 201
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_STA 1
#define WIFI_IF_STA 0
#define WIFI_PS_MIN_MODEM 1
#define WIFI_ALL_CHANNEL_SCAN 1
#define WIFI_CONNECT_AP_BY_SIGNAL 1
#define WIFI_AUTH_OPEN 0
#define WIFI_INIT_CONFIG_DEFAULT() { .nvs_enable = true }
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) { .start = true, .wait_for_sync = true, .servers = {server} }
#define ESP_EVENT_DEFINE_BASE(name) esp_event_base_t name = #name
#define ESP_ERROR_CHECK(expr) assert((expr) == ESP_OK)
#define ESP_LOGI(...) ((void)0)
#define EXT_RAM_BSS_ATTR
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_wifi_init(const wifi_init_config_t *cfg);
esp_err_t esp_wifi_set_storage(int storage);
void *esp_netif_create_default_wifi_sta(void);
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id, esp_event_handler_t cb, void *ctx);
esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void *data, size_t n, unsigned ticks);
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *cfg);
esp_err_t esp_netif_sntp_start(void);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_set_ps(int ps);
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *cfg);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap);
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *ap);
esp_err_t esp_wifi_clear_ap_list(void);
esp_err_t esp_wifi_scan_start(const void *cfg, bool block);
esp_err_t esp_wifi_scan_stop(void);
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits, int clear, int all, unsigned ticks);
#endif
