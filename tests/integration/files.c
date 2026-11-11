/*
 * The file service (system services design §4): paths and who may do what, copy and delete jobs, the USB
 * disk, a card that goes away or fills up, and all of it through the capabilities the AI uses. Results are
 * the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/task.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

static void wait_until_s(int s) {
    while (osal_now_us() < (uint64_t)s * 1000000ull) osal_task_delay_us(100000);
}

static int write_text(const char *path, const char *text) { return ink_fs_write_atomic(path, text, strlen(text)); }

static int read_text(const char *path, char *out, size_t cap) {
    ink_fs_file_t *f;
    int r = ink_fs_open(path, INK_FS_READ, &f);
    if (r) return r;
    int64_t n = ink_fs_read(f, out, cap - 1);
    ink_fs_close(f);
    if (n < 0) return (int)n;
    out[n] = 0;
    return 0;
}

static bool listed(const char *dir, const char *name) {
    ink_fs_dir_t *d;
    if (ink_fs_dir_open(dir, &d)) return false;
    ink_fs_stat_t st;
    bool hit = false;
    while (ink_fs_dir_next(d, &st) == 1)
        if (!strcmp(st.name, name)) hit = true;
    ink_fs_dir_close(d);
    return hit;
}

static volatile int g_changed, g_jobs_done, g_tasks_finished;
static volatile int g_rescanned;
static char g_task_state[16];
static char g_last_path[INK_FS_PATH_MAX], g_last_kind[16];
static bool g_job_ok;
static int g_usb_events;

static void on_file(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id, "file.changed")) {
        ink_args_get_str(a, "path", g_last_path, sizeof g_last_path);
        ink_args_get_str(a, "kind", g_last_kind, sizeof g_last_kind);
        if (!strcmp(g_last_kind, "rescan")) g_rescanned++;
        g_changed++;
    } else if (!strcmp(id, "job.finished")) {
        ink_args_get_str(a, "state", g_task_state, sizeof g_task_state);
        g_tasks_finished++;
    } else if (!strcmp(id, "file.job_done")) {
        ink_args_get_bool(a, "ok", &g_job_ok);
        g_jobs_done++;
    }
}

static void on_usb(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    (void)a;
    g_usb_events++;
}

static void paths(void) {
    static const char *const BAD[] = {
        "", "/", "me", "/user/settings.json", "/user/sys/x", "/content/books/a.txt", "/me/../x", "/me/./x", "/me//x", "/me/x/",
        "/me/.hidden", "/me/a\\b", "/me/a:b", "/me/a*b", "/me/a?b", "/me/a\"b", "/me/a<b", "/me/a|b", "/me/a\tb", "/me/\xC0\xAF", "/me/\xE4\xB8", "/nowhere/x",
        "/family/.m/owner/secret", "/family/.ink-logs/1-b1-2/logs.bin", "/me/trailing.", "/me/trailing "};
    char buf[8];
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
        ink_fs_file_t *f = NULL;
        int r = ink_fs_open(BAD[i], INK_FS_READ, &f);
        CHECK(r == INK_FS_ERR_PATH, "a path that must be refused: \"%s\" (%d)", BAD[i], r);
        if (!r) ink_fs_close(f);
    }
    char longp[200] = "/me/";
    memset(longp + 4, 'a', 150);
    longp[154] = 0;
    CHECK(ink_fs_mkdir(longp) == INK_FS_ERR_PATH, "too long is refused");
    CHECK(read_text("/me/no-such-file", buf, sizeof buf) == INK_FS_ERR_NOT_FOUND, "a missing file is not found");
    ink_fs_dir_t *d;
    ink_fs_stat_t st;
    CHECK(ink_fs_dir_open("/me", &d) == 0 && ink_fs_dir_next(d, &st) == 0, "a member's place is there from the start, empty");
    ink_fs_dir_close(d);
    CHECK(listed("/", "me") && listed("/", "family") && listed("/", "members"), "the root lists the virtual places");
}

static void basics(void) {
    static char buf[256];
    int changed0 = g_changed;
    CHECK(write_text("/me/notes/deep/a.txt", "hello") == 0, "writing makes the missing directories");
    CHECK(g_changed == changed0 + 1 && !strcmp(g_last_path, "/me/notes/deep/a.txt") && !strcmp(g_last_kind, "written"), "and says so (%s %s)", g_last_path, g_last_kind);
    CHECK(read_text("/me/notes/deep/a.txt", buf, sizeof buf) == 0 && !strcmp(buf, "hello"), "it reads back (%s)", buf);
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/notes/deep/a.txt", &st) == 0 && !st.is_dir && st.size == 5 && !strcmp(st.name, "a.txt"), "stat of a file");
    CHECK(ink_fs_stat("/me/notes", &st) == 0 && st.is_dir, "stat of a directory");
    CHECK(listed("/me/notes/deep", "a.txt") && !listed("/me/notes/deep", "a.txt.tmp"), "the temporary file is gone and listed nowhere");
    /* a copy cut short by a power-off left this: not a file to anyone looking */
    hal_file_t *stale = hal_fs_open("/content/.m/owner/notes/stale.bin.part", HAL_O_WRITE | HAL_O_CREATE);
    CHECK(stale != NULL, "what a power cut left");
    if (stale) hal_fs_close(stale);
    CHECK(listed("/me/notes", "deep") && !listed("/me/notes", "stale.bin.part"), "does not show in the list");
    hal_fs_remove("/content/.m/owner/notes/stale.bin.part");
    CHECK(write_text("/me/notes/deep/a.txt", "replaced") == 0 && read_text("/me/notes/deep/a.txt", buf, sizeof buf) == 0 && !strcmp(buf, "replaced"), "an atomic write replaces");
    ink_fs_file_t *f;
    CHECK(ink_fs_open("/me/notes/deep/a.txt", INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL, &f) == INK_FS_ERR_EXISTS, "exclusive create refuses an existing file");
    CHECK(ink_fs_open("/me/notes/b.bin", INK_FS_WRITE | INK_FS_CREATE, &f) == 0, "create");
    CHECK(ink_fs_write(f, "1234567890", 10) == 10 && ink_fs_tell(f) == 10 && ink_fs_sync(f) == 0, "write, tell, sync");
    CHECK(ink_fs_close(f) == 0, "close");
    CHECK(ink_fs_open("/me/notes/b.bin", INK_FS_APPEND, &f) == 0 && ink_fs_write(f, "ab", 2) == 2 && ink_fs_close(f) == 0, "append");
    CHECK(ink_fs_open("/me/notes/b.bin", INK_FS_READ, &f) == 0 && ink_fs_seek(f, 8, 0) == 0 && ink_fs_read(f, buf, 10) == 4 && !memcmp(buf, "90ab", 4) && ink_fs_close(f) == 0, "seek and read");
    CHECK(ink_fs_mkdir("/me/notes/deep") == 0, "an existing directory is fine");
    CHECK(ink_fs_remove("/me/no-such") == INK_FS_ERR_NOT_FOUND, "removing what is not there says so");

    /* moving */
    CHECK(ink_fs_move("/me/notes/b.bin", "/me/moved/c.bin") == 0 && listed("/me/moved", "c.bin") && !listed("/me/notes", "b.bin"), "move, with new directories");
    CHECK(write_text("/me/moved/d.bin", "x") == 0 && ink_fs_move("/me/moved/c.bin", "/me/moved/d.bin") == INK_FS_ERR_EXISTS, "a move does not overwrite");
    CHECK(ink_fs_move("/me/moved", "/me/moved/inside") == INK_FS_ERR_PATH, "nor go into itself");
    CHECK(ink_fs_move("/me/none", "/me/x") == INK_FS_ERR_NOT_FOUND, "a missing source is said");
    CHECK(ink_fs_remove("/me/moved/d.bin") == 0 && ink_fs_remove("/me/moved/c.bin") == 0 && ink_fs_remove("/me/moved") == 0, "remove files, then the empty directory");
    /* open files are counted */
    ink_fs_file_t *many[9];
    int n = 0;
    while (n < 9 && ink_fs_open("/me/notes/deep/a.txt", INK_FS_READ, &many[n]) == 0) n++;
    CHECK(n == 8, "eight files at a time (%d)", n);
    while (n > 0) ink_fs_close(many[--n]);
    uint64_t free_b = 0, total = 0;
    CHECK(ink_fs_free(&free_b, &total) == 0 && total > free_b && free_b > 0, "free space");
    ink_fs_refresh_state();
    int64_t v = 0;
    CHECK(ink_state_get_int(ink_state_find("storage.free"), &v) && v == (int64_t)free_b, "and it is system state");
}

static void members(void) {
    static char buf[64];
    CHECK(write_text("/me/private.txt", "adult") == 0 && write_text("/family/shared.txt", "for all") == 0, "an adult writes both places");
    CHECK(ink_member_switch("kid") == 0, "switch to the child");
    CHECK(read_text("/me/private.txt", buf, sizeof buf) == INK_FS_ERR_NOT_FOUND, "the child's /me is another place");
    CHECK(read_text("/members/owner/private.txt", buf, sizeof buf) == INK_FS_ERR_DENIED, "another member's files are refused");
    CHECK(!listed("/", "members") && listed("/", "me"), "and not listed");
    CHECK(read_text("/family/shared.txt", buf, sizeof buf) == 0 && !strcmp(buf, "for all"), "family content can be read");
    CHECK(write_text("/family/mine.txt", "x") == INK_FS_ERR_DENIED, "but not written by a child");
    ink_fs_file_t *f;
    CHECK(ink_fs_open("/family/shared.txt", INK_FS_APPEND, &f) == INK_FS_ERR_DENIED, "not even appended to");
    CHECK(ink_fs_remove("/family/shared.txt") == INK_FS_ERR_DENIED && ink_fs_move("/family/shared.txt", "/me/s.txt") == INK_FS_ERR_DENIED, "nor removed or moved out");
    CHECK(ink_fs_delete_start("/family/shared.txt") == INK_FS_ERR_DENIED, "nor by a job");
    CHECK(write_text("/me/kid.txt", "child") == 0, "their own place is theirs");
    int id = ink_fs_copy_start("/family/shared.txt", "/me/shared-copy.txt");
    CHECK(id > 0, "they can copy family content to it (%d)", id);
    for (int i = 0; i < 300 && g_jobs_done == 0; i++) osal_task_delay_us(10000);
    CHECK(read_text("/me/shared-copy.txt", buf, sizeof buf) == 0 && !strcmp(buf, "for all"), "and it is there");
    ink_member_test_reset();
    CHECK(read_text("/members/kid/kid.txt", buf, sizeof buf) == 0 && !strcmp(buf, "child"), "an adult can look at a child's files");
    CHECK(read_text("/members/../owner/private.txt", buf, sizeof buf) == INK_FS_ERR_PATH, "a path cannot climb");
    CHECK(read_text("/me/private.txt", buf, sizeof buf) == 0 && !strcmp(buf, "adult"), "the adult's own are as they were");
}

static int wait_job(int id, ink_fs_job_status_t *st) {
    for (int i = 0; i < 6000; i++) {
        ink_fs_job_status(id, st);
        if (st->state != INK_FS_JOB_RUNNING) {
            for (int k = 0; k < 100 && ink_fs_job_status(id, st) == 0; k++) osal_task_delay_us(1000); /* its end is told a moment after */
            return st->state;
        }
        osal_task_delay_us(10000);
    }
    return -1;
}

static void make_file(const char *path, size_t size) {
    static uint8_t chunk[4096];
    ink_fs_file_t *f;
    if (ink_fs_open(path, INK_FS_WRITE | INK_FS_CREATE, &f)) return;
    for (size_t off = 0; off < size; off += sizeof chunk) {
        for (size_t i = 0; i < sizeof chunk; i++) chunk[i] = (uint8_t)((off + i) * 31 / 7);
        ink_fs_write(f, chunk, size - off < sizeof chunk ? size - off : sizeof chunk);
    }
    ink_fs_close(f);
}

static bool same_file(const char *a, const char *b) {
    static uint8_t x[4096], y[4096];
    ink_fs_file_t *f, *g;
    if (ink_fs_open(a, INK_FS_READ, &f)) return false;
    if (ink_fs_open(b, INK_FS_READ, &g)) {
        ink_fs_close(f);
        return false;
    }
    bool same = true;
    for (;;) {
        int64_t n = ink_fs_read(f, x, sizeof x), m = ink_fs_read(g, y, sizeof y);
        if (n != m || (n > 0 && memcmp(x, y, (size_t)n))) same = false;
        if (n <= 0 || m <= 0 || !same) break;
    }
    ink_fs_close(f);
    ink_fs_close(g);
    return same;
}

static volatile int g_done;
static ink_receipt_t g_rc;
static char g_result[8192];

static void done_cb(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_rc = *rc;
    g_result[0] = 0;
    if (rc->result) snprintf(g_result, sizeof g_result, "%s", rc->result);
    g_rc.result = NULL;
    g_done++;
}

static bool call_as(const char *id, const char *args, ink_source_t src) {
    int before = g_done;
    ink_call_opts_t o = {.src = src, .args_json = args, .reply_exec = INK_EXEC_DIRECT, .done = done_cb};
    if (ink_cap_call(id, NULL, &o) != 0) return false;
    for (int i = 0; i < 3000 && g_done == before; i++) osal_task_delay_us(10000);
    return g_done > before;
}
#define AI(id, args) call_as(id, args, INK_SRC_AI)

/* The list when it is full: all of it still comes back, and what ended makes room for what is new. */
static void list_full(void) {
    static const char LONG[] = "一个很长很长的名字一个很长很长的名字一个很长很长的名字一个很长很长";
    int first = 0;
    for (int i = 0; i < 30; i++) {
        int id = ink_task_begin("test.long.kind.name.here", LONG, false, NULL, NULL);
        CHECK(id > 0, "room for it (%d)", i);
        if (!first) first = id;
        ink_task_progress(id, 50, "stage-word-long");
        ink_task_end(id, INK_TASK_DONE, "结果也很长结果也很长结果也很长结果也很长结果也很长结果也很长", "file:/me/a/long/path/of/the/thing/made/here");
    }
    CHECK(AI("job.list", NULL) && g_rc.status == INK_RC_DONE, "all of it comes back (%s)", g_rc.detail);
    int shown = 0;
    for (const char *p = g_result; (p = strstr(p, "\"kind\":")) != NULL; p++) shown++;
    CHECK(shown == 16, "as many as it keeps (%d)", shown);
    char oldest[32];
    snprintf(oldest, sizeof oldest, "\"id\":%d,", first);
    CHECK(!strstr(g_result, oldest), "the oldest made room");
    int running[17], n = 0;
    while (n < 17 && (running[n] = ink_task_begin("test.run", "跑着的", false, NULL, NULL)) > 0) n++;
    CHECK(n == 16, "what runs stays: no room for a 17th (%d)", n);
    ink_task_progress(0, 10, NULL); /* nothing happens */
    ink_task_end(0, INK_TASK_DONE, NULL, NULL);
    for (int i = 0; i < n; i++) ink_task_end(running[i], INK_TASK_DONE, NULL, NULL);
}

static void jobs(void) {
    list_full();
    ink_fs_job_status_t st;
    /* one large file */
    make_file("/me/big.bin", 300 * 1024);
    int done0 = g_jobs_done;
    int id = ink_fs_copy_start("/me/big.bin", "/me/copies/big.bin");
    CHECK(id > 0, "a copy starts (%d)", id);
    CHECK(ink_fs_copy_start("/me/big.bin", "/me/copies/other.bin") == INK_FS_ERR_BUSY, "one job at a time");
    bool seen_part = false;
    for (int i = 0; i < 2000; i++) {
        ink_fs_job_status(id, &st);
        if (st.state != INK_FS_JOB_RUNNING) break;
        seen_part = seen_part || (st.bytes_done > 0 && st.bytes_done < st.bytes_total && !listed("/me/copies", "big.bin"));
        osal_task_delay_us(10000);
    }
    wait_job(id, &st);
    CHECK(seen_part, "while it runs the progress moves and the file is not there under its name");
    CHECK(st.state == INK_FS_JOB_DONE && st.bytes_done == 300 * 1024 && st.files_done == 1, "it finishes (%llu bytes)", (unsigned long long)st.bytes_done);
    CHECK(same_file("/me/big.bin", "/me/copies/big.bin"), "and the copy is the same");
    CHECK(g_jobs_done == done0 + 1 && g_job_ok, "the end is told");
    CHECK(ink_fs_copy_start("/me/big.bin", "/me/copies/big.bin") == INK_FS_ERR_EXISTS, "a copy does not overwrite");
    CHECK(ink_fs_copy_start("/me/nothing", "/me/x") == INK_FS_ERR_NOT_FOUND, "a missing source is said at once");

    /* in the list of what goes on (system UI spec §7.7): how far, and how it ended, for a person and the AI alike */
    CHECK(AI("job.list", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"kind\":\"file.copy\"") && strstr(g_result, "拷贝 big.bin") && strstr(g_result, "\"state\":\"done\"") && strstr(g_result, "\"percent\":100")
          && strstr(g_result, "file:/me/copies/big.bin") && strstr(g_result, "拷贝好了") && !strstr(g_result, "can_cancel"), "the copy is there, done (%s)", g_result);
    int finished0 = g_tasks_finished;
    id = ink_fs_copy_start("/me/big.bin", "/me/copies/seen.bin");
    int moved = -1, percent = -1;
    for (int i = 0; i < 2000 && moved < 0; i++) {
        CHECK(AI("job.list", NULL), "the list");
        if (strstr(g_result, "\"state\":\"running\"") && strstr(g_result, "\"can_cancel\":true")) {
            const char *p = strstr(g_result, "\"percent\":");
            if (p && sscanf(p + 10, "%d", &percent) == 1 && percent > 0 && percent < 100) moved = percent;
        }
        osal_task_delay_us(5000);
    }
    CHECK(moved > 0, "while it runs: how far, and it can be stopped (%s)", g_result);
    CHECK(strstr(g_result, "\"state\":\"running\"") < strstr(g_result, "\"state\":\"done\""), "what runs comes first (%s)", g_result);
    int64_t running = -1;
    CHECK(ink_state_get_int(ink_state_find("job.running"), &running) && running == 1, "the state counts it (%d)", (int)running);
    /* someone else does not see it, and cannot stop it */
    int seen_id = 0;
    const char *found = strstr(g_result, "{\"id\":");
    if (found) sscanf(found + 6, "%d", &seen_id);
    CHECK(seen_id > 0, "its number");
    char job_args[48];
    snprintf(job_args, sizeof job_args, "{\"id\":%d}", seen_id);
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(AI("job.list", NULL) && !strstr(g_result, "seen") && !strstr(g_result, "big.bin"), "not for them to see (%s)", g_result);
    CHECK(AI("job.cancel", job_args) && g_rc.status == INK_RC_FAILED, "nor to stop");
    ink_member_test_reset();
    wait_job(id, &st);
    for (int i = 0; i < 200 && g_tasks_finished == finished0; i++) osal_task_delay_us(5000);
    CHECK(g_tasks_finished == finished0 + 1 && !strcmp(g_task_state, "done"), "the end is told (%s)", g_task_state);
    CHECK(ink_state_get_int(ink_state_find("job.running"), &running) && running == 0, "and it stops counting");

    /* cancelled half way: nothing under its name, no .part left */
    id = ink_fs_copy_start("/me/big.bin", "/me/copies/cancelled.bin");
    for (int i = 0; i < 500; i++) {
        ink_fs_job_status(id, &st);
        if (st.bytes_done > 0) break;
        osal_task_delay_us(5000);
    }
    CHECK(AI("job.list", NULL) && (found = strstr(g_result, "{\"id\":")) && sscanf(found + 6, "%d", &seen_id) == 1, "the list (%s)", g_result);
    snprintf(job_args, sizeof job_args, "{\"id\":%d}", seen_id);
    CHECK(AI("job.cancel", job_args) && g_rc.status == INK_RC_DONE, "stopped from the list (%s)", g_rc.detail);
    wait_job(id, &st);
    CHECK(AI("job.list", NULL) && strstr(g_result, "\"state\":\"cancelled\"") && strstr(g_result, "停下了"), "and the list says so (%s)", g_result);
    CHECK(strstr(g_result, "\"state\":\"cancelled\"") < strstr(g_result, "\"state\":\"done\""), "the newest of what ended first (%s)", g_result);
    CHECK(AI("job.cancel", job_args) && g_rc.status == INK_RC_FAILED, "it cannot be stopped twice");
    ink_fs_stat_t fs;
    CHECK(st.state == INK_FS_JOB_CANCELLED, "cancelled (%d)", st.state);
    CHECK(ink_fs_stat("/me/copies/cancelled.bin", &fs) == INK_FS_ERR_NOT_FOUND && hal_fs_size("/content/.m/owner/copies/cancelled.bin.part") < 0, "nothing is left of it");

    /* a tree */
    make_file("/me/tree/a/one.bin", 10000);
    make_file("/me/tree/a/b/two.bin", 20000);
    make_file("/me/tree/three.bin", 30000);
    CHECK(ink_fs_copy_start("/me/tree", "/me/tree/inside") == INK_FS_ERR_PATH, "a tree does not go into itself");
    id = ink_fs_copy_start("/me/tree", "/me/tree2");
    CHECK(id > 0, "a tree starts");
    wait_job(id, &st);
    CHECK(st.state == INK_FS_JOB_DONE && st.files_done == 3 && st.files_total == 3 && st.bytes_total == 60000, "all of it (%d, %u files, %llu bytes)", st.state, st.files_done, (unsigned long long)st.bytes_total);
    CHECK(same_file("/me/tree/a/b/two.bin", "/me/tree2/a/b/two.bin") && same_file("/me/tree/three.bin", "/me/tree2/three.bin"), "the same content, the same shape");

    /* delete a tree */
    id = ink_fs_delete_start("/me/tree2");
    CHECK(id > 0, "deleting a tree starts");
    wait_job(id, &st);
    CHECK(st.state == INK_FS_JOB_DONE && ink_fs_stat("/me/tree2", &fs) == INK_FS_ERR_NOT_FOUND && st.files_done == 3, "it is gone (%d, %u files)", st.state, st.files_done);
    CHECK(ink_fs_stat("/me/tree/three.bin", &fs) == 0, "the original is not touched");
    id = ink_fs_delete_start("/me/copies/big.bin");
    wait_job(id, &st);
    CHECK(st.state == INK_FS_JOB_DONE && ink_fs_stat("/me/copies/big.bin", &fs) == INK_FS_ERR_NOT_FOUND, "a single file too");
}

/* ---- through the bus, as the AI ---- */


static void capabilities(void) {
    CHECK(AI("file.list", "{\"path\":\"/me/tree\"}") && g_rc.status == INK_RC_DONE && strstr(g_result, "\"name\":\"three.bin\"") && strstr(g_result, "\"size\":30000") && strstr(g_result, "\"name\":\"a\",\"dir\":true"),
          "the AI lists a directory (%s %s)", g_result, g_rc.detail);
    CHECK(AI("file.list", "{\"path\":\"/me/tree\",\"limit\":1}") && strstr(g_result, "\"next\":\"1\""), "in pages (%s)", g_result);
    CHECK(AI("file.list", "{\"path\":\"/me/tree\",\"limit\":1,\"cursor\":\"1\"}") && !strstr(g_result, "\"next\":\"1\"") && strstr(g_result, "\"entries\":[{"), "the next page (%s)", g_result);
    CHECK(AI("file.stat", "{\"path\":\"/me/tree/three.bin\"}") && strstr(g_result, "\"size\":30000"), "stat (%s)", g_result);
    CHECK(AI("file.write", "{\"path\":\"/me/ai.txt\",\"text\":\"你好，世界\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "the AI writes a new file (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    char object[48];
    snprintf(object, sizeof object, "%s", g_rc.object);
    CHECK(AI("file.write", "{\"path\":\"/me/ai.txt\",\"text\":\"over\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "exists"), "it does not overwrite");
    CHECK(AI("file.write", "{\"path\":\"/me/ai.txt\",\"text\":\"！\",\"append\":true}") && g_rc.status == INK_RC_DONE, "it appends");
    CHECK(AI("file.read", "{\"path\":\"/me/ai.txt\"}") && strstr(g_result, "\"text\":\"你好，世界！\"") && strstr(g_result, "\"end\":true"), "it reads text (%s)", g_result);
    CHECK(AI("file.read", "{\"path\":\"/me/ai.txt\",\"max_bytes\":4}") && strstr(g_result, "\"text\":\"你\"") && strstr(g_result, "\"next_offset\":3"), "a piece ends between characters (%s)", g_result);
    CHECK(AI("file.read", "{\"path\":\"/me/big.bin\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "binary"), "a binary file is said to be one");
    CHECK(AI("file.move", "{\"from\":\"/me/ai.txt\",\"to\":\"/me/ai2.txt\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "the AI moves it");
    char args[120];
    snprintf(args, sizeof args, "{\"object\":\"%s\"}", g_rc.object);
    ink_fs_stat_t st;
    CHECK(AI("system.undo", args) && g_rc.status == INK_RC_UNDONE && ink_fs_stat("/me/ai.txt", &st) == 0 && ink_fs_stat("/me/ai2.txt", &st) != 0, "undo moves it back (%s)", g_rc.detail);
    CHECK(AI("file.delete", "{\"path\":\"/me/ai.txt\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION && ink_fs_stat("/me/ai.txt", &st) == 0, "a delete asked by the AI waits for a person");
    CHECK(call_as("file.delete", "{\"path\":\"/me/ai.txt\"}", INK_SRC_TOUCH) && g_rc.status == INK_RC_PENDING_CONFIRMATION && ink_fs_stat("/me/ai.txt", &st) == 0, "D-105: irreversible deletion asks by hand too");
    unsigned delete_token = 0;
    sscanf(g_rc.object, "confirm:%u", &delete_token);
    CHECK(ink_confirm_decide(delete_token, true, INK_SRC_TOUCH) == 0, "person accepts deletion");
    for (int wait = 0; wait < 150 && ink_fs_stat("/me/ai.txt", &st) == 0; wait++) osal_task_delay_us(20000);
    CHECK(ink_fs_stat("/me/ai.txt", &st) == INK_FS_ERR_NOT_FOUND, "deletion runs after the person accepts");
    CHECK(AI("file.copy", "{\"from\":\"/me/tree/a/one.bin\",\"to\":\"/me/one.bin\"}") && g_rc.status == INK_RC_DONE && ink_fs_stat("/me/one.bin", &st) == 0, "a small copy answers when it is done (%s)", g_rc.detail);
    CHECK(AI("file.copy", "{\"from\":\"/me/big.bin\",\"to\":\"/me/big2.bin\"}") && g_rc.status == INK_RC_QUEUED && !strncmp(g_rc.object, "file-job:", 9), "a large one answers queued and names the job (%s)", g_rc.object);
    ink_fs_job_status_t js;
    wait_job(atoi(g_rc.object + 9), &js);
    CHECK(js.state == INK_FS_JOB_DONE, "and finishes");
    CHECK(AI("file.free", NULL) && strstr(g_result, "\"free\":"), "free space (%s)", g_result);
    CHECK(AI("file.read", "{\"path\":\"/app/reader/state\"}") && g_rc.status == INK_RC_INVALID, "an app's own files are not on offer");
    CHECK(AI("file.list", "{\"path\":\"/cache/sys\"}") && g_rc.status == INK_RC_INVALID, "nor the caches");
    CHECK(AI("file.read", "{\"path\":\"/user/sys/settings.json\"}") && g_rc.status == INK_RC_INVALID, "nor anything in the internal flash");
    CHECK(ink_member_switch("kid") == 0, "the child");
    CHECK(AI("file.write", "{\"path\":\"/family/x.txt\",\"text\":\"x\"}") && g_rc.status == INK_RC_REJECTED, "a child's AI cannot write family content (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("file.read", "{\"path\":\"/members/owner/private.txt\"}") && g_rc.status == INK_RC_REJECTED, "nor read another member's");
    CHECK(AI("usb.mount", NULL) && g_rc.status == INK_RC_REJECTED, "nor lend the card (%s)", ink_rc_name(g_rc.status));
    CHECK(call_as("usb.mount", NULL, INK_SRC_TOUCH) && g_rc.status == INK_RC_REJECTED, "nor a child by touch");
    ink_member_test_reset();
    CHECK(AI("usb.mount", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "sharing the whole card still needs a person (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("usb.status", NULL) && strstr(g_result, "\"lent\":false"), "the status can be asked (%s)", g_result);
}

/* ---- the USB disk: the script plugs at 40 s ---- */

static ink_fs_file_t *g_holder;
static int g_release_told;
static void release_cb(void *ctx) {
    (void)ctx;
    g_release_told++;
    if (g_holder) {
        ink_fs_close(g_holder);
        g_holder = NULL;
    }
}

static int mount_results;
static ink_rc_t mount_status;
static char mount_detail[96];
static void mount_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    char cap[48], status[24];
    ink_args_get_str(a, "cap", cap, sizeof cap);
    if (strcmp(cap, "usb.mount")) return;
    ink_args_get_str(a, "status", status, sizeof status);
    mount_status = !strcmp(status, "done") ? INK_RC_DONE : INK_RC_FAILED;
    ink_args_get_str(a, "detail", mount_detail, sizeof mount_detail);
    mount_results++;
}
static bool mount_by_person(void) {
    int before = mount_results;
    if (!call_as("usb.mount", NULL, INK_SRC_TOUCH) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    if (ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0) return false;
    for (int i = 0; i < 1000 && mount_results == before; i++) osal_task_delay_us(10000);
    if (mount_results == before) return false;
    g_rc.status = mount_status;
    snprintf(g_rc.detail, sizeof g_rc.detail, "%s", mount_detail);
    return true;
}

static void usb_disk(void) {
    static char buf[64];
    CHECK(ink_cap_subscribe("confirm.result", mount_result, NULL, INK_EXEC_DIRECT) == 0, "confirmed USB results");
    CHECK(mount_by_person() && g_rc.status == INK_RC_FAILED, "no computer, no USB disk (%s)", g_rc.detail);
    wait_until_s(41);
    int64_t v = 0;
    CHECK(ink_state_get_int(ink_state_find("usb.connected"), &v) && v == 1 && g_usb_events >= 1, "the computer is noticed");
    /* a file held by someone who does not let go */
    ink_fs_file_t *stuck;
    CHECK(ink_fs_open("/me/tree/three.bin", INK_FS_READ, &stuck) == 0, "someone has a file open");
    uint64_t t0 = osal_now_us();
    CHECK(ink_usb_disk_start() == INK_FS_ERR_BUSY && !ink_usb_disk_active(), "lending waits a moment, then says busy");
    CHECK(osal_now_us() - t0 >= 3000000, "after three seconds");
    ink_fs_close(stuck);
    /* one who lets go when told */
    CHECK(ink_fs_on_release(release_cb, NULL) == 0, "a holder listens");
    CHECK(ink_fs_open("/me/tree/three.bin", INK_FS_READ, &g_holder) == 0, "and has a file open");
    int sleeps0 = sim_power_sleeps();
    CHECK(mount_by_person() && g_rc.status == INK_RC_DONE && ink_usb_disk_active() && g_release_told == 1 && !g_holder, "it closes it and the card is lent (%s)", g_rc.detail);
    char mode[16] = "";
    ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
    CHECK(!strcmp(mode, "usb"), "system state says so (%s)", mode);
    /* everything is refused now, through the service and below it */
    ink_fs_file_t *f;
    CHECK(ink_fs_open("/me/x", INK_FS_READ, &f) == INK_FS_ERR_USB && ink_fs_mkdir("/me/y") == INK_FS_ERR_USB, "the service refuses");
    CHECK(ink_fs_copy_start("/me/big.bin", "/me/big3.bin") == INK_FS_ERR_USB && ink_fs_free(NULL, NULL) == INK_FS_ERR_USB, "jobs and free space too");
    CHECK(hal_fs_open("/content/x", HAL_O_READ) == NULL, "and the card is really gone from the system");
    CHECK(AI("file.list", "{\"path\":\"/me\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "computer"), "the AI hears why (%s)", g_rc.detail);
    /* the internal flash goes on working: only the card was lent */
    CHECK(ink_storage_write_atomic("/user/sys/while-lent", "ok", 2) == 0, "the data store's flash is not lent");
    /* it keeps the chip awake, locked or not */
    ink_power_lock();
    wait_until_s(49);
    CHECK(sim_power_sleeps() == sleeps0, "locking does not put USB to sleep");
    ink_power_unlock(osal_now_us());
    /* the computer ejects at 50 s */
    int changed0 = g_changed, rescanned0 = g_rescanned;
    wait_until_s(51);
    CHECK(!ink_usb_disk_active(), "ejected: the card comes back");
    ink_state_get_str(ink_state_find("storage.mode"), mode, sizeof mode);
    CHECK(!strcmp(mode, "normal"), "state says so");
    CHECK(g_changed > changed0 && g_rescanned > rescanned0, "and everyone is told to look again (%s)", g_last_kind);
    CHECK(read_text("/family/from-pc/hello.txt", buf, sizeof buf) == 0 && !strcmp(buf, "copied by the computer\n"), "what the computer copied is there (%s)", buf);
    CHECK(write_text("/me/after.txt", "back") == 0, "and the card works again");

    /* pulled out without ejecting: 70 s plug, 80 s unplug. It does not mount again, and is not formatted. */
    wait_until_s(71);
    CHECK(ink_usb_disk_start() == 0, "lent again");
    wait_until_s(81);
    CHECK(!ink_usb_disk_active(), "the cable went: it is taken back");
    int64_t card = 1;
    CHECK(ink_state_get_int(ink_state_find("storage.card"), &card) && card == 0 && ink_fs_open("/me/after.txt", INK_FS_READ, &f) == INK_FS_ERR_NO_CARD, "not ejected: it does not mount, the system says no card");
    wait_until_s(91); /* the script puts it back, repaired */
    ink_fs_refresh_state();
    CHECK(ink_state_get_int(ink_state_find("storage.card"), &card) && card == 1 && read_text("/me/after.txt", buf, sizeof buf) == 0, "repaired and back: nothing was lost");
}

static void card_trouble(void) {
    static char buf[64];
    /* taken out at 100 s, back at 105 s; a file open across it fails, no crash */
    ink_fs_file_t *f;
    CHECK(ink_fs_open("/me/after.txt", INK_FS_READ, &f) == 0, "a file is open when the card goes");
    wait_until_s(101);
    int64_t n = ink_fs_read(f, buf, sizeof buf);
    CHECK(n == INK_FS_ERR_NO_CARD, "reading it says no card (%lld)", (long long)n);
    CHECK(read_text("/me/after.txt", buf, sizeof buf) == INK_FS_ERR_NO_CARD, "and so does opening anything");
    int64_t card = 1;
    CHECK(ink_state_get_int(ink_state_find("storage.card"), &card) && card == 0, "system state knows");
    ink_fs_close(f);
    wait_until_s(106);
    ink_fs_refresh_state();
    CHECK(read_text("/me/after.txt", buf, sizeof buf) == 0, "back in: it works");

    /* a full card: 10 KiB left at 110 s, no limit at 115 s */
    wait_until_s(111);
    static char big[16 * 1024];
    memset(big, 'z', sizeof big);
    ink_fs_file_t *w;
    CHECK(ink_fs_open("/me/full.bin", INK_FS_WRITE | INK_FS_CREATE, &w) == 0, "create on a nearly full card");
    int64_t wrote = ink_fs_write(w, big, sizeof big);
    int64_t again = ink_fs_write(w, big, sizeof big);
    CHECK(wrote == 10 * 1024 && again == INK_FS_ERR_NO_SPACE, "a write takes what fits, then says the card is full (%lld, %lld)", (long long)wrote, (long long)again);
    ink_fs_close(w);
    CHECK(ink_fs_write_atomic("/me/after.txt", big, sizeof big) == INK_FS_ERR_NO_SPACE, "an atomic write fails");
    CHECK(read_text("/me/after.txt", buf, sizeof buf) == 0 && !strcmp(buf, "back"), "and leaves the old content");
    CHECK(ink_fs_copy_start("/me/big.bin", "/me/too-big.bin") == INK_FS_ERR_NO_SPACE, "a copy that cannot fit is refused at the start");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/too-big.bin", &st) == INK_FS_ERR_NOT_FOUND, "and leaves nothing");
    wait_until_s(116);
}

/* The card as it comes: a run does not see what the last one left. */
static void clean_card(void *arg) {
    (void)arg;
    char cmd[700];
    snprintf(cmd, sizeof cmd, "rm -rf '%s/content' && mkdir -p '%s/content'", sim_sd_root(), sim_sd_root());
    if (system(cmd) != 0) sim_warn("cannot clean the card");
}

/* A page of Chinese is cut before a whole character: that is not the end (audit AUD15-003). */
static void long_chinese(void) {
    size_t n = 0;
    static char text[9000];
    while (n + 3 < 8000) n += (size_t)snprintf(text + n, sizeof text - n, "汉");
    CHECK(ink_fs_write_atomic("/me/long.txt", text, n) == 0, "a long text");
    size_t total = 0;
    int64_t off = 0;
    bool ended = false;
    for (int page = 0; page < 20 && !ended; page++) {
        char args[64];
        snprintf(args, sizeof args, "{\"path\":\"/me/long.txt\",\"offset\":%lld}", (long long)off);
        if (!AI("file.read", args) || g_rc.status != INK_RC_DONE) break;
        const char *t = strstr(g_result, "\"text\":\"");
        const char *e = t ? strchr(t + 8, '"') : NULL;
        const char *next = strstr(g_result, "\"next_offset\":");
        if (!t || !e || !next) break;
        total += (size_t)(e - (t + 8));
        off = atoll(next + 14);
        ended = strstr(g_result, "\"end\":true") != NULL;
    }
    CHECK(ended && total == n, "page by page, all of it (%zu of %zu)", total, n);
}

/* The places everyone sees are not folders to take away (audit AUD15-002). */
static void virtual_roots(void) {
    CHECK(write_text("/me/keep.txt", "mine") == 0 && write_text("/family/mine-too.txt", "ours") == 0, "something in each");
    CHECK(ink_fs_delete_start("/me") == INK_FS_ERR_DENIED, "/me cannot be deleted");
    CHECK(ink_fs_delete_start("/family") == INK_FS_ERR_DENIED, "/family neither");
    CHECK(ink_fs_delete_start("/members/owner") == INK_FS_ERR_DENIED, "nor a member's place");
    CHECK(ink_fs_move("/me", "/family/moved") == INK_FS_ERR_DENIED && ink_fs_move("/family", "/me/moved") == INK_FS_ERR_DENIED, "nor moved");
    /* a folder in the family's place is the family's: it goes */
    CHECK(write_text("/family/shelf/a.txt", "a") == 0, "a folder of theirs");
    int id = ink_fs_delete_start("/family/shelf");
    CHECK(id > 0, "can be deleted");
    ink_fs_job_status_t st = {0};
    for (int i = 0; i < 100 && ink_fs_job_status(id, &st) == 0 && st.state == INK_FS_JOB_RUNNING; i++) osal_task_delay_us(50000);
    ink_fs_stat_t fs;
    CHECK(ink_fs_stat("/family/shelf", &fs) == INK_FS_ERR_NOT_FOUND, "and is gone");
    CHECK(ink_fs_stat("/me/keep.txt", &fs) == 0, "what is elsewhere is not touched");
}

static void test_task(void *arg) {
    (void)arg;
    vtk_host_call(clean_card, NULL);
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_svc_init() != 0 || ink_confirm_init() != 0 || ink_task_init() != 0 || ink_data_init() != 0 || ink_power_init() != 0 || ink_file_service_init() != 0) {
        CHECK(0, "the services start");
        return;
    }
    ink_members_init();
    CHECK(ink_cap_subscribe("file.", on_file, NULL, INK_EXEC_DIRECT) == 0 && ink_cap_subscribe("usb.", on_usb, NULL, INK_EXEC_DIRECT) == 0, "subscribe to the events");
    CHECK(ink_cap_subscribe("job.", on_file, NULL, INK_EXEC_DIRECT) == 0 && ink_cap_subscribe("usb.", on_usb, NULL, INK_EXEC_DIRECT) == 0, "subscribe to the events");
    paths();
    basics();
    members();
    jobs();
    capabilities();
    usb_disk();
    card_trouble();
    virtual_roots();
    long_chinese();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "filetest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
