/* File latency during repeated UI redraws, on synthetic card data. */
#include "../services_finish/product.h"
#include "ink/ui_shell.h"

unsigned ink_test_index_path_id(const char *path);
unsigned ink_test_index_marker_writes(void);
bool ink_test_index_idle(void);
unsigned ink_test_index_load_size_queries(void);
static unsigned notifications;
static void indexed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    char kind[32];
    if (ink_args_get_str(a, "kind", kind, sizeof kind) > 0 && !strcmp(kind, "indexed")) notifications++;
}
static bool ready(void) {
    char state[32] = "";
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool completed(void) {
    return ready() && ink_test_index_idle() && !ink_storage_pending() && settled() && hal_fs_size("/content/.file-index-dirty") < 0;
}
static void overwrite(void) {
    CHECK(ink_test_index_load_size_queries() == 0, "known index loads add zero card size queries");
    for (int atomic = 0; atomic < 2; atomic++) {
        unsigned markers = ink_test_index_marker_writes(), notices = notifications;
        double runs = sim_metric_value("file.index.background_runs");
        for (int i = 0; i < 100; i++) {
            if (atomic) CHECK(!ink_fs_write_atomic("/family/SYNTHETIC_OLD.txt", "XXXXX", 5), "atomic overwrite");
            else {
                ink_fs_file_t *f = NULL;
                CHECK(!ink_fs_open("/family/SYNTHETIC_OLD.txt", INK_FS_WRITE | INK_FS_CREATE, &f), "open existing file");
                CHECK(ink_fs_write(f, "XXXXX", 5) == 5, "overwrite existing file");
                CHECK(!ink_fs_close(f), "close existing file");
            }
            CHECK(WAIT_FOR(completed(), 30000), "overwrite and any scheduled indexing settle");
        }
        char key[64];
        snprintf(key, sizeof key, "test.durable.%s.markers", atomic ? "atomic" : "open");
        ink_probe_value(key, ink_test_index_marker_writes() - markers);
        snprintf(key, sizeof key, "test.durable.%s.runs", atomic ? "atomic" : "open");
        ink_probe_value(key, sim_metric_value("file.index.background_runs") - runs);
        snprintf(key, sizeof key, "test.durable.%s.notifications", atomic ? "atomic" : "open");
        ink_probe_value(key, notifications - notices);
        CHECK(ink_test_index_marker_writes() == markers, "100 overwrites add zero marker writes");
        CHECK(sim_metric_value("file.index.background_runs") == runs, "100 overwrites add zero background runs");
        CHECK(notifications == notices, "100 overwrites add zero indexed notifications");
        CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"books\":5"),
            "overwrite updates usage without reindexing: %.150s", g_result);
    }
}
static void mutations(void) {
    ink_fs_file_t *f = NULL;
    CHECK(!ink_fs_open("/family/SYNTHETIC_NEW.txt", INK_FS_CREATE | INK_FS_WRITE, &f), "create through open");
    CHECK(hal_fs_size("/content/.file-index-dirty") == 0, "new open marks card before returning");
    CHECK(ink_fs_write(f, "XXXX", 4) == 4 && !ink_fs_close(f), "finish new file");
    CHECK(WAIT_FOR(completed(), 30000), "new file and completion record become durable");
    unsigned id = ink_test_index_path_id("/content/SYNTHETIC_NEW.txt");
    CHECK(id != 0, "new open receives an id");
    CHECK(!ink_fs_move("/family/SYNTHETIC_NEW.txt", "/family/SYNTHETIC_MOVED.txt"), "move indexed file");
    CHECK(hal_fs_size("/content/.file-index-dirty") == 0, "move marks its destination before returning");
    CHECK(WAIT_FOR(completed(), 30000), "move and completion record become durable");
    ink_fs_entry_t entry;
    CHECK(!ink_fs_lookup(id, &entry) && !strcmp(entry.path, "/family/SYNTHETIC_MOVED.txt"), "move preserves id");
    int job = ink_fs_copy_start("/family/SYNTHETIC_MOVED.txt", "/family/SYNTHETIC_COPY.txt");
    CHECK(job > 0 && hal_fs_size("/content/.file-index-dirty") == 0, "copy marks its destination before returning");
    CHECK(WAIT_FOR(completed(), 30000), "copy is indexed and durable");
    CHECK(ink_test_index_path_id("/content/SYNTHETIC_COPY.txt") != 0, "copy receives an id");
    unsigned markers = ink_test_index_marker_writes();
    double runs = sim_metric_value("file.index.background_runs");
    CHECK(!ink_fs_remove("/family/SYNTHETIC_COPY.txt"), "remove indexed file");
    CHECK(WAIT_FOR(completed(), 30000), "deletion settles");
    CHECK(ink_test_index_marker_writes() == markers, "deletion adds zero marker writes");
    CHECK(sim_metric_value("file.index.background_runs") == runs, "deletion adds zero background runs");
}
static unsigned page;
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    lv_obj_set_style_bg_color(screen, page++ % 2 ? lv_color_black() : lv_color_white(), 0);
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "合成翻页测试");
    lv_obj_center(label);
}
static const ink_activity_t ACTIVITY = {.name = "SYNTHETIC redraw", .create = create};
static void open_page(void) { ink_ui_push(&ACTIVITY, NULL); }
static void redraw(void) { ink_ui_redraw(); }
static void active(void) {
    on_ui(redraw);
    CHECK(WAIT_FOR(ink_display_busy(), 2000), "file operation overlaps a real display refresh");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(WAIT_FOR(completed(), 30000), "completed index and UI settle");
    const char *mode = getenv("INK_DURABLE_MODE");
    if (mode && (!strcmp(mode, "overwrite") || !strcmp(mode, "mutations"))) {
        if (!strcmp(mode, "overwrite")) overwrite();
        else mutations();
        ink_probe_count("test.finished"); vtk_stop(); return;
    }
    if (mode) {
        unsigned id = ink_test_index_path_id("/content/SYNTHETIC_OLD.txt");
        CHECK(id != 0, "legacy path retains a stable id");
        ink_probe_value("test.durable.old_id", id);
        if (!strcmp(mode, "upgrade")) {
            ink_fs_entry_t e;
            unsigned expected = (unsigned)strtoul(getenv("INK_DURABLE_OLD_ID"), NULL, 10);
            CHECK(id == expected && !ink_fs_lookup(id, &e), "v1 migration retains the original id");
            ink_db_t *db = ink_db_open(ink_db_find_def("file.index"), NULL);
            ink_rec_t rec;
            CHECK(db && !ink_db_get(db, id, &rec, NULL, 0) && rec.schema == 3 && rec.len == 32 + strlen("/content/SYNTHETIC_OLD.txt") + 1,
                "v1 record becomes v3 with metadata and an exact-length path");
            ink_db_close(db);
            CHECK(hal_fs_size("/content/.file-index-dirty") < 0, "completed migration has no dirty marker");
        }
        ink_probe_count("test.finished"); vtk_stop(); return;
    }
    on_ui(open_page);
    for (int i = 0; i < 5; i++) {
        char path[96], renamed[96];
        snprintf(path, sizeof path, "/family/SYNTHETIC_NEW_%d.txt", i);
        snprintf(renamed, sizeof renamed, "/family/SYNTHETIC_RENAMED_%d.txt", i);
        CHECK(WAIT_FOR(settled(), 12000), "previous frame settles");
        active();
        uint64_t start = osal_now_us();
        CHECK(!ink_fs_write_atomic(path, "XXXXX", 5), "create while display refreshes");
        ink_probe_value("test.durable.create_ms", (osal_now_us() - start) / 1000.0);
        CHECK(WAIT_FOR(ready() && !ink_storage_pending() && settled(), 30000), "new file gets indexed before rename");
        char physical[96];
        snprintf(physical, sizeof physical, "/content/SYNTHETIC_NEW_%d.txt", i);
        unsigned id = ink_test_index_path_id(physical);
        CHECK(id != 0, "renamed file has an existing stable id");
        active();
        start = osal_now_us();
        CHECK(!ink_fs_move(path, renamed), "rename while display refreshes");
        ink_probe_value("test.durable.rename_ms", (osal_now_us() - start) / 1000.0);
        CHECK(WAIT_FOR(settled(), 12000), "renamed frame settles");
        active();
        start = osal_now_us();
        CHECK(!ink_fs_remove(renamed), "delete while display refreshes");
        ink_probe_value("test.durable.delete_ms", (osal_now_us() - start) / 1000.0);
        ink_fs_entry_t e;
        CHECK(ink_fs_lookup(id, &e) == INK_FS_ERR_NOT_FOUND, "deleted id no longer resolves");
        CHECK(WAIT_FOR(ready() && !ink_storage_pending() && settled(), 30000), "next sample starts from a completed index");
    }
    ink_probe_count("test.finished");
    vtk_stop();
}
void ink_main(void) {
    product_main();
    CHECK(!ink_cap_subscribe("file.changed", indexed, NULL, INK_EXEC_DIRECT), "observe indexed notifications");
    osal_task_cfg_t cfg = {.name = "durable-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start durability latency test");
}
