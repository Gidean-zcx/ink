#include "product.h"

int ink_test_file_gate_at, ink_test_file_gate_kind;
bool ink_test_file_held, ink_test_file_release;
static ink_fs_file_t *file;
static bool closed;
static int private_events, results;
static char status[24];
static void observed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx;
    if (!strcmp(id, "confirm.result")) {
        ink_args_get_str(args, "status", status, sizeof status);
        results++;
    } else {
        char path[128] = "";
        ink_args_get_str(args, "path", path, sizeof path);
        if (strstr(path, "SYNTH_OWNER_PRIVATE")) private_events++;
    }
}
static bool confirmed(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    int before = results;
    return sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 &&
        WAIT_FOR(results > before, 5000) && !strcmp(status, "done");
}
static bool seen(int first, const char *name) {
    for (int i = first; i < sim_hub_events_seen(); i++) {
        const char *event = sim_hub_event_at(i);
        if (event && strstr(event, name)) return true;
    }
    return false;
}
static void close_file(void *arg) {
    (void)arg;
    CHECK(ink_fs_close(file) == 0, "old handle closes");
    closed = true;
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result", observed, NULL, INK_EXEC_DIRECT) == 0 &&
          ink_cap_subscribe("file.changed", observed, NULL, INK_EXEC_DIRECT) == 0, "observers");
    CHECK(confirmed("member.add", "{\"id\":\"former\",\"role\":\"adult\"}"), "add original adult");
    CHECK(ink_member_switch("former") == 0, "original adult enters");
    CHECK(ink_fs_write_atomic("/family/SYNTH_SHARED.txt", "SYNTH_SHARED", 12) == 0, "shared directory entry");
    ink_fs_dir_t *root = NULL, *family = NULL;
    CHECK(ink_fs_dir_open("/", &root) == 0 && ink_fs_dir_open("/family", &family) == 0, "original directory handles");
    CHECK(ink_fs_open("/me/SYNTH_OWNER_PRIVATE.txt", INK_FS_WRITE | INK_FS_CREATE, &file) == 0 &&
          ink_fs_write(file, "SYNTH", 5) == 5, "original writable handle");
    ink_test_file_gate_kind = 2;
    ink_test_file_gate_at = 1;
    osal_task_cfg_t cfg = {.name = "file-close-reuse", .fn = close_file, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK && WAIT_FOR(ink_test_file_held, 1000), "pause close after releasing slot before event");
    CHECK(ink_member_switch("owner") == 0, "parent enters for removal");
    CHECK(confirmed("member.remove", "{\"id\":\"former\"}"), "real cleanup succeeds after slot release");
    CHECK(confirmed("member.add", "{\"id\":\"former\",\"role\":\"child\"}"), "child reuses cleaned id");
    CHECK(ink_member_switch("former") == 0 && !ink_member_exists("former"), "new incarnation enters");
    ink_fs_stat_t entry;
    CHECK(ink_fs_dir_next(root, &entry) == INK_FS_ERR_DENIED, "old virtual directory cannot inherit reused identity");
    CHECK(ink_fs_dir_next(family, &entry) == INK_FS_ERR_DENIED, "old shared directory cannot inherit reused identity");
    ink_fs_dir_close(root);
    ink_fs_dir_close(family);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 10000), "new incarnation online");
    int first = sim_hub_events_seen();
    ink_test_file_release = true;
    CHECK(WAIT_FOR(closed, 2000), "delayed close event returns");
    CHECK(ink_task_begin_for("file.copy", "SYNTH_STALE_TASK", "former", NULL, NULL) == 0, "stale task registration refused");
    CHECK(ink_fs_write_atomic("/me/SYNTH_DELIVERY_CONTROL.txt", "X", 1) == 0 &&
          WAIT_FOR(seen(first, "SYNTH_DELIVERY_CONTROL"), 3000), "current event still delivered");
    CHECK(private_events == 0, "late old-generation path is absent from local subscriptions");
    CHECK(!seen(first, "SYNTH_OWNER_PRIVATE"), "late old-generation path is absent from new AI connection");
    CHECK(AI("file.read", "{\"path\":\"/me/SYNTH_OWNER_PRIVATE.txt\"}") && g_rc.status != INK_RC_DONE, "cleaned file remains absent");
    CHECK(AI("job.list", NULL) && !strstr(g_result, "SYNTH_STALE_TASK"), "new incarnation sees no old task");
    root = NULL;
    CHECK(ink_fs_dir_open("/", &root) == 0 && ink_fs_dir_next(root, &entry) == 1 && !strcmp(entry.name, "me") &&
          ink_fs_dir_next(root, &entry) == 1 && !strcmp(entry.name, "family"), "current child directory works");
    CHECK(ink_member_switch("owner") == 0, "adult takes over child directory handle");
    CHECK(ink_fs_dir_next(root, &entry) == 0, "child directory never gains adult-only roots");
    ink_fs_dir_close(root);
    CHECK(ink_member_switch("former") == 0, "restore current child");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "file-generation-reuse", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
