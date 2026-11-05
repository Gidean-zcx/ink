#include "product.h"

int ink_test_file_gate_at, ink_test_file_gate_kind;
bool ink_test_file_held, ink_test_file_release;
static bool worker_done, accepted;
static ink_rc_t status;
static char arguments[400];
static const char *operation;
static void work(void *arg) {
    (void)arg;
    accepted = BY_HAND(operation, arguments);
    status = g_rc.status;
    worker_done = true;
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    const char *root = getenv("INK_REVIEW_ROOT"), *stage = getenv("INK_REVIEW_STAGE");
    bool change = getenv("INK_REVIEW_CHANGE") != NULL;
    bool child = getenv("INK_REVIEW_CHILD") != NULL;
    bool tree = getenv("INK_REVIEW_TREE") != NULL;
    operation = getenv("INK_REVIEW_OPERATION");
    bool copying = !strcmp(operation, "file.copy");
    const char *physical = !strcmp(root, "/app/demo") ? "/content/.m/owner/apps/demo" :
        !strcmp(root, "/family") ? "/content" : "/content/.m/owner";
    if (child) {
        CHECK(ink_member_switch("kid") == 0, "child starts operation");
        physical = "/content/.m/kid";
    }
    char from[INK_FS_PATH_MAX], to[INK_FS_PATH_MAX], seed[INK_FS_PATH_MAX];
    snprintf(from, sizeof from, "%s/SYNTH_SOURCE", root);
    snprintf(to, sizeof to, "%s/SYNTH_TARGET", child ? "/family" : root);
    snprintf(seed, sizeof seed, "%s%s", from, tree ? "/leaf.txt" : "");
    CHECK(ink_fs_write_atomic(seed, "SYNTH_ADULT_CONTENT", 19) == 0, "seed original member source");
    snprintf(arguments, sizeof arguments, "{\"from\":\"%s\",\"to\":\"%s\"}", from, to);
    ink_test_file_gate_kind = !strcmp(stage, "setup") ? 1 : 0;
    ink_test_file_gate_at = copying && strcmp(stage, "stat") ? 2 : 1;
    if (!strncmp(root, "/app/", 5)) {
        /* App paths are internal; exercise the same resolver without exposing them to AI. */
        operation = copying ? "internal.copy" : "internal.move";
        ink_test_file_gate_at = 1;
    }
    osal_task_cfg_t cfg = {.name = "file-path-worker", .fn = work, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK && WAIT_FOR(ink_test_file_held, 2000), "hold real resolved path before next stage");
    if (change) CHECK(ink_member_switch(child ? "owner" : "kid") == 0, "switch in actual operation");
    ink_test_file_release = true;
    CHECK(WAIT_FOR(worker_done, 5000) && accepted, "operation returns receipt");
    char original[220], target[220];
    snprintf(original, sizeof original, "%s/SYNTH_SOURCE%s", physical, tree ? "/leaf.txt" : "");
    snprintf(target, sizeof target, "%s/SYNTH_TARGET%s", child ? "/content" : physical, tree ? "/leaf.txt" : "");
    if (tree) CHECK(WAIT_FOR(hal_fs_size(target) == 19, 5000), "real tree copy finishes");
    if (child) {
        CHECK(hal_fs_size(target) < 0 && hal_fs_size(original) == 19, "child cannot inherit adult write authority");
        CHECK(!change ? status == INK_RC_REJECTED : true, "child control is denied");
    } else {
        CHECK(hal_fs_size(target) == 19, "destination remains in accepted namespace");
        CHECK(hal_fs_size(original) == (copying ? 19 : -1), "original has correct move/copy result");
        if (change && strcmp(root, "/family")) {
            CHECK(hal_fs_size("/content/.m/kid/SYNTH_TARGET") < 0 &&
                  hal_fs_size("/content/.m/kid/apps/demo/SYNTH_TARGET") < 0, "no file or directory enters child namespace");
            CHECK(AI("file.read", "{\"path\":\"/me/SYNTH_TARGET\"}") &&
                  (g_rc.status != INK_RC_DONE || !strstr(g_result, "SYNTH_ADULT_CONTENT")), "real child read cannot find adult content");
            CHECK(AI("job.list", NULL) && !strstr(g_result, "SYNTH_SOURCE"), "original job stays out of child's list");
        }
        if (!change) CHECK(status == INK_RC_DONE || status == INK_RC_QUEUED, "ordinary operation succeeds");
    }
    ink_probe_count("test.finished");
}
static void internal(ink_call_t *call) {
    char from[INK_FS_PATH_MAX], to[INK_FS_PATH_MAX];
    ink_args_get_str(&call->args, "from", from, sizeof from);
    ink_args_get_str(&call->args, "to", to, sizeof to);
    int r = !strcmp(call->cap->id, "internal.copy") ? ink_fs_copy_start(from, to) : ink_fs_move(from, to);
    if (r < 0) call->rc.status = INK_RC_FAILED;
    if (r > 0) {
        ink_fs_job_status_t job;
        CHECK(WAIT_FOR(ink_fs_job_status(r, &job) == 0 && job.state != INK_FS_JOB_RUNNING, 5000), "internal job completes");
    }
}
static const ink_cap_t INTERNAL[] = {
    {.id = "internal.copy", .effect = INK_FX_LOCAL_WRITE, .exec = INK_EXEC_DIRECT, .run = internal},
    {.id = "internal.move", .effect = INK_FX_LOCAL_WRITE, .exec = INK_EXEC_DIRECT, .run = internal},
};
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(INTERNAL, 2) == 0, "test-only internal caller");
    osal_task_cfg_t cfg = {.name = "file-path-handoff", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
