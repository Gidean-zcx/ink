/*
 * USB to a computer on the simulated device. A behaviour model, not a USB stack: the script says when
 * a computer is at the other end of the cable, when it ejects the disk and what it copies. While the
 * card is lent every hal_fs_* call on it fails, so the system cannot keep using it unnoticed; the
 * cable going without an eject leaves the FAT dirty and the next mount fails.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ink/hal_storage.h"
#include "ink/hal_usb.h"
#include "sim.h"

static struct {
    bool connected, ejected, lent;
    void (*cb)(void *ctx);
    void *ctx;
    unsigned lends, copies;
    bool console;            /* the console is open as a line link (card files) */
    char rx[8192];           /* what the install tool answered, not read yet */
    size_t rx_n;
    vtk_waitq_t rx_wq;
} U;

static void tell(void *arg) { /* the platform's event task */
    (void)arg;
    if (U.cb) U.cb(U.ctx);
}

void sim_usb_plug(bool on) { /* ISR (script) */
    U.connected = on;
    U.ejected = false;
    sim_power_set_cable(on);
    sim_sys_evt(tell, NULL);
    vtk_isr_wake_one(&U.rx_wq); /* a console reader hears that the cable went */
}

void sim_usb_eject(void) { /* ISR (script) */
    if (!U.lent) return;
    U.ejected = true;
    sim_sys_evt(tell, NULL);
}

static void make_dirs(char *path) { /* the parent directories of a host path */
    for (char *p = path + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(path, 0755);
            *p = '/';
        }
}

void sim_usb_copy_in(const char *host_path, const char *card_path) { /* ISR (script): host files only */
    if (!U.lent) {
        sim_warn("usb copy-in: the disk is not lent to the computer");
        return;
    }
    char dst[1024];
    snprintf(dst, sizeof dst, "%s/%s", sim_sd_root(), card_path);
    make_dirs(dst);
    FILE *in = fopen(host_path, "rb"), *out = fopen(dst, "wb");
    if (in && out) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
        U.copies++;
    } else sim_warn("usb copy-in: cannot copy %s", host_path);
    if (in) fclose(in);
    if (out) fclose(out);
}

bool hal_usb_host_connected(void) { return U.connected; }
bool hal_usb_host_done(void) { return U.lent && (U.ejected || !U.connected); }

void hal_usb_set_callback(void (*cb)(void *ctx), void *ctx) {
    U.cb = cb;
    U.ctx = ctx;
}

int hal_usb_disk_start(void) {
    if (!U.connected || !hal_fs_card_present()) return -1;
    osal_task_delay_us(50000); /* unmount, then the MSC class comes up (estimate) */
    vtk_lock();
    U.lent = true;
    U.ejected = false;
    sim_sd_lend(true, false);
    U.lends++;
    vtk_unlock();
    return 0;
}

int hal_usb_disk_stop(void) {
    if (!U.lent) return 0;
    vtk_lock();
    bool dirty = !U.connected && !U.ejected; /* the cable went without an eject; taken back while connected, the system let go first */
    U.lent = false;
    sim_sd_lend(false, dirty);
    vtk_unlock();
    osal_task_delay_us(100000); /* mount (estimate) */
    return hal_fs_card_present() ? 0 : -1;
}

void sim_usb_report(void) {
    if (U.lends) sim_metric_set("usb.disk_lends", U.lends);
    if (U.copies) sim_metric_set("usb.copies_from_host", U.copies);
}

void sim_usb_console_rx(const char *data, size_t n) { /* ISR: the install tool's answer arrives */
    if (!U.console || n > sizeof U.rx - U.rx_n) return;
    memcpy(U.rx + U.rx_n, data, n);
    U.rx_n += n;
    vtk_isr_wake_one(&U.rx_wq);
}

int hal_usb_console_open(void) {
    if (!U.connected || U.lent) return -1;
    vtk_lock();
    U.console = true;
    U.rx_n = 0; /* nothing from before counts */
    vtk_unlock();
    return 0;
}

int hal_usb_console_read(void *buf, size_t cap, uint32_t timeout_ms) {
    vtk_lock();
    uint64_t end = vtk_now() + SIM_MS(timeout_ms);
    while (U.console && U.connected && !U.rx_n && vtk_now() < end) vtk_block_locked(&U.rx_wq, end - vtk_now());
    int n = !U.console || !U.connected ? -1 : (int)(U.rx_n < cap ? U.rx_n : cap);
    if (n > 0) {
        memcpy(buf, U.rx, (size_t)n);
        U.rx_n -= (size_t)n;
        memmove(U.rx, U.rx + n, U.rx_n);
    }
    vtk_unlock();
    return n;
}

int hal_usb_console_write(const char *line, size_t n) {
    if (U.connected && !U.lent) sim_installer_line(line, n); /* only a computer on the cable hears it */
    return 0;
}

void hal_usb_console_close(void) {
    vtk_lock();
    U.console = false;
    U.rx_n = 0;
    vtk_unlock();
}
