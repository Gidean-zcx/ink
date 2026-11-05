/*
 * Capabilities (architecture v1.0 §4.3, detailed design §2): what a person or
 * the AI asks for to get something done. Every one is registered once with a
 * description (arguments as JSON Schema, side-effect class, who may use it,
 * service class, where its handler runs) and invoked the same way from a
 * button, a key, another app, the AI or the hub, so "doing it by hand" and
 * "asking the AI" give the same result by construction (foundation §6.1).
 * The policy gate here, not a prompt, decides confirmation and refusal
 * (§6.2, §8). Every invocation returns a receipt with one status from a
 * single enumeration (§6.4).
 *
 * The description table is the only source: the tool list the hub and the
 * model see is generated from it.
 */
#ifndef INK_SYS_CAP_H
#define INK_SYS_CAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/args.h"
#include "ink/sys/exec.h"

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
    INK_RC_INVALID,  /* bad arguments, unknown capability, not available here */
    INK_RC_FAILED,
    INK_RC_CANCELLED,
    INK_RC_UNDONE,
} ink_rc_t;

/* Who asks. Touch, keys and the system are the member's own direct manipulation. CONFIRMED: someone else
 * asked and the member accepted the confirmation card. */
typedef enum { INK_SRC_TOUCH = 0, INK_SRC_KEY, INK_SRC_AI, INK_SRC_SYSTEM, INK_SRC_APP, INK_SRC_REMOTE, INK_SRC_CONFIRMED } ink_source_t;

typedef enum { INK_FORM_CALL = 0, INK_FORM_STREAM, INK_FORM_EVENT, INK_FORM_SESSION } ink_form_t;
typedef enum { INK_QOS_INTERACTIVE = 0, INK_QOS_REALTIME, INK_QOS_MEDIA, INK_QOS_NETWORK, INK_QOS_BACKGROUND } ink_qos_t;
typedef enum { INK_OFFLINE_FAIL = 0, INK_OFFLINE_QUEUE, INK_OFFLINE_LOCAL } ink_offline_t;
enum { INK_EXPOSE_APP = 1, INK_EXPOSE_AI = 2, INK_EXPOSE_REMOTE = 4, INK_EXPOSE_OWNER = 8 }; /* 0 = everyone; OWNER: only the member's own touch and keys */

typedef struct {
    ink_rc_t status;
    char object[48];   /* affected object, e.g. "book:星星邮差#page=12" */
    uint32_t revision; /* new state version of that object */
    uint32_t undo_ms;  /* how long it can be undone (0 = not undoable) */
    char say[160];     /* one sentence the AI may speak without another model call */
    char detail[96];   /* why it was rejected / invalid / failed */
    uint32_t page_gen; /* the content version when it finished (ink_ui_content_version): what the next
                        * tool call of the same turn is prepared against */
    const char *result; /* what was found, as JSON (ink_cap_result), for the done callback only: it does not
                         * outlive it. NULL = only the fields above. Not kept for repeats of a request id. */
} ink_receipt_t;

typedef struct ink_cap ink_cap_t;

/* One invocation, as the handler sees it. The handler fills `rc` (status is DONE on entry). */
typedef struct {
    const ink_cap_t *cap;
    ink_args_t args;
    ink_source_t src;
    uint32_t page_gen; /* the content version the caller saw (AI, hub), 0 = not page-bound */
    ink_receipt_t rc;
} ink_call_t;

struct ink_cap {
    const char *id;         /* "reader.next_page": named after what it does, never after a chip */
    uint16_t version;       /* 0 means 1 */
    ink_form_t form;
    const char *title;      /* shown to people and to the model: "下一页" */
    const char *input;      /* JSON Schema (object) of the arguments; NULL = none */
    const char *output;     /* JSON Schema of the result; NULL = the receipt only */
    ink_effect_t effect;
    bool child_ok;          /* children may use it (their whitelist can still narrow this) */
    const char *permission; /* what the calling app needs (app manifests, step 9); NULL = nothing */
    ink_qos_t qos;
    uint32_t timeout_ms;    /* 0 = the service class default */
    bool retry_ok;
    ink_offline_t offline;
    uint8_t expose;         /* INK_EXPOSE_* bits; 0 = everyone */
    const char *activity;   /* only meaningful while this activity is on top; NULL = anywhere */
    ink_exec_t exec;        /* where the handler runs (UI task by default) */
    bool while_locked;      /* can be used while an adult's space is locked: stopping an alarm, answering a call */
    bool parent_pin;        /* whoever asks, an adult's PIN (if one is set) goes with the yes: removing a member, factory reset */
    bool ask_by_hand;       /* a person is asked too: a tap on a notification should not take the device away for minutes */
    const char *ask;        /* what the confirmation card says, with {argument}s: "发短信给 {to}：{text}"; NULL = title and arguments */
    void (*run)(ink_call_t *call);
};

/** Allocate the call pool (PSRAM). Before the first call. */
int ink_cap_init(void);
/** Register a static table (it must outlive the program). 0, or -1 when the registry is full. */
int ink_cap_register(const ink_cap_t *table, int n);
const ink_cap_t *ink_cap_find(const char *id);
/** All of them, in the order registered: NULL past the last. */
const ink_cap_t *ink_cap_at(int i);
/** Registered and usable now (a page-bound capability only while its page is on top). */
bool ink_cap_available(const char *id);

/**
 * Invoke and wait (UI task). `args` may be NULL. `page_gen`, if not 0, is the content version the
 * caller saw (ink_ui_content_version): page-bound capabilities on newer content are refused as
 * stale. Returns rc->status.
 * A capability that runs elsewhere or completes later answers INK_RC_QUEUED here; use
 * ink_cap_call to hear its receipt.
 */
ink_rc_t ink_cap_invoke(const char *id, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc);
/** The same with JSON arguments (the AI's, the hub's): converted and checked against the schema. */
ink_rc_t ink_cap_invoke_json(const char *id, const char *args_json, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc);

/*
 * The soft bus (architecture §5.1, detailed design §3): one way to call a capability wherever it
 * is. An address without a node ("reader.next_page") is this device; "hub/<id>" is a capability
 * of the family hub, reached through the remote proxy (JSON-RPC "tools/call"). The handler runs
 * on the capability's executor; the receipt comes back on the caller's.
 */
typedef void (*ink_cap_done_fn)(void *ctx, const ink_receipt_t *rc);
typedef struct {
    ink_source_t src;
    uint32_t page_gen;      /* the content version the caller saw (AI, hub); 0 = not page-bound */
    const char *request_id; /* a repeated id does not run twice: the first receipt again, or "queued" while it runs; NULL = none.
                             * A read that gave a result runs again: it has no effect to repeat, and the result is not kept. */
    const char *args_json;  /* JSON arguments instead of a table (the AI's, the hub's): checked after the gate */
    const char *member;     /* only for this member: another using the device by the time it starts, and it does not run; NULL = whoever */
    uint32_t member_generation; /* optional incarnation binding for delayed callers */
    ink_exec_t reply_exec;  /* where `done` runs */
    ink_cap_done_fn done;   /* may be NULL */
    void *ctx;
} ink_call_opts_t;

/**
 * Call a capability (any task). 0: accepted, `done` will run exactly once on reply_exec (at once,
 * if the handler may run here and finishes). -1: not accepted (the call pool is full, an executor
 * it needs cannot run, a request id longer than 39 bytes): `done` is not called.
 * The deadline counts from here. A call that is still waiting to start when it passes is answered
 * "failed" and never runs; one already running is answered "failed", "it may still finish".
 * A request id remembers the last 32 final receipts.
 */
int ink_cap_call(const char *addr, const ink_args_t *args, const ink_call_opts_t *o);
/** In a handler: the receipt comes later, from ink_cap_done (any task, once; it may come before the
 *  handler returns). The call object stays valid until then. A call not done by its deadline is
 *  answered "failed: timed out"; its late ink_cap_done is then ignored. */
void ink_cap_defer(ink_call_t *call);
void ink_cap_done(ink_call_t *call);
/** In a handler that blocks: how long until its deadline (0: past it). What it may still wait for. */
uint32_t ink_cap_time_left_ms(const ink_call_t *call);
/** Immutable owner of an accepted call; valid while its provider holds the call. */
const char *ink_cap_call_member(const ink_call_t *call, uint32_t *generation);
/** In a handler: what it found, as JSON text (copied), for the AI, the hub and other apps: rc.result. */
void ink_cap_result(ink_call_t *call, const char *json);
/** In a handler: this change can be undone for `ms` by running capability `cap` with `args`: the receipt's
 *  object names it, undo_ms is set, and system.undo with that object does it (same member, in time). */
void ink_cap_undoable(ink_call_t *call, const char *cap, const ink_args_t *args, uint32_t ms);

/*
 * Events (form INK_FORM_EVENT, e.g. "message.received"): registered like any capability so the
 * hub and the model see them, but subscribed to rather than called. The payload is a copy.
 */
typedef void (*ink_event_fn)(void *ctx, const char *id, const ink_args_t *payload);
/** fn on `exec` for every event whose id starts with `prefix`. Up to 16. */
int ink_cap_subscribe(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec);
/** The providing service emits an event (any task). */
void ink_cap_emit(const char *id, const ink_args_t *payload);
/** Delayed producers keep the original member incarnation, including across handoff. */
void ink_cap_emit_for(const char *id, const ink_args_t *payload, const char *member, uint32_t generation);

/** The tool list for the hub and the model: JSON array of what the current member may use now. */
int ink_cap_manifest_json(char *out, size_t cap);
/** A page of it, from registry position `start`, only ids starting with `prefix` (may be NULL), as much as
 *  fits: bytes or -1. *next: where the next page starts, -1 after the last. */
int ink_cap_manifest_page(int start, const char *prefix, char *out, size_t cap, int *next);
/** Compact JSON array of the ids available now (for page snapshots). */
int ink_cap_ids_json(char *out, size_t cap);
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
