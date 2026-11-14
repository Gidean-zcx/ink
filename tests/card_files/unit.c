/* The device side of the card-file protocol (auto-provision-1): card_files.c itself, built with a test list
 * (tests/card_files/manifest), the console and the clock faked. The card, states and executors are stubs. */
#include "../../system/services/card_files/card_files.c"

#include <assert.h>

static int fails;
#define EXPECT(c, ...) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* ---- the clock and the console */
static uint64_t now_us;
static char rxq[16384], tx[8192];
static size_t rxq_n, rxq_pos, tx_n;
static bool console_open, cable = true;
uint64_t osal_now_us(void) { return now_us; }
int hal_usb_console_open(void) { console_open = true; return 0; }
void hal_usb_console_close(void) { console_open = false; }
int hal_usb_console_write(const char *line, size_t n) {
    if (tx_n + n < sizeof tx) memcpy(tx + tx_n, line, n), tx_n += n;
    tx[tx_n] = 0;
    return 0;
}
int hal_usb_console_read(void *buf, size_t cap, uint32_t timeout_ms) {
    assert(timeout_ms <= CF_SLICE_MS && timeout_ms > 0);
    if (!cable) return -1;
    if (rxq_pos == rxq_n) { now_us += (uint64_t)timeout_ms * 1000; return 0; }
    size_t n = rxq_n - rxq_pos < cap ? rxq_n - rxq_pos : cap;
    if (n > 7) n = 7; /* in pieces, as a USB FIFO hands them over */
    memcpy(buf, rxq + rxq_pos, n);
    rxq_pos += n;
    return (int)n;
}
static void feed(const char *s) { size_t n = strlen(s); memcpy(rxq + rxq_n, s, n); rxq_n += n; }
static void reset_link(void) { rxq_n = rxq_pos = tx_n = 0; tx[0] = 0; CF.len = CF.rx_n = CF.rx_pos = 0; CF.overflow = false; CF.stop = false; }

/* ---- stubs: nothing here touches a card or another task */
bool hal_usb_host_connected(void) { return true; }
hal_file_t *hal_fs_open(const char *p, int f) { (void)p; (void)f; return NULL; }
ssize_t hal_fs_read(hal_file_t *f, void *b, size_t n) { (void)f; (void)b; (void)n; return -1; }
ssize_t hal_fs_write(hal_file_t *f, const void *b, size_t n) { (void)f; (void)b; (void)n; return -1; }
int hal_fs_seek(hal_file_t *f, int64_t o, int w) { (void)f; (void)o; (void)w; return -1; }
int hal_fs_sync(hal_file_t *f) { (void)f; return 0; }
int hal_fs_close(hal_file_t *f) { (void)f; return 0; }
int hal_fs_remove(const char *p) { (void)p; return -1; }
int hal_fs_rename(const char *a, const char *b) { (void)a; (void)b; return -1; }
int hal_fs_mkdir(const char *p) { (void)p; return 0; }
int64_t hal_fs_size(const char *p) { (void)p; return -1; }
int hal_fs_volume_info(const char *v, hal_fs_volume_info_t *o) { (void)v; o->free_bytes = o->total_bytes = 0; return 0; }
int ink_fs_card_hold(void) { return INK_FS_ERR_NO_CARD; }
void ink_fs_card_release(void) {}
void ink_fs_changed_outside(void) {}
int ink_fs_on_release(ink_fs_release_fn fn, void *ctx) { (void)fn; (void)ctx; return 0; }
int ink_fs_card_replace(const char *part, const char *phys) { (void)part; (void)phys; return INK_FS_ERR_NO_CARD; }
int ink_cap_register(const ink_cap_t *c, int n) { (void)c; (void)n; return 0; }
int ink_exec_post(ink_exec_t e, ink_exec_job_t *j, void (*fn)(void *), void *a) { (void)e; (void)j; (void)fn; (void)a; return 0; }
void ink_log(ink_log_level_t l, const char *m, const char *fmt, ...) { (void)l; (void)m; (void)fmt; }
void ink_power_hold(ink_power_need_t n) { (void)n; }
void ink_power_release(ink_power_need_t n) { (void)n; }
static int probes_rejected;
void ink_probe_count(const char *m) { probes_rejected += !strcmp(m, "card_files.rejected"); }
osal_err_t osal_task_create(const osal_task_cfg_t *c, osal_task_t **o) { (void)c; (void)o; return OSAL_NO_MEM; }
void osal_task_delay_us(uint64_t us) { now_us += us; }
static uint8_t heap[CF_BUF + INK_CARD_CHUNK + INK_CARD_LINE_MAX + 64];
void *osal_malloc(size_t n, uint32_t caps) { (void)caps; return n <= sizeof heap ? heap : NULL; }
int ink_state_define(const ink_state_def_t *d) { (void)d; return 1; }
void ink_state_set_int(int h, int64_t v) { (void)h; (void)v; }
void ink_state_set_str(int h, const char *s) { (void)h; (void)s; }
int ink_state_find(const char *id) { (void)id; return 2; }
bool ink_state_get_int(int h, int64_t *o) { (void)h; *o = 1; return true; }
int ink_state_get_str(int h, char *o, size_t cap) { (void)h; return snprintf(o, cap, "normal"); }
void osal_cost_units(const char *c, uint64_t u) { (void)c; (void)u; }
void osal_preempt_point(void) {}
int ink_state_watch(const char *p, ink_state_fn fn, void *c, ink_exec_t e) { (void)p; (void)fn; (void)c; (void)e; return 0; }

static void data_line(char *out, size_t cap, int idx, unsigned long off, const uint8_t *bytes, size_t n, uint32_t crc, const char *b64) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char enc[2736]; /* base64 of 2048 bytes and its NUL */
    size_t k = 0;
    assert(n <= 2048);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)bytes[i] << 16 | (i + 1 < n ? (uint32_t)bytes[i + 1] << 8 : 0) | (i + 2 < n ? bytes[i + 2] : 0);
        enc[k++] = T[v >> 18 & 63];
        enc[k++] = T[v >> 12 & 63];
        enc[k++] = i + 1 < n ? T[v >> 6 & 63] : '=';
        enc[k++] = i + 2 < n ? T[v & 63] : '=';
    }
    enc[k] = 0;
    snprintf(out, cap, "INKCF1 DATA %d %lu %zu %08x %s\n", idx, off, n, (unsigned)crc, b64 ? b64 : enc);
}

/* One GET answered by `reply` (and nothing after it): what usb_fetch returns. */
static int fetch_with(const char *reply, uint8_t *buf, uint32_t len) {
    reset_link();
    feed(reply);
    probes_rejected = 0;
    return usb_fetch(0, 4096, buf, len);
}

int main(void) {
    /* the list's paths are checked again at boot: bad ones do not exist */
    assert(ink_card_files_init() == 0);
    bool want[] = {true, false, false, false, false, false};
    for (int i = 0; i < INK_CARD_FILES_COUNT; i++) EXPECT(CF.valid[i] == want[i], "entry %d valid=%d", i, CF.valid[i]);
    EXPECT(ink_card_files_state("other", NULL) == INK_CARD_READY, "an app with only bad entries needs nothing");
    EXPECT(ink_card_files_state("reader", NULL) == INK_CARD_CHECKING, "the good entry is not checked yet");

    /* CRC: ink_crc32 equals zlib.crc32 (vectors computed with Python's zlib) */
    uint8_t all[256];
    for (int i = 0; i < 256; i++) all[i] = (uint8_t)i;
    EXPECT(ink_crc32(0, "123456789", 9) == 0xcbf43926u, "crc 123456789");
    EXPECT(ink_crc32(0, all, 256) == 0x29058c73u, "crc 0..255");
    EXPECT(ink_crc32(0, "INKCF1 card files", 17) == 0xe1b3c25bu, "crc text");

    /* HELLO, verbatim; another digest in SERVE opens nothing, ours does */
    CF.need = 1;
    reset_link();
    feed("INKCF1 SERVE ffffffffffffffff\n");
    now_us = 0;
    EXPECT(usb_begin() == -1, "SERVE with another digest is not a tool");
    EXPECT(!strncmp(tx, "INKCF1 HELLO 00112233aabbccdd 1\n", 32), "HELLO line: %.60s", tx);
    EXPECT(now_us >= CF_HELLO_FOR_MS * 1000ull && now_us < (CF_HELLO_FOR_MS + 2 * CF_HELLO_EVERY_MS) * 1000ull, "gave up after 30 s");
    int hellos = 0;
    for (const char *p = tx; (p = strstr(p, "INKCF1 HELLO ")) != NULL; p++) hellos++;
    EXPECT(hellos == CF_HELLO_FOR_MS / CF_HELLO_EVERY_MS, "HELLO every 2 s: %d", hellos);
    reset_link();
    feed("INKCF1 DATA 0 0 4 00000000 AAAAAA==\nINKCF1 NOBUNDLE 1\n");
    EXPECT(usb_begin() == -1, "NOBUNDLE: no tool");
    reset_link();
    feed("noise from the log\nINKCF1 ACK\nINKCF1 SERVE 00112233aabbccdd\n");
    EXPECT(usb_begin() == 0, "our digest: the tool is there");
    CF.need = 0;
    reset_link();
    feed("INKCF1 ACK\n");
    EXPECT(usb_begin() == -1 && hellos, "need 0: any answer ends it");

    /* GET, verbatim, and every wrong DATA is dropped and asked again */
    uint8_t chunk[2048], buf[2048];
    for (int i = 0; i < 2048; i++) chunk[i] = (uint8_t)(i * 7);
    uint32_t crc = ink_crc32(0, chunk, sizeof chunk);
    char line[3200];
    data_line(line, sizeof line, 0, 4096, chunk, 2048, crc, NULL);
    EXPECT(fetch_with(line, buf, 2048) == F_OK && !memcmp(buf, chunk, 2048), "a right chunk");
    EXPECT(!strncmp(tx, "INKCF1 GET 0 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef 4096 2048\n", 91), "GET line: %.100s", tx);
    struct { const char *why; int idx; unsigned long off; size_t n; uint32_t crc; const char *b64; } bad[] = {
        {"CRC", 0, 4096, 2048, crc ^ 1, NULL}, {"index", 1, 4096, 2048, crc, NULL}, {"offset", 0, 6144, 2048, crc, NULL},
        {"length", 0, 4096, 2047, ink_crc32(0, chunk, 2047), NULL}, {"base64", 0, 4096, 2048, crc, "AAAA*AAA"},
        {"base64 length", 0, 4096, 2048, crc, "AAAAAA"},
    };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        data_line(line, sizeof line, bad[k].idx, bad[k].off, chunk, bad[k].n, bad[k].crc, bad[k].b64);
        char twice[6600];
        snprintf(twice, sizeof twice, "%s%s", line, line);
        int r = fetch_with(twice, buf, 2048);
        EXPECT(r == F_IO && probes_rejected == 2, "wrong %s: dropped (r=%d rejected=%d)", bad[k].why, r, probes_rejected);
    }
    /* a line longer than INK_CARD_LINE_MAX goes as a whole, the next one counts */
    char longline[3100];
    memset(longline, 'A', sizeof longline);
    memcpy(longline, "INKCF1 NO 0 ", 12);
    longline[INK_CARD_LINE_MAX + 1] = '\n';
    longline[INK_CARD_LINE_MAX + 2] = 0;
    char both[6400];
    snprintf(both, sizeof both, "%s%s", longline, line);
    data_line(line, sizeof line, 0, 4096, chunk, 2048, crc, NULL);
    snprintf(both, sizeof both, "%s%s", longline, line);
    EXPECT(fetch_with(both, buf, 2048) == F_OK, "an over-long line is dropped whole");
    EXPECT(fetch_with("INKCF1 NO 0 sha\n", buf, 2048) == F_NO, "NO for this file");
    EXPECT(fetch_with("INKCF1 NO 3 sha\n", buf, 2048) == F_IO, "NO for another file is not an answer");
    cable = false;
    EXPECT(fetch_with(line, buf, 2048) == F_IO, "the cable went");
    cable = true;
    if (fails) return 1;
    puts("card files unit: list paths, HELLO/GET lines, DATA checks, line limit and CRC as zlib: PASS");
    return 0;
}
