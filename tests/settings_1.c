/* settings-1: the settings home after AOSP (groups, every row with its value, search, the six states) and one registry
 * behind the rows, the search and settings.open. Touch and the AI walk the same path; a changed value redraws one row. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "../system/ui/shell/settings_registry.h"
#include "ink/hal_afe.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char view[3072], touched[3072];
static const char *wanted;
static bool seen, framed;
static int lx, ly, sx1, sx2, ry1, ry2, text_size;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void inspect(void) { CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "the page has an AI snapshot"); }
static void locate(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); lx = (a.x1 + a.x2) / 2; ly = (a.y1 + a.y2) / 2; seen = true;
        lv_obj_get_coords(lv_obj_get_parent(o), &a); sx1 = a.x1; sx2 = a.x2; ry1 = a.y1; ry2 = a.y2;
    }
    if (lv_obj_get_style_outline_width(o, 0) == INK_STROKE_FOCUS || /* a list row, or the frame the page draws */
        (lv_obj_get_style_border_width(o, 0) == ink_ui_px(INK_STROKE_FOCUS) && lv_obj_get_style_bg_opa(o, 0) == LV_OPA_TRANSP))
        framed = true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) { seen = framed = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static bool visible(const char *text) { wanted = text; on_ui(find); return seen; }
static void touch(const char *text) { CHECK(visible(text), "visible: %s", text); if (seen) tap(lx, ly); quiet(); }
static bool field_at(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_textarea_class)) { lv_area_t a; lv_obj_get_coords(c, &a); lx = (a.x1 + a.x2) / 2; ly = (a.y1 + a.y2) / 2; return true; }
        if (field_at(c)) return true;
    }
    return false;
}
static void find_field(void) { lv_obj_update_layout(lv_screen_active()); seen = field_at(lv_screen_active()); }
static bool captured;
static void capture_now(void *name) { sim_display_snapshot(name); captured = true; }
static void shot(const char *name) { /* the settled panel, saved as snap-<name>.png */
    quiet(); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture_now, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "captured %s", name);
}
/* A row of the snapshot: "value" and "state" after its id. */
static bool row(const char *snap, const char *id, const char *field, char *out, size_t cap) {
    char key[80]; snprintf(key, sizeof key, "{\"id\":\"%s\"", id);
    const char *at = strstr(snap, key), *end = at ? strchr(at, '}') : NULL;
    snprintf(key, sizeof key, "\"%s\":\"", field);
    const char *v = at ? strstr(at, key) : NULL;
    if (!v || v > end) return false;
    v += strlen(key);
    size_t n = (size_t)(strchr(v, '"') - v);
    snprintf(out, cap, "%.*s", (int)(n < cap ? n : cap - 1), v);
    return true;
}
static bool row_is(const char *id, const char *field, const char *want) {
    char got[96]; on_ui(inspect);
    return row(view, id, field, got, sizeof got) && !strcmp(got, want);
}
static bool on_page(const char *name) { return !strcmp(ink_ui_page_name(), name); }
static bool home_page_with(const char *id) { /* the home from its first page, turned until the row is on it */
    char key[64]; snprintf(key, sizeof key, "{\"id\":\"%s\"", id);
    if (!BY_HAND("settings.open", "{}") || g_rc.status != INK_RC_DONE) return false;
    quiet();
    for (int i = 0; i < 6; i++) {
        on_ui(inspect);
        if (strstr(view, key)) return true;
        if (!BY_HAND("shell.page_next", NULL) || g_rc.status != INK_RC_DONE) return false;
        quiet();
    }
    return false;
}
static const char *say(void) { return g_rc.say; }

static bool child_may_change(const char *id) { /* what the settings service lets a child change */
    const ink_setting_def_t *d;
    for (int i = 0; id && (d = ink_setting_at(i)); i++) if (!strcmp(d->id, id)) return d->child_may_change;
    return false;
}
/* Every row the home shows comes from the registry, and every row of it says what it is now. A parent's home is the
 * registry's home rows, a child's only its child rows, each one a setting the service lets a child change (D-137). */
static int walk_home(bool kid) {
    unsigned mask = kid ? INK_SR_KID : INK_SR_HOME;
    CHECK(BY_HAND("settings.open", "{}") && g_rc.status == INK_RC_DONE && on_page("Settings"), "the home opens");
    quiet();
    int pages = 0, rows = 0;
    for (int page = 0; page < 6; page++) {
        on_ui(inspect);
        const char *p = view;
        while ((p = strstr(p, "{\"id\":\""))) {
            char id[48], value[96]; p += 7;
            snprintf(id, sizeof id, "%.*s", (int)(strchr(p, '"') - p), p);
            const ink_sr_item_t *it = ink_sr_find(id);
            CHECK(it && (it->where & mask), "a home row is a home item of the registry: %s", id);
            CHECK(row(view, id, "value", value, sizeof value) && value[0], "every row says what it is now: %s", id);
            CHECK(!kid || (it && !it->adult && child_may_change(it->setting)), "a child's home has only what a child may change: %s", id);
            CHECK(kid || !(it && (it->where & INK_SR_KID) && !(it->where & INK_SR_HOME)), "a parent's home has no child-only row: %s", id);
            rows++;
        }
        pages++;
        if (strstr(view, "\"page_number\":") && atoi(strstr(view, "\"page_number\":") + 14) == atoi(strstr(view, "\"pages\":") + 8)) break;
        CHECK(BY_HAND("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "turn the home"); quiet();
    }
    int home = 0;
    for (int i = 0; ink_sr_at(i); i++) home += (ink_sr_at(i)->where & mask) && (!kid || !ink_sr_at(i)->adult);
    CHECK(rows >= home, "all the home items are drawn across %d pages (%d of %d)", pages, rows, home);
    CHECK(!kid || home == 8, "a child's home: brightness, warmth, volume, do-not-disturb, key tone, vibration, two rings (%d)", home);
    return pages;
}

/* One declaration (acceptance 3): each item is found by search under its name, named in settings.open's description, and
 * settings.open lands on its page with the row framed where that page can frame it. */
static void one_declaration(void) {
    const char *schema = ink_cap_find("settings.open") ? ink_cap_find("settings.open")->input : "";
    for (int i = 0; ink_sr_at(i); i++) {
        const ink_sr_item_t *it = ink_sr_at(i);
        char quoted[64], args[96], why[96];
        snprintf(quoted, sizeof quoted, "\"%s\"", it->id);
        CHECK(strstr(schema, quoted), "settings.open's description names %s", it->id);
        ink_sr_state_t s = ink_sr_value(it, why, sizeof why);
        if (s == INK_SR_AVAILABLE) {
            snprintf(args, sizeof args, "{\"query\":\"%s\"}", ink_tr(it->title));
            CHECK(AI("settings.search", args) && strstr(g_result, quoted), "the search finds %s by its name", it->id);
        }
        if (s != INK_SR_AVAILABLE && s != INK_SR_UNSEARCHABLE) continue;
        snprintf(args, sizeof args, "{\"id\":\"%s\"}", it->id);
        CHECK(AI("settings.open", args) && g_rc.status == INK_RC_DONE && on_page(it->page), "settings.open %s lands on %s (%s)", it->id, it->page, ink_ui_page_name());
        quiet();
        char page[48]; snprintf(page, sizeof page, " %s ", it->page);
        if ((it->where & INK_SR_LIST) || strstr(" SetDisplay SetSound SetNetwork ", page)) { /* the pages with its row */
            wanted = ""; on_ui(find); CHECK(framed, "%s is framed on its page", it->id);
        }
        CHECK(BY_HAND("system.home", NULL), "home again"); quiet();
    }
}

static void run(void *arg) {
    (void)arg;
    const char *size = getenv("INK_SETTINGS1_TEXT");
    text_size = size ? atoi(size) : 1;
    quiet();
    ink_receipt_t rc;
    CHECK(ink_setting_set("display.font_size", text_size, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "text size %d", text_size);
    CHECK(ink_setting_set("display.warmth", 30, INK_SRC_TOUCH, &rc) == INK_RC_DONE && ink_setting_set("display.light", 60, INK_SRC_TOUCH, &rc) == INK_RC_DONE,
          "known light and warmth");
    quiet();

    /* 1. The home: groups, every row with its value (AOSP top level). */
    CHECK(BY_HAND("shell.settings", NULL) && g_rc.status == INK_RC_DONE && on_page("Settings"), "settings opens on its home"); quiet();
    on_ui(inspect);
    CHECK(strstr(view, "\"group\":\"网络和连接\"") && visible("网络和连接") && visible("Wi-Fi"), "the first group and its rows: %.200s", view);
    shot("home");
    int pages = walk_home(false);
    sim_metric_set("settings1.home_pages", pages);
    CHECK(text_size != 1 || pages == 2, "the home fits two pages at the default text size (%d)", pages);
    shot("home-last");

    /* 1b. 系统 (D-137): the rarely used rows on one list page, under their groups and with their values; a tap on one
     * opens its page as it did from the home. The touch and the AI take settings.open {"id":"system"}. */
    char got[96];
    CHECK(home_page_with("system") && row_is("system", "capability", "settings.open") && row(view, "system", "value", got, sizeof got) &&
          strstr(got, ink_tr("存储")) && strstr(got, ink_tr("关于和更新")), "the home's 系统 says what it holds: %s", got);
    for (int i = 0; ink_sr_at(i); i++)
        CHECK(!(ink_sr_at(i)->where & INK_SR_SYSTEM) || !(ink_sr_at(i)->where & INK_SR_HOME), "a row is on the home or on 系统: %s", ink_sr_at(i)->id);
    touch("系统");
    on_ui(inspect); memcpy(touched, view, sizeof touched);
    CHECK(on_page("SetSystem"), "a tap on 系统 opens its page (%s)", ink_ui_page_name());
    int listed = 0, sys = 0;
    for (int i = 0; ink_sr_at(i); i++) {
        const ink_sr_item_t *it = ink_sr_at(i);
        char key[64]; snprintf(key, sizeof key, "{\"id\":\"%s\"", it->id);
        sys += (it->where & INK_SR_SYSTEM) != 0;
        if (!strstr(view, key)) continue;
        CHECK(it->where & INK_SR_SYSTEM, "系统 lists only its registry rows: %s", it->id);
        CHECK(row(view, it->id, "value", got, sizeof got) && got[0], "every 系统 row says what it is now: %s", it->id);
        listed++;
    }
    CHECK(strstr(view, "\"pages\":1,") && listed == sys && sys == 5, "家人, 语音和 AI, 存储, 备份, 关于和更新 on one page (%d of %d)", listed, sys);
    CHECK(visible("家人和孩子") && visible("存储和备份") && visible("关于和更新") && strstr(view, "\"capability\":\"settings.open\""),
          "under their groups, each opening its page: %.200s", view);
    shot("system");
    CHECK(BY_HAND("shell.back", NULL) && AI("settings.open", "{\"id\":\"system\"}") && g_rc.status == INK_RC_DONE && on_page("SetSystem"), "the AI opens 系统");
    quiet(); on_ui(inspect);
    const char *x = strstr(view, "\"rows\":["), *y = strstr(touched, "\"rows\":[");
    CHECK(x && y && !strcmp(x, y), "the AI and the touch see the same 系统 rows: %.150s", x ? x : "");
    touch("存储");
    CHECK(on_page("SetStorage"), "存储 opens 存储和备份 as it did from the home (%s)", ink_ui_page_name());
    CHECK(BY_HAND("system.home", NULL) && BY_HAND("settings.open", "{}") && on_page("Settings"), "the settings home again"); quiet();

    /* 2. Search 暖光 by touch: the field, the words, the result with where it is and what it is now. */
    on_ui(find_field); CHECK(seen, "the search field is there"); if (seen) tap(lx, ly); quiet();
    CHECK(ink_text_focused(), "a tap focuses the one search field");
    CHECK(AI("text.insert", "{\"text\":\"暖光\"}") && BY_HAND("text.submit", NULL), "type 暖光 and search, as the person does"); quiet();
    on_ui(inspect); memcpy(touched, view, sizeof touched);
    CHECK(strstr(view, "\"query\":\"暖光\"") && row_is("display.warmth", "value", "3") && visible("显示和阅读 > 暖光 · 3"),
          "the result says 显示和阅读 > 暖光 · 3: %.200s", view);
    shot("search");

    /* 3. The result opens its page on its row; one step more; back on the home the row says the new value. */
    touch("暖光");
    CHECK(on_page("SetDisplay") && visible("暖光"), "the result opens 显示和亮度 (%s)", ink_ui_page_name());
    wanted = ""; on_ui(find); CHECK(framed, "the row the search found is framed");
    shot("display");
    CHECK(visible("暖光"), "the warmth slider");
    if (seen) tap(sx1 + (sx2 - sx1) * 4 / 10 + 2, ly); /* the slider's fourth step */
    quiet();
    CHECK(ink_setting_get("display.warmth") == 40, "one step up through settings.set (%lld)", (long long)ink_setting_get("display.warmth"));
    CHECK(BY_HAND("shell.back", NULL), "back"); quiet();
    CHECK(on_page("Settings") && row_is("display.warmth", "value", "4") && visible("显示和阅读 > 暖光 · 4"), "the found row says 4 now");
    CHECK(home_page_with("display.warmth") && row_is("display.warmth", "value", "4"), "the home's 暖光 row says 4 now");
    shot("home-after");
    /* The alias finds it too. */
    CHECK(BY_HAND("settings.search", "{\"query\":\"色温\"}") && row_is("display.warmth", "value", "4"), "色温 finds 暖光"); quiet();

    /* 4. The refresh: a changed value is written into its row and nothing else on the page is drawn again (D-127). */
    CHECK(home_page_with("display.warmth"), "the home with 暖光");
    wanted = "4"; on_ui(find);
    CHECK(seen, "the 暖光 value on the home");
    sim_metric_set("settings1.row_y1", ry1); sim_metric_set("settings1.row_y2", ry2);
    ink_probe_mark("settings1.refresh.before");
    CHECK(AI("settings.set", "{\"id\":\"display.warmth\",\"value\":50}") && g_rc.status == INK_RC_DONE, "the AI changes the warmth");
    quiet();
    ink_probe_mark("settings1.refresh.after");
    CHECK(row_is("display.warmth", "value", "5") && visible("5"), "the row says 5");

    /* 5. Wi-Fi off says 已关闭; with airplane mode on, 移动网络 is grayed and says why; a dependent row in search too. */
    CHECK(BY_HAND("wifi.set_enabled", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "Wi-Fi off"); quiet();
    CHECK(home_page_with("wifi") && row_is("wifi", "value", "已关闭") && visible("已关闭"), "the Wi-Fi row says 已关闭");
    /* 移动网络 follows the modem's state (not a setting) while the home is open. */
    ink_state_set_bool(ink_state_find("cell.registered"), true); quiet();
    CHECK(row_is("cell", "value", "4G 在线") && visible("4G 在线"), "移动网络 says 4G 在线 once it registers");
    ink_state_set_bool(ink_state_find("cell.registered"), false); quiet();
    CHECK(row_is("cell", "value", "4G 还没有连上") && visible("4G 还没有连上"), "and 4G 还没有连上 again");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "airplane mode on"); quiet();
    CHECK(row_is("cell", "state", "dependent") && row_is("cell", "value", "飞行模式开着") && visible("飞行模式开着"),
          "移动网络 is grayed with the reason");
    char cap[48]; on_ui(inspect);
    CHECK(!row(view, "cell", "capability", cap, sizeof cap), "a grayed row offers no action");
    shot("dependent");
    CHECK(AI("settings.open", "{\"id\":\"cell\"}") && g_rc.status == INK_RC_REJECTED && strstr(say(), "飞行模式开着"), "the AI is told the same reason");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && BY_HAND("wifi.set_enabled", "{\"enabled\":true}"), "airplane off, Wi-Fi on"); quiet();
    CHECK(BY_HAND("settings.set", "{\"id\":\"audio.vibration\",\"value\":0}") && BY_HAND("settings.search", "{\"query\":\"振动\"}"), "vibration off");
    CHECK(row_is("audio.vibration_level", "state", "dependent") && row_is("audio.vibration_level", "value", "振动已关闭"), "振动强度 says why in search");
    CHECK(BY_HAND("settings.set", "{\"id\":\"audio.vibration\",\"value\":1}"), "vibration on"); quiet();
    /* The other two states: not here on this device (no wake-word front end in this profile), not now (SIM read-only). */
    if (!hal_afe_available())
        CHECK(AI("settings.search", "{\"query\":\"叫名字\"}") && !strstr(g_result, "\"voice.handsfree\"") && AI("settings.open", "{\"id\":\"voice.handsfree\"}") &&
              g_rc.status == INK_RC_REJECTED && strstr(say(), "这台设备暂不支持叫名字"), "UNSUPPORTED: not found, not opened, the reason told");
    ink_state_set_bool(ink_state_find("cell.config_readonly"), true);
    CHECK(AI("settings.search", "{\"query\":\"sim\"}") && !strstr(g_result, "\"cell.sim_slot\"") && strstr(g_result, "\"cell\""),
          "NOT_NOW: the SIM card choice is left out while it cannot change: %.150s", g_result);
    ink_state_set_bool(ink_state_find("cell.config_readonly"), false);
    CHECK(AI("settings.search", "{\"query\":\"sim\"}") && strstr(g_result, "\"cell.sim_slot\""), "and back when it can"); quiet();

    /* 6. The same path by the AI: the same results, the same page. */
    CHECK(AI("settings.search", "{\"query\":\"暖光\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"id\":\"display.warmth\""), "the AI searches: %.150s", g_result);
    CHECK(BY_HAND("settings.set", "{\"id\":\"display.warmth\",\"value\":30}"), "warmth back to 3"); quiet();
    on_ui(inspect);
    const char *a = strstr(view, "\"rows\":["), *b = strstr(touched, "\"rows\":[");
    CHECK(a && b && !strncmp(a, b, (size_t)(strstr(b, "]") - b)), "the AI and the field list the same rows: %.150s", a ? a : "");
    CHECK(AI("settings.open", "{\"id\":\"display.warmth\"}") && on_page("SetDisplay"), "the AI opens the same page");
    quiet(); wanted = ""; on_ui(find); CHECK(framed, "framed for the AI too");
    if (text_size == 1) one_declaration();

    /* 7. A child: only a child's rows; the parents' rows are found but say 家长管理, never their value. */
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet();
    walk_home(true);
    CHECK(BY_HAND("settings.open", "{}") && on_page("Settings"), "the child's home"); quiet();
    shot("kid");
    CHECK(AI("settings.search", "{\"query\":\"密码\"}") && strstr(g_result, "\"id\":\"family\"") && strstr(g_result, "家长管理") &&
          !strstr(g_result, "个人"), "a child finds 家人 by 密码 as 家长管理, without its value: %.200s", g_result);
    quiet();
    CHECK(row_is("family", "value", "家长管理") && row_is("family", "state", "parent") && visible("家人"), "drawn the same, grayed");
    shot("kid-search");
    CHECK(AI("settings.open", "{\"id\":\"family\"}") && g_rc.status == INK_RC_REJECTED && strstr(say(), "家长管理"), "and not opened for a child");
    CHECK(AI("settings.open", "{\"id\":\"system\"}") && g_rc.status == INK_RC_REJECTED && strstr(say(), "家长管理") && !on_page("SetSystem"),
          "nor 系统, the parents' rows");
    CHECK(AI("settings.open", "{\"id\":\"display.warmth\"}") && g_rc.status == INK_RC_DONE && on_page("SetDisplay") && visible("暖光") &&
          !visible("字号") && !visible("自动锁屏 10 分钟"), "the child's 暖光 opens a page without the parents' rows");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "settings-1", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
