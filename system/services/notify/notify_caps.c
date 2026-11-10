/*
 * Notifications on the soft bus: what the notification list's buttons do, and what the AI can read and post.
 * The AI posts into its own channel; its notifications say who posted them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"
#include "notify_internal.h"

#define RESULT_CAP 4096

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void cap_post(ink_call_t *c) {
    const char *member = ink_cap_call_member(c);
    char title[48] = "", body[160] = "", group[24] = "", channel[16] = "", act[40] = "";
    ink_args_get_str(&c->args, "title", title, sizeof title);
    bool has_body = ink_args_get_str(&c->args, "body", body, sizeof body) >= 0;
    bool has_group = ink_args_get_str(&c->args, "group", group, sizeof group) >= 0;
    ink_args_get_str(&c->args, "channel", channel, sizeof channel);
    bool has_action = ink_args_get_str(&c->args, "action", act, sizeof act) > 0;
    bool from_person = c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY || c->src == INK_SRC_SYSTEM;
    ink_notify_t n = {.written_ver = &c->rc.revision, .channel = from_person && channel[0] ? channel : "ai", .title = title, .body = has_body ? body : NULL, .group = has_group ? group : NULL,
                      .action = has_action ? act : NULL, .prio = -1, .src = (uint8_t)c->src};
    uint32_t id = 0;
    if (ink_notify_post_for(&n, &id, member) != 0) {
        fail(c, INK_RC_FAILED, "not posted");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "notification:%u", (unsigned)id);
    ink_args_t *undo = osal_malloc(sizeof *undo, OSAL_MEM_EXTERNAL);
    if (undo && id) {
        ink_args_init(undo);
        ink_args_int(undo, "id", id);
        ink_args_int(undo, "if_revision", c->rc.revision);
        ink_cap_undoable(c, "notify.dismiss", undo, 10000);
    }
    osal_free(undo);
}

static void cap_list(ink_call_t *c) {
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    ink_notify_view_t *v = osal_malloc(sizeof *v, OSAL_MEM_EXTERNAL);
    if (!out || !v) {
        osal_free(out);
        osal_free(v);
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    int64_t limit = 20;
    bool unread_only = false;
    ink_args_get_int(&c->args, "limit", &limit);
    ink_args_get_bool(&c->args, "unread_only", &unread_only);
    ink_db_cursor_t cur = {0};
    ink_jw_t w;
    ink_jw_init(&w, out, RESULT_CAP);
    ink_jw_raw(&w, "{\"unread\":");
    ink_jw_int(&w, ink_notify_unread());
    ink_jw_raw(&w, ",\"items\":[");
    bool first = true;
    int shown = 0;
    while (shown < limit && ink_notify_next(&cur, v) == 1) {
        if (unread_only && v->read) continue;
        size_t before = w.len;
        ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":");
        ink_jw_int(&w, v->id);
        ink_jw_raw(&w, ",\"when_s\":");
        ink_jw_int(&w, v->when_s);
        ink_jw_raw(&w, ",\"channel\":");
        ink_jw_str(&w, v->channel);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, v->title);
        ink_jw_raw(&w, ",\"body\":");
        ink_jw_str(&w, v->body);
        ink_jw_raw(&w, v->read ? ",\"read\":true" : ",\"read\":false");
        if (v->group[0]) {
            ink_jw_raw(&w, ",\"group\":");
            ink_jw_str(&w, v->group);
        }
        if (v->action[0]) {
            ink_jw_raw(&w, ",\"action\":");
            ink_jw_str(&w, v->action);
        }
        ink_jw_raw(&w, "}");
        if (w.overflow) {
            w.overflow = false;
            w.len = before;
            break;
        }
        first = false;
        shown++;
    }
    ink_jw_raw(&w, "]}");
    if (w.overflow) fail(c, INK_RC_FAILED, "too long");
    else ink_cap_result(c, out);
    osal_free(out);
    osal_free(v);
}

static void cap_read(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    if (ink_notify_mark_read((uint32_t)id) != 0) fail(c, INK_RC_FAILED, "no such notification");
}

static void cap_dismiss(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    int64_t revision = 0;
    ink_args_get_int(&c->args, "if_revision", &revision);
    if (ink_notify_cancel_if_ver((uint32_t)id, (uint32_t)revision) != 0) fail(c, revision ? INK_RC_STALE : INK_RC_FAILED, "notification changed or absent");
}

static void cap_clear(ink_call_t *c) {
    if (ink_notify_clear() != 0) fail(c, INK_RC_FAILED, "not cleared");
}

static void cap_channels(ink_call_t *c) {
    char out[512]; /* 16 channels */
    ink_jw_t w;
    ink_jw_init(&w, out, sizeof out);
    ink_jw_raw(&w, "{");
    const ink_notify_channel_t *ch;
    for (int i = 0; (ch = ink_notify_channel_at(i)) != NULL; i++) {
        if (i) ink_jw_raw(&w, ",");
        ink_jw_str(&w, ch->id);
        ink_jw_raw(&w, ink_notify_channel_on(ch->id) ? ":true" : ":false");
    }
    ink_jw_raw(&w, "}");
    ink_cap_result(c, out);
}

static void cap_set_channel(ink_call_t *c) { /* exec.bg: the preferences are read and written through the data store */
    char id[16];
    bool on = true;
    ink_args_get_str(&c->args, "id", id, sizeof id);
    ink_args_get_bool(&c->args, "enabled", &on);
    if (ink_notify_channel_set(id, on) != 0) fail(c, INK_RC_INVALID, "no such channel");
}

static const ink_cap_t NOTIFY_CAPS[] = {
    {.id = "notify.post",
     .title = "发一条通知（给用户看的提醒；来源记为 AI）",
     .input = "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\",\"maxLength\":15},\"body\":{\"type\":\"string\",\"maxLength\":50},\"group\":{\"type\":\"string\",\"maxLength\":7},"
              "\"channel\":{\"type\":\"string\",\"maxLength\":15},\"action\":{\"type\":\"string\",\"maxLength\":39}},\"required\":[\"title\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_post},
    {.id = "notify.list",
     .title = "通知列表（新的在前）",
     .input = "{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":50},\"unread_only\":{\"type\":\"boolean\"}}}",
     .output = "{\"type\":\"object\",\"properties\":{\"unread\":{\"type\":\"integer\"},\"items\":{\"type\":\"array\"}}}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_list},
    {.id = "notify.read",
     .title = "标为已读（id 0 或不给：全部）",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":0}}}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_read},
    {.id = "notify.dismiss",
     .title = "删一条通知",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_dismiss},
    {.id = "notify.clear", .title = "清空通知", .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_clear},
    {.id = "notify.channels", .title = "哪些通知渠道开着", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_channels},
    {.id = "notify.set_channel",
     .title = "开关一个通知渠道",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"enabled\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = false,
     .exec = INK_EXEC_BG,
     .run = cap_set_channel},
    {.id = "notify.changed", .title = "通知列表有变化", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "notify.channels_changed", .title = "通知渠道有变化", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "notify.posted", .title = "有新通知", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "battery.low", .title = "电量低了（level 1 低，2 很低，3 马上关机，0 充上电了）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

int notify_caps_init(void) { return ink_cap_register(NOTIFY_CAPS, (int)(sizeof NOTIFY_CAPS / sizeof NOTIFY_CAPS[0])); }
