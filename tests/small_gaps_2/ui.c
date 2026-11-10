/* Small UI regressions through the real product, synthetic data and touch. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static char scene[24], snapshot[8192];
static const char *wanted;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "page settles"); }
static void call(const char *cap, const char *args) {
    CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s completes: %s", cap, ink_rc_name(g_rc.status)); quiet();
}
static void read_page(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "AI snapshot fits"); }
static void locate(lv_obj_t *obj) {
    if (found || !lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) found = obj;
    for (uint32_t i = 0; !found && i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int32_t)i));
}
static void find_label(void) { found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static void touch(const char *text) {
    wanted = text; on_ui(find_label); CHECK(found, "visible touch target: %s", text);
    if (found) { lv_area_t a; lv_obj_get_coords(found, &a); tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); }
    quiet();
}
static void capture(void *ctx) { sim_display_snapshot(ctx); }
static void frame(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
static void font_size(int size) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", size); call("settings.set", args);
}

static unsigned icons_seen;
static void inspect_icons(void) {
    static const char *const titles[] = {"时钟", "相机", "相册"}, *const names[] = {"clock", "camera", "gallery"};
    for (unsigned i = 0; i < sizeof titles / sizeof *titles; i++) {
        wanted = titles[i]; find_label();
        if (!found) continue;
        lv_obj_t *circle = lv_obj_get_child(lv_obj_get_parent(found), (int32_t)lv_obj_get_index(found) - 1);
        CHECK(circle && ink_shell_bound_cap(circle) && !strcmp(ink_shell_bound_cap(circle), "shell.launch"), "app has a real tile: %s", names[i]);
        lv_obj_t *expected = ink_ui_icon(lv_screen_active(), names[i], INK_ICON_LIST, INK_ICON_LINE, 0, 0, false);
        lv_obj_t *image = circle ? lv_obj_get_child(circle, 0) : NULL;
        CHECK(image && lv_obj_check_type(image, &lv_image_class) && lv_image_get_src(image) == lv_image_get_src(expected), "Apps tile uses its own packed icon: %s", names[i]);
        lv_obj_delete(expected); icons_seen |= 1u << i;
    }
}
static void icons(void) {
    call("shell.search", "{\"query\":\"\"}");
    for (int i = 0; i < 4; i++) {
        on_ui(inspect_icons); char name[32]; snprintf(name, sizeof name, "apps-%d", i + 1); frame(name);
        if (icons_seen == 7) break;
        call("home.next", NULL);
    }
    CHECK(icons_seen == 7, "all three icons appear on Apps");
}

static void inspect_phone(void) {
    wanted = "没有完成，请重试"; find_label(); CHECK(found, "dial error is visible");
    if (!found) return;
    lv_area_t notice; lv_obj_get_coords(found, &notice);
    static const char *const digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
    for (unsigned i = 0; i < sizeof digits / sizeof *digits; i++) {
        wanted = digits[i]; find_label(); CHECK(found, "digit remains visible: %s", digits[i]);
        if (!found) continue;
        lv_area_t key; lv_obj_get_coords(lv_obj_get_parent(found), &key);
        CHECK(key.y2 - key.y1 + 1 == ink_ui_px(INK_DIGIT) && key.x2 - key.x1 + 1 == ink_ui_px(INK_DIGIT), "dial keys retain their full size");
        CHECK(notice.x2 < key.x1 || notice.x1 > key.x2 || notice.y2 < key.y1 || notice.y1 > key.y2, "notice does not overlap digit %s", digits[i]);
    }
    CHECK(notice.x1 >= 0 && notice.x2 < ink_ui_width() && notice.y1 >= 0 && notice.y2 < ink_ui_height(), "notice fits the screen");
}
static void phone(void) {
    for (int level = 1; level <= 3; level += 2) {
        font_size(level); call("phone.open", NULL);
        tap(INK_CANVAS_W / 2, INK_CANVAS_H - INK_MARGIN - INK_DIGIT / 2); quiet();
        on_ui(inspect_phone); frame(level == 1 ? "phone-100" : "phone-120");
    }
}

static lv_area_t time_rows[5], time_buttons[10];
static bool compare;
static int buttons;
static void inspect_buttons(lv_obj_t *obj) {
    const char *cap = ink_shell_bound_cap(obj);
    if (cap && !strcmp(cap, compare ? "reminder.draft" : "shell.time_step")) {
        CHECK(buttons < 10, "picker has ten step buttons");
        if (buttons < 10) {
            lv_area_t a; lv_obj_get_coords(obj, &a);
            if (!compare) time_buttons[buttons] = a;
            else CHECK(!memcmp(&a, &time_buttons[buttons], sizeof a), "both pickers share button geometry #%d", buttons);
        }
        buttons++;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) inspect_buttons(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect_picker(void) {
    static const char *const rows[] = {"年 2028", "月 02", "日 28", "时 10", "分 20"};
    for (unsigned i = 0; i < sizeof rows / sizeof *rows; i++) {
        wanted = rows[i]; find_label(); CHECK(found, "shared date row is visible: %s", rows[i]);
        if (!found) continue;
        lv_area_t a; lv_obj_get_coords(found, &a);
        if (!compare) time_rows[i] = a;
        else CHECK(!memcmp(&a, &time_rows[i], sizeof a), "both pickers share date row geometry #%u", i);
    }
    buttons = 0; inspect_buttons(lv_screen_active()); CHECK(buttons == 10, "all date and time controls are present");
}
static void picker(void) {
    call("settings.set", "{\"id\":\"time.auto\",\"value\":0}");
    ink_tm_t t = {.year = 2028, .month = 2, .day = 28, .hour = 10, .min = 20};
    char args[64]; snprintf(args, sizeof args, "{\"utc_s\":%lld}", (long long)ink_time_from_local(&t)); call("time.set", args);
    call("shell.time_edit", NULL); on_ui(inspect_picker); frame("time-picker");
    call("system.home", NULL); call("reminder.new", NULL);
    call("reminder.draft", "{\"year\":2028,\"month\":2,\"day\":28,\"hour\":10,\"minute\":20}");
    call("reminder.time", NULL); compare = true; on_ui(inspect_picker); frame("reminder-picker");
    touch("+"); on_ui(read_page); CHECK(strstr(snapshot, "2029-02-28"), "shared year button still uses reminder.draft");
}

static lv_obj_t *expression, *result;
static lv_area_t key_rect;
static void inspect_calculator(void) {
    expression = result = NULL;
    lv_obj_t *root = lv_screen_active();
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *obj = lv_obj_get_child(root, (int32_t)i); const char *cap = ink_shell_bound_cap(obj);
        if (cap && !strcmp(cap, "calc.key")) { lv_obj_get_coords(obj, &key_rect); break; }
        if (!lv_obj_check_type(obj, &lv_label_class)) continue;
        if (strstr(lv_label_get_text(obj), "1.5")) expression = obj;
        if (!strcmp(lv_label_get_text(obj), "18.75")) result = obj;
    }
    CHECK(expression && result, "expression and result are visible");
    if (!expression || !result) return;
    lv_area_t e, r; lv_obj_get_coords(expression, &e); lv_obj_get_coords(result, &r);
    lv_point_t need; lv_text_get_size(&need, lv_label_get_text(expression), lv_obj_get_style_text_font(expression, 0), 0, 0, lv_obj_get_width(expression), LV_TEXT_FLAG_NONE);
    CHECK(need.y <= lv_obj_get_height(expression), "120 percent expression is not clipped (%d <= %d)", (int)need.y, (int)lv_obj_get_height(expression));
    CHECK(e.y1 >= ink_ui_px(INK_STATUS_BAR + INK_PRIMARY) && e.y2 < r.y1 && r.y2 < key_rect.y1, "expression, result and keypad do not overlap");
}
static void calculator(void) {
    font_size(3); call("app.open", "{\"id\":\"calculator\"}");
    call("calc.eval", "{\"expr\":\"1.5+1.5*1.5+1.5*1.5+1.5*1.5+1.5*1.5+1.5*1.5+1.5*1.5+1.5*1.5+1.5\"}");
    on_ui(inspect_calculator); frame("calculator-120");
}

static void airplane(void) {
    call("net.set_airplane", "{\"enabled\":true}");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane"), 4000), "airplane mode is on");
    call("shell.control", NULL); call("shell.more", NULL); frame("airplane-control-on");
    touch("飞行模式");
    CHECK(WAIT_FOR(!ink_setting_get("net.airplane"), 4000), "touch disables airplane mode");
    CHECK(strcmp(ink_ui_top_name(), "Control"), "control panel closes after disabling airplane mode"); frame("airplane-control-off");
}
static void host_scene(void *ctx) { (void)ctx; snprintf(scene, sizeof scene, "%s", getenv("INK_SMALL_GAPS_CASE")); }
static void run(void *ctx) {
    (void)ctx; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    if (!strcmp(scene, "icons")) icons();
    else if (!strcmp(scene, "phone")) phone();
    else if (!strcmp(scene, "picker")) picker();
    else if (!strcmp(scene, "calculator")) calculator();
    else if (!strcmp(scene, "airplane")) airplane();
    else CHECK(false, "known acceptance case");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_scene, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "test.small-gaps-2", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start acceptance driver");
}
