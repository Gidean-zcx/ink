#include "../services_finish/product.h"
#include <unistd.h>

void ink_test_audit11_arm(void);
unsigned ink_test_audit11_faults(void);
bool ink_test_audit11_idle(void);
bool ink_test_audit11_between_batches(void);
unsigned ink_test_audit11_count(void);
unsigned ink_test_audit11_id(const char *path);
bool ink_test_audit11_done(void);

static bool host_ok;
static bool ready(void) {
    char state[32];
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static bool completed(void) {
    return ready() && ink_test_audit11_idle() && ink_db_idle() && !ink_storage_pending() &&
        hal_fs_size("/content/.file-index-dirty") < 0;
}
static void path_for(unsigned i, char *path, size_t cap) {
    snprintf(path, cap, "/content/%sSYNTH_%04u.txt", i < 30 ? "" : i < 50 ? ".m/owner/" : ".m/kid/", i);
}
/* Host filesystem work runs outside the simulated task. */
static void offline_delete(void *arg) {
    (void)arg;
    static const unsigned deleted[] = {0,1,2,3,4,5,6,7,8,9,35,36,37,38,39,55,56,57,58,59};
    char path[200], host[768]; host_ok = true;
    for (unsigned j = 0; j < 20; j++) {
        path_for(deleted[j], path, sizeof path);
        snprintf(host, sizeof host, "%s%s", sim_sd_root(), path);
        if (unlink(host)) host_ok = false;
    }
}
static void verify(unsigned id) {
    CHECK(ink_test_audit11_count() == 40, "index count equals 40 actual files: %u", ink_test_audit11_count());
    CHECK(hal_fs_size("/content/SYNTH_0010.txt") == 11 && ink_test_audit11_id("/content/SYNTH_0010.txt") == id,
        "existing file retains original id %u", id);
    ink_fs_entry_t entry;
    CHECK(!ink_fs_lookup(id, &entry) && entry.id == id && !strcmp(entry.path, "/family/SYNTH_0010.txt"),
        "retained id resolves through the public file service");
    ink_probe_value("test.audit11.count", ink_test_audit11_count());
    ink_probe_value("test.audit11.id", ink_test_audit11_id("/content/SYNTH_0010.txt"));
}
static void run(void *arg) {
    (void)arg;
    bool stat_error = !strcmp(getenv("INK_AUDIT11_CASE"), "b11-01");
    const char *previous = getenv("INK_AUDIT11_ID");
    CHECK(WAIT_FOR(completed(), 90000), "initial index becomes fully durable");
    if (previous) verify((unsigned)strtoul(previous, NULL, 10));
    else {
        CHECK(ink_test_audit11_count() == 60, "seed card has 60 indexed files");
        unsigned id = ink_test_audit11_id("/content/SYNTH_0010.txt");
        CHECK(id != 0, "capture original stable id");
        ink_probe_value("test.audit11.original", id);
        sim_sd_set_present(false); delay_ms(1200);
        vtk_host_call(offline_delete, NULL);
        CHECK(host_ok, "computer deletes 20 synthetic files");
        if (stat_error) ink_test_audit11_arm();
        sim_sd_set_present(true);
        if (stat_error) {
            CHECK(WAIT_FOR(ink_test_audit11_faults() == 1 && ink_test_audit11_idle() && ink_db_idle() && !ink_storage_pending(), 30000),
                "one status error stops the cleanup pass");
            CHECK(!ready() && !ink_test_audit11_done(), "status error leaves completion unset");
            CHECK(hal_fs_size("/content/.file-index-dirty") >= 0, "status error keeps the dirty marker");
            CHECK(hal_fs_size("/content/SYNTH_0010.txt") == 11 && ink_test_audit11_id("/content/SYNTH_0010.txt") == id,
                "status error keeps the existing file id");
            /* The next pass: the card is taken out and put back. */
            sim_sd_set_present(false); delay_ms(1200);
            sim_sd_set_present(true);
            CHECK(WAIT_FOR(completed(), 90000), "the next pass finishes the cleanup");
            verify(id);
        } else {
            CHECK(WAIT_FOR(ink_test_audit11_between_batches(), 30000), "query lands between cleanup batches");
            CHECK(AI("file.index", "{\"path\":\"/family/SYNTH_EMPTY\",\"limit\":1}") && g_rc.status == INK_RC_DONE &&
                strstr(g_result, "\"entries\":[]"), "normal capability query returns the empty directory: %.150s", g_result);
            CHECK(!ready(), "local scan cannot mark the unfinished card ready");
            CHECK(WAIT_FOR(completed(), 90000), "cleanup finishes after the empty-directory query");
            verify(id);
        }
    }
    ink_probe_count("test.finished"); vtk_stop();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit11-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start audit11 index regression");
}
