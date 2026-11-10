#include "product.h"
#include "ink/sys/image.h"
#include "image_fixtures.h"
#include <fcntl.h>
#include <sys/stat.h>

uint64_t ink_test_thumb_version(uint32_t id);
size_t ink_test_thumb_bytes(void);

static bool stamp_ok;
static struct { const char *path; int64_t seconds; } stamp_request;
static void stamp_on_host(void *arg) {
    (void)arg;
    char path[768];
    snprintf(path, sizeof path, "%s/content/.m/owner%s", sim_sd_root(), stamp_request.path + 3);
    struct timespec times[2] = {{stamp_request.seconds, 0}, {stamp_request.seconds, 0}};
    stamp_ok = utimensat(AT_FDCWD, path, times, 0) == 0;
}
static void stamp(const char *path, int64_t seconds) {
    stamp_request.path = path; stamp_request.seconds = seconds;
    vtk_host_call(stamp_on_host, NULL);
    CHECK(stamp_ok, "set synthetic modification metadata");
}

static void picture(const char *path, bool white, size_t size) {
    uint8_t *data = osal_calloc(1, size, OSAL_MEM_EXTERNAL);
    CHECK(data != NULL, "synthetic encoded image buffer");
    if (!data) return;
    const uint8_t *source = white ? white_png : black_png;
    size_t bytes = white ? sizeof white_png : sizeof black_png;
    CHECK(size >= bytes, "padded PNG fits");
    memcpy(data, source, bytes);
    CHECK(ink_fs_write_atomic(path, data, size) == 0, "write synthetic PNG: %s", path);
    osal_free(data);
}

static uint32_t find(const char *path) {
    uint32_t cursor = 0; ink_fs_entry_t e; int r;
    while ((r = ink_fs_index("/me/SYNTH", INK_FS_IMAGE, &cursor, &e)) == 1)
        if (!strcmp(e.path, path)) return e.id;
    CHECK(r == 0, "index search ends");
    return 0;
}

static volatile bool thumb_done;
static ink_rc_t thumb_status;
static bool pixels_ok;
static char expected_gray;
static void thumb_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    thumb_status = rc->status;
    pixels_ok = false;
    const char *pixels = rc->result ? strstr(rc->result, "\"gray4\":\"") : NULL;
    if (pixels) {
        pixels += strlen("\"gray4\":\"");
        const char *end = strchr(pixels, '"');
        pixels_ok = end && (size_t)(end - pixels) == INK_THUMB_BYTES &&
            strstr(rc->result, "\"width\":128,") && strstr(rc->result, "\"height\":128,");
        if (pixels_ok) for (const char *p = pixels; p < end; p++) if (*p != expected_gray) { pixels_ok = false; break; }
    }
    if (rc->status != INK_RC_DONE) CHECK(!rc->result, "failed thumbnail has no pixel payload");
    __atomic_store_n(&thumb_done, true, __ATOMIC_RELEASE);
}
static void thumb(uint32_t id, ink_source_t src, ink_rc_t status, char gray) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", id);
    ink_call_opts_t opts = {.src = src, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = thumb_reply};
    expected_gray = gray; thumb_done = false;
    CHECK(ink_cap_call("file.thumb", NULL, &opts) == 0, "thumbnail accepted on the bus");
    CHECK(WAIT_FOR(__atomic_load_n(&thumb_done, __ATOMIC_ACQUIRE), 5000), "thumbnail replied");
    CHECK(thumb_status == status, "thumbnail status %s, expected %s", ink_rc_name(thumb_status), ink_rc_name(status));
    if (status == INK_RC_DONE) CHECK(pixels_ok, "full thumbnail dimensions and all gray4 pixels");
}

static void identity_and_versions(void) {
    const char *first = "/me/SYNTH/one.png", *renamed = "/me/SYNTH/renamed.png", *moved = "/me/SYNTH/moved/renamed.png";
    picture(first, false, 2048); stamp(first, 1234567800);
    uint32_t id = find(first);
    CHECK(id != 0, "stable file id");
    thumb(id, INK_SRC_TOUCH, INK_RC_DONE, '0');
    uint64_t version = ink_test_thumb_version(id);
    thumb(id, INK_SRC_AI, INK_RC_DONE, '0');
    CHECK(ink_test_thumb_version(id) == version, "button and AI share cached object/version");
    CHECK(ink_fs_move(first, renamed) == 0, "rename");
    thumb(id, INK_SRC_AI, INK_RC_DONE, '0');
    CHECK(ink_test_thumb_version(id) == version, "rename preserves cached version");
    CHECK(ink_fs_mkdir("/me/SYNTH/moved") == 0 && ink_fs_move(renamed, moved) == 0, "move");
    thumb(id, INK_SRC_TOUCH, INK_RC_DONE, '0');
    CHECK(ink_test_thumb_version(id) == version, "move preserves cached version");
    picture(moved, true, 2048); stamp(moved, 1234567802);
    thumb(id, INK_SRC_AI, INK_RC_DONE, 'f');
    CHECK(ink_test_thumb_version(id) != version, "same-size replacement changes version");
    version = ink_test_thumb_version(id); stamp(moved, 1234567804);
    thumb(id, INK_SRC_AI, INK_RC_DONE, 'f');
    CHECK(ink_test_thumb_version(id) != version, "mtime alone changes version");
    version = ink_test_thumb_version(id);
    picture(moved, false, 4096); stamp(moved, 1234567804);
    thumb(id, INK_SRC_TOUCH, INK_RC_DONE, '0');
    CHECK(ink_test_thumb_version(id) != version, "size alone changes version");
    CHECK(ink_fs_write_atomic(moved, "SYNTH_BAD_PNG", 13) == 0, "corrupt replacement");
    thumb(id, INK_SRC_AI, INK_RC_FAILED, 0);
    CHECK(!ink_test_thumb_version(id), "failed replacement also removes old thumbnail");
    picture(moved, true, 2048); stamp(moved, 1234567806);
    thumb(id, INK_SRC_AI, INK_RC_DONE, 'f');
    CHECK(ink_fs_remove(moved) == 0, "delete indexed image");
    CHECK(!ink_test_thumb_version(id) && ink_test_thumb_bytes() == 0, "deletion immediately frees cached pixels");
    thumb(id, INK_SRC_AI, INK_RC_FAILED, 0);
    picture(moved, false, 2048);
    uint32_t replacement = find(moved);
    CHECK(replacement && replacement != id, "recreated image has different id");
    thumb(id, INK_SRC_AI, INK_RC_FAILED, 0);
    thumb(replacement, INK_SRC_TOUCH, INK_RC_DONE, '0');
    int job = ink_fs_delete_start("/me/SYNTH/moved"); ink_fs_job_status_t st;
    CHECK(job > 0 && WAIT_FOR(ink_fs_job_status(job, &st) == 0 && st.state != INK_FS_JOB_RUNNING, 5000) && st.state == INK_FS_JOB_DONE, "background tree deletion completes");
    CHECK(!ink_test_thumb_version(replacement) && ink_test_thumb_bytes() == 0, "background deletion clears thumbnails");
}

static void limits_and_isolation(void) {
    uint32_t ids[7]; char path[128];
    for (unsigned i = 0; i < 7; i++) {
        snprintf(path, sizeof path, "/me/SYNTH/bounded-%u.png", i);
        picture(path, false, 2048); ids[i] = find(path);
        thumb(ids[i], INK_SRC_AI, INK_RC_DONE, '0');
        CHECK(ink_test_thumb_bytes() <= INK_THUMB_CACHE_BYTES, "cache remains within 65536 bytes");
        if (i >= INK_THUMB_SLOTS) CHECK(!ink_test_thumb_version(ids[i - INK_THUMB_SLOTS]), "oldest image evicted");
    }
    CHECK(ink_test_thumb_bytes() == INK_THUMB_CACHE_BYTES, "four full-size thumbnails at capacity");
    CHECK(ink_member_switch("kid") == 0, "switch to child");
    thumb(ids[6], INK_SRC_AI, INK_RC_REJECTED, 0);
    CHECK(WAIT_FOR(ink_test_thumb_bytes() == 0, 3000), "member change clears cache and child cannot read parent");
    CHECK(ink_member_switch("owner") == 0, "return to owner");
    thumb(ids[6], INK_SRC_TOUCH, INK_RC_DONE, '0');
    sim_sd_set_present(false);
    thumb(ids[6], INK_SRC_AI, INK_RC_FAILED, 0);
    CHECK(ink_test_thumb_bytes() == 0, "no cached image served from an absent card");
    sim_sd_set_present(true);
}

static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_exists("kid") && ink_cap_find("file.thumb"), 3000), "product services ready");
    const ink_cap_t *cap = ink_cap_find("file.thumb");
    CHECK(cap && cap->exec == INK_EXEC_MEDIA && cap->effect == INK_FX_READ && ink_cap_ai_callable(cap), "read capability uses existing media executor and is AI-accessible");
    identity_and_versions(); limits_and_isolation();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "thumb-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "thumbnail test task");
}
