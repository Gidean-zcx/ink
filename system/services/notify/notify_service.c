/*
 * Notification service (see ink/sys/notify.h): the inbox of each member, channels,
 * do-not-disturb; and what turns the system's events into notifications and
 * interruptions. The other services know nothing of this: they emit events (a
 * text message arrived, an alarm fired, the battery is low) and this one subscribes.
 *
 * The current member's inbox stays open, so posting touches only memory. Another
 * member's opens on the background executor, which may wait for the flash.
 *
 * Which task the events arrive in: a call and an alarm directly in the one that
 * emitted them (the modem's, the timer's), so that nothing queues in front of them;
 * and so an alarm's handler must not post a notification or use much stack. The rest
 * on the background executor: the power and storage tasks have small stacks of
 * internal RAM, and a low battery can wait for a file copy to take its next step.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/notify.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "notify_internal.h"

#define MAX_CHANNELS 16
#define RECORD_CAP 1024
#define INBOX_KEEP 200

static const ink_db_def_t INBOX = {.name = "notify.inbox", .title = "通知", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true,
                                   .max_records = INBOX_KEEP, .max_bytes = 64u * 1024u, .drop_oldest = true};
static const ink_db_def_t PREFS = {.name = "notify.prefs", .title = "通知渠道开关", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true};

static const ink_notify_channel_t BUILTIN[] = {
    {"system", "系统", INK_NOTIFY_NORMAL}, {"message", "短信", INK_NOTIFY_NORMAL}, {"call", "电话", INK_NOTIFY_HIGH},   {"alarm", "闹钟", INK_NOTIFY_HIGH},
    {"reminder", "提醒", INK_NOTIFY_HIGH}, {"download", "下载", INK_NOTIFY_LOW},   {"ai", "AI", INK_NOTIFY_NORMAL},
};

static struct {
    const ink_notify_channel_t *ch[MAX_CHANNELS];
    int nch;
    uint32_t off;  /* the current member's channels turned off */
    ink_db_t *held; /* the current member's inbox stays loaded */
    char member[INK_MEMBER_ID_MAX];
    int unread;
    int h_unread;
    ink_exec_job_t haptic_job;
    /* the call on screen */
    bool call_answered;
    char call_from[24];
    ink_interrupt_t alarm_it; /* the timer task's: its stack is small */
} NS INK_PSRAM_BSS;

/* The current member's inbox, for one use: a switch of members may close the held one at any time. It is
 * loaded, so this only counts a reference. */
static ink_db_t *inbox(void) {
    char who[INK_MEMBER_ID_MAX];
    ink_svc_lock();
    bool open = NS.held != NULL;
    snprintf(who, sizeof who, "%s", NS.member);
    ink_svc_unlock();
    return open ? ink_db_open(&INBOX, who) : NULL;
}

/* ------------------------------------------------------------ the inbox */

/* {"read":false,"prio":1,... : the flag is first, so counting needs no parsing. */
static const char READ_PREFIX[] = "{\"read\":true";

static bool record_read(const char *json, size_t n) { return n >= sizeof READ_PREFIX - 1 && !memcmp(json, READ_PREFIX, sizeof READ_PREFIX - 1); }

static int channel_index(const char *id) {
    for (int i = 0; i < NS.nch; i++)
        if (!strcmp(NS.ch[i]->id, id)) return i;
    return -1;
}

static void publish_unread(void) {
    ink_svc_lock();
    int n = NS.unread;
    ink_svc_unlock();
    ink_state_set_int(NS.h_unread, n);
}

static void recount(void) {
    char buf[16];
    int n = 0;
    ink_db_t *db = inbox();
    if (db) {
        ink_db_cursor_t c = {0};
        ink_rec_t r;
        while (ink_db_next(db, &c, &r, buf, sizeof buf) == 1)
            if (!record_read(buf, r.len < sizeof buf ? r.len : sizeof buf)) n++;
        ink_db_close(db);
    }
    ink_svc_lock();
    NS.unread = n;
    ink_svc_unlock();
    publish_unread();
}

static int encode(char *out, size_t cap, const ink_notify_t *n, const char *channel, int prio, bool read) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, read ? "{\"read\":true,\"prio\":" : "{\"read\":false,\"prio\":");
    ink_jw_int(&w, prio);
    ink_jw_raw(&w, ",\"ch\":");
    ink_jw_str(&w, channel);
    ink_jw_raw(&w, ",\"title\":");
    ink_jw_str(&w, n->title ? n->title : "");
    if (n->body) {
        ink_jw_raw(&w, ",\"body\":");
        ink_jw_str(&w, n->body);
    }
    if (n->group) {
        ink_jw_raw(&w, ",\"group\":");
        ink_jw_str(&w, n->group);
    }
    if (n->tag) {
        ink_jw_raw(&w, ",\"tag\":");
        ink_jw_str(&w, n->tag);
    }
    if (n->action) {
        ink_jw_raw(&w, ",\"action\":");
        ink_jw_str(&w, n->action);
        if (n->action_args) {
            ink_jw_raw(&w, ",\"args\":");
            ink_jw_raw(&w, n->action_args);
        }
    }
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}

static uint32_t find_tag(ink_db_t *db, const char *tag) {
    char *buf = osal_malloc(RECORD_CAP, OSAL_MEM_EXTERNAL);
    char needle[48];
    snprintf(needle, sizeof needle, "\"tag\":\"%s\"", tag);
    uint32_t hit = 0;
    if (buf) {
        ink_db_cursor_t c = {0};
        ink_rec_t r;
        while (!hit && ink_db_next(db, &c, &r, buf, RECORD_CAP - 1) == 1) {
            buf[r.len < RECORD_CAP - 1 ? r.len : RECORD_CAP - 1] = 0;
            if (strstr(buf, needle)) hit = r.id;
        }
    }
    osal_free(buf);
    return hit;
}

static void announce(uint32_t id, const char *channel, const char *title, int prio) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_int(a, "id", id);
        ink_args_str(a, "channel", channel);
        ink_args_str(a, "title", title);
        ink_args_int(a, "prio", prio);
        ink_cap_emit("notify.posted", a);
        osal_free(a);
    }
}

static void haptic_job(void *arg) { /* exec.bg: the motor is on the shared I2C bus */
    (void)arg;
    hal_haptic_pulse(0);
}

/* The channels someone turned off. Waits for the flash when it is not the current member. */
static uint32_t prefs_off(const char *member) {
    uint32_t off = 0;
    ink_db_t *db = ink_db_open(&PREFS, member);
    if (!db) return 0;
    char buf[256];
    ink_rec_t r;
    if (ink_db_get(db, 1, &r, buf, sizeof buf - 1) == INK_DB_OK) {
        buf[r.len < sizeof buf - 1 ? r.len : sizeof buf - 1] = 0;
        for (int i = 0; i < NS.nch; i++) {
            char needle[32];
            snprintf(needle, sizeof needle, "\"%s\"", NS.ch[i]->id);
            if (strstr(buf, needle)) off |= 1u << i;
        }
    }
    ink_db_close(db);
    return off;
}

typedef struct {
    ink_exec_job_t job;
    char member[INK_MEMBER_ID_MAX];
    uint8_t src;
    int channel;
    int64_t sort;
    char json[];
} away_t;

static void post_away(void *arg) { /* exec.bg: another member's inbox, nobody is disturbed */
    away_t *a = arg;
    if (prefs_off(a->member) & (1u << a->channel)) ink_probe_count("notify.channel_off");
    else {
        ink_db_t *db = ink_db_open(&INBOX, a->member);
        ink_db_write_t w = {.sort = a->sort, .data = a->json, .len = strlen(a->json), .src = a->src};
        if (!db || ink_db_put(db, &w, NULL) != INK_DB_OK) ink_probe_count("notify.dropped");
        ink_db_close(db);
        ink_svc_lock();
        bool now_theirs = !strcmp(NS.member, a->member); /* the device changed hands while this waited */
        ink_svc_unlock();
        if (now_theirs) recount();
    }
    osal_free(a);
}

int ink_notify_post(const ink_notify_t *n, uint32_t *out_id) {
    if (!n || !n->title) return -1;
    const char *channel = n->channel ? n->channel : "system";
    int ci = channel_index(channel);
    if (ci < 0) {
        channel = "system";
        ci = 0;
    }
    int prio = n->prio >= 0 ? n->prio : (int)NS.ch[ci]->prio;
    char who[INK_MEMBER_ID_MAX];
    snprintf(who, sizeof who, "%s", n->member ? n->member : ink_member_current()->id);
    int64_t wall = osal_wall_us();
    int64_t sort = wall > 0 ? wall / 1000000 : (int64_t)(osal_now_us() / 1000000);
    const char *prev = osal_mem_tag("notify");
    char *buf = osal_malloc(RECORD_CAP, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!buf) return -1;
    int len = encode(buf, RECORD_CAP, n, channel, prio, false);
    if (len < 0) {
        osal_free(buf);
        ink_probe_count("notify.too_long");
        return -1;
    }
    ink_svc_lock();
    bool mine = !strcmp(who, NS.member);
    bool off = mine && (NS.off & (1u << ci));
    ink_svc_unlock();
    if (!mine) {
        away_t *a = ink_layout_member_ok(who) ? osal_calloc(1, sizeof *a + (size_t)len + 1, OSAL_MEM_EXTERNAL) : NULL;
        if (!a) {
            osal_free(buf);
            return -1;
        }
        snprintf(a->member, sizeof a->member, "%s", who);
        a->src = n->src;
        a->channel = ci;
        a->sort = sort;
        memcpy(a->json, buf, (size_t)len + 1);
        osal_free(buf);
        if (ink_exec_post(INK_EXEC_BG, &a->job, post_away, a) != 0) {
            osal_free(a);
            return -1;
        }
        return 0;
    }
    if (off) {
        osal_free(buf);
        ink_probe_count("notify.channel_off");
        return 0;
    }
    ink_db_t *db = ink_db_open(&INBOX, who); /* whose it is, as decided above, whatever has happened since */
    uint32_t wrote = 0;
    ink_db_write_t w = {.id = db && n->tag ? find_tag(db, n->tag) : 0, .sort = sort, .data = buf, .len = (size_t)len, .src = n->src};
    int r = db ? ink_db_put(db, &w, &wrote) : INK_DB_ERR_ARG;
    ink_db_close(db);
    osal_free(buf);
    if (r != INK_DB_OK) {
        ink_probe_count("notify.dropped");
        return -1;
    }
    if (out_id) *out_id = wrote;
    recount();
    ink_probe_count("notify.posted");
    ink_svc_lock();
    mine = !strcmp(who, NS.member);
    ink_svc_unlock();
    if (!mine) return 0; /* someone else came meanwhile: it is in the right inbox, not for them to hear about */
    announce(wrote, channel, n->title, prio);
    if (prio >= INK_NOTIFY_HIGH && !ink_setting_get("notify.dnd")) { /* no banner: e-ink would need a refresh for it to go away too */
        ink_probe_count("notify.alerted");
        ink_audio_tone(INK_TONE_DONE, 0, NULL);
        ink_exec_post(INK_EXEC_BG, &NS.haptic_job, haptic_job, NULL);
    }
    return 0;
}

int ink_notify_cancel(uint32_t id) {
    ink_db_t *db = inbox();
    int r = db ? ink_db_del(db, id, INK_SRC_SYSTEM) : INK_DB_ERR_ARG;
    ink_db_close(db);
    if (r != INK_DB_OK) return -1;
    recount();
    return 0;
}

int ink_notify_cancel_tag(const char *tag) {
    ink_db_t *db = inbox();
    uint32_t id = db ? find_tag(db, tag) : 0;
    ink_db_close(db);
    return id ? ink_notify_cancel(id) : -1;
}

/* The record again with its flag changed: the rest of the text as it was. */
static int mark_one(ink_db_t *db, uint32_t id, char *buf) {
    ink_rec_t r;
    if (ink_db_get(db, id, &r, buf, RECORD_CAP - 16) != INK_DB_OK) return -1;
    if (record_read(buf, r.len)) return 0;
    static const char UNREAD[] = "{\"read\":false";
    size_t skip = sizeof UNREAD - 1, have = r.len < RECORD_CAP - 16 ? r.len : RECORD_CAP - 16;
    if (have <= skip || memcmp(buf, UNREAD, skip)) return -1;
    memmove(buf + sizeof READ_PREFIX - 1, buf + skip, have - skip);
    memcpy(buf, READ_PREFIX, sizeof READ_PREFIX - 1);
    ink_db_write_t w = {.id = id, .sort = r.sort, .data = buf, .len = have - skip + sizeof READ_PREFIX - 1, .src = r.src, .flags = INK_DB_LAZY};
    return ink_db_put(db, &w, NULL) == INK_DB_OK ? 1 : -1;
}

int ink_notify_mark_read(uint32_t id) {
    ink_db_t *db = inbox();
    char *buf = db ? osal_malloc(RECORD_CAP, OSAL_MEM_EXTERNAL) : NULL;
    if (!buf) {
        ink_db_close(db);
        return -1;
    }
    int r = 0;
    if (id) r = mark_one(db, id, buf);
    else {
        ink_db_cursor_t c = {0};
        ink_rec_t rec;
        while (ink_db_next(db, &c, &rec, NULL, 0) == 1) mark_one(db, rec.id, buf);
    }
    osal_free(buf);
    ink_db_close(db);
    recount();
    return r < 0 ? -1 : 0;
}

int ink_notify_clear(void) {
    ink_db_t *db = inbox();
    if (!db) return -1;
    ink_db_cursor_t c = {0};
    ink_rec_t r;
    while (ink_db_next(db, &c, &r, NULL, 0) == 1) ink_db_del(db, r.id, INK_SRC_SYSTEM);
    ink_db_close(db);
    recount();
    return 0;
}

int ink_notify_next(ink_db_cursor_t *c, ink_notify_view_t *out) {
    ink_db_t *db = inbox();
    if (!db) return 0;
    char *buf = osal_malloc(RECORD_CAP, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    ink_rec_t r;
    int got = 0;
    c->desc = true;
    if (buf && j && (got = ink_db_next(db, c, &r, buf, RECORD_CAP - 1)) == 1) {
        size_t n = r.len < RECORD_CAP - 1 ? r.len : RECORD_CAP - 1;
        buf[n] = 0;
        memset(out, 0, sizeof *out);
        out->id = r.id;
        out->when_s = r.sort;
        out->src = r.src;
        out->read = record_read(buf, n);
        if (ink_json_parse(j, buf, n) == 0) {
            int64_t v = 0;
            if (ink_json_int(j, ink_json_find(j, 0, "prio"), &v)) out->prio = (uint8_t)v;
            ink_json_str(j, ink_json_find(j, 0, "ch"), out->channel, sizeof out->channel);
            ink_json_str(j, ink_json_find(j, 0, "title"), out->title, sizeof out->title);
            ink_json_str(j, ink_json_find(j, 0, "body"), out->body, sizeof out->body);
            ink_json_str(j, ink_json_find(j, 0, "group"), out->group, sizeof out->group);
            ink_json_str(j, ink_json_find(j, 0, "tag"), out->tag, sizeof out->tag);
            ink_json_str(j, ink_json_find(j, 0, "action"), out->action, sizeof out->action);
            int args = ink_json_find(j, 0, "args");
            if (args >= 0) snprintf(out->action_args, sizeof out->action_args, "%.*s", j->tok[args].end - j->tok[args].start, j->text + j->tok[args].start);
        }
    }
    osal_free(j);
    osal_free(buf);
    ink_db_close(db);
    return got == 1 ? 1 : 0;
}

int ink_notify_unread(void) {
    ink_svc_lock();
    int n = NS.unread;
    ink_svc_unlock();
    return n;
}

/* ------------------------------------------------------------ channels */

int ink_notify_channel_define(const ink_notify_channel_t *ch) {
    ink_svc_lock();
    int r = -1;
    if (ch && NS.nch < MAX_CHANNELS && channel_index(ch->id) < 0) {
        NS.ch[NS.nch++] = ch;
        r = 0;
    }
    ink_svc_unlock();
    return r;
}

const ink_notify_channel_t *ink_notify_channel_at(int i) {
    ink_svc_lock();
    const ink_notify_channel_t *ch = i >= 0 && i < NS.nch ? NS.ch[i] : NULL;
    ink_svc_unlock();
    return ch;
}

static void save_prefs(void) {
    char who[INK_MEMBER_ID_MAX], out[256];
    ink_svc_lock();
    uint32_t off = NS.off;
    snprintf(who, sizeof who, "%s", NS.member);
    ink_svc_unlock();
    ink_db_t *db = ink_db_open(&PREFS, who);
    if (!db) return;
    ink_jw_t w;
    ink_jw_init(&w, out, sizeof out);
    ink_jw_raw(&w, "{\"off\":[");
    bool first = true;
    for (int i = 0; i < NS.nch; i++)
        if (off & (1u << i)) {
            if (!first) ink_jw_raw(&w, ",");
            first = false;
            ink_jw_str(&w, NS.ch[i]->id);
        }
    ink_jw_raw(&w, "]}");
    ink_db_write_t rec = {.id = 1, .data = out, .len = w.len};
    if (!w.overflow) ink_db_put(db, &rec, NULL);
    ink_db_close(db);
}

int ink_notify_channel_set(const char *id, bool on) {
    int i = channel_index(id);
    if (i < 0) return -1;
    ink_svc_lock();
    if (on) NS.off &= ~(1u << i);
    else NS.off |= 1u << i;
    ink_svc_unlock();
    save_prefs();
    return 0;
}

bool ink_notify_channel_on(const char *id) {
    int i = channel_index(id);
    ink_svc_lock();
    bool on = i >= 0 && !(NS.off & (1u << i));
    ink_svc_unlock();
    return on;
}

/* ------------------------------------------------------------ members */

static void open_inbox(void) { /* may wait for the flash */
    const char *id = ink_member_current()->id;
    ink_db_t *db = ink_db_open(&INBOX, id);
    uint32_t off = prefs_off(id);
    ink_svc_lock();
    ink_db_t *old = NS.held;
    NS.held = db;
    NS.off = off;
    snprintf(NS.member, sizeof NS.member, "%s", id);
    ink_svc_unlock();
    ink_db_close(old);
    recount();
}

static void on_member(void *ctx, int h, const char *id) { /* exec.bg: another member's files are read */
    (void)ctx;
    (void)h;
    (void)id;
    open_inbox();
}

/* ------------------------------------------------------------ what the other services' events mean */

static void action(ink_int_action_t *a, const char *label, const char *cap, const char *args) {
    a->label = label;
    a->cap = cap;
    snprintf(a->args, sizeof a->args, "%s", args ? args : "");
}

static void on_call(void *ctx, const char *id, const ink_args_t *p) { /* the modem's task; the answer's on exec.bg */
    (void)ctx;
    if (!strcmp(id, "call.incoming")) {
        ink_interrupt_t it = {.kind = INK_INT_CALL, .ringing = !ink_setting_get("notify.dnd"), .nactions = 2};
        snprintf(it.title, sizeof it.title, "来电");
        ink_args_get_str(p, "from", it.body, sizeof it.body);
        action(&it.actions[0], "接听", "call.answer", NULL);
        action(&it.actions[1], "挂断", "call.hangup", NULL);
        ink_svc_lock();
        NS.call_answered = false;
        snprintf(NS.call_from, sizeof NS.call_from, "%.23s", it.body); /* a number: it fits */
        ink_svc_unlock();
        ink_interrupt_raise(&it);
    } else if (!strcmp(id, "call.answered")) {
        int h = notify_interrupt_find(INK_INT_CALL, 0);
        ink_int_action_t hang;
        action(&hang, "挂断", "call.hangup", NULL);
        ink_svc_lock();
        NS.call_answered = true;
        ink_svc_unlock();
        if (h) ink_interrupt_update(h, "通话中", &hang, 1, false);
    } else if (!strcmp(id, "call.ended")) {
        int h = notify_interrupt_find(INK_INT_CALL, 0);
        ink_svc_lock();
        bool missed = !NS.call_answered;
        char from[sizeof NS.call_from];
        snprintf(from, sizeof from, "%s", NS.call_from);
        ink_svc_unlock();
        if (h) ink_interrupt_end(h);
        if (h && missed) {
            ink_notify_t n = {.channel = "call", .title = "未接来电", .body = from, .group = "missed-calls", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
            ink_notify_post(&n, NULL);
        }
    }
}

static void on_alarm(void *ctx, const char *id, const ink_args_t *p) { /* the timer task (or a capability's): short, little stack */
    (void)ctx;
    int64_t ref = 0;
    ink_args_get_int(p, "id", &ref);
    if (!strcmp(id, "alarm.ended")) {
        int h = notify_interrupt_find(INK_INT_ALARM, (uint32_t)ref);
        if (h) ink_interrupt_end(h);
        return;
    }
    ink_interrupt_t *it = &NS.alarm_it; /* only the timer task fires alarms */
    char args[32];
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ref);
    memset(it, 0, sizeof *it);
    it->kind = INK_INT_ALARM;
    it->ref = (uint32_t)ref;
    it->ringing = true; /* do not disturb does not silence an alarm */
    it->nactions = 2;
    snprintf(it->title, sizeof it->title, "闹钟");
    ink_args_get_str(p, "title", it->body, sizeof it->body);
    action(&it->actions[0], "关闭", "alarm.dismiss", args);
    action(&it->actions[1], "稍后", "alarm.snooze", args);
    ink_interrupt_raise(it);
}

static void on_missed(void *ctx, const char *id, const ink_args_t *p) { /* exec.bg */
    (void)ctx;
    char title[48] = "", owner[16] = "";
    ink_args_get_str(p, "title", title, sizeof title);
    ink_args_get_str(p, "owner", owner, sizeof owner);
    bool alarm = !strncmp(id, "alarm.", 6), missed = alarm || !strcmp(id, "reminder.missed");
    ink_notify_t n = {.channel = alarm ? "alarm" : "reminder", .title = alarm ? "错过的闹钟" : missed ? "错过的提醒" : "提醒", .body = title,
                      .prio = missed ? INK_NOTIFY_NORMAL : INK_NOTIFY_HIGH, .member = owner[0] ? owner : NULL, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

static void on_message(void *ctx, const char *id, const ink_args_t *p) { /* exec.bg */
    (void)ctx;
    (void)id;
    char from[24] = "", text[160] = "";
    ink_args_get_str(p, "from", from, sizeof from);
    ink_args_get_str(p, "text", text, sizeof text);
    ink_notify_t n = {.channel = "message", .title = from, .body = text, .group = from, .prio = -1, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

static void on_battery(void *ctx, const char *id, const ink_args_t *p) { /* exec.bg */
    (void)ctx;
    (void)id;
    int64_t level = 0, percent = 0;
    ink_args_get_int(p, "level", &level);
    ink_args_get_int(p, "percent", &percent);
    int up = notify_interrupt_find(INK_INT_LOW_BATTERY, 0);
    if (!level) { /* charging */
        if (up) ink_interrupt_end(up);
        ink_notify_cancel_tag("battery");
        return;
    }
    char body[48];
    snprintf(body, sizeof body, "还剩 %d%%，请充电", (int)percent);
    if (level == 1) {
        ink_notify_t n = {.channel = "system", .title = "电量低", .body = body, .tag = "battery", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
        ink_notify_post(&n, NULL);
    } else if (!up) {
        ink_interrupt_t it = {.kind = INK_INT_LOW_BATTERY, .nactions = 1};
        snprintf(it.title, sizeof it.title, level >= 3 ? "马上关机" : "电量很低");
        snprintf(it.body, sizeof it.body, "%s", body);
        action(&it.actions[0], "知道了", "interrupt.ack", NULL);
        ink_interrupt_raise(&it);
        ink_audio_tone(INK_TONE_ERROR, 0, NULL);
    }
}

static void on_data_failed(void *ctx, const char *id, const ink_args_t *p) { /* exec.bg */
    (void)ctx;
    (void)id;
    (void)p;
    ink_notify_t n = {.channel = "system", .title = "有内容还没保存好", .body = "内部存储可能满了，系统会再试", .tag = "storage", .prio = INK_NOTIFY_HIGH, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

static void on_usb(void *ctx, const char *id, const ink_args_t *p) { /* exec.bg */
    (void)ctx;
    (void)id;
    bool connected = false, lent = false;
    ink_args_get_bool(p, "connected", &connected);
    ink_args_get_bool(p, "lent", &lent);
    if (connected && !lent) {
        ink_notify_t n = {.channel = "system", .title = "连接了电脑", .body = "可以把 TF 卡当 U 盘用", .tag = "usb", .action = "usb.mount", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
        ink_notify_post(&n, NULL);
    } else ink_notify_cancel_tag("usb");
}

/* ------------------------------------------------------------ start */

static const ink_state_def_t ST_UNREAD = {.id = "notify.unread", .type = INK_ST_INT, .title = "没看的通知数"};
static const ink_setting_def_t NOTIFY_SETTINGS[] = {
    {.id = "notify.dnd", .title = "勿扰", .type = INK_SET_BOOL, .def = 0, .child_may_change = true},
};

int ink_notify_init(void) {
    if (!NS.nch)
        for (size_t i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++) NS.ch[NS.nch++] = &BUILTIN[i];
    NS.h_unread = ink_state_define(&ST_UNREAD);
    if (ink_settings_define(NOTIFY_SETTINGS, 1) != 0 || ink_db_register(&INBOX) != 0 || notify_interrupt_init() != 0 || notify_caps_init() != 0) return -1;
    open_inbox();
    if (ink_state_watch("member.current", on_member, NULL, INK_EXEC_BG) != 0) return -1;
    if (ink_cap_subscribe("call.", on_call, NULL, INK_EXEC_DIRECT) || ink_cap_subscribe("alarm.fired", on_alarm, NULL, INK_EXEC_DIRECT) ||
        ink_cap_subscribe("alarm.ended", on_alarm, NULL, INK_EXEC_DIRECT) || ink_cap_subscribe("alarm.missed", on_missed, NULL, INK_EXEC_BG) ||
        ink_cap_subscribe("reminder.", on_missed, NULL, INK_EXEC_BG) || ink_cap_subscribe("message.received", on_message, NULL, INK_EXEC_BG) ||
        ink_cap_subscribe("battery.low", on_battery, NULL, INK_EXEC_BG) || ink_cap_subscribe("data.write_failed", on_data_failed, NULL, INK_EXEC_BG) ||
        ink_cap_subscribe("usb.changed", on_usb, NULL, INK_EXEC_BG))
        return -1;
    return 0;
}
