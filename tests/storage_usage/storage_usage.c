#include "../services_finish/product.h"
#include "ink/ui_shell.h"
#include "image_fixtures.h"

unsigned ink_test_storage_reads(void);
unsigned ink_test_storage_sizes(void);
unsigned ink_test_storage_locked_sizes(void);
unsigned ink_test_storage_holds(void);
void ink_test_storage_begin(void);
void ink_test_storage_end(void);
void ink_test_storage_edit_on_size(void (*edit)(void));
bool ink_test_storage_has_thumbs(void);
static const char *const kinds[] = {"books", "audio", "photos", "other"};
static const char *const paths[] = {"/me/SYNTH_BOOK.epub", "/me/SYNTH_AUDIO.mp3", "/me/SYNTH_PHOTO.jpg", "/me/SYNTH_OTHER.bin"};
static const int sizes[2][4] = {{17, 23, 31, 7}, {19, 29, 37, 11}};
static int64_t base[4];
static bool initial_organizing, captured;
static char action_args[128], action_cap[64];
static int label_x, label_y;
static unsigned indexed_events;
static void index_event(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; char kind[16];
    if (ink_args_get_str(args, "kind", kind, sizeof kind) > 0 && !strcmp(kind, "indexed")) indexed_events++;
}

static int64_t number(const char *scope, const char *key) {
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t v = -1;
    if (j && !ink_json_parse(j, g_result, strlen(g_result))) {
        int root = scope ? ink_json_find(j, 0, scope) : 0;
        ink_json_int(j, ink_json_find(j, root, key), &v);
    }
    osal_free(j); return v;
}
static void write_file(const char *path, int n) {
    char data[64]; memset(data, 'X', sizeof data);
    CHECK(n <= (int)sizeof data && ink_fs_write_atomic(path, data, (size_t)n) == 0, "synthetic file %s", path);
}
static void raw_file(const char *path, int n) {
    char data[64]; memset(data, 'X', sizeof data);
    hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f && hal_fs_write(f, data, (size_t)n) == n, "synthetic reserved backup fixture");
    if (f) CHECK(!hal_fs_close(f), "close reserved fixture");
}
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void read_page(void) { CHECK(ink_ui_snapshot(g_snap, sizeof g_snap, NULL) > 0, "complete production AI snapshot"); }
static void open_storage(void) {
    ink_settings_page_open("SetStorage"); read_page();
    initial_organizing = strstr(g_snap, "正在整理") != NULL;
}
static bool usage_visible(void) {
    on_ui(read_page);
    return strstr(g_snap, "我 17 B") && !strstr(g_snap, "正在整理");
}
static void index_device(void) {
    for (int i = -1; i < INK_MEMBERS_MAX && (i < 0 || ink_member_at(i)); i++) {
        char root[64]; uint32_t cursor = 0; ink_fs_entry_t entry; int r;
        if (i < 0) snprintf(root, sizeof root, "/family");
        else snprintf(root, sizeof root, "/members/%s", ink_member_at(i)->id);
        while ((r = ink_fs_index(root, INK_FS_ANY, &cursor, &entry)) == 1) {}
        CHECK(r == 0, "existing index workflow completes %s (%d)", root, r);
    }
}
static void edit_during_usage(void) { write_file(paths[0], 18); }
static bool index_ready(void) {
    char state[32];
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, "ready");
}
static void test_usage_cache(void) {
    CHECK(BY_HAND("system.home", NULL), "leave storage before counting usage work");
    write_file(paths[0], sizes[0][0]);
    CHECK(WAIT_FOR(index_ready(), 10000), "automatic index repair finishes before measuring usage cache");
    for (int i = 0; i < 2; i++) {
        unsigned reads = ink_test_storage_reads();
        ink_test_storage_begin();
        CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && number("member", "books") == 17,
            "usage after an indexed file write is exact");
        CHECK(ink_test_storage_reads() == reads, "usage never scans the card after a write");
        CHECK(i ? ink_test_storage_sizes() == 0 : ink_test_storage_sizes() > 0,
            "write invalidates usage once; second query reads no indexed file sizes");
        CHECK(ink_test_storage_locked_sizes() == 0 && ink_test_storage_holds() == 2,
            "size reads hold no file edit lock; only two bounded edit sections");
        ink_test_storage_end();
    }
    write_file(paths[0], sizes[0][0]);
    CHECK(WAIT_FOR(index_ready(), 10000), "automatic repair finishes before concurrent usage edit");
    ink_test_storage_begin(); ink_test_storage_edit_on_size(edit_during_usage);
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"organizing\""),
        "write during size lookup discards the expired revision");
    ink_test_storage_edit_on_size(NULL); ink_test_storage_end();
    CHECK(WAIT_FOR(index_ready(), 10000), "concurrent edit triggers automatic index repair");
    for (int i = 0; i < 2; i++) {
        ink_test_storage_begin();
        CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && number("member", "books") == 18,
            "next query recomputes the changed file exactly");
        CHECK(i ? ink_test_storage_sizes() == 0 : ink_test_storage_sizes() > 0,
            "expired statistics are recomputed once and then cached");
        ink_test_storage_end();
    }
    write_file(paths[0], sizes[0][0]);
}
static void test_units(void) {
    static const char *const labels[] = {"1.0 KB", "1.0 MB", "1.0 GB"};
    int64_t n = 1;
    for (int i = 0; i < 3; i++) {
        n *= 1024;
        ink_fs_file_t *f = NULL;
        CHECK(ink_fs_open(paths[0], INK_FS_WRITE, &f) == 0, "open synthetic sparse unit fixture");
        CHECK(f && ink_fs_seek(f, n - 1, 0) == 0 && ink_fs_write(f, "X", 1) == 1, "write sparse unit fixture");
        if (f) CHECK(ink_fs_close(f) == 0, "close sparse unit fixture");
        ink_state_set_int(ink_state_find("storage.free"), n);
        CHECK(AI("shell.storage", NULL) && WAIT_FOR(settled(), 10000), "show unit on real storage page");
        on_ui(read_page);
        CHECK(strstr(g_snap, labels[i]) && !strstr(g_snap, "iB"), "production snapshot uses %s", labels[i]);
        char free_label[64]; snprintf(free_label, sizeof free_label, "还能放 %s", labels[i]);
        CHECK(strstr(g_snap, free_label), "remaining space also uses %s", labels[i]);
        CHECK(BY_HAND("system.home", NULL), "leave storage between unit fixtures");
    }
    write_file(paths[0], sizes[0][0]);
}
static void test_card_marker(void) {
    uint8_t card[16]; hal_file_t *f = hal_fs_open("/content/.file-index-card", HAL_O_READ);
    CHECK(f && hal_fs_read(f, card, sizeof card) == sizeof card, "read synthetic card identity");
    if (!f) return;
    CHECK(hal_fs_close(f) == 0, "close synthetic card identity");
    for (int i = 0; i < 3; i++) {
        f = hal_fs_open("/content/.file-index-card", HAL_O_WRITE | HAL_O_TRUNC);
        size_t n = i == 0 ? 15 : sizeof card;
        CHECK(f && hal_fs_write(f, card, n) == (ssize_t)n, "synthetic card identity length fixture");
        if (f) {
            if (i == 1) CHECK(hal_fs_write(f, "X", 1) == 1, "oversized card identity");
            CHECK(hal_fs_close(f) == 0, "close card identity fixture");
        }
        CHECK(AI("file.usage", NULL) && g_rc.status == (i < 2 ? INK_RC_FAILED : INK_RC_DONE),
            "card identity must be exactly 16 bytes; intact identity recovers");
    }
}
static void capture_event(void *arg) {
    (void)arg; CHECK(settled(), "settled comparison frame");
    sim_display_snapshot("scene"); captured = true;
}
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), "清理临时文件")) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void inspect_action(void) {
    read_page();
    const char *p = strstr(g_snap, "\"actions\":");
    p = p ? strstr(p, "\"id\":\"storage.clean\"") : NULL;
    const char *end = p ? strstr(p, "}}") : NULL;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && p && end && !ink_json_parse(j, p - 1, (size_t)(end - p) + 3)) {
        ink_json_str(j, ink_json_find(j, 0, "capability"), action_cap, sizeof action_cap);
        ink_json_str(j, ink_json_find(j, 0, "args_json"), action_args, sizeof action_args);
    }
    osal_free(j); label_x = label_y = -1; find_label(lv_screen_active());
}
static void cleanup_args(char *out, size_t cap) {
    CHECK(AI("file.cleanup_preview", NULL) && g_rc.status == INK_RC_DONE, "AI reads cleanup preview");
    snprintf(out, cap, "{\"revision\":%lld}", (long long)number(NULL, "revision"));
}
static bool have_clean_action(void) {
    on_ui(inspect_action);
    return action_cap[0] && action_args[0];
}
static void test_cleanup(void) {
    CHECK(BY_HAND("system.home", NULL), "leave storage before fixtures");
    write_file("/me/orphan.txt.tmp", 5);
    write_file("/me/收件箱/unfinished.bin.part", 7);
    write_file("/cache/sys/thumbs/SYNTH.raw", 11);
    write_file("/me/SYNTH_USER.bin", 13);
    write_file("/me/folder.tmp/keep.bin", 19);
    CHECK(ink_fs_write_atomic("/me/SYNTH_CACHE.png", black_png, sizeof black_png) == 0, "real encoded thumbnail source");
    uint32_t cursor = 0, picture = 0; ink_fs_entry_t entry; int next;
    while ((next = ink_fs_index("/me", INK_FS_IMAGE, &cursor, &entry)) == 1)
        if (!strcmp(entry.path, "/me/SYNTH_CACHE.png")) picture = entry.id;
    CHECK(next == 0 && picture, "index real thumbnail source");
    char thumb_args[64]; snprintf(thumb_args, sizeof thumb_args, "{\"id\":%u}", picture);
    CHECK(AI("file.thumb", thumb_args) && g_rc.status == INK_RC_DONE && ink_test_storage_has_thumbs(), "populate actual PSRAM thumbnail cache through file.thumb");
    CHECK(hal_fs_mkdir("/content/.m/owner/.backup") == 0, "reserved backup directory");
    raw_file("/content/.m/owner/.backup/keep.part", 17);
    char args[128]; cleanup_args(args, sizeof args);
    CHECK(number(NULL, "temporary") == 5 && number(NULL, "cache") == 11 && number(NULL, "partial") == 7 && number(NULL, "bytes") == 23,
        "preview reports exactly three safe categories and 23 bytes");
    CHECK(AI("file.cleanup", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "irreversible AI cleanup requires confirmation");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1, "confirmation token");
    CHECK(hal_fs_size("/content/.m/owner/orphan.txt.tmp") == 5, "confirmation has no deletion side effect");
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "human can cancel cleanup");
    CHECK(AI("shell.storage", NULL) && WAIT_FOR(settled(), 10000), "open real cleanup row");
    for (int i = 0; i < 4 && !have_clean_action(); i++) CHECK(AI("shell.settings_next", NULL), "page to cleanup");
    CHECK(WAIT_FOR(have_clean_action(), 10000), "cleanup preview automatically arrives");
    CHECK(!strcmp(action_cap, "file.cleanup") && !strcmp(action_args, args), "button and AI use identical capability and revision");
    CHECK(strstr(g_snap, "能腾出 23 B") && strstr(g_snap, "临时文件、缓存和未传完"), "visible summary lists cleanup categories and bytes");
    CHECK(label_x >= 0 && label_y >= 0, "cleanup row has a touch target");
    tap(label_x, label_y);
    ink_confirm_view_t card;
    CHECK(WAIT_FOR(ink_confirm_list(&card, 1) == 1, 2000), "actual button opens confirmation");
    CHECK(!strcmp(card.title, "清理临时文件"), "confirmation title stays complete UTF-8");
    CHECK(!strcmp(card.cap, "file.cleanup") && ink_confirm_decide(card.token, true, INK_SRC_TOUCH) == 0, "human confirms button cleanup");
    CHECK(WAIT_FOR(hal_fs_size("/content/.m/owner/orphan.txt.tmp") < 0, 5000), "temporary file removed");
    CHECK(hal_fs_size("/content/.m/owner/收件箱/unfinished.bin.part") < 0 && hal_fs_size("/cache/sys/thumbs/SYNTH.raw") < 0,
        "partial transfer and thumbnail cache removed");
    CHECK(hal_fs_size("/content/.m/owner/SYNTH_USER.bin") == 13 && hal_fs_size("/content/.m/owner/folder.tmp/keep.bin") == 19 &&
        hal_fs_size("/content/.m/owner/.backup/keep.part") == 17, "ordinary user files, suffix directories and backups survive cleanup");
    CHECK(!ink_test_storage_has_thumbs() && hal_fs_size("/content/.m/owner/SYNTH_CACHE.png") == (int64_t)sizeof black_png,
        "actual thumbnail cache is released while its user image survives");
    cleanup_args(args, sizeof args);
    CHECK(number(NULL, "bytes") == 0, "removed bytes equal displayed reclaimable bytes");
    CHECK(BY_HAND("system.home", NULL), "leave storage for cache-only check");
    write_file("/cache/sys/thumbs/SYNTH.raw", 11); cleanup_args(args, sizeof args);
    write_file("/me/SYNTH_LATER.bin", 1);
    CHECK(AI("file.cache_clear", args) && g_rc.status == INK_RC_FAILED && hal_fs_size("/cache/sys/thumbs/SYNTH.raw") == 11,
        "changed files invalidate preview before any deletion");
    cleanup_args(args, sizeof args);
    CHECK(AI("file.cache_clear", args) && g_rc.status == INK_RC_DONE && number(NULL, "bytes") == 11,
        "regenerable cache needs no confirmation and reports released bytes");
}
static void child_open(void) {
    ink_settings_page_open("SetStorage");
    CHECK(strcmp(ink_ui_page_name(), "SetStorage"), "child sees no cleanup page through direct navigation");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); test_start_home();
    CHECK(WAIT_FOR(index_ready(), 10000), "background index finishes without a query or application warmup");
    unsigned reads = ink_test_storage_reads();
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "\"organizing\"") &&
        ink_test_storage_reads() == reads, "background-built index reports numbers without usage scanning the card");
    index_device();
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "initial device usage from existing index");
    for (int k = 0; k < 4; k++) base[k] = number("device", kinds[k]);
    on_ui(read_page); unsigned events = indexed_events;
    index_device(); on_ui(read_page);
    CHECK(indexed_events == events, "unchanged repeated indexing emits no file change and cannot trigger redraw loops");
    CHECK(BY_HAND("member.add", "{\"id\":\"guest\",\"role\":\"adult\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "add synthetic second adult");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "authorize synthetic member");
    CHECK(WAIT_FOR(ink_member_exists("guest"), 5000), "second adult exists");
    for (int k = 0; k < 4; k++) write_file(paths[k], sizes[0][k]);
    CHECK(ink_member_switch("guest") == 0, "switch to second adult");
    for (int k = 0; k < 4; k++) write_file(paths[k], sizes[1][k]);
    CHECK(ink_member_switch("owner") == 0, "return to owner");
    on_ui(open_storage);
    CHECK(initial_organizing, "unfinished index displays 正在整理 without invented zeroes");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE,
        "new member root exposes its current background indexing status");
    index_device();
    CHECK(WAIT_FOR(usage_visible(), 10000), "completed index automatically refreshes screen and AI snapshot");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "AI reads the same indexed usage");
    for (int k = 0; k < 4; k++) {
        CHECK(number("member", kinds[k]) == sizes[0][k], "owner category %s exact bytes", kinds[k]);
        CHECK(number("device", kinds[k]) == base[k] + sizes[0][k] + sizes[1][k], "device category %s exact bytes", kinds[k]);
    }
    reads = ink_test_storage_reads();
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE && ink_test_storage_reads() == reads,
        "unchanged usage query performs no directory traversal");
    CHECK(WAIT_FOR(settled(), 10000), "storage display settles");
    vtk_event_at(vtk_now() + SIM_MS(1), capture_event, NULL);
    CHECK(WAIT_FOR(captured, 2000), "capture storage comparison evidence");
    ink_probe_count("test.snapshot.SetStorage"); ink_probe_count("test.snapshot.settled");
    CHECK(ink_member_switch("guest") == 0, "switch member for second usage query");
    CHECK(AI("file.usage", NULL) && g_rc.status == INK_RC_DONE, "second member usage");
    for (int k = 0; k < 4; k++) {
        CHECK(number("member", kinds[k]) == sizes[1][k], "personal category changes with member %s", kinds[k]);
        CHECK(number("device", kinds[k]) == base[k] + sizes[0][k] + sizes[1][k], "device category unchanged %s", kinds[k]);
    }
    CHECK(ink_member_switch("owner") == 0, "return before cleanup");
    test_usage_cache(); test_card_marker(); test_units(); test_cleanup();
    CHECK(ink_member_switch("kid") == 0, "enter child space");
    CHECK(AI("shell.storage", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot open cleanup page via AI");
    CHECK(AI("file.cleanup_preview", NULL) && g_rc.status == INK_RC_REJECTED, "child cannot list device cleanup");
    CHECK(AI("file.cleanup", "{\"revision\":1}") && g_rc.status == INK_RC_REJECTED, "child cannot clean via AI");
    CHECK(BY_HAND("file.cache_clear", "{\"revision\":1}") && g_rc.status == INK_RC_REJECTED, "child cannot clean cache by hand");
    on_ui(child_open); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(!ink_cap_subscribe("file.changed", index_event, NULL, INK_EXEC_UI), "observe index completion notifications");
    osal_task_cfg_t cfg = {.name = "storage-usage-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
