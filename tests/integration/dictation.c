/*
 * Dictation (system services design §9.5), on the whole product with the hub's stand-in for asr.stream (simulator/environment/
 * sim_hub.c). INK_DICTATION_PHASE picks the run: main, or handsfree on the profile with the voice front end (the wake word
 * means nothing while someone dictates). Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/hub.h"
#include "ink/sys/media.h"
#include "ink/sys/rt.h"
#include "ink/sys/textfield.h"
#include "ink/sys/textinput.h"
#include "ink/ui_shell.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

/* ---- a session, on the UI task ---- */

static ink_text_cfg_t g_cfg;
static int g_sess;
static void do_focus(void) { g_sess = ink_text_focus(&g_cfg, NULL, NULL); }
static void do_blur(void) { ink_text_blur(g_sess); }
static void focus_as(ink_text_type_t type, const char *initial) {
    g_cfg = (ink_text_cfg_t){.type = type, .initial = initial, .hint = "写点什么"};
    on_ui(do_focus);
}
static char g_value[1024], g_notice[96], g_heard[256];
static ink_voice_state_t g_state;
static void do_look(void) {
    snprintf(g_value, sizeof g_value, "%s", ink_text_value());
    snprintf(g_notice, sizeof g_notice, "%s", ink_text_voice_notice());
    snprintf(g_heard, sizeof g_heard, "%s", ink_text_voice_heard());
    g_state = ink_text_voice_state();
}
static void look(void) { on_ui(do_look); }
static int g_start_r;
static void do_start(void) { g_start_r = ink_text_voice_start(); }
static void do_backspace(void) { ink_text_backspace(); }

/* ---- the keyboard, by finger ---- */

static int g_found_x, g_found_y;
static const char *g_find;
static bool g_found;
static bool shown(lv_obj_t *o) {
    for (; o; o = lv_obj_get_parent(o))
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
    return true;
}
static void find_in(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !g_found; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        const char *action = ink_shell_bound_cap(c);
        if (!strcmp(g_find, "话筒") && action && !strcmp(action, "text.voice_start") && shown(c)) {
            lv_area_t a;
            lv_obj_update_layout(c);
            lv_obj_get_coords(c, &a);
            g_found_x = (a.x1 + a.x2) / 2;
            g_found_y = (a.y1 + a.y2) / 2;
            g_found = true;
            return;
        }
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), g_find) && shown(c)) {
            lv_area_t a;
            lv_obj_update_layout(c);
            lv_obj_get_coords(lv_obj_get_parent(c), &a);
            g_found_x = (a.x1 + a.x2) / 2;
            g_found_y = (a.y1 + a.y2) / 2;
            g_found = true;
            return;
        }
        find_in(c);
    }
}
static void do_find(void) {
    g_found = false;
    find_in(lv_layer_sys());
}
static bool there(const char *label) {
    g_find = label;
    on_ui(do_find);
    return g_found;
}
static bool press(const char *label) {
    if (!there(label)) return false;
    tap(g_found_x, g_found_y);
    return true;
}
/* A label on the keyboard that says this, somewhere. */
static const char *g_saying;
static bool g_says;
static void says_in(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !g_says; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && strstr(lv_label_get_text(c), g_saying) && shown(c)) {
            g_says = true;
            return;
        }
        says_in(c);
    }
}
static void do_says(void) {
    g_says = false;
    says_in(lv_layer_sys());
}
static bool keyboard_says(const char *text) {
    g_saying = text;
    on_ui(do_says);
    return g_says;
}

/* ---- the world ---- */

static void mic_event(void *arg) { sim_audio_mic_source(arg); }
static void mic(const char *what) {
    vtk_event_at(vtk_now() + SIM_MS(1), mic_event, (void *)what);
    delay_ms(5);
}
extern void sim_net_set_link(const char *);
static void net_none(void *arg) {
    (void)arg;
    sim_net_set_link("none");
}
static void net_wifi(void *arg) {
    (void)arg;
    sim_net_set_link("wifi");
}
static void net_down(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    CHECK(WAIT_FOR(!ink_hub_online(), 40000), "the network goes");
}
static void net_up(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), net_wifi, NULL);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 60000), "the network is back");
}
static bool state_is(ink_voice_state_t s) {
    look();
    return g_state == s;
}
static bool over_and_free(void) { return ink_rt_open_count() == 0 && !ink_audio_capture_busy() && ink_audio_focus_top() == 0; }

/* Tapped, talking, and heard for a while. */
static void start_talking(const char *phrases) {
    sim_hub_asr_script(phrases, NULL, 0);
    CHECK(press("话筒"), "the microphone key is there");
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "listening (%d)", g_state);
    mic("speech");
}

/* The keyboard's height: the panel takes the place of the keys, nothing above it moves. */
static int g_kb_h;
static void do_kb_height(void) {
    g_kb_h = 0;
    lv_obj_t *top = lv_layer_sys();
    for (uint32_t i = 0; i < lv_obj_get_child_count(top); i++) {
        lv_obj_t *c = lv_obj_get_child(top, (int32_t)i);
        lv_obj_update_layout(c);
        if (lv_obj_get_height(c) > g_kb_h) g_kb_h = lv_obj_get_height(c);
    }
}
static int kb_height(void) {
    on_ui(do_kb_height);
    return g_kb_h;
}

static volatile int g_ended;
static char g_ended_how[16], g_ended_why[64];
static void on_ended(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    g_ended_how[0] = g_ended_why[0] = 0;
    ink_args_get_str(a, "how", g_ended_how, sizeof g_ended_how);
    ink_args_get_str(a, "why", g_ended_why, sizeof g_ended_why);
    g_ended++;
}

/* ---- the plain case ---- */

static void happy(void) {
    CHECK(ink_cap_subscribe("text.voice_ended", on_ended, NULL, INK_EXEC_DIRECT) == 0, "listening for the end");
    focus_as(INK_TEXT_PLAIN, "备注：");
    delay_ms(300);
    double opened = sim_metric_value("hub.asr_opened");
    int height = kb_height();
    start_talking("今晚七点到家。|给我留饭。");
    CHECK(kb_height() == height, "the panel is the size of the keys (%d, %d)", kb_height(), height);
    CHECK(sim_metric_value("hub.asr_opened") == opened + 1, "one session");
    CHECK(strstr(sim_hub_asr_params(), "\"kind\":\"text\"") && strstr(sim_hub_asr_params(), "\"format\":\"opus\""), "as what (%s)", sim_hub_asr_params());
    CHECK(!strcmp(ink_state_get_str(ink_state_find("text.voice"), g_value, sizeof g_value) >= 0 ? g_value : "", "listening"), "state says so (%s)", g_value);
    /* the panel where the keys were */
    CHECK(there("说完了") && there("用键盘") && !there("q") && !there("话筒"), "the panel instead of the keys");
    {
        char mic_now[16] = "";
        ink_state_get_str(ink_state_find("audio.mic"), mic_now, sizeof mic_now);
        CHECK(!strcmp(mic_now, "sending"), "the microphone says it goes out (%s)", mic_now);
    }
    /* what is heard shows in the panel and not in the field */
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "the first words come");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "今晚七点到家。") != NULL), 3000), "the first sentence (%s)", g_heard);
    CHECK(keyboard_says("今晚七点到家。"), "on the panel");
    CHECK(!strcmp(g_value, "备注：") && !strstr(ink_text_value(), "今晚"), "the field has not changed (%s)", g_value);
    CHECK(WAIT_FOR((look(), strstr(g_heard, "给我") != NULL), 3000), "a part of the next (%s)", g_heard);
    CHECK(sim_metric_value("hub.asr_frames") > 20, "frames go (%.0f)", sim_metric_value("hub.asr_frames"));
    /* the finger says it is over */
    double ends = sim_metric_value("hub.asr_turn_end");
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over (%d)", g_state);
    CHECK(sim_metric_value("hub.asr_turn_end") == ends + 1, "the hub was told");
    CHECK(g_ended == 1 && !strcmp(g_ended_how, "done"), "told as an event (%d, %s)", g_ended, g_ended_how);
    look();
    CHECK(!strcmp(g_value, "备注：今晚七点到家。给我留饭。"), "all of it in the field (%s)", g_value);
    CHECK(WAIT_FOR(over_and_free(), 1000), "session, microphone and sound are free");
    CHECK(there("q") && there("话筒") && !there("说完了"), "the keys are back");
    /* and it can be edited like anything typed */
    on_ui(do_backspace);
    look();
    CHECK(!strcmp(g_value, "备注：今晚七点到家。给我留饭"), "a character back (%s)", g_value);
    on_ui(do_blur);
    delay_ms(100);
}

/* A pause is the end; nothing said is said so. */
static void pauses(void) {
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("明天见。");
    delay_ms(1500);
    double ends = sim_metric_value("hub.asr_turn_end");
    mic("silence");
    delay_ms(800);
    look();
    CHECK(g_state == INK_VOICE_LISTENING, "a short pause goes on");
    mic("speech");
    delay_ms(1500);
    CHECK(sim_metric_value("hub.asr_turn_end") == ends && state_is(INK_VOICE_LISTENING), "talking again");
    mic("silence");
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_turn_end") == ends + 1, 3000), "a long one tells the hub by itself");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 2000), "over");
    look();
    CHECK(!strcmp(g_value, "明天见。"), "in the field (%s)", g_value);
    CHECK(WAIT_FOR(over_and_free(), 1000), "free");

    /* nothing at all */
    sim_hub_asr_script("", NULL, 0);
    mic("silence");
    CHECK(press("话筒"), "again");
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "listening");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 12000), "it does not wait for ever");
    look();
    CHECK(!strcmp(g_value, "明天见。") && strstr(g_notice, "没听清"), "and says so (%s, %s)", g_value, g_notice);
    CHECK(keyboard_says("没听清"), "where it can be read");
    press("q");
    look();
    CHECK(!g_notice[0], "until the next key");
    on_ui(do_blur);
    delay_ms(100);
    mic("speech");
}

/* The person takes it back. */
static void cancelled(void) {
    focus_as(INK_TEXT_PLAIN, "原来的");
    delay_ms(300);
    double was = sim_metric_value("hub.asr_cancelled");
    start_talking("不要了。");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "不要了。") != NULL), 3000), "heard (%s)", g_heard);
    CHECK(press("用键盘"), "back to the keys");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 500), "at once");
    CHECK(!strcmp(g_value, "原来的"), "nothing goes in (%s)", g_value);
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_cancelled") == was + 1, 1000), "the hub hears it was given up");
    CHECK(WAIT_FOR(over_and_free(), 1000), "free");
    CHECK(there("q"), "the keys");
    on_ui(do_blur);
    delay_ms(100);
}

/* ---- what goes wrong ---- */

static void hub_says_no(const char *code, const char *told) {
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    sim_hub_asr_script("先说的这句。|后面的", code, 30);
    CHECK(press("话筒"), "%s: the key", code);
    mic("speech");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 6000), "%s: it ends", code);
    look();
    CHECK(!strncmp(g_value, "先说的这句。", strlen("先说的这句。")) && strlen(g_value) > strlen("先说的这句。") && strstr("先说的这句。后面的", g_value), "%s: what was said stays, half a sentence too (%s)", code, g_value);
    CHECK(strstr(g_notice, told), "%s: says why (%s)", code, g_notice);
    CHECK(WAIT_FOR(over_and_free(), 1000), "%s: free", code);
    CHECK(there("q"), "%s: the keys", code);
    on_ui(do_blur);
    delay_ms(100);
}

/* A page that takes its time: the UI task does not get to look at anything. */
static void ui_busy_for_a_while(void) {
    delay_ms(100);
    for (int i = 0; i < 1200 && ink_hub_online(); i++) delay_ms(10);
    for (int i = 0; i < 6000 && !ink_hub_online(); i++) delay_ms(10);
    delay_ms(1500); /* the new connection has confirmed the session again by now */
}

static void offline(void) {
    /* no network: said, and back to the keys */
    focus_as(INK_TEXT_PLAIN, "留着");
    delay_ms(300);
    net_down();
    double opened = sim_metric_value("hub.asr_opened"), gone = sim_metric_value("hub.asr_cancelled");
    CHECK(there("话筒"), "the key is still there: tapping it says why");
    CHECK(press("话筒"), "tapped");
    mic("speech");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 9000), "it gives up");
    look();
    CHECK(strstr(g_notice, "连不上") && !strcmp(g_value, "留着"), "says so (%s, %s)", g_notice, g_value);
    CHECK(WAIT_FOR(over_and_free(), 1000), "free");
    CHECK(!strcmp(g_ended_how, "failed") && strstr(g_ended_why, "连不上"), "told as an event (%s, %s)", g_ended_how, g_ended_why);
    double frames = sim_metric_value("hub.asr_frames");
    net_up();
    delay_ms(1000);
    /* the request that was waiting for the network may still go out: it is taken back at once, and nothing is heard */
    CHECK(ink_rt_open_count() == 0 && sim_metric_value("hub.asr_frames") == frames, "and does not start by itself when the network is back");
    CHECK(sim_metric_value("hub.asr_opened") - opened == sim_metric_value("hub.asr_cancelled") - gone, "nothing stays open at the hub (%.0f, %.0f)", sim_metric_value("hub.asr_opened") - opened, sim_metric_value("hub.asr_cancelled") - gone);
    on_ui(do_blur);
    delay_ms(100);

    /* it goes in the middle: not carried over to the next connection */
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("断之前的。|断之后的。");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "断之前的。") != NULL), 3000), "the first sentence");
    opened = sim_metric_value("hub.asr_opened");
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 40000), "the link goes, and it ends (%d)", g_state);
    look();
    CHECK(!strcmp(g_value, "断之前的。") && strstr(g_notice, "连不上"), "what was final stays (%s, %s)", g_value, g_notice);
    CHECK(WAIT_FOR(over_and_free(), 1000), "free");
    net_up();
    delay_ms(2000);
    CHECK(sim_metric_value("hub.asr_opened") == opened, "the next connection knows nothing of it (%.0f)", sim_metric_value("hub.asr_opened") - opened);
    on_ui(do_blur);
    delay_ms(100);

    /* the link goes and is back before anyone here has looked: a new connection is not this session either */
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("一闪而过。");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "一闪而过。") != NULL), 3000), "heard");
    vtk_event_at(vtk_now() + SIM_MS(10), net_none, NULL);
    vtk_event_at(vtk_now() + SIM_MS(1000), net_wifi, NULL);
    on_ui(ui_busy_for_a_while);
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 2000), "over (%d)", g_state);
    look();
    CHECK(!strcmp(g_value, "一闪而过。") && strstr(g_notice, "连不上"), "as if it had looked (%s, %s)", g_value, g_notice);
    CHECK(WAIT_FOR(over_and_free(), 1000), "free");
    on_ui(do_blur);
    delay_ms(100);

    /* what is said while it connects is not lost */
    net_down();
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    sim_hub_asr_script("早说的也算。", NULL, 0);
    double before = sim_metric_value("hub.asr_frames");
    uint64_t tapped = osal_now_us();
    CHECK(press("话筒"), "tapped");
    mic("speech");
    delay_ms(500);
    CHECK(state_is(INK_VOICE_CONNECTING), "connecting (%d)", g_state);
    CHECK(keyboard_says("正在连接"), "and it says so");
    net_up();
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "there (%d)", g_state);
    delay_ms(1000);
    double should = (double)(osal_now_us() - tapped) / 60000.0;
    CHECK(sim_metric_value("hub.asr_frames") - before >= should * 0.8, "all of it went, the first second too (%.0f of %.0f)", sim_metric_value("hub.asr_frames") - before, should);
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    on_ui(do_blur);
    delay_ms(100);

    /* a hub that hears and says nothing */
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    sim_hub_asr_script("", "silent", 0);
    CHECK(press("话筒"), "the key");
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "listening");
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 10000), "it does not wait for ever");
    look();
    CHECK(strstr(g_notice, "用不了"), "says so (%s)", g_notice);
    on_ui(do_blur);
    delay_ms(100);
    sim_hub_asr_script("", NULL, 0);

    /* the hub refuses the session */
    sim_hub_asr_refuse(true);
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    CHECK(press("话筒"), "the key");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "refused");
    look();
    CHECK(strstr(g_notice, "用不了"), "says so (%s)", g_notice);
    sim_hub_asr_refuse(false);
    on_ui(do_blur);
    delay_ms(100);
}

/* ---- which fields ---- */

static void fields(void) {
    /* a password: no microphone, whoever asks */
    focus_as(INK_TEXT_PASSWORD, "");
    delay_ms(300);
    CHECK(there("q") && !there("话筒"), "a password has no microphone key");
    on_ui(do_start);
    CHECK(g_start_r != 0 && state_is(INK_VOICE_IDLE), "and cannot be made to");
    CHECK(BY_HAND("text.voice_start", NULL) && g_rc.status == INK_RC_REJECTED, "not by the capability (%s)", ink_rc_name(g_rc.status));
    on_ui(do_blur);
    delay_ms(100);

    /* a number field asks for digits and takes digits */
    focus_as(INK_TEXT_PHONE, "");
    delay_ms(300);
    start_talking("138 0013 8000，好。");
    CHECK(strstr(sim_hub_asr_params(), "\"kind\":\"number\""), "asks for a number (%s)", sim_hub_asr_params());
    delay_ms(1500);
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    CHECK(!strcmp(g_value, "13800138000"), "digits (%s)", g_value);
    on_ui(do_blur);
    delay_ms(100);

    /* more than the field holds: whole characters */
    g_cfg = (ink_text_cfg_t){.type = INK_TEXT_PLAIN, .max = 10};
    on_ui(do_focus);
    delay_ms(300);
    start_talking("一二三四五六七八九十。");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "九十") != NULL), 4000), "heard (%s)", g_heard);
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    CHECK(!strcmp(g_value, "一二三"), "as much as fits (%s)", g_value);
    on_ui(do_blur);
    delay_ms(100);

    /* this member is not offered it: no key */
    sim_hub_asr_hide(true);
    net_down();
    net_up();
    delay_ms(1000);
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    CHECK(!there("话筒"), "the hub does not offer it to this member: no key");
    on_ui(do_blur);
    sim_hub_asr_hide(false);
    net_down();
    net_up();
    delay_ms(1000);
}

/* ---- what ends it, and what it does to the rest ---- */

static lv_obj_t *g_field;
static void field_entered(void *ctx, const char *value) {
    (void)ctx;
    (void)value;
}
static void field_page(lv_obj_t *scr, void *arg) {
    (void)arg;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .hint = "搜索"};
    g_field = ink_textfield_create(scr, &cfg, field_entered, NULL, true);
    lv_obj_align(g_field, LV_ALIGN_TOP_MID, 0, ink_dp(40));
}
static const ink_activity_t FIELD_PAGE = {.name = "fieldpage", .create = field_page};
static void open_field_page(void) { ink_ui_push(&FIELD_PAGE, NULL); }
static void close_field_page(void) { ink_ui_pop(); }

static void story_focus(void *ctx, ink_focus_event_t ev, ink_audio_prio_t by) {
    (void)by;
    *(volatile int *)ctx = ev == INK_FOCUS_BACK ? 2 : 1;
}

static void ended_by(void) {
    double cancelled;

    /* leaving the page */
    on_ui(open_field_page);
    delay_ms(600);
    cancelled = sim_metric_value("hub.asr_cancelled");
    start_talking("这句丢掉。");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "这句丢掉。") != NULL), 3000), "heard");
    on_ui(close_field_page);
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 500), "the page goes: over");
    CHECK(!ink_text_focused() && WAIT_FOR(over_and_free(), 1000), "with the session");
    CHECK(WAIT_FOR(sim_metric_value("hub.asr_cancelled") == cancelled + 1, 1000), "and the hub hears it");
    delay_ms(300);

    /* locking */
    focus_as(INK_TEXT_PLAIN, "锁之前");
    delay_ms(300);
    start_talking("这句也丢掉。");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "heard");
    ink_power_lock();
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 2000), "locked: over");
    look();
    CHECK(!strcmp(g_value, "锁之前"), "nothing goes in (%s)", g_value);
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked");
    ink_power_activity(osal_now_us());
    delay_ms(300);

    /* a call (system UI spec §8): the microphone goes at once, what was said is not lost, and it can go on afterwards */
    start_talking("来电话了。");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "heard");
    g_hw.num[0] = 0;
    snprintf(g_hw.num, sizeof g_hw.num, "10086");
    int ended = g_ended;
    hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 5000), "it rings (%s)", active_state());
    CHECK(!ink_audio_capture_busy() || WAIT_FOR(!ink_audio_capture_busy(), 500), "the microphone is free for it");
    double frames = sim_metric_value("hub.asr_frames");
    delay_ms(1000);
    CHECK(sim_metric_value("hub.asr_frames") == frames, "and nothing of the call goes to the hub (%.0f)", sim_metric_value("hub.asr_frames") - frames);
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "a call: over");
    CHECK(WAIT_FOR(g_ended > ended, 500) && !strcmp(g_ended_how, "interrupted"), "and says how (%s)", g_ended_how);
    look();
    CHECK(strstr(g_value, "锁之前") && strstr(g_value, "来电话了。"), "what was said goes in (%s)", g_value);
    CHECK(strstr(g_notice, "接着说"), "and it can go on (%s)", g_notice);
    hw(HW_HANGUP, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 5000), "it hangs up");
    delay_ms(1000);
    CHECK(ink_text_focused() && there("话筒"), "back in the field, the key is there");
    start_talking("接着说的。");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "heard");
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    look();
    CHECK(strstr(g_value, "来电话了。接着说的。"), "it goes on from there (%s)", g_value);
    delay_ms(300);

    /* half a sentence, and the hub does not answer any more: what was heard is what there is */
    sim_hub_asr_script("半句话说到这里", "mute", 12);
    CHECK(press("话筒"), "the microphone key is there");
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "listening (%d)", g_state);
    mic("speech");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "heard (%s)", g_heard);
    char half[64];
    snprintf(half, sizeof half, "%s", g_heard);
    int by_call = ink_audio_focus_acquire(INK_AUDIO_PRIO_CALL, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(by_call > 0 && WAIT_FOR(state_is(INK_VOICE_IDLE), 10000), "over, after a while");
    look();
    CHECK(strstr(g_value, half), "half a sentence is not lost (%s, %s)", g_value, half);
    CHECK(strstr(g_notice, "接着说"), "and it is not told about the hub, only that it can go on (%s)", g_notice);
    ink_audio_focus_release(by_call);
    delay_ms(300);

    /* an alarm's level */
    start_talking("闹钟响了。");
    int alert = ink_audio_focus_acquire(INK_AUDIO_PRIO_ALERT, NULL, NULL, INK_EXEC_DIRECT);
    CHECK(alert > 0 && WAIT_FOR(!ink_audio_capture_busy(), 500), "an alarm: the microphone is free");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    look();
    CHECK(strstr(g_value, "闹钟响了"), "what was said goes in (%s)", g_value);
    /* under it, or under a call, it does not start */
    on_ui(do_start);
    look();
    CHECK(g_start_r != 0 && strstr(g_notice, "不能用"), "and does not start under one (%s)", g_notice);
    ink_audio_focus_release(alert);
    delay_ms(300);

    /* someone else takes the device */
    start_talking("别人的话。");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "heard");
    CHECK(ink_member_switch("kid") == 0, "the child");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 1000), "over");
    CHECK(!ink_text_focused(), "with the session");
    /* a child can use it: it is typing, not the AI (hub design §5.3.1) */
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("小孩说的。");
    delay_ms(1500);
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    CHECK(!strcmp(g_value, "小孩说的。"), "a child's words (%s)", g_value);
    on_ui(do_blur);
    ink_member_test_reset();
    delay_ms(500);

    /* something being read out waits for it, then goes on */
    volatile int story = 0;
    int t = ink_audio_focus_acquire(INK_AUDIO_PRIO_STORY, story_focus, (void *)&story, INK_EXEC_DIRECT);
    CHECK(t > 0, "a story");
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("短的。");
    CHECK(WAIT_FOR(story == 1, 1000), "it waits while someone talks");
    CHECK(press("用键盘"), "back");
    CHECK(WAIT_FOR(story == 2, 2000), "then it goes on");
    ink_audio_focus_release(t);

    /* the microphone is one */
    ink_fs_remove("/me/note.opus");
    ink_fs_remove("/me/note2.opus");
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/note.opus\"}") && g_rc.status == INK_RC_DONE, "recording (%s)", g_rc.detail);
    delay_ms(300);
    CHECK(press("话筒"), "the key");
    delay_ms(300);
    look();
    CHECK(g_state == INK_VOICE_IDLE && strstr(g_notice, "录音"), "not while recording (%s)", g_notice);
    CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "stopped");
    delay_ms(500);
    start_talking("再说一遍。");
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/note2.opus\"}") && g_rc.status == INK_RC_FAILED, "nor recording while someone dictates (%s)", ink_rc_name(g_rc.status));
    CHECK(state_is(INK_VOICE_LISTENING), "and it goes on");
    CHECK(press("用键盘"), "back");
    delay_ms(300);

    /* the AI key still means the AI */
    start_talking("说到一半。");
    double presses = sim_metric_value("ai.presses"), no_mic = sim_metric_value("ai.mic_unavailable");
    key(HAL_KEY_AI);
    CHECK(sim_metric_value("ai.presses") == presses + 1 && sim_metric_value("ai.mic_unavailable") == no_mic, "and the AI has the microphone (%.0f, %.0f)", sim_metric_value("ai.presses") - presses, sim_metric_value("ai.mic_unavailable") - no_mic);
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "pressed: it ends");
    look();
    CHECK(strstr(g_value, "说到一半。"), "what was said goes into the field (%s)", g_value);
    delay_ms(12000);
    on_ui(do_blur);
    delay_ms(300);
}

/* ---- what the AI can do ---- */

static void the_ai(void) {
    CHECK(AI("text.voice_start", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "no field yet, it asks all the same (%s)", ink_rc_name(g_rc.status));
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "no");
    delay_ms(1500);
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"voice\":\"idle\"") && strstr(g_result, "\"voice_possible\":true"), "it sees (%s)", g_result);
    sim_hub_asr_script("人说的。", NULL, 0);
    CHECK(AI("text.voice_start", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "opening a microphone takes a yes");
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "yes");
    CHECK(WAIT_FOR(state_is(INK_VOICE_LISTENING), 5000), "listening");
    mic("speech");
    CHECK(WAIT_FOR((AI("text.state", NULL), strstr(g_result, "\"voice\":\"listening\"") && strstr(g_result, "人说的")), 3000), "it sees what is heard (%s)", g_result);
    delay_ms(1500);
    CHECK(AI("text.voice_finish", NULL) && g_rc.status == INK_RC_DONE, "and can say it is over (%s)", ink_rc_name(g_rc.status));
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    CHECK(!strcmp(g_value, "人说的。"), "in the field (%s)", g_value);
    CHECK(AI("text.voice_cancel", NULL) && g_rc.status == INK_RC_FAILED, "nothing to cancel");
    on_ui(do_blur);
    delay_ms(1200);
    /* a field that says the AI stays out */
    g_cfg = (ink_text_cfg_t){.type = INK_TEXT_PLAIN, .ai_hidden = true};
    on_ui(do_focus);
    delay_ms(300);
    CHECK(AI("text.voice_start", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "asked");
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "yes");
    delay_ms(300);
    look();
    CHECK(g_state == INK_VOICE_IDLE, "but this field is the person's");
    start_talking("别让它看见。");
    CHECK(WAIT_FOR((look(), g_heard[0] != 0), 3000), "the person can");
    CHECK(AI("text.state", NULL) && !strstr(g_result, "别让") && !strstr(g_result, "\"heard\""), "the AI does not see it (%s)", g_result);
    CHECK(press("用键盘"), "back");
    on_ui(do_blur);
    delay_ms(1200);
}

/* ---- the wake word, on the profile that has one ---- */

static void handsfree(void) {
    CHECK(WAIT_FOR(({ int64_t v = 0; ink_state_get_int(ink_state_find("audio.listening"), &v); v != 0; }), 2000), "listening for the wake word");
    focus_as(INK_TEXT_PLAIN, "");
    delay_ms(300);
    start_talking("小助手你好。");
    double wakes = sim_metric_value("ai.wakes"), asks = sim_metric_value("hub.requests.ai.listen");
    mic("wake");
    delay_ms(1000);
    mic("speech");
    delay_ms(500);
    CHECK(sim_metric_value("ai.wakes") == wakes && sim_metric_value("hub.requests.ai.listen") == asks, "the wake word is just a word here");
    CHECK(state_is(INK_VOICE_LISTENING), "and it goes on");
    CHECK(WAIT_FOR((look(), strstr(g_heard, "小助手你好。") != NULL), 3000), "heard (%s)", g_heard);
    CHECK(press("说完了"), "said");
    CHECK(WAIT_FOR(state_is(INK_VOICE_IDLE), 3000), "over");
    CHECK(!strcmp(g_value, "小助手你好。"), "in the field (%s)", g_value);
    CHECK(WAIT_FOR(({ int64_t v = 0; ink_state_get_int(ink_state_find("audio.listening"), &v); v != 0; }), 2000), "still listening for it");
    on_ui(do_blur);
    delay_ms(300);
    mic("wake");
    delay_ms(700);
    mic("speech");
    delay_ms(1000);
    mic("silence");
    CHECK(WAIT_FOR(sim_metric_value("ai.wakes") == wakes + 1, 3000), "and afterwards it works as ever");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    delay_ms(1000); /* the hub's directory */
    const char *phase = getenv("INK_DICTATION_PHASE");
    if (phase && !strcmp(phase, "handsfree")) handsfree();
    else {
        happy();
        pauses();
        cancelled();
        hub_says_no("budget", "额度");
        hub_says_no("vendor", "用不了");
        hub_says_no("too_long", "太长");
        offline();
        fields();
        ended_by();
        the_ai();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "dictatetest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
