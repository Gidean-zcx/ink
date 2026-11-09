/* Real USB ownership and directory edits on synthetic card contents. */
#include "../services_finish/product.h"
#include "ink/hal_usb.h"

unsigned ink_test_index_path_id(const char *path);
bool ink_test_index_idle(void);
void ink_test_audit7_reset(void);
uint64_t ink_test_audit7_hold(void);
unsigned ink_test_audit7_size_queries(void);
static char mode[24], host_file[1024];
static bool ready(void) {
    char state[32] = "";
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static bool completed(void) {
    return ready() && ink_test_index_idle() && !ink_storage_pending() && ink_db_idle() &&
        !ink_ui_busy() && !ink_display_busy() && hal_fs_size("/content/.file-index-dirty") < 0;
}
static void read_env(void *arg) {
    (void)arg;
    snprintf(mode, sizeof mode, "%s", getenv("INK_AUDIT7_MODE"));
    const char *path = getenv("INK_AUDIT7_HOST_FILE");
    snprintf(host_file, sizeof host_file, "%s", path ? path : "");
}
static void computer_copy(void *arg) {
    (void)arg;
    char path[1200]; snprintf(path, sizeof path, "%s/content/.file-index-dirty", sim_sd_root());
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "B7-03: dirty marker exists before the computer edits the lent card");
    if (f) CHECK(fclose(f) == 0, "close the computer's marker inspection");
    sim_usb_copy_in(host_file, "content/SYNTHETIC_NEW.txt");
}
static void cut(void *arg) { (void)arg; ink_probe_count("test.power_cut"); sim_storage_power_cut(); }
static void usb_cut(void) {
    CHECK(ink_test_index_path_id("/content/SYNTHETIC_OLD.txt") != 0, "old file is durably indexed");
    hw(HW_USB_ON, 0, 0);
    CHECK(hal_usb_host_connected() && ink_usb_disk_start() == INK_FS_OK && ink_usb_disk_active(), "lend card through the real USB interface");
    vtk_event_after(SIM_US(1), computer_copy, NULL); delay_ms(5);
    CHECK(ink_usb_disk_stop() == INK_FS_OK && !ink_usb_disk_active(), "computer returns the card");
    CHECK(hal_fs_size("/content/.file-index-dirty") == 0, "B7-03: durable dirty marker survives USB return");
    ink_probe_count("test.finished");
    vtk_event_after(SIM_US(1), cut, NULL);
    delay_ms(1000); CHECK(false, "power cut stops before background indexing");
}
static void usb_reboot(void) {
    CHECK(hal_fs_size("/content/SYNTHETIC_NEW.txt") == 5, "computer's new file survives reboot");
    CHECK(ink_test_index_path_id("/content/SYNTHETIC_NEW.txt") != 0, "B7-03: reboot assigns the new file an id");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"books\":8"),
        "B7-03: ready index reports all eight bytes: %.150s", g_result);
    CHECK(hal_fs_size("/content/.file-index-dirty") < 0, "completed rebuild clears the marker");
}
static void directory(void) {
    unsigned ids[300]; ink_fs_entry_t entry;
    for (unsigned i = 0; i < 300; i++) {
        char path[96]; snprintf(path, sizeof path, "/content/SYNTHETIC_DIR/FILE_%03u.txt", i);
        ids[i] = ink_test_index_path_id(path);
        CHECK(ids[i] != 0, "directory child is already indexed");
    }
    ink_test_audit7_reset(); uint64_t start = osal_now_us();
    CHECK(ink_fs_move("/family/SYNTHETIC_DIR", "/family/SYNTHETIC_RENAMED") == INK_FS_OK, "rename the real 300-file directory");
    uint64_t elapsed = osal_now_us() - start, hold = ink_test_audit7_hold();
    unsigned queries = ink_test_audit7_size_queries();
    ink_probe_value("test.audit7.rename_us", (double)elapsed);
    ink_probe_value("test.audit7.hold_us", (double)hold);
    ink_probe_value("test.audit7.size_queries", queries);
    CHECK(hold < 300000, "B7-04: directory rename holds edits under 300 ms, got %llu us", (unsigned long long)hold);
    CHECK(queries < 10, "B7-04: directory rename adds no per-file size queries, got %u", queries);
    for (unsigned i = 0; i < 300; i++) {
        char path[96]; snprintf(path, sizeof path, "/family/SYNTHETIC_RENAMED/FILE_%03u.txt", i);
        CHECK(!ink_fs_lookup(ids[i], &entry) && !strcmp(entry.path, path) && entry.size == 1,
            "renamed child preserves its id, path and size");
    }
    CHECK(WAIT_FOR(completed(), 60000), "renamed directory index becomes durable");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"books\":300"),
        "directory rename preserves usage: %.150s", g_result);
}
static void run(void *arg) {
    (void)arg; vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    CHECK(WAIT_FOR(completed(), 120000), "initial index and completion record become durable");
    if (!strcmp(mode, "usb-cut")) { usb_cut(); return; }
    if (!strcmp(mode, "usb-reboot")) usb_reboot();
    else directory();
    ink_probe_count("test.finished"); vtk_stop();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit7-file", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start audit7 file regressions");
}
