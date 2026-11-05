/* Render the product's real presenters with synthetic service inputs. */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/audio.h"
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
static const char *wanted;
static int label_x, label_y;
static void label_at(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *c = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), wanted)) {
            lv_area_t a; lv_obj_update_layout(c); lv_obj_get_coords(c, &a);
            label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2;
        }
        label_at(c);
    }
}
static void find_label(void) { label_x = label_y = -1; label_at(lv_screen_active()); label_at(lv_layer_sys()); }
static bool press_label(const char *label) { wanted = label; on_ui(find_label); if (label_x < 0) return false; tap(label_x, label_y); return true; }

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
    if (!strcmp(scene, "ImePinyin") || !strcmp(scene, "ImeCandidates")) for (const char *p = !strcmp(scene, "ImeCandidates") ? "shi" : "jintian"; *p; p++) ink_text_key(*p);
    if (!strcmp(scene, "ImeEdit")) ink_text_select(2, 4);
    if (!strcmp(scene, "ImeVoice")) ink_text_voice_start();
}
static const ink_activity_t TEXT_PAGE = {.name = "test.text", .create = text_create, .snapshot = text_snapshot};

static void component_scene(lv_obj_t *screen, void *arg) {
    (void)arg;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, "公共组件");
    if (!strcmp(scene, "ComponentsDock")) { ink_shell_dock(screen); return; }
    if (!strcmp(scene, "ComponentsPanel")) {
        lv_obj_t *panel = ink_ui_panel(screen, 3 * INK_PRIMARY + 2 * INK_MARGIN, true);
        ink_ui_label(panel, "底部面板", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN, w);
        ink_ui_notice(panel, "内容从 32 开始，面板贴底", INK_MARGIN + INK_PRIMARY);
        return;
    }
    int y = INK_LINE_BODY + INK_PRIMARY + INK_MARGIN;
    ink_ui_pill(screen, "主要", INK_MARGIN, y, 2 * INK_PRIMARY, INK_PRIMARY, true, NULL, NULL);
    ink_ui_pill(screen, "次要", INK_MARGIN + 2 * INK_PRIMARY + INK_GAP, y, 2 * INK_PRIMARY, INK_PRIMARY, false, NULL, NULL);
    ink_ui_circle(screen, "ai", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, y, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    y += INK_PRIMARY + INK_GAP;
    ink_ui_toggle(screen, "开关", true, INK_MARGIN, y, w, NULL, NULL); y += INK_CONTROL + INK_GAP;
    ink_ui_slider(screen, "亮度", 4, INK_MARGIN, y, w, NULL, NULL); y += INK_SLIDER + INK_GAP;
    const char *const labels[] = {"自动", "Wi-Fi", "4G"};
    ink_ui_segments(screen, labels, 3, 0, INK_MARGIN, y, w, NULL, NULL); y += INK_SEGMENT + INK_GAP;
    ink_ui_list_row(screen, "wifi", "网络", "没有连接", INK_MARGIN, y, w, false, NULL, NULL); y += INK_CONTROL + INK_GAP;
    ink_ui_card(screen, INK_MARGIN, y, w, INK_ROW_DOUBLE, INK_SURFACE_WHITE); y += INK_ROW_DOUBLE + INK_GAP;
    ink_ui_pagebar(screen, 1, 3, y, NULL); y += INK_CONTROL + INK_GAP;
    ink_ui_notice(screen, "动作的结果就是反馈", y);
}
static const ink_activity_t COMPONENTS = {.name = "Components", .create = component_scene, .snapshot = text_snapshot};

static void prepare(void) {
    ink_time_set(1793803620, false); /* A fixed synthetic clock, not the host clock. */
    set_level("display.light", 40); set_level("display.warmth", 30); set_level("audio.volume", 60);
    if (!strcmp(scene, "Today")) ink_shell_home_swipe(INK_SWIPE_RIGHT);
    else if (!strcmp(scene, "Apps")) ink_shell_home_swipe(INK_SWIPE_LEFT);
    else if (!strcmp(scene, "Control")) ink_shell_control();
    else if (!strncmp(scene, "Components", 10)) ink_ui_push(&COMPONENTS, NULL);
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
static void make_alarm(void) {
    ink_alarm_t a = {.kind = INK_ALARM, .at_utc = ink_time_now_s() + 2, .src = INK_SRC_TOUCH};
    snprintf(a.title, sizeof a.title, "起床");
    CHECK(ink_alarm_add(&a) > 0, "schedule a real synthetic alarm");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool quiet(void) {
    bool ok = WAIT_FOR(settled(), 10000);
    CHECK(ok, "%s did not settle within 10 virtual seconds", scene);
    return ok;
}
static void take_snapshot(void *arg) {
    if (!settled()) { CHECK(false, "snapshot must be a fully presented frame on %s", scene); return; }
    ink_probe_count("test.snapshot.settled");
    char id[80]; snprintf(id, sizeof id, "test.snapshot.%s", scene); ink_probe_count(id);
    sim_display_snapshot((const char *)arg);
}
static void snapshot_now(void) {
    if (!quiet()) return;
    vtk_event_at(vtk_now() + SIM_MS(1), take_snapshot, "scene"); delay_ms(5);
}
static bool listed(const char *id) {
    char *page = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    if (!page) return false;
    char quoted[80]; snprintf(quoted, sizeof quoted, "\"%s\"", id);
    bool found = false;
    int next = 0;
    do {
        if (ink_cap_manifest_page(next, NULL, page, 4096, &next) < 0) break;
        if (strstr(page, quoted)) { found = true; break; }
    } while (next >= 0);
    osal_free(page);
    return found;
}
static bool label_visible(const char *label) { wanted = label; find_label(); return label_x >= 0; }
static bool snapshot_has(const char *text) {
    char *snapshot = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    const char *name;
    bool found = snapshot && ink_ui_snapshot(snapshot, 4096, &name) > 0 && strstr(snapshot, text);
    osal_free(snapshot);
    return found;
}
static void assert_identity(void) {
    const char *top = scene, *page = "home", *expected_text = scene;
    if (!strcmp(scene, "Main") || !strcmp(scene, "Today") || !strcmp(scene, "Apps")) top = "home";
    else if (!strcmp(scene, "Lock") || !strcmp(scene, "LockPage")) { top = "LockPage"; expected_text = "设备已锁定"; }
    else if (!strcmp(scene, "Members") || !strcmp(scene, "MembersBack") || !strncmp(scene, "Pin", 3)) { top = "member_gate"; expected_text = "请选择家人"; }
    else if (!strncmp(scene, "Ime", 3)) { top = "Keyboard"; page = "test.text"; expected_text = password ? "密码输入" : "键盘"; }
    else if (!strncmp(scene, "Confirm", 7)) { top = "confirm"; expected_text = !strcmp(scene, "ConfirmDelete") ? "测试文件" : !strcmp(scene, "ConfirmSecret") ? "******" : "example.com"; }
    else if (!strncmp(scene, "Ai", 2)) { top = "Ai"; expected_text = !strcmp(scene, "AiListen") ? "正在听" : !strcmp(scene, "AiThink") ? "正在看这一页" : "测试答案"; }
    else if (!strcmp(scene, "PowerMenu")) { top = "power_menu"; expected_text = "电源菜单"; }
    else if (!strcmp(scene, "PowerOff")) { top = "off"; expected_text = "已关机"; }
    else if (!strcmp(scene, "PhoneIncoming") || !strcmp(scene, "ClockRing")) { top = "interrupt"; expected_text = !strcmp(scene, "ClockRing") ? "起床" : "00000000000"; }
    else if (!strncmp(scene, "Components", 10)) { top = "Components"; page = "Components"; expected_text = "测试输入页"; }
    else if (!strcmp(scene, "Settings") || !strncmp(scene, "Set", 3) || !strcmp(scene, "Notifications")) page = scene;
    if (!strncmp(scene, "Ai", 2) || !strcmp(scene, "LockPage") || !strcmp(scene, "PowerOff")) page = "book";
    CHECK(!strcmp(ink_ui_top_name(), top), "%s requires overlay/page %s, got %s", scene, top, ink_ui_top_name());
    CHECK(!strcmp(ink_ui_page_name(), page), "%s requires activity %s, got %s", scene, page, ink_ui_page_name());
    if (!strcmp(top, "home")) CHECK(!strcmp(ink_shell_board(), scene), "%s is the selected home screen", scene);
    CHECK(snapshot_has(expected_text), "%s snapshot must contain %s", scene, expected_text);
    if (!strcmp(scene, "Lock") || !strcmp(scene, "LockPage"))
        CHECK(ink_setting_get("display.lock_style") == !strcmp(scene, "Lock") && ink_power_state() == INK_PWR_LOCKED, "the requested lock variant is actually locked");
    if (!strcmp(scene, "Pin")) CHECK(label_visible("输入密码"), "the PIN pad is visible");
    if (!strcmp(scene, "PinWrong")) CHECK(label_visible("密码不对") && !ink_member_space_open(), "a real rejected PIN is visible, the space stays closed");
    if (!strcmp(scene, "Members")) CHECK(label_visible("谁在用？") && ink_member_space_open(), "requested member picker, not the lock gate");
    if (!strcmp(scene, "MembersBack")) CHECK(label_visible("输密码，回到刚才的地方") && !ink_member_space_open(), "returning member gate, not the requested picker");
    if (!strcmp(scene, "ImePinyin") || !strcmp(scene, "ImeCandidates"))
        CHECK(!strcmp(ink_text_composing(), !strcmp(scene, "ImePinyin") ? "jintian" : "shi"), "the expected composition is active");
    if (!strcmp(scene, "ImeCandidates")) CHECK(label_visible("键盘"), "expanded candidates, not the alphabet layout");
    if (!strcmp(scene, "ImeEdit")) { size_t a, b; ink_text_selection(&a, &b); CHECK(a == 2 && b == 4 && label_visible("复制"), "the real selection and edit tools are visible"); }
    if (!strcmp(scene, "ImeVoice")) CHECK(ink_text_voice_state() == INK_VOICE_LISTENING && label_visible("用键盘"), "the real dictation is listening");
    if (!strcmp(scene, "Components")) CHECK(label_visible("开关") && label_visible("自动"), "component controls exist");
    if (!strcmp(scene, "ComponentsPanel")) CHECK(label_visible("底部面板"), "the panel component exists");
    if (!strcmp(scene, "ComponentsDock")) CHECK(lv_obj_get_child_count(lv_screen_active()) > 1 && !label_visible("开关") && !label_visible("底部面板"), "Dock fixture has its own variant");
    if (!strcmp(scene, "Control") || !strcmp(scene, "SetDisplay")) CHECK(snapshot_has("亮度=40"), "current brightness, not just a registered setting");
    if (!strcmp(scene, "SetDisplay")) CHECK(snapshot_has("字号=1") && snapshot_has("自动锁屏秒=600"), "display settings have their current values");
    if (!strcmp(scene, "SetNetwork")) CHECK(snapshot_has("网络组=1") && snapshot_has("SIM="), "network selection context is present");
    if (!strcmp(scene, "Today")) CHECK(snapshot_has("今日报纸=") && snapshot_has("单词=") && snapshot_has("接下来="), "all Today cards have semantic context");
}

static void assert_view(void) {
    assert_identity();
    char *snapshot = osal_malloc(4096, OSAL_MEM_EXTERNAL), *actions = osal_malloc(8192, OSAL_MEM_EXTERNAL);
    const char *name;
    CHECK(snapshot && ink_ui_snapshot(snapshot, 4096, &name) > 0, "nonempty AI view for %s", scene);
    CHECK(actions && ink_cap_ids_json(actions, 8192) > 0, "available actions come from the capability registry for %s", scene);
    CHECK(ink_cap_find("shell.control") && ink_cap_find("text.insert") && ink_cap_find("confirm.withdraw"), "atomic shell, input and confirmation paths exist");
    const char *p = snapshot ? strstr(snapshot, "动作：") : NULL;
    if (p) for (p += strlen("动作："); *p; ) {
        if (!islower((unsigned char)*p)) { p++; continue; }
        char id[64]; size_t n = 0;
        while ((islower((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_' || *p == '.') && n + 1 < sizeof id) id[n++] = *p++;
        id[n] = 0;
        if (strchr(id, '.')) CHECK(ink_cap_available(id) && listed(id), "the declared action %s is usable by this member on %s", id, scene);
    }
    if (password) CHECK(snapshot && !strstr(snapshot, "FAKE_TEST_SECRET") && !strstr(snapshot, "text.insert"), "password snapshot has no secret or AI editing action");
    osal_free(snapshot); osal_free(actions);
}


static void assert_operation(void) {
    if (!ink_member_space_open()) {
        CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_REJECTED, "an unauthenticated AI cannot navigate private controls");
        CHECK(AI("text.insert", "{\"text\":\"test\"}") && g_rc.status != INK_RC_DONE, "an unauthenticated AI cannot write");
        return;
    }
    if (!strcmp(scene, "Main") || !strcmp(scene, "Today") || !strcmp(scene, "Apps")) {
        tap(460, 1120); quiet();
        CHECK(strcmp(ink_ui_page_name(), "home"), "the Dock touch actually opens the reading application");
    } else if (!strcmp(scene, "Settings")) {
        CHECK(press_label("显示和亮度"), "touch the display settings row"); quiet();
        CHECK(!strcmp(ink_ui_page_name(), "SetDisplay"), "the row opens its promised page");
    } else if (!strcmp(scene, "Control") || !strcmp(scene, "SetSound")) {
        tap(528, 208); quiet();
        CHECK(ink_setting_get(!strcmp(scene, "Control") ? "display.light" : "audio.volume") == 80, "the visible slider reaches the setting service");
    } else if (!strcmp(scene, "SetDisplay")) {
        CHECK(press_label("自动锁屏 10 分钟"), "touch the current lock interval"); quiet();
        CHECK(ink_setting_get("power.lock_after_s") == 1800, "the lock interval changes instead of writing 600 again");
    } else if (!strcmp(scene, "SetNetwork")) {
        CHECK(press_label("4G"), "touch the network preference"); quiet();
        CHECK(ink_setting_get("net.prefer") == 2, "network preference is applied by the service");
    } else if (!strcmp(scene, "Notifications")) {
        CHECK(press_label("全部已读"), "touch mark read"); quiet(); CHECK(ink_notify_unread() == 0, "the service marks notifications read");
    } else if (!strncmp(scene, "Confirm", 7)) {
        CHECK(press_label("取消"), "cancel by touch"); quiet(); ink_confirm_view_t v;
        CHECK(ink_confirm_list(&v, 1) == 0, "the transaction is removed without executing the fixture");
    } else if (!strcmp(scene, "ImePassword")) {
        CHECK(AI("text.insert", "{\"text\":\"FORBIDDEN\"}") && g_rc.status != INK_RC_DONE, "AI cannot edit a password");
        CHECK(!strcmp(ink_text_value(), "FAKE_TEST_SECRET"), "the denied call leaves the password untouched");
    } else if (!strcmp(scene, "ImeVoice")) {
        CHECK(press_label("用键盘"), "leave dictation by touch");
        CHECK(WAIT_FOR(ink_text_voice_state() == INK_VOICE_IDLE, 3000), "the touch stops actual dictation");
    } else if (!strcmp(scene, "ImePinyin")) {
        CHECK(AI("keyboard.pick", "{\"index\":0}") && g_rc.status == INK_RC_DONE, "AI selects a real candidate through the same path");
        CHECK(strcmp(ink_text_value(), "明天带作业本，"), "the candidate changes the real text buffer");
    } else if (!strcmp(scene, "ImeCandidates")) {
        CHECK(press_label("键盘"), "fold candidates by touch"); quiet();
        wanted = "q"; on_ui(find_label); CHECK(label_x >= 0, "alphabet keys reappear");
    } else if (!strcmp(scene, "ImeEdit")) {
        CHECK(AI("keyboard.edit", "{\"action\":2}") && g_rc.status == INK_RC_DONE, "copy the selection through its capability");
        char clip[64]; CHECK(ink_clip_get(clip, sizeof clip, INK_SRC_TOUCH, NULL) > 0 && !strcmp(clip, "带作"), "the service copied the selected characters");
        CHECK(ink_clip_get(clip, sizeof clip, INK_SRC_AI, NULL) < 0, "AI clipboard reading still requires its setting");
    } else if (!strcmp(scene, "PowerMenu")) {
        CHECK(press_label("取消"), "cancel the power menu by touch"); quiet(); CHECK(strcmp(ink_ui_top_name(), "power_menu"), "the menu closes");
    } else if (!strcmp(scene, "PhoneIncoming") || !strcmp(scene, "ClockRing")) {
        CHECK(press_label(!strcmp(scene, "PhoneIncoming") ? "拒接" : "关闭"), "touch the interruption action"); quiet();
        CHECK(!strcmp(active_state(), "none"), "the interruption service handles the action");
    } else if (!strncmp(scene, "Ai", 2)) {
        CHECK(AI("ai.panel.hide", NULL) && g_rc.status == INK_RC_DONE, "hide the actual AI panel through its capability"); quiet();
        CHECK(strcmp(ink_ui_top_name(), "Ai"), "the AI panel is hidden and the reading page remains");
    }
}
extern void (*sim_power_before_shutdown)(void);
static void at_shutdown(void) {
    on_ui(assert_view);
    snapshot_now();
    ink_probe_count("test.finished");
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1600);
    if (!strncmp(scene, "Ai", 2) || !strcmp(scene, "LockPage") || !strcmp(scene, "PowerOff")) {
        CHECK(BY_HAND("reader.open_book", "{\"title\":\"星星邮差\"}") && g_rc.status == INK_RC_DONE, "open the real reading surface below the shell");
    }
    quiet();
    on_ui(prepare);
    if (!strcmp(scene, "MembersBack") || !strcmp(scene, "Pin") || !strcmp(scene, "PinWrong")) {
        ink_member_set_pin("owner", NULL, "1234", pin_result, NULL);
        CHECK(WAIT_FOR(pin_done > 0, 5000), "a synthetic PIN is set");
        ink_power_lock(); delay_ms(600); ink_power_unlock(osal_now_us()); delay_ms(1000);
        if (strcmp(scene, "MembersBack")) { on_ui(find_member); tap(g_btn_x[0], g_btn_y[0]); }
        if (!strcmp(scene, "PinWrong")) {
            for (int i = 0; i < 4; i++) CHECK(press_label("9"), "type a wrong test password through the real keypad");
            CHECK(press_label("确定"), "submit through the real keypad"); delay_ms(1000);
        }
    } else if (!strcmp(scene, "ImeCandidates")) {
        CHECK(press_label("更多"), "expand real candidates");
    } else if (!strcmp(scene, "Lock") || !strcmp(scene, "LockPage")) {
        ink_power_hold(INK_NEED_AWAKE); /* Snapshot while locked, before simulated light sleep. */
        set_level("display.lock_style", !strcmp(scene, "Lock")); ink_power_lock();
    } else if (!strcmp(scene, "PowerMenu")) {
        hw(HW_KEY_DOWN, HAL_KEY_POWER, 0); delay_ms(1600); hw(HW_KEY_UP, HAL_KEY_POWER, 0);
    } else if (!strcmp(scene, "PowerOff")) {
        CHECK(ink_cap_find("system.shutdown") != NULL, "shutdown is a real capability");
        sim_power_before_shutdown = at_shutdown;
        BY_HAND("system.shutdown", NULL);
        delay_ms(60000);
        CHECK(false, "shutdown must end the run after the validated frame");
        return;
    } else if (!strcmp(scene, "PhoneIncoming")) {
        snprintf(g_hw.num, sizeof g_hw.num, "00000000000"); hw(HW_CALL, 0, 0);
    } else if (!strcmp(scene, "ClockRing")) {
        on_ui(make_alarm);
        CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "the alarm service is actually ringing");
    }
    if (!strcmp(scene, "Ai")) CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 1, 20000), "the fixture answer arrives");
    if (!strcmp(scene, "ImeVoice")) CHECK(WAIT_FOR(ink_text_voice_state() == INK_VOICE_LISTENING, 5000), "the dictation fixture is listening");
    quiet();
    on_ui(read_screen);
    on_ui(assert_view);
    CHECK(g_snap[0] != 0, "the screen has an AI snapshot (%s)", scene);
    if (password) CHECK(!strstr(g_snap, "FAKE_TEST_SECRET"), "password is hidden from the AI");
    snapshot_now();
    if (!strcmp(scene, "Pin")) {
        CHECK(press_label("1") && press_label("2") && press_label("3") && press_label("4") && press_label("确定"), "submit the synthetic PIN by touch");
        CHECK(WAIT_FOR(ink_member_space_open(), 5000), "the correct PIN opens the space");
        on_ui(read_screen);
        CHECK(strcmp(g_top, "member_gate"), "the successful PIN closes its presenter");
    }
    assert_operation();
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
