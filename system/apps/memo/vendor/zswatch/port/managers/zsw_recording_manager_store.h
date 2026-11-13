/* Shim for the copied ZSWatch voice memo UI only (see ../../SOURCE.md): the list entry type. Recordings are written by
 * our media service, not this store. One entry: the list screen is off, so the UI keeps 32 bytes of names, not 1.6 KB. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOICE_MEMO_MAX_FILENAME 32
#define ZSW_RECORDING_MAX_FILES 1

typedef struct {
    char     filename[VOICE_MEMO_MAX_FILENAME];
    uint32_t timestamp;
    uint32_t duration_ms;
    uint32_t size_bytes;
} zsw_recording_entry_t;
