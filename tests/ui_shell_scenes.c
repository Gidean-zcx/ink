/* Render the product's real presenters with synthetic service inputs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/notify.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"

#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char scene[32];
static lv_obj_t *field;
static bool password;
static volatile int pin_done;
static int wrong;

static void pin_result(void *ctx, int result, uint32_t wait_s) { (void)ctx; (void)result; (void)wait_s; pin_done++; }
static void set_level(const char *id, int64_t value) { ink_receipt_t rc; ink_setting_set(id, value, INK_SRC_TOUCH, &rc); }
static void fake_effect(ink_call_t *c) { snprintf(c->rc.say, sizeof c->rc.say, "测试动作已执行"); ink_probe_count("test.confirmed"); }
static const ink_cap_t FIXTURES[] = {
    {.id = "test.share", .title = "AI 想把这条备忘发出去", .ask = "发给 example.com：明天记得带作业本", .effect = INK_FX_EXTERNAL, .child_ok = true, .run = fake_effect},
    {.id = "test.erase", .title = "删除这 3 个测试文件？", .ask = "传进来的：测试文件，共 6.4 MB。从存储卡上删掉，找不回来。", .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true, .run = fake_effect},
    {.id = "test.secret", .title = "AI 想把验证码填进这个网页", .ask = "bank.example.com，验证码 ******。密码、验证码和证件号要你点头。", .effect = INK_FX_EXTERNAL, .child_ok = true, .run = fake_effect}
};

static int text_snapshot(char *buf, size_t cap) { return snprintf(buf, cap, "测试输入页；%s", password ? "密码内容不可见" : ink_text_value()); }
static void text_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, password ? "网络密码" : "写备忘");
    ink_text_cfg_t cfg = {.type = password ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN, .enter = INK_ENTER_DONE,
                          .initial = password ? "FAKE_TEST_SECRET" : "明天带作业本，", .hint = "写下一句话", .max = 1024};
    field = ink_textfield_create(screen, &cfg, NULL, NULL, true);
    lv_obj_set_pos(field, INK_MARGIN, INK_LINE_BODY + INK_PRIMARY + INK_GAP_XS);
    lv_obj_set_size(field, INK_CANVAS_W - 2 * INK_MARGIN, 4 * INK_PRIMARY);
    if (!strcmp(scene, "ImePinyin") || !strcmp(scene, "ImeCandidates")) for (const char *p = "jintian"; *p; p++) ink_text_key(*p);
    if (!strcmp(scene, "ImeEdit")) ink_text_select(2, 4);
    if (!strcmp(scene, "ImeVoice")) ink_text_voice_start();
}
static const ink_activity_t TEXT_PAGE = {.name = "test.text", .create = text_create, .snapshot = text_snapshot};

static void component_scene(lv_obj_t *screen, void *arg) {
    (void)arg;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_ui_label(screen, "公共组件", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN, w);
    ink_ui_pill(screen, "主要", INK_MARGIN, INK_PRIMARY, 2 * INK_PRIMARY, INK_PRIMARY, true, NULL, NULL);
    ink_ui_pill(screen, "次要", INK_MARGIN + 2 * INK_PRIMARY + INK_GAP, INK_PRIMARY, 2 * INK_PRIMARY, INK_PRIMARY, false, NULL, NULL);
    ink_ui_toggle(screen, "开关", true, INK_MARGIN, 2 * INK_PRIMARY + INK_GAP_L, w, NULL, NULL);
    ink_ui_slider(screen, "亮度", 4, INK_MARGIN, 3 * INK_PRIMARY + INK_GAP_L, w, NULL, NULL);
    const char *const labels[] = {"自动", "Wi-Fi", "4G"};
    ink_ui_segments(screen, labels, 3, 0, INK_MARGIN, 4 * INK_PRIMARY + INK_GAP_XXL, w, NULL, NULL);
    ink_ui_list_row(screen, "wifi", "网络", "没有连接", INK_MARGIN, 6 * INK_PRIMARY, w, false, NULL, NULL);
    ink_ui_card(screen, INK_MARGIN, 7 * INK_PRIMARY, w, INK_ROW_DOUBLE, INK_SURFACE_WHITE);
    ink_ui_pagebar(screen, 1, 3, 9 * INK_PRIMARY, NULL);
    ink_ui_notice(screen, "动作的结果就是反馈", 10 * INK_PRIMARY);
}
static const ink_activity_t COMPONENTS = {.name = "Components", .create = component_scene, .snapshot = text_snapshot};

static void prepare(void) {
    ink_time_set(1793803620, false); /* A fixed synthetic clock, not the host clock. */
    set_level("display.light", 40); set_level("display.warmth", 30); set_level("audio.volume", 60);
    if (!strcmp(scene, "Today")) ink_shell_home_swipe(INK_SWIPE_RIGHT);
    else if (!strcmp(scene, "Apps")) ink_shell_home_swipe(INK_SWIPE_LEFT);
    else if (!strcmp(scene, "Control")) ink_shell_control();
    else if (!strcmp(scene, "Components")) ink_ui_push(&COMPONENTS, NULL);
    else if (!strcmp(scene, "Settings") || !strncmp(scene, "Set", 3) || !strcmp(scene, "Notifications")) ink_shell_open_page(scene);
    else if (!strncmp(scene, "Ime", 3)) { password = !strcmp(scene, "ImePassword"); ink_ui_push(&TEXT_PAGE, NULL); }
    else if (!strncmp(scene, "Confirm", 7)) {
        ink_receipt_t rc;
        ink_cap_invoke(!strcmp(scene, "ConfirmDelete") ? "test.erase" : !strcmp(scene, "ConfirmSecret") ? "test.secret" : "test.share", NULL, INK_SRC_AI, 0, &rc);
        CHECK(rc.status == INK_RC_PENDING_CONFIRMATION, "a real confirmation transaction");
    } else if (!strcmp(scene, "AiListen")) ink_ai_key(true, osal_now_us());
    else if (!strcmp(scene, "AiThink")) ink_ai_ask_text("这页是什么意思？");
    else if (!strcmp(scene, "Ai")) ink_ai_ask_text("夜邮是什么意思？");
    else if (!strcmp(scene, "Members")) ink_shell_members_open(false);
}

static void find_member(void) {
    /* The first real member button is the adult. */
    read_screen();
}
static void make_wrong(void) {
    ink_member_unlock("owner", "9999", pin_result, NULL);
    wrong++;
}
static void make_alarm(void) {
    ink_interrupt_t a = {.kind = INK_INT_ALARM, .nactions = 2};
    snprintf(a.title, sizeof a.title, "起床"); snprintf(a.body, sizeof a.body, "7:30");
    a.actions[0].label = "再睡 10 分钟"; a.actions[0].cap = "alarm.snooze";
    a.actions[1].label = "这次不再响"; a.actions[1].cap = "alarm.dismiss";
    ink_interrupt_raise(&a);
}
static void take_snapshot(void *arg) { sim_display_snapshot((const char *)arg); }
static void snapshot_now(void) { vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, "scene"); delay_ms(5); }

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1600);
    on_ui(prepare);
    if (!strcmp(scene, "MembersBack") || !strcmp(scene, "Pin") || !strcmp(scene, "PinWrong")) {
        ink_member_set_pin("owner", NULL, "1234", pin_result, NULL);
        CHECK(WAIT_FOR(pin_done > 0, 5000), "a synthetic PIN is set");
        ink_power_lock(); delay_ms(600); ink_power_unlock(osal_now_us()); delay_ms(1000);
        if (strcmp(scene, "MembersBack")) { on_ui(find_member); tap(g_btn_x[0], g_btn_y[0]); }
        if (!strcmp(scene, "PinWrong")) { on_ui(make_wrong); delay_ms(500); }
    } else if (!strcmp(scene, "Lock") || !strcmp(scene, "LockPage")) {
        set_level("display.lock_style", !strcmp(scene, "Lock")); ink_power_lock();
    } else if (!strcmp(scene, "PowerMenu")) {
        hw(HW_KEY_DOWN, HAL_KEY_POWER, 0); delay_ms(1600); hw(HW_KEY_UP, HAL_KEY_POWER, 0);
    } else if (!strcmp(scene, "PowerOff")) {
        BY_HAND("system.shutdown", NULL); delay_ms(10000);
    } else if (!strcmp(scene, "PhoneIncoming")) {
        snprintf(g_hw.num, sizeof g_hw.num, "00000000000"); hw(HW_CALL, 0, 0);
    } else if (!strcmp(scene, "ClockRing")) on_ui(make_alarm);
    delay_ms(!strcmp(scene, "Ai") ? 14000 : !strcmp(scene, "AiThink") ? 500 : 2000);
    on_ui(read_screen);
    CHECK(g_snap[0] != 0, "the screen has an AI snapshot (%s)", scene);
    if (password) CHECK(!strstr(g_snap, "FAKE_TEST_SECRET"), "password is hidden from the AI");
    snapshot_now();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    const char *requested = getenv("INK_UI_SCENE");
    snprintf(scene, sizeof scene, "%s", requested ? requested : "Main");
    product_main();
    ink_cap_register(FIXTURES, sizeof FIXTURES / sizeof *FIXTURES);
    osal_task_cfg_t cfg = {.name = "ui-scenes", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
                          .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI scene task");
}
