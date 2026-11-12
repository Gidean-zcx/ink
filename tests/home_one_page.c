/* D-135, the one-page home: a tap and the AI reach the same entries, and each member keeps their own six. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char view[3072], first[3072];
static const char *wanted;
static bool seen;
static int lx, ly;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void inspect(void) { CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "the page has an AI snapshot"); }
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); lx = (a.x1 + a.x2) / 2; ly = (a.y1 + a.y2) / 2; seen = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) { seen = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); locate(lv_layer_sys()); }
static bool visible(const char *text) { wanted = text; on_ui(find); return seen; }
static bool field_at(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_textarea_class)) { lv_area_t a; lv_obj_get_coords(c, &a); lx = (a.x1 + a.x2) / 2; ly = (a.y1 + a.y2) / 2; return true; }
        if (field_at(c)) return true;
    }
    return false;
}
static void find_field(void) { lv_obj_update_layout(lv_screen_active()); seen = field_at(lv_screen_active()); }
static void touch(const char *text) { CHECK(visible(text), "visible: %s", text); if (seen) tap(lx, ly); quiet(); }
static void hold(const char *text) { /* a long press, as the person does it */
    CHECK(visible(text), "visible: %s", text);
    if (seen) { hw(HW_PRESS, lx, ly); delay_ms(650); hw(HW_RELEASE, lx, ly); }
    quiet();
}
static void home(void) { ink_ui_home(); }
static void go_home(void) { on_ui(home); quiet(); on_ui(inspect); }
static const char *field(const char *text, const char *name) { const char *at = strstr(text, name); return at ? at : ""; }
static bool same_apps(const char *a, const char *b) { /* the list the page names, up to the actions */
    const char *x = field(a, "应用="), *y = field(b, "应用="), *xe = strstr(x, " 动作："), *ye = strstr(y, " 动作：");
    return *x && *y && xe && ye && xe - x == ye - y && !strncmp(x, y, (size_t)(xe - x));
}
static bool open_by(ink_source_t src, const char *cap, const char *args) { return call_as(cap, args, src) && g_rc.status == INK_RC_DONE; }

static void run(void *arg) {
    (void)arg; quiet(); go_home();
    CHECK(strstr(view, "常用=电话（phone）；短信（message）；阅读（reader）；问 AI（ai）；相机（camera）；时钟（clock）；"),
          "the six default favourites, in order: %.200s", field(view, "常用="));
    /* One page: neither a swipe nor a page key nor the AI turns it. */
    hw(HW_PRESS, 560, 600); delay_ms(100); hw(HW_PRESS, 120, 600); delay_ms(100); hw(HW_RELEASE, 120, 600); quiet();
    key(HAL_KEY_NEXT); quiet();
    CHECK(!strcmp(ink_shell_board(), "Main"), "a swipe and the page key leave the one page as it is");
    CHECK(!AI("home.next", NULL) || g_rc.status != INK_RC_DONE, "nothing turns home screens any more");
    /* A favourite: the tap and the AI open the same app. */
    touch("时钟"); CHECK(!strcmp(ink_ui_page_name(), "Clock"), "the clock favourite opens the clock (%s)", ink_ui_page_name());
    go_home(); CHECK(open_by(INK_SRC_AI, "shell.launch", "{\"id\":\"clock\"}") && !strcmp(ink_ui_page_name(), "Clock"), "the AI opens the same clock");
    /* 全部应用: the same second page by tap and by the AI. */
    go_home(); touch("全部应用"); on_ui(inspect); memcpy(first, view, sizeof first);
    CHECK(!strcmp(ink_ui_page_name(), "Apps") && strstr(view, "工具页=1/"), "全部应用 opens all apps on its first page");
    CHECK(BY_HAND("shell.back", NULL), "back"); go_home();
    CHECK(open_by(INK_SRC_AI, "shell.apps", NULL) && !strcmp(ink_ui_page_name(), "Apps"), "the AI opens the same page"); on_ui(inspect);
    CHECK(same_apps(first, view), "both see the same first page of apps");
    /* Search: the field and the AI list the same matches. */
    CHECK(open_by(INK_SRC_AI, "shell.search", "{\"query\":\"计算\"}"), "the AI searches"); on_ui(inspect); memcpy(first, view, sizeof first);
    CHECK(strstr(view, "计算器（calculator）") && !strstr(view, "时钟（clock）") && visible("计算器"), "only the matches are listed and drawn");
    CHECK(open_by(INK_SRC_AI, "shell.apps", NULL), "start again"); quiet();
    on_ui(find_field); CHECK(seen, "the search field is there"); if (seen) tap(lx, ly); quiet();
    CHECK(ink_text_focused(), "a tap focuses the field");
    CHECK(AI("text.insert", "{\"text\":\"计算\"}") && BY_HAND("text.submit", NULL), "type and submit, as the person does"); quiet();
    on_ui(inspect); CHECK(same_apps(first, view), "the field lists exactly what the AI's search listed");
    /* The six: off by a long press on the home, on by a long press in all apps; the AI does the same through one capability. */
    go_home(); hold("相机");
    CHECK(!strcmp(ink_ui_top_name(), "IconMenu") && visible("放在主屏"), "a long press opens the menu with 放在主屏");
    touch("放在主屏"); on_ui(inspect);
    CHECK(!strcmp(ink_ui_top_name(), "home") && !strstr(view, "相机（camera）") && !visible("相机"), "off the home, by touch");
    CHECK(open_by(INK_SRC_AI, "shell.search", "{\"query\":\"计算\"}"), "all apps, found"); hold("计算器"); touch("放在主屏");
    go_home(); CHECK(strstr(view, "时钟（clock）；计算器（calculator）；"), "on the home at the end, by touch: %.200s", field(view, "常用="));
    CHECK(AI("shell.favorite", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_REJECTED && strstr(g_rc.say, "6"), "a seventh is refused with the reason");
    CHECK(AI("shell.favorite", "{\"id\":\"calculator\"}") && g_rc.status == INK_RC_DONE, "the AI takes one off");
    CHECK(AI("shell.favorite", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_DONE, "the AI puts one on"); go_home();
    CHECK(strstr(view, "常用=电话（phone）；短信（message）；阅读（reader）；问 AI（ai）；时钟（clock）；相机（camera）；"), "the AI's changes are on the home: %.200s", field(view, "常用="));
    CHECK(AI("data.get", "{\"collection\":\"home.today\",\"id\":1}") && strstr(g_result, "camera"), "the six are kept for this member: %.120s", g_result);
    /* Another member keeps the defaults. */
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet(); go_home();
    CHECK(strstr(view, "页面=KidHome") && strstr(view, "常用=电话（phone）；短信（message）；阅读（reader）；"), "the child has their own six: %.200s", field(view, "常用="));
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "home-one-page", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
