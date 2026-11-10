/* Restart actual flash images containing explicit schema 1 and 2 fixtures. */
#include "../services_finish/product.h"

unsigned ink_test_index_path_id(const char *path);
unsigned ink_test_index_reads(void);
unsigned ink_test_index_writes(void);
bool ink_test_index_idle(void);
bool ink_test_compact_seed(unsigned schema);
bool ink_test_compact_records(unsigned schema, unsigned rewritten);
bool ink_test_compact_completion(unsigned schema);
bool ink_test_compact_rewrite(unsigned id);
bool ink_test_compact_long(bool create);
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
static bool completed(void) {
    return ready() && ink_test_index_idle() && ink_db_idle() && !ink_storage_pending() &&
        hal_fs_size("/content/.file-index-dirty") < 0;
}
static void run(void *arg) {
    (void)arg;
    const char *stage = getenv("INK_COMPACT_STAGE");
    unsigned schema = (unsigned)(getenv("INK_COMPACT_SCHEMA")[0] - '0');
    CHECK(WAIT_FOR(completed(), 30000), "background index becomes durable without an app query");
    unsigned reads = ink_test_index_reads(), writes = ink_test_index_writes();
    unsigned ids[3] = {0};
    static const char *const paths[] = {"/content/SYNTH_OLD.txt", "/content/中文合成.txt", "/content/.m/owner/SYNTH_PRIVATE.txt"};
    for (unsigned i = 0; i < 3; i++) {
        unsigned id = ink_test_index_path_id(paths[i]);
        ids[i] = id;
        char key[48]; snprintf(key, sizeof key, "test.compact.id_%u", i);
        ink_probe_value(key, id);
        ink_fs_entry_t entry;
        CHECK(id && !ink_fs_lookup(id, &entry) && entry.size == (int64_t)(i + 1), "file id and content metadata resolve");
    }
    bool seed = !strncmp(stage, "seed", 4);
    CHECK(ink_test_compact_long(seed), "155-byte UTF-8 path persists as 188 bytes and decodes through get and next");
    if (seed) {
        CHECK(ink_test_compact_completion(2), "new completion keeps the existing version 2");
        CHECK(ink_test_compact_seed(schema) && ink_test_compact_records(schema, 0), "persist explicit legacy index and completion records");
        for (unsigned i = 0; i < 3; i++) {
            ink_fs_entry_t entry;
            CHECK(!ink_fs_lookup(ids[i], &entry) && entry.size == (int64_t)(i + 1), "legacy record resolves before background migration");
        }
    } else {
        CHECK(writes == 0 && reads == 0 && notifications == 0,
            "completed legacy or mixed restart does not scan, write or notify");
        bool upgraded = !strcmp(stage, "reboot");
        bool mixed = !strcmp(stage, "mixed") || !strcmp(stage, "upgrade");
        CHECK(ink_test_compact_records(upgraded ? 3 : schema, mixed ? ids[0] : 0),
            "unchanged legacy and mixed records retain their format");
        if (!strcmp(stage, "rewrite")) {
            CHECK(ink_test_compact_rewrite(ids[0]) && ink_test_compact_records(schema, ids[0]),
                "writing one legacy record upgrades only that record and keeps its id");
        } else if (!strcmp(stage, "upgrade")) {
            static const char *const roots[] = {"/family", "/me"};
            for (unsigned i = 0; i < 2; i++) {
                uint32_t cursor = 0; ink_fs_entry_t entry; int n;
                while ((n = ink_fs_index(roots[i], INK_FS_ANY, &cursor, &entry)) == 1) {}
                CHECK(n == 0, "existing query scan completes");
            }
            CHECK(WAIT_FOR(completed(), 30000) && ink_test_compact_records(3, 0),
                "every upgraded record has exact path length and original metadata");
        }
    }
    CHECK(ink_test_compact_completion(schema), "legacy completion stays valid and keeps its original version");
    ink_probe_value("test.compact.index_writes", writes);
    ink_probe_value("test.compact.dir_reads", reads);
    ink_probe_value("test.compact.notifications", notifications);
    ink_probe_count("test.finished"); vtk_stop();
}
void ink_main(void) {
    product_main();
    CHECK(!ink_cap_subscribe("file.changed", indexed, NULL, INK_EXEC_DIRECT), "observe index notifications");
    osal_task_cfg_t cfg = {.name = "compact-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start compact index regression");
}
