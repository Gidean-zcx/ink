#include "product.h"

int ink_test_file_gate_at, ink_test_file_gate_kind;
bool ink_test_file_held, ink_test_file_release;
static const char *operation;
static bool worker_done, worker_ok;
static int wrong, local_events;
static const char *private_name = "SYNTH_OWNER_PRIVATE";
static void observed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    char path[INK_FS_PATH_MAX] = "", title[64] = "", member[16] = "";
    ink_args_get_str(a, "path", path, sizeof path);
    ink_args_get_str(a, "title", title, sizeof title);
    ink_args_get_str(a, "member", member, sizeof member);
    if ((strstr(path, private_name) || strstr(title, private_name)) && !strcmp(member, "kid")) wrong++;
    if (!strcmp(id, "file.changed") && strstr(path, private_name) && !strcmp(member, "owner")) local_events++;
}
static bool seen(int first, const char *name, const char *member) {
    for (int i = first; i < sim_hub_events_seen(); i++) {
        const char *event = sim_hub_event_at(i);
        if (event && strstr(event, name) && strstr(event, member)) return true;
    }
    return false;
}
static void work(void *arg) {
    (void)arg;
    if (!strcmp(operation, "atomic")) worker_ok = ink_fs_write_atomic("/me/SYNTH_OWNER_PRIVATE.txt", "SYNTH", 5) == 0;
    else if (!strcmp(operation, "mkdir")) worker_ok = ink_fs_mkdir("/me/SYNTH_OWNER_PRIVATE") == 0;
    else if (!strcmp(operation, "remove")) worker_ok = ink_fs_remove("/me/SYNTH_OWNER_PRIVATE.txt") == 0;
    else if (!strcmp(operation, "move")) worker_ok = BY_HAND("file.move", "{\"from\":\"/me/SYNTH_OWNER_PRIVATE.txt\",\"to\":\"/me/SYNTH_OWNER_PRIVATE_MOVED.txt\"}");
    else if (!strcmp(operation, "copy")) worker_ok = BY_HAND("file.copy", "{\"from\":\"/me/SYNTH_OWNER_PRIVATE.txt\",\"to\":\"/me/SYNTH_OWNER_PRIVATE_COPY.txt\"}");
    else {
        worker_ok = BY_HAND("file.delete", "{\"path\":\"/me/SYNTH_OWNER_PRIVATE.txt\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION;
        unsigned token = 0;
        worker_ok = worker_ok && sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0;
    }
    worker_done = true;
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    operation = getenv("INK_REVIEW_OPERATION");
    bool change = getenv("INK_REVIEW_CHANGE") != NULL;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 10000), "hub online");
    if (!strcmp(operation, "remove") || !strcmp(operation, "move") || !strcmp(operation, "copy") || !strcmp(operation, "delete"))
        CHECK(ink_fs_write_atomic("/me/SYNTH_OWNER_PRIVATE.txt", "SYNTH", 5) == 0, "seed source");
    delay_ms(300);
    CHECK(ink_cap_subscribe("file.", observed, NULL, INK_EXEC_DIRECT) == 0 &&
          ink_cap_subscribe("job.finished", observed, NULL, INK_EXEC_DIRECT) == 0, "local observers");
    ink_fs_file_t *file = NULL;
    if (!strcmp(operation, "record")) {
        CHECK(BY_HAND("audio.record", "{\"path\":\"/me/SYNTH_OWNER_PRIVATE.opus\"}") && g_rc.status == INK_RC_DONE, "real recording starts");
        delay_ms(400);
    } else if (!strcmp(operation, "handle")) {
        CHECK(ink_fs_open("/me/SYNTH_OWNER_PRIVATE.txt", INK_FS_CREATE | INK_FS_WRITE, &file) == 0, "open original file");
        CHECK(file && ink_fs_write(file, "SYNTH", 5) == 5, "write original file");
    } else {
        ink_test_file_gate_at = !strcmp(operation, "copy") ? 2 : 1;
        osal_task_cfg_t cfg = {.name = "file-event-writer", .fn = work, .stack_bytes = 16384,
            .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        CHECK(osal_task_create(&cfg, NULL) == OSAL_OK && WAIT_FOR(ink_test_file_held, 2000), "pause after real source resolution");
    }
    int first = sim_hub_events_seen();
    if (change) CHECK(ink_member_switch("kid") == 0, "handoff");
    if (!strcmp(operation, "record")) {
        if (!change) CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "stop recording");
        CHECK(WAIT_FOR(!ink_audio_recording(), 3000), "recording finishes");
    } else if (file) CHECK(ink_fs_close(file) == 0, "original handle closes");
    else {
        ink_test_file_release = true;
        CHECK(WAIT_FOR(worker_done, 5000) && worker_ok, "real operation returns");
        if (!strcmp(operation, "delete")) CHECK(WAIT_FOR(local_events || wrong, 5000), "confirmed deletion finishes");
    }
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 10000), "current connection ready");
    CHECK(ink_fs_write_atomic("/me/SYNTH_DELIVERY_CONTROL.txt", "X", 1) == 0, "current event control");
    CHECK(WAIT_FOR(seen(first, "SYNTH_DELIVERY_CONTROL", change ? "\"member\":\"kid\"" : "\"member\":\"owner\""), 3000), "current event delivered");
    CHECK(AI("job.list", NULL) && (!change || !strstr(g_result, private_name)), "private task title stays with original member");
    if (change) {
        CHECK(wrong == 0, "private path/title never relabelled for child: %d", wrong);
        CHECK(!seen(first, private_name, "\"member\":\"kid\""), "private file event never reaches child AI");
        CHECK(local_events > 0, "local event keeps original owner");
    } else CHECK(WAIT_FOR(seen(first, private_name, "\"member\":\"owner\""), 1000) && local_events > 0, "owner sees own change");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "file-event-handoff", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
