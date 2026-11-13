/*
 * System state (architecture v1.0 §4.4): one table of named, typed items
 * that say how things are now (battery.level, net.online, power.state ...).
 * Apps and the AI read and watch it; they never write it. Each item has one
 * source: only the service that defined it holds its handle and updates it.
 * Internal mechanisms (power rails, AT commands, refresh scheduling) are not
 * in this table.
 */
#ifndef INK_SYS_STATE_H
#define INK_SYS_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/exec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_STATE_MAX 64
#define INK_STATE_STR 32 /* longest string value, NUL included */

typedef enum { INK_ST_BOOL = 1, INK_ST_INT, INK_ST_STR } ink_st_type_t;

typedef struct {
    const char *id;           /* "battery.level" */
    ink_st_type_t type;
    const char *title;        /* for people and the model: "电量" */
    uint8_t expose;           /* INK_EXPOSE_* (ink/sys/cap.h); 0 = everyone */
    uint16_t min_interval_ms; /* watchers hear of it at most this often (the latest value); 0 = every change */
} ink_state_def_t;

int ink_state_init(void);
/** Define an item (the definition must outlive the program): its handle (>= 0), or -1. */
int ink_state_define(const ink_state_def_t *def);
/* The defining service updates its item (any task). An unchanged value notifies nobody. */
void ink_state_set_int(int h, int64_t v);
void ink_state_set_bool(int h, bool v);
void ink_state_set_str(int h, const char *s);

int ink_state_find(const char *id);
/** false until the item has a value (or for a type mismatch). Booleans read as 0 / 1. */
bool ink_state_get_int(int h, int64_t *out);
/** String value: its length (copied, truncated to cap), -1 without a value. */
int ink_state_get_str(int h, char *out, size_t cap);

/** fn(ctx, h, id) on executor `exec` when an item whose id starts with `prefix` changes. Up to 96 (WATCH_MAX). */
typedef void (*ink_state_fn)(void *ctx, int h, const char *id);
int ink_state_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec);
/** Removable watch: positive handle, -1 on failure. UI/MEDIA/BG only; prefix lives until unwatch.
 * Unwatch on that same executor: queued callbacks are discarded, active callbacks finish there.
 * From inside a callback, only that invocation continues. Old watch() keeps returning 0/-1. */
int ink_state_watch_handle(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec);
int ink_state_unwatch(int handle); /* 0 removed, -1 stale handle or wrong executor */
/** {"id": value, ...} of the items with a value that `expose` (INK_EXPOSE_* bit, 0 = all) may see. */
int ink_state_json(char *out, size_t cap, uint8_t expose);

#ifdef __cplusplus
}
#endif
#endif
