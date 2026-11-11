/* BH1098 @874538e: docs/bh1098-audio-protocol.md:23-61,98-136.
 * UART1, including audio mode changes, is owned here. No undocumented AT commands. */
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "bh1098g/bh1098g.h"
#include "ink/hal_bt.h"
#include "ink/hal_device.h"
#include "ink/osal.h"
#include "metalio_plus/metalio_board.h"
#include "s31_bh1098.h"

static struct {
    osal_mutex_t *lock;
    hal_uart_t *uart;
    unsigned epoch;
    int mode;
    bool suspended, connected, capture, call, sco;
    char addr[18];
    void (*event)(void *, hal_bt_event_t);
    void *ctx;
} B;

void ink_s31_bh_init(void) {
    B.lock = osal_mutex_create();
    B.epoch = UINT_MAX;
    if (!B.lock) osal_panic("BH1098: mutex setup failed");
}
static bool ink_lock(void) {
    return B.lock && osal_mutex_lock(B.lock, OSAL_WAIT_FOREVER) == OSAL_OK;
}
static void ink_unlock(void) { osal_mutex_unlock(B.lock); }
static bool ink_powered(void) {
    return !B.suspended && metalio_board_ready() && !metalio_board_rails_cut() &&
        (metalio_board_output(1) & MB_P1_SHARED_PWR);
}
static int ink_send(const char *cmd) {
    size_t n = strlen(cmd);
    return hal_uart_write(B.uart, cmd, n) == (int)n ? 0 : -1;
}
static void ink_link(bool on) {
    __atomic_store_n(&B.connected, on, __ATOMIC_RELEASE);
    if (!on) { B.addr[0] = 0; B.sco = false; }
}
static void ink_response(const char *line) {
    /* bluetooth_uart.cc:243-255: mode 1 ends the headset connection. */
    if (!strcmp(line, "SET MODE 1")) {
        B.mode = 1;
        ink_link(false);
    }
}
static int ink_read(char *line, size_t cap, uint32_t ms) {
    int n = hal_uart_read_line(B.uart, line, cap, ms);
    if (n < 0) return n;
    size_t start = 0, end = strlen(line);
    while (start < end && line[start] == ' ') start++;
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\r')) end--;
    memmove(line, line + start, end - start);
    line[end - start] = 0;
    ink_response(line);
    return (int)(end - start);
}
static int ink_wait(const char *reply, uint32_t ms) {
    char line[128];
    uint64_t end = osal_now_us() + (uint64_t)ms * 1000;
    while (osal_now_us() < end) {
        if (ink_read(line, sizeof line, 10) < 0) continue;
        if (strstr(line, "ERROR") || strstr(line, "FAIL") || !strcmp(line, "CONNECT TIMEOUT")) return -1;
        if (!strcmp(line, reply)) return 0;
    }
    return -1;
}
static int ink_mode(int mode) {
    /* bluetooth_uart.cc:365-385; metalio_audio.cc:24-27,94-126. */
    if (B.mode == mode) return 0;
    if (metalio_board_set(0, MB_P0_AMP, 0) != HAL_I2C_OK) return -1;
    char ack[16];
    snprintf(ack, sizeof ack, "SET MODE %d", mode);
    B.mode = 0;
    if (bh1098g_set_mode(B.uart, mode) || ink_wait(ack, 1000)) return -1;
    B.mode = mode;
    B.sco = false;
    return 0;
}
static bool ink_ready(void) {
    if (!ink_powered()) return false;
    unsigned epoch = metalio_board_shared_power_epoch();
    if (B.epoch != epoch || !B.uart || !B.mode) {
        /* metalio_audio.cc:25-27,153-161,225-235: settle once per power epoch. */
        ink_link(false);
        B.mode = 0;
        uint64_t ready = metalio_board_shared_power_since_us() + BH1098G_BOOT_US;
        uint64_t now = osal_now_us();
        if (now < ready) osal_task_delay_us(ready - now);
        B.uart = hal_uart_open(BH1098G_UART_PORT, BH1098G_BAUD);
        if (!B.uart) return false;
        if (!ink_s31_uart_discard(B.uart)) return false;
        if (ink_mode(1)) return false;
        B.epoch = epoch;
    }
    return B.mode != 0;
}
bool ink_s31_bh_ready(void) {
    if (!ink_lock()) return false;
    bool ok = ink_ready();
    ink_unlock();
    return ok;
}
bool ink_s31_bh_headset(void) { return __atomic_load_n(&B.connected, __ATOMIC_ACQUIRE); }
bool ink_s31_bh_amp(bool on) {
    if (!ink_lock()) return false;
    bool ok = metalio_board_set(0, MB_P0_AMP, on && !B.connected ? MB_P0_AMP : 0) == HAL_I2C_OK;
    ink_unlock();
    return ok;
}
static int ink_sco(bool on) {
    /* bluetooth_uart.cc:439-466: fixed 200 ms spacing; also check documented reports. */
    if (on == B.sco) return 0;
    if (on) {
        B.sco = true; /* The normal stop path must close SCO even after an ACK timeout. */
        if (bh1098g_headset_call(B.uart) || ink_wait("SETUP SCO", 1000)) return -1;
    } else {
        if (ink_send("AT+BTSCO=0\r\n")) return -1;
        osal_task_delay_us(200000);
        if (ink_send("AT+PP=1\r\n") || ink_wait("DISC SCO", 1000)) return -1;
    }
    B.sco = on;
    return 0;
}
int ink_s31_bh_capture(bool on) {
    if (!ink_lock()) return -1;
    int rc = B.connected ? ink_sco(on || B.call) : 0;
    if (!rc || !on) B.capture = on;
    ink_unlock();
    return rc;
}
void ink_s31_bh_call(bool on) {
    if (!ink_lock()) return;
    B.call = on;
    if (B.connected && ink_sco(on || B.capture)) hal_log_write("BH1098: call audio change failed");
    ink_unlock();
}
int ink_s31_bh_route(hal_audio_route_t route) {
    if (!ink_lock()) return -1;
    int rc = -1;
    if (route == HAL_ROUTE_BT_HEADSET) rc = B.connected && B.mode == 2 ? 0 : -1;
    else if (route == HAL_ROUTE_SPEAKER) {
        rc = !B.mode && !B.connected ? 0 : ink_ready() ? ink_mode(1) : -1;
        if (!rc) ink_link(false);
    }
    ink_unlock();
    return rc;
}
bool hal_bt_available(void) { return B.lock && metalio_board_ready(); }
static bool ink_address(const char *addr, char hex[13]) {
    if (!addr || strlen(addr) != 17) return false;
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) { if (addr[i] != ':') return false; }
        else {
            if (!isxdigit((unsigned char)addr[i])) return false;
            hex[i - i / 3] = (char)toupper((unsigned char)addr[i]);
        }
    }
    hex[12] = 0;
    return true;
}
int hal_bt_scan(hal_bt_device_t *out, int max, int timeout_ms) {
    if (!out || max <= 0 || timeout_ms <= 0 || !ink_lock()) return -1;
    int count = -1;
    bool linked = B.connected;
    if (!ink_ready() || B.connected || ink_mode(2) || ink_send("AT+INQUIRING\r\n")) goto done;
    /* bluetooth_screen.cc:654-669; bluetooth_uart.cc:72-89,278-305. */
    uint64_t end = osal_now_us() + (uint64_t)timeout_ms * 1000;
    int found = 0;
    char line[160];
    while (osal_now_us() < end) {
        if (ink_read(line, sizeof line, 10) < 0) continue;
        if (!strcmp(line, "INQ COMPLETE")) { count = found; break; }
        if (strncmp(line, "AT+BT:", 6) || strlen(line) < 18) continue;
        bool valid = true;
        for (int i = 6; i < 18; i++) valid &= isxdigit((unsigned char)line[i]) != 0;
        if (!valid || found == max) continue;
        hal_bt_device_t d = {0};
        for (int i = 0; i < 6; i++) {
            d.addr[i * 3] = (char)toupper((unsigned char)line[6 + i * 2]);
            d.addr[i * 3 + 1] = (char)toupper((unsigned char)line[7 + i * 2]);
            if (i < 5) d.addr[i * 3 + 2] = ':';
        }
        size_t n = strlen(line + 18);
        if (n >= sizeof d.name) {
            n = sizeof d.name - 1;
            while (n && ((unsigned char)line[18 + n] & 0xc0) == 0x80) n--;
        }
        memcpy(d.name, line + 18, n);
        bool duplicate = false;
        for (int i = 0; i < found; i++) duplicate |= !strcmp(out[i].addr, d.addr);
        if (!duplicate) out[found++] = d;
    }
done:
    if (!linked && B.uart && ink_powered() && ink_mode(1)) count = -1;
    ink_unlock();
    return count;
}
static int ink_connect(const char *addr) {
    char hex[13], cmd[32];
    if (!ink_address(addr, hex) || !ink_ready()) return -1;
    if (B.connected) return !strcmp(B.addr, addr) ? 0 : -1;
    /* bluetooth_screen.cc:504-509; protocol.md:106-118. Pairing is part of CONNECT. */
    snprintf(cmd, sizeof cmd, "AT+CONNECT=%s\r\n", hex);
    if (ink_mode(2) || ink_send(cmd) || ink_wait("CONNECT SUCCESS", 15000) || ((B.capture || B.call) && ink_sco(true))) {
        ink_mode(1);
        return -1;
    }
    snprintf(B.addr, sizeof B.addr, "%s", addr);
    ink_link(true);
    return 0;
}
int hal_bt_pair(const char *addr) {
    if (!ink_lock()) return -1;
    int rc = ink_connect(addr);
    if (!rc) { rc = ink_mode(1); ink_link(false); }
    ink_unlock();
    return rc;
}
int hal_bt_connect(const char *addr) {
    if (!ink_lock()) return -1;
    int rc = ink_connect(addr);
    void (*event)(void *, hal_bt_event_t) = B.event;
    void *ctx = B.ctx;
    ink_unlock();
    if (!rc) ink_s31_bh_wake();
    if (!rc && event) event(ctx, HAL_BT_EV_CONNECTED);
    return rc;
}
void hal_bt_disconnect(void) { (void)ink_s31_bh_route(HAL_ROUTE_SPEAKER); }
void hal_bt_forget(const char *addr) {
    if (!ink_lock()) return;
    /* protocol.md:112-114: chip-side pairing removal is unspecified; host DB owns forgetting. */
    if (addr && B.connected && !strcmp(B.addr, addr) && !ink_mode(1)) ink_link(false);
    ink_unlock();
}
void hal_bt_on_event(void (*fn)(void *, hal_bt_event_t), void *ctx) {
    if (!ink_lock()) return;
    B.event = fn;
    B.ctx = ctx;
    ink_unlock();
}
void ink_s31_bh_poll(void) {
    if (!B.lock || osal_mutex_lock(B.lock, 0) != OSAL_OK) return;
    bool linked = B.connected;
    char line[128];
    if (B.uart && ink_powered()) {
        for (int i = 0; i < 4; i++) {
            if (ink_read(line, sizeof line, 0) < 0) break;
        }
    }
    void (*event)(void *, hal_bt_event_t) = B.event;
    void *ctx = B.ctx;
    bool lost = linked && !B.connected;
    ink_unlock();
    if (lost && event) event(ctx, HAL_BT_EV_LOST);
}
void ink_s31_bh_suspend(void) {
    if (!ink_lock()) return;
    bool linked = B.connected;
    B.suspended = true;
    B.mode = 0;
    ink_link(false);
    void (*event)(void *, hal_bt_event_t) = B.event;
    void *ctx = B.ctx;
    ink_unlock();
    if (linked && event) event(ctx, HAL_BT_EV_LOST);
}
void ink_s31_bh_resume(void) {
    if (!ink_lock()) return;
    B.suspended = false;
    B.uart = NULL;
    B.epoch = UINT_MAX;
    ink_unlock();
}
