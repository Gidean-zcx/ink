#include "../services_finish/product.h"
#include <sys/stat.h>
#include <unistd.h>

unsigned ink_test_index_count(void);
unsigned ink_test_prune_largest(void);
unsigned ink_test_index_path_id(const char *path);
bool ink_test_index_idle(void);
void ink_test_index_report(void);
void ink_test_prune_measure(void);
void ink_test_prune_phase(const char *name);
bool ink_test_prune_record(unsigned id);
bool ink_test_prune_partial(void);
bool ink_test_prune_thumb(unsigned id, bool seed);

static const unsigned deleted[] = {0,1,2,3,4,5,6,7,8,9,35,36,37,38,39,55,56,57,58,59};
static const unsigned renamed[] = {20,21,30,31,50};
static unsigned total, old_ids[25], largest;
static bool full, host_ok;
static const char *mode;

static bool ready(void) {
    char state[32];
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static bool completed(void) {
    return ready() && ink_test_index_idle() && ink_db_idle() && !ink_storage_pending() &&
        hal_fs_size("/content/.file-index-dirty") < 0;
}
static void path_for(unsigned i, char *path, size_t cap) {
    unsigned family = full ? 1000 : 30, owner = full ? 1016 : 50;
    const char *root = i < family ? "" : i < owner ? ".m/owner/" : ".m/kid/";
    snprintf(path, cap, "/content/%sSYNTH_%04u.txt", root, i);
}
static void renamed_path(unsigned j, char *path, size_t cap) {
    static const char *const roots[] = {".m/owner/", ".m/kid/", "", ".m/kid/", ".m/owner/"};
    snprintf(path, cap, "/content/%sSYNTH_RENAMED_%u.txt", roots[j], j);
}
static void probe_id(const char *kind, unsigned i, unsigned id) {
    char key[64];
    snprintf(key, sizeof key, "test.prune.%s_%u", kind, i);
    ink_probe_value(key, id);
}
/* All host filesystem work runs outside the simulated task. */
static void offline_edit(void *arg) {
    (void)arg;
    host_ok = true;
    char path[200], other[200], host[768], target[768];
    for (unsigned j = 0; j < 20; j++) {
        path_for(full ? j : deleted[j], path, sizeof path);
        snprintf(host, sizeof host, "%s%s", sim_sd_root(), path);
        if (unlink(host)) host_ok = false;
    }
    for (unsigned j = 0; !full && j < 5; j++) {
        path_for(renamed[j], path, sizeof path);
        renamed_path(j, other, sizeof other);
        snprintf(host, sizeof host, "%s%s", sim_sd_root(), path);
        snprintf(target, sizeof target, "%s%s", sim_sd_root(), other);
        if (rename(host, target)) host_ok = false;
    }
    for (unsigned j = 0; full && j < 12; j++) {
        snprintf(host, sizeof host, "%s/content/SYNTH_NEW_%u.txt", sim_sd_root(), j);
        FILE *f = fopen(host, "wb");
        if (!f) { host_ok = false; continue; }
        if (fwrite("SYNTHETIC_NEW_CONTENT", 1, 21, f) != 21) host_ok = false;
        if (fclose(f)) host_ok = false;
    }
}
static void exchange(void *arg) {
    char content[768], saved[768], other[768];
    snprintf(content, sizeof content, "%s/content", sim_sd_root());
    snprintf(saved, sizeof saved, "%s/card-a", sim_sd_root());
    snprintf(other, sizeof other, "%s/card-b", sim_sd_root());
    host_ok = arg ? rename(content, other) == 0 && rename(saved, content) == 0 :
        rename(content, saved) == 0 && mkdir(content, 0700) == 0;
}
static void verify(void) {
    unsigned expected = full ? 1016 : 40;
    CHECK(ink_test_index_count() == expected, "pruned index equals actual card files: %u, expected %u", ink_test_index_count(), expected);
    for (unsigned j = 0; j < 20; j++) {
        ink_fs_entry_t entry;
        CHECK(!ink_test_prune_record(old_ids[j]) && ink_fs_lookup(old_ids[j], &entry) == INK_FS_ERR_NOT_FOUND,
            "deleted id %u is absent from the database and lookup", old_ids[j]);
    }
    CHECK(!ink_test_prune_thumb(old_ids[0], false), "deleted id loses its cached thumbnail");
    char path[200];
    for (unsigned j = 0; !full && j < 5; j++) {
        renamed_path(j, path, sizeof path);
        unsigned id = ink_test_index_path_id(path);
        CHECK(id == old_ids[20 + j], "cross-root rename %u retains original id", j);
        probe_id("renamed", j, id);
    }
    for (unsigned j = 0; full && j < 12; j++) {
        snprintf(path, sizeof path, "/content/SYNTH_NEW_%u.txt", j);
        unsigned id = ink_test_index_path_id(path);
        ink_fs_entry_t entry;
        CHECK(id > largest && !ink_fs_lookup(id, &entry), "new file gets a fresh id after a full index: %u > %u", id, largest);
        probe_id("new", j, id);
    }
    ink_probe_value("test.prune.count", ink_test_index_count());
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(completed(), 90000), "initial index becomes fully durable");
    ink_test_prune_measure();
    char path[200];
    if (getenv("INK_PRUNE_OLD_IDS")) {
        const char *p = getenv("INK_PRUNE_OLD_IDS");
        for (unsigned j = 0; j < 25; j++) {
            old_ids[j] = (unsigned)strtoul(p, NULL, 10);
            const char *next = strchr(p, ',');
            p = next ? next + 1 : p + strlen(p);
        }
        largest = (unsigned)strtoul(getenv("INK_PRUNE_LARGEST"), NULL, 10);
        verify();
    } else {
        CHECK(ink_test_index_count() == total, "seed card fills the expected index capacity");
        largest = ink_test_prune_largest();
        for (unsigned j = 0; j < 25; j++) {
            path_for(j < 20 ? (full ? j : deleted[j]) : renamed[j - 20], path, sizeof path);
            old_ids[j] = ink_test_index_path_id(path);
            CHECK(old_ids[j] != 0, "retain original id %u", j);
            probe_id("old", j, old_ids[j]);
        }
        ink_probe_value("test.prune.largest", largest);
        CHECK(ink_test_prune_thumb(old_ids[0], true), "seed a real decoded thumbnail for a stale id");
        ink_test_prune_phase("test.prune.after_thumb");
        sim_sd_set_present(false); delay_ms(1200);
        vtk_host_call(offline_edit, NULL);
        CHECK(host_ok, "computer deletes 20 files and renames 5 or adds 12");
        ink_test_prune_phase("test.prune.before_return");
        sim_sd_set_present(true);
        if (!strcmp(mode, "remove") || !strcmp(mode, "swap")) {
            CHECK(WAIT_FOR(ink_test_prune_partial(), 30000), "family is scanned but member roots are still incomplete");
            sim_sd_set_present(false); delay_ms(1500);
            CHECK(ink_test_index_count() == total, "interrupted scan deletes no records");
            for (unsigned j = 0; j < 20; j++) CHECK(ink_test_prune_record(old_ids[j]), "interrupted scan keeps stale id %u", old_ids[j]);
            CHECK(ink_test_prune_thumb(old_ids[0], false), "interrupted scan keeps cached thumbnail");
            if (!strcmp(mode, "swap")) {
                vtk_host_call(exchange, NULL); CHECK(host_ok, "insert another synthetic card");
                sim_sd_set_present(true);
                CHECK(WAIT_FOR(completed(), 30000) && ink_test_index_count() == total, "empty replacement card preserves every other-card record");
                sim_sd_set_present(false); delay_ms(1200);
                vtk_host_call(exchange, (void *)1); CHECK(host_ok, "return original synthetic card");
            }
            sim_sd_set_present(true);
        }
        CHECK(WAIT_FOR(completed(), 90000), "complete card rescan and pruning become durable");
        ink_test_prune_phase("test.prune.after_scan");
        verify();
    }
    ink_test_index_report(); ink_probe_count("test.finished"); vtk_stop();
}
void ink_main(void) {
    product_main();
    mode = getenv("INK_PRUNE_MODE");
    full = !strcmp(mode, "full"); total = full ? 1024 : 60;
    osal_task_cfg_t cfg = {.name = "prune-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start index pruning regression");
}
