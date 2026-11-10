#include <string.h>
#include "ink/hal_log.h"
#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/sys/audio.h"
#include "ink/sys/media.h"
#include "ink/sys/cap.h"
#include "ink/sys/log.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#define EVENTS 50
static struct {
    osal_mutex_t *lock;
    osal_timer_t *timer;
    hal_log_tail_t *tail;
    uint8_t *sector;
    uint32_t seq, page, pending, dropped, issue, events;
    uint64_t written_at, health_at, refresh_us, refresh_max;
    uint32_t refreshes, inputs;
    bool force, ready, prepared;
    char page_name[48];
    char recent[EVENTS][80];
} L INK_PSRAM_BSS;
_Static_assert(sizeof L.recent / sizeof L.recent[0] <= EVENTS, "event rows");
static int format(char *out, size_t n, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    int r = hal_log_format(out, n, fmt, a);
    va_end(a);
    return r;
}
void ink_log(ink_log_level_t level, const char *module, const char *fmt, ...) {
    if (!L.lock || (unsigned)level > INK_LOG_ERROR) return;
    char msg[256], line[384];
    va_list a;
    va_start(a, fmt);
    hal_log_format(msg, sizeof msg, fmt, a);
    va_end(a);
    int64_t wall = osal_wall_us();
    format(line, sizeof line, "utc=%s%lld ms=%llu boot=%u %s %.32s %.255s\n", wall ? "" : "unknown:",
           (long long)(wall / 1000000), (unsigned long long)(osal_now_us() / 1000), L.tail->boot,
           (const char *const[]){"DEBUG", "INFO", "WARN", "ERROR"}[level], module, msg);
    if (osal_mutex_lock(L.lock, OSAL_NO_WAIT) != OSAL_OK) { __atomic_add_fetch(&L.dropped, 1, __ATOMIC_RELAXED); return; }
    for (size_t i = 0; line[i]; i++) {
        L.tail->data[L.tail->head++ % INK_LOG_TAIL] = line[i];
        if (L.tail->used < INK_LOG_TAIL) L.tail->used++;
        if (L.pending < INK_LOG_TAIL) L.pending++;
        else __atomic_add_fetch(&L.dropped, 1, __ATOMIC_RELAXED);
    }
    if (level == INK_LOG_ERROR) L.force = true;
    bool wake = L.force || L.pending >= 8192;
    osal_mutex_unlock(L.lock);
    if (wake) ink_storage_log_ready();
}
bool ink_log_from_hal(const char *line) { if (!L.lock) return false; ink_log(INK_LOG_WARN, "hal", "%s", line); return true; }
void ink_log_display(uint64_t us, bool ok) {
    if (!L.lock) return;
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
    L.refreshes++; L.refresh_us += us;
    if (us > L.refresh_max) L.refresh_max = us;
    osal_mutex_unlock(L.lock);
    ink_log(ok ? INK_LOG_DEBUG : INK_LOG_ERROR, "display", "refresh_us=%llu ok=%d", (unsigned long long)us, ok);
}
void ink_log_input(void) { __atomic_add_fetch(&L.inputs, 1, __ATOMIC_RELAXED); }
void ink_log_page(const char *app, const char *page) {
    if (!L.lock) return;
    char old[48];
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
    memcpy(old, L.page_name, sizeof old);
    format(L.page_name, sizeof L.page_name, "%.47s", page);
    osal_mutex_unlock(L.lock);
    if (old[0]) ink_log(INK_LOG_INFO, "page", "close %.47s", old);
    ink_log(INK_LOG_INFO, "page", "open %.47s app=%.47s", page, app ? app : "system");
}
void ink_log_health(void) {
    osal_mem_stats_t in, ex;
    hal_battery_t b;
    osal_mem_stats(OSAL_MEM_INTERNAL, &in); osal_mem_stats(OSAL_MEM_EXTERNAL, &ex);
    hal_power_battery(&b);
    uint32_t count; uint64_t total, longest;
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); count = L.refreshes; total = L.refresh_us; longest = L.refresh_max; osal_mutex_unlock(L.lock);
    char tasks[224]; hal_log_tasks(tasks, sizeof tasks);
    ink_log(INK_LOG_INFO, "health", "%s", tasks);
    ink_log(INK_LOG_INFO, "health", "internal=%zu/%zu/%zu psram=%zu/%zu/%zu battery=%d charging=%d mv=%d",
            in.free, in.min_free, in.largest_free, ex.free, ex.min_free, ex.largest_free, b.percent, b.charging, b.millivolts);
    ink_log(INK_LOG_INFO, "health", "refreshes=%u avg_us=%llu max_us=%llu inputs=%u dropped=%u", (unsigned)count,
            (unsigned long long)(count ? total / count : 0), (unsigned long long)longest,
            (unsigned)__atomic_load_n(&L.inputs, __ATOMIC_RELAXED), (unsigned)__atomic_load_n(&L.dropped, __ATOMIC_RELAXED));
}
static bool writable(void) { return !ink_storage_writes_held() && !ink_audio_capture_busy() && !ink_audio_recording() && ink_audio_focus_top() < INK_AUDIO_PRIO_VOICE; }
static void flush(void *arg) {
    bool *ok = arg;
    *ok = false;
    if (!L.ready || !writable()) return;
    for (unsigned batch = 0; batch < (INK_LOG_TAIL / (INK_LOG_SECTOR - sizeof(ink_log_record_t))) + 1; batch++) {
        osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
        uint32_t pending = L.pending, head = L.tail->head;
        size_t n = pending < INK_LOG_SECTOR - sizeof(ink_log_record_t) ? pending : INK_LOG_SECTOR - sizeof(ink_log_record_t);
        ink_log_record_t *r = (void *)L.sector;
        *r = (ink_log_record_t){.magic = INK_LOG_MAGIC, .seq = L.seq + 1, .boot = L.tail->boot, .len = (uint32_t)n};
        for (size_t i = 0; i < n; i++) L.sector[sizeof *r + i] = (uint8_t)L.tail->data[(head + INK_LOG_TAIL - pending + i) % INK_LOG_TAIL];
        osal_mutex_unlock(L.lock);
        if (!n) { *ok = true; break; }
        r->crc = ~ink_log_crc(r + 1, n, ink_log_crc(r, offsetof(ink_log_record_t, crc), ~0u));
        if ((!L.prepared && !hal_log_erase(L.page * INK_LOG_SECTOR)) || !hal_log_program(L.page * INK_LOG_SECTOR, r, sizeof *r + n)) { L.prepared = false; return; }
        L.seq++; L.page = (L.page + 1) % (INK_LOG_BYTES / INK_LOG_SECTOR);
        L.prepared = hal_log_erase(L.page * INK_LOG_SECTOR);
        osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
        /* Bytes overwritten during the write were already lost; never subtract new bytes. */
        uint32_t appended = L.tail->head - head;
        L.pending = pending - (uint32_t)n + appended;
        if (L.pending > INK_LOG_TAIL) L.pending = INK_LOG_TAIL;
        osal_mutex_unlock(L.lock);
    }
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
    *ok = !L.pending;
    if (*ok) { L.force = false; L.written_at = osal_now_us(); }
    osal_mutex_unlock(L.lock);
}
bool ink_log_flush(void) {
    if (!L.lock) return false;
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); L.force = true; osal_mutex_unlock(L.lock);
    bool ok;
    ink_storage_run(flush, &ok, true);
    return ok;
}
void ink_log_poll(bool quiet) {
    if (!L.ready) return;
    uint64_t now = osal_now_us();
    if (now - L.health_at >= 300000000ull) { L.health_at = now; ink_log_health(); }
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER);
    bool urgent = L.force, due = L.pending && (urgent || L.pending >= 8192 || now - L.written_at >= 30000000ull);
    osal_mutex_unlock(L.lock);
    if (due && (urgent || quiet) && writable()) { bool ok; ink_storage_run(flush, &ok, true); }
}
static void recover(void *unused) {
    (void)unused;
    uint32_t boot = 0;
    for (uint32_t i = 0; i < INK_LOG_BYTES / INK_LOG_SECTOR; i++) {
        if (!hal_log_read(false, i * INK_LOG_SECTOR, L.sector, INK_LOG_SECTOR)) return;
        ink_log_record_t *r = (void *)L.sector;
        if (ink_log_valid(r) && r->seq > L.seq) { L.seq = r->seq; L.page = (i + 1) % (INK_LOG_BYTES / INK_LOG_SECTOR); boot = r->boot; }
    }
    bool valid = L.tail->magic == INK_LOG_MAGIC && L.tail->used <= INK_LOG_TAIL;
    const char *why = hal_device_reset_reason();
    bool crash = strstr(why, "panic") || strstr(why, "watchdog") || strstr(why, "wdt");
    if (valid && L.tail->boot > boot) boot = L.tail->boot;
    L.pending = valid && crash ? L.tail->used : 0;
    if (!L.pending) L.tail->head = L.tail->used = 0;
    L.tail->magic = INK_LOG_MAGIC; L.tail->boot = boot + 1; L.ready = true;
    if (L.pending) { ink_log(INK_LOG_WARN, "boot", "上次开机最后的日志：以上为 RTC 保留尾巴"); L.force = true; }
    char info[256];
    hal_log_boot_info(info, sizeof info);
    ink_log(INK_LOG_INFO, "boot", "reset=%.24s %.220s", why, info);
}
int ink_log_init(void) {
    L.tail = hal_log_tail(); L.lock = osal_mutex_create();
    L.sector = osal_malloc(INK_LOG_SECTOR, OSAL_MEM_EXTERNAL);
    if (!L.tail || !L.lock || !L.sector) return -1;
    ink_storage_run(recover, NULL, true);
    return L.ready ? 0 : -1;
}
static void event(void *ctx, const char *id, const ink_args_t *payload) {
    (void)ctx; (void)payload;
    if (osal_mutex_lock(L.lock, OSAL_NO_WAIT) != OSAL_OK) return;
    format(L.recent[L.events++ % EVENTS], sizeof L.recent[0], "ms=%llu %.48s", (unsigned long long)(osal_now_us() / 1000), id);
    osal_mutex_unlock(L.lock);
    ink_log(INK_LOG_INFO, "event", "%.48s", id);
}
void ink_log_state(int h, const char *id) {
    if (!L.lock) return;
    if (!strcmp(id, "power.state") || !strcmp(id, "net.type") || !strcmp(id, "member.current")) {
        char value[24]; ink_state_get_str(h, value, sizeof value);
        ink_log(INK_LOG_INFO, "state", "%.32s=%.23s", id, value);
        if (!strcmp(value, "locked")) { ink_log_health(); osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); L.force = true; osal_mutex_unlock(L.lock); }
    } else if (!strcmp(id, "battery.charging") || !strcmp(id, "battery.low") || !strcmp(id, "confirm.pending")) {
        int64_t value = 0; ink_state_get_int(h, &value);
        ink_log(INK_LOG_INFO, "state", "%.32s=%lld", id, (long long)value);
        if (!strcmp(id, "battery.low") && value) ink_log_health();
    }
}
static void report(ink_call_t *c) {
    uint32_t n = L.tail->boot * 100000u + ++L.issue;
    char page[48];
    osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); memcpy(page, L.page_name, sizeof page); osal_mutex_unlock(L.lock);
    ink_log(INK_LOG_INFO, "problem", "问题 #%u page=%.47s description=[private]", (unsigned)n, page);
    /* Event metadata only: payloads, free text and screen snapshots may contain credentials. */
    uint32_t events; osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); events = L.events; osal_mutex_unlock(L.lock);
    for (uint32_t i = events > EVENTS ? events - EVENTS : 0; i < events; i++) {
        char e[80];
        osal_mutex_lock(L.lock, OSAL_WAIT_FOREVER); memcpy(e, L.recent[i % EVENTS], sizeof e); osal_mutex_unlock(L.lock);
        ink_log(INK_LOG_INFO, "recent", "%s", e);
    }
    ink_log_health();
    bool ok = ink_log_flush();
    c->rc.status = ok ? INK_RC_DONE : INK_RC_FAILED;
    format(c->rc.say, sizeof c->rc.say, ok ? "已记录问题 #%u" : "问题 #%u 尚未保存，请稍后重试", n);
    format(c->rc.object, sizeof c->rc.object, "problem:%u", n);
}
static void export_raw(void *arg) {
    bool *ok = arg; *ok = false;
    if (!writable() || !hal_fs_card_present()) return;
    char dir[112], path[144];
    format(dir, sizeof dir, "/content/ink-logs/%lld-b%u-%llu", (long long)(osal_wall_us() / 1000000), L.tail->boot, (unsigned long long)(osal_now_us() / 1000));
    if ((hal_fs_mkdir("/content/ink-logs") && hal_fs_exists("/content/ink-logs") != 1) || hal_fs_mkdir(dir)) return;
    for (int core = 0; core < 2; core++) {
        format(path, sizeof path, "%s/%s.bin", dir, core ? "coredump" : "logs");
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
        if (!f) return;
        bool good = true;
        for (uint32_t off = 0; off < (core ? 65536u : INK_LOG_BYTES); off += INK_LOG_SECTOR) {
            if (!hal_log_read(core != 0, off, L.sector, INK_LOG_SECTOR) || hal_fs_write(f, L.sector, INK_LOG_SECTOR) != INK_LOG_SECTOR) { good = false; break; }
        }
        if (hal_fs_sync(f)) good = false;
        if (hal_fs_close(f)) good = false;
        if (!good) return;
    }
    *ok = true;
}
static void export_logs(ink_call_t *c) {
    bool ok = ink_log_flush();
    if (ok) ink_storage_run(export_raw, &ok, true);
    c->rc.status = ok ? INK_RC_DONE : INK_RC_FAILED;
    format(c->rc.say, sizeof c->rc.say, "%s", ok ? "日志已导出到存储卡 ink-logs" : "日志未导出，请检查存储卡或稍后重试");
}
static const ink_cap_t CAPS[] = {
    {.id = "system.report_problem", .title = "记录问题", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG,
     .input = "{\"type\":\"object\",\"properties\":{\"description\":{\"type\":\"string\",\"maxLength\":120}},\"additionalProperties\":false}", .run = report},
    {.id = "system.export_logs", .title = "导出日志", .effect = INK_FX_LOCAL_WRITE, .exec = INK_EXEC_BG, .expose = INK_EXPOSE_OWNER, .run = export_logs}
};
static void tick(void *ctx) { (void)ctx; ink_storage_log_ready(); }
int ink_log_start(void) {
    if (!L.lock || !L.tail || !L.sector) return -1;
    ink_log_health();
    L.timer = osal_timer_create("log", tick, NULL);
    return !L.timer || osal_timer_start(L.timer, 30000000, 30000000) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_cap_subscribe("", event, NULL, INK_EXEC_DIRECT);
}
