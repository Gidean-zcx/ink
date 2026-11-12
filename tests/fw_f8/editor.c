/* F8 exercises production cards, gestures, capabilities and member storage. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include <stdlib.h>

static bool restarting;
static unsigned opens;
static unsigned launches;
static ink_source_t opened_by;
static char snapshot[3072], context[3072];
static const char *wanted;
static lv_obj_t *label;
static int x, y;
static void empty(lv_obj_t *s, void *arg) { (void)s; (void)arg; launches++; }
static const ink_activity_t LISTEN_PAGE = {.name = "example.listen", .app = "listen", .create = empty};
static void resume(ink_call_t *c) { opens++; opened_by = c->src; ink_ui_push(&LISTEN_PAGE, NULL); }
static const ink_cap_t RESUME = {.id = "example.listen_resume", .title = "接着听合成内容", .child_ok = true,
    .effect = INK_FX_NAVIGATE, .run = resume};
static int fixture(ink_card_t *c, const char *id, const char *title) {
    snprintf(c->id, sizeof c->id, "%s", id); snprintf(c->title, sizeof c->title, "%s", title);
    snprintf(c->text, sizeof c->text, "%s合成%s", ink_member_current()->role == INK_ROLE_CHILD ? "孩子" : "成人", title);
    c->rev = 1; return 1;
}
static int listen_card(int n, ink_card_t *c) {
    if (n) return 0;
    snprintf(c->action, sizeof c->action, "%s", RESUME.id); return fixture(c, "resume", "听");
}
static int news_card(int n, ink_card_t *c) { return n ? 0 : fixture(c, "daily", "报纸"); }
static int word_card(int n, ink_card_t *c) { return n ? 0 : fixture(c, "review", "单词"); }
static int extra_card(int n, ink_card_t *c) { return n > 1 ? 0 : fixture(c, n ? "b" : "a", n ? "合成卡乙" : "合成卡甲"); }
static const ink_app_def_t APPS[] = {
    {.id = "listen", .title = "听", .icon = "headphones", .entry = &LISTEN_PAGE, .child_ok = true, .card = listen_card},
    {.id = "news", .title = "报纸", .icon = "news", .card = news_card},
    {.id = "word", .title = "单词", .icon = "word", .child_ok = true, .card = word_card},
    {.id = "example", .title = "合成应用", .icon = "grid", .child_ok = true, .card = extra_card}};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void inspect(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "snapshot fits the production AI buffer");
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strcmp(snapshot, context), "AI and UI share the same page snapshot");
}
static void find_in(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) label = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_in(lv_obj_get_child(o, (int)i));
}
static void find(void) {
    label = NULL; find_in(lv_screen_active()); find_in(lv_layer_sys()); x = y = -1;
    if (!label) return;
    lv_obj_update_layout(label); lv_area_t a; lv_obj_get_coords(label, &a);
    x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
}
static void touch(const char *text) {
    wanted = text; on_ui(find); CHECK(label, "visible touch target: %s", text);
    if (label) tap(x, y);
    quiet();
}
static bool has_editor_binding(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.today_edit")) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        if (has_editor_binding(lv_obj_get_child(o, (int)i))) return true;
    return false;
}
static void check_today_entry(void) {
    CHECK(!has_editor_binding(lv_screen_active()), "Today has no invisible editor touch region");
}
static void check_settings_entry(void) {
    inspect();
    CHECK(strstr(snapshot, "shell.today_edit"), "display settings snapshot exposes the existing editor capability");
    wanted = "今日屏放什么"; find();
    CHECK(label, "display settings has a visible Today editor row");
    if (!label) return;
    lv_obj_t *row = lv_obj_get_parent(label);
    const char *cap = ink_shell_bound_cap(row);
    CHECK(cap && !strcmp(cap, "shell.today_edit"), "settings row and AI share the existing editor capability");
    lv_area_t a; lv_obj_get_coords(row, &a);
    CHECK(a.y1 >= 0 && a.y2 < ink_ui_height() && lv_obj_get_height(row) >= ink_ui_px(INK_CONTROL),
        "the complete settings row fits the screen with the standard touch height");
}
static void home(void) { ink_ui_home(); }
static void child_space(void) { CHECK(ink_member_switch("kid") == 0, "switch to child"); }
/* D-135: the one page shows the first two chosen cards that have content, in their order. */
static void check_order(void) {
    inspect();
    const char *b = strstr(snapshot, "合成卡乙：成人合成合成卡乙"), *listen = strstr(snapshot, "听：成人合成听");
    CHECK(b && listen && b < listen && !strstr(snapshot, "成人合成单词") && !strstr(snapshot, "成人合成合成卡甲") &&
        !strstr(snapshot, "成人合成报纸"), "the home shows the first two chosen cards in their exact order");
    const char *contents[] = {"成人合成合成卡乙", "成人合成听"};
    for (unsigned i = 0; i < sizeof contents / sizeof *contents; i++) {
        wanted = contents[i]; find();
        CHECK(label, "selected card content is really drawn: %s", contents[i]);
    }
}
/* All four chosen, in order, as the editor numbers them. */
static void check_saved_order(void) {
    inspect();
    const char *want[] = {"\"id\":\"example:b\",\"role\":\"card\",\"label\":\"合成卡乙\",\"value\":\"第 1 张\"",
        "\"id\":\"listen:resume\",\"role\":\"card\",\"label\":\"听\",\"value\":\"第 2 张\"",
        "\"id\":\"word:review\",\"role\":\"card\",\"label\":\"单词\",\"value\":\"第 3 张\"",
        "\"id\":\"example:a\",\"role\":\"card\",\"label\":\"合成卡甲\",\"value\":\"第 4 张\""};
    for (unsigned i = 0; i < sizeof want / sizeof *want; i++) CHECK(strstr(snapshot, want[i]), "four selected cards retain their exact order: %s", want[i]);
}
static void check_child_default(void) {
    inspect();
    CHECK(strstr(snapshot, "听：孩子合成听") && !strstr(snapshot, "合成卡乙") && !strstr(snapshot, "成人合成"),
        "adult choices and adult card data do not affect the child's default home");
}
static void check_child_saved(void) {
    inspect();
    CHECK(strstr(snapshot, "合成卡甲：孩子合成合成卡甲") && !strstr(snapshot, "听：孩子合成听") && !strstr(snapshot, "成人合成"),
        "child has an independent saved selection after restart");
}
static void snapshot_event(void *name) { sim_display_snapshot(name); }
static void capture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name); delay_ms(5); }
static void save_snap(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    CHECK(n > 0 && (size_t)n < sizeof path, "evidence path fits");
    FILE *f = fopen(path, "w");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void record_editor(void) {
    inspect();
    wanted = "报纸"; find();
    CHECK(label, "editor shows an actual provider title");
    if (label) {
        lv_obj_t *note = lv_obj_get_child(lv_obj_get_parent(label), 2);
        lv_area_t title_area, note_area; lv_obj_update_layout(label);
        lv_obj_get_coords(label, &title_area); lv_obj_get_coords(note, &note_area);
        CHECK(title_area.y2 < note_area.y1, "editor title and saved order never overlap");
    }
    vtk_host_call(save_snap, "TodayEdit");
}
static void record_menu(void) {
    inspect();
    CHECK(!strcmp(ink_ui_top_name(), "IconMenu") && strstr(snapshot, RESUME.id) && strstr(snapshot, "shell.today_card"),
        "IconMenu exports the app's own shortcut and the shared card capability");
    wanted = "关闭"; find();
    CHECK(label, "menu has a visible close button");
    if (label) {
        lv_area_t a; lv_obj_update_layout(label); lv_obj_get_coords(lv_obj_get_parent(label), &a);
        CHECK(a.y2 < ink_ui_height() && a.y1 >= 0, "the entire close control fits above the screen edge");
    }
    vtk_host_call(save_snap, "IconMenu");
}
static void locate_listen_icon(void) { /* the row in all apps, which takes the tap on its name */
    wanted = "听"; find();
    CHECK(label, "listen row has its original label");
}
static void locate_up_b(void) {
    wanted = "合成卡乙"; find();
    CHECK(label, "selected B remains visible");
    if (label) {
        lv_obj_t *row = lv_obj_get_parent(label); label = NULL; wanted = "上移"; find_in(row);
        if (label) { lv_area_t a; lv_obj_update_layout(label); lv_obj_get_coords(label, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2; }
    }
    CHECK(label, "selected cards expose the up control");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_NET); delay_ms(1800); quiet();
    if (!restarting) {
        ink_alarm_t alarm = {.kind = INK_ALARM, .at_utc = ink_time_now_s() + 3600, .hour = 7, .min = 45, .src = INK_SRC_TOUCH};
        snprintf(alarm.title, sizeof alarm.title, "合成待办");
        CHECK(ink_alarm_add(&alarm) > 0, "seed the existing upcoming card provider");
        on_ui(home); quiet(); on_ui(ink_ui_redraw); quiet(); on_ui(inspect);
        CHECK(strstr(snapshot, "卡片=接下来：合成待办 7:45；听：成人合成听"), "the unconfigured home shows the next thing first, then listening");
        on_ui(check_today_entry);
        CHECK(BY_HAND("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "open Settings"); quiet();
        CHECK(!strcmp(ink_ui_page_name(), "Settings"), "start the visible editor path in Settings");
        touch("显示和亮度");
        CHECK(!strcmp(ink_ui_page_name(), "SetDisplay"), "Settings row opens display settings");
        for (int page = 0; page < 3; page++) {
            wanted = "今日屏放什么"; on_ui(find);
            if (label) break;
            CHECK(BY_HAND("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "page to the display settings editor row"); quiet();
        }
        on_ui(check_settings_entry); capture("SetDisplayToday");
        touch("今日屏放什么");
        CHECK(!strcmp(ink_ui_page_name(), "TodayEdit"), "visible settings row opens TodayEdit by touch");
        on_ui(record_editor); capture("TodayEdit"); ink_probe_count("test.snapshot.TodayEdit");
        touch("报纸");
        CHECK(AI("shell.today_card", "{\"card\":\"home:upcoming\"}") && g_rc.status == INK_RC_DONE, "AI removes the same kind of card through the shared capability"); quiet();
        touch("合成卡甲");
        CHECK(BY_HAND("shell.today_next", NULL), "editor can page to the last card"); quiet(); touch("合成卡乙");
        on_ui(locate_up_b); if (label) tap(x, y); quiet();
        for (int i = 0; i < 2; i++) CHECK(AI("shell.today_card", "{\"card\":\"example:b\",\"up\":true}") && g_rc.status == INK_RC_DONE, "AI uses the same up capability as touch");
        CHECK(AI("shell.today_card", "{\"card\":\"news:daily\"}") && g_rc.status == INK_RC_REJECTED, "a fifth card is refused without replacing a selection");
        CHECK(BY_HAND("shell.back", NULL) && BY_HAND("shell.back", NULL) && BY_HAND("shell.back", NULL),
            "return to the home through display settings and Settings"); quiet(); on_ui(check_order); capture("TodayOrder");
        CHECK(AI("shell.today_edit", NULL), "open the editor"); quiet(); on_ui(check_saved_order); CHECK(BY_HAND("shell.back", NULL), "back home"); quiet();
        CHECK(BY_HAND("shell.apps", NULL), "open All apps"); quiet();
        on_ui(locate_listen_icon); unsigned before = opens, prior_launches = launches; if (label) tap(x, y); quiet();
        CHECK(launches == prior_launches + 1 && opens == before && !strcmp(ink_ui_page_name(), LISTEN_PAGE.name), "short press still opens the application");
        CHECK(BY_HAND("shell.back", NULL), "back to all apps"); quiet(); on_ui(locate_listen_icon); before = opens;
        hw(HW_PRESS, x, y); delay_ms(650); hw(HW_RELEASE, x, y); quiet();
        CHECK(opens == before && !strcmp(ink_ui_top_name(), "IconMenu"), "long press opens only the menu, without launching the app");
        on_ui(record_menu); capture("IconMenu"); ink_probe_count("test.snapshot.IconMenu");
        touch("接着听");
        CHECK(opens == before + 1 && opened_by == INK_SRC_TOUCH && !strcmp(ink_ui_top_name(), LISTEN_PAGE.name), "menu shortcut uses the app capability and dismisses the menu");
        on_ui(home); quiet();
        CHECK(AI("shell.icon_menu", "{\"id\":\"listen\"}") && g_rc.status == INK_RC_DONE, "AI opens the same IconMenu"); quiet();
        CHECK(AI(RESUME.id, NULL) && g_rc.status == INK_RC_DONE && opens == before + 2 && opened_by == INK_SRC_AI, "AI resumes through the identical app handler");
        on_ui(home); quiet();
        CHECK(AI("shell.today_card", "{\"card\":\"listen:resume\"}") && g_rc.status == INK_RC_DONE, "remove listen before testing the menu's Today action");
        CHECK(AI("shell.icon_menu", "{\"id\":\"listen\"}") && g_rc.status == INK_RC_DONE, "open menu to add its card"); quiet(); touch("卡片放在主屏");
        CHECK(AI("shell.today_card", "{\"card\":\"listen:resume\",\"up\":true}") && g_rc.status == INK_RC_DONE, "restore listener to third");
        CHECK(AI("shell.today_card", "{\"card\":\"listen:resume\",\"up\":true}") && g_rc.status == INK_RC_DONE, "restore listener to second");
        on_ui(check_order);
    } else {
        on_ui(home); quiet(); on_ui(check_order); /* persisted, in a new process */
    }
    on_ui(child_space); quiet(); on_ui(home); quiet();
    if (!restarting) {
        on_ui(check_child_default);
        CHECK(AI("shell.today_edit", NULL) && g_rc.status == INK_RC_DONE, "child can edit only their own home cards"); quiet(); on_ui(inspect);
        CHECK(strstr(snapshot, "\"selected\":\"\""), "child editor does not retain the adult navigation selection");
        CHECK(!strstr(snapshot, "news:daily"), "adult-only app is absent from the child editor");
        CHECK(AI("shell.today_card", "{\"card\":\"news:daily\"}") && g_rc.status == INK_RC_REJECTED, "child cannot select an adult-only card via AI");
        CHECK(AI("shell.today_card", "{\"card\":\"listen:resume\"}") && g_rc.status == INK_RC_DONE, "child changes their own selection");
        CHECK(AI("shell.today_card", "{\"card\":\"example:a\"}") && g_rc.status == INK_RC_DONE, "child adds an independent card");
        CHECK(BY_HAND("shell.back", NULL), "child returns to the home"); quiet();
    }
    on_ui(check_child_saved);
    ink_member_test_reset(); quiet(); on_ui(home); quiet(); on_ui(check_order);
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle(), 10000), "member selections are durable before restart");
    ink_probe_count("test.snapshot.settled"); ink_power_release(INK_NEED_NET); ink_probe_count("test.finished");
}
void ink_main(void) {
    restarting = getenv("INK_F8_RESTART") != NULL;
    product_main(); test_start_home();
    CHECK(ink_cap_register(&RESUME, 1) == 0, "register synthetic shortcut");
    for (unsigned i = 0; i < sizeof APPS / sizeof *APPS; i++) CHECK(ink_app_register(&APPS[i]) == 0, "register fixture app");
    osal_task_cfg_t cfg = {.name = "fw-f8-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start bounded test driver");
}
