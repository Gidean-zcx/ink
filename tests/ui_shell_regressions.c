/* Review regressions exercise the real card and settings through safe fixtures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/settings.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static const char *mode, *detail, *wanted;
static int effects, page, pages, accept_x, accept_y, next_x, next_y, label_x, label_y;
static bool disabled, fits;
static char visible[160];
static lv_obj_t *accept;
static lv_area_t slider;
static bool have_slider;
static int screen_w, screen_h, interrupt_id;
static int pager_bottom, accept_top;
static void effect(ink_call_t *c) { (void)c; effects++; }
static const ink_cap_t CAPS[] = {
    {.id = "test.long_confirm", .title = "删除测试文件", .ask = "{detail}", .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"detail\":{\"type\":\"string\",\"maxLength\":512}},\"required\":[\"detail\"]}", .run = effect}
};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void inspect(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *o = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(o, &lv_label_class)) {
            const char *s = lv_label_get_text(o);
            lv_obj_t *parent = lv_obj_get_parent(o);
            lv_area_t area; lv_obj_get_coords(o, &area);
            if (!strcmp(s, "删除")) {
                accept = parent; disabled = lv_obj_has_state(parent, LV_STATE_DISABLED);
                accept_x = (area.x1 + area.x2) / 2; accept_y = (area.y1 + area.y2) / 2;
                lv_area_t bounds; lv_obj_get_coords(parent, &bounds); accept_top = bounds.y1;
            }
            int a, b;
            if (sscanf(s, "%d / %d", &a, &b) == 2) {
                page = a; pages = b;
                lv_area_t bounds; lv_obj_get_coords(parent, &bounds); pager_bottom = bounds.y2;
                lv_obj_get_coords(lv_obj_get_child(parent, 2), &area);
                next_x = (area.x1 + area.x2) / 2; next_y = (area.y1 + area.y2) / 2;
            }
            if (detail && s[0] && strstr(detail, s) && !lv_obj_check_type(parent, &lv_button_class)) {
                snprintf(visible, sizeof visible, "%s", s);
                lv_point_t size;
                lv_text_get_size(&size, s, lv_obj_get_style_text_font(o, LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), lv_obj_get_style_text_line_space(o, LV_PART_MAIN),
                    lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
                fits = size.y <= lv_obj_get_height(o);
            }
            if (wanted && !strcmp(s, wanted)) {
                label_x = (area.x1 + area.x2) / 2; label_y = (area.y1 + area.y2) / 2;
                while (parent && !lv_obj_has_flag(parent, LV_OBJ_FLAG_CLICKABLE)) parent = lv_obj_get_parent(parent);
                if (parent) { lv_obj_get_coords(parent, &slider); have_slider = true; }
            }
        }
        inspect(o);
    }
}
static void read_view(void) {
    lv_obj_update_layout(lv_layer_sys()); lv_obj_update_layout(lv_screen_active());
    page = pages = 1; accept = NULL; visible[0] = 0; fits = disabled = have_slider = false; label_x = label_y = -1;
    inspect(lv_screen_active()); inspect(lv_layer_sys());
    screen_w = ink_ui_width(); screen_h = ink_ui_height();
}
static void swipe(int x0, int y0, int x1, int y1) {
    hw(HW_PRESS, x0, y0); delay_ms(100); hw(HW_PRESS, x1, y1); delay_ms(100); hw(HW_RELEASE, x1, y1); quiet();
}
static uint32_t open_confirm(const char *text) {
    detail = text;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    ink_args_init(args); ink_args_str(args, "detail", text);
    g_done = 0;
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    CHECK(ink_cap_call("test.long_confirm", args, &opts) == 0, "submit a safe confirmation fixture");
    osal_free(args);
    CHECK(WAIT_FOR(g_done, 3000) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "a real pending confirmation");
    quiet(); on_ui(read_view);
    ink_confirm_view_t view;
    CHECK(ink_confirm_list(&view, 1) == 1 && !strcmp(view.detail, text), "the service preserves the complete detail");
    return view.token;
}
static void forced_accept(void) { if (accept) lv_obj_send_event(accept, LV_EVENT_CLICKED, NULL); }
static void raise_call(void) {
    ink_interrupt_t it = {.kind = INK_INT_CALL};
    snprintf(it.title, sizeof it.title, "测试来电");
    interrupt_id = ink_interrupt_raise(&it);
}
static void read_all(const char *text) {
    char seen[512] = "";
    CHECK(page == 1 && pages > 1 && disabled, "a long transaction starts unread with acceptance disabled");
    CHECK(pager_bottom < accept_top, "the paging controls do not overlap confirmation buttons");
    on_ui(forced_accept); quiet();
    CHECK(effects == 0, "even a synthetic click cannot bypass unread pages");
    for (int i = 1; i <= pages && i < 80; i++) {
        CHECK(page == i && fits && visible[0], "page %d fits the actual label without clipping", i);
        if (strlen(seen) + strlen(visible) < sizeof seen) strcat(seen, visible);
        if (i < pages) { tap(next_x, next_y); quiet(); on_ui(read_view); }
    }
    CHECK(!strcmp(seen, text), "all objects and the irreversible consequence appear, without lost bytes");
    CHECK(WAIT_FOR(({ on_ui(read_view); !disabled; }), 1000), "the presented last page enables acceptance");
    CHECK(!disabled, "acceptance is enabled after presenting the last page");
}
static void confirmation_tests(void) {
    const char *long_text = "测试文件甲\n测试文件乙\n测试文件丙\n这三个文件都找不回来";
    uint32_t first = open_confirm(long_text);
    char page_args[96]; snprintf(page_args, sizeof page_args, "{\"token\":%u,\"delta\":1}", (unsigned)first);
    CHECK(AI("confirm.page", page_args) && g_rc.status == INK_RC_INVALID, "AI cannot mark mandatory pages read");
    on_ui(read_view); CHECK(page == 1 && disabled, "an AI page request cannot change reading progress");
    CHECK(ink_confirm_decide(first, true, INK_SRC_AI) < 0 && effects == 0, "AI cannot accept even with a real token");
    read_all(long_text);
    tap(accept_x, accept_y); quiet();
    CHECK(effects == 1 && strcmp(ink_ui_top_name(), "confirm"), "one human acceptance executes the fixture exactly once");
    effects = 0;
    uint32_t second = open_confirm(long_text);
    CHECK(second != first && disabled && page == 1, "a new token cannot inherit the prior reading state");
    CHECK(BY_HAND("confirm.page", page_args) && g_rc.status == INK_RC_STALE, "a stale token cannot turn the next transaction's pages");
    on_ui(read_view); CHECK(page == 1, "stale pagination leaves the new card on its first page");
    int w = screen_w, h = screen_h;
    swipe(w / 2, h - 4, w / 2, h * 3 / 4);
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "bottom edge cancels without executing");
    open_confirm(long_text);
    swipe(4, h * 2 / 3, w / 2, h * 2 / 3);
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "left edge cancels without executing");
    open_confirm("文件名称很长很长很长很长很长很长很长很长很长很长很长很长.txt\n删除后找不回来");
    CHECK(disabled && pages > 1 && fits, "a wrapped filename is paginated, not clipped");
    key(g_profile.keys_mask & (1u << HAL_KEY_BACK) ? HAL_KEY_BACK : HAL_KEY_HOME); quiet();
    CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "a supported physical cancel key also cancels");
    open_confirm("另一份测试文件\n另一个对象\n内容已经更新\n删除后找不回来");
    CHECK(disabled && page == 1, "updated transaction contents must be read again");
    key(HAL_KEY_HOME); quiet(); CHECK(strcmp(ink_ui_top_name(), "confirm") && effects == 0, "HOME also cancels");
    open_confirm(long_text); on_ui(raise_call); quiet();
    swipe(w / 2, h - 4, w / 2, h * 3 / 4);
    swipe(4, h * 2 / 3, w / 2, h * 2 / 3); key(HAL_KEY_HOME);
    CHECK(!strcmp(ink_ui_top_name(), "interrupt") && effects == 0, "call gestures and HOME never fall through to confirmation");
    ink_interrupt_end(interrupt_id); quiet(); on_ui(read_view);
    CHECK(!strcmp(ink_ui_top_name(), "confirm") && page == 1 && disabled, "returning from interruption requires reading again");
    key(HAL_KEY_HOME); quiet();
    char oversized[221]; memset(oversized, 'x', sizeof oversized - 1); oversized[sizeof oversized - 1] = 0;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    ink_args_init(args); ink_args_str(args, "detail", oversized);
    ink_call_opts_t opts = {.src = INK_SRC_AI, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    CHECK(ink_cap_call("test.long_confirm", args, &opts) == 0, "submit oversized description"); osal_free(args);
    CHECK(WAIT_FOR(g_done, 3000) && g_rc.status == INK_RC_REJECTED, "an unrepresentable description is rejected before confirmation");
    ink_confirm_view_t view; CHECK(ink_confirm_list(&view, 1) == 0 && effects == 0, "no truncated transaction can be accepted");
}
static void slider_tests(void) {
    const char *ids[] = {"display.light", "display.warmth", "audio.volume"};
    const char *titles[] = {"亮度", "暖光", "音量"};
    for (int i = 0; i < 3; i++) {
        CHECK(BY_HAND(i < 2 ? "shell.display" : "shell.sound", NULL) && g_rc.status == INK_RC_DONE, "open real slider page"); quiet();
        wanted = titles[i]; on_ui(read_view);
        CHECK(have_slider, "find slider %s in the rendered tree", titles[i]);
        for (int drag = 0; drag < 2; drag++) for (int value = 0; value <= 10; value++) {
            int span = slider.x2 - slider.x1, y = (slider.y1 + slider.y2) / 2;
            int x = slider.x1 + (span * value + 5) / 10;
            if (value == 0) x++; if (value == 10) x--;
            if (drag) {
                hw(HW_PRESS, (slider.x1 + slider.x2) / 2, y); delay_ms(80);
                hw(HW_PRESS, x, y); delay_ms(80); hw(HW_RELEASE, x, y);
            } else tap(x, y);
            quiet();
            CHECK(ink_setting_get(ids[i]) == value * 10, "%s %s level %d maps to %d, got %lld", ids[i], drag ? "drag" : "tap", value, value * 10, (long long)ink_setting_get(ids[i]));
            on_ui(read_view);
        }
        key(HAL_KEY_HOME); quiet();
    }
}
static char context[4096];
static void read_context(void) {
    const char *name;
    ink_ui_snapshot(context, sizeof context, &name);
}
static void check_child_actions(void) {
    char *ids = osal_malloc(8192, OSAL_MEM_EXTERNAL);
    CHECK(ids && ink_cap_ids_json(ids, 8192) > 0, "read the current child's callable ids");
    if (ids) {
        CHECK(!strstr(ids, "\"shell.settings\"") && !strstr(ids, "\"member.set_pin\""), "the compact action list excludes disallowed and owner-only actions");
        osal_free(ids);
    }
}
static void add_test_networks(void *unused) {
    (void)unused;
    sim_wifi_ap_add("FAKE_UI_TEST_AP_1", -45, "");
    sim_wifi_ap_add("FAKE_UI_TEST_AP_2", -60, "");
}
static void context_tests(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), add_test_networks, NULL); delay_ms(5);
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open control through the bus"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "叫名字=") && strstr(context, "更多=0"), "control describes its current wake and expansion states");
    CHECK(AI("shell.more", NULL) && g_rc.status == INK_RC_DONE, "expand through the same capability"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "更多=1") && strstr(context, "暖光="), "expanded control includes visible warmth");
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open real network settings"); quiet();
    wanted = "找附近的 Wi-Fi"; on_ui(read_view);
    CHECK(label_x >= 0, "the scan control exists"); tap(label_x, label_y);
    bool found = false;
    for (int i = 0; i < 500 && !found; i++) { on_ui(read_context); found = strstr(context, "网络[0]=") != NULL; if (!found) delay_ms(20); }
    CHECK(found && strstr(context, "网络组=1") && strstr(context, "SIM="), "visible networks have stable selection indices and connection context");
    CHECK(AI("shell.wifi_pick", "{\"index\":0}") && g_rc.status == INK_RC_DONE, "the listed index selects the real network"); quiet();
    CHECK(ink_text_focused() && ink_text_type() == INK_TEXT_PASSWORD, "selection opens a human password field");
    CHECK(AI("text.insert", "{\"text\":\"FORBIDDEN\"}") && g_rc.status != INK_RC_DONE, "AI cannot type the network password");
    key(HAL_KEY_HOME); quiet();
    CHECK(AI("settings.set", "{\"id\":\"voice.handsfree\",\"value\":2}") && g_rc.status == INK_RC_DONE, "change the actual wake setting");
    CHECK(AI("settings.set", "{\"id\":\"display.lock_style\",\"value\":1}") && g_rc.status == INK_RC_DONE, "change the actual lock style");
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "return to the settings overview"); quiet();
    wanted = "叫名字已关闭"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the wake setting");
    wanted = "时钟和待办"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the lock style");
    /* A UI-only connection-state fixture; no pairing or real radio is used. */
    ink_state_set_bool(ink_state_find("bt.connected"), true); quiet();
    wanted = "已连接"; on_ui(read_view); CHECK(label_x >= 0, "a connection-state event updates the visible Bluetooth status");
    ink_state_set_bool(ink_state_find("bt.connected"), false); quiet();
    CHECK(ink_member_switch("kid") == 0, "switch to the real child space"); quiet();
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "child opens their own controls"); quiet(); on_ui(read_context);
    CHECK(!strstr(context, "shell.settings") && !strstr(context, "shell.members"), "the child snapshot does not advertise adult controls");
    on_ui(check_child_actions);
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_REJECTED, "a globally registered setting entry is actually denied to the child");
    CHECK(!strcmp(ink_ui_top_name(), "Control"), "the denied operation cannot navigate away");
}
static void test_task(void *unused) {
    (void)unused; delay_ms(1800); quiet();
    if (!strcmp(mode, "slider")) slider_tests();
    else if (!strcmp(mode, "context")) context_tests();
    else confirmation_tests();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    mode = getenv("INK_UI_REGRESSION"); if (!mode) mode = "confirm";
    product_main(); ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
    osal_task_cfg_t cfg = {.name = "ui-review", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI regression task");
}
