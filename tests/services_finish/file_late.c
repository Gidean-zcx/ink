#include "product.h"

extern bool ink_test_file_cleanup_entered, ink_test_file_cleanup_returned;
static bool held, release_writer, armed, writer_done;
static ink_fs_file_t *file;
static int write_result, results;
static char status[24];

void ink_test_file_before_mkdir(const char *path) {
    if (!armed || strcmp(path, "/content/.m/oldadult")) return;
    armed = false;
    held = true;
    while (!release_writer) osal_task_delay_us(10000);
}

static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", status, sizeof status);
    results++;
}

static bool start(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    return sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0;
}

static bool confirmed(const char *cap, const char *args) {
    int before = results;
    return start(cap, args) && WAIT_FOR(results > before, 5000) && !strcmp(status, "done");
}

static void write_late(void *arg) {
    (void)arg;
    if (getenv("INK_REVIEW_FILE_OPEN")) {
        write_result = ink_fs_open("/me/synthetic.txt", INK_FS_WRITE | INK_FS_CREATE, &file);
        if (!write_result) write_result = ink_fs_write(file, "SYNTH_OLD", 9) == 9 ? 0 : -1;
    } else write_result = ink_fs_write_atomic("/me/synthetic.txt", "SYNTH_OLD", 9);
    writer_done = true;
}

static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    bool control = getenv("INK_REVIEW_CONTROL") != NULL;
    bool reuse = getenv("INK_REVIEW_NO_REUSE") == NULL;
    bool open = getenv("INK_REVIEW_FILE_OPEN") != NULL;
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add adult");
    CHECK(ink_member_switch("oldadult") == 0, "enter adult");
    delay_ms(300);
    armed = true;
    osal_task_cfg_t cfg = {.name = "file-writer", .fn = write_late, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK && WAIT_FOR(held, 1000), "hold after physical path resolution");
    CHECK(ink_member_switch("owner") == 0, "handoff during file operation");
    int before = results;
    if (!control) {
        CHECK(start("member.remove", "{\"id\":\"oldadult\"}") && WAIT_FOR(ink_test_file_cleanup_entered, 2000), "removal reaches card cleanup");
        /* Give the unfixed cleanup a bounded turn to finish while the writer stays gated. */
        (void)WAIT_FOR(ink_test_file_cleanup_returned, 500);
    }
    release_writer = true;
    CHECK(WAIT_FOR(writer_done, 2000) && write_result == 0, "accepted file operation completes: %d", write_result);
    if (!control) {
        CHECK(WAIT_FOR(results > before, 5000) && !strcmp(status, open ? "failed" : "done"), "cleanup result with %s: %s", open ? "open handle" : "short write", status);
        if (file) { ink_fs_close(file); file = NULL; }
        if (!strcmp(status, "failed")) CHECK(confirmed("member.remove", "{\"id\":\"oldadult\"}"), "retry after closing original member handle");
        if (reuse) CHECK(confirmed("member.add", "{\"id\":\"oldadult\",\"role\":\"child\"}"), "reuse cleaned id");
    }
    if (file) ink_fs_close(file);
    CHECK(control ? hal_fs_size("/content/.m/oldadult/synthetic.txt") == 9 : hal_fs_size("/content/.m/oldadult/synthetic.txt") < 0,
          "old file cannot outlive cleanup");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "file-lifecycle", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
