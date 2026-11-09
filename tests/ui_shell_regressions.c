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
static bool disabled, fits, pager_fits;
static char visible[160];
static lv_obj_t *accept;
static lv_area_t slider;
static bool have_slider;
static int slider_icons;
static int screen_w, screen_h, interrupt_id;
static int pager_bottom, accept_top;
static void effect(ink_call_t *c) { (void)c; effects++; }
static const ink_cap_t CAPS[] = {
    {.id = "test.long_confirm", .title = "删除测试文件", .ask = "{detail}", .effect = INK_FX_LOCAL_DESTRUCTIVE, .child_ok = true,
     .input = "{\"type\":\"object\",\"properties\":{\"detail\":{\"type\":\"string\",\"maxLength\":512}},\"required\":[\"detail\"]}", .run = effect}
};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static int count_icons(lv_obj_t *root) {
    int count = lv_obj_check_type(root, &lv_image_class) ? 1 : 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) count += count_icons(lv_obj_get_child(root, (int32_t)i));
    return count;
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), text);
        if (found) return found;
    }
    return NULL;
}
static void check_segments(void) {
    const char *const labels[] = {"小", "中", "大", "特大"};
    lv_obj_t *root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    for (int count = 2; count <= 4; count++) for (int selected = 0; selected < count; selected += count - 1) {
        lv_obj_t *row = ink_ui_segments(root, labels, count, selected, INK_MARGIN, INK_MARGIN,
                                         INK_CANVAS_W - 2 * INK_MARGIN, NULL, NULL);
        lv_obj_update_layout(root);
        lv_area_t outer; lv_obj_get_coords(row, &outer);
        int gap = ink_ui_px(INK_GAP_XS);
        for (int i = 0; i < count; i++) {
            lv_area_t inner; lv_obj_get_coords(lv_obj_get_child(row, i), &inner);
            CHECK(abs(inner.y1 - outer.y1 - gap) <= 1 && abs(outer.y2 - inner.y2 - gap) <= 1,
                  "%d-way segment %d leaves the top and bottom outlines visible", count, i);
            CHECK(inner.x1 >= outer.x1 + gap - 1 && inner.x2 <= outer.x2 - gap + 1,
                  "%d-way segment %d stays within both rounded ends", count, i);
        }
        lv_obj_delete(row);
    }
    lv_obj_delete(root);
}
static void check_search_layout(void) {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *label = find_label(lv_screen_active(), "查找应用");
    CHECK(label, "the real Apps search field is visible");
    if (!label) return;
    lv_obj_t *field = lv_obj_get_parent(label);
    CHECK(count_icons(field) == 1, "search includes exactly one search icon");
    lv_area_t text, bounds; lv_obj_get_coords(label, &text); lv_obj_get_coords(field, &bounds);
    for (uint32_t i = 0; i < lv_obj_get_child_count(field); i++) {
        lv_obj_t *icon = lv_obj_get_child(field, (int32_t)i);
        if (!lv_obj_check_type(icon, &lv_image_class)) continue;
        lv_area_t a; lv_obj_get_coords(icon, &a);
        CHECK(a.x1 >= bounds.x1 && a.x2 < text.x1 && a.y1 >= bounds.y1 && a.y2 <= bounds.y2,
              "the search icon fits the field without overlapping editable text");
    }
}
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
                lv_point_t size;
                lv_text_get_size(&size, s, lv_obj_get_style_text_font(o, 0), lv_obj_get_style_text_letter_space(o, 0),
                                 lv_obj_get_style_text_line_space(o, 0), lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
                pager_fits = size.y <= ink_ui_px(INK_LINE_LIST) && size.x <= lv_obj_get_width(o);
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
                if (i > 0) {
                    lv_obj_t *icon = lv_obj_get_child(root, (int32_t)i - 1);
                    const char *cap = ink_shell_bound_cap(icon);
                    if (cap && !strcmp(cap, "shell.launch")) {
                        lv_area_t tile; lv_obj_get_coords(icon, &tile);
                        label_x = (tile.x1 + tile.x2) / 2; label_y = (tile.y1 + tile.y2) / 2;
                    }
                }
                while (parent && !lv_obj_has_flag(parent, LV_OBJ_FLAG_CLICKABLE)) parent = lv_obj_get_parent(parent);
                if (parent) { lv_obj_get_coords(parent, &slider); have_slider = true; slider_icons = count_icons(parent); }
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
    on_ui(check_segments); quiet();
    const char *ids[] = {"display.light", "display.warmth", "audio.volume"};
    const char *titles[] = {"亮度", "暖光", "音量"};
    for (int i = 0; i < 3; i++) {
        CHECK(BY_HAND(i < 2 ? "shell.display" : "shell.sound", NULL) && g_rc.status == INK_RC_DONE, "open real slider page"); quiet();
        wanted = titles[i]; on_ui(read_view);
        CHECK(have_slider, "find slider %s in the rendered tree", titles[i]);
        CHECK(slider_icons >= 2, "%s includes its normal and reversed icon for either slider fill", titles[i]);
        for (int drag = 0; drag < 2; drag++) for (int value = 0; value <= 10; value++) {
            int span = slider.x2 - slider.x1, y = (slider.y1 + slider.y2) / 2;
            int x = slider.x1 + (span * value + 5) / 10;
            if (value == 0) x++;
            if (value == 10) x--;
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
    CHECK(AI("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "set the actual automatic lock time");
    CHECK(AI("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "return to the settings overview"); quiet();
    wanted = "叫名字已关闭"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the wake setting");
    wanted = "10 分钟"; on_ui(read_view); CHECK(label_x >= 0, "the overview reflects the automatic lock time");
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
static ink_exec_job_t blocked_job, drained_job;
static volatile bool bg_started, bg_release, bg_drained;
static void block_background(void *arg) {
    (void)arg;
    __atomic_store_n(&bg_started, true, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&bg_release, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
}
static void background_drained(void *arg) { (void)arg; __atomic_store_n(&bg_drained, true, __ATOMIC_RELEASE); }
static void member_privacy_tests(void) {
    CHECK(BY_HAND("shell.search", "{\"query\":\"PRIVATE_OWNER_TEST_QUERY\"}") && g_rc.status == INK_RC_DONE, "the adult searches through the real capability"); quiet();
    CHECK(BY_HAND("shell.display", NULL) && g_rc.status == INK_RC_DONE, "leave home in the same member's session"); quiet();
    on_ui(ink_ui_home); quiet(); on_ui(read_context);
    CHECK(strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "ordinary navigation preserves the current member's query");
    CHECK(BY_HAND("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open real network settings"); quiet();
    CHECK(ink_exec_post(INK_EXEC_BG, &blocked_job, block_background, NULL) == 0 && WAIT_FOR(__atomic_load_n(&bg_started, __ATOMIC_ACQUIRE), 2000), "hold the background executor before the scan starts");
    wanted = "找附近的 Wi-Fi"; on_ui(read_view);
    CHECK(label_x >= 0, "find the real scan button"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(BY_HAND("shell.members", NULL) && g_rc.status == INK_RC_DONE, "open the actual member picker"); quiet();
    CHECK(BY_HAND("shell.member_choose", NULL) && g_rc.status == INK_RC_DONE, "continue from the member handoff preview"); quiet();
    wanted = "小"; on_ui(read_view);
    CHECK(label_x >= 0, "find the child's avatar"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(ink_member_current()->id, "kid"), "touch really switches to the child");
    on_ui(read_context);
    CHECK(!strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "the child snapshot has no adult query");
    CHECK(!strcmp(ink_shell_board(), "Main"), "a different member starts on their main page");
    wanted = "PRIVATE_OWNER_TEST_QUERY"; on_ui(read_view);
    CHECK(label_x < 0, "the child screen has no adult query");
    __atomic_store_n(&bg_release, true, __ATOMIC_RELEASE);
    CHECK(ink_exec_post(INK_EXEC_BG, &drained_job, background_drained, NULL) == 0 && WAIT_FOR(__atomic_load_n(&bg_drained, __ATOMIC_ACQUIRE), 5000), "the old member's queued scan has returned");
    quiet(); on_ui(read_context);
    CHECK(!strstr(context, "没能完成，可以再试一次") && !strstr(context, "已经找过附近的网络"), "a late scan receipt cannot change the new member's result");
    CHECK(BY_HAND("home.next", NULL) && g_rc.status == INK_RC_DONE, "inspect the child's own apps page"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "查找=；") && !strstr(context, "PRIVATE_OWNER_TEST_QUERY"), "the child's search field is empty on Apps too");
    wanted = "PRIVATE_OWNER_TEST_QUERY"; on_ui(read_view); CHECK(label_x < 0, "Apps does not reveal the old query");
    CHECK(ink_member_switch("owner") == 0, "return to the adult without reusing the old session"); quiet();
    CHECK(BY_HAND("shell.network", NULL) && g_rc.status == INK_RC_DONE, "reopen networks after switching back"); quiet(); on_ui(read_context);
    CHECK(!strstr(context, "网络[0]=") && strstr(context, "选中网络=；"), "network results and selection do not cross sessions");
}
static lv_obj_t *answer_root, *answer_status, *answer_question, *answer_label;
static const char *answer_words;
static char answer_text[4096] INK_PSRAM_BSS, answer_seen[4096] INK_PSRAM_BSS;
static bool answer_fits, answer_next_disabled, answer_prev_disabled;
static lv_obj_t *bound(lv_obj_t *root, const char *id) {
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, id)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = bound(lv_obj_get_child(root, (int32_t)i), id);
        if (found) return found;
    }
    return NULL;
}
static void open_answer(void) { ink_shell_ai_open(&answer_root, &answer_status, &answer_question, &answer_label); }
static void update_answer(void) { ink_shell_ai_words(answer_words); ink_shell_ai_status("回答完成"); }
static void close_answer(void) { ink_shell_ai_closed(); lv_obj_delete(answer_root); answer_root = NULL; }
static void read_answer(void) {
    read_view();
    const char *text = lv_label_get_text(answer_label);
    snprintf(answer_text, sizeof answer_text, "%s", text);
    lv_point_t size;
    lv_text_get_size(&size, text, lv_obj_get_style_text_font(answer_label, 0), lv_obj_get_style_text_letter_space(answer_label, 0),
                     lv_obj_get_style_text_line_space(answer_label, 0), lv_obj_get_width(answer_label), LV_TEXT_FLAG_NONE);
    answer_fits = size.y <= lv_obj_get_height(answer_label);
    lv_obj_t *next = bound(answer_root, "ai.panel.next"), *prev = bound(answer_root, "ai.panel.prev");
    answer_next_disabled = !next || lv_obj_has_state(next, LV_STATE_DISABLED);
    answer_prev_disabled = !prev || lv_obj_has_state(prev, LV_STATE_DISABLED);
    if (next) { lv_area_t a; lv_obj_get_coords(next, &a); next_x = (a.x1 + a.x2) / 2; next_y = (a.y1 + a.y2) / 2; }
    if (prev) { lv_area_t a; lv_obj_get_coords(prev, &a); label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2; }
}
static void check_long_layout(void) {
    lv_obj_update_layout(answer_root);
    int height = ink_ui_height() - lv_obj_get_y(answer_root);
    CHECK(height * 100 >= ink_ui_height() * 53 && height * 100 <= ink_ui_height() * 55,
          "a long answer expands to about 54 percent of the screen");
    CHECK(lv_obj_get_height(answer_label) >= ink_ui_px(5 * ink_ui_line_height(INK_FONT_BODY)), "the expanded answer has five body lines");
    lv_obj_t *source = find_label(answer_root, "出自这一页");
    lv_obj_t *next = bound(answer_root, "ai.panel.next");
    CHECK(source && next && lv_obj_get_y(answer_label) + lv_obj_get_height(answer_label) <= lv_obj_get_y(source) &&
          lv_obj_get_y(source) + lv_obj_get_height(source) <= lv_obj_get_y(lv_obj_get_parent(next)) &&
          lv_obj_get_y(lv_obj_get_parent(next)) + lv_obj_get_height(lv_obj_get_parent(next)) <= height,
          "expanded text, source and paging controls fit without overlap");
}
static void check_short_layout(void) {
    lv_obj_update_layout(answer_root);
    int height = ink_ui_height() - lv_obj_get_y(answer_root);
    CHECK(height * 100 >= ink_ui_height() * 42 && height * 100 <= ink_ui_height() * 44 &&
          lv_obj_get_height(answer_label) == ink_ui_px(3 * ink_ui_line_height(INK_FONT_BODY)), "a short answer returns to the compact panel");
}
static void traverse_answer(const char *text) {
    answer_words = text; on_ui(open_answer); on_ui(update_answer); quiet(); on_ui(read_answer);
    on_ui(check_long_layout);
    CHECK(page == 1 && pages > 1 && answer_prev_disabled, "a long answer has a visible page count and starts on its first page");
    answer_seen[0] = 0;
    int total = pages;
    for (int i = 1; i <= total && i < 200; i++) {
        CHECK(page == i && answer_fits && answer_text[0], "answer page %d fits the actual font and label", i);
        size_t used = strlen(answer_seen), size = strlen(answer_text);
        CHECK(used + size < sizeof answer_seen, "the traversed fixture fits its complete-text buffer");
        if (used + size < sizeof answer_seen) memcpy(answer_seen + used, answer_text, size + 1);
        on_ui(read_context);
        static char escaped[8192]; ink_jw_t writer; ink_jw_init(&writer, escaped, sizeof escaped); ink_jw_str(&writer, answer_text);
        CHECK(!writer.overflow && strstr(context, escaped) && strstr(context, "页码="), "AI sees the displayed page and page count in F1 JSON");
        if (i < total) {
            CHECK(!answer_next_disabled, "a further page has an enabled next button");
            if (i % 2) tap(next_x, next_y);
            else CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "AI uses the same next-page capability");
            quiet(); on_ui(read_answer);
        }
    }
    CHECK(!strcmp(answer_seen, text), "complete traversal preserves every byte, without missing or repeated text");
    CHECK(answer_next_disabled, "the last page disables forward navigation");
    CHECK(BY_HAND("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "an extra next-page request is bounded");
    quiet(); on_ui(read_answer); CHECK(page == total, "forward navigation stays on the last page");
    if (total > 1) { tap(label_x, label_y); quiet(); on_ui(read_answer); CHECK(page == total - 1, "the visible previous button goes back"); }
    for (int i = total - 1; i > 1; i--) {
        CHECK(AI("ai.panel.prev", NULL) && g_rc.status == INK_RC_DONE, "AI can return through previous pages");
        quiet(); on_ui(read_answer); CHECK(page == i - 1 && answer_fits, "previous page is reachable without clipping");
    }
    on_ui(close_answer); quiet();
}
static void answer_tests(void) {
    traverse_answer("第一行\n第二行\n第三行\n第四行\n第五行\n第六行\n第七行\n第八行");
    static char long_text[1800] INK_PSRAM_BSS;
    for (int i = 0; i < 30; i++) strcat(long_text, "中文答案 mixed words 123，阅读这一页。 ");
    traverse_answer(long_text);
    answer_words = "第一行\n第二行\n第三行\n第四行\n第五行\n第六行\n第七行\n第八行"; on_ui(open_answer); on_ui(update_answer); quiet();
    CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_DONE, "advance before replacing streamed content");
    answer_words = "回答"; on_ui(update_answer); quiet(); on_ui(read_answer);
    on_ui(check_short_layout);
    CHECK(page == 1 && pages == 1 && !strcmp(answer_text, "回答") && answer_next_disabled && answer_prev_disabled, "shorter streamed content clamps the page and updates both buttons");
    static char many_lines[4096] INK_PSRAM_BSS;
    memset(many_lines, '\n', sizeof many_lines - 1);
    answer_words = many_lines; on_ui(update_answer); quiet(); on_ui(read_answer);
    CHECK(pages >= 1000 && pager_fits, "four-digit page counts fit the visible pager on one line");
    on_ui(close_answer); quiet();
    CHECK(AI("ai.panel.next", NULL) && g_rc.status == INK_RC_STALE, "a closed answer cannot be paginated");
}
static void tool_page(lv_obj_t *screen, void *arg) { (void)screen; (void)arg; }
static const ink_activity_t TOOL_PAGE_A = {.name = "fake-tool-a", .app = "tool_a", .create = tool_page};
static const ink_activity_t TOOL_PAGE_B = {.name = "fake-tool-b", .app = "tool_b", .create = tool_page};
static const ink_app_def_t TOOL_A = {.id = "tool_a", .title = "工具甲", .entry = &TOOL_PAGE_A, .child_ok = true};
static const ink_app_def_t TOOL_B = {.id = "tool_b", .title = "工具乙", .entry = &TOOL_PAGE_B, .child_ok = true};
static const ink_text_client_t TEXT_CLIENT = {0};
static ink_text_type_t keyboard_type;
static char first_candidate[40], picked_text[128];
static lv_area_t keyboard_bounds;
static lv_obj_t *keyboard_panel(void) {
    lv_obj_t *root = find_label(lv_layer_sys(), "空格");
    for (int i = 0; i < 4 && root; i++) root = lv_obj_get_parent(root);
    CHECK(root && !strcmp(ink_ui_top_name(), "Keyboard"), "the real keyboard panel exists");
    return root;
}
static void remember_keyboard(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *root = keyboard_panel();
    if (root) lv_obj_get_coords(root, &keyboard_bounds);
}
static void check_keyboard_bounds(void) {
    lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *root = keyboard_panel();
    if (!root) return;
    lv_area_t current; lv_obj_get_coords(root, &current);
    CHECK(current.x1 == keyboard_bounds.x1 && current.x2 == keyboard_bounds.x2 &&
          current.y1 == keyboard_bounds.y1 && current.y2 == keyboard_bounds.y2,
          "candidate, edit and password states preserve the same keyboard area");
}
static void focus_keyboard(void) {
    ink_text_cfg_t cfg = {.type = keyboard_type, .initial = keyboard_type == INK_TEXT_PASSWORD ? "FAKE_UI_SECRET" : "测试文字", .max = 128};
    CHECK(ink_text_focus(&cfg, &TEXT_CLIENT, NULL) > 0, "focus the real text service");
}
static void blur_keyboard(void) { ink_text_blur(0); }
static void read_candidate(void) {
    ink_text_cand_t candidates[INK_TEXT_PAGE];
    int n = ink_text_candidates(0, candidates, INK_TEXT_PAGE);
    CHECK(n > 0, "the synthetic pinyin input has real candidates");
    snprintf(first_candidate, sizeof first_candidate, "%s", n ? candidates[0].word : "");
}
static void compose_xian(void) {
    for (const char *p = "xian"; *p; p++) {
        char args[32]; snprintf(args, sizeof args, "{\"code\":%d}", (int)*p);
        CHECK(AI("keyboard.key", args) && g_rc.status == INK_RC_DONE, "type through the keyboard capability");
    }
    quiet();
}
static void snapshot_tests(void) {
    CHECK(BY_HAND("home.prev", NULL) && g_rc.status == INK_RC_DONE, "open Today through its real action"); quiet();
    wanted = "继续听"; on_ui(read_view); CHECK(label_x >= 0, "Today describes listening as continue listening");
    wanted = "今日报纸"; on_ui(read_view); CHECK(label_x >= 0, "Today labels today's newspaper explicitly");
    CHECK(BY_HAND("shell.search", "{\"query\":\"\"}") && g_rc.status == INK_RC_DONE, "open Apps with a two-page tool fixture"); quiet(); on_ui(read_context);
    on_ui(check_search_layout);
    CHECK(strstr(context, "工具页=1/2") && strstr(context, "工具甲（tool_a）") && strstr(context, "短信（message）"), "Apps includes the visible registered, built-in and fixed-group entries");
    CHECK(!strstr(context, "设置（settings）") && strstr(context, "对话记录（ai）"), "Apps includes registered AI and omits settings on the next page");
    wanted = "工具甲"; on_ui(read_view); CHECK(label_x >= 0, "the advertised tool is visible"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "fake-tool-a"), "touch launches the advertised stable id");
    on_ui(ink_ui_home); quiet();
    CHECK(AI("home.next", NULL) && g_rc.status == INK_RC_DONE, "turn the tools page through the same capability"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "工具页=2/2") && strstr(context, "设置（settings）") && !strstr(context, "工具甲（tool_a）") && strstr(context, "相机（camera）"), "the next-page snapshot matches only its visible tools");
    CHECK(AI("shell.launch", "{\"id\":\"settings\"}") && g_rc.status == INK_RC_DONE, "AI can use the visible settings id"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "Settings"), "the advertised id opens actual settings");
    on_ui(ink_ui_home); quiet();
    CHECK(AI("shell.search", "{\"query\":\"工具甲\"}") && g_rc.status == INK_RC_DONE, "filter Apps through its capability"); quiet(); on_ui(read_context);
    CHECK(strstr(context, "工具甲（tool_a）") && !strstr(context, "工具乙（tool_b）") && !strstr(context, "短信（message）"), "search applies the identical visible-item filter to the snapshot");
    CHECK(AI("shell.launch", "{\"id\":\"tool_a\"}") && g_rc.status == INK_RC_DONE, "AI launches the same tool as touch"); quiet(); CHECK(!strcmp(ink_ui_page_name(), "fake-tool-a"), "touch and AI reach the same real activity");
    on_ui(ink_ui_home); quiet();
    const char *titles[] = {"测试通知甲", "测试通知乙", "测试通知丙"}; uint32_t ids[3] = {0};
    for (int i = 0; i < 3; i++) {
        ink_notify_t n = {.title = titles[i], .body = "测试通知内容", .prio = INK_NOTIFY_LOW, .src = INK_SRC_TOUCH};
        CHECK(ink_notify_post(&n, &ids[i]) == 0 && ids[i], "post a synthetic notification to the real service");
    }
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "open the populated control center"); quiet(); on_ui(read_context);
    for (int i = 0; i < 3; i++) {
        wanted = titles[i]; on_ui(read_view);
        CHECK((label_x >= 0) == (i > 0) && (strstr(context, titles[i]) != NULL) == (i > 0), "control notification %d agrees between screen and snapshot", i);
    }
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ids[2]);
    CHECK(AI("notify.read", args) && g_rc.status == INK_RC_DONE, "AI marks the displayed notification read"); quiet(); on_ui(read_context);
    char token[64]; snprintf(token, sizeof token, "通知[%u]=", (unsigned)ids[2]);
    CHECK(strstr(context, token) && strstr(context, "已读=1"), "control preserves the stable notification id and its read state");
    CHECK(AI("shell.notifications", NULL) && g_rc.status == INK_RC_DONE, "open the full notification list"); quiet(); on_ui(read_context);
    for (int i = 0; i < 3; i++) CHECK(strstr(context, titles[i]), "the full visible list exposes notification %d", i);
    on_ui(ink_ui_home); quiet(); keyboard_type = INK_TEXT_PLAIN; on_ui(focus_keyboard); quiet(); on_ui(remember_keyboard);
    compose_xian(); on_ui(check_keyboard_bounds);
    CHECK(AI("keyboard.candidates", "{}") && g_rc.status == INK_RC_DONE, "expand the real candidates through the bus"); quiet(); on_ui(check_keyboard_bounds);
    CHECK(AI("keyboard.candidates", "{\"fold\":true}") && g_rc.status == INK_RC_DONE, "fold candidates through the same capability"); quiet(); on_ui(check_keyboard_bounds);
    on_ui(read_candidate); on_ui(read_context);
    snprintf(token, sizeof token, "候选[0]=%s", first_candidate);
    CHECK(first_candidate[0] && strstr(context, token) && strstr(context, "光标=") && strstr(context, "选区="), "keyboard describes its actual candidate id, cursor and selection");
    wanted = first_candidate; on_ui(read_view); CHECK(label_x >= 0, "the described candidate is visible"); if (label_x >= 0) tap(label_x, label_y); quiet();
    snprintf(picked_text, sizeof picked_text, "%s", ink_text_value());
    on_ui(focus_keyboard); compose_xian();
    CHECK(AI("keyboard.pick", "{\"index\":0}") && g_rc.status == INK_RC_DONE, "AI selects the candidate id from the snapshot"); quiet();
    CHECK(!strcmp(picked_text, ink_text_value()), "touch and AI commit the same candidate");
    CHECK(AI("text.select", "{\"start\":1,\"end\":3}") && g_rc.status == INK_RC_DONE, "select real text through the bus"); quiet(); on_ui(read_context);
    on_ui(check_keyboard_bounds);
    CHECK(strstr(context, "选区=1..3") && !strstr(context, "候选["), "the editing snapshot reports selection and omits hidden candidates");
    keyboard_type = INK_TEXT_PASSWORD; on_ui(focus_keyboard); quiet(); on_ui(read_context);
    on_ui(check_keyboard_bounds);
    CHECK(strstr(context, "******") && !strstr(context, "FAKE_UI_SECRET") && !strstr(context, "候选[") && !strstr(context, "选区="), "password snapshots remain masked without candidates or selection");
    on_ui(blur_keyboard); quiet();
}
static bool alarm_title_font, alarm_time_font;
static void read_alarm_fields(void) {
    lv_obj_t *root = lv_layer_sys();
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *panel = lv_obj_get_child(root, (int32_t)i);
        for (uint32_t j = 0; j < lv_obj_get_child_count(panel); j++) {
            lv_obj_t *body = lv_obj_get_child(panel, (int32_t)j);
            for (uint32_t k = 0; k < lv_obj_get_child_count(body); k++) {
                lv_obj_t *o = lv_obj_get_child(body, (int32_t)k);
                if (!lv_obj_check_type(o, &lv_label_class)) continue;
                const char *s = lv_label_get_text(o);
                if (!strcmp(s, "起床")) alarm_title_font = lv_obj_get_style_text_font(o, 0) == ink_ui_font_px(INK_FONT_TITLE, INK_WEIGHT_BOLD);
                if (!strcmp(s, "07:30")) alarm_time_font = lv_obj_get_style_text_font(o, 0) == ink_ui_font_px(INK_FONT_NUMBER, INK_WEIGHT_REGULAR);
            }
        }
    }
}
static void alarm_tests(void) {
    ink_tm_t tm = {.year = 2026, .month = 11, .day = 5, .hour = 7, .min = 29, .sec = 58};
    ink_time_set(ink_time_from_local(&tm), false);
    ink_alarm_t a = {.kind = INK_ALARM, .at_utc = ink_time_now_s() + 2, .src = INK_SRC_TOUCH};
    snprintf(a.title, sizeof a.title, "起床");
    CHECK(ink_alarm_add(&a) > 0 && WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "a real synthetic alarm rings at 07:30"); quiet(); on_ui(read_context); on_ui(read_alarm_fields);
    CHECK(strstr(context, "起床") && strstr(context, "时间=07:30") && strstr(context, "日期=") && strstr(context, "11月5日"), "alarm snapshot separates its title, actual time and date");
    CHECK(alarm_title_font && alarm_time_font, "Chinese alarm title uses title type while time uses the numeric font");
    wanted = "再睡 10 分钟"; on_ui(read_view); CHECK(label_x >= 0, "the snooze button states its actual duration"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(active_state(), "none"), "touch snoozes the real alarm");
    int cursor = 0; ink_alarm_t saved;
    CHECK(ink_alarm_next(&cursor, INK_ALARM, &saved) == 1 && saved.id == a.id && saved.snooze_until - ink_time_now_s() >= 598 && saved.snooze_until - ink_time_now_s() <= 600, "the same service schedules ten minutes, not its nine-minute default");
    CHECK(ink_alarm_cancel(a.id) == 0, "remove the snoozed fixture");
    a.id = 0; a.at_utc = ink_time_now_s() + 2;
    CHECK(ink_alarm_add(&a) > 0 && WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "ring another real alarm to check dismiss"); quiet();
    wanted = "这次不再响"; on_ui(read_view); CHECK(label_x >= 0, "dismiss states that it affects this occurrence"); if (label_x >= 0) tap(label_x, label_y); quiet();
    CHECK(!strcmp(active_state(), "none"), "touch dismisses the actual alarm");
}
static void test_task(void *unused) {
    (void)unused; delay_ms(1800); quiet();
    if (!strcmp(mode, "slider")) slider_tests();
    else if (!strcmp(mode, "context")) context_tests();
    else if (!strcmp(mode, "privacy")) member_privacy_tests();
    else if (!strcmp(mode, "answer")) answer_tests();
    else if (!strcmp(mode, "snapshot")) snapshot_tests();
    else if (!strcmp(mode, "alarm")) alarm_tests();
    else confirmation_tests();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    mode = getenv("INK_UI_REGRESSION"); if (!mode) mode = "confirm";
    product_main(); test_start_home(); ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
    if (!strcmp(mode, "snapshot")) { ink_app_register(&TOOL_A); ink_app_register(&TOOL_B); }
    osal_task_cfg_t cfg = {.name = "ui-review", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("UI regression task");
}
