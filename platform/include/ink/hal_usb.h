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

#ifdef __cplusplus
}
#endif
#endif
