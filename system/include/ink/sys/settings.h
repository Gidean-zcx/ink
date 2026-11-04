/*
 * Settings (architecture v1.0 §4.5): one table of what people adjust
 * (volume, light, auto-lock, which SIM card, when the device listens ...).
 * Each item says its type and range, who may change it and which service
 * applies it. The settings app draws its screen from this table; the AI
 * changes settings only through the "settings.set" capability, past the same
 * policy gate; apps only read.
 *
 * Values are kept in one small file written through the storage service
 * (D-57), one second after the last change, and read back at start-up.
 */
#ifndef INK_SYS_SETTINGS_H
#define INK_SYS_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_SETTINGS_MAX 48
#define INK_SETTINGS_PATH "/user/sys/settings.json"

typedef enum { INK_SET_BOOL = 1, INK_SET_INT, INK_SET_ENUM } ink_set_type_t;

typedef struct {
    const char *id;                /* "audio.volume" */
    const char *title;             /* "音量" */
    ink_set_type_t type;
    int64_t min, max, def;         /* ENUM: the value is an index into choices */
    const char *const *choices;    /* ENUM: value names, NULL-terminated */
    bool child_may_change;
    bool system;                   /* system-level: adults only; the AI's change waits for confirmation */
    void (*apply)(void *ctx, int64_t v); /* the owning service puts the value into effect: on every set,
                                          * also of the same value, so it must not harm when nothing changed */
    void *ctx;
    ink_exec_t apply_exec;
} ink_setting_def_t;

/** Read the saved values (after the storage service started) and register settings.get/set. */
int ink_settings_init(void);
/** Define settings (the table must outlive the program). Each takes its saved value, or the
 *  default when there is none or it is out of range; `apply` is not called for it: the
 *  service reads ink_setting_get() after defining. 0, or -1 when the table is full. */
int ink_settings_define(const ink_setting_def_t *defs, int n);
/** Current value (any task); 0 for an unknown id. */
int64_t ink_setting_get(const char *id);
/** Change a setting past the policy gate; applies it, tells watchers, saves it. rc->status: the
 *  value is stored and on its way to `apply`; what the hardware does is its owner's to report (state). */
ink_rc_t ink_setting_set(const char *id, int64_t v, ink_source_t src, ink_receipt_t *rc);
/** fn(ctx, index, id) on `exec` when a setting whose id starts with `prefix` changes. Up to 16. */
int ink_settings_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec);
/** The table for the settings app and the AI: [{id,title,type,value,...}]. Bytes or -1. */
int ink_settings_json(char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
