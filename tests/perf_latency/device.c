/* Virtual-time measurements on the real product; all data is synthetic. */
#include <stdlib.h>
#include <string.h>
#include "ink/sys/textinput.h"
#include "ink/sys/task.h"
#include "ink/ui_shell.h"
#include "image_fixtures.h"
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

extern unsigned perf_activity_reads(void);
extern size_t perf_log_count(ink_db_t *, int64_t, unsigned *);
extern bool perf_decode_active(void);
extern int ink_activity_home_offset(void);
extern bool ink_activity_home_draw(lv_obj_t *, int);
static bool regression;
static const char *tag, *launch_id;
static uint64_t began;
static char body[3001];
static ink_db_t *log_db;
static int64_t retention;
static size_t expected;
static int tap_x, tap_y;

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "display settles"); }
static void noop(void) {}
static void locate_control(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.control")) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a);
        tap_x = (a.x1 + a.x2) / 2; tap_y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) locate_control(lv_obj_get_child(o, i));
}
static void locate(void) { tap_x = tap_y = -1; locate_control(lv_screen_active()); }
static void open_app(void) {
    ink_receipt_t rc;
    ink_args_t args; ink_args_init(&args); ink_args_str(&args, "id", launch_id);
    ink_cap_invoke("shell.launch", &args, INK_SRC_TOUCH, 0, &rc);
    ink_ui_cause(began, tag);
    CHECK(rc.status == INK_RC_DONE, "open %s", launch_id);
}
static void home(void) { CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "home"); quiet(); }
static void turn(void) { ink_ui_cause(began, tag); }
static void open_log(void) {
    ink_receipt_t rc;
    ink_cap_invoke("shell.ai_log", NULL, INK_SRC_TOUCH, 0, &rc);
    ink_ui_cause(began, "perf.open_log_ms");
    CHECK(rc.status == INK_RC_DONE, "open populated AiLog");
}
static void activity(void) {
    lv_obj_t *root = lv_obj_create(NULL);
    perf_activity_reads();
    uint64_t start = osal_now_us();
    ink_activity_home_offset();
    ink_activity_home_draw(root, 100);
    ink_probe_value("perf.activity_read_ms", (osal_now_us() - start) / 1000.0);
    unsigned n = perf_activity_reads();
    ink_probe_value("perf.activity_job_reads", n);
    CHECK(!regression || n == 1, "home reads job.list once (got %u)", n);
    lv_obj_delete(root);
}
static void count_log(void) {
    unsigned steps;
    uint64_t start = osal_now_us();
    size_t n = perf_log_count(log_db, retention, &steps);
    ink_probe_value("perf.log_count_ms", (osal_now_us() - start) / 1000.0);
    ink_probe_value("perf.log_scans", steps);
    CHECK(n == expected, "count keeps zero sort and inclusive retention (%zu/%zu)", n, expected);
    CHECK(!regression || steps <= 4, "recent records need no full scan (%u)", steps);
}
static void type_key(void) {
    uint64_t start = osal_now_us();
    ink_text_key('n');
    ink_probe_value("perf.text_key_ms", (osal_now_us() - start) / 1000.0);
}
static void focus(void) {
    memset(body, 'A', sizeof body - 1);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095, .initial = body, .draft_page = "perf.synthetic"};
    CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "open synthetic long draft");
}
static void blur(void) { ink_text_blur(0); }
static volatile bool thumb_done;
static void thumb_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    CHECK(rc->status == INK_RC_DONE, "thumbnail completes");
    thumb_done = true;
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens");
    on_ui(noop); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "home"), "configured boot reaches home");
    ink_probe_value("perf.boot_home_ms", osal_now_us() / 1000.0);
    if (getenv("INK_PERF_BOOT_ONLY")) { ink_probe_count("test.finished"); vtk_stop(); return; }
    const char *const ids[] = {"settings", "files", "gallery", "message", "ai"};
    const char *const tags[] = {"perf.open_settings_ms", "perf.open_files_ms", "perf.open_gallery_ms", "perf.open_sms_ms", "perf.open_ai_ms"};
    for (unsigned i = 0; i < 5; i++) {
        home(); launch_id = ids[i]; tag = tags[i]; began = osal_now_us();
        double before = sim_metric_value(tag);
        on_ui(open_app); quiet();
        CHECK(sim_metric_value(tag) > before, "first readable frame measured for %s", ids[i]);
    }
    home();
    double before = sim_metric_value("ui.action_to_first_change_ms");
    on_ui(locate); CHECK(tap_x >= 0, "control centre touch target exists");
    if (tap_x >= 0) tap(tap_x, tap_y);
    quiet();
    CHECK(sim_metric_value("ui.action_to_first_change_ms") > before, "physical tap feedback measured");
    ink_probe_value("perf.tap_first_change_ms", sim_metric_last("ui.action_to_first_change_ms"));
    CHECK(BY_HAND("shell.close", NULL), "close control centre"); quiet();
    CHECK(BY_HAND("app.open", "{\"id\":\"reader\"}") && g_rc.status == INK_RC_DONE, "open library"); quiet();
    CHECK(BY_HAND("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "open test book"); quiet();
    tag = "perf.page_readable_ms"; began = osal_now_us(); on_ui(turn);
    before = sim_metric_value(tag);
    CHECK(BY_HAND("reader.next_page", NULL) && g_rc.status == INK_RC_DONE, "turn book page"); quiet();
    CHECK(sim_metric_value(tag) > before, "book page readable measured");
    home();
    for (unsigned i = 0; i < 20; i++) on_ui(activity);
    log_db = ink_db_open_member(ink_db_find_def("activity.log"), "owner");
    CHECK(log_db != NULL, "open activity fixture");
    int64_t now = osal_wall_us() / 1000000;
    retention = 365ll * 86400; expected = 513;
    for (unsigned i = 0; i < 514; i++) {
        int64_t sort = i == 0 ? 0 : i == 1 ? now - retention - 60 : i == 2 ? now - retention + 60 : now;
        ink_db_write_t w = {.sort = sort, .data = "{}", .len = 2, .flags = INK_DB_LAZY};
        CHECK(ink_db_put(log_db, &w, NULL) == INK_DB_OK, "seed activity %u", i);
    }
    for (unsigned i = 0; i < 20; i++) on_ui(count_log);
    retention = 180ll * 86400; expected = 512; on_ui(count_log);
    ink_db_close(log_db);
    began = osal_now_us(); on_ui(open_log); quiet(); home();
    on_ui(focus); quiet();
    for (unsigned i = 0; i < 20; i++) { on_ui(type_key); quiet(); }
    on_ui(blur); quiet();
    CHECK(ink_fs_write_atomic("/me/perf-large.png", max_png, sizeof max_png) == 0, "write synthetic image");
    uint32_t cursor = 0, id = 0; ink_fs_entry_t entry;
    while (ink_fs_index("/me", INK_FS_IMAGE, &cursor, &entry) == 1)
        if (!strcmp(entry.path, "/me/perf-large.png")) id = entry.id;
    CHECK(id != 0, "image indexed");
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", id);
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = thumb_reply};
    CHECK(ink_cap_call("file.thumb", NULL, &opts) == 0, "request cold thumbnail");
    CHECK(WAIT_FOR(perf_decode_active(), 10000), "real thumbnail decoding starts");
    began = osal_now_us();
    CHECK(BY_HAND("file.lookup", args) && g_rc.status == INK_RC_DONE, "lookup during decode");
    ink_probe_value("perf.lookup_during_thumbnail_ms", (osal_now_us() - began) / 1000.0);
    CHECK(!regression || perf_decode_active(), "file lookup completes while thumbnail decoding continues");
    CHECK(WAIT_FOR(thumb_done, 10000), "thumbnail reply delivered");
    ink_probe_count("test.finished");
    vtk_stop();
}
void ink_main(void) {
    regression = getenv("INK_PERF_REGRESSION") != NULL;
    product_main();
    osal_task_cfg_t cfg = {.name = "test-perf-latency", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start latency test");
}
