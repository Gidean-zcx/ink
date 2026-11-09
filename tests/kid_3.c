/* Synthetic family data; exercise the production shell and capability bus. */
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/sys/appreg.h"
#include "ink/ui_shell.h"
#include <stdlib.h>

static char view[3072], context[3072], child_today[3072], capture_name[24];
static const char *wanted;
static lv_obj_t *label;
static int x, y;
static unsigned opened;
static ink_source_t opened_by;
static volatile bool pin_done;
static int pin_result;
static void empty(lv_obj_t *screen, void *arg) { (void)screen; (void)arg; opened++; }
static const ink_activity_t LISTEN = {.name = "example.listen", .app = "listen", .create = empty};
static void resume(ink_call_t *c) { opened_by = c->src; ink_ui_push(&LISTEN, NULL); }
static void allowed(ink_call_t *c) { (void)c; }
static const ink_cap_t CAPS[] = {
    {.id = "example.listen_resume", .title = "接着听合成故事", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = resume},
    {.id = "reader.example_allowed", .title = "白名单中的合成能力", .effect = INK_FX_READ, .run = allowed},
    {.id = "example.adult_only", .title = "仅大人的合成能力", .effect = INK_FX_READ, .run = allowed}
};
static int fixture(ink_card_t *out, const char *id, const char *title, const char *action) {
    snprintf(out->id, sizeof out->id, "%s", id);
    snprintf(out->title, sizeof out->title, "%s", title);
    snprintf(out->text, sizeof out->text, "%s合成%s", ink_member_current()->role == INK_ROLE_CHILD ? "孩子" : "大人", title);
    snprintf(out->action, sizeof out->action, "%s", action);
    snprintf(out->member, sizeof out->member, "%s", ink_member_current()->id);
    out->rev = 1;
    return 1;
}
static int listen_card(int i, ink_card_t *out) { return i ? 0 : fixture(out, "resume", "故事", CAPS[0].id); }
static int news_card(int i, ink_card_t *out) { return i ? 0 : fixture(out, "daily", "报纸", ""); }
static int word_card(int i, ink_card_t *out) { return i ? 0 : fixture(out, "review", "单词", ""); }
static int reading_card(int i, ink_card_t *out) {
    if (i > 1) return 0;
    fixture(out, "reading", "合成书", "reader.open_book");
    snprintf(out->member, sizeof out->member, "%s", i ? "kid" : "owner");
    snprintf(out->text, sizeof out->text, "《合成书》第 %d 页", i ? 2 : 4);
    out->bookmark = true;
    return 1;
}
static const char *const ALLOWED_NEEDS[] = {"reader.example_allowed", NULL};
static const char *const DENIED_NEEDS[] = {"example.adult_only", NULL};
static const ink_app_def_t APPS[] = {
    {.id = "listen", .title = "听", .icon = "headphones", .entry = &LISTEN, .child_ok = true, .card = listen_card},
    {.id = "news", .title = "报纸", .icon = "news", .entry = &LISTEN, .child_ok = true, .card = news_card, .needs = ALLOWED_NEEDS},
    {.id = "word", .title = "单词", .icon = "word", .entry = &LISTEN, .child_ok = true, .card = word_card},
    {.id = "example.parent", .title = "合成家长应用", .entry = &LISTEN},
    {.id = "example.reading", .title = "合成阅读卡片", .child_ok = true, .card = reading_card},
    {.id = "example.denied", .title = "合成未授权", .entry = &LISTEN, .child_ok = true, .needs = DENIED_NEEDS}
};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "physical display settles"); }
static void call_ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s: %s (%s)", id, ink_rc_name(g_rc.status), g_rc.detail);
    quiet();
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "snapshot fits the real AI buffer");
    CHECK(ink_ui_context_snapshot(context, sizeof context, NULL) > 0 && !strcmp(view, context), "UI and AI read the same child page");
}
static void home(void) { ink_ui_home(); }
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), wanted)) label = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int32_t)i));
}
static void find(void) {
    label = NULL;
    lv_obj_update_layout(lv_screen_active());
    locate(lv_screen_active());
    if (label) {
        lv_area_t area; lv_obj_get_coords(label, &area);
        x = (area.x1 + area.x2) / 2; y = (area.y1 + area.y2) / 2;
    }
}
static void touch(const char *text, bool tile) {
    wanted = text; on_ui(find);
    CHECK(label, "visible child touch target: %s", text);
    if (label) {
        if (tile) y -= ink_ui_px(INK_CIRCLE / 2 + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) / 2);
        tap(x, y);
    }
    quiet();
}
static void check_note(void) {
    wanted = "爸爸妈妈可以看到你的记录"; find();
    CHECK(label, "KidToday draws the parent visibility notice");
    if (label) {
        lv_area_t area; lv_obj_get_coords(label, &area);
        CHECK(area.y1 >= 0 && area.y2 < ink_ui_px(INK_CANVAS_H - INK_DOCK - INK_MARGIN), "the complete child notice fits above the dock");
    }
}
static void check_apps(void) {
    inspect();
    CHECK(strstr(view, "KidApps"), "child Apps snapshot uses KidApps");
    CHECK(!strstr(view, "文件（") && !strstr(view, "网页（") && !strstr(view, "设置（") &&
        !strstr(view, "合成家长应用") && !strstr(view, "合成未授权"), "child Apps hides forbidden and unavailable applications");
    if (strstr(view, "工具页=1/")) {
        CHECK(strstr(view, "报纸（news）"), "child Apps respects the existing capability whitelist");
        CHECK(strstr(view, "电话（phone）") && strstr(view, "短信（message）"), "phone and SMS remain in the child directory");
    }
    const char *absent[] = {"文件", "网页", "设置", "合成家长应用", "合成未授权"};
    for (unsigned i = 0; i < sizeof absent / sizeof *absent; i++) {
        wanted = absent[i]; find(); CHECK(!label, "no forbidden visible tile: %s", wanted);
    }
}
static void check_home(void) {
    inspect();
    CHECK(strstr(view, "页面=KidHome") && strstr(view, "第 2 页") && !strstr(view, "第 4 页"), "KidHome shows only the child's provider reading position");
    CHECK(strstr(view, "今天不限时"), "KidHome reads the actual unlimited policy");
    wanted = "今天不限时"; find();
    lv_area_t time_area = {0}, icon_label = {0};
    CHECK(label, "the real time allowance is visible on KidHome");
    if (label) lv_obj_get_coords(label, &time_area);
    wanted = "听"; find();
    CHECK(label, "the first child application row is visible");
    if (label) lv_obj_get_coords(label, &icon_label);
    CHECK(time_area.y2 < icon_label.y1 - ink_ui_px(INK_CIRCLE_APP + INK_GAP_S), "KidHome allowance never overlaps the application icons");
    wanted = "设置"; find(); CHECK(!label, "KidHome has no adult settings tile");
}
static void snapshot_event(void *arg) {
    const char *name = arg;
    CHECK(settled(), "capture a settled child frame");
    ink_probe_count("test.snapshot.settled");
    char metric[64]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
    sim_display_snapshot(name);
}
static void save_view(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.txt", g_sim.out_dir, (const char *)arg);
    CHECK(n > 0 && (size_t)n < sizeof path, "snapshot evidence path fits");
    FILE *f = fopen(path, "w");
    if (!f || fputs(view, f) == EOF || fclose(f)) abort();
}
static void capture(const char *name) {
    if (strcmp(name, capture_name)) return;
    quiet(); on_ui(inspect); vtk_host_call(save_view, (void *)name);
    vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name); delay_ms(5);
}
static void pin_reply(void *ctx, int result, uint32_t wait) { (void)ctx; (void)wait; pin_result = result; pin_done = true; }
static void set_time(void) {
    pin_done = false;
    CHECK(ink_member_set_pin("owner", "", "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "set a synthetic parent password");
    CHECK(AI("member.time_set", "{\"id\":\"kid\",\"minutes\":2}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "time changes require the real parent gate");
    unsigned token = 0;
    CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "parent confirmation has a token");
    pin_done = false;
    CHECK(ink_confirm_accept_with_pin(token, "24681357", pin_reply, NULL) == 0 && WAIT_FOR(pin_done, 6000) && pin_result == INK_PIN_OK, "the parent approves the two-minute policy");
    quiet();
}
static void expired_apps(void) {
    /* Inspect the underlying directory while KidTimeUp owns the foreground. */
    ink_shell_kid_close(); ink_ui_home();
    for (int i = 0; i < 2 && strcmp(ink_shell_board(), "KidApps"); i++) ink_shell_home_swipe(INK_SWIPE_LEFT);
    ink_shell_home_snapshot(view, sizeof view);
    CHECK(ink_member_time_up() && strstr(view, "KidApps"), "inspect KidApps under the existing time-up overlay");
    CHECK(strstr(view, "电话（phone）") && strstr(view, "短信（message）"), "time-up directory retains phone and SMS");
    int entries = 0;
    for (const char *at = view; (at = strstr(at, "（")); at += strlen("（")) entries++;
    CHECK(entries == 2, "the complete time-up directory contains exactly two applications");
    const char *absent[] = {"阅读（", "听（", "报纸（", "单词（", "备忘（", "接下来（", "时钟（", "相机（", "相册（", "对话记录（", "合成白名单"};
    for (unsigned i = 0; i < sizeof absent / sizeof *absent; i++) CHECK(!strstr(view, absent[i]), "time-up hides %s", absent[i]);
    wanted = "时钟"; find(); CHECK(!label, "time-up also removes actual noncommunication tiles");
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    call_ok("reader.open_book", "{\"title\":\"测试长书\"}"); call_ok("reader.goto_page", "{\"page\":4}");
    on_ui(home); quiet(); on_ui(inspect);
    CHECK(strstr(view, "页面=Main"), "adult Main name stays unchanged");
    call_ok("home.prev", NULL); on_ui(inspect);
    CHECK(!strcmp(ink_shell_board(), "Today") && strstr(view, "\"page\":\"home\""), "adult Today snapshot stays unchanged");
    call_ok("shell.today_edit", NULL); call_ok("shell.today_card", "{\"card\":\"listen:resume\"}"); call_ok("shell.back", NULL);
    call_ok("home.next", NULL); call_ok("home.next", NULL); on_ui(inspect);
    bool files = false, remote = false, settings = false;
    const char *at = strstr(view, "工具页="); int page = 0, pages = 0;
    CHECK(at && sscanf(at, "工具页=%d/%d", &page, &pages) == 2 && page == 1, "adult Apps starts at the first directory page");
    for (int i = 0; i < pages; i++) {
        files |= strstr(view, "文件（") != NULL; remote |= strstr(view, "网页（") != NULL; settings |= strstr(view, "设置（") != NULL;
        if (i + 1 < pages) { call_ok("home.next", NULL); on_ui(inspect); }
    }
    CHECK(!strncmp(view, "Apps；", strlen("Apps；")) && files && remote && settings, "adult Apps retains its original entries and name");
    CHECK(ink_member_switch("kid") == 0, "enter the child's actual member space"); quiet();
    call_ok("reader.open_book", "{\"title\":\"测试长书\"}"); call_ok("reader.goto_page", "{\"page\":2}");
    on_ui(home); quiet(); on_ui(check_home); capture("KidHome");
    unsigned before = opened;
    touch("听", true); CHECK(opened == before + 1, "KidHome touch launches through shell.launch");
    on_ui(home); quiet(); call_ok("shell.launch", "{\"id\":\"listen\"}");
    CHECK(opened == before + 2, "KidHome AI launches the same application entry"); on_ui(home); quiet();
    call_ok("home.prev", NULL); on_ui(inspect); on_ui(check_note);
    CHECK(strstr(view, "\"page\":\"KidToday\"") && strstr(view, "孩子合成故事") && !strstr(view, "大人合成"), "KidToday uses only the child's providers and page name");
    memcpy(child_today, view, sizeof child_today); capture("KidToday");
    before = opened; touch("孩子合成故事", false);
    CHECK(opened == before + 1 && opened_by == INK_SRC_TOUCH, "KidToday touch uses the provider capability");
    on_ui(home); quiet(); call_ok(CAPS[0].id, NULL);
    CHECK(opened == before + 2 && opened_by == INK_SRC_AI, "KidToday AI uses the identical provider handler");
    on_ui(home); quiet(); call_ok("home.next", NULL); call_ok("home.next", NULL); on_ui(check_apps); capture("KidApps");
    const char *child_at = strstr(view, "工具页="); int child_page = 0, child_pages = 0;
    CHECK(child_at && sscanf(child_at, "工具页=%d/%d", &child_page, &child_pages) == 2 && child_page == 1, "child app directory exposes its real pagination");
    for (int i = 1; i < child_pages; i++) { call_ok("home.next", NULL); on_ui(check_apps); }
    for (int i = 1; i < child_pages; i++) call_ok("home.prev", NULL);
    before = opened; touch("听", true);
    CHECK(opened == before + 1, "KidApps touch launches the registered entry"); on_ui(home); quiet();
    call_ok("shell.launch", "{\"id\":\"listen\"}"); CHECK(opened == before + 2, "KidApps AI launches the same entry");
    on_ui(home); quiet();
    CHECK(ink_member_switch("owner") == 0, "return to the adult without a password"); quiet();
    call_ok("shell.today_edit", NULL); call_ok("shell.today_card", "{\"card\":\"news:daily\"}"); call_ok("shell.back", NULL);
    CHECK(ink_member_switch("kid") == 0, "return to the child's Today after changing adult choices"); quiet();
    call_ok("home.prev", NULL); on_ui(inspect);
    CHECK(strstr(view, "孩子合成故事") && strstr(view, "孩子合成报纸") && !strstr(view, "大人合成"), "adult TodayEdit changes cannot remove the child's cards (counterexample)");
    CHECK(strstr(child_today, "孩子合成故事") && strstr(child_today, "孩子合成报纸"), "the child's initial cards are independent of earlier adult selections");
    set_time(); on_ui(ink_ui_redraw); quiet(); on_ui(inspect);
    CHECK(strstr(view, "今天还能用") && !strstr(view, "今天不限时"), "KidToday reads the changed real time allowance");
    delay_ms(121000);
    CHECK(ink_member_time_up(), "virtual usage really reaches the child limit");
    on_ui(expired_apps); quiet();
    call_ok("shell.launch", "{\"id\":\"phone\"}"); CHECK(!strcmp(ink_ui_page_name(), "Phone"), "phone works after time is up");
    call_ok("shell.launch", "{\"id\":\"message\"}"); CHECK(!strcmp(ink_ui_page_name(), "Sms"), "SMS works after time is up");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    snprintf(capture_name, sizeof capture_name, "%s", getenv("INK_KID3_SCENE") ? getenv("INK_KID3_SCENE") : "KidToday");
    product_main(); test_start_home();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0, "register synthetic provider capabilities");
    for (unsigned i = 0; i < sizeof APPS / sizeof *APPS; i++) CHECK(ink_app_register(&APPS[i]) == 0, "register synthetic application");
    osal_task_cfg_t cfg = {.name = "test.kid-3", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the bounded child test");
}
