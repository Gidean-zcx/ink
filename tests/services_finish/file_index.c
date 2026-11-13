#include "product.h"
#include <sys/stat.h>

static uint32_t proof[3];
unsigned ink_test_file_index_count(void);
static const char *const names[] = {"one.PNG", "nested/two.jpg", "song.OPUS", "note.txt", "other.bin", "second.md"};

static uint32_t find(const char *root, const char *name, ink_fs_type_t type) {
    uint32_t cursor = 0; ink_fs_entry_t e; int r;
    while ((r = ink_fs_index(root, type, &cursor, &e)) == 1)
        if (!strcmp(e.path, name)) return e.id;
    CHECK(r == 0, "index search ends (%d)", r);
    return 0;
}

static bool state_is(const char *expected) {
    char state[32] = "";
    return ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state) && !strcmp(state, expected);
}

static bool confirmed(const char *id, const char *args) {
    if (!BY_HAND(id, args)) return false;
    if (g_rc.status == INK_RC_DONE) return true;
    unsigned token = 0;
    return sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0;
}

static void unrelated_long_path(void) {
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "/me/%058d/%054d.txt", 0, 0);
    CHECK(strlen(path) == 121 && ink_fs_write_atomic(path, "SYNTH_PRIVATE", 13) == 0, "create valid long private path");
    CHECK(find("/me", path, INK_FS_TEXT) != 0, "index long private path");
    CHECK(ink_fs_write_atomic("/family/review/public.txt", "SYNTH_PUBLIC", 12) == 0, "create shared subtree fixture");
    uint32_t shared = find("/family/review", "/family/review/public.txt", INK_FS_TEXT);
    CHECK(shared != 0, "owner can query shared subtree");
    CHECK(ink_member_switch("kid") == 0, "enter existing child space");
    uint32_t cursor = 0; ink_fs_entry_t e; int r;
    r = ink_fs_index("/family/review", INK_FS_TEXT, &cursor, &e);
    CHECK(r == 1 && e.id == shared && !strcmp(e.path, "/family/review/public.txt"), "child can query unrelated shared subtree (r=%d)", r);
    CHECK(ink_fs_index("/family/review", INK_FS_TEXT, &cursor, &e) == 0, "child shared pagination ends");
    CHECK(ink_fs_write_atomic("/me/review/local.txt", "SYNTH_CHILD", 11) == 0, "create child subtree fixture");
    cursor = 0;
    r = ink_fs_index("/me/review", INK_FS_TEXT, &cursor, &e);
    CHECK(r == 1 && !strcmp(e.path, "/me/review/local.txt"), "child can query own subtree despite unrelated long path (r=%d)", r);
    CHECK(ink_fs_index("/me/review", INK_FS_TEXT, &cursor, &e) == 0, "child private pagination ends");
    CHECK(ink_member_switch("owner") == 0, "return to owner after long-path regression");
}

static void create(void) {
    char path[INK_FS_PATH_MAX];
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        snprintf(path, sizeof path, "/me/library/%s", names[i]);
        CHECK(ink_fs_write_atomic(path, "SYNTHETIC", 9) == 0, "create %s", path);
    }
    uint32_t first = find("/me/library", "/me/library/one.PNG", INK_FS_IMAGE);
    proof[1] = find("/me/library", "/me/library/song.OPUS", INK_FS_AUDIO);
    proof[2] = find("/me/library", "/me/library/note.txt", INK_FS_TEXT);
    CHECK(first && proof[1] && proof[2], "images, audio and text have ids");
    CHECK(find("/me/library", "/me/library/other.bin", INK_FS_OTHER) != 0, "other type");
    uint32_t cursor = 0; ink_fs_entry_t e;
    CHECK(ink_fs_write_atomic("/family/SYNTH_SHARED.txt", "SHARED", 6) == 0, "family fixture");
    int listed;
    while ((listed = ink_fs_index("/family", INK_FS_ANY, &cursor, &e)) == 1)
        CHECK(!strncmp(e.path, "/family/", 8), "family index excludes private namespaces: %s", e.path);
    CHECK(listed == 0, "family listing ends");
    cursor = 0;
    CHECK(ink_fs_index("/cache/synthetic", INK_FS_ANY, &cursor, &e) == INK_FS_ERR_DENIED, "cache never enters public index");
    CHECK(ink_fs_index("/me/library", INK_FS_IMAGE, &cursor, &e) == 1, "first image page");
    uint32_t previous = cursor;
    CHECK(ink_fs_index("/me/library", INK_FS_IMAGE, &cursor, &e) == 1 && cursor != previous, "next image page");
    CHECK(ink_fs_index("/me/library", INK_FS_IMAGE, &cursor, &e) == 0, "image pagination ends");
    CHECK(AI("file.index", "{\"path\":\"/me/library\",\"type\":\"image\",\"limit\":1}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"next\":"), "AI first page: %s", g_result);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t next = 0;
    CHECK(j && ink_json_parse(j, g_result, strlen(g_result)) == 0 && ink_json_int(j, ink_json_find(j, 0, "next"), &next), "numeric continuation");
    osal_free(j);
    char args[200]; snprintf(args, sizeof args, "{\"path\":\"/me/library\",\"type\":\"image\",\"limit\":1,\"cursor\":%lld}", (long long)next);
    CHECK(AI("file.index", args) && g_rc.status == INK_RC_DONE && !strstr(g_result, "\"next\":"), "AI last page: %s", g_result);
    CHECK(ink_fs_move("/me/library/one.PNG", "/me/library/renamed.PNG") == 0 && ink_fs_lookup(first, &e) == 0 && !strcmp(e.path, "/me/library/renamed.PNG"), "rename keeps id");
    CHECK(ink_fs_move("/me/library", "/me/moved") == 0 && ink_fs_lookup(first, &e) == 0 && !strcmp(e.path, "/me/moved/renamed.PNG"), "directory move keeps descendant id");
    CHECK(ink_fs_remove("/me/moved/renamed.PNG") == 0 && ink_fs_lookup(first, &e) == INK_FS_ERR_NOT_FOUND, "deleted id is absent");
    CHECK(ink_fs_write_atomic("/me/moved/renamed.PNG", "NEW_SYNTHETIC", 13) == 0, "recreate same name");
    proof[0] = find("/me/moved", "/me/moved/renamed.PNG", INK_FS_IMAGE);
    CHECK(proof[0] && proof[0] != first && ink_fs_lookup(first, &e) == INK_FS_ERR_NOT_FOUND, "deleted id never aliases recreated file");
    const char *const deleted[] = {"/me/moved/nested/two.jpg", "/me/moved/second.md"};
    for (size_t i = 0; i < sizeof deleted / sizeof deleted[0]; i++) {
        uint32_t old = find("/me/moved", deleted[i], INK_FS_ANY);
        CHECK(old != 0, "save id before background deletion: %s", deleted[i]);
        int job = ink_fs_delete_start(i ? deleted[i] : "/me/moved/nested"); ink_fs_job_status_t st;
        CHECK(job > 0 && WAIT_FOR(ink_fs_job_status(job, &st) == 0 && st.state != INK_FS_JOB_RUNNING, 5000) && st.state == INK_FS_JOB_DONE, "indexed background deletion completes: %s", deleted[i]);
        CHECK(ink_fs_lookup(old, &e) == INK_FS_ERR_NOT_FOUND, "background deletion invalidates id: %s", deleted[i]);
        CHECK(ink_fs_write_atomic(deleted[i], "NEW_SYNTHETIC", 13) == 0, "recreate background-deleted path: %s", deleted[i]);
        uint32_t replacement = find("/me/moved", deleted[i], INK_FS_ANY);
        CHECK(replacement && replacement != old && ink_fs_lookup(old, &e) == INK_FS_ERR_NOT_FOUND, "background-deleted id never aliases recreated file: %s", deleted[i]);
    }
    CHECK(ink_fs_write_atomic("/me/index-proof.bin", proof, sizeof proof) == 0, "save synthetic ids for real reboot");
    unrelated_long_path();
}

static void read_proof(void) {
    ink_fs_file_t *f = NULL;
    CHECK(ink_fs_open("/me/index-proof.bin", INK_FS_READ, &f) == 0, "open persisted proof");
    CHECK(f && ink_fs_read(f, proof, sizeof proof) == sizeof proof, "read persisted ids");
    if (f) CHECK(ink_fs_close(f) == 0, "close proof");
}

static bool exchanged;
static void exchange(void *arg) {
    bool back = arg != NULL;
    char content[600], saved[600], other[600];
    snprintf(content, sizeof content, "%s/content", sim_sd_root());
    snprintf(saved, sizeof saved, "%s/card-a", sim_sd_root());
    snprintf(other, sizeof other, "%s/card-b", sim_sd_root());
    exchanged = back ? rename(content, other) == 0 && rename(saved, content) == 0 : rename(content, saved) == 0 && mkdir(content, 0700) == 0;
}

static void reboot_and_swap(bool swap) {
    read_proof(); ink_fs_entry_t e;
    if (!swap) {
        CHECK(ink_fs_move("/me/moved/note.txt", "/me/moved/reboot.txt") == 0, "move before loading index after reboot");
        CHECK(ink_fs_lookup(proof[0], &e) == 0 && e.type == INK_FS_IMAGE, "image id persists across reboot");
        CHECK(ink_fs_lookup(proof[2], &e) == 0 && !strcmp(e.path, "/me/moved/reboot.txt"), "pre-query move retained persisted id");
        char args[80]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)proof[0]);
        CHECK(BY_HAND("file.lookup", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "renamed.PNG"), "button capability lookup");
        CHECK(confirmed("member.add", "{\"id\":\"synthetic-child\",\"role\":\"child\"}"), "add synthetic child");
        CHECK(WAIT_FOR(ink_member_exists("synthetic-child"), 5000) && ink_member_switch("synthetic-child") == 0, "enter child space");
        CHECK(ink_fs_lookup(proof[0], &e) == INK_FS_ERR_DENIED, "child cannot resolve parent's id");
        CHECK(AI("file.lookup", args) && g_rc.status == INK_RC_REJECTED && !strstr(g_result, "renamed.PNG"), "AI lookup enforces member isolation");
        CHECK(ink_member_switch("owner") == 0, "return to owner");
        return;
    }
    CHECK(ink_fs_lookup(proof[0], &e) == 0, "original card ready");
    sim_sd_set_present(false);
    CHECK(ink_fs_lookup(proof[0], &e) == INK_FS_ERR_NO_CARD && state_is("unavailable"), "removed card is explicitly unavailable");
    vtk_host_call(exchange, NULL);
    CHECK(exchanged, "exchange two synthetic card directories");
    sim_sd_set_present(true);
    CHECK(ink_fs_write_atomic("/me/moved/renamed.PNG", "CARD_B_SYNTHETIC", 16) == 0, "same path on second card");
    CHECK(ink_fs_lookup(proof[0], &e) == INK_FS_ERR_CARD_CHANGED && state_is("changed"), "old id refuses second card and reports changed");
    uint32_t second = find("/me/moved", "/me/moved/renamed.PNG", INK_FS_IMAGE);
    CHECK(second && second != proof[0] && ink_fs_lookup(proof[0], &e) == INK_FS_ERR_CARD_CHANGED, "second card has distinct ids");
    vtk_host_call(exchange, (void *)1);
    CHECK(exchanged && ink_fs_lookup(proof[0], &e) == 0 && ink_fs_lookup(second, &e) == INK_FS_ERR_CARD_CHANGED, "returning original card restores only its ids");
}

static void capacity(void) {
    char path[80];
    /* Count what the first background pass indexed (the card's books), not whatever it reached so far. */
    CHECK(WAIT_FOR(state_is("ready"), 10000), "first index pass finished before counting");
    unsigned existing = ink_test_file_index_count();
    for (unsigned i = 0; i <= INK_FS_INDEX_MAX - existing; i++) {
        snprintf(path, sizeof path, "/me/capacity/SYNTH_%04d.txt", i);
        CHECK(ink_fs_write_atomic(path, "X", 1) == 0, "capacity fixture %d", i);
    }
    uint32_t cursor = 0; ink_fs_entry_t e;
    int full = ink_fs_index("/me/capacity", INK_FS_ANY, &cursor, &e);
    CHECK(full == INK_FS_ERR_INDEX_FULL, "one extra file honestly exceeds the device index limit (%d)", full);
    CHECK(ink_fs_lookup(1, &e) == 0, "full index keeps previously assigned ids");
    uint32_t old = e.id;
    int removed = ink_fs_remove(e.path);
    CHECK(removed == 0, "free one index slot (%d)", removed);
    cursor = 0;
    CHECK(ink_fs_index("/me/capacity", INK_FS_ANY, &cursor, &e) == 1 && ink_fs_lookup(old, &e) == INK_FS_ERR_NOT_FOUND, "full index recovers without reusing deleted id");
}

static void run(void *arg) {
    (void)arg; delay_ms(1800);
    const char *stage = getenv("INK_INDEX_STAGE");
    if (stage && !strcmp(stage, "reboot")) reboot_and_swap(false);
    else if (stage && !strcmp(stage, "swap")) reboot_and_swap(true);
    else if (stage && !strcmp(stage, "capacity")) capacity();
    else create();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "file-index-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
