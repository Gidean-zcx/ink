/* The continue-reading card outlives the open book: per member, across a restart, gone with the book. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ink/hal_storage.h"
#include "ink/sys/file.h"
#include "ink/sys/app.h"
#include "ink/sys/storage.h"
#include "ink/sys/crc.h"
#include "ink/sys/layout.h"
#include "ink/ui_shell.h"

static char view[3072];
static const char *wanted;
static lv_obj_t *label;
static bool card_present;
static ink_card_t reading;
static void durable(void);

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void call_ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s: %s (%s)", id, ink_rc_name(g_rc.status), g_rc.detail);
    quiet();
}
static void home(void) { ink_ui_home(); }
static void inspect(void) {
    CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "snapshot fits the AI buffer");
    memset(&reading, 0, sizeof reading);
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!strcmp(app->id, "reader")) app->card(0, &reading);
    }
}
static bool today_has(const char *text) { on_ui(inspect); return strstr(view, text) != NULL; }
static void capture(void *ctx) { sim_display_snapshot(ctx); }
static void frame(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
static void percentage(void) {
    durable(); on_ui(inspect);
    char name[40], path[200], data[64] = {0}, expected[16]; unsigned offset = 0;
    snprintf(name, sizeof name, "reader.%08x.pos", (unsigned)ink_crc32(0, "测试长书", strlen("测试长书")));
    CHECK(ink_layout_member(path, sizeof path, ink_member_current()->id, name) > 0, "position path fits");
    CHECK(ink_storage_read(path, data, sizeof data - 1) > 0 && sscanf(data, "offset=%u", &offset) == 1, "saved byte position exists");
    int64_t bytes = ink_file_size("/content/books/测试长书.txt");
    CHECK(bytes > 0 && offset > 0 && offset < bytes, "middle-book progress has a valid byte fraction");
    snprintf(expected, sizeof expected, "%u%%", bytes > 0 ? (unsigned)(offset * 100ull / (uint64_t)bytes) : 0);
    CHECK(!strcmp(reading.title, "继续读") && strstr(reading.text, expected), "card title and exact byte percentage: %.95s (%s)", reading.text, expected);
    CHECK(strstr(view, expected), "AI sees the same percentage");
}
static void locate(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && strstr(lv_label_get_text(obj), wanted)) label = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) locate(lv_obj_get_child(obj, (int32_t)i));
}
static void find(void) { label = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void touch(const char *text) {
    wanted = text; on_ui(find);
    CHECK(label, "visible card: %s", text);
    if (!label) return;
    lv_area_t a; lv_obj_get_coords(label, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); quiet();
}
static int page_now(void) {
    CHECK(AI("reader.read_page", NULL) && g_rc.status == INK_RC_DONE, "the book is open (%s, top=%s)", g_rc.detail, ink_ui_top_name());
    return (int)g_rc.revision;
}
/* The home's card (D-135: today's cards are on the one page), as the person sees it and as the AI reads it.
 * NULL: no card at all, rather than an empty one. */
static void today(const char *expect) {
    on_ui(home); quiet(); on_ui(ink_ui_redraw); quiet(); /* drawn afresh, as when the person comes to it */
    on_ui(inspect);
    if (expect) CHECK(strstr(view, expect) && strstr(view, "reader.continue"), "the home shows %s and offers the card's action (%.150s)", expect, view);
    else CHECK(!strstr(view, "reader.continue") && !strstr(view, "测试长书") && !strstr(view, "继续读"), "no card without the book (%.150s)", view);
    call_ok("app.cards", NULL);
    CHECK(expect ? strstr(g_result, expect) != NULL : !strstr(g_result, "测试长书"), "app.cards agrees (%.200s)", g_result);
}
static void read_to(int page) {
    char args[32]; snprintf(args, sizeof args, "{\"page\":%d}", page);
    call_ok("reader.open_book", "{\"title\":\"测试长书\"}"); call_ok("reader.goto_page", args);
}
static void durable(void) { CHECK(WAIT_FOR(!ink_storage_pending(), 15000), "positions written"); }
static void set_card(void *ctx) { (void)ctx; sim_sd_set_present(card_present); }
static void card(bool present) {
    card_present = present; vtk_event_at(vtk_now() + SIM_MS(1), set_card, NULL); delay_ms(10); ink_fs_refresh_state();
}

static void first_run(void) {
    today(NULL);
    read_to(5); on_ui(home); quiet();
    today("《测试长书》第 5 页");
    percentage(); frame("continue-reading");
    touch("第 5 页"); CHECK(page_now() == 5, "tapping the card goes back to page 5");
    on_ui(home); quiet(); call_ok("shell.resume", NULL); CHECK(page_now() == 5, "the main page's line goes back to page 5");
    on_ui(home); quiet(); call_ok("reader.continue", NULL); CHECK(page_now() == 5, "the AI capability goes back to page 5");
    call_ok("reader.goto_page", "{\"page\":20}"); on_ui(home); quiet();
    today("《测试长书》第 20 页"); percentage(); frame("continue-progress");
    CHECK(!strstr(reading.text, "· 0%"), "later pages show nonzero progress");
    read_to(5); on_ui(home); quiet();
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet();
    today(NULL);
    read_to(2); on_ui(home); quiet();
    today("《测试长书》第 2 页"); CHECK(!strstr(view, "第 5 页"), "the child does not see the adult's place");
    CHECK(ink_member_switch("owner") == 0, "the adult again"); quiet();
    today("《测试长书》第 5 页"); CHECK(!strstr(view, "第 2 页"), "the adult does not see the child's place");
    on_ui(home); quiet(); durable();
}
static void after_restart(void) {
    today("《测试长书》第 5 页");
    percentage(); frame("continue-restarted");
    on_ui(home); quiet(); call_ok("reader.continue", NULL); CHECK(page_now() == 5, "after a restart the card still goes to page 5");
    CHECK(ink_member_switch("kid") == 0, "the child after the restart"); quiet();
    today("《测试长书》第 2 页");
    touch("第 2 页"); CHECK(page_now() == 2, "the child's card goes to page 2");
    CHECK(ink_member_switch("owner") == 0, "the adult again"); quiet();
    today("《测试长书》第 5 页");
    card(false);
    CHECK(WAIT_FOR(!today_has("测试长书"), 3000), "the home updates after card removal without navigation"); frame("continue-removed");
    on_ui(home); quiet(); CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "nothing to continue without the card");
    card(true);
    CHECK(WAIT_FOR(today_has("《测试长书》第 5 页"), 3000), "the home updates after card insertion without navigation"); frame("continue-inserted");
    on_ui(home); quiet();
    CHECK(hal_fs_remove("/content/books/测试长书.txt") == 0, "the book is deleted");
    today(NULL);
    on_ui(home); quiet(); CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "a deleted book is not opened");
    call_ok("shell.resume", NULL); CHECK(!strcmp(ink_ui_top_name(), "reader"), "the main page's line opens the shelf instead (%s)", ink_ui_top_name());
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    const char *phase = getenv("INK_READER_CONTINUE_PHASE");
    if (phase && !strcmp(phase, "restart")) after_restart();
    else first_run();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "test.reader-continue", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test");
}
