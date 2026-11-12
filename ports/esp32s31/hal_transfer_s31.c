/* The kiikoread HTTP server owns sockets; the system owns policy. */
#include "ink/hal_transfer.h"
#include "ink/hal_net.h"
#include "esp_netif.h"
#include "qrcodegen.h"
#include <stdio.h>
#include <string.h>
extern int ink_pico_start(const hal_transfer_policy_t *);
extern int ink_pico_radio_start(void);
extern void ink_pico_radio_stop(void);
extern void ink_pico_stop(void);
extern void ink_transfer_wifi_init(void);
static bool ink_hotspot;
int hal_transfer_server_start(const hal_transfer_policy_t *policy, uint16_t *port) {
    hal_link_status_t link; hal_net_link_status(&link); ink_transfer_wifi_init();
    ink_hotspot = !link.up || link.type != HAL_LINK_WIFI;
    if (ink_hotspot && ink_pico_radio_start()) { ink_pico_radio_stop(); return -1; }
    int r = ink_pico_start(policy);
    if (r == 1) *port = 80;
    else if (ink_hotspot) ink_pico_radio_stop();
    return r;
}
void hal_transfer_server_stop(void) { ink_pico_stop(); if (ink_hotspot) ink_pico_radio_stop(); ink_hotspot = false; }
bool hal_transfer_hotspot(void) { return ink_hotspot; }
int hal_transfer_host(char *out, size_t cap) {
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ink_hotspot ? "WIFI_AP_DEF" : "WIFI_STA_DEF");
    if (!netif || esp_netif_get_ip_info(netif, &ip) != ESP_OK || !ip.ip.addr) return -1;
    int n = snprintf(out, cap, IPSTR, IP2STR(&ip.ip)); return n > 0 && (size_t)n < cap ? 0 : -1;
}
int hal_transfer_listen(uint16_t *port) { (void)port; return -1; }
int hal_transfer_accept(void) { return -1; }
int hal_transfer_read(void *data, size_t cap) { (void)data; (void)cap; return -1; }
int hal_transfer_write(const void *data, size_t len) { (void)data; (void)len; return -1; }
void hal_transfer_close(int listener) { (void)listener; }
int hal_transfer_qr(const char *url, const char *code, void (*pixel)(void *, int, int, bool), void *ctx) {
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(8)], qr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
    char text[128]; int n = snprintf(text, sizeof text, "%s#code=%s", url, code);
    if (n < 0 || n >= (int)sizeof text || !pixel || !qrcodegen_encodeText(text, temp, qr, qrcodegen_Ecc_LOW, 1, 8, qrcodegen_Mask_AUTO, true)) return -1;
    int side = qrcodegen_getSize(qr);
    for (int y = -4; y < side + 4; y++) for (int x = -4; x < side + 4; x++) pixel(ctx, x + 4, y + 4, qrcodegen_getModule(qr, x, y));
    return side + 8;
}
