/* Real logger and file/NOR HAL, deterministic host fixtures; no device access. */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "sim.h"
#ifndef INK_LOG_SOURCE
#define INK_LOG_SOURCE "../../system/services/log/log.c"
#endif
#include INK_LOG_SOURCE

sim_options_t g_sim;
static uint64_t now;
static bool held;
static const char *reason = "poweron";
struct osal_mutex { bool held; };
struct hal_file { FILE *fp; };
static char root[1024];
void vtk_host_call(void (*fn)(void *), void *arg) { fn(arg); }
void *sim_host_malloc(size_t n) { return malloc(n); }
void sim_host_free(void *p) { free(p); }
void sim_copy_name(char *out, size_t n, const char *s) { snprintf(out, n, "%s", s); }
int vtk_task_count(void) { return 1; }
void vtk_task_info(int i, vtk_task_info_t *out) { (void)i; *out = (vtk_task_info_t){.name = "fake-task", .stack_target_bytes = 4096, .stack_used_est_bytes = 1024}; }
uint64_t osal_now_us(void) { return now; }
int64_t osal_wall_us(void) { return now ? 1800000000000000ll + (int64_t)now : 0; }
void *osal_malloc(size_t n, uint32_t caps) { (void)caps; return malloc(n); }
osal_mutex_t *osal_mutex_create(void) { return calloc(1, sizeof(osal_mutex_t)); }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t wait) {
    if (m->held) { assert(wait == OSAL_NO_WAIT); return OSAL_TIMEOUT; }
    m->held = true; return OSAL_OK;
}
void osal_mutex_unlock(osal_mutex_t *m) { assert(m->held); m->held = false; }
void osal_mem_stats(uint32_t caps, osal_mem_stats_t *s) { (void)caps; *s = (osal_mem_stats_t){.free = 1000, .min_free = 900, .largest_free = 800}; }
void hal_power_battery(hal_battery_t *b) { *b = (hal_battery_t){.percent = 70, .millivolts = 3900}; }
const char *hal_device_reset_reason(void) { return reason; }
bool ink_storage_writes_held(void) { return held; }
void ink_storage_log_ready(void) {}
bool ink_audio_capture_busy(void) { return false; }
bool ink_audio_recording(void) { return false; }
int ink_audio_focus_top(void) { return 0; }
void ink_storage_run(void (*fn)(void *), void *arg, bool guarded) { (void)guarded; fn(arg); }
int ink_cap_register(const ink_cap_t *c, int n) { assert(n == 2 && !strcmp(c[0].id, "system.report_problem")); return 0; }
int ink_cap_subscribe(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec) { (void)fn; (void)ctx; (void)exec; assert(!*prefix); return 0; }
int ink_state_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec) { (void)prefix; (void)fn; (void)ctx; (void)exec; return 0; }
bool ink_state_get_int(int h, int64_t *out) { (void)h; *out = 1; return true; }
int ink_state_get_str(int h, char *out, size_t n) { (void)h; snprintf(out, n, "locked"); return 6; }
static void path(char *out, size_t n, const char *p) { int r = snprintf(out, n, "%s%s", root, p); assert(r > 0 && (size_t)r < n); }
bool hal_fs_card_present(void) { return true; }
int hal_fs_mkdir(const char *p) { char b[1400]; path(b, sizeof b, p); return mkdir(b, 0700); }
int hal_fs_exists(const char *p) { char b[1400]; path(b, sizeof b, p); struct stat s; return !stat(b, &s); }
hal_file_t *hal_fs_open(const char *p, int flags) {
    (void)flags; char b[1400]; path(b, sizeof b, p);
    FILE *f = fopen(b, "wb");
    if (!f) return NULL;
    hal_file_t *h = malloc(sizeof *h); assert(h); h->fp = f; return h;
}
ssize_t hal_fs_write(hal_file_t *f, const void *p, size_t n) { return (ssize_t)fwrite(p, 1, n, f->fp); }
int hal_fs_sync(hal_file_t *f) { return fflush(f->fp); }
int hal_fs_close(hal_file_t *f) { int r = fclose(f->fp); free(f); return r; }

static unsigned good_records(char *text, size_t cap) {
    uint8_t sector[INK_LOG_SECTOR]; size_t at = 0; unsigned count = 0;
    for (unsigned i = 0; i < INK_LOG_BYTES / INK_LOG_SECTOR; i++) {
        assert(hal_log_read(false, i * INK_LOG_SECTOR, sector, sizeof sector));
        ink_log_record_t *r = (void *)sector;
        if (!ink_log_valid(r)) continue;
        count++;
        size_t take = r->len < cap - at - 1 ? r->len : cap - at - 1;
        memcpy(text + at, r + 1, take); at += take;
    }
    text[at] = 0; return count;
}
static char text[INK_LOG_BYTES];
static void reboot(const char *why) {
    free(L.lock); free(L.sector); memset(&L, 0, sizeof L); reason = why;
    assert(!ink_log_init());
}
int main(int argc, char **argv) {
    assert(argc == 2);
    snprintf(root, sizeof root, "%s", argv[1]);
    g_sim.out_dir = root;
    assert(!ink_log_init()); assert(!ink_log_start());
    assert(L.tail->boot == 1);
    /* Level/module/time, threshold, and a non-blocking contended producer. */
    ink_log(INK_LOG_INFO, "fixture", "first");
    L.lock->held = true; ink_log(INK_LOG_INFO, "fixture", "must-drop"); L.lock->held = false;
    assert(L.dropped == 1);
    ink_log_poll(); assert(good_records(text, sizeof text) == 0);
    now = 30000000; ink_log_poll();
    assert(good_records(text, sizeof text) && strstr(text, "utc=unknown:0 ms=0 boot=1 INFO fixture first"));
    assert(!strstr(text, "must-drop"));
    uint32_t seq = L.seq;
    for (int i = 0; i < 40; i++) ink_log(INK_LOG_INFO, "fixture", "%0240d", i);
    ink_log_poll(); assert(L.seq > seq);
    held = true; seq = L.seq;
    ink_log(INK_LOG_ERROR, "fixture", "error-now"); ink_log_poll(); assert(L.seq == seq);
    assert(!ink_log_flush()); held = false; ink_log_poll();
    assert(good_records(text, sizeof text) && strstr(text, "ERROR fixture error-now"));
    /* Wrap the actual 512 KiB file, with real NOR clear-bit semantics. */
    for (int i = 0; i < 140; i++) { ink_log(INK_LOG_INFO, "wrap", "entry=%d", i); assert(ink_log_flush()); }
    assert(good_records(text, sizeof text) == 127);
    assert(strstr(text, "entry=139") && !strstr(text, "fixture first"));
    /* Torn record has a newer sequence but incomplete payload/CRC. */
    uint32_t before = L.seq;
    uint8_t torn[INK_LOG_SECTOR]; memset(torn, 255, sizeof torn);
    ink_log_record_t *r = (void *)torn;
    *r = (ink_log_record_t){.magic = INK_LOG_MAGIC, .seq = before + 100, .boot = L.tail->boot, .len = 100, .crc = 0};
    assert(hal_log_erase(L.page * INK_LOG_SECTOR));
    assert(hal_log_program(L.page * INK_LOG_SECTOR, torn, sizeof *r + 2));
    assert(!ink_log_valid(torn));
    reboot("restart"); assert(L.seq == before); assert(L.tail->boot == 2);
    /* Pending tail survives a watchdog reset, without having reached the disk. */
    ink_log(INK_LOG_INFO, "tail", "CRASH_TAIL_SENTINEL");
    assert(!strstr(text, "CRASH_TAIL_SENTINEL"));
    reboot("watchdog"); assert(L.tail->boot == 3); assert(ink_log_flush());
    assert(good_records(text, sizeof text) && strstr(text, "CRASH_TAIL_SENTINEL") && strstr(text, "上次开机最后的日志"));
    /* Payloads and the supplied sentence are deliberately private, including embedded names/secrets. */
    ink_args_t args = {0}; snprintf(args.text, sizeof args.text, "PRIVATE_TEST_SECRET wifi=example-network42 phone=15500000042 PIN=1234 member=FAKE_NAME SMS=FAKE_CHAT");
    for (int i = 0; i < 60; i++) { char e[48]; snprintf(e, sizeof e, "test.event.%d", i); event(NULL, e, &args); }
    ink_log_page("reader", "Reader");
    ink_call_t call = {.args = args}; report(&call);
    assert(call.rc.status == INK_RC_DONE && strstr(call.rc.say, "已记录问题 #"));
    assert(good_records(text, sizeof text) && strstr(text, "问题 #300001 page=Reader"));
    assert(strstr(text, "recent ms=") && strstr(text, "test.event.59"));
    assert(!strstr(text, "PRIVATE_TEST_SECRET") && !strstr(text, "FAKE_CHAT") && !strstr(text, "FAKE_NAME"));
    now = 300000000; ink_log_poll(); assert(ink_log_flush());
    assert(good_records(text, sizeof text) && strstr(text, "internal=1000/900/800") && strstr(text, "tasks=1"));
    char folder[1400]; path(folder, sizeof folder, "/content"); assert(!mkdir(folder, 0700));
    export_logs(&call); assert(call.rc.status == INK_RC_DONE);
    free(L.lock); free(L.sector);
    puts("PASS: batch, nonblocking, wrap, torn CRC, watchdog tail, immediate problem, privacy, health, export");
    return 0;
}
