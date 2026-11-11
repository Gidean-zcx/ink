/* Explicit unavailable HALs. No bus, flash, radio or device access. */
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_random.h"
#include "ink/hal_afe.h"
#include "ink/hal_assets.h"
#include "ink/hal_audio.h"
#include "ink/hal_bt.h"
#include "ink/hal_camera.h"
#include "ink/hal_cellular.h"
#include "ink/hal_codec.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/hal_input.h"
#include "ink/hal_jpeg.h"
#include "ink/hal_net.h"
#include "ink/hal_ota.h"
#include "ink/hal_power.h"
#include "ink/hal_provision.h"
#include "ink/hal_storage.h"
#include "ink/hal_transfer.h"
#include "ink/hal_usb.h"
#include "ink/probe.h"
#include "ink/osal.h"

/* TODO: ESP-SR AFE/WakeNet (S31 support unverified). */
bool hal_afe_available(void) { return false; }
const char *const *hal_afe_names(void) { static const char *const empty[] = {NULL}; return empty; }
int hal_afe_select_name(int index) { return -1; }
int hal_afe_start(void) { return -1; }
int hal_afe_set_commands(const char *const *phrases, int n) { return -1; }
void hal_afe_stop(void) {  }
void hal_afe_feed(const int16_t *mic, const int16_t *ref, int samples, uint64_t t_us, hal_afe_result_t *out) { *out = (hal_afe_result_t){.command = -1}; }

/* Audio and Opus: hal_audio_s31.c and hal_opus_s31.c. */

/* Bluetooth headsets and shared UART1 control: hal_bt_s31.c. */

/* Camera: hal_camera_s31.c. */


/* Device info, log, random; RTC and accelerometer: s31_board.c. */
__attribute__((weak)) bool ink_log_from_hal(const char *line) { (void)line; return false; }
void hal_log_write(const char *line) { if (!ink_log_from_hal(line)) esp_log_write(ESP_LOG_INFO, "ink", "%s\n", line); }
void hal_random_fill(void *out, size_t n) { esp_fill_random(out, n); }

#ifndef INK_S31_DISPLAY
bool ink_s31_display_acquire(uint32_t timeout_ms) { (void)timeout_ms; return true; }
void ink_s31_display_release(void) { }
void ink_s31_display_shutdown(void) { }
void ink_s31_display_panic(void) { }
/* Offline fallback: no panel transport linked. */
void hal_display_get_info(hal_panel_info_t *out) { *out = (hal_panel_info_t){0}; }
float hal_display_level_luminance(unsigned level) { return 1.0f; }
uint32_t hal_display_update(const hal_rect_t *area, const uint8_t *gray8, int stride, hal_waveform_t wf, uint32_t flags, hal_display_done_fn done, void *ctx) { return 0; }
int hal_display_pending(void) { return 0; }
void hal_display_hold_power(uint32_t ms) {  }
void hal_display_wake(uint32_t ms) {  }
void hal_display_pause(bool pause) {  }
void hal_display_set_temperature(int celsius) {  }
#endif

/* I2C: hal_i2c_s31.c. */

/* Input: hal_input_s31.c. */

/* TODO: ESP-IDF S31 JPEG decoder. */
int hal_jpeg_decode_gray(const uint8_t *src, size_t n, uint8_t *dst, int w, int h) { return -1; }

/* Wi-Fi/link/SNTP: hal_wifi_s31.c; TODO: TLS/WebSocket. */
hal_ws_t *hal_ws_open_pinned(const char *url, const uint8_t *sha256, hal_ws_cb cb, void *ctx) { return NULL; }
hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx) { return NULL; }
bool hal_ws_peer_key(hal_ws_t *ws, uint8_t out[32]) { return false; }
int hal_ws_send(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us) { return -1; }
int hal_ws_flush(hal_ws_t *ws, int64_t timeout_us) { return -1; }
void hal_ws_free(hal_ws_t *ws) {  }

/* TODO: ESP-IDF OTA/partition APIs + verified signature backend. */
int hal_ota_begin(size_t size) { return -1; }
int hal_ota_write(const void *data, size_t n) { return -1; }
int hal_ota_end(void) { return -1; }
int hal_ota_set_boot(void) { return -1; }
int hal_ota_mark_valid(void) { return -1; }
int hal_ota_write_resource(int kind, size_t offset, const void *data, size_t n) { return -1; }
bool hal_crypto_verify_fw(const uint8_t hash[32], const uint8_t *sig, size_t sig_len, uint32_t key_id) { return false; }

/* Power: hal_power_s31.c; battery/light: s31_board.c; motor: hal_input_s31.c. */

/* TODO: ESP-IDF SoftAP + HTTP server. */
int hal_provision_start(const char *ssid, const char *password, hal_provision_fn fn, void *ctx) { return -1; }
void hal_provision_stop(void) {  }
void hal_provision_reply(uint32_t request, int status) {  }

/* TODO: ESP-IDF lwIP local TCP listener. */

/* UART: hal_uart_s31.c. */

/* TODO: ESP-IDF TinyUSB MSC + SDMMC unmount/remount. */
bool hal_usb_host_connected(void) { return false; }
bool hal_usb_host_done(void) { return false; }
void hal_usb_set_callback(void (*cb)(void *ctx), void *ctx) {  }
int hal_usb_disk_start(void) { return -1; }
int hal_usb_disk_stop(void) { return -1; }

void ink_probe_value(const char *metric, double value) { }
void ink_probe_count(const char *metric) { }
void ink_probe_mark(const char *name) { }
