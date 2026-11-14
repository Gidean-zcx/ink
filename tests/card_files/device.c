/* Card files on the whole product (auto-provision-1): one boot per phase (INK_CF_PHASE), driven by tests/card_files/run.py,
 * which prepares the card, starts the fake install tool and checks the card and the probes afterwards. */
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"
#include "graft.h"
#include "ink/sys/appreg.h"
#include "ink/sys/card_files.h"
#include "ink/sys/file.h"
#include <stdlib.h>

static char view[2048], files[16], found[256];
static volatile bool captured, cleanup_done;
static ink_exec_job_t cleanup_barrier;
static const char *phase;

static void inspect(void) { ink_graft_snapshot(view, sizeof view); }
static const char *files_state(void) {
    ink_state_get_str(ink_state_find("card.files"), files, sizeof files);
    return files;
}
static bool state_is(const char *want, int ms) { return WAIT_FOR(!strcmp(files_state(), want), ms); }
static bool shows(const char *needle) { /* the page's snapshot, settled */
    for (int i = 0; i < 1200; ++i) {
        on_ui(inspect);
        if (strstr(view, needle) && !ink_ui_busy() && !ink_display_busy()) return true;
        delay_ms(50);
    }
    return false;
}
static const char *g_needle;
static void find_label(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), g_needle))
        snprintf(found, sizeof found, "%s", lv_label_get_text(o));
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) find_label(lv_obj_get_child(o, (int32_t)i));
}
static void look(void) { found[0] = 0; find_label(lv_screen_active()); }
static lv_obj_t *screen_now;
static void grab_screen(void) { screen_now = lv_screen_active(); }
static lv_obj_t *active_screen(void) { on_ui(grab_screen); return screen_now; }
static bool on_screen(const char *needle) { /* drawn, not only described */
    g_needle = needle;
    on_ui(look);
    return found[0] != 0;
}
static void capture(void *name) { sim_display_snapshot(name); captured = true; }
static void frame(const char *name) {
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "capture %s", name);
}
static void after_cleanup(void *arg) { (void)arg; cleanup_done = true; }
static void close_reader(void) { CHECK(!ink_app_close("reader"), "close the reader"); }
static void close_book(void) {
    cleanup_done = false;
    on_ui(close_reader);
    ink_exec_post(INK_EXEC_BG, &cleanup_barrier, after_cleanup, NULL);
    CHECK(WAIT_FOR(cleanup_done, 15000), "reader cleanup finished");
}
static ink_app_state_t music_state;
static void read_music_state(void) { music_state = ink_app_state_of("music"); }
static void open_book(void) { (void)AI("reader.open_book", "{\"title\":\"\",\"path\":\"/me/books/书.txt\"}"); }
static void eject(void *arg) { (void)arg; sim_usb_eject(); }
static void stop(void *arg) { (void)arg; vtk_stop(); }

static void notice(const char *words, const char *shot) {
    open_book(); /* the app opens (queued) and its page says why */
    CHECK(shows(words), "snapshot names it: %.200s", view);
    CHECK(on_screen(words), "the page shows it, not a blank page: %.200s", view);
    if (shot) frame(shot);
    open_book(); /* asked again on the open page: the receipt says why too */
    CHECK(g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "font not on the card"), "open says why: rc=%d %.80s", g_rc.status, g_rc.detail);
    CHECK(shows(words) && on_screen(words), "still explained: %.200s", view);
}

/* A page that cannot use a file the list has as ready (its own trouble, or the file changed since the last check): the
 * notice checks once; when that check finds the file right it says so and what to do, never "checking" for ever, and it
 * does not reopen the app in a loop. */
static int reopened;
static lv_obj_t *ready_box;
static char said[256];
static void count_reopen(void) { reopened++; }
static void put_notice(void) { ready_box = ink_card_files_notice(lv_screen_active(), "reader", "字体", count_reopen); }
static void take_notice(void) { if (ready_box) lv_obj_delete(ready_box); ready_box = NULL; }
static void read_notice(void) { ink_card_files_notice_snapshot("reader", said, sizeof said); }
static bool notice_says(const char *needle, int ms) {
    for (int i = 0; i < ms / 50; i++) { on_ui(read_notice); if (strstr(said, needle)) return true; delay_ms(50); }
    return false;
}
static void ready_notice(void) {
    double checks = sim_metric_value("card_files.check");
    on_ui(put_notice);
    CHECK(notice_says("正在重新检查", 1000), "a ready file the app cannot use: it checks again: %.200s", said);
    CHECK(WAIT_FOR(sim_metric_value("card_files.check") > checks, 10000), "the card is checked again");
    CHECK(notice_says("检查过是好的", 60000) && on_screen("请返回后再打开试试"), "after the check: the file is right, go back and try again: %.200s", said);
    CHECK(state_is("ready", 1000) && !reopened && sim_metric_value("card_files.check") == checks + 1,
          "no reopening and no second check (reopened %d, checks %g)", reopened, sim_metric_value("card_files.check") - checks);
    on_ui(take_notice);
}

static void run(void *arg) {
    (void)arg;
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_hold(INK_NEED_NO_LOCK);
    bool plug = strcmp(phase, "E3") && strcmp(phase, "I") && strcmp(phase, "J") && strcmp(phase, "K"); /* I, J, K: the computer comes later */
    if (plug) hw(HW_USB_ON, 0, 0);
    if (!strcmp(phase, "A")) { /* a computer, no install tool */
        CHECK(state_is("missing", 20000), "missing without the tool: %s", files);
        notice("字体还没放到存储卡上", "notice-default");
        CHECK(strstr(view, "card.files=missing"), "snapshot has the state word: %.200s", view);
        close_book();
        CHECK(BY_HAND("settings.set", "{\"id\":\"display.font_size\",\"value\":3}") && g_rc.status == INK_RC_DONE, "largest text size");
        notice("字体还没放到存储卡上", "notice-largest");
        close_book();
        CHECK(BY_HAND("settings.set", "{\"id\":\"display.font_size\",\"value\":1}") && g_rc.status == INK_RC_DONE, "default text size");
    } else if (!strcmp(phase, "B") || !strcmp(phase, "E") || !strcmp(phase, "E2")) {
        CHECK(state_is("ready", 120000), "fetched and installed: %s", files);
        if (!strcmp(phase, "B")) {
            open_book();
            CHECK(shows("面板=正文"), "the reader opens a book with the fetched font: %.200s", view);
            close_book();
        }
    } else if (!strcmp(phase, "C")) { /* everything there: only HELLO ... 0 */
        CHECK(state_is("ready", 60000), "ready again: %s", files);
        ready_notice();
        delay_ms(5000);
    } else if (phase[0] == 'D') { /* D, D2: the cable goes in the middle */
        CHECK(state_is("failed", 120000), "the session ends when the cable goes: %s", files);
    } else if (!strcmp(phase, "E3")) { /* a whole .part, no computer */
        CHECK(state_is("ready", 60000), "a whole .part is installed here: %s", files);
    } else if (!strcmp(phase, "F")) {
        CHECK(state_is("failed", 120000), "wrong bytes are not installed: %s", files);
        notice("没放成", "notice-failed");
        close_book();
    } else if (!strcmp(phase, "G1")) {
        CHECK(state_is("missing", 20000), "another bundle counts as no tool: %s", files);
        delay_ms(5000);
    } else if (!strcmp(phase, "G2")) {
        CHECK(state_is("missing", 20000), "missing: %s", files);
        delay_ms(35000); /* the whole HELLO window: DATA without SERVE writes nothing */
    } else if (!strcmp(phase, "H")) { /* the card lent to the computer while the reader opens */
        CHECK(state_is("ready", 60000), "ready before lending: %s", files);
        CHECK(ink_usb_disk_start() == INK_FS_OK, "card lent to the computer");
        notice("存储卡正给电脑用", "notice-lent");
        CHECK(!strstr(view, "没有存储卡"), "not called a missing card: %.200s", view);
        /* A conclusion, not a started check: while the card is lent no check concludes (it waits, as it was), so one
         * after the eject read the card after it. A check asked for before the eject may still be waiting for a quiet
         * screen when the card comes back; it is then the one that reads it, and the eject's own request waits behind
         * it and the computer's HELLO window, so counting started checks depends on where that one was. */
        unsigned concluded = ink_card_files_checks();
        lv_obj_t *page = active_screen();
        vtk_event_at(vtk_now() + SIM_MS(1), eject, NULL);
        CHECK(WAIT_FOR(ink_card_files_checks() > concluded, 10000), "checked again after the eject");
        CHECK(state_is("ready", 60000), "ready after the eject: %s", files);
        CHECK(shows("面板=正文") && !on_screen("存储卡正给电脑用"), "the reader comes back by itself, in the book: %.200s", view);
        CHECK(active_screen() == page, "the same screen: no rebuilt page, no blank one in between");
        close_book();
    } else if (!strcmp(phase, "I")) { /* a wrong font (an interrupted copy) while the reader explains it */
        CHECK(state_is("missing", 20000), "a wrong-sized font is missing: %s", files);
        notice("还没放到存储卡上", NULL);
        lv_obj_t *page = active_screen();
        hw(HW_USB_ON, 0, 0); /* the computer: the old file goes while the notice is up (the reader holds no font) */
        CHECK(state_is("ready", 120000), "replaced under the notice: %s", files);
        CHECK(shows("面板=正文") && !on_screen("还没放到存储卡上") && !on_screen("正在从电脑放"), "the reader opens the book: %.200s", view);
        CHECK(active_screen() == page, "the same screen: no rebuilt page, no blank one in between");
        close_book();
    } else if (!strcmp(phase, "J")) { /* a wrong font the open reader is using: not replaced under it, and said so */
        CHECK(state_is("missing", 20000), "a wrong font needs fetching: %s", files);
        open_book();
        CHECK(shows("面板=正文"), "the reader opens with the font it has: %.200s", view);
        hw(HW_USB_ON, 0, 0);
        CHECK(state_is("failed", 120000), "fetched but not installed under the open reader: %s", files);
        CHECK(shows("面板=正文"), "the reader is left alone: %.200s", view);
        close_book();
        CHECK(AI("card.files_check", "{}") && g_rc.status == INK_RC_DONE, "check again: rc=%d", g_rc.status);
        CHECK(state_is("ready", 60000), "the whole .part goes in once the reader is closed: %s", files);
    } else if (!strcmp(phase, "K")) { /* the font is shared: music used since boot and left (not stopped) holds none of it */
        CHECK(state_is("missing", 20000), "a wrong-sized font is missing: %s", files);
        CHECK(AI("app.open", "{\"id\":\"music\"}") && g_rc.status == INK_RC_DONE, "open music: rc=%d", g_rc.status);
        CHECK(WAIT_FOR(on_screen("还没放到存储卡上"), 20000), "music explains the font as the reader does");
        notice("还没放到存储卡上", NULL); /* the reader opens over it: music leaves the screen and closes the font */
        on_ui(read_music_state);
        CHECK(music_state != INK_APP_STOPPED, "music is left in the background, not stopped: %d", (int)music_state);
        lv_obj_t *page = active_screen();
        hw(HW_USB_ON, 0, 0);
        CHECK(state_is("ready", 120000), "replaced under the reader's notice with music in the background: %s", files);
        CHECK(shows("面板=正文") && !on_screen("还没放到存储卡上"), "the reader opens the book: %.200s", view);
        CHECK(active_screen() == page, "the same screen: no rebuilt page, no blank one in between");
        close_book();
    }
    sim_note("card files: phase %s finished", phase);
    ink_probe_count("test.finished");
    vtk_event_at(vtk_now() + SIM_MS(10), stop, NULL);
}

void ink_main(void) {
    phase = getenv("INK_CF_PHASE");
    if (!phase) phase = "A";
    product_main();
    test_start_home();
    osal_task_cfg_t cfg = {.name = "test.cards", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1,
                           .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start card file checks");
}
