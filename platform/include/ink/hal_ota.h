/*
 * ink HAL — the system's two application slots (system services design §12.5). On the device these are esp_ota_*: the
 * bootloader goes back to the old slot when a new one has not said it is good. The bootloader that came with the Plus has
 * rollback off (foundation §10.1): a first install of ours over USB, with the user's consent, comes before this holds there.
 * All of it blocks; writing goes through the storage task, a block at a time, under the flash guard.
 */
#ifndef INK_HAL_OTA_H
#define INK_HAL_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t slot_size;     /* how much a slot takes */
    int running;          /* 0 or 1 */
    bool pending_verify;  /* this is the first boot of what was just installed */
    bool rolled_back;     /* the last boot of a new version did not hold: this is the old one again */
    char version[24];     /* of what runs now */
    uint32_t version_code;
} hal_ota_info_t;

void hal_ota_info(hal_ota_info_t *out);
/** Start writing the other slot. 0 or -1. */
int hal_ota_begin(size_t size);
int hal_ota_write(const void *data, size_t n);
/** All of it there and well formed. */
int hal_ota_end(void);
/** The next boot is from the slot just written. */
int hal_ota_set_boot(void);
/** The version running is good: no going back. Until this is said, a restart goes back to the old slot. */
int hal_ota_mark_valid(void);
/** What is not in a slot: fonts, resources. kind: 1 font, 2 resources. Written after the new system has proved itself. */
int hal_ota_write_resource(int kind, size_t offset, const void *data, size_t n);

/** The upgrade package's signature over its hash. On the device ECDSA P-256 against a key in the firmware; in the simulator a
 *  stand-in with a shared key, enough to check the system's logic and no more. */
bool hal_crypto_verify_fw(const uint8_t hash[32], const uint8_t *sig, size_t sig_len, uint32_t key_id);

#ifdef __cplusplus
}
#endif
#endif
