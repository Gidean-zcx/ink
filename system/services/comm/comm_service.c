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
#include "ink/sys/cap.h"
#include "ink/sys/comm.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"

#define COMM_STACK 8192 /* PSRAM: never touches the flash; AT replies and SMS text are parsed on it */
#define COMM_PRIO 3
#define MSG_KEEP 16
#define STATUS_DELAY_US 5000000ull /* first look at the modem, after it had time to register */
#define SIM_RETRY_FIRST_US 10000000ull /* a card that could not be selected, or a modem that does not answer: tried again */
#define SIM_RETRY_MAX_US 300000000ull
#define SIM_RETRIES 6
#define SMS_MIN_LEFT_MS 3000 /* with less time before its deadline a message is not started */

typedef struct {
    char from[24];
    char text[200];
} msg_t;

static struct {
    osal_mutex_t *lock;
    osal_mutex_t *at; /* the modem takes one AT command at a time, and some requests are several: the comm task
                       * and exec.bg both talk to it. Held while a request runs, up to the seconds it takes */
    msg_t msg[MSG_KEEP]; /* newest last */
    int nmsg;
    bool ringing;
    int h_registered, h_sim;
    ink_exec_job_t sim_job;
    uint64_t sim_retry_at, sim_retry_us; /* comm task's next look at the card, 0 = none due */
    int sim_tries;
    /* comm task: too large for its stack, and internal RAM has no room for them */
    msg_t rx;
    ink_args_t rx_args, ev_args;
} CM INK_PSRAM_BSS;

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
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    if (CM.nmsg == MSG_KEEP) memmove(&CM.msg[0], &CM.msg[1], sizeof CM.msg[0] * (MSG_KEEP - 1));
    else CM.nmsg++;
    CM.msg[CM.nmsg - 1] = *m;
    osal_mutex_unlock(CM.lock);
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
                ink_args_str(a, "from", ev.number);
                ink_cap_emit("call.incoming", a);
            } else if (ev.type == HAL_CELL_EV_CALL_ENDED) {
                CM.ringing = false;
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
    char to[24], text[INK_ARGS_TEXT]; /* a message is never cut: 118 Chinese characters fit in the arguments (INK_ARGS_TEXT);
                                       * with other 4-byte characters the arguments may refuse it, also never cut */
    ink_args_get_str(&c->args, "to", to, sizeof to);
    ink_args_get_str(&c->args, "text", text, sizeof text);
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    uint32_t left = ink_cap_time_left_ms(c); /* the time it waited for this executor and the modem counts */
    int r = left < SMS_MIN_LEFT_MS ? 1 : hal_cell_sms_send(to, text, left < 20000 ? left : 20000);
    osal_mutex_unlock(CM.at);
    if (r > 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "not sent: no time left");
        return;
    }
    if (r != 0) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "the message was not sent");
        return;
    }
    ink_probe_count("comm.sms_sent");
    snprintf(c->rc.object, sizeof c->rc.object, "message:to:%s", to);
    snprintf(c->rc.say, sizeof c->rc.say, "短信发出去了");
}

static void cap_list(ink_call_t *c) {
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    int n = CM.nmsg;
    if (n) snprintf(c->rc.say, sizeof c->rc.say, "%d 条短信，最近一条来自 %s：%s", n, CM.msg[n - 1].from, CM.msg[n - 1].text);
    else snprintf(c->rc.say, sizeof c->rc.say, "没有新短信");
    osal_mutex_unlock(CM.lock);
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
    osal_mutex_unlock(CM.at);
    call_result(c, r, "正在拨号", "the call could not be placed");
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

static const ink_cap_t COMM_CAPS[] = {
    {.id = "message.send",
     .title = "发短信",
     .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":118}},"
              "\"required\":[\"to\",\"text\"]}",
     .effect = INK_FX_EXTERNAL,
     .child_ok = true, /* children like adults (D-25); the AI still waits for confirmation */
     .permission = "message",
     .qos = INK_QOS_NETWORK,
     .timeout_ms = 30000,
     .exec = INK_EXEC_BG,
     .run = cap_send},
    {.id = "message.list", .title = "最近的短信", .effect = INK_FX_READ, .child_ok = true, .permission = "message", .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "call.dial",
     .title = "打电话",
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
