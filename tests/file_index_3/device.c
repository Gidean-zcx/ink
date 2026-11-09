#include "../services_finish/product.h"
#include "ink/ui_shell.h"
#include <sys/stat.h>

unsigned ink_test_index_count(void);
unsigned ink_test_index_writes(void);
unsigned ink_test_index_comparisons(void);
unsigned ink_test_index_reads(void);
unsigned ink_test_index_path_id(const char *path);
void ink_test_index_report(void);
static bool organizing, exchanged;
static unsigned notifications;
static char page[32768];
static bool ready(void) {
    char state[32];
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static void indexed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id; char kind[32];
    if (ink_args_get_str(a, "kind", kind, sizeof kind) > 0 && !strcmp(kind, "indexed")) notifications++;
}
static int64_t number(const char *scope, const char *key) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t n = -1;
    if (j && !ink_json_parse(j, g_result, strlen(g_result)))
        ink_json_int(j, ink_json_find(j, ink_json_find(j, 0, scope), key), &n);
    osal_free(j); return n;
}
static void read_page(void) { CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "storage AI snapshot"); }
static void open_page(void) { ink_settings_page_open("SetStorage"); read_page(); organizing = strstr(page, "正在整理") != NULL; }
static bool visible(void) { on_ui(read_page); return !strstr(page, "正在整理") && strstr(page, "我 17 B"); }
static void exchange(void *arg) {
    char content[600], saved[600], other[600];
    snprintf(content, sizeof content, "%s/content", sim_sd_root());
    snprintf(saved, sizeof saved, "%s/card-a", sim_sd_root());
    snprintf(other, sizeof other, "%s/card-b", sim_sd_root());
    exchanged = arg ? rename(content, other) == 0 && rename(saved, content) == 0 :
        rename(content, saved) == 0 && mkdir(content, 0700) == 0;
}
static void run(void *arg) {
    (void)arg;
    const char *mode = getenv("INK_INDEX_3_STAGE");
    if (!strcmp(mode, "legacy")) {
        uint32_t cur = 0; ink_fs_entry_t e;
        CHECK(ink_fs_index("/family", INK_FS_ANY, &cur, &e) == 1, "same 1000-file first-page workload");
        CHECK(ink_test_index_count() >= 1000, "1000 files are actually indexed");
    } else if (!strcmp(mode, "compare")) {
        CHECK(WAIT_FOR(ready(), 30000) && ink_test_index_count() == 1002, "same 1000-file card completes automatically");
    } else if (!strcmp(mode, "reboot")) {
        CHECK(WAIT_FOR(ready(), 10000), "completed card becomes ready after actual process restart");
        unsigned reads = ink_test_index_reads(); delay_ms(2100);
        CHECK(ink_test_index_reads() == reads && ink_test_index_writes() == 0,
            "matching card and schema cause no directory scan or index write");
    } else {
        CHECK(WAIT_FOR(ink_test_index_count() >= 32 && ink_test_index_count() < 1000, 10000), "background starts before any app or file.index query");
        if (!strcmp(mode, "ui")) { on_ui(open_page); CHECK(organizing, "storage page initially shows 正在整理"); }
        unsigned before = ink_test_index_count();
        uint64_t start = osal_now_us();
        CHECK(ink_fs_write_atomic("/family/SYNTH_DURING.txt", "XYZ", 3) == 0, "user write during indexing");
        ink_probe_value("test.index.user_write_us", (double)(osal_now_us() - start));
        CHECK(ink_test_index_count() < 1000, "user write finishes before card index");
        if (!strcmp(mode, "remove") || !strcmp(mode, "swap")) {
            unsigned id = 1; ink_fs_entry_t prefix;
            CHECK(ink_fs_lookup(id, &prefix) == 0, "retain an id from the completed prefix");
            sim_sd_set_present(false); delay_ms(1500);
            CHECK(!ready(), "removal stops indexing and clears ready");
            unsigned stopped = ink_test_index_count(), writes = ink_test_index_writes();
            delay_ms(1500);
            CHECK(stopped >= before && ink_test_index_count() == stopped && ink_test_index_writes() == writes,
                "no index progress or writes while absent");
            if (!strcmp(mode, "swap")) {
                vtk_host_call(exchange, NULL); CHECK(exchanged, "exchange synthetic cards");
                sim_sd_set_present(true);
                CHECK(WAIT_FOR(ready(), 10000), "replacement empty card completes separately");
                ink_fs_entry_t e;
                CHECK(ink_fs_lookup(id, &e) == INK_FS_ERR_CARD_CHANGED, "replacement card refuses old ids");
                sim_sd_set_present(false); delay_ms(1200);
                vtk_host_call(exchange, (void *)1); CHECK(exchanged, "restore original synthetic card");
            }
            sim_sd_set_present(true);
            CHECK(WAIT_FOR(ready(), 30000), "insertion resumes automatically without an application query");
            ink_fs_entry_t resumed;
            CHECK(ink_fs_lookup(id, &resumed) == 0 && !strcmp(prefix.path, resumed.path), "resume preserves already assigned ids");
        } else CHECK(WAIT_FOR(ready(), 30000), "whole card finishes with no application query");
        CHECK(ink_test_index_count() == 1003, "all shared and member files including concurrent write indexed exactly once");
        CHECK(ink_test_index_writes() <= 100, "1000-file indexing writes stay below one tenth of baseline");
        CHECK(ink_test_index_comparisons() < 40000, "dedup walks database once per batch");
        CHECK(notifications > 0, "completion emits file.changed indexed");
        if (!strcmp(mode, "ui")) CHECK(WAIT_FOR(visible(), 10000), "storage page updates through production subscription");
    }
    if (strcmp(mode, "compare") && strcmp(mode, "legacy")) {
        CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "organizing"), "file.usage becomes numerical automatically");
        CHECK(number("member", "books") == 17 && number("device", "books") == 1031, "usage includes shared and every member with unchanged scope");
    }
    ink_probe_value("test.index.writes", ink_test_index_writes());
    ink_probe_value("test.index.first_id", ink_test_index_path_id("/content/SYNTH_0000.txt"));
    ink_test_index_report(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(!ink_cap_subscribe("file.changed", indexed, NULL, INK_EXEC_DIRECT), "observe completion");
    osal_task_cfg_t cfg = {.name = "index-3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "index regression task");
}
