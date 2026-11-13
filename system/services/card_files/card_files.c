/* Card files (D-138, docs/system/card-files.md): the files apps need on the card, from the list built into the firmware.
 * Checked at boot, card in, USB disk over, computer plugged in and on card.files_check; what is missing comes from the
 * install tool on the USB console (INKCF1) into <path>.part, installed only when its whole sha256 matches the list.
 * The computer never names a path. The fetch runs in a short-lived task that touches only the card. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/hal_usb.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/card_files.h"
#include "ink/sys/crc.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/log.h"
#include "ink/sys/power.h"
#include "ink/sys/sha256.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/card_files_manifest.h"

enum {
    INK_CARD_CHUNK = 2048, INK_CARD_LINE_MAX = 3000, /* base64 of 2048 bytes is 2732, plus the fields */
    CF_SLICE_MS = 200,                               /* reads in slices: a card release is seen this fast */
    CF_REPLY_MS = 2000,                              /* a GET, DONE or BYE waits this long for its answer */
    CF_TRIES = 4,                                    /* the 4th failure of the same chunk ends the session */
    CF_HELLO_EVERY_MS = 2000, CF_HELLO_FOR_MS = 30000, /* HELLO again every 2 s for 30 s (nobody may read yet) */
    CF_STEP = 256 * 1024, CF_BUF = 16 * 1024,        /* hashed per executor turn, read per call */
    CF_RESUME = 64 * 1024,                           /* a .part resumes at a multiple of this; synced as often */
    CF_QUIET_US = 500000,                            /* the screen idle and no input this long before a read */
};
_Static_assert(INK_CARD_FILES_COUNT <= 8, "card files: at most 8 rows (tools/card_files.py MAX_ENTRIES)");
enum { E_UNKNOWN, E_OK, E_MISSING, E_WRONG, E_SKIP, NOT_OK = -1 };
enum { F_OK, F_NO, F_IO };        /* a fetch */
enum { D_OK, D_BAD, D_IO, D_NO, D_KEPT }; /* a file's end, as DONE says it (KEPT: whole, not installed yet) */
static const char *const DONE_WORD[] = {"ok", "bad", "io", "no", "io"};
static const char *const STATE_WORD[] = {"ready", "checking", "missing", "copying", "failed", "no_card"};

/* Where chunks come from: the install tool on the USB console now, the hub later (docs/system/card-files.md §7). */
typedef struct {
    const char *name;
    int (*begin)(void);                                              /* 0: someone answers */
    int (*fetch)(int idx, uint32_t offset, uint8_t *buf, uint32_t len); /* one checked chunk: F_OK, or why not */
    void (*done)(int idx, int how);                                  /* one file ended (D_*) */
    void (*end)(int ok, int failed);
} ink_card_source_t;

static struct {
    int h_files, h_progress, h_card, h_mode, h_usb, i, step, need, len, rx_n, rx_pos; /* i, step, off: the check's cursor */
    uint8_t st[INK_CARD_FILES_COUNT];
    bool valid[INK_CARD_FILES_COUNT], busy, again, prev_card, prev_normal, prev_usb, swap_from_task, overflow;
    volatile int phase, pct;
    volatile bool stop; /* the card is about to be lent: let go */
    int64_t off;
    ink_sha256_t sha;
    uint64_t t0, done_bytes, total_bytes;
    uint32_t swap, let_go; /* let_go: those files' apps hold none of them */
    unsigned concluded; /* checks and sessions that reached a conclusion (ink_card_files_checks) */
    uint8_t *buf, *chunk; /* PSRAM, with line */
    char *line, rx[512], out[200];
    volatile bool swapped; /* the task's install is done (no semaphore: internal RAM) */
    ink_exec_job_t kick_job, step_job, ui_job, swap_job, end_job;
    ink_svc_deadline_t quiet_at; /* the next read waits for a quiet screen */
} CF INK_PSRAM_BSS; /* internal RAM is short */

static bool path_ok(const char *p) {
    size_t n = strlen(p);
    if ((strncmp(p, "/content/fonts/", 15) && strncmp(p, "/content/.system/", 17)) || n + 6 > INK_FS_PATH_MAX) return false;
    if (strstr(p, "..") || strstr(p, "//") || (n >= 5 && !strcmp(p + n - 5, ".part")) || p[n - 1] == '/') return false;
    for (; *p; p++)
        if ((unsigned char)*p < 0x20 || *p == 0x7F || *p == '\\') return false;
    return true;
}

static void part_path(char *out, size_t cap, int i) { snprintf(out, cap, "%s.part", INK_CARD_FILES[i].path); }

static void publish(int phase, int pct) { /* the conclusion is written first: then what others see */
    CF.phase = phase, CF.pct = pct;
    if (phase != INK_CARD_CHECKING && phase != INK_CARD_COPYING) __atomic_add_fetch(&CF.concluded, 1, __ATOMIC_RELEASE);
    ink_state_set_int(CF.h_progress, pct);
    ink_state_set_str(CF.h_files, STATE_WORD[phase]);
}

static int count(int status) { /* listed files with this status (NOT_OK: not in place) */
    int n = 0;
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++) n += CF.valid[i] && (status == NOT_OK ? CF.st[i] != E_OK : CF.st[i] == status);
    return n;
}

/* Up to CF_STEP more bytes of path into CF.sha from CF.off: 1 when all `size` are in, 0 for more, -1 on an error. */
static int hash_some(const char *path, int64_t size) {
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f || (CF.off && hal_fs_seek(f, CF.off, SEEK_SET))) {
        if (f) hal_fs_close(f);
        return -1;
    }
    if (!CF.off) ink_sha256_init(&CF.sha);
    int64_t end = CF.off + CF_STEP < size ? CF.off + CF_STEP : size;
    while (CF.off < end) {
        ssize_t n = hal_fs_read(f, CF.buf, end - CF.off < CF_BUF ? (size_t)(end - CF.off) : CF_BUF);
        if (n <= 0) break;
        ink_sha256_update(&CF.sha, CF.buf, (size_t)n);
        CF.off += n;
    }
    hal_fs_close(f);
    return CF.off == size ? 1 : CF.off < end ? -1 : 0;
}

static bool hash_is(int i) {
    uint8_t d[32];
    ink_sha256_final(&CF.sha, d);
    return !memcmp(d, INK_CARD_FILES[i].sha256, 32);
}

static void check_finish(void);
static void swap_bg(void *arg) {
    (void)arg;
    bool changed = false;
    char part[INK_FS_PATH_MAX + 8];
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++) {
        if (!(CF.swap & 1u << i) || ink_fs_card_hold()) continue;
        const char *path = INK_CARD_FILES[i].path;
        part_path(part, sizeof part, i);
        /* the card's file system cannot remove a file someone has open: the old one goes only while its app lets go */
        bool gone = hal_fs_size(path) < 0 || ((CF.let_go & 1u << i) && !hal_fs_remove(path)); /* the old one */
        if (gone && !hal_fs_rename(part, path)) CF.st[i] = E_OK, changed = true;
        else ink_log(INK_LOG_WARN, "card", "card files: %s not replaced yet, its .part stays", path);
        ink_fs_card_release();
    }
    CF.swap = 0;
    if (changed) ink_fs_changed_outside();
    if (CF.swap_from_task) CF.swapped = true;
    else check_finish();
}

/* An entry's app is one id or several joined by commas ("reader,music": both draw with the reader's font).
 * The n-th of them into out; false past the last. */
static bool app_at(const char *list, int n, char *out, size_t cap) {
    for (; n > 0 && list; n--) if ((list = strchr(list, ','))) list++;
    if (!list) return false;
    size_t len = strcspn(list, ",");
    snprintf(out, cap, "%.*s", (int)(len < cap ? len : cap - 1), list);
    return true;
}
static bool wants(const char *list, const char *app) {
    char id[24];
    for (int n = 0; app_at(list, n, id, sizeof id); n++) if (!strcmp(id, app)) return true;
    return false;
}

static void swap_ui(void *arg) { /* app states and notices are the UI task's */
    (void)arg;
    CF.let_go = 0;
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++) {
        bool free = true; /* every app that wants it is stopped, or its page shows the notice */
        char app[24];
        for (int n = 0; free && app_at(INK_CARD_FILES[i].app, n, app, sizeof app); n++)
            free = ink_app_state_of(app) == INK_APP_STOPPED || ink_card_files_notice_snapshot(app, NULL, 0);
        if (free) CF.let_go |= 1u << i;
    }
    ink_exec_post(INK_EXEC_BG, &CF.swap_job, swap_bg, NULL);
}

static void swap_start(uint32_t mask, bool from_task) {
    CF.swap = mask, CF.swap_from_task = from_task;
    ink_exec_post(INK_EXEC_UI, &CF.ui_job, swap_ui, NULL);
}

static int usb_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int usb_line(const char *fmt, ...) {
    int n = snprintf(CF.out, sizeof CF.out, "INKCF1 ");
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(CF.out + n, sizeof CF.out - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof CF.out - 1) return -1;
    CF.out[n++] = '\n';
    return hal_usb_console_write(CF.out, (size_t)n);
}

/* The next protocol line within ms into CF.line: its length, 0 on a timeout, -1 when the link went or the card is wanted.
 * Other lines are dropped, and so is a line longer than INK_CARD_LINE_MAX. */
static int usb_recv(uint32_t ms) {
    uint64_t end = osal_now_us() + (uint64_t)ms * 1000;
    for (;;) {
        while (CF.rx_pos < CF.rx_n) {
            char c = CF.rx[CF.rx_pos++];
            if (c != '\n') {
                if (CF.len < INK_CARD_LINE_MAX) CF.line[CF.len++] = c;
                else CF.overflow = true;
                continue;
            }
            if (CF.len && CF.line[CF.len - 1] == '\r') CF.len--;
            CF.line[CF.len] = 0;
            int n = CF.len;
            bool keep = !CF.overflow && !strncmp(CF.line, "INKCF1 ", 7);
            CF.len = 0, CF.overflow = false;
            if (keep) return n;
        }
        uint64_t now = osal_now_us();
        if (CF.stop) return -1;
        if (now >= end) return 0;
        uint64_t left = (end - now + 999) / 1000;
        int n = hal_usb_console_read(CF.rx, sizeof CF.rx, left < CF_SLICE_MS ? (uint32_t)left : CF_SLICE_MS);
        if (n < 0) return -1;
        CF.rx_n = n, CF.rx_pos = 0;
    }
}

static int b64_decode(const char *s, uint8_t *out, size_t cap) {
    uint32_t acc = 0;
    int bits = 0, chars = 0, pad = 0;
    size_t n = 0;
    for (; *s; s++, chars++) {
        int c = *s, v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
                    : c == '+' ? 62 : c == '/' ? 63 : c == '=' ? 64 : -1;
        if (v < 0 || (pad && v != 64)) return -1;
        if (v == 64) { pad++; continue; }
        acc = acc << 6 | (uint32_t)v, bits += 6;
        if (bits >= 8 && n >= cap) return -1;
        if (bits >= 8) out[n++] = (uint8_t)(acc >> (bits -= 8));
    }
    return chars % 4 || pad > 2 ? -1 : (int)n;
}

/* CF.line against "GET idx offset len": 0 the chunk is in buf, 1 the tool has no such chunk, -1 a wrong DATA line,
 * -2 not an answer to a GET (dropped). */
static int usb_reply(int idx, uint32_t off, uint8_t *buf, uint32_t len) {
    const char *l = CF.line + 7;
    char *e;
    if (!strncmp(l, "NO ", 3)) return strtol(l + 3, &e, 10) == idx && e != l + 3 && *e == ' ' ? 1 : -2;
    if (strncmp(l, "DATA ", 5)) return -2;
    unsigned long i, o, n, crc;
    int pos = 0;
    if (sscanf(l + 5, "%lu %lu %lu %8lx %n", &i, &o, &n, &crc, &pos) != 4 || !pos) return -1;
    if (i != (unsigned long)idx || o != off || n != len) return -1;
    return b64_decode(l + 5 + pos, buf, len) == (int)len && ink_crc32(0, buf, len) == crc ? 0 : -1;
}

static int usb_begin(void) { /* only a SERVE with our digest, after our HELLO, makes the tool count as there */
    if (hal_usb_console_open()) return -1;
    CF.len = CF.rx_n = CF.rx_pos = 0, CF.overflow = false;
    ink_power_hold(INK_NEED_AWAKE);
    for (uint64_t start = osal_now_us(); osal_now_us() - start < CF_HELLO_FOR_MS * 1000ull;) {
        usb_line("HELLO %s %d", INK_CARD_FILES_DIGEST, CF.need);
        for (uint64_t sent = osal_now_us(), waited; (waited = (osal_now_us() - sent) / 1000) < CF_HELLO_EVERY_MS;) {
            int n = usb_recv((uint32_t)(CF_HELLO_EVERY_MS - waited));
            if (n < 0 || (n > 0 && CF.need == 0)) goto none; /* need 0: any answer ends it */
            if (n > 0 && !strncmp(CF.line, "INKCF1 SERVE ", 13) && !strcmp(CF.line + 13, INK_CARD_FILES_DIGEST)) return 0;
            if (n > 0 && !strncmp(CF.line, "INKCF1 NOBUNDLE ", 16)) goto none;
        }
    }
none:
    hal_usb_console_close();
    ink_power_release(INK_NEED_AWAKE);
    return -1;
}

static int usb_fetch(int idx, uint32_t off, uint8_t *buf, uint32_t len) {
    char sha[65];
    for (int k = 0; k < 32; k++) snprintf(sha + 2 * k, 3, "%02x", INK_CARD_FILES[idx].sha256[k]);
    for (int t = 0; t < CF_TRIES; t++) {
        if (t) ink_probe_count("card_files.chunk_retry");
        if (usb_line("GET %d %s %lu %lu", idx, sha, (unsigned long)off, (unsigned long)len)) return F_IO;
        for (uint64_t end = osal_now_us() + CF_REPLY_MS * 1000ull, now; (now = osal_now_us()) < end;) {
            int n = usb_recv((uint32_t)((end - now + 999) / 1000));
            if (n < 0) return F_IO;
            if (n == 0) break;
            int r = usb_reply(idx, off, buf, len);
            if (r == 0) ink_probe_count("card_files.chunk");
            if (r >= 0) return r ? F_NO : F_OK;
            if (r == -1) { ink_probe_count("card_files.rejected"); break; }
        }
    }
    return F_IO;
}

static void usb_done(int idx, int how) {
    if (!usb_line("DONE %d %s", idx, DONE_WORD[how])) usb_recv(CF_REPLY_MS); /* its ACK */
}

static void usb_end(int ok, int failed) {
    if (!usb_line("BYE %d %d", ok, failed)) usb_recv(CF_REPLY_MS);
    hal_usb_console_close();
    ink_power_release(INK_NEED_AWAKE);
}

static const ink_card_source_t USB_SOURCE = {"usb", usb_begin, usb_fetch, usb_done, usb_end};

static void make_parents(const char *path) { /* below /content/ */
    char dir[INK_FS_PATH_MAX + 8];
    snprintf(dir, sizeof dir, "%s", path);
    for (char *p = dir + 9; (p = strchr(p, '/')) != NULL; *p++ = '/') *p = 0, hal_fs_mkdir(dir);
}

/* Write <path>.part from its source, resuming a .part that is there; check the whole file; install it (D_*). */
static int fetch_file(const ink_card_source_t *src, int i) {
    const ink_card_file_t *e = &INK_CARD_FILES[i];
    char part[INK_FS_PATH_MAX + 8];
    part_path(part, sizeof part, i);
    for (int again = 0; again < 2; again++) {
        if (ink_fs_card_hold()) return D_IO;
        int64_t have = hal_fs_size(part);
        uint32_t start = have < 0 || have > e->size || again ? 0 : (uint32_t)(have / CF_RESUME * CF_RESUME);
        if (!start) hal_fs_remove(part);
        hal_fs_volume_info_t vi;
        if (hal_fs_volume_info("/content", &vi) || vi.free_bytes < e->size - start) {
            ink_log(INK_LOG_WARN, "card", "card files: no_space for %s", e->path);
            ink_fs_card_release();
            return D_IO;
        }
        make_parents(part);
        hal_file_t *f = hal_fs_open(part, HAL_O_WRITE | HAL_O_CREATE); /* no truncation: a resume keeps what is there */
        int r = f && !hal_fs_seek(f, start, SEEK_SET) ? F_OK : F_IO;
        CF.done_bytes += start;
        for (uint32_t off = start; r == F_OK && off < e->size;) {
            uint32_t len = e->size - off < INK_CARD_CHUNK ? e->size - off : INK_CARD_CHUNK;
            r = src->fetch(i, off, CF.chunk, len);
            if (r == F_OK && hal_fs_write(f, CF.chunk, len) != (ssize_t)len) r = F_IO;
            if (r != F_OK) break;
            off += len, CF.done_bytes += len;
            if (off % CF_RESUME == 0) hal_fs_sync(f);
            publish(INK_CARD_COPYING, (int)(CF.done_bytes * 100 / (CF.total_bytes ? CF.total_bytes : 1)));
        }
        if (f && hal_fs_close(f) && r == F_OK) r = F_IO;
        ink_fs_card_release();
        if (r != F_OK) return r == F_NO ? D_NO : D_IO;
        if (ink_fs_card_hold()) return D_IO;
        int got;
        for (CF.off = 0; (got = hash_some(part, e->size)) == 0 && !CF.stop;) {}
        bool same = got == 1 && hash_is(i);
        if (!same && got) hal_fs_remove(part);
        ink_fs_card_release();
        if (!got) return D_IO; /* stopped for the card's loan: the .part stays */
        if (same) {
            ink_probe_count("card_files.written");
            CF.swapped = false;
            swap_start(1u << i, true);
            while (!CF.swapped) osal_task_delay_us(10000);
            return CF.st[i] == E_OK ? D_OK : CF.stop ? D_IO : D_KEPT; /* installed, or said so: the .part stays */
        }
        CF.done_bytes -= e->size;
        ink_probe_count(start ? "card_files.restart" : "card_files.bad_hash"); /* a resumed one may be an older list's */
        if (!start) return D_BAD;
    }
    return D_BAD;
}

static void kick_bg(void *arg);
static void session_end_bg(void *arg) { /* also the check's end */
    (void)arg, CF.busy = false;
    if (CF.again) kick_bg(NULL);
}

static void session(void *arg) {
    const ink_card_source_t *src = arg;
    int ok = 0, failed = 0;
    if (src->begin() == 0) {
        ink_probe_count("card_files.session");
        CF.done_bytes = CF.total_bytes = 0;
        for (int i = 0; i < INK_CARD_FILES_COUNT; i++)
            if (CF.valid[i] && CF.st[i] != E_OK) CF.total_bytes += INK_CARD_FILES[i].size;
        publish(INK_CARD_COPYING, 0);
        for (int i = 0; i < INK_CARD_FILES_COUNT; i++) {
            if (!CF.valid[i] || CF.st[i] == E_OK) continue;
            int how = fetch_file(src, i);
            src->done(i, how);
            how == D_OK ? ok++ : failed++;
            if (how == D_IO) break; /* the link or the card went: the rest waits for the next time */
        }
        src->end(ok, failed);
        ink_log(INK_LOG_INFO, "card", "card files: session %s ok=%d failed=%d", src->name, ok, failed);
        bool all = count(NOT_OK) == 0;
        publish(all ? INK_CARD_READY : INK_CARD_FAILED, all ? 100 : CF.pct);
    }
    ink_exec_post(INK_EXEC_BG, &CF.end_job, session_end_bg, NULL);
}

static void check_finish(void) {
    CF.need = count(NOT_OK);
    publish(CF.need ? INK_CARD_MISSING : INK_CARD_READY, CF.need ? 0 : 100);
    osal_task_cfg_t cfg = {.name = "cardfiles", .fn = session, .arg = (void *)&USB_SOURCE, .stack_bytes = 6144,
                           .priority = OSAL_PRIO_LOW, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CF.stop = false;
    if (hal_usb_host_connected() && osal_task_create(&cfg, NULL) == OSAL_OK) {
        CF.prev_usb = true; /* this computer is seen: its connected event, if it comes late, is not news */
        return;
    }
    session_end_bg(NULL);
}

static void check_step(void *arg);
static void step_again(void *ctx) { (void)ctx; ink_exec_post(INK_EXEC_BG, &CF.step_job, check_step, NULL); }
static void check_step(void *arg) {
    (void)arg;
    /* Reading a whole file waits while the screen draws or someone is using the device (as background storage does):
     * the shared executor and the card stay free for what they are doing. */
    if (ink_display_busy() > 0 || osal_now_us() < ink_power_last_activity() + CF_QUIET_US) {
        ink_svc_at(&CF.quiet_at, osal_now_us() + CF_QUIET_US, step_again, NULL);
        return;
    }
    int r = ink_fs_card_hold();
    if (r) { /* lent to a computer: as it was, until it comes back */
        if (r != INK_FS_ERR_USB) {
            memset(CF.st, E_UNKNOWN, sizeof CF.st);
            publish(INK_CARD_NO_CARD, 0);
        }
        session_end_bg(NULL);
        return;
    }
    if (!CF.i && !CF.step && !CF.off) publish(INK_CARD_CHECKING, 0);
    char part[INK_FS_PATH_MAX + 8];
    while (CF.i < INK_CARD_FILES_COUNT) {
        int i = CF.i, got = -2;
        const ink_card_file_t *e = &INK_CARD_FILES[i];
        part_path(part, sizeof part, i);
        const char *path = CF.step ? part : e->path;
        if (CF.valid[i] && hal_fs_size(path) == e->size) got = hash_some(path, e->size);
        if (got == 0) { /* more next turn: others get the executor in between */
            ink_fs_card_release();
            ink_exec_post(INK_EXEC_BG, &CF.step_job, check_step, NULL);
            return;
        }
        bool same = got == 1 && hash_is(i);
        CF.off = 0;
        if (!CF.step) CF.st[i] = !CF.valid[i] ? E_SKIP : same ? E_OK : got == -2 ? E_MISSING : E_WRONG;
        else if (same) CF.swap |= 1u << i; /* a whole .part: installed here, no computer needed */
        if (!CF.step && CF.st[i] != E_OK && CF.st[i] != E_SKIP) CF.step = 1;
        else CF.step = 0, CF.i++;
    }
    ink_fs_card_release();
    ink_log(INK_LOG_INFO, "card", "card files: need=%d ok=%d failed=%d check_ms=%llu", count(NOT_OK), count(E_OK), count(E_WRONG),
            (unsigned long long)((osal_now_us() - CF.t0) / 1000));
    if (CF.swap) swap_start(CF.swap, false);
    else check_finish();
}

static void kick_bg(void *arg) {
    (void)arg;
    CF.again = CF.busy;
    if (CF.busy) return;
    CF.busy = true, CF.i = CF.step = 0, CF.off = 0, CF.swap = 0, CF.t0 = osal_now_us();
    ink_probe_count("card_files.check");
    check_step(NULL);
}

static void kick(void) { ink_exec_post(INK_EXEC_BG, &CF.kick_job, kick_bg, NULL); }

static bool storage_rose(void) { /* a card in, the USB disk over, a computer plugged in: since the last look */
    int64_t card = 0, usb = 0;
    char mode[8] = "";
    ink_state_get_int(CF.h_card, &card);
    ink_state_get_int(CF.h_usb, &usb);
    ink_state_get_str(CF.h_mode, mode, sizeof mode);
    bool normal = !strcmp(mode, "normal"), rose = (card && !CF.prev_card) || (normal && !CF.prev_normal) || (usb && !CF.prev_usb);
    CF.prev_card = card, CF.prev_normal = normal, CF.prev_usb = usb;
    return rose;
}

static void on_storage(void *ctx, int h, const char *id) { /* exec.bg */
    (void)ctx, (void)h, (void)id;
    if (storage_rose()) kick_bg(NULL);
}

static void on_release(void *ctx) { (void)ctx; CF.stop = true; }

static void cap_check(ink_call_t *c) {
    kick();
    snprintf(c->rc.detail, sizeof c->rc.detail, "card.files=%s", STATE_WORD[CF.phase]);
}

static const ink_cap_t CAPS[] = {
    {.id = "card.files_check", .title = "再核对一次存储卡上系统要用的文件", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = cap_check},
};
static const ink_state_def_t ST_FILES = {.id = "card.files", .type = INK_ST_STR,
    .title = "存储卡上系统要用的文件（ready 齐了，checking，missing 缺，copying 正从电脑放，failed 没放成，no_card）"};
static const ink_state_def_t ST_PROGRESS = {.id = "card.files_progress", .type = INK_ST_INT, .title = "从电脑放文件的进度（0-100）", .min_interval_ms = 2000};

unsigned ink_card_files_checks(void) { return __atomic_load_n(&CF.concluded, __ATOMIC_ACQUIRE); }

ink_card_state_t ink_card_files_state(const char *app, int *percent) {
    bool mine = false, all = true;
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++)
        if (CF.valid[i] && wants(INK_CARD_FILES[i].app, app)) mine = true, all = all && CF.st[i] == E_OK;
    if (percent) *percent = CF.pct;
    return !mine || all ? INK_CARD_READY : CF.phase == INK_CARD_READY ? INK_CARD_CHECKING : (ink_card_state_t)CF.phase;
}

int ink_card_files_init(void) {
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++)
        if (!(CF.valid[i] = path_ok(INK_CARD_FILES[i].path)))
            ink_log(INK_LOG_WARN, "card", "card files: entry %d is not a card file path, ignored", i);
    CF.h_files = ink_state_define(&ST_FILES);
    CF.h_progress = ink_state_define(&ST_PROGRESS);
    CF.buf = osal_malloc(CF_BUF + INK_CARD_CHUNK + INK_CARD_LINE_MAX + 1, OSAL_MEM_EXTERNAL);
    if (CF.h_files < 0 || CF.h_progress < 0 || !CF.buf) return -1;
    CF.chunk = CF.buf + CF_BUF, CF.line = (char *)CF.chunk + INK_CARD_CHUNK;
    publish(INK_CARD_CHECKING, 0);
    CF.h_card = ink_state_find("storage.card"); /* the file service's, defined before this */
    CF.h_usb = ink_state_find("usb.connected");
    CF.h_mode = ink_state_find("storage.mode");
    storage_rose(); /* as it is now: the boot check covers it */
    if (ink_fs_on_release(on_release, NULL) || ink_state_watch("storage.card", on_storage, NULL, INK_EXEC_BG) ||
        ink_state_watch("storage.mode", on_storage, NULL, INK_EXEC_BG) || ink_state_watch("usb.connected", on_storage, NULL, INK_EXEC_BG))
        return -1;
    kick(); /* the boot check */
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS);
}
