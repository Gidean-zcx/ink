/*
 * The app directory and lifecycle (system services design §8). An app is declared once: who it is, its entry page, what
 * it needs and may use. The system decides when it is in the foreground, in the background, frozen or stopped, and tells
 * it, always in the UI task; an app has no threads, so "frozen" means it has promised to do nothing (timers stopped,
 * caches let go) until it is resumed. What it saves (a kilobyte) comes back when it is opened again after being stopped.
 *
 * Home and the AI read the same directory and the same service cards.
 */
#ifndef INK_SYS_APPREG_H
#define INK_SYS_APPREG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_APP_STOPPED = 0, INK_APP_FOREGROUND, INK_APP_BACKGROUND, INK_APP_FROZEN } ink_app_state_t;
enum { INK_BG_PLAYBACK = 1, INK_BG_DOWNLOAD = 2, INK_BG_SYNC = 4 }; /* what an app may keep doing in the background */

#define INK_APP_STATE_MAX 1024

/* All optional, all in the UI task. */
typedef struct {
    void (*on_start)(void);
    void (*on_foreground)(void);
    void (*on_background)(void);
    void (*on_freeze)(void);
    void (*on_resume)(void);
    void (*on_stop)(void);
    int (*save_state)(void *buf, size_t cap);           /* bytes written; 0 = nothing to keep */
    void (*restore_state)(const void *buf, size_t len); /* after on_start, if something was kept */
} ink_app_ops_t;

/* A service card: what an app shows outside itself (home, the AI). */
typedef struct {
    char id[16], title[32], text[96], action[40]; /* action: the capability behind it */
    int64_t value;
    bool has_value;
    bool bookmark; /* where someone was: worth leaving on the panel when the device goes off */
    uint32_t rev;
    char member[16]; /* owner; empty means the provider already scoped it to the current member */
} ink_card_t;

struct ink_activity;

typedef struct {
    const char *id, *title, *icon;
    const struct ink_activity *entry;
    bool system_app;   /* comes with the system, cannot be removed */
    bool child_ok;
    bool keep;         /* never stopped for memory (home) */
    uint32_t background;              /* what it may keep doing: INK_BG_* */
    const char *const *needs;         /* capabilities it cannot work without, NULL-terminated: not shown without them */
    const char *const *permissions;   /* what it may use, NULL-terminated */
    const ink_app_ops_t *ops;
    int (*card)(int index, ink_card_t *out); /* cheap, whether the app is open or not: 1, 0 past the last */
} ink_app_def_t;

/** At boot, before the UI starts; static definitions. Up to 16. 0, or -1 (full, or the id is taken or not usable as a file name). */
int ink_app_register(const ink_app_def_t *def);
int ink_apps_init(void);
int ink_app_count(void);
const ink_app_def_t *ink_app_at(int i);
/** May the current member see it, does this device have what it needs? */
bool ink_app_visible(const ink_app_def_t *def);
ink_app_state_t ink_app_state_of(const char *id);
const char *ink_app_state_name(ink_app_state_t s);
/** UI task. Its page comes to the top, opened with `arg` if it was not open. `from` (an lv_obj_t, may be NULL): the control
 *  tapped, which shows it at once. 0, -1 no such app, -2 not for this member or device. */
int ink_app_open(const char *id, void *from, void *arg);
/** UI task. Save, stop, off the screen. 0, or -1 (no such app, or one that is kept). */
int ink_app_close(const char *id);
/** UI task: persist the existing save_state payload without closing the app. */
int ink_app_save_state(const char *id);
/** Any task: it starts or finishes background work of a kind it declared. 0, or -1 (not declared). */
int ink_app_background(const char *id, uint32_t kind, bool on);
/** One of its own collections: `def->name` must start with "<app>.". */
ink_db_t *ink_app_db_open(const char *app, const ink_db_def_t *def);
typedef void (*ink_app_db_done_fn)(void *ctx, ink_db_t *db);
/** UI task: open on BG as the captured member, then done on UI (NULL db: open failed).
 * page is the caller's LVGL root; deleting it or changing member/auth drops done and closes db.
 * 0 accepted, -1 rejected without callback. def outlives the request; done owns the db reference. */
int ink_app_db_open_async(const char *app, const ink_db_def_t *def, void *page, ink_app_db_done_fn done, void *ctx);
/** Its cards changed (any task): home and the AI read them again (state app.cards_rev). */
void ink_app_card_changed(const char *id);
/** Shared member filter for service cards and retained bookmarks. */
bool ink_app_card_visible(const ink_card_t *card);
/** Does the app in the foreground declare this permission? Asked by the policy gate for what apps call. */
bool ink_app_foreground_may(const char *permission);

#ifdef __cplusplus
}
#endif
#endif
