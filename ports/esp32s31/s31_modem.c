/* Copyright 2025 Terrence. SPDX-License-Identifier: Apache-2.0
 * C adaptation of uart_eth_modem.cc/.h @690a023; see vendor/nt26/NOTICE.
 * Header: .h:216-265; MRDY/SRDY: .cc:739-832,1149-1236,1750-1767;
 * reassembly: .cc:973-1086; Ethernet/AT: .cc:1360-1454. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_rom_sys.h"
#include "ink/hal_net.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "s31_board.h"
#include "s31_cell.h"

#define INK_FRAME_MAX 1600
#define INK_URC_MAX 160
#define INK_MRDY 60
#define INK_UART UART_NUM_2
typedef struct { size_t len; unsigned epoch; uint64_t deadline; uint8_t data[]; } ink_frame_t;
static struct {
    osal_mutex_t *at, *lock, *io;
    osal_sem_t *done;
    osal_queue_t *tx, *urc;
    esp_netif_t *netif;
    bool ready, suspended, waiting, prompt, desynced, handshake, ip, registered, selected, starting, net_connected;
    unsigned epoch, seq;
    int signal_dbm;
    uint64_t activity, rx_bytes, tx_bytes, deadline, ready_after;
    hal_at_result_t result;
    size_t frame_n, expected, line_n, response_n;
    char command[128], response[4096], line[1024];
    uint8_t frame[INK_FRAME_MAX], chunk[256];
} ink_cell EXT_RAM_BSS_ATTR;
static unsigned ink_ack;
static void ink_urc(const char *line);
#define CELL_LOCK() osal_mutex_lock(ink_cell.lock, OSAL_WAIT_FOREVER)
#define CELL_UNLOCK() osal_mutex_unlock(ink_cell.lock)

/* Deliberately finite: neither arbitrary AT text nor command chaining can reach UART2. */
static bool ink_allowed(const char *cmd) {
    static const char *const exact[] = {
        "AT", "ATI", "AT+CGMM", "AT+CGMR", "AT+CPIN?", "AT+CEREG?", "AT+CSQ",
        "AT+ECSIMCFG?", "AT+ECNETCFG?", "AT+ECNETDEVCTL?", "AT+CMGF?", "AT+CMGF=?",
        "AT+CSMS=?", "AT+CNMI?", "AT+CIREG?", "AT+CIREG=?", "AT+CLCC", "AT+CMUT?",
        "AT+CFUN=1", "AT+CFUN=4", "AT+ECNETDEVCTL=2,1,1", "AT+CMGF=0",
        "ATA", "AT+CHUP", "AT+CMUT=0", "AT+CMUT=1"
    };
    if (!cmd) return false;
    for (unsigned i = 0; i < sizeof exact / sizeof *exact; i++) if (!strcmp(cmd, exact[i])) return true;
    if (!strncmp(cmd, "AT+CMGS=", 8) || !strncmp(cmd, "AT+CMGR=", 8)) {
        const char *p = cmd + 8;
        unsigned n = 0;
        for (; *p >= '0' && *p <= '9' && n < 5; p++) n++;
        return n && !*p;
    }
    if (!strncmp(cmd, "ATD", 3)) {
        const char *p = cmd + 3;
        if (*p == '+') p++;
        unsigned n = 0;
        for (; *p >= '0' && *p <= '9' && n < 20; p++) n++;
        return n && !strcmp(p, ";");
    }
    return strlen(cmd) == 10 && !strncmp(cmd, "AT+VTS=\"", 8) &&
        strchr("0123456789*#ABCD", cmd[8]) && !strcmp(cmd + 9, "\"");
}

static void ink_link_down(void) { /* lock held */
    ink_cell.handshake = ink_cell.ip = ink_cell.registered = false;
    ink_s31_net_changed();
}
static void ink_reset(void) { /* lock held; never toggles P12 or P14 */
    ink_cell.epoch++;
    ink_cell.seq = 0;
    ink_cell.frame_n = ink_cell.expected = ink_cell.line_n = 0;
    ink_cell.desynced = false;
    if (ink_cell.waiting) { ink_cell.result = HAL_AT_NO_MODEM; ink_cell.waiting = false; osal_sem_give(ink_cell.done); }
    ink_link_down();
    ink_urc("+CEREG: 0");
}
static void ink_urc(const char *line) {
    char item[INK_URC_MAX];
    if (strlen(line) >= sizeof item) return;
    snprintf(item, sizeof item, "%s", line);
    osal_queue_send(ink_cell.urc, item, 0);
}
static void ink_line(void) { /* lock held */
    char *s = ink_cell.line;
    if (!*s) return;
    if (!strcmp(s, "ECRDY") || !strcmp(s, "+ECRDY")) { ink_reset(); return; }
    bool final = !strcmp(s, "OK") || !strcmp(s, "ERROR") || !strncmp(s, "+CME ERROR", 10) || !strncmp(s, "+CMS ERROR", 10);
    bool urc = !strcmp(s, "RING") || !strcmp(s, "NO CARRIER") || !strncmp(s, "+CMTI:", 6) ||
        !strncmp(s, "+CLIP:", 6) || (!strncmp(s, "+CLCC:", 6) && (!ink_cell.waiting || strcmp(ink_cell.command, "AT+CLCC"))) ||
        (!strncmp(s, "+CEREG:", 7) && (!ink_cell.waiting || strcmp(ink_cell.command, "AT+CEREG?")));
    if (urc) ink_urc(s);
    if (final && ink_cell.desynced) { ink_cell.desynced = false; return; }
    bool call_final = !strcmp(s, "NO CARRIER") && (!strncmp(ink_cell.command, "ATD", 3) || !strcmp(ink_cell.command, "ATA"));
    if (!ink_cell.waiting || (urc && !call_final) || !strcmp(s, ink_cell.command)) return;
    if (final || !strcmp(s, "NO CARRIER") || !strcmp(s, "BUSY") || !strcmp(s, "NO ANSWER") || !strcmp(s, "NO DIALTONE")) {
        if (!strncmp(s, "+CME ERROR", 10) || !strncmp(s, "+CMS ERROR", 10)) {
            ink_cell.response_n = strlen(s);
            memcpy(ink_cell.response, s, ink_cell.response_n + 1);
        }
        ink_cell.result = !strcmp(s, "OK") ? HAL_AT_OK : HAL_AT_ERROR;
        ink_cell.waiting = false;
        osal_sem_give(ink_cell.done);
    } else {
        size_t n = strlen(s), at = ink_cell.response_n;
        if (at + n + 2 >= sizeof ink_cell.response) {
            ink_cell.result = HAL_AT_ERROR;
            ink_cell.waiting = false;
            ink_cell.desynced = true;
            osal_sem_give(ink_cell.done);
            return;
        }
        if (at) ink_cell.response[at++] = '\n';
        memcpy(ink_cell.response + at, s, n + 1);
        ink_cell.response_n = at + n;
    }
}
static void ink_dispatch(void) {
    uint8_t *p = ink_cell.frame + 4;
    size_t n = ink_cell.expected - 4;
    unsigned type = (ink_cell.frame[2] >> 2) & 3;
    CELL_LOCK();
    if (type == 1) for (size_t i = 0; i < n; i++) {
        char c = (char)p[i];
        if (c == '>' && !ink_cell.line_n && ink_cell.waiting) { ink_cell.prompt = true; osal_sem_give(ink_cell.done); }
        else if (c == '\r' || c == '\n') {
            ink_cell.line[ink_cell.line_n] = 0;
            ink_line();
            ink_cell.line_n = 0;
        } else if (ink_cell.line_n + 1 < sizeof ink_cell.line && !(c == ' ' && !ink_cell.line_n))
            ink_cell.line[ink_cell.line_n++] = c;
    }
    if (type == 0 && !ink_cell.handshake) {
        static const uint8_t ack[] = {0x53, 0x50, 0x49, 0x43, 0x01, 0x80};
        if (n >= sizeof ack && !memcmp(p, ack, sizeof ack)) {
            ink_cell.handshake = true;
            ink_s31_net_changed();
        }
    } else if (type == 0 && n >= 14 && ink_cell.netif && ink_cell.selected && ink_cell.registered) {
        uint8_t *copy = osal_malloc(n, OSAL_MEM_EXTERNAL);
        if (copy) {
            memcpy(copy, p, n);
            esp_netif_t *netif = ink_cell.netif;
            CELL_UNLOCK();
            esp_err_t r = esp_netif_receive(netif, copy, n, NULL);
            CELL_LOCK();
            if (r != ESP_OK) osal_free(copy);
            else ink_cell.rx_bytes += n;
        }
    }
    CELL_UNLOCK();
    gpio_set_level(INK_MRDY, 1);
    esp_rom_delay_us(50); /* upstream SendAckPulse, .cc:1761-1767 */
    gpio_set_level(INK_MRDY, 0);
}
static void ink_feed(const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ink_cell.frame[ink_cell.frame_n++] = data[i];
        if (ink_cell.frame_n == 4) {
            unsigned sum = ink_cell.frame[0] + ink_cell.frame[1] + ink_cell.frame[2];
            ink_cell.expected = 4 + ink_cell.frame[0] + ((ink_cell.frame[1] & 15u) << 8);
            if (ink_cell.frame[3] != (uint8_t)((sum >> 8) ^ sum ^ 3) || ink_cell.expected > INK_FRAME_MAX) {
                ink_cell.frame_n = ink_cell.expected = 0;
                return; /* same discard policy as upstream .cc:994-999 */
            }
        }
        if (ink_cell.expected && ink_cell.frame_n == ink_cell.expected) {
            ink_dispatch();
            ink_cell.frame_n = ink_cell.expected = 0;
        }
    }
}
static void ink_receive(void) {
    int n;
    do {
        n = uart_read_bytes(INK_UART, ink_cell.chunk, sizeof ink_cell.chunk, 0);
        if (n > 0) { ink_cell.activity = osal_now_us(); ink_feed(ink_cell.chunk, (size_t)n); }
    } while (n == (int)sizeof ink_cell.chunk);
}
static void ink_srdy(void *arg) {
    (void)arg;
    if (gpio_get_level(S31_GPIO_MODEM_SRDY)) __atomic_add_fetch(&ink_ack, 1, __ATOMIC_RELEASE);
}
static bool ink_enqueue(const void *data, size_t n, unsigned type) {
    if (n > INK_FRAME_MAX - 4) return false;
    ink_frame_t *f = osal_malloc(sizeof *f + n + 4, OSAL_MEM_EXTERNAL);
    if (!f) return false;
    CELL_LOCK();
    f->epoch = ink_cell.epoch;
    f->deadline = type == 1 ? ink_cell.deadline : 0;
    f->len = n + 4;
    f->data[0] = n & 255;
    f->data[1] = ((n >> 8) & 15) | ((ink_cell.seq++ & 15) << 4);
    f->data[2] = (uint8_t)(type << 2);
    unsigned sum = f->data[0] + f->data[1] + f->data[2];
    f->data[3] = (uint8_t)((sum >> 8) ^ sum ^ 3);
    memcpy(f->data + 4, data, n);
    bool ok = !ink_cell.suspended && osal_queue_send(ink_cell.tx, &f, 0) == OSAL_OK;
    CELL_UNLOCK();
    if (!ok) osal_free(f);
    return ok;
}
static void ink_step(void) {
    osal_mutex_lock(ink_cell.io, OSAL_WAIT_FOREVER);
    if (ink_cell.suspended || osal_now_us() < ink_cell.ready_after) { osal_mutex_unlock(ink_cell.io); return; }
    if (!gpio_get_level(S31_GPIO_MODEM_SRDY)) gpio_set_level(INK_MRDY, 0);
    ink_receive();
    ink_frame_t *f;
    if (osal_queue_recv(ink_cell.tx, &f, 0) == OSAL_OK) {
        gpio_set_level(INK_MRDY, 0);
        uint64_t until = osal_now_us() + 200000;
        while (gpio_get_level(S31_GPIO_MODEM_SRDY) && osal_now_us() < until) { ink_receive(); osal_task_delay_us(1000); }
        CELL_LOCK();
        bool send = !ink_cell.suspended && f->epoch == ink_cell.epoch && !gpio_get_level(S31_GPIO_MODEM_SRDY) &&
            (!f->deadline || osal_now_us() < f->deadline);
        CELL_UNLOCK();
        if (send) {
            unsigned ack = __atomic_load_n(&ink_ack, __ATOMIC_ACQUIRE);
            int wrote = uart_write_bytes(INK_UART, f->data, f->len);
            if (wrote == (int)f->len) {
                until = osal_now_us() + 100000;
                while (__atomic_load_n(&ink_ack, __ATOMIC_ACQUIRE) == ack && osal_now_us() < until) { ink_receive(); osal_task_delay_us(1000); }
                /* Upstream .cc:817 assumes delivery on ACK timeout; never repeat a paid action. */
                if (!(f->data[2] & 12)) { CELL_LOCK(); ink_cell.tx_bytes += f->len - 4; CELL_UNLOCK(); }
            }
        }
        osal_free(f);
        ink_cell.activity = osal_now_us();
    }
    if (osal_now_us() - ink_cell.activity >= 500000) gpio_set_level(INK_MRDY, 1);
    else if (!gpio_get_level(S31_GPIO_MODEM_SRDY)) gpio_set_level(INK_MRDY, 0);
    osal_mutex_unlock(ink_cell.io);
}
static void ink_io(void *arg) {
    (void)arg;
    for (;;) { ink_step(); osal_task_delay_us(1000); }
}
void ink_s31_cell_init(void) { /* board boot, once; nt26_board.cc:130 settles P14 for 1s */
    ink_cell.at = osal_mutex_create(); ink_cell.lock = osal_mutex_create(); ink_cell.io = osal_mutex_create();
    ink_cell.done = osal_sem_create(0, 1);
    ink_cell.tx = osal_queue_create(sizeof(ink_frame_t *), 8);
    ink_cell.urc = osal_queue_create(INK_URC_MAX, 8);
    if (!ink_cell.at || !ink_cell.lock || !ink_cell.io || !ink_cell.done || !ink_cell.tx || !ink_cell.urc) osal_panic("cell: no memory");
    if (metalio_board_set(1, MB_P1_SHARED_RST | MB_P1_SHARED_PWR, MB_P1_SHARED_RST | MB_P1_SHARED_PWR) != HAL_I2C_OK) return;
    uart_config_t u = {.baud_rate = 2000000, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    gpio_config_t pins = {.pin_bit_mask = 1ull << INK_MRDY, .mode = GPIO_MODE_OUTPUT};
    if (gpio_config(&pins) != ESP_OK || gpio_set_level(INK_MRDY, 1) != ESP_OK) return;
    pins = (gpio_config_t){.pin_bit_mask = 1ull << S31_GPIO_MODEM_SRDY, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_ANYEDGE};
    if (gpio_config(&pins) != ESP_OK) return;
    esp_err_t r = gpio_install_isr_service(0);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) return;
    if (gpio_isr_handler_add(S31_GPIO_MODEM_SRDY, ink_srdy, NULL) != ESP_OK ||
        uart_driver_install(INK_UART, 4096, 0, 0, NULL, 0) != ESP_OK || uart_param_config(INK_UART, &u) != ESP_OK ||
        uart_set_pin(INK_UART, S31_UART2_TX, S31_UART2_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return;
    ink_cell.ready = true;
    ink_cell.ready_after = metalio_board_shared_power_since_us() + 1000000;
    osal_task_cfg_t cfg = {.name = "nt26-uart", .fn = ink_io, .stack_bytes = 4096, .priority = 5, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) { ink_cell.ready = false; hal_log_write("cell: UART task failed"); }
}
static hal_at_result_t ink_at(const char *cmd, const char *payload, char *out, size_t cap, uint32_t ms) {
    if (out && cap) *out = 0;
    if (!ink_allowed(cmd)) return HAL_AT_ERROR;
    if (payload) {
        if (strncmp(cmd, "AT+CMGS=", 8) || strlen(payload) > 1024) return HAL_AT_ERROR;
        for (const char *p = payload; *p; p++) if (!strchr("0123456789ABCDEFabcdef", *p)) return HAL_AT_ERROR;
    }
    if (!ink_cell.ready) return HAL_AT_NO_MODEM;
    uint64_t end = osal_now_us() + (uint64_t)ms * 1000;
    uint64_t now = osal_now_us();
    if (now < ink_cell.ready_after) {
        uint64_t wait = ink_cell.ready_after - now;
        if (wait >= (uint64_t)ms * 1000) return HAL_AT_TIMEOUT;
        osal_task_delay_us(wait);
    }
    int64_t left_us = (int64_t)end - (int64_t)osal_now_us();
    if (left_us <= 0 || osal_mutex_lock(ink_cell.at, left_us) != OSAL_OK) return HAL_AT_TIMEOUT;
    CELL_LOCK();
    if (ink_cell.suspended || ink_cell.desynced) { CELL_UNLOCK(); osal_mutex_unlock(ink_cell.at); return HAL_AT_NO_MODEM; }
    while (osal_sem_take(ink_cell.done, 0) == OSAL_OK) { }
    ink_cell.waiting = true; ink_cell.prompt = false; ink_cell.response_n = 0; ink_cell.response[0] = 0;
    snprintf(ink_cell.command, sizeof ink_cell.command, "%s", cmd);
    ink_cell.result = HAL_AT_TIMEOUT;
    ink_cell.deadline = end;
    CELL_UNLOCK();
    char command[130];
    int len = snprintf(command, sizeof command, "%s\r", cmd);
    bool queued = ink_enqueue(command, (size_t)len, 1), sent_payload = false;
    while (queued && osal_now_us() < end) {
        int64_t left = (int64_t)end - (int64_t)osal_now_us();
        if (left <= 0 || osal_sem_take(ink_cell.done, left) != OSAL_OK) break;
        CELL_LOCK();
        bool done = !ink_cell.waiting, prompt = ink_cell.prompt;
        CELL_UNLOCK();
        if (done) break;
        if (prompt && payload && !sent_payload) {
            size_t n = strlen(payload);
            char *p = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
            if (!p) break;
            memcpy(p, payload, n); p[n] = 26;
            queued = ink_enqueue(p, n + 1, 1); osal_free(p); sent_payload = true;
        }
    }
    CELL_LOCK();
    hal_at_result_t result = queued ? ink_cell.result : HAL_AT_ERROR;
    if (ink_cell.waiting) { ink_cell.desynced = queued; ink_cell.waiting = false; }
    if (out && cap) {
        if (ink_cell.response_n >= cap) result = HAL_AT_ERROR;
        else memcpy(out, ink_cell.response, ink_cell.response_n + 1);
    }
    CELL_UNLOCK();
    osal_mutex_unlock(ink_cell.at);
    return result;
}
hal_at_result_t hal_modem_at(const char *cmd, char *resp, size_t cap, uint32_t ms) { return ink_at(cmd, NULL, resp, cap, ms); }
hal_at_result_t hal_modem_at_payload(const char *cmd, const char *payload, char *resp, size_t cap, uint32_t ms) {
    return payload ? ink_at(cmd, payload, resp, cap, ms) : HAL_AT_ERROR;
}
int hal_modem_urc(char *buf, size_t cap, uint32_t ms) {
    if (!ink_cell.ready) { osal_task_delay_us((uint64_t)ms * 1000); return -1; }
    char line[INK_URC_MAX];
    if (!buf || !cap || osal_queue_recv(ink_cell.urc, line, (int64_t)ms * 1000) != OSAL_OK) return -1;
    size_t n = strlen(line);
    if (n >= cap) return -1;
    memcpy(buf, line, n + 1);
    return (int)n;
}
static esp_err_t ink_eth_tx(void *ctx, void *data, size_t n) {
    (void)ctx;
    CELL_LOCK(); bool connected = ink_cell.handshake && ink_cell.selected && ink_cell.registered; CELL_UNLOCK();
    if (!connected) return ESP_FAIL;
    return ink_enqueue(data, n, 0) ? ESP_OK : ESP_FAIL;
}
static void ink_eth_free(void *ctx, void *data) { (void)ctx; osal_free(data); }
static void ink_ip(void *ctx, esp_event_base_t base, int32_t id, void *data) {
    (void)ctx; (void)base;
    CELL_LOCK();
    if (id == IP_EVENT_ETH_GOT_IP && ink_cell.selected && ((ip_event_got_ip_t *)data)->esp_netif == ink_cell.netif) ink_cell.ip = true;
    if (id == IP_EVENT_ETH_LOST_IP) ink_cell.ip = false;
    CELL_UNLOCK();
    ink_s31_net_changed();
}
void ink_s31_cell_netif_init(void) { /* same ETH/DHCP flags as upstream InitIotEth .cc:588-655 */
    if (!ink_cell.ready || ink_cell.netif) return;
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.flags &= ~ESP_NETIF_FLAG_GARP;
    base.if_key = "INK_NT26"; base.if_desc = "nt26"; base.route_prio = 10;
    esp_netif_driver_ifconfig_t driver = {.transmit = ink_eth_tx, .driver_free_rx_buffer = ink_eth_free};
    esp_netif_config_t cfg = {.base = &base, .driver = &driver, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    ink_cell.netif = esp_netif_new(&cfg);
    if (!ink_cell.netif) return;
    uint8_t mac[6]; /* same derivation as upstream .cc:25-26; never logged */
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_ETH));
    mac[5] ^= 1;
    ESP_ERROR_CHECK(esp_netif_set_mac(ink_cell.netif, mac));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, ink_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, ink_ip, NULL));
    esp_netif_action_start(ink_cell.netif, NULL, 0, NULL);
    ink_s31_net_changed();
}
void ink_s31_cell_sync(void) { /* network actions must never run under the AT/RX state lock */
    if (!ink_cell.ready || !ink_cell.netif) return;
    CELL_LOCK();
    bool connected = ink_cell.handshake && ink_cell.selected && ink_cell.registered && !ink_cell.suspended;
    CELL_UNLOCK();
    if (connected == ink_cell.net_connected) return;
    ink_cell.net_connected = connected;
    if (connected) esp_netif_action_connected(ink_cell.netif, NULL, 0, NULL);
    else esp_netif_action_disconnected(ink_cell.netif, NULL, 0, NULL);
}
void ink_s31_cell_use(bool on) {
    if (!ink_cell.ready) return;
    CELL_LOCK(); ink_cell.selected = on;
    if (!on) ink_cell.ip = false;
    CELL_UNLOCK();
    if (on && ink_cell.netif) esp_netif_set_default_netif(ink_cell.netif);
    ink_s31_net_changed();
    if (on) ink_s31_cell_start_data();
}
bool ink_s31_cell_available(void) {
    if (!ink_cell.ready) return false;
    CELL_LOCK(); bool yes = ink_cell.registered && !ink_cell.suspended; CELL_UNLOCK();
    return yes;
}
bool ink_s31_cell_link(hal_link_status_t *out) {
    if (!ink_cell.ready) return false;
    CELL_LOCK(); *out = (hal_link_status_t){.type = HAL_LINK_LTE, .up = ink_cell.ip && ink_cell.handshake && ink_cell.registered, .signal_dbm = ink_cell.signal_dbm}; CELL_UNLOCK();
    return true;
}
bool hal_net_lte_bytes(uint64_t *rx, uint64_t *tx) {
    if (!ink_cell.ready) return false;
    CELL_LOCK(); *rx = ink_cell.rx_bytes; *tx = ink_cell.tx_bytes; CELL_UNLOCK();
    return true;
}
bool ink_s31_cell_suspend(void) {
    if (!ink_cell.ready) return true;
    if (osal_mutex_lock(ink_cell.at, 0) != OSAL_OK) return false;
    if (osal_mutex_lock(ink_cell.io, 0) != OSAL_OK) { osal_mutex_unlock(ink_cell.at); return false; }
    CELL_LOCK(); ink_cell.suspended = true; ink_reset(); CELL_UNLOCK();
    gpio_set_level(INK_MRDY, 1);
    osal_mutex_unlock(ink_cell.io);
    osal_mutex_unlock(ink_cell.at);
    return true;
}
void ink_s31_cell_resume(void) {
    if (!ink_cell.ready) return;
    osal_mutex_lock(ink_cell.io, OSAL_WAIT_FOREVER);
    uart_flush_input(INK_UART);
    CELL_LOCK(); ink_cell.suspended = false;
    ink_cell.ready_after = metalio_board_shared_power_since_us() + 1000000;
    CELL_UNLOCK();
    osal_mutex_unlock(ink_cell.io);
}
void ink_s31_cell_update(bool registered, int signal_dbm) {
    CELL_LOCK();
    bool changed = ink_cell.registered != registered || ink_cell.signal_dbm != signal_dbm;
    if (!registered && ink_cell.registered) ink_link_down();
    ink_cell.registered = registered; ink_cell.signal_dbm = signal_dbm;
    CELL_UNLOCK();
    if (changed) ink_s31_net_changed();
}
void ink_s31_cell_start_data(void) { /* upstream RunNormalModeInitSequence .cc:1637-1676 */
    CELL_LOCK();
    bool started = ink_cell.handshake, registered = ink_cell.registered && ink_cell.selected;
    bool busy = ink_cell.starting;
    if (!started && registered && !busy) ink_cell.starting = true;
    unsigned epoch = ink_cell.epoch;
    CELL_UNLOCK();
    if (started || !registered || busy) return;
    char response[96];
    if (hal_modem_at("AT+ECNETCFG?", response, sizeof response, 1000) != HAL_AT_OK || !strstr(response, "\"nat\",1")) goto done;
    if (hal_modem_at("AT+ECNETDEVCTL?", response, sizeof response, 1000) != HAL_AT_OK) goto done;
    int state = 0;
    const char *p = strstr(response, "+ECNETDEVCTL:");
    if (p) sscanf(p, "+ECNETDEVCTL: %*d,%*d,%*d,%d", &state);
    if (state == 1) {
        CELL_LOCK();
        if (epoch == ink_cell.epoch) ink_cell.handshake = true;
        CELL_UNLOCK();
        ink_s31_net_changed();
        goto done;
    }
    CELL_LOCK(); bool current = epoch == ink_cell.epoch && !ink_cell.suspended; CELL_UNLOCK();
    if (!current || hal_modem_at("AT+ECNETDEVCTL=2,1,1", response, sizeof response, 5000) != HAL_AT_OK) goto done;
    static const uint8_t request[] = {0x53, 0x50, 0x49, 0x43, 2, 4, 12, 0, 0, 0, 0, 0};
    if (!ink_enqueue(request, sizeof request, 0)) goto done;
    uint64_t end = osal_now_us() + 5000000;
    do {
        osal_task_delay_us(1000);
        CELL_LOCK(); started = ink_cell.handshake; CELL_UNLOCK();
    } while (!started && osal_now_us() < end);
done:
    CELL_LOCK(); ink_cell.starting = false; CELL_UNLOCK();
}
