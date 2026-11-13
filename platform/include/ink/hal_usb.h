/*
 * ink HAL — USB to a computer: is one there, and lending it the card as a USB
 * disk (mass storage). While lent, /content and /cache are unmounted: every
 * hal_fs_* call on them fails; the computer works on the card directly.
 *   device     TinyUSB MSC (upstream enables it; not measured by this project; the
 *              USB port normally is the serial/JTAG console, what switching does to
 *              it is unknown, see docs/system/system-services-design.md §14)
 *   simulator  a model: the script says when a computer is there, ejects the disk
 *              or copies a file onto it
 */
#ifndef INK_HAL_USB_H
#define INK_HAL_USB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A computer is at the other end of the cable (not a charger). */
bool hal_usb_host_connected(void);
/** The computer ejected the disk, or the cable went: time to stop lending it. */
bool hal_usb_host_done(void);
/** Told when either of the above changes. In the platform's event task: keep it short. One listener. */
void hal_usb_set_callback(void (*cb)(void *ctx), void *ctx);
/** Unmount the card and lend it to the computer. 0, or -1 (no card, no computer). Task context. */
int hal_usb_disk_start(void);
/** Take it back and mount it again. 0, or -1: it does not mount (e.g. pulled without ejecting, the FAT
 *  needs repair on a computer); /content and /cache then fail as if there were no card. Task context. */
int hal_usb_disk_stop(void);

/** The USB serial console as a line link to an install tool (card files, D-138). 0, or -1 (no computer, USB disk mode, no memory). Task context, one user. */
int hal_usb_console_open(void);
/** Bytes that arrived, waiting up to timeout_ms: n > 0, 0 when none, -1 when closed or the cable went. */
int hal_usb_console_read(void *buf, size_t cap, uint32_t timeout_ms);
/** One whole line, the caller ends it with '\n'. Works without open. Never split by log output. */
int hal_usb_console_write(const char *line, size_t n);
void hal_usb_console_close(void);

#ifdef __cplusplus
}
#endif
#endif
