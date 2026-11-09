#include "../services_finish/product.h"
#include "ink/ui_shell.h"

unsigned ink_test_storage_reads(void);
unsigned ink_test_storage_organizing(void);
static const char *const kinds[] = {"books", "audio", "photos", "other"};
static const char *const paths[] = {"/me/SYNTH_BOOK.epub", "/me/SYNTH_AUDIO.mp3", "/me/SYNTH_PHOTO.jpg", "/me/SYNTH_OTHER.bin"};
static const int sizes[2][4] = {{17, 23, 31, 7}, {19, 29, 37, 11}};
static int64_t base[4];
static bool initial_organizing, captured;
static char action_args[128], action_cap[64];
static int label_x, label_y;

static int64_t number(const char *scope, const char *key) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t v = -1;
    if (j && !ink_json_parse(j, g_result, strlen(g_result))) {
        int root = scope ? ink_json_find(j, 0, scope) : 0;
        ink_json_int(j, ink_json_find(j, root, key), &v);
    }
    osal_free(j); return v;
}
static void write_file(const char *path, int n) {
    char data[64]; memset(data, 'X', sizeof data);
    CHECK(n <= (int)sizeof data && ink_fs_write_atomic(path, data, (size_t)n) == 0, "synthetic file %s", path);
}
static void raw_file(const char *path, int n) {
    char data[64]; memset(data, 'X', sizeof data);
    hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f, data, (size_t)n) == n, "synthetic reserved backup fixture");
    if (f) CHECK(!hal_fs_close(f), "close reserved fixture");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void read_page(void) { CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0, "complete production AI snapshot"); }
static void open_storage(void) {
    ink_settings_page_open("SetStorage"); read_page();
    initial_organizing = strstr(g_snap, "正在整理") != NULL;
}
static bool usage_visible(void) {
    on_ui(read_page);
    return strstr(g_snap, "我 17 B") && !strstr(g_snap, "正在整理");
}
static void capture_event(void *arg) {
    (void)arg; CHECK(settled(), "settled comparison frame");
    sim_display_snapshot("scene"); captured = true;
}
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), "清理临时文件")) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void inspect_action(void) {
    read_page();
    const char *p = strstr(g_snap, "\"actions\":");
    p = p ? strstr(p, "\"id\":\"storage.clean\"") : NULL;
    const char *end = p ? strstr(p, "}}") : NULL;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && p && end && !ink_json_parse(j, p - 1, (size_t)(end - p) + 3)) {
        ink_json_str(j, ink_json_find(j, 0, "capability"), action_cap, sizeof action_cap);
        ink_json_str(j, ink_json_find(j, 0, "args_json"), action_args, sizeof action_args);
    }
    osal_free(j); label_x = label_y = -1; find_label(lv_screen_active());
}
static void cleanup_args(char *out, size_t cap) {
    CHECK(AI("file.cleanup_preview", NULL) && g_rc.status == INK_RC_DONE, "AI reads cleanup preview");
    snprintf(out, cap, "{\"revision\":%lld}", (long long)number(NULL, "revision"));
}
static bool have_clean_action(void) {
    on_ui(inspect_action);
    return action_cap[0] && action_args[0];
}
static void test_cleanup(void) {
    CHECK(BY_HAND("system.home", NULL), "leave storage before fixtures");
    write_file("/me/orphan.txt.tmp", 5);
    write_file("/me/收件箱/unfinished.bin.part", 7);
    write_file("/cache/sys/thumbs/SYNTH.raw", 11);
    write_file("/me/SYNTH_USER.bin", 13);
    write_file("/me/folder.tmp/keep.bin", 19);
    CHECK(hal_fs_mkdir("/content/.m/owner/.backup") == 0, "reserved backup directory");
    raw_file("/content/.m/owner/.backup/keep.part", 17);
    char args[128]; cleanup_args(args, sizeof args);
    CHECK(number(NULL, "temporary") == 5 && number(NULL, "cache") == 11 && number(NULL, "partial") == 7 && number(NULL, "bytes") == 23,
        "preview reports exactly three safe categories and 23 bytes");
    CHECK(AI("file.cleanup", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "irreversible AI cleanup requires confirmation");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "confirmation token");
    CHECK(hal_fs_size("/content/.m/owner/orphan.txt.tmp") == 5, "confirmation has no deletion side effect");
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "human can cancel cleanup");
    CHECK(AI("shell.storage", NULL) && WAIT_FOR(settled(), 10000), "open real cleanup row");
    for (int i = 0; i < 4 && !have_clean_action(); i++) CHECK(AI("shell.settings_next", NULL), "page to cleanup");
    CHECK(WAIT_FOR(have_clean_action(), 10000), "cleanup preview automatically arrives");
    CHECK(!strcmp(action_cap, "file.cleanup") && !strcmp(action_args, args), "button and AI use identical capability and revision");
    CHECK(strstr(g_snap, "能腾出 23 B") && strstr(g_snap, "临时文件、缓存和未传完"), "visible summary lists cleanup categories and bytes");
    CHECK(label_x >= 0 && label_y >= 0, "cleanup row has a touch target");
    tap(label_x, label_y);
    ink_confirm_view_t card;
    CHECK(WAIT_FOR(ink_confirm_list(&card, 1) == 1, 2000), "actual button opens confirmation");
    CHECK(!strcmp(card.cap, "file.cleanup") && ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "human confirms button cleanup");
    CHECK(WAIT_FOR(hal_fs_size("/content/.m/owner/orphan.txt.tmp") < 0, 5000), "temporary file removed");
    CHECK(hal_fs_size("/content/.m/owner/收件箱/unfinished.bin.part") < 0 && hal_fs_size("/cache/sys/thumbs/SYNTH.raw") < 0,
        "partial transfer and thumbnail cache removed");
    CHECK(hal_fs_size("/content/.m/owner/SYNTH_USER.bin") == 13 && hal_fs_size("/content/.m/owner/folder.tmp/keep.bin") == 19 &&
        hal_fs_size("/content/.m/owner/.backup/keep.part") == 17, "ordinary user files, suffix directories and backups survive cleanup");
    cleanup_args(args, sizeof args);
    CHECK(number(NULL, "bytes") == 0, "removed bytes equal displayed reclaimable bytes");
    CHECK(BY_HAND("system.home", NULL), "leave storage for cache-only check");
    write_file("/cache/sys/thumbs/SYNTH.raw", 11); cleanup_args(args, sizeof args);
    write_file("/me/SYNTH_LATER.bin", 1);
    CHECK(AI("file.cache_clear", args) && g_rc.status == INK_RC_FAILED && hal_fs_size("/cache/sys/thumbs/SYNTH.raw") == 11,
        "changed files invalidate preview before any deletion");
    cleanup_args(args, sizeof args);
    CHECK(AI("file.cache_clear", args) && g_rc.status == INK_RC_DONE && number(NULL, "bytes") == 11,
        "regenerable cache needs no confirmation and reports released bytes");
}
static void child_open(void) {
    ink_settings_page_open("SetStorage");
    CHECK(strcmp(ink_ui_page_name(), "SetStorage"), "child sees no cleanup page through direct navigation");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); test_start_home();
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "initial device index");
    for (int k = 0; k < 4; k++) base[k] = number("device", kinds[k]);
    CHECK(BY_HAND("member.add", "{\"id\":\"guest\",\"role\":\"adult\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "add synthetic second adult");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "authorize synthetic member");
    CHECK(WAIT_FOR(ink_member_exists("guest"), 5000), "second adult exists");
    for (int k = 0; k < 4; k++) write_file(paths[k], sizes[0][k]);
    CHECK(ink_member_switch("guest") == 0, "switch to second adult");
    for (int k = 0; k < 4; k++) write_file(paths[k], sizes[1][k]);
    CHECK(ink_member_switch("owner") == 0, "return to owner");
    on_ui(open_storage);
    CHECK(initial_organizing, "unfinished index displays 正在整理 without invented zeroes");
    CHECK(WAIT_FOR(usage_visible(), 10000), "completed index automatically refreshes screen and AI snapshot");
    CHECK(ink_test_storage_organizing() > 0, "real index traversal was observed while organizing");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "AI reads the same indexed usage");
    for (int k = 0; k < 4; k++) {
        CHECK(number("member", kinds[k]) == sizes[0][k], "owner category %s exact bytes", kinds[k]);
        CHECK(number("device", kinds[k]) == base[k] + sizes[0][k] + sizes[1][k], "device category %s exact bytes", kinds[k]);
    }
    unsigned reads = ink_test_storage_reads();
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && ink_test_storage_reads() == reads,
        "unchanged usage query performs no directory traversal");
    CHECK(WAIT_FOR(settled(), 10000), "storage display settles");
    vtk_event_at(vtk_now() + SIM_MS(1), capture_event, NULL);
    CHECK(WAIT_FOR(captured, 2000), "capture storage comparison evidence");
    ink_probe_count("test.snapshot.SetStorage"); ink_probe_count("test.snapshot.settled");
    CHECK(ink_member_switch("guest") == 0, "switch member for second usage query");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "second member usage");
    for (int k = 0; k < 4; k++) {
        CHECK(number("member", kinds[k]) == sizes[1][k], "personal category changes with member %s", kinds[k]);
        CHECK(number("device", kinds[k]) == base[k] + sizes[0][k] + sizes[1][k], "device category unchanged %s", kinds[k]);
    }
    CHECK(ink_member_switch("owner") == 0, "return before cleanup"); test_cleanup();
    CHECK(ink_member_switch("kid") == 0, "enter child space");
    CHECK(AI("shell.storage", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open cleanup page via AI");
    CHECK(AI("file.cleanup_preview", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot list device cleanup");
    CHECK(AI("file.cleanup", "{\"revision\":1}") && g_rc.status == INK_RC_REJECTED, "child cannot clean via AI");
    CHECK(BY_HAND("file.cache_clear", "{\"revision\":1}") && g_rc.status == INK_RC_REJECTED, "child cannot clean cache by hand");
    on_ui(child_open); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "storage-usage-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
