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
 *
 * Airplane mode (the setting net.airplane): the modem's radio is off, also
 * after a restart (the modem does not keep it); calls and messages are refused
 * with the reason, and a card switch waits until the radio is on again.
 *
 * Long messages come in parts (3GPP TS 23.040 §9.2.3.24.1). Each part is kept
 * in sms.parts (on the flash: a restart loses none) until all have come; then
 * they are one message.received, in order. A part still missing after
 * SMS_PART_WAIT_US: what came is shown, marked incomplete, so a code or an
 * address is not held back for good. Parts kept before a restart wait again.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
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
#include "ink/sys/sha256.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/textinput.h"
#include "ink/sys/time.h"

#define COMM_STACK 8192 /* PSRAM: never touches the flash; AT replies and SMS text are parsed on it */
#define COMM_PRIO 3
#define STATUS_DELAY_US 5000000ull /* first look at the modem, after it had time to register */
#define SIM_RETRY_FIRST_US 10000000ull /* a card that could not be selected, or a modem that does not answer: tried again */
#define SIM_RETRY_MAX_US 300000000ull
#define SIM_RETRIES 6
#define SMS_MIN_LEFT_MS 3000 /* with less time before its deadline a message is not started */
/* The parts of one message are sent one after another and the network hands them on together, within seconds;
 * one that is still missing after this is waiting for the network's next try, often much later (ESTIMATE). */
#define SMS_PART_WAIT_US 600000000ull
#define SMS_GROUPS 8 /* messages being put together at once; a ninth shows the oldest as it is */
#define SMS_INCOMPLETE "【短信不完整】"
#define REG_LOOK_US 2000000ull /* after the radio came on, how often registering is looked at, and how many times */
#define REG_LOOKS 15

static const ink_db_def_t MESSAGES = {.name = "sms.messages", .title = "短信会话", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 512};
static const ink_db_def_t PARTS = {.name = "sms.parts", .title = "未收齐的长短信", .scope = INK_DB_DEVICE, .schema = 1,
    .json = true, .max_records = 64};
static const ink_db_def_t MOBILE = {.name = "net.mobile", .title = "移动网络私有设置", .scope = INK_DB_DEVICE, .schema = 1, .max_records = 2};

typedef struct { char peer[24]; int64_t ref, total; } sms_key_t; /* one long message: sender, reference, parts */

static struct {
    osal_mutex_t *lock;
    osal_mutex_t *at; /* the modem takes one AT command at a time, and some requests are several: the comm task
                       * and exec.bg both talk to it. Held while a request runs, up to the seconds it takes */
    bool ringing;
    char call_member[16], call_peer[24]; /* protected by at; member and tones also by lock */
    uint32_t call_generation;
    char tones[65]; /* local display only, protected by lock */
    int h_registered, h_sim, h_roaming, h_mobile, mobile_rev;
    hal_cell_apn_t apn;
    struct { uint64_t current, previous; int month, day; } usage;
    uint64_t sampled_bytes;
    ink_exec_job_t sim_job, reg_job;
    ink_svc_deadline_t reg_at;
    uint64_t sim_retry_at, sim_retry_us; /* comm task's next look at the card, 0 = none due */
    int sim_tries;
    ink_exec_job_t radio_job, apn_job;
    /* comm task: too large for its stack, and internal RAM has no room for them */
    hal_cell_sms_t rx;
    ink_args_t rx_args, ev_args;
    /* exec.bg only (one task, one job at a time): too large for its stack */
    char text[INK_SMS_TEXT_MAX], part[sizeof ((hal_cell_sms_t *)0)->text], raw[3 * INK_SMS_TEXT_MAX];
    ink_json_t json;
    ink_args_t bg_args;
    struct { sms_key_t key; uint64_t since; } groups[SMS_GROUPS]; /* since: 0 = free */
    bool parts_loaded;
    int groups_open; /* atomic: the comm task looks again while messages wait for parts */
    ink_exec_job_t sweep_job;
} CM INK_PSRAM_BSS;

static void call_begin(const char *member, const char *peer) { /* at held */
    CM.call_generation++;
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    snprintf(CM.call_member, sizeof CM.call_member, "%s", member);
    snprintf(CM.call_peer, sizeof CM.call_peer, "%s", peer);
    CM.tones[0] = 0;
    osal_mutex_unlock(CM.lock);
}

static ink_rc_t ink_comm_bind_dtmf(ink_args_t *args, ink_receipt_t *rc, ink_source_t src) {
    (void)src;
    if (CM.at) {
        char key[4] = "";
        ink_args_get_str(args, "key", key, sizeof key);
        ink_args_init(args);
        osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
        bool bound = CM.call_member[0] && !strcmp(CM.call_member, ink_member_current()->id) &&
            !ink_args_str(args, "key", key) && !ink_args_int(args, "_call", CM.call_generation) && !ink_args_str(args, "_peer", CM.call_peer);
        osal_mutex_unlock(CM.at);
        if (bound) return INK_RC_DONE;
    }
    snprintf(rc->detail, sizeof rc->detail, "the call is no longer available");
    snprintf(rc->say, sizeof rc->say, "当前没有可用的通话，请重新操作。");
    return INK_RC_STALE;
}

void ink_comm_call_tones(char *out, size_t cap) {
    osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
    snprintf(out, cap, "%s", ink_member_space_open() && !strcmp(CM.call_member, ink_member_current()->id) ? CM.tones : "");
    osal_mutex_unlock(CM.lock);
}

static bool save_message(ink_db_t *db, uint32_t *id, const char *peer, const char *text, const char *status, uint8_t src) { /* exec.bg */
    char *raw = CM.raw; ink_jw_t w; ink_jw_init(&w, raw, sizeof CM.raw);
    ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, peer); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, text);
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, status); ink_jw_raw(&w, "}");
    if (!*id) *id = ink_db_new_id(db);
    uint32_t ver = 0; ink_db_write_t wr = {.id = *id, .sort = *id, .data = raw, .len = w.len, .src = src, .written_ver = &ver};
    return !w.overflow && ink_db_put(db, &wr, NULL) == INK_DB_OK && ink_db_wait(db, ver, 3000000) == 0;
}
static void received(void *ctx, const char *event, const ink_args_t *a) {
    if (ink_args_find(a, "id")) return; /* assembled text was stored before its bounded event */
    (void)ctx; (void)event; char member[16], peer[24], *text = CM.text;
    ink_args_get_str(a, "member", member, sizeof member); ink_args_get_str(a, "from", peer, sizeof peer); ink_args_get_str(a, "text", text, sizeof CM.text);
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
    ink_state_set_bool(CM.h_roaming, st->roaming);
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

static void on_sms(int index) { /* comm task: like a whole message, a part is stored on exec.bg */
    hal_cell_sms_t *m = &CM.rx;
    ink_args_t *a = &CM.rx_args;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int got = hal_cell_sms_take(index, m);
    osal_mutex_unlock(CM.at);
    if (got < 0) {
        ink_probe_count("comm.sms_read_failed");
        return;
    }
    utf8_clip(m->text);
    ink_args_init(a);
    ink_args_str(a, "from", m->number);
    ink_args_str(a, "text", m->text);
    if (m->total > 1) { ink_args_int(a, "ref", m->ref); ink_args_int(a, "total", m->total); ink_args_int(a, "seq", m->seq); }
    ink_cap_emit(m->total > 1 ? "comm.sms_part" : "message.received", a);
}

/* ------------------------------------------------------------ long messages: parts (exec.bg) */

/* With seq 0: the next kept part after the cursor, whichever message it belongs to (*k filled in). Else part seq of
 * message *k, its text into CM.part. Its record id, 0 if none. */
static uint32_t part_next(ink_db_t *db, ink_db_cursor_t *cur, sms_key_t *k, int64_t seq) {
    ink_rec_t rec; sms_key_t f; int64_t q = 0; ink_json_t *j = &CM.json;
    while (ink_db_next(db, cur, &rec, CM.raw, sizeof CM.raw - 1) == 1) {
        CM.raw[rec.len < sizeof CM.raw ? rec.len : sizeof CM.raw - 1] = 0;
        if (ink_json_parse(j, CM.raw, strlen(CM.raw)) || ink_json_str(j, ink_json_find(j, 0, "peer"), f.peer, sizeof f.peer) < 0 ||
            !ink_json_int(j, ink_json_find(j, 0, "ref"), &f.ref) || !ink_json_int(j, ink_json_find(j, 0, "total"), &f.total) ||
            !ink_json_int(j, ink_json_find(j, 0, "seq"), &q)) continue;
        if (!seq) { *k = f; return rec.id; }
        if (strcmp(f.peer, k->peer) || f.ref != k->ref || f.total != k->total || q != seq) continue;
        ink_json_str(j, ink_json_find(j, 0, "text"), CM.part, sizeof CM.part);
        return rec.id;
    }
    return 0;
}
static uint32_t part_find(ink_db_t *db, sms_key_t k, int64_t seq) { ink_db_cursor_t cur = {0}; return part_next(db, &cur, &k, seq); }
static bool same(const sms_key_t *a, const sms_key_t *b) { return !strcmp(a->peer, b->peer) && a->ref == b->ref && a->total == b->total; }

/* All parts here: one message, in order. force (waited long enough, or no room): what came, "……" for what did not. */
static void settle(ink_db_t *db, sms_key_t k, bool force) {
    int64_t have = 0;
    for (int64_t s = 1; s <= k.total; s++) have += part_find(db, k, s) != 0;
    if (have < k.total && !force) return;
    size_t n = (size_t)snprintf(CM.text, sizeof CM.text, "%s", have < k.total ? SMS_INCOMPLETE : "");
    for (int64_t s = 1; s <= k.total; s++) {
        uint32_t id = part_find(db, k, s);
        const char *piece = id ? CM.part : n >= 6 && !strcmp(CM.text + n - 6, "……") ? "" : "……"; /* one mark per gap */
        n += (size_t)snprintf(CM.text + n, sizeof CM.text - n, "%.*s", (int)(sizeof CM.text - 1 - n), piece);
        if (id) ink_db_del(db, id, INK_SRC_SYSTEM);
    }
    utf8_clip(CM.text); /* longer than any message sent from here: what fits */
    ink_db_t *messages = ink_db_open_member(&MESSAGES, ink_member_current()->id); uint32_t id = 0;
    bool saved = messages && save_message(messages, &id, k.peer, CM.text, "收到", INK_SRC_SYSTEM);
    ink_db_close(messages);
    if (!saved) ink_probe_count("sms.save_failed");
    size_t preview = strlen(CM.text); if (preview > 354) preview = 354;
    memcpy(CM.raw, CM.text, preview); CM.raw[preview] = 0; utf8_clip(CM.raw);
    ink_args_init(&CM.bg_args);
    ink_args_str(&CM.bg_args, "from", k.peer);
    ink_args_str(&CM.bg_args, "text", CM.raw);
    ink_args_int(&CM.bg_args, "id", id);
    ink_cap_emit("message.received", &CM.bg_args);
    int open = 0;
    for (int i = 0; i < SMS_GROUPS; i++) {
        if (CM.groups[i].since && same(&CM.groups[i].key, &k)) CM.groups[i].since = 0;
        open += CM.groups[i].since != 0;
    }
    __atomic_store_n(&CM.groups_open, open, __ATOMIC_RELEASE);
}

/* A message waiting for parts, from now on (the wait starts again after a restart). No room: the oldest is shown. */
static void group_note(ink_db_t *db, const sms_key_t *k) {
    int at = -1, oldest = 0;
    for (int i = 0; i < SMS_GROUPS; i++) {
        if (CM.groups[i].since && same(&CM.groups[i].key, k)) return;
        if (!CM.groups[i].since && at < 0) at = i;
        if (CM.groups[i].since < CM.groups[oldest].since) oldest = i;
    }
    if (at < 0) settle(db, CM.groups[at = oldest].key, true);
    CM.groups[at].key = *k;
    CM.groups[at].since = osal_now_us() | 1;
    __atomic_add_fetch(&CM.groups_open, 1, __ATOMIC_ACQ_REL);
}

static void sweep(void *arg) { /* exec.bg: parts kept before a restart; messages that waited long enough */
    (void)arg;
    ink_db_t *db = ink_db_open(&PARTS, NULL);
    if (!db) return;
    sms_key_t k; ink_db_cursor_t cur = {0};
    while (!CM.parts_loaded && part_next(db, &cur, &k, 0)) group_note(db, &k);
    CM.parts_loaded = true;
    for (int i = 0; i < SMS_GROUPS; i++)
        if (CM.groups[i].since && osal_now_us() - CM.groups[i].since >= SMS_PART_WAIT_US) settle(db, CM.groups[i].key, true);
    ink_db_close(db);
}

static void part_arrived(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; sms_key_t k = {0}; int64_t seq = 0;
    ink_args_get_str(a, "from", k.peer, sizeof k.peer);
    ink_args_get_int(a, "ref", &k.ref); ink_args_get_int(a, "total", &k.total); ink_args_get_int(a, "seq", &seq);
    ink_db_t *db = ink_db_open(&PARTS, NULL);
    if (!db) { ink_probe_count("sms.save_failed"); return; }
    sweep(NULL); /* after a restart, what was kept comes first */
    if (!part_find(db, k, seq)) { /* a part sent again is kept once */
        ink_args_get_str(a, "text", CM.part, sizeof CM.part);
        ink_jw_t w; ink_jw_init(&w, CM.raw, sizeof CM.raw); uint32_t ver = 0;
        ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, k.peer); ink_jw_raw(&w, ",\"ref\":"); ink_jw_int(&w, k.ref); ink_jw_raw(&w, ",\"total\":");
        ink_jw_int(&w, k.total); ink_jw_raw(&w, ",\"seq\":"); ink_jw_int(&w, seq); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, CM.part); ink_jw_raw(&w, "}");
        ink_db_write_t wr = {.id = ink_db_new_id(db), .sort = seq, .data = CM.raw, .len = w.len, .src = INK_SRC_SYSTEM, .written_ver = &ver};
        if (w.overflow || ink_db_put(db, &wr, NULL) != INK_DB_OK || ink_db_wait(db, ver, 3000000) != 0) ink_probe_count("sms.save_failed");
    }
    group_note(db, &k);
    settle(db, k, false);
    ink_db_close(db);
}

int ink_comm_sms_parts(const char *text) { return hal_cell_sms_parts(text); }

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

static void sim_switch(void *arg) { /* exec.bg: the switch takes seconds; from radio() (arg) no waiting to register */
    hal_cell_slot_t want = wanted_slot();
    hal_cell_status_t st;
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_status(&st) ? 0 : -1;
    bool switched = r == 0 && st.slot != want && !ink_setting_get("net.airplane"); /* the switch turns the radio on */
    if (switched) r = hal_cell_select_sim(want, arg ? 0 : 60000);
    osal_mutex_unlock(CM.at);
    if (switched) ink_probe_count(r == 0 ? "comm.sim_switched" : r == 1 ? "comm.sim_switched_unregistered" : "comm.sim_switch_failed");
    refresh_status(&st);
    sim_retry(r != 0); /* 1: on the new card, not registered yet: cell.registered is looked at again too */
}

/* After the radio came on: has the card registered? Looked at now and then for REG_LOOKS, the modem free in between. */
static void reg_due(void *arg);
static void reg_look(void *arg) { /* exec.bg */
    hal_cell_status_t st;
    uintptr_t n = (uintptr_t)arg;
    if (!ink_setting_get("net.airplane") && refresh_status(&st) && !st.registered && n < REG_LOOKS)
        ink_svc_at(&CM.reg_at, osal_now_us() + REG_LOOK_US, reg_due, (void *)(n + 1));
}
static void reg_due(void *arg) { ink_exec_post(INK_EXEC_BG, &CM.reg_job, reg_look, arg); } /* timer task */

static void radio(void *arg) { /* exec.bg: airplane mode changed, or the modem came up with it on */
    (void)arg;
    bool on = !ink_setting_get("net.airplane");
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    int r = hal_cell_radio(on, 0); /* only the command: registering is not waited for with the modem held */
    osal_mutex_unlock(CM.at);
    if (r < 0) ink_probe_count("comm.radio_failed");
    hal_cell_status_t st;
    if (!on) refresh_status(&st);
    else sim_switch((void *)1); /* a card chosen meanwhile; it also says what the modem does now */
    if (on) ink_svc_at(&CM.reg_at, osal_now_us() + REG_LOOK_US, reg_due, (void *)1);
}
static void airplane_changed(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; radio(NULL); }

/* Refused while airplane mode is on: no radio. The way out is said, and the page offers it. */
static bool airplane(ink_call_t *c, const char *what) {
    if (!ink_setting_get("net.airplane")) return false;
    c->rc.status = INK_RC_REJECTED;
    snprintf(c->rc.detail, sizeof c->rc.detail, "airplane mode is on");
    snprintf(c->rc.say, sizeof c->rc.say, "飞行模式已打开，关闭飞行模式才能%s", what);
    return true;
}

/* Comm task: the first look, and each retry. exec.bg only when there is something to do. */
static void sim_check(void) {
    hal_cell_status_t st;
    bool up = refresh_status(&st);
    if (up && ink_setting_get("net.airplane") && ink_exec_post(INK_EXEC_BG, &CM.radio_job, radio, NULL) == 0) sim_retry(false);
    else if (up && st.slot == wanted_slot()) sim_retry(false);
    else if (!up) sim_retry(true); /* still starting, or no modem */
    else if (ink_exec_post(INK_EXEC_BG, &CM.sim_job, sim_switch, NULL) != 0) sim_retry(true);
}

static void comm_task(void *arg) {
    (void)arg;
    uint64_t status_at = osal_now_us() + (ink_setting_get("net.airplane") ? 0 : STATUS_DELAY_US); /* the radio goes off at once */
    uint64_t sweep_at = osal_now_us() + 60000000ull; /* parts kept before a restart: once start-up is over (a new part looks too) */
    bool loaded = false;
    for (;;) {
        osal_mutex_lock(CM.lock, OSAL_WAIT_FOREVER);
        uint64_t retry_at = CM.sim_retry_at;
        osal_mutex_unlock(CM.lock);
        hal_cell_event_t ev;
        bool waiting = !loaded || __atomic_load_n(&CM.groups_open, __ATOMIC_ACQUIRE) > 0;
        if (hal_cell_wait_event(&ev, status_at || retry_at ? 1000 : waiting ? 10000 : 60000)) { /* a long wait when nothing is due */
            ink_args_t *a = &CM.ev_args;
            ink_args_init(a);
            if (ev.type == HAL_CELL_EV_SMS) on_sms(ev.sms_index);
            else if (ev.type == HAL_CELL_EV_REG) { hal_cell_status_t st; refresh_status(&st); }
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
        if (waiting && now >= sweep_at && ink_exec_post(INK_EXEC_BG, &CM.sweep_job, sweep, NULL) == 0) sweep_at = now + 10000000ull, loaded = true;
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

/* What a message's record and receipt say: of a long one, which parts went and which did not. */
static void send_status(char *s, size_t cap, int r, int parts, int sent) {
    if (r == 0) { snprintf(s, cap, "已发出"); return; }
    if (parts == 1 || (!sent && r > 0)) { snprintf(s, cap, "%s", r > 0 ? "发送失败，未发出" : "结果未知，请先核实"); return; }
    int rest = parts - sent - (r < 0);
    char done[48] = "", unknown[64] = "", left[32] = "";
    if (sent) snprintf(done, sizeof done, "前 %d 条已发出，", sent);
    if (r < 0) snprintf(unknown, sizeof unknown, "第 %d 条结果未知，请先核实%s", sent + 1, rest ? "，" : "");
    if (rest) snprintf(left, sizeof left, "后 %d 条未发出", rest);
    snprintf(s, cap, "共 %d 条：%s%s%s", parts, done, unknown, left);
}

/* Both callers use the same validation; the AI can supply text, never a local draft reference. */
static int send_text(const ink_args_t *args, const char *member, ink_source_t src, ink_receipt_t *rc, char *to) {
    char page[48]; int64_t draft = 0;
    int to_len = ink_args_get_str(args, "to", to, 24), bytes;
    if (ink_args_find(args, "draft")) {
        if (ink_args_find(args, "text") || src == INK_SRC_AI || src == INK_SRC_REMOTE) {
            snprintf(rc->say, sizeof rc->say, "请直接提供短信正文，不能同时提供正文和草稿；AI 不能发送草稿引用"); return -1;
        }
        bytes = ink_args_get_int(args, "draft", &draft) && draft > 0 && draft <= UINT32_MAX &&
            ink_args_get_str(args, "page", page, sizeof page) > 0 ? ink_text_draft_read(member, page, (uint32_t)draft, CM.text, sizeof CM.text) : -1;
        if (bytes < 0) { snprintf(rc->say, sizeof rc->say, "短信草稿不可用，未发送，请重新打开后再试"); return -1; }
    } else bytes = ink_args_get_str(args, "text", CM.text, sizeof CM.text);
    bool valid = to_len > 0 && to_len < 24 && bytes > 0 && bytes < (int)sizeof CM.text;
    for (size_t i = 0; to[i]; i++) if ((to[i] < '0' || to[i] > '9') && !(i == 0 && to[i] == '+' && to[1])) valid = false;
    int parts = valid ? hal_cell_sms_parts(CM.text) : -1;
    if (parts > INK_SMS_PARTS_MAX) snprintf(rc->say, sizeof rc->say, "短信太长，最多分 %d 条发送，请删短一些", INK_SMS_PARTS_MAX);
    return parts > 0 && parts <= INK_SMS_PARTS_MAX ? bytes : -1;
}
static void text_digest(char out[65]) {
    uint8_t digest[32]; ink_sha256(CM.text, strlen(CM.text), digest);
    for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", digest[i]);
}
static ink_rc_t bind_send(ink_args_t *args, ink_receipt_t *rc, ink_source_t src);
static void cap_send(ink_call_t *c) { /* exec.bg */
    char to[24] = "", *text = CM.text, status[160];
    if (airplane(c, "发短信")) return;
    int bytes = send_text(&c->args, ink_cap_call_member(c), c->src, &c->rc, to);
    if (bytes < 0) { c->rc.status = INK_RC_INVALID; return; }
    bool draft = ink_args_find(&c->args, "draft") != NULL;
    if (draft && c->src == INK_SRC_CONFIRMED) {
        char expected[65], actual[65]; text_digest(actual);
        if (ink_args_get_str(&c->args, "_digest", expected, sizeof expected) != 64 || strcmp(expected, actual)) {
            c->rc.status = INK_RC_STALE; snprintf(c->rc.say, sizeof c->rc.say, "短信草稿已改变，未发送，请重新确认"); return;
        }
    }
    /* Preserve existing direct sends; a draft exceeding the old call capacity gets a billed-parts card. */
    if (draft && c->src != INK_SRC_CONFIRMED && (size_t)bytes + strlen(to) + 10 > INK_ARGS_TEXT) {
        ink_confirm_hold(c->cap, &c->args, c->src, c->page_gen, &c->rc); return;
    }
    int parts = hal_cell_sms_parts(text);
    ink_db_t *db = ink_db_open_member(&MESSAGES, ink_cap_call_member(c)); uint32_t id = 0;
    if (!db || !save_message(db, &id, to, text, "结果未知，请先核实", c->src)) {
        ink_db_close(db); c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "短信没能存下，未发送"); return;
    }
    osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    hal_cell_status_t st; bool known = hal_cell_status(&st);
    uint32_t left = ink_cap_time_left_ms(c); /* the time it waited for this executor and the modem counts */
    int sent = 0, r = (known && !st.registered) || left < SMS_MIN_LEFT_MS ? 1 : hal_cell_sms_send(to, text, left, &sent);
    osal_mutex_unlock(CM.at);
    send_status(status, sizeof status, r, parts, sent);
    if (r != 0) {
        c->rc.status = sent ? INK_RC_PARTIAL : INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", r > 0 ? "not sent: offline or no time left" : "send result unknown");
    } else ink_probe_count("comm.sms_sent");
    snprintf(c->rc.object, sizeof c->rc.object, "message:to:%s", to);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", status);
    if (!save_message(db, &id, to, text, status, c->src)) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "发送结果没能存下，请先核实"); }
    ink_db_close(db);
}

/* Before an AI's message waits for a yes: the card says how many billed parts it goes as, like the composer. */
static ink_rc_t bind_send(ink_args_t *args, ink_receipt_t *rc, ink_source_t src) {
    char to[24] = "", page[48], digest[65], preview[61], note[48] = ""; int64_t draft = 0;
    if (send_text(args, ink_member_current()->id, src, rc, to) < 0) return INK_RC_INVALID;
    int parts = hal_cell_sms_parts(CM.text); size_t chars = ink_text_char_offset(CM.text, strlen(CM.text)), end = ink_text_byte_offset(CM.text, 20);
    snprintf(preview, sizeof preview, "%.*s", (int)end, CM.text); utf8_clip(preview);
    if (ink_args_get_int(args, "draft", &draft)) {
        ink_args_get_str(args, "page", page, sizeof page); text_digest(digest); ink_args_init(args);
        if (ink_args_str(args, "to", to) || ink_args_str(args, "page", page) || ink_args_int(args, "draft", draft) || ink_args_str(args, "_digest", digest)) return INK_RC_FAILED;
    }
    if (parts > 1) snprintf(note, sizeof note, "（共 %u 字，将按 %d 条发送）", (unsigned)chars, parts);
    snprintf(rc->say, sizeof rc->say, "发短信给 %s：%.60s%s%s", to, preview, strlen(CM.text) > strlen(preview) ? "…" : "", note);
    return INK_RC_DONE;
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
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec; char *raw = CM.raw, peer[24], text[355];
    ink_json_t *j = &CM.json; int n = 0;
    while (ink_db_next(db, &cur, &rec, raw, sizeof CM.raw - 1) == 1) {
        raw[rec.len < sizeof CM.raw ? rec.len : sizeof CM.raw - 1] = 0;
        if (ink_json_parse(j, raw, rec.len) || !ink_json_eq(j, ink_json_find(j, 0, "status"), "收到")) continue;
        if (!n++) { ink_json_str(j, ink_json_find(j, 0, "peer"), peer, sizeof peer); ink_json_str(j, ink_json_find(j, 0, "text"), text, sizeof text); }
    }
    if (!n)
        snprintf(c->rc.say, sizeof c->rc.say, "没有新短信");
    else if (snprintf(c->rc.say, sizeof c->rc.say, "%d 条短信，最近一条来自 %s：%s", n, peer, text) >= (int)sizeof c->rc.say)
        whole_characters(c->rc.say); /* a long message is cut to what can be said */
    ink_db_close(db);
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
    if (airplane(c, "打电话")) return; /* no exception for emergency numbers yet: not decided */
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
    {.id = "call.dtmf", .title = "发送一个通话按键", .ask = "向 {_peer} 的本次通话发送按键 {key}", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":1}},\"required\":[\"key\"]}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "call", .timeout_ms = 10000, .exec = INK_EXEC_BG, .prepare_confirm = ink_comm_bind_dtmf, .run = cap_control},
    {.id = "call.route", .title = "切换通话耳机或喇叭", .input = BOOL_ARG("headset"), .effect = INK_FX_NAVIGATE, .child_ok = true, .permission = "call", .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.record", .title = "查看通话录音不可用的原因", .effect = INK_FX_READ, .child_ok = true, .permission = "call", .exec = INK_EXEC_BG, .run = cap_control},
    {.id = "call.controls", .title = "通话控制已获模组确认", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true, .permission = "call"},
    {.id = "message.send",
     .title = "发短信",
     .ask = "发短信给 {to}：{text}",
     .prepare_confirm = bind_send,
     .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":765}},"
              "\"required\":[\"to\"]}",
     .effect = INK_FX_EXTERNAL,
     .child_ok = true, /* children like adults (D-25); the AI still waits for confirmation */
     .permission = "message",
     .qos = INK_QOS_NETWORK,
     .timeout_ms = 30000,
     .exec = INK_EXEC_BG,
     .run = cap_send},
    {.id = "message.list", .title = "最近的短信", .effect = INK_FX_READ, .child_ok = true, .permission = "message", .exec = INK_EXEC_BG, .run = cap_list},
    {.id = "sms.send", .title = "保存并发送短信", .ask = "发短信给 {to}：{text}", .prepare_confirm = bind_send, .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":765}},\"required\":[\"to\"]}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "message", .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .timeout_ms = 30000, .run = cap_send},
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

static bool mobile_save(uint32_t id, const void *data, size_t len) { /* exec.bg; private records never exported */
    ink_db_t *db = ink_db_open(&MOBILE, NULL); uint32_t ver = 0;
    ink_db_write_t w = {.id = id, .data = data, .len = len, .written_ver = &ver};
    bool ok = db && !ink_db_put(db, &w, NULL) && !ink_db_wait(db, ver, 3000000);
    ink_db_close(db); return ok;
}
static void mobile_apply(void *arg) {
    (void)arg; osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER);
    if (hal_cell_apn(&CM.apn)) ink_probe_count("net.apn_not_applied");
    osal_mutex_unlock(CM.at);
}
static void mobile_sample(void *ctx, int h, const char *id) { /* existing minute event; one checkpoint per local day */
    (void)ctx; (void)h; (void)id;
    uint64_t rx = 0, tx = 0; ink_tm_t t;
    if (!ink_time_valid() || !hal_net_lte_bytes(&rx, &tx)) return;
    ink_time_to_local(ink_time_now_s(), &t);
    int month = t.year * 12 + t.month, day = t.year * 10000 + t.month * 100 + t.day;
    bool changed = CM.usage.month != month || rx + tx != CM.sampled_bytes;
    if (CM.usage.month != month) {
        CM.usage.previous = CM.usage.month == month - 1 ? CM.usage.current : 0;
        CM.usage.current = 0; CM.usage.month = month;
    }
    CM.usage.current += rx + tx - CM.sampled_bytes; CM.sampled_bytes = rx + tx;
    if (CM.usage.day != day && (CM.usage.current || CM.usage.previous)) {
        int before = CM.usage.day; CM.usage.day = day;
        if (!mobile_save(2, &CM.usage, sizeof CM.usage)) { CM.usage.day = before; ink_probe_count("net.usage_not_saved"); }
    }
    if (changed) ink_state_set_int(CM.h_mobile, ++CM.mobile_rev);
}
static void mobile_cap(ink_call_t *c) { /* exec.bg */
    const char *id = c->cap->id;
    if (!strcmp(id, "net.apn_set")) {
        hal_cell_apn_t a = CM.apn; bool automatic = true;
        ink_args_get_bool(&c->args, "automatic", &automatic);
        bool password = ink_args_find(&c->args, "password") != NULL;
        if (password && c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY) { c->rc.status = INK_RC_REJECTED; return; }
        if (automatic) memset(&a, 0, sizeof a);
        else {
            ink_args_get_str(&c->args, "name", a.name, sizeof a.name); ink_args_get_str(&c->args, "user", a.user, sizeof a.user);
            if (password) ink_args_get_str(&c->args, "password", a.password, sizeof a.password);
            if (!a.name[0]) { c->rc.status = INK_RC_INVALID; return; }
        }
        osal_mutex_lock(CM.at, OSAL_WAIT_FOREVER); int r = hal_cell_apn(&a); osal_mutex_unlock(CM.at);
        if (r || !mobile_save(1, &a, sizeof a)) c->rc.status = INK_RC_FAILED;
        else CM.apn = a;
        memset(&a, 0, sizeof a);
    } else if (!strcmp(id, "net.roaming_set")) {
        bool on = false; ink_args_get_bool(&c->args, "enabled", &on); ink_setting_set("net.roaming", on, c->src, &c->rc);
    } else {
        mobile_sample(NULL, 0, NULL);
        if (!strcmp(id, "net.usage_reset")) {
            CM.usage.current = CM.usage.previous = 0;
            if (!mobile_save(2, &CM.usage, sizeof CM.usage)) c->rc.status = INK_RC_FAILED;
        } else {
            char out[512]; ink_jw_t w; ink_jw_init(&w, out, sizeof out);
            uint64_t rx, tx;
            ink_jw_raw(&w, hal_net_lte_bytes(&rx, &tx) && ink_time_valid() ? "{\"available\":true,\"current\":" : "{\"available\":false,\"current\":"); ink_jw_int(&w, (int64_t)CM.usage.current);
            ink_jw_raw(&w, ",\"previous\":"); ink_jw_int(&w, (int64_t)CM.usage.previous);
            ink_jw_raw(&w, ",\"name\":"); ink_jw_str(&w, CM.apn.name); ink_jw_raw(&w, ",\"user\":"); ink_jw_str(&w, CM.apn.user);
            ink_jw_raw(&w, CM.apn.password[0] ? ",\"password\":true}" : ",\"password\":false}"); ink_cap_result(c, out);
        }
    }
    if (strcmp(id, "net.usage") && c->rc.status == INK_RC_DONE) ink_state_set_int(CM.h_mobile, ++CM.mobile_rev);
}
#define MOBILE_STR "{\"type\":\"string\",\"maxLength\":64}"
static const ink_cap_t MOBILE_CAPS[] = {
    {.id = "net.apn_set", .title = "设置接入点并重连移动数据", .effect = INK_FX_SYSTEM, .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = mobile_cap,
     .input = "{\"type\":\"object\",\"properties\":{\"automatic\":{\"type\":\"boolean\"},\"name\":{\"type\":\"string\",\"maxLength\":100},\"user\":" MOBILE_STR ",\"password\":" MOBILE_STR "},\"required\":[\"automatic\"]}"},
    {.id = "net.roaming_set", .title = "开关数据漫游", .effect = INK_FX_SYSTEM, .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = mobile_cap,
     .input = "{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"enabled\"]}"},
    {.id = "net.usage", .title = "移动流量和接入点（不含密码）", .effect = INK_FX_READ, .child_ok = true, .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = mobile_cap},
    {.id = "net.usage_reset", .title = "清零流量统计", .effect = INK_FX_SYSTEM, .ask_by_hand = true, .ask = "清零本月和上个月的流量统计，不能撤销", .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = mobile_cap}};

int ink_comm_init(void) {
    static const ink_state_def_t mobile = {.id = "net.mobile_rev", .type = INK_ST_INT, .title = "移动网络设置变化"};
    CM.h_mobile = ink_state_define(&mobile); ink_state_set_int(CM.h_mobile, 0);
    if (ink_db_register(&MOBILE)) return -1;
    ink_db_t *db = ink_db_open(&MOBILE, NULL);
    bool saved = db && !ink_db_get(db, 1, NULL, &CM.apn, sizeof CM.apn);
    if (db) ink_db_get(db, 2, NULL, &CM.usage, sizeof CM.usage);
    ink_db_close(db);
    if (ink_cap_register(MOBILE_CAPS, sizeof MOBILE_CAPS / sizeof *MOBILE_CAPS) || ink_state_watch("time.minute", mobile_sample, NULL, INK_EXEC_BG)) return -1;
    if (ink_db_register(&MESSAGES) || ink_db_register(&PARTS) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG) ||
        ink_cap_subscribe("comm.sms_part", part_arrived, NULL, INK_EXEC_BG)) return -1;
    CM.lock = osal_mutex_create();
    CM.at = osal_mutex_create();
    if (!CM.lock || !CM.at) return -1;
    if (saved) ink_exec_post(INK_EXEC_BG, &CM.apn_job, mobile_apply, NULL);
    CM.h_registered = ink_state_define(&ST_REGISTERED);
    static const ink_state_def_t roaming = {.id = "cell.roaming", .type = INK_ST_BOOL, .title = "漫游注册"};
    CM.h_roaming = ink_state_define(&roaming);
    CM.h_sim = ink_state_define(&ST_SIM);
    ink_settings_define(COMM_SETTINGS, 1);
    if (ink_settings_watch("net.airplane", airplane_changed, NULL, INK_EXEC_BG) != 0) return -1;
    if (ink_cap_register(COMM_CAPS, (int)(sizeof COMM_CAPS / sizeof COMM_CAPS[0])) != 0) return -1;
    osal_task_cfg_t cfg = {.name = "comm", .fn = comm_task, .stack_bytes = COMM_STACK, .priority = COMM_PRIO, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    return osal_task_create(&cfg, NULL) == OSAL_OK ? 0 : -1;
}
