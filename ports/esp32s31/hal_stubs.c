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

/* TODO: ESP-SR AFE/WakeNet (S31 support unverified). */
bool hal_afe_available(void) { return false; }
int hal_afe_start(void) { return -1; }
int hal_afe_set_commands(const char *const *phrases, int n) { return -1; }
void hal_afe_stop(void) {  }
void hal_afe_feed(const int16_t *mic, const int16_t *ref, int samples, uint64_t t_us, hal_afe_result_t *out) { *out = (hal_afe_result_t){.command = -1}; }

/* TODO: Read-only resource partition via esp_partition_mmap. */
const void *hal_asset_map(const char *name, size_t *size) { if (size) *size = 0;
    return NULL; }
void hal_asset_unmap(const void *data) {  }
void hal_asset_access(const void *p, size_t len) {  }

/* TODO: ESP-IDF I2S + upstream codec and board amplifier control. */
int hal_audio_route_set(hal_audio_route_t route) { return -1; }
void hal_audio_get_info(hal_audio_info_t *out) { *out = (hal_audio_info_t){0}; }
int hal_audio_capture_start(hal_audio_capture_cb cb, void *ctx) { return -1; }
void hal_audio_capture_stop(void) {  }
int hal_audio_play_start(void) { return -1; }
int hal_audio_play_write(const int16_t *pcm, int n, int64_t timeout_us) { return -1; }
int64_t hal_audio_play_delay_us(void) { return -1; }
void hal_audio_play_stop(bool drain) {  }

/* TODO: Portable BH1098G driver + ESP-IDF UART. */
bool hal_bt_available(void) { return false; }
int hal_bt_scan(hal_bt_device_t *out, int max, int timeout_ms) { return -1; }
int hal_bt_pair(const char *addr) { return -1; }
int hal_bt_connect(const char *addr) { return -1; }
void hal_bt_disconnect(void) {  }
void hal_bt_forget(const char *addr) {  }
void hal_bt_on_event(void (*fn)(void *ctx, hal_bt_event_t ev), void *ctx) {  }

/* TODO: Portable GC2145 driver + ESP-IDF camera DMA. */
bool hal_camera_info(hal_camera_info_t *out) { *out = (hal_camera_info_t){0}; return false; }
int hal_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout_ms) { return -1; }

/* TODO: Portable NT26 driver + upstream modem transport. */
bool hal_cell_status(hal_cell_status_t *out) { *out = (hal_cell_status_t){0}; return false; }
int hal_cell_select_sim(hal_cell_slot_t slot, uint32_t reg_timeout_ms) { return -1; }
int hal_cell_radio(bool on, uint32_t reg_timeout_ms) { return -1; }
int hal_cell_sms_parts(const char *utf8) { return -1; }
int hal_cell_sms_send(const char *number, const char *utf8, uint32_t timeout_ms, int *sent) { *sent = 0; return -1; }
int hal_cell_sms_take(int index, hal_cell_sms_t *out) { return -1; }
int hal_cell_dial(const char *number) { return -1; }
int hal_cell_answer(void) { return -1; }
int hal_cell_hangup(void) { return -1; }
int hal_cell_mute(bool on, bool *muted) { return -1; }
int hal_cell_dtmf(char key) { return -1; }
int hal_cell_wait_event(hal_cell_event_t *ev, uint32_t timeout_ms) { return -1; }

/* TODO: Upstream libopus (not linked in this skeleton). */
hal_opus_dec_t *hal_opus_dec_new(void) { return NULL; }
hal_opus_enc_t *hal_opus_enc_new(int bitrate, int complexity) { return NULL; }
int hal_opus_encode(hal_opus_enc_t *e, const int16_t *pcm, uint8_t *out, int cap) { return -1; }
void hal_opus_enc_free(hal_opus_enc_t *e) {  }
int hal_opus_decode(hal_opus_dec_t *d, const uint8_t *pkt, int len, int16_t *pcm) { return -1; }
void hal_opus_dec_free(hal_opus_dec_t *d) {  }

/* Device info, log, random; RTC and accelerometer: s31_board.c. */
const char *hal_device_reset_reason(void) { return "skeleton"; }
void hal_device_get_info(hal_device_info_t *out) { *out = (hal_device_info_t){.profile_id = "esp32s31-skeleton", .display_name = "ESP32-S31 skeleton", .device_class = HAL_CLASS_MCU, .cpu_cores = configNUMBER_OF_CORES, .cpu_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ}; }
void hal_log_write(const char *line) { esp_log_write(ESP_LOG_INFO, "ink", "%s\n", line); }
void hal_random_fill(void *out, size_t n) { esp_fill_random(out, n); }

/* TODO: Upstream modified EPDiy + TPS65185 board power; no panel attached. */
void hal_display_get_info(hal_panel_info_t *out) { *out = (hal_panel_info_t){0}; }
float hal_display_level_luminance(unsigned level) { return 1.0f; }
uint32_t hal_display_update(const hal_rect_t *area, const uint8_t *gray8, int stride, hal_waveform_t wf, uint32_t flags, hal_display_done_fn done, void *ctx) { return 0; }
int hal_display_pending(void) { return 0; }
void hal_display_hold_power(uint32_t ms) {  }
void hal_display_wake(uint32_t ms) {  }
void hal_display_pause(bool pause) {  }
void hal_display_set_temperature(int celsius) {  }

/* I2C: hal_i2c_s31.c. */

/* Input: hal_input_s31.c. */

/* TODO: ESP-IDF S31 JPEG decoder. */
int hal_jpeg_decode_gray(const uint8_t *src, size_t n, uint8_t *dst, int w, int h) { return -1; }

/* TODO: ESP-IDF Wi-Fi/lwIP/TLS/WebSocket + NT26 modem. */
hal_ws_t *hal_ws_open_pinned(const char *url, const uint8_t *sha256, hal_ws_cb cb, void *ctx) { return NULL; }
hal_ws_t *hal_ws_open(const char *url, hal_ws_cb cb, void *ctx) { return NULL; }
void hal_net_link_status(hal_link_status_t *out) { *out = (hal_link_status_t){0}; }
void hal_net_use(hal_link_type_t type) {  }
bool hal_net_available(hal_link_type_t type) { return false; }
int hal_wifi_scan(hal_wifi_ap_t *out, int max, uint32_t timeout_ms) { return -1; }
int hal_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) { return -1; }
void hal_wifi_set_known(const hal_wifi_known_t *list, int n) {  }
void hal_net_set_link_callback(hal_link_cb cb, void *ctx) {  }
hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t resp_cap, uint32_t timeout_ms) { return HAL_AT_NO_MODEM; }
hal_at_result_t hal_modem_at_payload(const char *cmd, const char *payload, char *resp, size_t resp_cap, uint32_t timeout_ms) { return HAL_AT_NO_MODEM; }
int hal_modem_urc(char *buf, size_t cap, uint32_t timeout_ms) { return -1; }
bool hal_ws_peer_key(hal_ws_t *ws, uint8_t out[32]) { return false; }
int hal_ws_send(hal_ws_t *ws, bool binary, const void *data, size_t len, int64_t timeout_us) { return -1; }
int hal_ws_flush(hal_ws_t *ws, int64_t timeout_us) { return -1; }
void hal_ws_free(hal_ws_t *ws) {  }

/* TODO: ESP-IDF OTA/partition APIs + verified signature backend. */
void hal_ota_info(hal_ota_info_t *out) { *out = (hal_ota_info_t){0}; }
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

/* TODO: LittleFS internal buffers + ESP-IDF SDMMC/FatFS. */
hal_dir_t *hal_fs_opendir(const char *path) { return NULL; }
hal_file_t *hal_fs_open(const char *path, int flags) { return NULL; }
ssize_t hal_fs_read(hal_file_t *f, void *buf, size_t len) { return -1; }
ssize_t hal_fs_write(hal_file_t *f, const void *buf, size_t len) { return -1; }
int hal_fs_seek(hal_file_t *f, int64_t off, int whence) { return -1; }
int64_t hal_fs_tell(hal_file_t *f) { return -1; }
int hal_fs_sync(hal_file_t *f) { return -1; }
int hal_fs_close(hal_file_t *f) { return -1; }
int hal_fs_remove(const char *path) { return -1; }
int hal_fs_rename(const char *from, const char *to) { return -1; }
int hal_fs_mkdir(const char *path) { return -1; }
int64_t hal_fs_size(const char *path) { return -1; }
int hal_fs_exists(const char *path) { return -1; }
int64_t hal_fs_mtime(const char *path) { return -1; }
int hal_fs_readdir(hal_dir_t *d, hal_dirent_t *out) { return -1; }
void hal_fs_closedir(hal_dir_t *d) {  }
int hal_fs_volume_info(const char *volume , hal_fs_volume_info_t *out) { return -1; }
bool hal_fs_card_present(void) { return false; }
void hal_fs_set_card_callback(void (*cb)(void *ctx), void *ctx) {  }
void hal_fs_new_card_id(uint8_t out[16]) { esp_fill_random(out, 16); }

/* TODO: ESP-IDF lwIP local TCP listener. */
int hal_transfer_listen(uint16_t *port) { return -1; }
int hal_transfer_host(char *out, size_t cap) { return -1; }
int hal_transfer_accept(void) { return -1; }
int hal_transfer_read(void *data, size_t cap) { return -1; }
int hal_transfer_write(const void *data, size_t len) { return -1; }
void hal_transfer_close(int listener) {  }

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
