/*
 * Communication service (see ink/sys/comm.h): what a person asks of the 4G
 * modem, as capabilities: send and list text messages, dial, answer and hang
 * up calls; incoming messages and calls as events. Which card is used is a
 * setting (R-044: switched online, the steps are the driver's). The modem's
 * power-up, registration and AT commands stay below the HAL.
 *
 * Its task waits for modem events. Sending and dialling block for seconds, so
 * those capabilities run on the background executor.
 *
 * The card: the setting says which one is wanted, the modem which one is in
 * use, "cell.sim" reports the latter. sim_check() brings them together: at
 * start-up (a saved choice), after every set, and again after a failure.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/comm.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"

#define COMM_STACK 8192 /* PSRAM: never touches the flash; AT replies and SMS text are parsed on it */
#define COMM_PRIO 3
#define STATUS_DELAY_US 5000000ull /* first look at the modem, after it had time to register */
#define SIM_RETRY_FIRST_US 10000000ull /* a card that could not be selected, or a modem that does not answer: tried again */
#define SIM_RETRY_MAX_US 300000000ull
#define SIM_RETRIES 6
#define SMS_MIN_LEFT_MS 3000 /* with less time before its deadline a message is not started */

typedef struct {
    char from[24];
    char text[355];
} msg_t;

static const ink_db_def_t MESSAGES = {.name = "sms.messages", .title = "短信会话", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 512};

static struct {
    osal_mutex_t *lock;
    osal_mutex_t *at; /* the modem takes one AT command at a time, and some requests are several: the comm task
                       * and exec.bg both talk to it. Held while a request runs, up to the seconds it takes */
    bool ringing;
    char call_member[16], call_peer[24]; /* protected by at; member and tones also by lock */
    uint32_t call_generation;
    char tones[65]; /* local display only, protected by lock */
    int h_registered, h_sim;
    ink_exec_job_t sim_job;
    uint64_t sim_retry_at, sim_retry_us; /* comm task's next look at the card, 0 = none due */
    int sim_tries;
    /* comm task: too large for its stack, and internal RAM has no room for them */
    msg_t rx;
    ink_args_t rx_args, ev_args;
} CM INK_PSRAM_BSS;

static void call_begin(const char *member, const char *peer) { /* at held */
    CM.call_generation++;
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    snprintf(CM.call_member, sizeof CM.call_member, "%s", member);
    snprintf(CM.call_peer, sizeof CM.call_peer, "%s", peer);
    CM.tones[0] = 0;
    osal_mutex_unlock(CM.lock);
}

bool ink_comm_bind_dtmf(ink_args_t *args) {
    if (!CM.at) return false;
    char key[4] = "";
    ink_args_get_str(args, "key", key, sizeof key);
    ink_args_init(args);
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    bool bound = CM.call_member[0] && !strcmp(CM.call_member, ink_member_current()->id) &&
        !ink_args_str(args, "key", key) && !ink_args_int(args, "_call", CM.call_generation) && !ink_args_str(args, "_peer", CM.call_peer);
    osal_mutex_unlock(CM.at);
    return bound;
}

void ink_comm_call_tones(char *out, size_t cap) {
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    snprintf(out, cap, "%s", ink_member_space_open() && !strcmp(CM.call_member, ink_member_current()->id) ? CM.tones : "");
    osal_mutex_unlock(CM.lock);
}

static bool save_message(ink_db_t *db, uint32_t *id, const char *peer, const char *text, const char *status, uint8_t src) {
    char raw[1024]; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
    ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, peer); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, text);
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, status); ink_jw_raw(&w, "}");
    if (!*id) *id = ink_db_new_id(db);
    uint32_t ver = 0; ink_db_write_t wr = {.id = *id, .sort = *id, .data = raw, .len = w.len, .src = src, .written_ver = &ver};
    return !w.overflow && ink_db_put(db, &wr, NULL) == INK_DB_OK && ink_db_wait(db, ver, 3000000) == 0;
}
static void received(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char member[16], peer[24], text[355];
    ink_args_get_str(a, "member", member, sizeof member); ink_args_get_str(a, "from", peer, sizeof peer); ink_args_get_str(a, "text", text, sizeof text);
    ink_db_t *db = ink_db_open_member(&MESSAGES, member); uint32_t id = 0;
    if (!db || !save_message(db, &id, peer, text, "收到", INK_SRC_SYSTEM)) ink_probe_count("sms.save_failed");
    ink_db_close(db);
}

/* Cut a UTF-8 string so it does not end inside a character. */
static void utf8_clip(char *s) {
    size_t n = strlen(s), i = n;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) i--; /* continuation bytes */
    if (i > 0 && ((unsigned char)s[i - 1] & 0x80)) {               /* a lead byte: complete? */
        unsigned char lead = (unsigned char)s[i - 1];
        size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        if (n - (i - 1) < need) s[i - 1] = 0;
    }
}

/* Publish what the modem says now. False when it does not answer. */
static bool refresh_status(hal_cell_status_t *st) {
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    bool up = hal_cell_status(st);
    osal_mutex_unlock(CM.at);
    if (!up) {
        ink_state_set_bool(CM.h_registered, false);
        ink_state_set_str(CM.h_sim, "none");
        return false;
    }
    ink_state_set_bool(CM.h_registered, st->registered);
    ink_state_set_str(CM.h_sim, st->slot == HAL_CELL_SLOT_INTERNAL ? "internal" : "external");
    return true;
}

/* A call has the sound to itself, ringing or going on: media pauses, speech ends. */
static void call_focus(bool on) {
    static int token;
    if (on && !token) {
        int t = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
        __atomic_store_n(&token, t > 0 ? t : 0, __ATOMIC_RELEASE);
    } else if (!on) ink_audio_focus_release(__atomic_exchange_n(&token, 0, __ATOMIC_ACQ_REL));
}

static void on_sms(int index) { /* comm task */
    msg_t *m = &CM.rx;
    ink_args_t *a = &CM.rx_args;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int got = hal_cell_sms_take(index, m->from, sizeof m->from, m->text, sizeof m->text);
    osal_mutex_unlock(CM.at);
    if (got < 0) {
        ink_probe_count("comm.sms_read_failed");
        return;
    }
    utf8_clip(m->text);
    ink_args_init(a);
    ink_args_str(a, "from", m->from);
    ink_args_str(a, "text", m->text);
    ink_cap_emit("message.received", a);
}

/* ------------------------------------------------------------ the card in use */

static hal_cell_slot_t wanted_slot(void) { return ink_setting_get("cell.sim_slot") == 1 ? HAL_CELL_SLOT_INTERNAL : HAL_CELL_SLOT_EXTERNAL; }

/* Look again later, a few times, each wait longer; then give up until the next set of the setting. */
static void sim_retry(bool again) {
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    if (!again) {
        CM.sim_retry_at = CM.sim_retry_us = 0;
        CM.sim_tries = 0;
    } else if (CM.sim_tries++ < SIM_RETRIES) {
        CM.sim_retry_us = CM.sim_retry_us ? CM.sim_retry_us * 2 : SIM_RETRY_FIRST_US;
        if (CM.sim_retry_us > SIM_RETRY_MAX_US) CM.sim_retry_us = SIM_RETRY_MAX_US;
        CM.sim_retry_at = osal_now_us() + CM.sim_retry_us;
    } else {
        CM.sim_retry_at = 0;
        ink_probe_count("comm.sim_gave_up");
    }
    osal_mutex_unlock(CM.lock);
}

static void sim_switch(void *arg) { /* exec.bg: the switch takes seconds */
    (void)arg;
    hal_cell_slot_t want = wanted_slot();
    hal_cell_status_t st;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_status(&st) ? 0 : -1;
    bool switched = r == 0 && st.slot != want;
    if (switched) r = hal_cell_select_sim(want, 60000);
    osal_mutex_unlock(CM.at);
    if (switched) ink_probe_count(r == 0 ? "comm.sim_switched" : r == 1 ? "comm.sim_switched_unregistered" : "comm.sim_switch_failed");
    refresh_status(&st);
    sim_retry(r != 0); /* 1: on the new card, not registered yet: cell.registered is looked at again too */
}

/* Comm task: the first look, and each retry. exec.bg only when there is something to do. */
static void sim_check(void) {
    hal_cell_status_t st;
    bool up = refresh_status(&st);
    if (up && st.slot == wanted_slot()) sim_retry(false);
    else if (!up) sim_retry(true); /* still starting, or no modem */
    else if (ink_exec_post(INK_EXEC_BG, &CM.sim_job, sim_switch, NULL) != 0) sim_retry(true);
}

static void comm_task(void *arg) {
    (void)arg;
    uint64_t status_at = osal_now_us() + STATUS_DELAY_US;
    for (;;) {
        osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
        uint64_t retry_at = CM.sim_retry_at;
        osal_mutex_unlock(CM.lock);
        hal_cell_event_t ev;
        if (hal_cell_wait_event(&ev, status_at || retry_at ? 1000 : 60000)) { /* a long wait when nothing is due */
            ink_args_t *a = &CM.ev_args;
            ink_args_init(a);
            if (ev.type == HAL_CELL_EV_SMS) on_sms(ev.sms_index);
            else if (ev.type == HAL_CELL_EV_RING && !CM.ringing) { /* once per call: it rings again every few seconds */
                CM.ringing = true;
                osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
                call_begin(ink_member_current()->id, ev.number);
                osal_mutex_unlock(CM.at);
                call_focus(true);
                ink_args_str(a, "from", ev.number);
                ink_cap_emit("call.incoming", a);
            } else if (ev.type == HAL_CELL_EV_CALL_ACTIVE) ink_cap_emit("call.connected", a);
            else if (ev.type == HAL_CELL_EV_CALL_ENDED) { /* to everyone, like call.incoming: the screen follows the call */
                CM.ringing = false;
                call_focus(false);
                osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
                osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
                CM.call_member[0] = CM.call_peer[0] = CM.tones[0] = 0;
                osal_mutex_unlock(CM.lock);
                osal_mutex_unlock(CM.at);
                ink_confirm_expire("call.dtmf");
                ink_cap_emit("call.ended", a);
            }
        }
        uint64_t now = osal_now_us();
        if ((status_at && now >= status_at) || (retry_at && now >= retry_at)) {
            status_at = 0;
            osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
            if (CM.sim_retry_at == retry_at) CM.sim_retry_at = 0;
            osal_mutex_unlock(CM.lock);
            sim_check();
        }
    }
}

/* ------------------------------------------------------------ capabilities */

static void cap_send(ink_call_t *c) { /* exec.bg */
    char to[24], text[355]; /* 118 Han characters; reject rather than truncate longer byte strings. */
    ink_args_get_str(&c->args, "to", to, sizeof to);
    int bytes = ink_args_get_str(&c->args, "text", text, sizeof text);
    bool valid = to[0] && bytes > 0 && bytes < (int)sizeof text;
    for (size_t i = 0; to[i]; i++) if ((to[i] < '0' || to[i] > '9') && !(i == 0 && to[i] == '+' && to[1])) valid = false;
    if (!valid) { c->rc.status = INK_RC_INVALID; return; }
    ink_db_t *db = ink_db_open_member(&MESSAGES, ink_cap_call_member(c)); uint32_t id = 0;
    if (!db || !save_message(db, &id, to, text, "结果未知，请先核实", c->src)) {
        ink_db_close(db); c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "短信没能存下，未发送"); return;
    }
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    hal_cell_status_t st; bool known = hal_cell_status(&st);
    uint32_t left = ink_cap_time_left_ms(c); /* the time it waited for this executor and the modem counts */
    int r = (known && !st.registered) || left < SMS_MIN_LEFT_MS ? 1 : hal_cell_sms_send(to, text, left < 20000 ? left : 20000);
    osal_mutex_unlock(CM.at);
    const char *status = r == 0 ? "已发出" : r > 0 ? "发送失败，未发出" : "结果未知，请先核实";
    if (r != 0) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", r > 0 ? "not sent: offline or no time left" : "send result unknown"); }
    else ink_probe_count("comm.sms_sent");
    snprintf(c->rc.object, sizeof c->rc.object, "message:to:%s", to);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", status);
    if (!save_message(db, &id, to, text, status, c->src)) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "发送结果没能存下，请先核实"); }
    ink_db_close(db);
}

/* What snprintf cut short ends on a whole character: a half character would be read out as noise. */
static void whole_characters(char *s) {
    size_t n = strlen(s), j = n;
    while (j && ((unsigned char)s[j - 1] & 0xC0) == 0x80) j--;
    if (!j--) return;
    unsigned char b = (unsigned char)s[j];
    size_t len = b < 0xC0 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
    if (j + len > n) s[j] = 0;
}

static void cap_list(ink_call_t *c) {
    ink_db_t *db = ink_db_open_member(&MESSAGES, ink_cap_call_member(c));
    if (!db) { c->rc.status = INK_RC_FAILED; return; }
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; char raw[1024] = {0}, peer[24], text[355];
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int n = 0;
    if (!j) { ink_db_close(db); c->rc.status = INK_RC_FAILED; return; }
    while (ink_db_next(db, &cur, &rec, raw, sizeof raw - 1) == 1) {
        raw[rec.len] = 0;
        if (ink_json_parse(j, raw, rec.len) || !ink_json_eq(j, ink_json_find(j, 0, "status"), "收到")) continue;
        if (!n++) { ink_json_str(j, ink_json_find(j, 0, "peer"), peer, sizeof peer); ink_json_str(j, ink_json_find(j, 0, "text"), text, sizeof text); }
    }
    if (!n)
        snprintf(c->rc.say, sizeof c->rc.say, "没有新短信");
    else if (snprintf(c->rc.say, sizeof c->rc.say, "%d 条短信，最近一条来自 %s：%s", n, peer, text) >= (int)sizeof c->rc.say)
        whole_characters(c->rc.say); /* a long message is cut to what can be said */
    osal_free(j); ink_db_close(db);
    utf8_clip(c->rc.say);
    snprintf(c->rc.object, sizeof c->rc.object, "messages");
    c->rc.revision = (uint32_t)n;
}

static void call_result(ink_call_t *c, int r, const char *ok, const char *fail) {
    if (r != 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", fail);
    } else snprintf(c->rc.say, sizeof c->rc.say, "%s", ok);
}

static void cap_dial(ink_call_t *c) {
    char number[24];
    ink_args_get_str(&c->args, "number", number, sizeof number);
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_dial(number);
    if (!r) call_begin(ink_cap_call_member(c), number);
    osal_mutex_unlock(CM.at);
    if (r == 0) call_focus(true);
    call_result(c, r, "正在拨号", "the call could not be placed");
    if (r == 0) ink_cap_emit_for("call.dialed", &c->args, ink_cap_call_member(c));
}

static void cap_answer(ink_call_t *c) {
    CM.ringing = false;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_answer();
    osal_mutex_unlock(CM.at);
    call_result(c, r, "接通了", "nothing to answer");
    if (r == 0) ink_cap_emit("call.answered", NULL);
}

static void cap_hangup(ink_call_t *c) {
    CM.ringing = false;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_hangup();
    osal_mutex_unlock(CM.at);
    call_result(c, r, "挂断了", "no call to end");
}

static void cap_control(ink_call_t *c) { /* exec.bg */
    const char *id = c->cap->id;
    char key[4] = ""; bool on = false, muted = false;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = -1;
    if (!CM.call_member[0] || strcmp(CM.call_member, ink_cap_call_member(c))) c->rc.status = INK_RC_STALE;
    else if (!strcmp(id, "call.mute")) {
        if (ink_args_get_bool(&c->args, "on", &on)) r = hal_cell_mute(on, &muted);
    } else if (!strcmp(id, "call.dtmf")) {
        int64_t generation = 0; char peer[24] = "";
        ink_args_get_str(&c->args, "_peer", peer, sizeof peer);
        if (c->src == INK_SRC_CONFIRMED && (!ink_args_get_int(&c->args, "_call", &generation) ||
            generation != CM.call_generation || strcmp(peer, CM.call_peer))) c->rc.status = INK_RC_STALE;
        else if (ink_args_get_str(&c->args, "key", key, sizeof key) == 1 && strchr("0123456789*#ABCD", key[0])) r = hal_cell_dtmf(key[0]);
        else c->rc.status = INK_RC_INVALID;
        if (!r) {
            osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
            size_t n = strlen(CM.tones);
            if (n < sizeof CM.tones - 1) { CM.tones[n] = key[0]; CM.tones[n + 1] = 0; }
            osal_mutex_unlock(CM.lock);
        }
    } else if (!strcmp(id, "call.route")) {
        int64_t connected = 0; bool headset = false;
        ink_state_get_int(ink_state_find("bt.connected"), &connected);
        if (ink_args_get_bool(&c->args, "headset", &headset) && (!headset || connected)) {
            ink_audio_set_route(headset ? INK_ROUTE_HEADSET : INK_ROUTE_SPEAKER); r = 0;
        } else snprintf(c->rc.say, sizeof c->rc.say, "没有连接蓝牙耳机，请先连接耳机");
    } else snprintf(c->rc.say, sizeof c->rc.say, "通话音频接线未核实，暂不能录音");
    osal_mutex_unlock(CM.at);
    if (r && c->rc.status == INK_RC_DONE) c->rc.status = INK_RC_FAILED;
    if (r && !c->rc.say[0]) snprintf(c->rc.say, sizeof c->rc.say, "模组没有确认，请重试");
    if (!r) {
        ink_args_init(&c->args);
        if (!strcmp(id, "call.mute")) ink_args_bool(&c->args, "muted", muted);
        ink_cap_emit_for("call.controls", &c->args, ink_cap_call_member(c));
    }
}

#define BOOL_ARG(name) "{\"type\":\"object\",\"properties\":{\"" name "\":{\"type\":\"boolean\"}},\"required\":[\"" name "\"]}"

static const ink_cap_t COMM_CAPS[] = {
    {.id = "call.mute", .title = "通话麦克风静音", .input = BOOL_ARG("on"), .effect = INK_FX_NAVIGATE, .child_ok = true, .permission = "call", .timeout_ms = 15000, .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.dtmf", .title = "发送一个通话按键", .ask = "向 {_peer} 的本次通话发送按键 {key}", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":1}},\"required\":[\"key\"]}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "call", .timeout_ms = 10000, .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.route", .title = "切换通话耳机或喇叭", .input = BOOL_ARG("headset"), .effect = INK_FX_NAVIGATE, .child_ok = true, .permission = "call", .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.record", .title = "查看通话录音不可用的原因", .effect = INK_FX_READ, .child_ok = true, .permission = "call", .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.controls", .title = "通话控制已获模组确认", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "message.send",
     .title = "发短信",
     .ask = "发短信给 {to}：{text}",
     .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":118}},"
              "\"required\":[\"to\",\"text\"]}",
     .effect = INK_FX_EXTERNAL,
     .child_ok = true, /* children like adults (D-25); the AI still waits for confirmation */
     .permission = "message",
     .qos = INK_QOS_NETWORK,
     .timeout_ms = 30000,
     .exec = INK_EXEC_BG,
     .run = cap_send},
    {.id = "message.list", .title = "最近的短信", .effect = INK_FX_READ, .child_ok = true, .permission = "message", .exec = INK_EXEC_BG, .run = cap_list},
    {.id = "sms.send", .title = "保存并发送短信", .ask = "发短信给 {to}：{text}", .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":118}},\"required\":[\"to\",\"text\"]}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "message", .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .timeout_ms = 30000, .run = cap_send},
    {.id = "call.dial",
     .title = "打电话",
     .ask = "打电话给 {number}",
     .input = "{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"number\"]}",
     .effect = INK_FX_EXTERNAL,
     .child_ok = true,
     .permission = "call",
     .qos = INK_QOS_NETWORK,
     .timeout_ms = 10000,
     .exec = INK_EXEC_BG,
     .run = cap_dial},
    /* the modem may be busy with a message for up to 20 s; a call rings longer than that */
    {.id = "call.answer", .title = "接电话", .effect = INK_FX_EXTERNAL, .child_ok = true, .while_locked = true, .permission = "call", .timeout_ms = 25000, .exec = INK_EXEC_BG, .run = cap_answer},
    {.id = "call.hangup", .title = "挂电话", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .permission = "call", .timeout_ms = 25000, .exec = INK_EXEC_BG, .run = cap_hangup},
    {.id = "message.received", .title = "收到短信", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "message"},
    {.id = "call.incoming", .title = "来电", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "call.answered", .title = "电话接通了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "call.ended", .title = "通话结束", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "call.dialed", .title = "电话已拨出", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "call.connected", .title = "拨出的电话已接通", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
};

/* ------------------------------------------------------------ state and settings */

static const ink_state_def_t ST_REGISTERED = {.id = "cell.registered", .type = INK_ST_BOOL, .title = "4G 在线"};
static const ink_state_def_t ST_SIM = {.id = "cell.sim", .type = INK_ST_STR, .title = "在用的 SIM 卡"};

static void apply_slot(void *ctx, int64_t v) { /* exec.bg */
    (void)ctx;
    (void)v; /* it reads the newest */
    sim_retry(false); /* a set starts over */
    sim_switch(NULL);
}

static const char *const SLOTS[] = {"external", "internal", NULL};
static const ink_setting_def_t COMM_SETTINGS[] = {
    {.id = "cell.sim_slot", .title = "SIM 卡", .type = INK_SET_ENUM, .def = 0, .choices = SLOTS, .apply = apply_slot, .apply_exec = INK_EXEC_BG},
};

int ink_comm_init(void) {
    if (ink_db_register(&MESSAGES) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG)) return -1;
    CM.lock = osal_mutex_create();
    CM.at = osal_mutex_create();
    if (!CM.lock || !CM.at) return -1;
    CM.h_registered = ink_state_define(&ST_REGISTERED);
    CM.h_sim = ink_state_define(&ST_SIM);
    ink_settings_define(COMM_SETTINGS, 1);
    if (ink_cap_register(COMM_CAPS, (int)(sizeof COMM_CAPS / sizeof COMM_CAPS[0])) != 0) return -1;
    osal_task_cfg_t cfg = {.name = "comm", .fn = comm_task, .stack_bytes = COMM_STACK, .priority = COMM_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
}
