/*
 * Actions: every user-visible operation is registered once and invoked the
 * same way from a button, a key, a script or the AI (foundation §6.1), so
 * "doing it by hand" and "asking the AI" give the same result by
 * construction. Each action declares its arguments (JSON schema), its side
 * effect class and whether children may use it; the policy gate here — not a
 * prompt — decides confirmation and refusal (§6.2, §8). Every invocation
 * returns a receipt with one status from a single enumeration (§6.4).
 *
 * Actions run in the UI task (they may touch LVGL). Other tasks (the hub link
 * for AI tool calls) post to the UI task and get the receipt back.
 */
#ifndef INK_SYS_ACTIONS_H
#define INK_SYS_ACTIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_FX_READ = 0,          /* read the current page, list the shelf */
    INK_FX_NAVIGATE,          /* open a book, go to a page (can go back) */
    INK_FX_LOCAL_WRITE,       /* add a word, note something (undo window in the receipt) */
    INK_FX_LOCAL_DESTRUCTIVE, /* delete, bulk replace: confirm on the device */
    INK_FX_EXTERNAL,          /* message, call, share: confirm on the device by the current member */
    INK_FX_SYSTEM,            /* reset, upgrade, system settings: confirm (PIN when needed), never children */
} ink_effect_t;

typedef enum { INK_CONFIRM_NONE = 0, INK_CONFIRM_DEVICE, INK_CONFIRM_PARENT } ink_confirm_t;

/* The one receipt status enumeration (shared with the hub and the docs). */
typedef enum {
    INK_RC_DONE = 0,
    INK_RC_PARTIAL,
    INK_RC_QUEUED,
    INK_RC_PENDING_CONFIRMATION,
    INK_RC_REJECTED, /* refused by the policy gate */
    INK_RC_STALE,    /* aimed at a page that is no longer on screen */
    INK_RC_INVALID,  /* bad arguments, unknown action, not available here */
    INK_RC_FAILED,
    INK_RC_CANCELLED,
    INK_RC_UNDONE,
} ink_rc_t;

typedef enum { INK_SRC_TOUCH = 0, INK_SRC_KEY, INK_SRC_AI, INK_SRC_SYSTEM } ink_source_t;

typedef struct {
    ink_rc_t status;
    char object[48];   /* affected object, e.g. "book:星星邮差#page=12" */
    uint32_t revision; /* new state version of that object */
    uint32_t undo_ms;  /* how long it can be undone (0 = not undoable) */
    char say[160];     /* one sentence the AI may speak without another model call */
    char detail[96];   /* why it was rejected / invalid / failed */
} ink_receipt_t;

typedef struct {
    const char *id;       /* "reader.next_page" */
    const char *title;    /* shown to people and to the model: "下一页" */
    const char *args;     /* JSON schema of the arguments; NULL = none */
    ink_effect_t effect;
    bool child_ok;        /* children may use it (their whitelist can still narrow this) */
    const char *activity; /* only meaningful while this activity is on top; NULL = anywhere */
    void (*run)(const ink_json_t *args, int atok, ink_receipt_t *rc); /* UI task; atok < 0 = no arguments */
} ink_action_t;

/** Register a static table of actions (the table must outlive the program). */
int ink_actions_register(const ink_action_t *table, int n);
/**
 * Invoke an action (UI task). `args_json` may be NULL. `page_gen`, if not 0, is the page
 * generation the caller saw (AI tool calls): page-bound actions on a newer page are refused as
 * stale. Returns rc->status.
 */
ink_rc_t ink_action_invoke(const char *id, const char *args_json, ink_source_t src, uint32_t page_gen,
                           ink_receipt_t *rc);
/** JSON array of the actions available now ({id,title,effect,confirm,args}); bytes written or -1. */
int ink_actions_json(char *out, size_t cap);
/** Compact JSON array of available action ids (for page snapshots). */
int ink_actions_ids_json(char *out, size_t cap);
const char *ink_rc_name(ink_rc_t rc);
const char *ink_effect_name(ink_effect_t fx);
/** Receipt as JSON (for the hub). */
int ink_receipt_json(const ink_receipt_t *rc, char *out, size_t cap);

/** Hub glue: answer the hub's "tools/list" and "tools/call" requests through the registry. */
int ink_tools_init(void);

#ifdef __cplusplus
}
#endif
#endif
