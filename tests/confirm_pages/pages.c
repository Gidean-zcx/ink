/* Confirmation presenters driven through real services, with synthetic data only. */
#include <stdlib.h>
#include "ink/sys/confirm.h"
#include "ink/sys/hub.h"
#include "ink/sys/settings.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

extern void test_confirm_request(const ink_json_t *j);
static char scene[32], decision[24], status[24];
static uint32_t result_token;
static int runs, px, py;
static int64_t executed_n;
static const char *wanted;
static bool enabled;
static volatile int pin_done;
static int pin_result;
static void pin_told(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done++; }
static void effect(ink_call_t *c) { ink_args_get_int(&c->args, "n", &executed_n); runs++; }
static const ink_cap_t FIXTURES[] = {
    {.id="test.parent_page", .title="移除合成家人？", .ask="合成家人的备忘、录音和进度都会删掉，找不回来。", .ask_by_hand=true,
     .parent_pin=true, .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_BG, .run=effect},
    {.id="test.stress_page", .title="AI 想发送这条合成备忘", .ask="第 {n} 项合成备忘\n第一段内容\n第二段内容\n第三段内容\n第四段内容\n第五段内容",
     .input="{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}},\"required\":[\"n\"]}",
     .effect=INK_FX_EXTERNAL, .child_ok=true, .exec=INK_EXEC_BG, .run=effect}
};
static const ink_setting_def_t SETTING = {.id="test.confirm_page", .title="合成高风险规则", .type=INK_SET_BOOL, .ask_by_hand=true};
static void result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id; int64_t token = 0;
    status[0] = 0;
    ink_args_get_int(a, "token", &token); result_token = (uint32_t)token;
    ink_args_get_str(a, "decision", decision, sizeof decision);
    ink_args_get_str(a, "status", status, sizeof status);
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "panel settles on %s", scene); }
static void find(lv_obj_t *root) {
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted)) {
        lv_obj_t *button = lv_obj_get_parent(root); lv_area_t a;
        if (!lv_obj_check_type(button, &lv_button_class) && lv_obj_get_child_count(button) == 3) button = lv_obj_get_child(button, 2);
        lv_obj_get_coords(button, &a); px = (a.x1 + a.x2) / 2; py = (a.y1 + a.y2) / 2;
        enabled = !lv_obj_has_state(button, LV_STATE_DISABLED);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) find(lv_obj_get_child(root, (int32_t)i));
}
static void locate(void) { px = py = -1; enabled = false; lv_obj_update_layout(lv_layer_sys()); find(lv_layer_sys()); }
static void press(const char *label) {
    wanted = label;
    CHECK(WAIT_FOR(({ on_ui(locate); px >= 0 && enabled; }), 5000), "human control '%s' is available", label);
    if (px >= 0 && enabled) tap(px, py);
    quiet();
}
static void view(void) { quiet(); on_ui(read_screen); }
static void save_snapshot(void *arg) {
    (void)arg; char path[1024];
    int n = snprintf(path, sizeof path, "%s/snapshot.txt", g_sim.out_dir);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "w");
    if (!f || fputs(g_snap, f) == EOF || fclose(f)) abort();
}
static void capture(void *arg) {
    (void)arg; CHECK(settled(), "screenshot is fully presented");
    ink_probe_count("test.snapshot.settled");
    char key_name[80]; snprintf(key_name, sizeof key_name, "test.snapshot.%s", scene); ink_probe_count(key_name);
    sim_display_snapshot("scene");
}
static void screenshot(void) { ink_time_set(1793803620, false); view(); vtk_host_call(save_snapshot, NULL); vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL); delay_ms(5); }
static bool waiting(uint32_t token) {
    ink_confirm_view_t cards[4]; int n = ink_confirm_list(cards, 4);
    for (int i = 0; i < n; i++) if (cards[i].token == token) return true;
    return false;
}
static uint32_t ask(int n) {
    if (!strcmp(scene, "ConfirmWeb") || !strcmp(scene, "ConfirmExpired")) {
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); char json[768];
        snprintf(json, sizeof json, "{\"title\":\"这一步要花钱\",\"body\":\"shop.example.com · 合成订单\\n提交订单 · 38 元\",\"member\":\"owner\",\"parent\":false,\"expires_ms\":60000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}", n, ink_hub_member_epoch());
        CHECK(j && ink_json_parse(j, json, strlen(json)) == 0, "synthetic web request parses");
        if (j) { test_confirm_request(j); osal_free(j); }
    } else if (!strcmp(scene, "ConfirmCall")) {
        CHECK(AI("call.dial", "{\"number\":\"00000000000\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "real call waits for a person");
    } else if (!strcmp(scene, "ConfirmSetting")) {
        CHECK(AI("settings.set", n == 0 ? "{\"id\":\"test.confirm_page\",\"value\":true}" : "{\"id\":\"test.confirm_page\",\"value\":false}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "real high-risk setting waits");
    } else if (!strcmp(scene, "ConfirmParent")) {
        CHECK(AI("test.parent_page", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "parent fixture waits");
    } else {
        char args[40]; snprintf(args, sizeof args, "{\"n\":%d}", n);
        CHECK(AI("test.stress_page", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "long confirmation waits");
    }
    ink_confirm_view_t cards[4]; int count = ink_confirm_list(cards, 4);
    CHECK(count > 0, "a real transaction exists");
    return count ? cards[count - 1].token : 0;
}
static void read_all(uint32_t token) {
    (void)token;
    for (int i = 0; i < 161; i++) {
        view(); const char *at = strstr(g_snap, "阅读="); int page = 0, pages = 0;
        CHECK(at && sscanf(at, "阅读=%d/%d", &page, &pages) == 2 && pages > 0, "snapshot reports mandatory reading");
        if (!pages || page == pages) return;
        char label[40]; snprintf(label, sizeof label, "%d / %d", page, pages);
        press(label);
    }
    CHECK(false, "all pages are reachable");
}
static const char *accept_label(void) {
    if (!strcmp(scene, "ConfirmParent")) return "输入密码";
    if (!strcmp(scene, "ConfirmCall")) return "打电话";
    if (!strcmp(scene, "ConfirmSetting")) return "确认修改";
    if (!strcmp(scene, "ConfirmWeb")) return "确认支付";
    return "确认";
}
static void ai_cannot_answer(uint32_t token) {
    for (int src = INK_SRC_AI; src <= INK_SRC_CONFIRMED; src++) {
        CHECK(ink_confirm_decide(token, true, (ink_source_t)src) == -1, "source %d cannot accept", src);
        CHECK(ink_confirm_decide(token, false, (ink_source_t)src) == -1, "source %d cannot reject", src);
    }
    char args[80]; snprintf(args, sizeof args, "{\"token\":%u,\"delta\":1}", token);
    CHECK(AI("confirm.page", args) && g_rc.status == INK_RC_INVALID, "AI cannot mark pages as read");
    CHECK(waiting(token), "nonhuman attempts leave the transaction intact");
}
static void check_snapshot(void) {
    view();
    CHECK(strstr(g_snap, scene) && strstr(g_snap, "家人=owner") && strstr(g_snap, "确认编号="), "%s has its own member-bound AI snapshot (top=%s)", scene, g_top);
}
static void queue_test(void) {
    int before = runs;
    executed_n = -1;
    uint32_t tokens[4];
    for (int i = 0; i < 4; i++) tokens[i] = ask(i + 10);
    CHECK(AI("test.stress_page", "{\"n\":99}") && g_rc.status == INK_RC_REJECTED, "fifth request is rejected without replacing the queue");
    for (int i = 0; i < 4; i++) {
        ink_confirm_view_t cards[4]; CHECK(ink_confirm_list(cards, 4) == 4 - i && cards[0].token == tokens[i], "FIFO item %d", i);
        view(); char count[40]; snprintf(count, sizeof count, "排队=%d", 4 - i);
        CHECK(strstr(g_snap, count), "snapshot reports the actual queue length");
        if (i == 1) { read_all(tokens[i]); press("确认"); }
        else press("取消");
        CHECK(WAIT_FOR(result_token == tokens[i], 10000), "each queue item gets its own result");
        CHECK(!strcmp(decision, i == 1 ? "accepted" : "rejected") && (i == 1 ? !strcmp(status, "done") : !status[0]),
              "queue item %d has the expected decision and execution status (%s/%s)", i, decision, status);
        CHECK(runs == before + (i >= 1) && executed_n == (i >= 1 ? 11 : -1),
              "only the second queued object executes: item=%d runs=%d n=%lld", i, runs - before, (long long)executed_n);
    }
    CHECK(runs == before + 1 && executed_n == 11, "queue executes only n=11 once");
}
static void run(void *arg) {
    (void)arg; delay_ms(2000); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    CHECK(ink_cap_register(FIXTURES, 2) == 0 && ink_settings_define(&SETTING, 1) == 0, "synthetic fixtures register");
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "observe real confirmation results");
    ink_time_set(1793803620, false);
    if (!strcmp(scene, "ConfirmParent") || !strcmp(scene, "ConfirmExpired")) {
        CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "set a synthetic parent password");
    }
    if (!strcmp(scene, "ConfirmWeb") || !strcmp(scene, "ConfirmExpired")) {
        ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner", ink_hub_member_epoch(), ink_hub_connection()), 30000), "synthetic hub is connected");
    }
    if (!strcmp(scene, "ConfirmExpired")) {
        uint32_t token = ask(0);
        CHECK(WAIT_FOR(result_token == token, 62000) && !strcmp(decision, "expired"), "real deadline expires");
        check_snapshot(); screenshot();
        char args[80]; snprintf(args, sizeof args, "{\"token\":%u,\"delta\":0}", token);
        CHECK(AI("confirm.page", args) && g_rc.status == INK_RC_INVALID, "expired explanation remains a human page");
        press("关闭"); view(); CHECK(strcmp(g_top, "ConfirmExpired"), "close removes the expired page");
        token = ask(1);
        CHECK(WAIT_FOR(result_token == token, 62000) && !strcmp(decision, "expired"), "another request really expires");
        view(); CHECK(!strcmp(g_top, "ConfirmExpired"), "expired card is visible before interruption");
        snprintf(g_hw.num, sizeof g_hw.num, "00000000000"); hw(HW_CALL, 0, 0);
        CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 15000), "synthetic incoming call interrupts");
        hw(HW_HANGUP, 0, 0);
        CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 15000), "synthetic call ends");
        view(); CHECK(strcmp(g_top, "ConfirmExpired") && g_snap[0], "expired card stays closed after interruption");
        token = ask(2);
        CHECK(WAIT_FOR(result_token == token, 62000) && !strcmp(decision, "expired"), "request expires before member handoff");
        view(); CHECK(!strcmp(g_top, "ConfirmExpired"), "expired card is visible before locking");
        key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 5000), "lock expired card");
        view(); CHECK(strcmp(g_top, "ConfirmExpired"), "locking closes expired overlay");
        delay_ms(121000); key(HAL_KEY_POWER); view();
        CHECK(!strcmp(g_top, "member_gate"), "late wake requires member choice (%s)", g_top);
        press("小"); view();
        CHECK(!strcmp(ink_member_current()->id, "kid"), "touch selects the child");
        CHECK(strcmp(g_top, "ConfirmExpired") && g_snap[0], "child has an unobstructed nonempty AI snapshot (%s)", g_top);
        CHECK(!strstr(g_snap, "shop.example.com") && !strstr(g_snap, "家人=owner"), "child snapshot excludes the old confirmation");
    } else for (int operation = 0; operation < 3; operation++) {
        uint32_t token = ask(operation); check_snapshot(); ai_cannot_answer(token);
        if (operation == 0 && strcmp(scene, "ConfirmParent")) screenshot();
        if (operation != 1) read_all(token);
        if (!strcmp(scene, "ConfirmParent") && operation != 1) {
            press(accept_label()); check_snapshot();
            CHECK(strstr(g_snap, "密码内容不可见"), "PIN snapshot hides password content");
        }
        if (operation == 0) {
            if (!strcmp(scene, "ConfirmParent")) screenshot();
            if (!strcmp(scene, "ConfirmParent")) {
                for (int i = 0; i < 4; i++) press("0");
                press("确定"); view(); CHECK(waiting(token) && strstr(g_snap, "密码内容不可见"), "wrong PIN never accepts");
                for (int i = 1; i <= 4; i++) { char digit[2] = {(char)('0' + i), 0}; press(digit); }
                view(); CHECK(!strstr(g_snap, "1234") && !strstr(g_snap, "0000"), "typed PIN never appears in AI snapshot");
                press("确定");
            } else press(accept_label());
            CHECK(WAIT_FOR(result_token == token, 10000) && !strcmp(decision, "accepted"), "human acceptance completes (%s/%s)", decision, status);
            if (!strcmp(scene, "ConfirmCall")) { CHECK(BY_HAND("call.hangup", NULL), "end the synthetic call"); quiet(); }
            if (!strcmp(scene, "ConfirmSetting")) CHECK(ink_setting_get(SETTING.id) == 1, "accepted setting changes");
            if (!strcmp(scene, "ConfirmParent") || !strcmp(scene, "StressConfirm")) CHECK(runs == 1, "one accepted fixture executes once");
        } else if (operation == 1) {
            if (!strcmp(scene, "ConfirmParent")) { read_all(token); press("输入密码"); press("1"); }
            press("取消");
            CHECK(WAIT_FOR(result_token == token, 5000) && !strcmp(decision, "rejected"), "human rejection completes");
        } else {
            CHECK(WAIT_FOR(result_token == token, 62000) && !strcmp(decision, "expired"), "each kind expires");
            view(); CHECK(!strcmp(g_top, "ConfirmExpired") && strstr(g_snap, "已经过期"), "expiration has an AI-readable result page");
            CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) < 0, "expired token cannot execute");
            press("关闭");
        }
        if (operation && !strcmp(scene, "ConfirmSetting")) CHECK(ink_setting_get(SETTING.id) == 1, "rejected and expired settings stay unchanged");
        if (operation && (!strcmp(scene, "ConfirmParent") || !strcmp(scene, "StressConfirm"))) CHECK(runs == 1, "rejected and expired fixtures never execute");
        quiet();
    }
    if (!strcmp(scene, "ConfirmParent")) {
        uint32_t token = ask(3); read_all(token); press("输入密码"); key(HAL_KEY_HOME);
        CHECK(WAIT_FOR(result_token == token, 5000) && !strcmp(decision, "rejected") && runs == 1, "HOME still rejects from the password page without execution");
    }
    if (!strcmp(scene, "StressConfirm")) queue_test();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    sim_copy_name(scene, sizeof scene, getenv("INK_CONFIRM_SCENE")); product_main();
    osal_task_cfg_t cfg = {.name="test-confirm-pages", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start confirmation page tests");
}
