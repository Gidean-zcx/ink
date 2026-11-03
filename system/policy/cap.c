/*
 * Capability registry, policy gate and receipts (see ink/sys/cap.h), plus the
 * hub's "tools/list" / "tools/call" (the AI's way in, D-12: MCP tool and
 * result structures inside the device's own JSON-RPC envelope).
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

#define MAX_CAPS 96
#define MAX_DEPTH 2 /* a handler may invoke one more capability itself */

static const ink_cap_t *REG[MAX_CAPS];
static int nreg;

static const char *RC_NAME[] = {"done",     "partial", "queued", "pending_confirmation", "rejected",
                                "stale",    "invalid", "failed", "cancelled",            "undone"};
static const char *FX_NAME[] = {"read", "navigate", "local_write", "local_destructive", "external", "system"};
static const char *CONFIRM_NAME[] = {"none", "device", "parent"};
static const char *FORM_NAME[] = {"call", "stream", "event", "session"};

const char *ink_rc_name(ink_rc_t rc) { return (unsigned)rc < sizeof RC_NAME / sizeof RC_NAME[0] ? RC_NAME[rc] : "?"; }
const char *ink_effect_name(ink_effect_t fx) { return (unsigned)fx < sizeof FX_NAME / sizeof FX_NAME[0] ? FX_NAME[fx] : "?"; }

int ink_cap_register(const ink_cap_t *table, int n) {
    for (int i = 0; i < n; i++) {
        if (nreg >= MAX_CAPS) return -1;
        REG[nreg++] = &table[i];
    }
    return 0;
}

const ink_cap_t *ink_cap_find(const char *id) {
    for (int i = 0; i < nreg; i++)
        if (!strcmp(REG[i]->id, id)) return REG[i];
    return NULL;
}

static bool on_its_page(const ink_cap_t *c) { return !c->activity || !strcmp(ink_ui_top_name(), c->activity); }

bool ink_cap_available(const char *id) {
    const ink_cap_t *c = ink_cap_find(id);
    return c && on_its_page(c);
}

/* Who may see it: the AI and the hub only what is exposed to them; touch, keys and the system
 * are the owner's own controls. */
static bool exposed(const ink_cap_t *c, ink_source_t src) {
    if (!c->expose) return true;
    switch (src) {
    case INK_SRC_AI: return c->expose & INK_EXPOSE_AI;
    case INK_SRC_REMOTE: return c->expose & INK_EXPOSE_REMOTE;
    case INK_SRC_APP: return c->expose & INK_EXPOSE_APP;
    default: return true;
    }
}

/* Default confirmation by side effect (foundation §6.2). Direct manipulation (touch, keys) is the
 * member's own confirmation except where a PIN is required. */
static ink_confirm_t confirm_for(const ink_cap_t *c, ink_source_t src) {
    ink_confirm_t k = INK_CONFIRM_NONE;
    switch (c->effect) {
    case INK_FX_LOCAL_DESTRUCTIVE:
    case INK_FX_EXTERNAL:
    case INK_FX_SYSTEM: k = INK_CONFIRM_DEVICE; break;
    default: break;
    }
    if (src == INK_SRC_TOUCH || src == INK_SRC_KEY || src == INK_SRC_SYSTEM) return INK_CONFIRM_NONE;
    return k;
}

static ink_rc_t reject(ink_receipt_t *rc, ink_rc_t st, const char *why) {
    rc->status = st;
    snprintf(rc->detail, sizeof rc->detail, "%s", why);
    return st;
}

/* The policy gate (detailed design §2.4), before anything runs. INK_RC_DONE = may run. */
static ink_rc_t gate(const ink_cap_t *c, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    if (!c) return reject(rc, INK_RC_INVALID, "unknown action");
    if (!on_its_page(c)) return reject(rc, INK_RC_INVALID, "not available on this page");
    if (!exposed(c, src)) return reject(rc, INK_RC_INVALID, "not offered to this caller");
    const ink_member_t *m = ink_member_current();
    if (m->role == INK_ROLE_CHILD) {
        if (c->effect == INK_FX_SYSTEM) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "children cannot run system actions");
        }
        if (!ink_member_allows(c->id, c->child_ok)) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "not on this child's whitelist");
        }
    }
    bool remote = src == INK_SRC_AI || src == INK_SRC_REMOTE;
    if (remote && strstr(c->id, "upgrade")) { /* the AI never starts an upgrade */
        ink_probe_count("policy.rejected");
        return reject(rc, INK_RC_REJECTED, "upgrades are started by people only");
    }
    /* a tool call prepared for an older page must not act on the new one */
    if (remote && page_gen && c->activity && page_gen != ink_ui_generation())
        return reject(rc, INK_RC_STALE, "the page changed since the request was made");
    if (confirm_for(c, src) != INK_CONFIRM_NONE) {
        /* the system confirmation card is not built yet: report what would happen */
        ink_probe_count("policy.pending_confirmation");
        return reject(rc, INK_RC_PENDING_CONFIRMATION, "needs confirmation on the device");
    }
    return INK_RC_DONE;
}

static ink_call_t CALLS[MAX_DEPTH]; /* UI task only */
static int depth;

/* Gate, arguments (a table, or JSON to convert), run. */
static ink_rc_t invoke(const char *id, const ink_args_t *args, const char *json, ink_source_t src, uint32_t page_gen,
                       ink_receipt_t *rc) {
    memset(rc, 0, sizeof *rc);
    char key[64];
    snprintf(key, sizeof key, "action.%s.%s", src == INK_SRC_AI ? "ai" : "user", id);
    ink_probe_count(key);
    const ink_cap_t *c = ink_cap_find(id);
    if (gate(c, src, page_gen, rc) != INK_RC_DONE) return rc->status;
    if (depth >= MAX_DEPTH) return reject(rc, INK_RC_FAILED, "capabilities nested too deep");
    ink_call_t *call = &CALLS[depth];
    char why[96];
    int bad;
    if (json) bad = ink_args_from_json(&call->args, json, strlen(json), c->input, why, sizeof why);
    else {
        if (args) call->args = *args;
        else ink_args_init(&call->args);
        bad = ink_args_check(&call->args, c->input, why, sizeof why);
    }
    if (bad) return reject(rc, INK_RC_INVALID, why);
    call->cap = c;
    call->src = src;
    call->page_gen = page_gen;
    memset(&call->rc, 0, sizeof call->rc);
    call->rc.status = INK_RC_DONE;
    depth++;
    c->run(call);
    depth--;
    *rc = call->rc;
    return rc->status;
}

ink_rc_t ink_cap_invoke(const char *id, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    return invoke(id, args, NULL, src, page_gen, rc);
}

ink_rc_t ink_cap_invoke_json(const char *id, const char *args_json, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    return invoke(id, NULL, args_json && *args_json ? args_json : NULL, src, page_gen, rc);
}

/* What the current member may use now, as the AI sees it. */
static bool listed(const ink_cap_t *c) {
    if (!on_its_page(c) || !exposed(c, INK_SRC_AI)) return false;
    const ink_member_t *m = ink_member_current();
    return m->role != INK_ROLE_CHILD || (c->effect != INK_FX_SYSTEM && ink_member_allows(c->id, c->child_ok));
}

int ink_cap_manifest_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        const ink_cap_t *c = REG[i];
        if (!listed(c)) continue;
        ink_jw_raw(&w, first ? "{\"name\":" : ",{\"name\":");
        first = false;
        ink_jw_str(&w, c->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, c->title);
        ink_jw_raw(&w, ",\"effect\":");
        ink_jw_str(&w, ink_effect_name(c->effect));
        ink_jw_raw(&w, ",\"confirm\":");
        ink_jw_str(&w, CONFIRM_NAME[confirm_for(c, INK_SRC_AI)]);
        if (c->version > 1) {
            ink_jw_raw(&w, ",\"version\":");
            ink_jw_int(&w, c->version);
        }
        if (c->form != INK_FORM_CALL) {
            ink_jw_raw(&w, ",\"form\":");
            ink_jw_str(&w, FORM_NAME[c->form]);
        }
        ink_jw_raw(&w, ",\"inputSchema\":");
        ink_jw_raw(&w, c->input ? c->input : "{\"type\":\"object\",\"properties\":{}}");
        if (c->output) {
            ink_jw_raw(&w, ",\"outputSchema\":");
            ink_jw_raw(&w, c->output);
        }
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_cap_ids_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        if (!on_its_page(REG[i]) || !exposed(REG[i], INK_SRC_AI)) continue;
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
        int n = ink_cap_manifest_json(out + 10, 6144 - 12);
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
        ink_ui_cause(t->received_us, "ai.tool_to_visible_ms"); /* what the capability changes on screen */
        ink_cap_invoke_json(t->name, t->args, INK_SRC_AI, t->page_gen, &rc);
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
