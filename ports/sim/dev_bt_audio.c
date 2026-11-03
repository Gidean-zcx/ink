/*
 * BH1098G Bluetooth audio chip on UART1, and hal_uart for that port. Behaviour from the official
 * firmware (see drivers/bh1098g; no datasheet in hand): CR LF lines, no acknowledgements for mode
 * commands, AT+GVER=1 answered with "AT+GVER=V1.03". Powered from P14 (shared with the modem and
 * the amplifier): deaf for 700 ms after power-up (upstream kBootSettleUs), and back to no mode after
 * a power cut, so whoever powers it up sets the mode again (upstream applies mode 1 at boot and
 * after P14 comes back). Bytes take 10 bit times at 115200 baud.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bh1098g/bh1098g.h"
#include "ink/hal_uart.h"
#include "ink/osal.h"
#include "sim.h"

struct hal_uart {
    int port;
};

static struct {
    bool powered;
    uint64_t ready_at;
    int mode;           /* 0: not set since power-up */
    bool sco;           /* headset call audio */
    char line[64];
    int n;
    char out[8][64];
    int out_h, out_n;
    vtk_waitq_t wq;
    unsigned cmds, lost;
} B;

static uint64_t now_ns(void) { return vtk_in_isr() ? vtk_isr_now() : vtk_now(); }

void sim_bt_audio_power(bool on) { /* board P14; any context */
    if (on && !B.powered) B.ready_at = now_ns() + SIM_US(BH1098G_BOOT_US);
    if (!on) {
        B.mode = 0;
        B.sco = false;
    }
    B.powered = on;
}

static void reply(const char *text) { /* task context */
    vtk_lock();
    if (B.out_n < 8) {
        snprintf(B.out[(B.out_h + B.out_n) % 8], sizeof B.out[0], "%s", text);
        B.out_n++;
    }
    vtk_wake_one_locked(&B.wq);
    vtk_unlock();
}

static void command(const char *l) {
    if (!B.powered || now_ns() < B.ready_at) {
        B.lost++;
        return;
    }
    B.cmds++;
    if (!strcmp(l, "AT+GVER=1")) reply("AT+GVER=V1.03");
    else if (!strncmp(l, "AT+MODE=", 8) && l[8] >= '1' && l[8] <= '3' && !l[9]) B.mode = l[8] - '0';
    else if (!strcmp(l, "AT+BTSCO=1")) B.sco = true;
    /* AT+RX=n, AT+TX=1, AT+PP=1: accepted without an answer */
}

hal_uart_t *hal_uart_open(int port, int baud) {
    static struct hal_uart u1 = {BH1098G_UART_PORT};
    (void)baud;
    return port == BH1098G_UART_PORT && sim_board_present() ? &u1 : NULL;
}

int hal_uart_write(hal_uart_t *u, const void *data, size_t len) {
    if (!u) return -1;
    vtk_sleep_ns((uint64_t)len * 10u * 1000000000ull / BH1098G_BAUD);
    const char *p = data;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\n') {
            B.line[B.n] = 0;
            if (B.n && B.line[B.n - 1] == '\r') B.line[B.n - 1] = 0;
            command(B.line);
            B.n = 0;
        } else if (B.n < (int)sizeof B.line - 1) {
            B.line[B.n++] = p[i];
        }
    }
    return (int)len;
}

int hal_uart_read_line(hal_uart_t *u, char *buf, size_t cap, uint32_t timeout_ms) {
    if (!u) return -1;
    uint64_t deadline = vtk_now() + SIM_MS(timeout_ms);
    vtk_lock();
    while (B.out_n == 0) {
        uint64_t now = vtk_now();
        if (now >= deadline || !vtk_block_locked(&B.wq, deadline - now)) {
            if (B.out_n) break;
            vtk_unlock();
            return -1;
        }
    }
    snprintf(buf, cap, "%s", B.out[B.out_h]);
    B.out_h = (B.out_h + 1) % 8;
    B.out_n--;
    vtk_unlock();
    return (int)strlen(buf);
}

/* At boot, as upstream's MetalioAudio_Init: once the chip is up, conversation mode (mode 1). */
static void bt_boot_task(void *arg) {
    (void)arg;
    uint64_t now = vtk_now();
    if (B.ready_at > now) osal_task_delay_us((B.ready_at - now) / 1000);
    hal_uart_t *u = hal_uart_open(BH1098G_UART_PORT, BH1098G_BAUD);
    if (u && bh1098g_set_mode(u, 1)) sim_warn("board: BH1098G mode 1 not set");
}

void sim_bt_audio_init(void) { /* task context (boot); P14 is high from the board's power-on levels */
    sim_bt_audio_power(true);
    osal_task_cfg_t c = {.name = "bt_audio", .fn = bt_boot_task, .stack_bytes = 3072, .priority = 4, .core = -1,
                         .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&c, NULL) != OSAL_OK) sim_warn("board: cannot start the BH1098G setup");
}

int sim_bt_audio_mode(void) { return B.mode; }
bool sim_bt_audio_powered(void) { return B.powered; }

void sim_bt_audio_report(void) {
    sim_metric_set("bt.commands", B.cmds);
    sim_metric_set("bt.lost_commands", B.lost);
    sim_metric_set("bt.mode", B.mode);
}
