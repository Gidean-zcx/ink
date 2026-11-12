/* The calculator on the real pages: keys by touch, the same sums by the AI, the Apps page, search and 20 extra apps. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char snapshot[3072], scene[24];
static const char *wanted_text;
static bool wanted_key, contains;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the real page settles"); }
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page fits the production AI snapshot budget");
}
static void expect(const char *text) { on_ui(read_snapshot); CHECK(strstr(snapshot, text), "snapshot contains %s: %.150s", text, snapshot); }
static void find_in(lv_obj_t *o) {
    if (found || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o), *cap = ink_shell_bound_cap(lv_obj_get_parent(o));
        bool match = contains ? strstr(t, wanted_text) != NULL : !strcmp(t, wanted_text);
        if (match && wanted_key == (cap && !strcmp(cap, "calc.key"))) { found = o; return; }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_in(lv_obj_get_child(o, (int32_t)i));
}
static void find_job(void) { found = NULL; lv_obj_update_layout(lv_screen_active()); find_in(lv_screen_active()); }
static lv_obj_t *find(const char *text, bool key, bool part) { wanted_text = text; wanted_key = key; contains = part; on_ui(find_job); return found; }
static void tap_obj(lv_obj_t *o) {
    lv_area_t a; lv_obj_get_coords(o, &a);
    CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height(), "the target fits the screen");
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); quiet();
}
/* The keys in the page's order; the signs, 删除 and +/- are icons there, named here as people read them. */
static const char *const KEY_NAMES[20] = {"C", "删除", "%", "÷", "7", "8", "9", "×", "4", "5", "6", "-",
                                           "1", "2", "3", "+", "+/-", "0", ".", "="};
static int key_wanted, key_seen;
static void nth_key(lv_obj_t *o) {
    for (uint32_t i = 0; !found && i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        const char *cap = ink_shell_bound_cap(c);
        if (cap && !strcmp(cap, "calc.key") && key_seen++ == key_wanted) found = c;
    }
}
static void nth_key_job(void) { found = NULL; key_seen = 0; lv_obj_update_layout(lv_screen_active()); nth_key(lv_screen_active()); }
/* Press the real keys, one at a time. */
static void keys(const char *const *labels) {
    for (; *labels; labels++) {
        key_wanted = -1;
        for (int i = 0; i < 20; i++) if (!strcmp(KEY_NAMES[i], *labels)) key_wanted = i;
        on_ui(nth_key_job);
        CHECK(key_wanted >= 0 && found, "key %s is on screen", *labels);
        if (found) tap_obj(found);
    }
}
static void result_is(const char *text) {
    lv_obj_t *label = find(text, false, false);
    CHECK(label, "the screen shows %s", text);
    expect(text);
}
static void ai_same(const char *expr, const char *result) {
    char args[160]; snprintf(args, sizeof args, "{\"expr\":\"%s\"}", expr);
    CHECK(AI("calc.eval", args) && g_rc.status == INK_RC_DONE, "AI evaluates %s (%s)", expr, ink_rc_name(g_rc.status));
    char want[96]; snprintf(want, sizeof want, "\"result\":\"%s\"", result);
    CHECK(strstr(g_result, want), "AI gets %s for %s: %.150s", result, expr, g_result);
    quiet();
}
static void ai_refused(const char *expr, const char *say) {
    char args[160]; snprintf(args, sizeof args, "{\"expr\":\"%s\"}", expr);
    CHECK(AI("calc.eval", args) && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, say), "AI is told %s for %s: %.100s", say, expr, g_rc.say);
    quiet();
}
static void capture_event(void *arg) { CHECK(settled(), "the captured frame is presented"); sim_display_snapshot(arg); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, (void *)name); delay_ms(5); }
static void go_home(void) { CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "back home"); quiet(); }
/* From home to all apps (D-135: its second page, behind 全部应用), then turn its pages until the row named `title`
 * shows; tap the row. */
static void first_apps_page(void) {
    go_home();
    CHECK(BY_HAND("shell.apps", NULL) && g_rc.status == INK_RC_DONE, "全部应用 opens"); quiet();
    on_ui(read_snapshot);
    CHECK(strstr(snapshot, "工具页=1/"), "all apps opens on its first page");
}
static bool open_from_apps(const char *title) {
    first_apps_page();
    for (int page = 0; page < 12; page++) {
        lv_obj_t *label = find(title, false, false);
        if (label) {
            lv_obj_t *row = lv_obj_get_parent(label);
            CHECK(ink_shell_bound_cap(row) && !strcmp(ink_shell_bound_cap(row), "shell.launch"), "the row of %s is tappable", title);
            tap_obj(row);
            return true;
        }
        key(HAL_KEY_NEXT); quiet();
    }
    return false;
}

static void calculator(void) {
    CHECK(open_from_apps("计算器"), "the Apps page lists 计算器");
    CHECK(!strcmp(ink_ui_page_name(), "Calculator"), "its tile opens the calculator (%s)", ink_ui_page_name());
    expect("\"算式\""); expect("calc.eval"); capture("CalcEmpty");

    keys((const char *const[]){"0", ".", "1", "+", "0", ".", "2", NULL}); expect("0.1+0.2"); result_is("0.3"); capture("CalcTyping");
    keys((const char *const[]){"=", NULL}); result_is("0.3"); expect("0.1+0.2 =");
    ai_same("0.1+0.2", "0.3"); capture("CalcResult");
    keys((const char *const[]){"C", "2", "+", "3", "×", "4", "=", NULL}); result_is("14");
    keys((const char *const[]){"-", "4", "=", NULL}); result_is("10"); ai_same("2+3×4-4", "10");
    keys((const char *const[]){"C", "1", "÷", "3", "=", NULL}); result_is("0.33333333"); ai_same("1/3", "0.33333333");
    keys((const char *const[]){"C", "2", "÷", "3", "=", NULL}); result_is("0.66666667");
    keys((const char *const[]){"C", "2", "0", "0", "+", "1", "0", "%", "=", NULL}); result_is("220"); ai_same("200+10%", "220");
    keys((const char *const[]){"C", "5", "0", "×", "1", "0", "%", "=", NULL}); result_is("5"); ai_same("50*10%", "5");
    keys((const char *const[]){"C", "5", "+/-", "×", "3", "=", NULL}); result_is("-15"); ai_same("-5*3", "-15");
    keys((const char *const[]){"+/-", NULL}); result_is("15");
    keys((const char *const[]){"C", "1", "2", "3", "删除", "=", NULL}); result_is("12");
    keys((const char *const[]){"C", "5", "÷", "0", "=", NULL}); result_is("不能除以 0"); capture("CalcDivZero");
    ai_refused("5/0", "不能除以 0");
    keys((const char *const[]){"1", "+", "1", "=", NULL}); result_is("2"); /* after an error, digits start again */
    ai_refused("9999999999*10", "超出范围"); ai_refused("1+", "算式不完整");
    ai_same("9999999999.99999999", "9999999999.99999999");
    CHECK(AI("calc.key", "{\"key\":\"clear\"}") && g_rc.status == INK_RC_DONE, "AI presses clear through the same key capability"); quiet();
    const char *const ai_keys[] = {"7", "*", "6", "="};
    for (unsigned i = 0; i < 4; i++) {
        char args[32]; snprintf(args, sizeof args, "{\"key\":\"%s\"}", ai_keys[i]);
        CHECK(AI("calc.key", args) && g_rc.status == INK_RC_DONE, "AI presses %s", ai_keys[i]); quiet();
    }
    result_is("42"); expect("7×6 ="); expect("\"结果\"");
    keys((const char *const[]){"C", "9", "9", "9", "9", "9", "9", "9", "9", "9", "9", "9", "9", NULL});
    result_is("9999999999"); /* ten digits, the eleventh and twelfth are not taken */
    keys((const char *const[]){"×", "9", "9", "=", NULL}); result_is("超出范围，整数最多 10 位"); capture("CalcRange");
    keys((const char *const[]){"C", NULL});
    for (int i = 0; i < 16; i++) keys((const char *const[]){"1", ".", "5", i % 2 ? "×" : "+", NULL});
    lv_obj_t *expr = find("1.5+1.5×", false, true);
    CHECK(expr && strlen(lv_label_get_text(expr)) >= 64, "the long expression is shown whole");
    if (expr) {
        lv_area_t e, r; lv_obj_t *result = find("18.75", false, false);
        lv_obj_get_coords(expr, &e);
        CHECK(result, "the result so far is shown");
        lv_point_t size; lv_text_get_size(&size, lv_label_get_text(expr), lv_obj_get_style_text_font(expr, 0), 0, 0, lv_obj_get_width(expr), LV_TEXT_FLAG_NONE);
        CHECK(size.y <= lv_obj_get_height(expr) && lv_obj_get_height(expr) > ink_ui_px(INK_LINE_SUBTITLE), "it wraps onto more lines and none is cut (%d > %d)", (int)size.y, (int)lv_obj_get_height(expr));
        const char *text = lv_label_get_text(expr), *nl = strchr(text, '\n');
        CHECK(nl, "the long expression has its own line breaks");
        for (; nl; nl = strchr(nl + 1, '\n'))
            CHECK(nl[1] == '+' || nl[1] == '-' || !strncmp(nl + 1, "×", 2) || !strncmp(nl + 1, "÷", 2), "a line starts with an operator, no number is split: %.40s", nl + 1);
        CHECK(e.y1 > ink_ui_px(INK_STATUS_BAR + INK_PRIMARY), "it stays under the title bar");
        if (result) { lv_obj_get_coords(result, &r); CHECK(e.y2 < r.y1, "it stays above the result"); }
    }
    capture("CalcLong");
    keys((const char *const[]){"=", NULL}); expect("\"结果\"");
    go_home();
    CHECK(AI("search.query", "{\"query\":\"计算器\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"calculator\"") && strstr(g_result, "应用"),
          "global search finds the calculator app: %.150s", g_result);
    CHECK(AI("search.show", "{\"query\":\"计算\"}") && g_rc.status == INK_RC_DONE, "search page for 计算"); quiet();
    CHECK(WAIT_FOR(strstr((on_ui(read_snapshot), snapshot), "计算器") != NULL, 5000), "the search page lists 计算器");
    lv_obj_t *hit = find("计算器", false, false);
    CHECK(hit, "the search result is visible");
    if (hit) tap_obj(lv_obj_get_parent(hit));
    CHECK(!strcmp(ink_ui_page_name(), "Calculator"), "the search result opens the calculator (%s)", ink_ui_page_name());
}

/* 20 extra apps: the Apps page turns pages, each is reachable and opens. */
static ink_activity_t TOOL_PAGES[20];
static ink_app_def_t TOOL_APPS[20];
static char tool_ids[20][16], tool_titles[20][32];
static void tool_create(lv_obj_t *screen, void *arg) { (void)arg; ink_shell_statusbar(screen, true); ink_shell_topbar(screen, ink_ui_page_name()); }
static int tool_snapshot(char *out, size_t cap) { return snprintf(out, cap, "{\"page\":\"%s\"}", ink_ui_page_name()); }
static void register_tools(void) {
    for (int i = 0; i < 20; i++) {
        snprintf(tool_ids[i], sizeof tool_ids[i], "tool%02d", i + 1);
        snprintf(tool_titles[i], sizeof tool_titles[i], "工具%02d", i + 1);
        TOOL_PAGES[i] = (ink_activity_t){.name = tool_titles[i], .app = tool_ids[i], .create = tool_create, .snapshot = tool_snapshot};
        TOOL_APPS[i] = (ink_app_def_t){.id = tool_ids[i], .title = tool_titles[i], .icon = INK_UI_ICON(grid), .entry = &TOOL_PAGES[i], .child_ok = true};
        CHECK(ink_app_register(&TOOL_APPS[i]) == 0, "register synthetic app %d", i + 1);
    }
}
static void twenty(void) {
    for (int i = 0; i < 20; i++) {
        CHECK(open_from_apps(tool_titles[i]), "the Apps page reaches %.31s", tool_titles[i]);
        CHECK(!strcmp(ink_ui_page_name(), tool_titles[i]), "%.31s opens (%.31s)", tool_titles[i], ink_ui_page_name());
    }
    CHECK(open_from_apps("计算器") && !strcmp(ink_ui_page_name(), "Calculator"), "the calculator is still there");
    first_apps_page(); expect("工具页=1/"); capture("AppsFirst");
    key(HAL_KEY_NEXT); quiet(); expect("工具页=2/"); capture("AppsSecond");
}

static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); quiet(); test_start_home(); quiet();
    if (!strcmp(scene, "apps20")) twenty();
    else calculator();
    ink_probe_count("test.finished");
}
static void host_scene(void *arg) { (void)arg; const char *name = getenv("INK_CALC_SCENE"); snprintf(scene, sizeof scene, "%s", name ? name : ""); }
void ink_main(void) {
    vtk_host_call(host_scene, NULL); product_main();
    if (!strcmp(scene, "apps20")) register_tools();
    osal_task_cfg_t cfg = {.name = "calculator-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test-only driver");
}
