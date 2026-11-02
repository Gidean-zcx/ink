/*
 * Action registry, policy gate and receipts (see ink/sys/actions.h), plus the
 * hub's "tools/list" / "tools/call" (the AI's way in, D-12: MCP tool and
 * result structures inside the device's own JSON-RPC envelope).
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/actions.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

#define MAX_ACTIONS 64

static const ink_action_t *REG[MAX_ACTIONS];
static int nreg;

static const char *RC_NAME[] = {"done",     "partial", "queued", "pending_confirmation", "rejected",
                                "stale",    "invalid", "failed", "cancelled",            "undone"};
static const char *FX_NAME[] = {"read", "navigate", "local_write", "local_destructive", "external", "system"};
static const char *CONFIRM_NAME[] = {"none", "device", "parent"};

const char *ink_rc_name(ink_rc_t rc) { return (unsigned)rc < sizeof RC_NAME / sizeof RC_NAME[0] ? RC_NAME[rc] : "?"; }
const char *ink_effect_name(ink_effect_t fx) { return (unsigned)fx < sizeof FX_NAME / sizeof FX_NAME[0] ? FX_NAME[fx] : "?"; }

int ink_actions_register(const ink_action_t *table, int n) {
    for (int i = 0; i < n; i++) {
        if (nreg >= MAX_ACTIONS) return -1;
        REG[nreg++] = &table[i];
    }
    return 0;
}

static const ink_action_t *find(const char *id) {
    for (int i = 0; i < nreg; i++)
        if (!strcmp(REG[i]->id, id)) return REG[i];
    return NULL;
}

static bool available(const ink_action_t *a) { return !a->activity || !strcmp(ink_ui_top_name(), a->activity); }

/* Default confirmation by side effect (foundation §6.2). Direct manipulation (touch, keys) is the
 * member's own confirmation except where a PIN is required. */
static ink_confirm_t confirm_for(const ink_action_t *a, ink_source_t src) {
    ink_confirm_t c = INK_CONFIRM_NONE;
    switch (a->effect) {
    case INK_FX_LOCAL_DESTRUCTIVE:
    case INK_FX_EXTERNAL:
    case INK_FX_SYSTEM: c = INK_CONFIRM_DEVICE; break;
    default: break;
    }
    if (src == INK_SRC_TOUCH || src == INK_SRC_KEY || src == INK_SRC_SYSTEM) return INK_CONFIRM_NONE;
    return c;
}

static void reject(ink_receipt_t *rc, ink_rc_t st, const char *why) {
    rc->status = st;
    snprintf(rc->detail, sizeof rc->detail, "%s", why);
}

ink_rc_t ink_action_invoke(const char *id, const char *args_json, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    memset(rc, 0, sizeof *rc);
    const ink_action_t *a = find(id);
    char key[64];
    snprintf(key, sizeof key, "action.%s.%s", src == INK_SRC_AI ? "ai" : "user", id);
    ink_probe_count(key);
    if (!a) {
        reject(rc, INK_RC_INVALID, "unknown action");
        return rc->status;
    }
    if (!available(a)) {
        reject(rc, INK_RC_INVALID, "not available on this page");
        return rc->status;
    }
    /* policy gate: member and child whitelist */
    const ink_member_t *m = ink_member_current();
    if (m->role == INK_ROLE_CHILD) {
        if (a->effect == INK_FX_SYSTEM) {
            reject(rc, INK_RC_REJECTED, "children cannot run system actions");
            ink_probe_count("policy.rejected");
            return rc->status;
        }
        if (!ink_member_allows(a->id, a->child_ok)) {
            reject(rc, INK_RC_REJECTED, "not on this child's whitelist");
            ink_probe_count("policy.rejected");
            return rc->status;
        }
    }
    if (src == INK_SRC_AI && strstr(a->id, "upgrade")) { /* the AI never starts an upgrade */
        reject(rc, INK_RC_REJECTED, "upgrades are started by people only");
        ink_probe_count("policy.rejected");
        return rc->status;
    }
    /* a tool call prepared for an older page must not act on the new one */
    if (src == INK_SRC_AI && page_gen && a->activity && page_gen != ink_ui_generation()) {
        reject(rc, INK_RC_STALE, "the page changed since the request was made");
        return rc->status;
    }
    if (confirm_for(a, src) != INK_CONFIRM_NONE) {
        /* the system confirmation card is not built yet: report what would happen */
        reject(rc, INK_RC_PENDING_CONFIRMATION, "needs confirmation on the device");
        ink_probe_count("policy.pending_confirmation");
        return rc->status;
    }
    static ink_json_t j; /* UI task only */
    int atok = -1;
    if (args_json && *args_json) {
        if (ink_json_parse(&j, args_json, strlen(args_json)) != 0 || j.ntok < 1 || j.tok[0].type != 1) {
            reject(rc, INK_RC_INVALID, "arguments are not a JSON object");
            return rc->status;
        }
        atok = 0;
    }
    rc->status = INK_RC_DONE;
    a->run(atok >= 0 ? &j : NULL, atok, rc);
    return rc->status;
}

int ink_actions_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        const ink_action_t *a = REG[i];
        if (!available(a)) continue;
        const ink_member_t *m = ink_member_current();
        if (m->role == INK_ROLE_CHILD && (a->effect == INK_FX_SYSTEM || !ink_member_allows(a->id, a->child_ok))) continue;
        ink_jw_raw(&w, first ? "{\"name\":" : ",{\"name\":");
        first = false;
        ink_jw_str(&w, a->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, a->title);
        ink_jw_raw(&w, ",\"effect\":");
        ink_jw_str(&w, ink_effect_name(a->effect));
        ink_jw_raw(&w, ",\"confirm\":");
        ink_jw_str(&w, CONFIRM_NAME[confirm_for(a, INK_SRC_AI)]);
        ink_jw_raw(&w, ",\"inputSchema\":");
        ink_jw_raw(&w, a->args ? a->args : "{\"type\":\"object\",\"properties\":{}}");
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_actions_ids_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        if (!available(REG[i])) continue;
        if (!first) ink_jw_raw(&w, ",");
        first = false;
        ink_jw_str(&w, REG[i]->id);
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_receipt_json(const ink_receipt_t *rc, char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{\"status\":");
    ink_jw_str(&w, ink_rc_name(rc->status));
    if (rc->object[0]) {
        ink_jw_raw(&w, ",\"object\":");
        ink_jw_str(&w, rc->object);
        ink_jw_raw(&w, ",\"revision\":");
        ink_jw_int(&w, rc->revision);
    }
    if (rc->undo_ms) {
        ink_jw_raw(&w, ",\"undo_ms\":");
        ink_jw_int(&w, rc->undo_ms);
    }
    if (rc->say[0]) {
        ink_jw_raw(&w, ",\"say\":");
        ink_jw_str(&w, rc->say);
    }
    if (rc->detail[0]) {
        ink_jw_raw(&w, ",\"detail\":");
        ink_jw_str(&w, rc->detail);
    }
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}

/* ------------------------------------------------------------ hub tools */

typedef struct {
    int64_t id;
    bool list;
    uint32_t page_gen;
    uint64_t received_us;
    char name[48];
    char args[]; /* JSON object text */
} tool_job_t;

static void tool_run(void *arg) { /* UI task */
    tool_job_t *t = arg;
    const char *prev = osal_mem_tag("tools");
    char *out = osal_malloc(6144, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!out) {
        ink_hub_respond_error(t->id, -32603, "out of memory");
        osal_free(t);
        return;
    }
    if (t->list) {
        int n = ink_actions_json(out + 10, 6144 - 12);
        if (n < 0) ink_hub_respond_error(t->id, -32603, "tool list too long");
        else {
            memcpy(out, "{\"tools\":", 9);
            out[9] = ' ';
            out[10 + n] = '}';
            out[11 + n] = 0;
            ink_hub_respond(t->id, out);
        }
    } else {
        ink_receipt_t rc;
        ink_ui_cause(t->received_us, "ai.tool_to_visible_ms"); /* what the action changes on screen */
        ink_action_invoke(t->name, t->args[0] ? t->args : NULL, INK_SRC_AI, t->page_gen, &rc);
        char key[48];
        snprintf(key, sizeof key, "tools.calls.%s", ink_rc_name(rc.status));
        ink_probe_count(key);
        if (ink_receipt_json(&rc, out, 6144) < 0) ink_hub_respond_error(t->id, -32603, "receipt too long");
        else ink_hub_respond(t->id, out);
    }
    osal_free(out);
    osal_free(t);
}

static void on_tools(void *ctx, int64_t id, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    bool list = !strcmp(method, "tools/list");
    if (!list && strcmp(method, "tools/call")) {
        ink_hub_respond_error(id, -32601, "unknown tools method");
        return;
    }
    int args_t = list ? -1 : ink_json_find(j, params, "arguments");
    size_t alen = args_t >= 0 ? (size_t)(j->tok[args_t].end - j->tok[args_t].start) : 0;
    const char *prev = osal_mem_tag("tools");
    tool_job_t *t = osal_calloc(1, sizeof *t + alen + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!t) {
        ink_hub_respond_error(id, -32603, "out of memory");
        return;
    }
    t->id = id;
    t->list = list;
    t->received_us = osal_now_us();
    if (!list) {
        if (ink_json_str(j, ink_json_find(j, params, "name"), t->name, sizeof t->name) < 0) {
            osal_free(t);
            ink_hub_respond_error(id, -32602, "tools/call needs a name");
            return;
        }
        int64_t pg = 0;
        if (ink_json_int(j, ink_json_find(j, params, "page_gen"), &pg)) t->page_gen = (uint32_t)pg;
        if (alen) memcpy(t->args, j->text + j->tok[args_t].start, alen);
        t->args[alen] = 0;
    }
    ink_ui_post(tool_run, t);
}

int ink_tools_init(void) { return ink_hub_on_request("tools/", on_tools, NULL); }
