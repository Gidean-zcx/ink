/* The continue-reading card outlives the open book: per member, across a restart, gone with the book (reader-one-1:
 * the card says the chapter and how far, 《书名》章名 · 读到 N%, and a tap goes back to the same place). */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/hal_storage.h"
#include "ink/sys/file.h"
#include "ink/sys/app.h"
#include "ink/sys/storage.h"
#include "ink/ui_shell.h"

static char view[3072], page[3072], place_now[64], place_saved[64];
static const char *wanted;
static lv_obj_t *label;
static bool card_present;
static ink_card_t reading;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void call_ok(const char *id, const char *args) {
    CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s: %s (%s)", id, ink_rc_name(g_rc.status), g_rc.detail);
    quiet();
}
static void home(void) { ink_ui_home(); }
static void inspect(void) {
    CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "snapshot fits the AI buffer");
    ink_graft_snapshot(page, sizeof page);
    memset(&reading, 0, sizeof reading);
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!strcmp(app->id, "reader")) app->card(0, &reading);
    }
}
static bool today_has(const char *text) { on_ui(inspect); return strstr(view, text) != NULL; }
static void capture(void *ctx) { sim_display_snapshot(ctx); }
static void frame(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5); }
/* Where the reader is: chapter and place in it, from the page snapshot. */
static const char *place(void) {
    for (int i = 0; i < 300; i++) {
        on_ui(inspect);
        const char *c = strstr(page, "章节="), *end = c ? strstr(c, " 结束=") : NULL;
        if (c && end && strstr(page, "面板=正文")) { snprintf(place_now, sizeof place_now, "%.*s", (int)(end - c), c); return place_now; }
        delay_ms(50);
    }
    return "";
}
static unsigned percent_shown(void) { place(); const char *p = strstr(page, "章内页"); p = p ? strstr(p, " · ") : NULL; return p ? (unsigned)atoi(p + strlen(" · ")) : 999; }
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
/* The card names the chapter and the percentage the open book shows. */
static void card_matches(void) {
    unsigned shown = percent_shown();
    const char *chapter = strstr(page, "》 · "), *chapter_end = chapter ? strstr(chapter, " · 章内页") : NULL;
    char name[96] = "", expected[16];
    if (chapter && chapter_end) snprintf(name, sizeof name, "%.*s", (int)(chapter_end - chapter - strlen("》 · ")), chapter + strlen("》 · "));
    snprintf(expected, sizeof expected, "读到 %u%%", shown);
    on_ui(home); quiet(); on_ui(inspect);
    CHECK(!strcmp(reading.title, "继续读") && name[0] && strstr(reading.text, name) && strstr(reading.text, expected) && reading.value == (int64_t)shown,
          "card title, chapter and percentage: %.95s (%s, %s)", reading.text, name, expected);
    CHECK(strstr(view, expected), "AI sees the same percentage");
}
static void read_to(int chapter) {
    char args[32]; snprintf(args, sizeof args, "{\"chapter\":%d}", chapter);
    call_ok("reader.open_book", "{\"title\":\"测试长书\"}"); call_ok("reader.goto_chapter", args); call_ok("reader.next_page", NULL);
    snprintf(place_saved, sizeof place_saved, "%s", place());
}
static void durable(void) { CHECK(WAIT_FOR(!ink_storage_pending(), 15000), "positions written"); }
static void set_card(void *ctx) { (void)ctx; sim_sd_set_present(card_present); }
static void card(bool present) {
    card_present = present; vtk_event_at(vtk_now() + SIM_MS(1), set_card, NULL); delay_ms(10); ink_fs_refresh_state();
}
static void back_to_saved(const char *how) { CHECK(!strcmp(place(), place_saved), "%s goes back to the same place (%s / %s)", how, place_now, place_saved); }

static void first_run(void) {
    today(NULL);
    read_to(5); card_matches(); today("《测试长书》"); frame("continue-reading");
    on_ui(home); quiet(); CHECK(today_has("《测试长书》"), "back on home the card is still there");
    touch("测试长书"); back_to_saved("tapping the card");
    on_ui(home); quiet(); call_ok("shell.resume", NULL); back_to_saved("the main page's line");
    on_ui(home); quiet(); call_ok("reader.continue", NULL); back_to_saved("the AI capability");
    call_ok("reader.goto_chapter", "{\"chapter\":20}"); call_ok("reader.next_page", NULL); card_matches(); frame("continue-progress");
    CHECK(!strstr(reading.text, "读到 0%"), "later chapters show nonzero progress");
    read_to(5); on_ui(home); quiet(); char adult[96]; snprintf(adult, sizeof adult, "%s", reading.text);
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet();
    today(NULL);
    read_to(2); card_matches(); char child[96]; snprintf(child, sizeof child, "%s", reading.text);
    CHECK(strcmp(child, adult), "the child's card is the child's place (%s / %s)", child, adult);
    CHECK(ink_member_switch("owner") == 0, "the adult again"); quiet();
    today("《测试长书》"); on_ui(inspect); CHECK(!strcmp(reading.text, adult), "the adult does not see the child's place (%s)", reading.text);
    on_ui(home); quiet(); durable();
}
static void after_restart(void) {
    today("《测试长书》"); on_ui(inspect); char adult[96]; snprintf(adult, sizeof adult, "%s", reading.text);
    CHECK(strstr(adult, "读到"), "after a restart the card is back: %s", adult); frame("continue-restarted");
    on_ui(home); quiet(); call_ok("reader.continue", NULL); snprintf(place_saved, sizeof place_saved, "%s", place());
    card_matches();
    CHECK(ink_member_switch("kid") == 0, "the child after the restart"); quiet();
    today("《测试长书》"); on_ui(inspect); CHECK(strcmp(reading.text, adult), "the child's own card");
    touch("测试长书"); CHECK(place()[0], "the child's card opens the child's book");
    CHECK(ink_member_switch("owner") == 0, "the adult again"); quiet();
    today("《测试长书》");
    card(false);
    CHECK(WAIT_FOR(!today_has("测试长书"), 3000), "the home updates after card removal without navigation"); frame("continue-removed");
    on_ui(home); quiet(); CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "nothing to continue without the card");
    card(true);
    CHECK(WAIT_FOR(today_has("《测试长书》"), 3000), "the home updates after card insertion without navigation"); frame("continue-inserted");
    on_ui(home); quiet();
    CHECK(ink_fs_remove("/family/books/测试长书.txt") == 0, "the book is deleted (through the file service, as the files app does)");
    today(NULL);
    on_ui(home); quiet(); CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "a deleted book is not opened");
    call_ok("shell.resume", NULL);
    for (int i = 0; i < 300 && (on_ui(inspect), !strstr(page, "面板=书架")); i++) delay_ms(50);
    CHECK(!strcmp(ink_ui_top_name(), "book") && strstr(page, "面板=书架"), "the main page's line opens the shelf instead (%s)", ink_ui_top_name());
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
