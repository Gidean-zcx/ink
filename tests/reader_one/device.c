/* reader-one-1: the one reader. Phases (INK_READER_ONE_PHASE): what a missing piece shows, the three book places and
 * the back paths, the bookmark rules, and the old reader's data moving over at boot. Synthetic content only. */
#include <stdlib.h>
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/appreg.h"
#include "ink/sys/crc.h"
#include "ink/sys/layout.h"
#include "ink/sys/storage.h"
#include "ink/sys/confirm.h"
#include "ink/ui_shell.h"
#include "ink/ui_theme.h"

static char view[4096], phase[24], text[2048];
static const char *wanted;
static lv_obj_t *found;
static bool captured, card_present;
static ink_card_t card;
static int bx, by, bw;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "display settles"); }
static void inspect(void) { ink_graft_snapshot(view, sizeof view); }
static bool ready(const char *needle) {
    for (int i = 0; i < 600; i++) {
        on_ui(inspect);
        if (strstr(view, needle) && settled()) return true;
        delay_ms(50);
    }
    return false;
}
static void ok(const char *id, const char *args) { CHECK(AI(id, args) && g_rc.status == INK_RC_DONE, "%s done (%d): %.80s", id, g_rc.status, g_rc.say); quiet(); }
static void capture(void *name) { sim_display_snapshot(name); captured = true; }
static void frame(const char *name) {
    quiet(); captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "capture %s", name);
}
static void locate(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted)) found = o;
    const char *cap = ink_shell_bound_cap(o); /* "<reader.back>": the control bound to that capability */
    if (wanted[0] == '<' && cap && !strncmp(wanted + 1, cap, strlen(cap)) && wanted[strlen(cap) + 1] == '>') found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate(lv_obj_get_child(o, (int32_t)i));
}
static void find(void) {
    found = NULL; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active());
    if (found) { lv_area_t a; lv_obj_get_coords(found, &a); bx = (a.x1 + a.x2) / 2; by = (a.y1 + a.y2) / 2; bw = a.x2 - a.x1 + 1; }
}
static bool visible(const char *what) { wanted = what; on_ui(find); return found != NULL; }
static void touch(const char *what) { CHECK(visible(what), "%s is on the screen", what); if (found) { tap(bx, by); quiet(); } }
static int top_y, page_h, page_w;
static void geometry(void) { top_y = ink_ui_px(INK_STATUS_BAR); page_h = ink_ui_height() - top_y; page_w = ink_ui_width(); }
static void tap_native(int x, int y) { on_ui(geometry); tap(x * page_w / 684, top_y + y * page_h / 1216); quiet(); }
static bool top_is(const char *name) { return !strcmp(ink_ui_top_name(), name); }
static void home(void) { ink_ui_home(); }
static void go_home(void) { on_ui(home); quiet(); }
static double backs(void) { return sim_metric_value("reader.back"); }
/* A BACK key as the UI task hands it to the page on top (ui_task.c: on_key, else pop). The Plus has no back key and
 * its edge swipe and shell.back pop the page without asking it, so the key path is driven here directly. */
static void back_key(void) {
    const ink_activity_t *page = NULL;
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "reader")) page = ink_app_at(i)->entry;
    if (top_is("book") && page && !(page->on_key && page->on_key(INK_KEY_BACK, false))) ink_ui_pop();
}
static void read_card(void) {
    memset(&card, 0, sizeof card);
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, "reader") && ink_app_at(i)->card) ink_app_at(i)->card(0, &card);
}
static bool card_has(const char *needle) { on_ui(read_card); return card.text[0] && strstr(card.text, needle); }
static void set_card(void *ctx) { (void)ctx; sim_sd_set_present(card_present); }
static void sd(bool present) { card_present = present; vtk_event_at(vtk_now() + SIM_MS(1), set_card, NULL); delay_ms(10); ink_fs_refresh_state(); quiet(); }
static void open_reader(const char *panel) { go_home(); ok("app.open", "{\"id\":\"reader\"}"); CHECK(ready(panel), "the reader shows %s: %.150s", panel, view); }

/* ---------------------------------------------------------------- explanation pages and the back paths */
static void back_paths(const char *panel, void (*to)(void), const char *after, bool root) {
    static const char *const how[] = {"button", "BACK", "AI"};
    for (int path = 0; path < 3; path++) {
        to(); CHECK(ready(panel), "%s again: %.120s", panel, view);
        double before = backs();
        if (path == 0) { CHECK(visible("<reader.back>") && bw >= ink_ui_px(INK_CONTROL), "%s has the back button, %d px wide", panel, bw); if (found) tap(bx, by); }
        else if (path == 1) on_ui(back_key);
        else CHECK(AI("reader.back", NULL), "AI reader.back accepted");
        quiet();
        if (root) CHECK(WAIT_FOR(top_is("home"), 3000), "%s by %s leaves the reader (%s)", panel, how[path], ink_ui_top_name());
        else CHECK(ready(after), "%s by %s goes to %s: %.120s", panel, how[path], after, view);
        double calls = backs() - before;
        CHECK(calls == (path == 1 && root ? 0 : 1), "%s by %s calls reader.back %g times", panel, how[path], calls);
    }
}
static void to_shelf(void) { open_reader("面板=书架"); }
static void to_book(void) { go_home(); ok("reader.open_book", "{\"title\":\"我的小书\"}"); CHECK(ready("面板=正文"), "the book is open"); }
static void to_toc(void) { to_book(); ok("reader.show_chapters", NULL); }
static void to_marks(void) { to_book(); ok("reader.bookmarks", "{\"open\":true}"); }
static void to_settings(void) { to_book(); ok("reader.menu", "{\"view\":\"settings\"}"); }
static void to_tools(void) { to_book(); ok("reader.menu", NULL); }
static void to_missing(void) { open_reader("面板=缺字体"); }
static void missing_font(void) {
    open_reader("面板=缺字体"); /* the card files' notice says it (D-138), one screen for the font */
    CHECK(ready("字体还没放到存储卡上") && strstr(view, "card.files=missing"), "the page says what is missing and that it comes by itself: %.200s", view);
    CHECK(visible("字体还没放到存储卡上"), "the explanation is drawn, not a blank page");
    CHECK(strstr(view, "动作：reader.back"), "the snapshot names what can be done");
    frame("missing-font");
    back_paths("面板=缺字体", to_missing, NULL, true);
}
static void no_card(void) {
    open_reader("面板=缺字体");
    CHECK(ready("没有存储卡") && visible("没有存储卡"), "no card is explained: %.150s", view);
    frame("no-card");
    CHECK(AI("reader.list_books", NULL) && g_rc.status == INK_RC_FAILED, "nothing to list without the card");
}
/* The member's own font does not load: the family's copy is used (the card files look after that one); no notice
 * that would wait for a check that finds nothing wrong with the card. */
static void broken_own_font(void) {
    open_reader("面板=书架");
    ok("reader.open_book", "{\"title\":\"测试长书\"}");
    CHECK(ready("面板=正文") && strstr(view, "《测试长书》"), "a book opens with the family's font: %.150s", view);
}
static void empty_shelf(void) {
    open_reader("面板=书架空");
    CHECK(visible("书架上还没有书") && visible("<transfer.open>"), "the adult is told how books get here, with 传书");
    CHECK(strstr(view, "transfer.open"), "the AI sees the same action");
    frame("empty-adult");
    touch("<transfer.open>");
    CHECK(WAIT_FOR(!top_is("book"), 3000), "传书 opens the transfer page (%s)", ink_ui_top_name());
    CHECK(!ink_member_switch("kid"), "the child"); quiet();
    open_reader("面板=书架空");
    CHECK(visible("请家长") && !visible("<transfer.open>"), "the child is told to ask a parent, without a button");
    frame("empty-child");
}
static void places(void) {
    open_reader("面板=书架");
    CHECK(strstr(view, "我的小书 我的书") && strstr(view, "测试长书 全家的") && strstr(view, "收件箱的书 传进来的"), "three places, each named: %.200s", view);
    frame("shelf-places");
    ink_member_identity_t me; ink_member_identity(&me); ink_fs_file_t *f = NULL;
    CHECK(ink_graft_file_open(&me, "/family/books/测试长书.txt", INK_FS_READ, &f) == 0, "the family books are readable"); if (f) ink_fs_close(f);
    f = NULL; CHECK(ink_graft_file_open(&me, "/family/books/新.txt", INK_FS_WRITE, &f) != 0 && !f, "but not writable through the graft");
    ok("reader.list_books", NULL);
    CHECK(strstr(g_result, "\"place\":\"传进来的\"") && strstr(g_result, "\"title\":\"测试长书\""), "list_books names the places: %.200s", g_result);
    /* back from every page, three ways, never more than one reader.back */
    back_paths("面板=书架", to_shelf, NULL, true);
    back_paths("面板=目录", to_toc, "面板=正文", false);
    back_paths("面板=书签列表", to_marks, "面板=正文", false);
    back_paths("面板=设置", to_settings, "面板=正文", false);
    back_paths("面板=工具", to_tools, "面板=正文", false);
    to_book(); CHECK(!visible("<reader.back>"), "the bare body keeps the page clean: no back button");
    tap_native(342, 600); CHECK(ready("面板=工具") && visible("<reader.back>"), "a tap in the middle brings the tools and the back button");
    frame("tools-back");
    ok("reader.back", NULL); CHECK(ready("面板=正文"), "back to the body");
    ok("reader.back", NULL); CHECK(ready("面板=书架"), "the body goes back to the shelf");
    /* BACK while a page is being laid out does not leave the reader */
    to_book();
    ink_call_opts_t opts = {.src = INK_SRC_KEY};
    CHECK(!ink_cap_call("reader.next_page", NULL, &opts), "a turn starts");
    on_ui(back_key); quiet();
    CHECK(top_is("book"), "BACK while laying out stays in the reader (%s)", ink_ui_top_name());
}
/* ---------------------------------------------------------------- bookmarks */
static int marks(uint32_t ids[32]) {
    ok("reader.bookmarks", NULL);
    int n = 0;
    for (const char *p = g_result; (p = strstr(p, "\"id\":")) && n < 32; p += 5) ids[n++] = (uint32_t)strtoul(p + 5, NULL, 10);
    return n;
}
static void bookmarks(void) {
    go_home(); ok("reader.open_book", "{\"title\":\"测试长书\"}");
    uint32_t ids[32], again[32];
    for (int i = 0; i < 26; i++) { ok("reader.add_bookmark", NULL); ok("reader.next_page", NULL); }
    CHECK(marks(ids) == 24, "at most 24 bookmarks are kept (%d)", marks(ids));
    for (int i = 0; i < 24; i++) for (int j = i + 1; j < 24; j++) CHECK(ids[i] != ids[j], "ids are distinct");
    ok("reader.prev_page", NULL); ok("reader.add_bookmark", NULL); ok("reader.add_bookmark", NULL);
    CHECK(marks(again) == 24 && !memcmp(ids, again, sizeof(uint32_t) * 24), "adding the same page again changes nothing");
    char args[32]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ids[5]); ok("reader.remove_bookmark", args);
    int n = marks(again);
    CHECK(n == 23, "one removed (%d)", n);
    for (int i = 0, k = 0; i < 24; i++) if (i != 5) CHECK(again[k++] == ids[i], "the others keep their ids");
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ids[10]); ok("reader.goto_bookmark", args);
    ok("reader.bookmarks", NULL); CHECK(strstr(g_result, "\"excerpt\":\"") && !strstr(g_result, "\"excerpt\":\"\""), "excerpts are read from the book");
    ok("reader.bookmarks", "{\"open\":true}"); CHECK(ready("面板=书签列表"), "the list opens"); frame("bookmarks");
}
/* ---------------------------------------------------------------- the old reader's data */
static const char *env(const char *name) { const char *v = getenv(name); return v ? v : ""; }
static void put_old(const char *member, const char *name, const char *data) {
    char path[160]; CHECK(ink_layout_member(path, sizeof path, member, name) > 0, "old path fits");
    CHECK(ink_storage_write_member_atomic(path, data, strlen(data)) == 0, "old file %s written", name);
}
static uint32_t old_crc(const char *member, const char *name) {
    char path[160], data[512] = {0}; ink_layout_member(path, sizeof path, member, name);
    int n = ink_storage_read(path, data, sizeof data);
    return n > 0 ? ink_crc32(0, data, (size_t)n) : 0;
}
static char pos_name[40];
static const char *pos(const char *key) { snprintf(pos_name, sizeof pos_name, "reader.%08x.pos", (unsigned)ink_crc32(0, key, strlen(key))); return pos_name; }
static void old_mark(ink_db_t *db, const char *book, int chapter, int block, unsigned offset, int64_t added, const char *excerpt) {
    char json[512]; snprintf(json, sizeof json, "{\"book\":\"%s\",\"id\":%lld,\"chapter\":%d,\"block\":%d,\"offset\":%u,\"end_chapter\":%d,\"end_block\":%d,\"end_offset\":%u,\"page\":1,\"added_at\":%lld,\"excerpt\":\"%s\"}",
        book, (long long)added, chapter, block, offset, chapter, block, offset + 100, (long long)added, excerpt);
    ink_db_write_t w = {.data = json, .len = strlen(json), .src = INK_SRC_SYSTEM};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "old bookmark written");
}
static uint32_t marks_print(const char *member) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("reader.bookmarks"), member); ink_db_cursor_t c = {0}; ink_rec_t r; uint32_t crc = 0;
    while (db && ink_db_next(db, &c, &r, text, sizeof text - 1) == 1) crc = ink_crc32(crc, text, r.len);
    ink_db_close(db); return crc;
}
static void seed(bool extra) {
    const ink_db_def_t *def = ink_db_find_def("reader.bookmarks");
    CHECK(def && def->scope == INK_DB_MEMBER && def->json && !def->ai_read && !def->ai_write, "the old bookmark collection stays registered, not for the AI");
    ink_db_t *db = ink_db_open_member(def, "owner");
    if (extra) { old_mark(db, "/content/books/测试长书.txt", 0, 0, (unsigned)atoi(env("INK_ONE_MARK5")), 1800000500, env("INK_ONE_EXCERPT5")); ink_db_flush(db); ink_db_close(db); return; }
    char last[256];
    snprintf(last, sizeof last, "page=9\nbook=测试长书\noffset=%s\nfile=测试长书\nchapter=0\n", env("INK_ONE_LAST"));
    put_old("owner", "reader.last", last);
    snprintf(last, sizeof last, "offset=%s\nchapter=0\n", env("INK_ONE_LAST")); put_old("owner", pos("测试长书"), last);
    put_old("owner", pos("inbox/来信.epub"), "offset=12\nchapter=1\n");
    put_old("owner", pos("夜邮（示例）"), "offset=200\nchapter=0\n");
    for (int i = 1; i <= 4; i++) {
        char name[24], ex[24]; snprintf(name, sizeof name, "INK_ONE_MARK%d", i); snprintf(ex, sizeof ex, "INK_ONE_EXCERPT%d", i);
        old_mark(db, "/content/books/测试长书.txt", 0, 0, (unsigned)atoi(env(name)), 1800000000 + i, env(ex));
    }
    old_mark(db, "/content/books/已删.txt", 0, 0, 10, 1800000100, "早就删了");
    ink_db_flush(db); CHECK(ink_db_wait(db, ink_db_last_ver(db), 5000000) == 0, "old bookmarks durable"); ink_db_close(db);
    snprintf(last, sizeof last, "page=2\nbook=测试长书\noffset=%s\nfile=测试长书\nchapter=0\n", env("INK_ONE_KID"));
    put_old("kid", "reader.last", last);
    snprintf(last, sizeof last, "offset=%s\nchapter=0\n", env("INK_ONE_KID")); put_old("kid", pos("测试长书"), last);
    char path[64], settings[2048] = {0}; ink_layout_sys(path, sizeof path, "settings.json");
    int n = ink_storage_read(path, settings, sizeof settings - 200);
    if (n <= 2) snprintf(settings, sizeof settings, "{}");
    char *end = strrchr(settings, '}');
    if (end) snprintf(end, sizeof settings - (size_t)(end - settings), "%s\"reader.font_size:owner\":2,\"reader.line_spacing:owner\":3,\"reader.margins:owner\":0,\"reader.footer_mask:owner\":1}", n > 2 ? "," : "");
    CHECK(ink_storage_write_atomic(path, settings, strlen(settings)) == 0, "old per-member layout in the settings file");
    CHECK(WAIT_FOR(!ink_storage_pending(), 15000), "old data durable");
}
static void boot_checks(int again) {
    uint32_t before[4] = {old_crc("owner", "reader.last"), old_crc("owner", pos("测试长书")), old_crc("kid", "reader.last"), marks_print("owner")};
    CHECK(WAIT_FOR(card_has("测试长书"), 20000), "the home card continues the old book: %.95s", card.text);
    CHECK(strstr(card.text, "读到") && !strcmp(card.action, "reader.continue") && card.bookmark, "card text and action: %.95s", card.text);
    char card_text[96]; snprintf(card_text, sizeof card_text, "%s", card.text);
    ok("settings.set", "{\"id\":\"display.lock_reading\",\"value\":1}"); ok("settings.set", "{\"id\":\"display.lock_style\",\"value\":4}");
    CHECK(BY_HAND("shell.lock", NULL), "lock"); quiet();
    char lock[4096]; ink_ui_snapshot(lock, sizeof lock, NULL);
    on_ui(read_card); CHECK(!strcmp(card.text, card_text), "the lock screen reads the same card");
    frame(again ? "lock-again" : "lock-continue");
    key(HAL_KEY_HOME); quiet(); go_home();
    ok("reader.continue", NULL);
    /* The first boot continues where the old reader stopped; later boots where this test left the book (the oldest mark). */
    CHECK(ready("面板=正文") && strstr(view, env(again ? "INK_ONE_EXCERPT1" : "INK_ONE_EXCERPT_LAST")), "continue opens where the old reader stopped: %.200s", view);
    const char *chapter = strstr(view, "》 · "), *chapter_end = chapter ? strstr(chapter, " · 章内页") : NULL;
    char name[96] = "";
    if (chapter && chapter_end) snprintf(name, sizeof name, "%.*s", (int)(chapter_end - chapter - strlen("》 · ")), chapter + strlen("》 · "));
    CHECK(name[0] && strstr(card_text, name), "the card names the chapter the book opens in (%s / %s)", name, card_text);
    uint32_t ids[32]; int n = marks(ids);
    CHECK(n == (again == 2 ? 5 : 4), "the old marks of this book moved over once (%d)", n);
    if (n > 0) {
        char args[32]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)ids[n - 1]); ok("reader.goto_bookmark", args);
        CHECK(ready("面板=正文") && strstr(view, env("INK_ONE_EXCERPT1")), "the oldest mark opens at its excerpt: %.200s", view);
    }
    ok("reader.get_layout", NULL);
    CHECK(strstr(g_result, "\"font_size\":56") && strstr(g_result, "\"line_spacing\":150") && strstr(g_result, "\"margins\":16") && strstr(g_result, "\"footer\":1"),
          "the old layout, converted: %.200s", g_result);
    double progress = sim_metric_value("reader.migrated_progress"), moved = sim_metric_value("reader.migrated_bookmarks"), skipped = sim_metric_value("reader.migration_skipped");
    if (!again) CHECK(progress == 3 && moved == 4 && skipped == 1, "counters: progress %g, bookmarks %g, skipped %g", progress, moved, skipped);
    else if (again == 1) CHECK(progress == 0 && moved == 0 && skipped == 0, "a second boot moves nothing: %g %g %g", progress, moved, skipped);
    else CHECK(moved == 1, "a restored older backup moves only the new mark: %g", moved);
    go_home(); CHECK(WAIT_FOR(card_has("测试长书"), 5000), "the card follows the reading");
    snprintf(card_text, sizeof card_text, "%s", card.text);
    CHECK(!ink_member_switch("kid"), "the child"); quiet();
    CHECK(WAIT_FOR(card_has("测试长书") && strcmp(card.text, card_text), 20000), "the child's own card: %.95s", card.text);
    ok("reader.continue", NULL);
    CHECK(ready("面板=正文") && strstr(view, env("INK_ONE_EXCERPT_KID")) && strstr(view, "书签=0"), "the child's place and no adult marks: %.200s", view);
    go_home(); CHECK(!ink_member_switch("owner"), "the adult again"); quiet();
    CHECK(WAIT_FOR(card_has(card_text), 20000), "the adult's card is unchanged: %.95s", card.text);
    uint32_t after[4] = {old_crc("owner", "reader.last"), old_crc("owner", pos("测试长书")), old_crc("kid", "reader.last"), marks_print("owner")};
    CHECK(!memcmp(before, after, sizeof before) && before[0] && before[3], "the old files are only read");
    /* The old files stay: once found done, opening a book does not walk the old folders again */
    go_home(); ok("reader.continue", NULL); CHECK(ready("面板=正文"), "continue");
    double looked = sim_metric_value("reader.migration_looked");
    for (int i = 0; i < 3; i++) { go_home(); ok("reader.continue", NULL); CHECK(ready("面板=正文"), "continue again"); ok("reader.open_book", "{\"title\":\"测试长书\"}"); }
    CHECK(looked > 0 && sim_metric_value("reader.migration_looked") == looked, "reopening walks the old folders %g more times (%g before)", sim_metric_value("reader.migration_looked") - looked, looked);
}
static bool marker(void) {
    static const ink_db_def_t prefs = {.name = "mogo.preferences", .title = "阅读偏好与进度", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 256, .max_bytes = 131072};
    ink_db_t *db = ink_db_open_member(&prefs, "owner"); ink_db_cursor_t c = {0}; ink_rec_t r; bool seen = false;
    while (db && ink_db_next(db, &c, &r, text, sizeof text) == 1) seen = seen || !strncmp(text, "ink_migr", 8);
    ink_db_close(db); return seen;
}
/* Everything the boot posted has run: the executors take jobs in order, so a job passed BG, UI, BG, UI four times
 * runs after the boot's migration (BG), its answer (UI), the card read (BG) and its answer (UI). */
static ink_exec_job_t fence_job[4]; static volatile int fence_hops;
static void fence_hop(void *arg) {
    int hop = (int)(intptr_t)arg; fence_hops = hop + 1;
    if (hop < 3) ink_exec_post(hop % 2 ? INK_EXEC_BG : INK_EXEC_UI, &fence_job[hop + 1], fence_hop, (void *)(intptr_t)(hop + 1));
}
static void boot_done(void) { fence_hops = 0; CHECK(!ink_exec_post(INK_EXEC_BG, &fence_job[0], fence_hop, NULL) && WAIT_FOR(fence_hops == 4, 10000), "the boot's work has run"); }
static void no_card_boot(void) {
    quiet(); boot_done();
    CHECK(!marker() && !card_has("测试长书"), "with the card out nothing is marked done and no card shows");
    sd(true);
    CHECK(WAIT_FOR(card_has("测试长书"), 20000), "the card in: the migration runs");
    CHECK(WAIT_FOR(marker(), 5000), "and marks it done");
}
/* ---------------------------------------------------------------- review fixes (reader-one-1 second round) */
/* A whole result, however long (g_result keeps 4 KB): printed for run.py, which parses it as JSON. */
static char *whole; static volatile int whole_done;
static void whole_rc(void *ctx, const ink_receipt_t *rc) {
    (void)ctx; g_rc = *rc; g_rc.result = NULL; free(whole);
    whole = strdup(rc->result ? rc->result : ""); __atomic_store_n(&whole_done, 1, __ATOMIC_RELEASE);
}
static void full(const char *id, const char *args) {
    ink_call_opts_t o = {.src = INK_SRC_AI, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = whole_rc};
    whole_done = 0;
    CHECK(!ink_cap_call(id, NULL, &o), "%s accepted", id);
    for (int i = 0; i < 4000 && !__atomic_load_n(&whole_done, __ATOMIC_ACQUIRE); i++) delay_ms(5);
    CHECK(whole_done && g_rc.status == INK_RC_DONE && whole && whole[0], "%s done with a result (%d): %.80s", id, g_rc.status, g_rc.say);
    printf("READER-ONE-RESULT %s %zu %s\n", id, whole ? strlen(whole) : 0, whole ? whole : ""); fflush(stdout);
}
static int occurrences(const char *in, const char *needle) { int n = 0; for (const char *p = in; p && (p = strstr(p, needle)); p += strlen(needle)) n++; return n; }
static void redraw(void) { ink_ui_redraw(); }
static void pop(void) { ink_ui_pop(); }
static char shown[96];
static void title_shown(void) { const char *a = strstr(view, "《"), *b = a ? strstr(a, "》") : NULL; snprintf(shown, sizeof shown, "%.*s", a && b ? (int)(b - a) : 0, a ? a : ""); }
static void review(void) {
    /* AI results are whole and valid however long: a shelf of 70 books, 300 contents entries, 24 bookmarks with long labels */
    for (int i = 0; i < 60 && (ok("reader.list_books", NULL), !strstr(g_result, "第69本")); i++) delay_ms(500); /* the file index takes the 70 new books in */
    full("reader.list_books", NULL);
    CHECK(whole && strlen(whole) > 4096 && occurrences(whole, "\"index\":") >= 70, "the whole shelf (%zu bytes, %d books)", whole ? strlen(whole) : 0, whole ? occurrences(whole, "\"index\":") : 0);
    go_home(); ok("reader.open_book", "{\"title\":\"目录很长\"}"); CHECK(ready("面板=正文"), "the long contents book opens: %.120s", view);
    full("reader.chapters", NULL);
    CHECK(whole && occurrences(whole, "\"entry\":") == 300 && strlen(whole) > 4096, "all 300 entries (%d, %zu bytes)", whole ? occurrences(whole, "\"entry\":") : 0, whole ? strlen(whole) : 0);
    CHECK(whole && strstr(whole, "\\\"书\\\"\\\\测试"), "the title's quote and backslash are escaped: %.120s", whole ? whole : "");
    for (int c = 1; c <= 24; c++) { char args[32]; snprintf(args, sizeof args, "{\"chapter\":%d}", c); ok("reader.goto_chapter", args); ok("reader.add_bookmark", NULL); }
    full("reader.bookmarks", NULL);
    CHECK(whole && occurrences(whole, "\"id\":") == 24 && strlen(whole) > 4096 && strstr(whole, "\\\"引号\\\""), "24 bookmarks with their long labels, escaped (%zu bytes)", whole ? strlen(whole) : 0);
    /* A tap on a bookmark row finds its mark as drawn: the UI task never opens the member's records */
    double records = sim_metric_value("reader.ui_records");
    ok("reader.bookmarks", "{\"open\":true}"); CHECK(ready("面板=书签列表"), "the list opens");
    tap_native(342, 656 + 40);
    CHECK(ready("面板=正文") && strstr(view, "章节=24 "), "the newest mark (chapter 24) opens by touch: %.160s", view);
    CHECK(sim_metric_value("reader.ui_records") == records && records == 0, "the UI task opened the member's records %g times", sim_metric_value("reader.ui_records"));
    /* Built again (a redraw, a page above closing), the page shows the book it had, at its place, whatever opened it */
    go_home(); ok("reader.open_book", "{\"title\":\"我的小书\"}"); CHECK(ready("面板=正文"), "opened by title from home");
    ok("reader.next_page", NULL); ok("reader.next_page", NULL); CHECK(ready("页=3/"), "page 3: %.120s", view);
    ok("reader.list_books", NULL); /* the shelf query reuses the buffer the title open came from */
    on_ui(redraw); quiet();
    CHECK(ready("面板=正文") && strstr(view, "《我的小书》") && strstr(view, "页=3/"), "a redraw keeps the book and its page: %.160s", view);
    go_home(); ok("reader.open_book", "{\"title\":\"\",\"path\":\"/me/收件箱/收件箱的书.txt\"}"); CHECK(ready("《收件箱的书》"), "opened by path: %.120s", view);
    ok("reader.next_page", NULL); CHECK(ready("页=2/"), "page 2");
    ok("app.open", "{\"id\":\"clock\"}"); CHECK(WAIT_FOR(!top_is("book"), 3000), "a page above the reader (%s)", ink_ui_top_name());
    on_ui(pop); quiet();
    CHECK(top_is("book") && ready("面板=正文") && strstr(view, "《收件箱的书》") && strstr(view, "页=2/"), "back under it: the same book and page: %.160s", view);
    open_reader("面板=书架"); tap_native(342, 210 + 80); CHECK(ready("面板=正文"), "a book opened from the shelf by touch");
    on_ui(title_shown); char first[96]; snprintf(first, sizeof first, "%s", shown);
    on_ui(redraw); quiet();
    on_ui(title_shown); CHECK(ready("面板=正文") && first[0] && !strcmp(first, shown), "a redraw keeps the touched book (%s / %s)", first, shown);
    go_home(); open_reader("面板=书架"); CHECK(!strstr(view, "面板=正文"), "opened from home again: the shelf, not the last book");
    /* A chapter title Mogo made up is shown in the current language (the book's data keeps it as made) */
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "English");
    go_home(); ok("reader.open_book", "{\"title\":\"无标题散记\"}"); CHECK(ready("第 1 节"), "a book without contents: %.120s", view);
    go_home(); CHECK(WAIT_FOR(card_has("Section 1"), 5000), "home's card names the made-up chapter in English: %s", card.text);
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "Chinese again");
    CHECK(WAIT_FOR(card_has("第 1 节"), 5000), "and in Chinese: %s", card.text);
}
/* A member removed while the reader's cleanup is still queued: its late save writes nothing for them (the old
 * reader_late suite); with no removal the reading place is kept when the reader closes first. */
static bool ui_held, release_ui; static ink_exec_job_t held_job;
static int results; static char result_status[24];
static void result(void *ctx, const char *id, const ink_args_t *args) { (void)ctx; (void)id; ink_args_get_str(args, "status", result_status, sizeof result_status); results++; }
static bool confirmed(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    if (sscanf(g_rc.object, "confirm:%u", &token) != 1) return false;
    int before = results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(results > before, 5000) && !strcmp(result_status, "done");
}
static void hold(void *ctx) { (void)ctx; ui_held = true; while (!release_ui) osal_task_delay_us(10000); }
/* The member's reader records on /user (through the storage service): the snapshot and the log generations. */
static int prefs_files(const char *member) {
    char path[160], name[48]; int n = 0;
    for (int gen = -1; gen < 16; gen++) {
        if (gen < 0) snprintf(name, sizeof name, "mogo.preferences.snap");
        else snprintf(name, sizeof name, "mogo.preferences.%d.log", gen);
        if (ink_layout_member(path, sizeof path, member, name) < 0) return -1;
        n += ink_storage_size(path) >= 0;
    }
    return n;
}
static void late(bool control) {
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add an adult");
    CHECK(ink_member_switch("oldadult") == 0, "the adult takes the device"); quiet();
    go_home(); ok("reader.open_book", "{\"title\":\"测试长书\"}"); CHECK(ready("面板=正文"), "the family book opens");
    ok("reader.add_bookmark", NULL); /* the member has reader records before the end */
    ok("reader.goto_page", "{\"page\":3}"); CHECK(ready("页=3/"), "page 3");
    double saved = sim_metric_value("reader.last_saved");
    if (control) { go_home(); CHECK(WAIT_FOR(sim_metric_value("reader.last_saved") > saved, 5000), "leaving the book saves its place"); }
    CHECK(ink_exec_post(INK_EXEC_UI, &held_job, hold, NULL) == 0 && WAIT_FOR(ui_held, 1000), "the reader's cleanup waits behind the UI");
    CHECK(ink_member_switch("owner") == 0, "another adult takes the device");
    bool removed = false;
    if (!control) {
        CHECK(prefs_files("oldadult") > 0, "the adult had reader records (%d)", prefs_files("oldadult"));
        /* The reader still holds their book open: removal may say "not finished, retry", but the member is gone at once
         * and their records are wiped either way */
        removed = confirmed("member.remove", "{\"id\":\"oldadult\"}");
        CHECK(!ink_member_exists("oldadult"), "the member is gone (removal %s)", removed ? "done" : result_status);
        CHECK(prefs_files("oldadult") == 0, "their reader records are wiped (%d files)", prefs_files("oldadult"));
    }
    release_ui = true; quiet();
    CHECK(WAIT_FOR(!ink_storage_pending(), 5000), "late writes settled");
    if (!control) {
        CHECK(prefs_files("oldadult") == 0, "the late save wrote nothing for the removed member (%d files)", prefs_files("oldadult"));
        if (!removed) CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}"), "removal finishes once the reader has let go: %s", result_status);
        CHECK(prefs_files("oldadult") == 0, "still nothing for them (%d files)", prefs_files("oldadult"));
        CHECK(!confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}"), "the id is never given again");
    }
    else {
        CHECK(ink_member_switch("oldadult") == 0, "the adult again"); quiet();
        go_home(); ok("reader.open_book", "{\"title\":\"测试长书\"}");
        CHECK(ready("面板=正文") && strstr(view, "页=3/"), "the place is kept: %.120s", view);
    }
}
/* After a member switch, BG work for the member who came in can run before the old page is let go: the shelf job
 * (ink_mogo_shelf_query, what reader.list_books runs) sets the engine's member to theirs. The page's late save still
 * writes for the page's member only, never into theirs: their 继续读 and home card stay empty. */
int ink_mogo_shelf_query(const ink_member_identity_t *owner, char **json, size_t *cap, const char *title, int index, char *path, size_t path_cap);
static volatile bool bg_passed; static ink_exec_job_t bg_job;
static void their_shelf(void *ctx) { (void)ctx; ink_member_identity_t id; ink_member_identity(&id); (void)ink_mogo_shelf_query(&id, NULL, NULL, "", -1, NULL, 0); bg_passed = true; }
static void bg_mark(void *ctx) { (void)ctx; bg_passed = true; }
/* A turn's pass on BG is over (it looked for a close and found none) and its answer waits behind this UI job, which
 * leaves the reader: the close comes while the page is still busy. */
static void leave_during_turn(void) {
    ink_call_opts_t o = {.src = INK_SRC_AI};
    bg_passed = false;
    bool posted = ink_cap_call("reader.next_page", NULL, &o) == 0 && !ink_exec_post(INK_EXEC_BG, &bg_job, bg_mark, NULL);
    for (int i = 0; posted && !bg_passed && i < 5000; i++) osal_task_delay_us(1000);
    ink_ui_home();
}
static void nothing_for_owner(void) {
    CHECK(WAIT_FOR(!ink_storage_pending(), 5000), "late writes settled");
    CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "the adult's book is not the owner's to continue (%d %s)", g_rc.status, g_rc.say);
    /* the card is read again for the member who came in 3 s after the switch (mogo.c ink_mogo_kick) */
    CHECK(!WAIT_FOR(card_has("测试长书"), 5000), "the owner's home card does not show the adult's book: %s", card.text);
}
static void late_other(bool turn) {
    CHECK(AI("reader.continue", NULL) && g_rc.status == INK_RC_FAILED, "the owner has no book to continue yet (%d %s)", g_rc.status, g_rc.detail);
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add an adult");
    CHECK(ink_member_switch("oldadult") == 0, "the adult takes the device"); quiet();
    go_home(); ok("reader.open_book", "{\"title\":\"测试长书\"}"); CHECK(ready("面板=正文"), "the family book opens");
    ok("reader.goto_page", "{\"page\":3}"); CHECK(ready("页=3/"), "page 3, not saved yet");
    if (turn) {
        double saved = sim_metric_value("reader.last_saved");
        on_ui(leave_during_turn); quiet();
        CHECK(bg_passed && !top_is("book"), "the reader was left while its turn was answered (%s)", ink_ui_top_name());
        CHECK(WAIT_FOR(sim_metric_value("reader.last_saved") > saved, 5000), "leaving during a turn still saves the place");
        CHECK(ink_member_switch("owner") == 0, "the owner takes the device"); quiet();
        ok("reader.list_books", NULL);
        open_reader("面板=书架"); go_home();
        nothing_for_owner();
        CHECK(ink_member_switch("oldadult") == 0, "the adult again"); quiet();
        go_home(); ok("reader.open_book", "{\"title\":\"测试长书\"}");
        /* the first chapter has 3 pages: the turn from its last page reached the second chapter's first */
        CHECK(ready("面板=正文") && strstr(view, "章节=2 页=1/"), "the place the turn reached is kept: %.160s", view);
        return;
    }
    CHECK(ink_exec_post(INK_EXEC_UI, &held_job, hold, NULL) == 0 && WAIT_FOR(ui_held, 1000), "the reader is not let go yet");
    CHECK(ink_member_switch("owner") == 0, "the owner takes the device");
    bg_passed = false;
    CHECK(ink_exec_post(INK_EXEC_BG, &bg_job, their_shelf, NULL) == 0 && WAIT_FOR(bg_passed, 5000), "the owner's shelf job ran on BG");
    release_ui = true; quiet();
    nothing_for_owner();
}
static void run(void *arg) {
    (void)arg; ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); quiet();
    snprintf(phase, sizeof phase, "%s", env("INK_READER_ONE_PHASE"));
    if (!strcmp(phase, "nofont")) missing_font();
    else if (!strcmp(phase, "nocard")) no_card();
    else if (!strcmp(phase, "empty")) empty_shelf();
    else if (!strcmp(phase, "badown")) broken_own_font();
    else if (!strcmp(phase, "places")) places();
    else if (!strcmp(phase, "bookmarks")) bookmarks();
    else if (!strcmp(phase, "seed")) seed(false);
    else if (!strcmp(phase, "seed-extra")) seed(true);
    else if (!strcmp(phase, "boot")) boot_checks(0);
    else if (!strcmp(phase, "again")) boot_checks(1);
    else if (!strcmp(phase, "restored")) boot_checks(2);
    else if (!strcmp(phase, "boot-nocard")) no_card_boot();
    else if (!strcmp(phase, "review")) review();
    else if (!strcmp(phase, "late")) late(false);
    else if (!strcmp(phase, "late-control")) late(true);
    else if (!strcmp(phase, "late-other")) late_other(false);
    else if (!strcmp(phase, "late-turn")) late_other(true);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "test.reader-one", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the reader-one checks");
}
