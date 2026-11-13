/*
 * Storage model.
 *
 * /user     LittleFS on a simulated NOR flash partition. Every erase, program
 *           and read takes the profile's time; on the MCU the caches are off
 *           during each operation, so all tasks that are not IRAM-safe (and
 *           everything touching PSRAM) freeze on BOTH cores (vtk stall
 *           windows). NOR semantics are exact (program can only clear bits),
 *           the image can persist across runs (--flash-image) and a power cut
 *           tears the in-flight operation, for data-integrity tests.
 * /content  host directory (SD card / FAT on the device): latency +
 * /cache    bandwidth per operation, periodic write-busy spikes (card GC).
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ink/hal_storage.h"
#include "lfs.h"
#include "sim.h"

#define LFS_READ 128
#define LFS_PROG 128
#define LFS_CACHE 512
#define LFS_LOOKAHEAD 128

/* ------------------------------------------------------------ NOR flash */

typedef enum { OP_NONE, OP_PROG, OP_ERASE } op_kind_t;

static struct {
    uint8_t *img;
    size_t size;
    uint32_t sector;
    uint32_t *erase_count;
    uint64_t reads, read_bytes, progs, prog_bytes, erases;
    uint64_t max_stall_ns;
    /* in-flight operation (power-cut tearing) */
    op_kind_t op;
    uint32_t op_off, op_len;
    const uint8_t *op_data;
    uint64_t op_t0, op_t1;
    bool cut;
    bool loaded; /* started from --flash-image */
} FL;

static void maybe_cut_during_op(void);

static uint64_t sample_us(uint32_t typ, uint32_t max) {
    switch (g_profile.flash_pessimism) {
    case 0: return typ;
    case 1: return max;
    default: /* tail: mostly a bit above typical, 10% anywhere up to max */
        if (sim_rand() % 10 == 0) return typ + (uint64_t)((double)(max - typ) * sim_rand01());
        return typ + (uint64_t)((double)typ * 0.2 * sim_rand01());
    }
}

static uint64_t g_erase_t0, g_erase_t1; /* the last erase's stall window (tests) */
void sim_flash_last_erase(uint64_t *t0, uint64_t *t1) { *t0 = g_erase_t0; *t1 = g_erase_t1; }

/* The flash is busy for ns: caches off (MCU) -> stall window; the caller polls the status. */
static void flash_busy(uint64_t ns, const char *what) {
    uint64_t t0 = vtk_now();
    if (g_profile.flash_stalls_cpu && sim_display_playing()) {
        /* the system rule is: no flash operation while the panel scans (storage service + flash guard) */
        static int warned;
        sim_metric_add("flash.ops_during_waveform", 1);
        if (++warned <= 5) sim_warn("flash %s while the panel scans (freezes the feeders; IPC waits behind them)", what);
        uint64_t w = sim_display_ipc_wait_ns(t0);
        if (w) {
            sim_metric_sample("flash.ipc_wait_ms", w / 1e6);
            ns += w;
        }
    }
    if (g_profile.flash_stalls_cpu) {
        /* --flash-holds-i2c: the S31 port's I2C lock around each operation (s31_flash_begin). Off by default: the
         * modelled mutex hands nothing to a waiter and no time passes between two operations, so a long littlefs job
         * would starve every touch read (546 ms seen), which the device is not shown to do (tap-loss-1 report) */
        bool bus = g_sim.flash_holds_i2c && sim_board_present() && sim_i2c_lock_for_flash();
        if (!strcmp(what, "erase")) g_erase_t0 = vtk_now(), g_erase_t1 = vtk_now() + ns;
        vtk_lock();
        vtk_stall_begin_locked(vtk_now() + ns);
        vtk_unlock(); /* every task, the caller included (it spins in IRAM), is frozen until the window ends */
        vtk_sched_point();
        sim_i2c_unlock_for_flash(bus);
        if (ns > FL.max_stall_ns) FL.max_stall_ns = ns;
        sim_metric_sample("flash.stall_ms", ns / 1e6);
    } else {
        vtk_sleep_ns(ns); /* eMMC/SPI NAND with DMA: the caller just waits */
    }
    sim_trace_span("flash", what, t0, t0 + ns, NULL);
}

/* SPI1 reads n bytes: one command per spi_cmd_bytes. */
static uint64_t read_ns(size_t n) {
    double mbps = g_profile.flash_read_mbps > 0 ? g_profile.flash_read_mbps : 20;
    uint64_t cmds = (n + g_profile.flash_spi_cmd_bytes - 1) / g_profile.flash_spi_cmd_bytes;
    return cmds * g_profile.flash_spi_cmd_ns + (uint64_t)((double)n * 1000.0 / mbps);
}

static int bd_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    (void)c;
    size_t a = (size_t)block * FL.sector + off;
    if (a + size > FL.size) return LFS_ERR_IO;
    size_t win = size; /* one cache-off window per read chunk */
    void *bounce = NULL;
    if (g_profile.flash_stalls_cpu && pools_is_external(buffer)) {
        /* esp_flash_read into PSRAM: an internal temp buffer of min(size, 16 KiB, largest free internal
         * block) (IDF get_buffer_malloc, 5 tries), the read in chunks of that size, then a copy */
        osal_mem_stats_t ms;
        osal_mem_stats(OSAL_MEM_INTERNAL, &ms);
        size_t want = size < 16384 ? size : 16384;
        if (want > ms.largest_free) want = ms.largest_free & ~(size_t)3;
        bounce = want ? osal_malloc(want, OSAL_MEM_INTERNAL) : NULL;
        sim_metric_add("flash.reads_into_psram", 1);
        if (!bounce) {
            sim_metric_add("flash.read_no_mem", 1); /* ESP_ERR_NO_MEM: littlefs sees an I/O error */
            sim_warn("flash read into PSRAM failed: no internal memory for the temp buffer");
            return LFS_ERR_IO;
        }
        win = want;
    }
    for (size_t done = 0; done < size; done += win) {
        size_t n = size - done < win ? size - done : win;
        flash_busy(SIM_US(g_profile.flash_op_overhead_us) + read_ns(n), "read");
    }
    if (bounce) {
        sim_charge_cycles((double)size * profile_cost("internal_to_psram.copy_byte"));
        osal_free(bounce);
    }
    sim_charge_cycles((double)size * profile_cost("crc32.byte")); /* littlefs checks CRCs on what it reads */
    memcpy(buffer, FL.img + a, size);
    FL.reads++;
    FL.read_bytes += size;
    return 0;
}

static void nor_program(uint32_t a, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) FL.img[a + i] &= data[i]; /* NOR: 1 -> 0 only */
}

/* One program command of n bytes inside a page whose full program takes page_ns. */
static uint64_t prog_cmd_ns(uint32_t n, uint32_t page, uint64_t page_ns) {
    uint64_t first = SIM_US(g_profile.prog_first_us);
    if (first > page_ns) first = page_ns;
    return g_profile.flash_spi_cmd_ns + first + (page_ns - first) * n / page;
}

/* esp_flash_write from littlefs' internal program buffer (direct write): one cache-off window per
 * write chunk (8 KiB upstream), inside it one SPI1 program command per <= 64 bytes within a page. */
static int bd_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    (void)c;
    uint32_t a = (uint32_t)(block * FL.sector + off);
    if (a + size > FL.size) return LFS_ERR_IO;
    const uint8_t *d = buffer;
    uint32_t page = g_profile.flash_page ? g_profile.flash_page : 256, cmd = g_profile.flash_spi_cmd_bytes;
    uint32_t done = 0;
    while (done < size) {
        uint32_t clen = size - done < g_profile.flash_write_chunk ? size - done : g_profile.flash_write_chunk;
        uint64_t ns = SIM_US(g_profile.flash_op_overhead_us), page_ns = 0;
        uint32_t cur = UINT32_MAX;
        for (uint32_t o = 0; o < clen;) {
            uint32_t pa = a + done + o;
            if (pa / page != cur) {
                cur = pa / page;
                page_ns = SIM_US(sample_us(g_profile.prog_us_typ, g_profile.prog_us_max));
            }
            uint32_t n = page - pa % page;
            if (n > cmd) n = cmd;
            if (n > clen - o) n = clen - o;
            ns += prog_cmd_ns(n, page, page_ns);
            o += n;
        }
        FL.op = OP_PROG;
        FL.op_off = a + done;
        FL.op_len = clen;
        FL.op_data = d + done;
        FL.op_t0 = vtk_now();
        FL.op_t1 = FL.op_t0 + ns;
        maybe_cut_during_op();
        flash_busy(ns, "program");
        if (FL.cut) return LFS_ERR_IO;
        FL.op = OP_NONE;
        nor_program(a + done, d + done, clen);
        done += clen;
    }
    sim_charge_cycles((double)size * profile_cost("crc32.byte")); /* commit CRCs */
    FL.progs++;
    FL.prog_bytes += size;
    return 0;
}

static int bd_erase(const struct lfs_config *c, lfs_block_t block) {
    (void)c;
    uint32_t a = block * FL.sector;
    if (a + FL.sector > FL.size) return LFS_ERR_IO;
    uint64_t ns = SIM_US(g_profile.flash_op_overhead_us) + SIM_US(sample_us(g_profile.erase_us_typ, g_profile.erase_us_max));
    FL.op = OP_ERASE;
    FL.op_off = a;
    FL.op_len = FL.sector;
    FL.op_t0 = vtk_now();
    FL.op_t1 = FL.op_t0 + ns;
    maybe_cut_during_op();
    flash_busy(ns, "erase"); /* one sector per call: IDF yields only before a later erase of a multi-sector call */
    if (FL.cut) return LFS_ERR_IO;
    FL.op = OP_NONE;
    memset(FL.img + a, 0xff, FL.sector);
    FL.erase_count[block]++;
    FL.erases++;
    return 0;
}

static int bd_sync(const struct lfs_config *c) {
    (void)c;
    return 0;
}

static void save_image(void) {
    if (!g_sim.flash_image || !FL.img) return;
    FILE *f = fopen(g_sim.flash_image, "wb");
    if (!f || fwrite(FL.img, 1, FL.size, f) != FL.size) sim_warn("cannot save flash image %s", g_sim.flash_image);
    if (f) fclose(f);
}

static int g_cut_on_write; /* > 0: cut power during the n-th program/erase from now */

static void armed_cut_cb(void *arg) {
    (void)arg;
    sim_storage_power_cut();
}

static void maybe_cut_during_op(void) { /* task context, right after FL.op_t0/op_t1 are set */
    if (g_cut_on_write <= 0 || --g_cut_on_write > 0) return;
    vtk_lock();
    vtk_event_at(FL.op_t0 + (FL.op_t1 - FL.op_t0) * 2 / 5, armed_cut_cb, NULL);
    vtk_unlock();
}

void sim_storage_arm_power_cut(int nth_write) { g_cut_on_write = nth_write > 0 ? nth_write : 1; }

void sim_storage_power_cut(void) { /* ISR context */
    if (FL.cut) return;
    FL.cut = true;
    uint64_t now = vtk_isr_now();
    if (FL.op != OP_NONE && now >= FL.op_t0 && now < FL.op_t1) {
        double f = (double)(now - FL.op_t0) / (double)(FL.op_t1 - FL.op_t0);
        uint32_t n = (uint32_t)(f * FL.op_len);
        if (FL.op == OP_PROG) {
            nor_program(FL.op_off, FL.op_data, n);
            if (n < FL.op_len) FL.img[FL.op_off + n] &= (uint8_t)(FL.op_data[n] | (sim_rand() & 0xff)); /* torn byte */
        } else {
            for (uint32_t i = 0; i < FL.op_len; i++) /* partially erased sector: bits drift towards 1 */
                if (sim_rand01() < f) FL.img[FL.op_off + i] = 0xff;
                else FL.img[FL.op_off + i] |= (uint8_t)(sim_rand() & sim_rand() & 0xff);
        }
        sim_note("power cut tore an in-flight flash %s at 0x%lx (%.0f%% done)", FL.op == OP_PROG ? "program" : "erase",
                 (unsigned long)FL.op_off, f * 100);
        sim_metric_add("flash.torn_ops", 1);
    }
    save_image();
    sim_trace_instant("power", "POWER CUT", now, NULL);
    vtk_stop();
}

/* ------------------------------------------------------------- LittleFS */

static struct {
    lfs_t lfs;
    struct lfs_config cfg;
    bool mounted;
    osal_mutex_t *lock;
} U;

/* ------------------------------------------------------------------ SD */

static struct {
    char root[480];
    uint64_t written_since_spike;
    uint64_t ops, bytes_read, bytes_written, spikes;
    bool absent;       /* taken out (script "sd remove") */
    bool lent;         /* lent to a computer as a USB disk: unmounted */
    bool dirty;        /* the computer did not eject it: the next mount fails */
    void (*cb)(void *ctx);
    void *ctx;
    int64_t free_left; /* < 0: half of the capacity, as a fixed number; else what writes may still use (script "sd full") */
} SD;

static bool sd_ok(void) { return !SD.absent && !SD.lent && !SD.dirty; }

bool hal_fs_card_present(void) { return sd_ok(); }
void hal_fs_new_card_id(uint8_t out[16]) {
    static unsigned entropy = 1; entropy ^= g_sim.seed ^ (uint32_t)osal_now_us();
    for (size_t i = 0; i < 16; i++) out[i] = (uint8_t)rand_r(&entropy);
}
static void sd_changed(void *arg) { (void)arg; if (SD.cb) SD.cb(SD.ctx); }
void hal_fs_set_card_callback(void (*cb)(void *ctx), void *ctx) { SD.cb = cb; SD.ctx = ctx; }
void sim_sd_set_present(bool present) { /* ISR (script) */
    bool changed = SD.absent == present || (present && SD.dirty);
    SD.absent = !present;
    if (present) SD.dirty = false;
    if (changed) sim_sys_evt(sd_changed, NULL);
}
void sim_sd_set_free(int64_t bytes) { SD.free_left = bytes; }                                   /* ISR (script); < 0: no limit */
void sim_sd_lend(bool lent, bool dirty) { /* the USB disk model */
    SD.lent = lent;
    if (!lent) SD.dirty = dirty;
}
const char *sim_sd_root(void) { return SD.root; }

static void sd_cost(size_t bytes, bool write) {
    double mbps = write ? g_profile.sd_write_mbps : g_profile.sd_read_mbps;
    if (mbps <= 0) mbps = 1;
    uint64_t ns = SIM_US(g_profile.sd_op_latency_us) + (uint64_t)((double)bytes * 1000.0 / mbps);
    if (write && g_profile.sd_write_busy_spike_ms) {
        SD.written_since_spike += bytes;
        if (SD.written_since_spike >= (uint64_t)g_profile.sd_spike_every_kib * 1024ull) {
            SD.written_since_spike = 0;
            ns += SIM_MS(g_profile.sd_write_busy_spike_ms);
            SD.spikes++;
        }
    }
    /* SDMMC DMA cannot reach PSRAM: the driver bounces through a DMA-capable internal buffer
     * (held for the operation) and copies; the transfer itself is DMA */
    void *bounce = NULL;
    if (g_profile.flash_stalls_cpu) {
        const char *prev = osal_mem_tag("sd-bounce");
        bounce = osal_malloc(4096, OSAL_MEM_DMA);
        osal_mem_tag(prev);
        if (!bounce) sim_metric_add("sd.bounce_alloc_failures", 1); /* ESP_ERR_NO_MEM on the device */
    }
    sim_charge_cycles((double)bytes * profile_cost("psram_to_internal.copy_byte") + 6000);
    uint64_t t0 = vtk_now();
    vtk_sleep_ns(ns);
    if (bounce) osal_free(bounce);
    sim_trace_span("sd", write ? "write" : "read", t0, t0 + ns, NULL);
    SD.ops++;
    if (write) SD.bytes_written += bytes;
    else SD.bytes_read += bytes;
}

/* ----------------------------------------------------------- dispatch */

typedef enum { VOL_USER, VOL_SD, VOL_BAD } vol_t;

static vol_t route(const char *path, char *out, size_t outlen) {
    if (!strncmp(path, "/user", 5) && (path[5] == '/' || path[5] == 0)) {
        snprintf(out, outlen, "%s", path[5] ? path + 5 : "/");
        return VOL_USER;
    }
    if ((!strncmp(path, "/content", 8) && (path[8] == '/' || path[8] == 0)) ||
        (!strncmp(path, "/cache", 6) && (path[6] == '/' || path[6] == 0))) {
        if (strstr(path, "..") || !sd_ok()) return VOL_BAD;
        snprintf(out, outlen, "%s%s", SD.root, path);
        return VOL_SD;
    }
    return VOL_BAD;
}

struct hal_file {
    vol_t vol;
    lfs_file_t lf;
    struct lfs_file_config fcfg;
    void *cache;   /* LittleFS per-file cache (device heap) */
    void *fat_fil; /* FatFS FIL object incl. sector buffer (device heap) */
    int fd;
};

/* Every /user operation goes through here. On the MCU the flash API disables the cache; ESP-IDF
 * asserts that the caller's stack stays reachable (spi_flash cache_utils), so a task with a PSRAM
 * stack calling into /user crashes the device even for a read. */
static void lock_user(void) {
    if (g_profile.flash_stalls_cpu && sim_task_stack_external())
        osal_panic("task '%s' has its stack in PSRAM and accessed /user: the flash driver disables the cache and "
                   "ESP-IDF asserts (stack unusable with cache disabled) -> device reset. Go through the storage service.",
                   osal_task_name(osal_task_self()));
    osal_mutex_lock(U.lock, OSAL_WAIT_FOREVER);
    sim_charge_cycles(profile_cost("littlefs.op")); /* VFS + littlefs bookkeeping per call */
}
static void unlock_user(void) { osal_mutex_unlock(U.lock); }

hal_file_t *hal_fs_open(const char *path, int flags) {
    char p[512];
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_BAD) return NULL;
    hal_file_t *f = sim_host_calloc(1, sizeof *f);
    if (!f) abort();
    f->vol = v;
    if (v == VOL_USER) {
        if (!U.mounted) {
            sim_host_free(f);
            return NULL;
        }
        f->cache = osal_malloc(LFS_CACHE, OSAL_MEM_INTERNAL); /* CONFIG_LITTLEFS_MALLOC_STRATEGY_INTERNAL */
        if (!f->cache) {
            sim_host_free(f);
            return NULL;
        }
        f->fcfg.buffer = f->cache;
        int lf = 0;
        if ((flags & HAL_O_READ) && (flags & HAL_O_WRITE)) lf |= LFS_O_RDWR;
        else if (flags & HAL_O_WRITE) lf |= LFS_O_WRONLY;
        else lf |= LFS_O_RDONLY;
        if (flags & HAL_O_CREATE) lf |= LFS_O_CREAT;
        if (flags & HAL_O_TRUNC) lf |= LFS_O_TRUNC;
        if (flags & HAL_O_APPEND) lf |= LFS_O_APPEND;
        lock_user();
        int r = lfs_file_opencfg(&U.lfs, &f->lf, p, lf, &f->fcfg);
        unlock_user();
        if (r < 0) {
            osal_free(f->cache);
            sim_host_free(f);
            return NULL;
        }
        return f;
    }
    int of = 0;
    if ((flags & HAL_O_READ) && (flags & HAL_O_WRITE)) of |= O_RDWR;
    else if (flags & HAL_O_WRITE) of |= O_WRONLY;
    else of |= O_RDONLY;
    if (flags & HAL_O_CREATE) of |= O_CREAT;
    if (flags & HAL_O_TRUNC) of |= O_TRUNC;
    if (flags & HAL_O_APPEND) of |= O_APPEND;
    f->fat_fil = osal_malloc(560, OSAL_MEM_DEFAULT);
    if (!f->fat_fil) {
        sim_host_free(f);
        return NULL;
    }
    sd_cost(0, false); /* directory lookup */
    f->fd = open(p, of, 0644);
    if (f->fd < 0) {
        osal_free(f->fat_fil);
        sim_host_free(f);
        return NULL;
    }
    return f;
}

ssize_t hal_fs_read(hal_file_t *f, void *buf, size_t len) {
    if (!f) return -1;
    if (f->vol == VOL_USER) {
        lock_user();
        lfs_ssize_t r = lfs_file_read(&U.lfs, &f->lf, buf, (lfs_size_t)len);
        unlock_user();
        return r;
    }
    if (!sd_ok()) return -1; /* taken away while open */
    ssize_t r = read(f->fd, buf, len);
    if (r > 0) sd_cost((size_t)r, false);
    return r;
}

ssize_t hal_fs_write(hal_file_t *f, const void *buf, size_t len) {
    if (!f) return -1;
    if (f->vol == VOL_USER) {
        lock_user();
        lfs_ssize_t r = lfs_file_write(&U.lfs, &f->lf, buf, (lfs_size_t)len);
        unlock_user();
        return r;
    }
    if (!sd_ok()) return -1;
    if (SD.free_left >= 0 && (int64_t)len > SD.free_left) { /* the card is full: as much as fits, then nothing */
        if (!SD.free_left) return -1;
        len = (size_t)SD.free_left;
    }
    ssize_t r = write(f->fd, buf, len);
    if (r > 0) {
        if (SD.free_left >= 0) SD.free_left -= r;
        sd_cost((size_t)r, true);
    }
    return r;
}

int hal_fs_seek(hal_file_t *f, int64_t off, int whence) {
    if (!f) return -1;
    if (f->vol == VOL_USER) {
        int lw = whence == SEEK_CUR ? LFS_SEEK_CUR : whence == SEEK_END ? LFS_SEEK_END : LFS_SEEK_SET;
        lock_user();
        lfs_soff_t r = lfs_file_seek(&U.lfs, &f->lf, (lfs_soff_t)off, lw);
        unlock_user();
        return r < 0 ? -1 : 0;
    }
    return lseek(f->fd, (off_t)off, whence) < 0 ? -1 : 0;
}

int64_t hal_fs_tell(hal_file_t *f) {
    if (!f) return -1;
    if (f->vol == VOL_USER) {
        lock_user();
        lfs_soff_t r = lfs_file_tell(&U.lfs, &f->lf);
        unlock_user();
        return r;
    }
    return lseek(f->fd, 0, SEEK_CUR);
}

int hal_fs_sync(hal_file_t *f) {
    if (!f) return -1;
    if (f->vol == VOL_USER) {
        lock_user();
        int r = lfs_file_sync(&U.lfs, &f->lf);
        unlock_user();
        return r < 0 ? -1 : 0;
    }
    sd_cost(512, true); /* FAT + directory entry update */
    return fsync(f->fd);
}

int hal_fs_close(hal_file_t *f) {
    if (!f) return -1;
    int r;
    if (f->vol == VOL_USER) {
        lock_user();
        r = lfs_file_close(&U.lfs, &f->lf);
        unlock_user();
        osal_free(f->cache);
    } else {
        sd_cost(512, true);
        r = close(f->fd);
        osal_free(f->fat_fil);
    }
    sim_host_free(f);
    return r < 0 ? -1 : 0;
}

int hal_fs_remove(const char *path) {
    char p[512];
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_USER) {
        lock_user();
        int r = lfs_remove(&U.lfs, p);
        unlock_user();
        return r < 0 ? -1 : 0;
    }
    if (v == VOL_SD) {
        sd_cost(512, true);
        return remove(p);
    }
    return -1;
}

int hal_fs_rename(const char *from, const char *to) {
    char a[512], b[512];
    vol_t va = route(from, a, sizeof a), vb = route(to, b, sizeof b);
    if (va != vb || va == VOL_BAD) return -1;
    if (va == VOL_USER) {
        lock_user();
        int r = lfs_rename(&U.lfs, a, b);
        unlock_user();
        return r < 0 ? -1 : 0;
    }
    sd_cost(1024, true);
    return rename(a, b);
}

int hal_fs_mkdir(const char *path) {
    char p[512];
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_USER) {
        lock_user();
        int r = lfs_mkdir(&U.lfs, p);
        unlock_user();
        return (r < 0 && r != LFS_ERR_EXIST) ? -1 : 0;
    }
    if (v == VOL_SD) {
        sd_cost(512, true);
        return (mkdir(p, 0755) < 0 && errno != EEXIST) ? -1 : 0;
    }
    return -1;
}

int hal_fs_exists(const char *path) {
    char p[512]; struct stat st;
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_USER) {
        struct lfs_info info;
        lock_user();
        int r = lfs_stat(&U.lfs, p, &info);
        unlock_user();
        return r == 0 ? 1 : r == LFS_ERR_NOENT || r == LFS_ERR_NOTDIR ? 0 : -1;
    }
    if (v != VOL_SD) return -1;
    sd_cost(0, false);
    if (!stat(p, &st)) return 1;
    return errno == ENOENT || errno == ENOTDIR ? 0 : -1;
}

int64_t hal_fs_size(const char *path) {
    char p[512];
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_USER) {
        struct lfs_info info;
        lock_user();
        int r = lfs_stat(&U.lfs, p, &info);
        unlock_user();
        return r < 0 ? -1 : (int64_t)info.size;
    }
    if (v == VOL_SD) {
        struct stat st;
        sd_cost(0, false);
        return stat(p, &st) < 0 ? -1 : (int64_t)st.st_size;
    }
    return -1;
}

typedef struct { const char *path; int64_t stamp; } sd_mtime_t;
static int64_t sd_stamp(const struct stat *st) {
#ifdef __APPLE__
    return (int64_t)st->st_mtimespec.tv_sec * 1000000000 + st->st_mtimespec.tv_nsec;
#else
    return (int64_t)st->st_mtim.tv_sec * 1000000000 + st->st_mtim.tv_nsec;
#endif
}
static void sd_mtime_on_host(void *arg) {
    sd_mtime_t *a = arg; struct stat st;
    if (stat(a->path, &st) == 0) a->stamp = sd_stamp(&st);
}
int64_t hal_fs_mtime(const char *path) {
    char p[512];
    if (route(path, p, sizeof p) != VOL_SD) return -1;
    sd_cost(0, false);
    sd_mtime_t a = {p, -1}; vtk_host_call(sd_mtime_on_host, &a);
    return a.stamp;
}

struct hal_dir {
    vol_t vol;
    lfs_dir_t ld;
    DIR *hd;
    char base[512];
};

/* The host directory behind /content and /cache is host work: opendir allocates, readdir refills. */
static void sd_opendir_on_host(void *p) {
    hal_dir_t *d = p;
    d->hd = opendir(d->base);
}

static void sd_closedir_on_host(void *p) { closedir(((hal_dir_t *)p)->hd); }

typedef struct {
    hal_dir_t *d;
    hal_dirent_t *out;
    int r;
} sd_readdir_t;

static void sd_readdir_on_host(void *p) {
    sd_readdir_t *a = p;
    a->r = 0;
    struct dirent *e;
    while ((e = readdir(a->d->hd))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1024];
        struct stat st;
        snprintf(full, sizeof full, "%s/%s", a->d->base, e->d_name);
        if (stat(full, &st) < 0) continue;
        snprintf(a->out->name, sizeof a->out->name, "%s", e->d_name);
        a->out->is_dir = S_ISDIR(st.st_mode);
        a->out->size = st.st_size;
        a->out->mtime = sd_stamp(&st);
        a->r = 1;
        return;
    }
}

hal_dir_t *hal_fs_opendir(const char *path) {
    char p[512];
    vol_t v = route(path, p, sizeof p);
    if (v == VOL_BAD) return NULL;
    hal_dir_t *d = sim_host_calloc(1, sizeof *d);
    if (!d) abort();
    d->vol = v;
    snprintf(d->base, sizeof d->base, "%s", p);
    if (v == VOL_USER) {
        lock_user();
        int r = lfs_dir_open(&U.lfs, &d->ld, p);
        unlock_user();
        if (r < 0) {
            sim_host_free(d);
            return NULL;
        }
        return d;
    }
    sd_cost(512, false);
    vtk_host_call(sd_opendir_on_host, d);
    if (!d->hd) {
        sim_host_free(d);
        return NULL;
    }
    return d;
}

int hal_fs_readdir(hal_dir_t *d, hal_dirent_t *out) {
    if (!d) return -1;
    if (d->vol == VOL_USER) {
        for (;;) {
            struct lfs_info info;
            lock_user();
            int r = lfs_dir_read(&U.lfs, &d->ld, &info);
            unlock_user();
            if (r <= 0) return r;
            if (!strcmp(info.name, ".") || !strcmp(info.name, "..")) continue;
            snprintf(out->name, sizeof out->name, "%s", info.name);
            out->is_dir = info.type == LFS_TYPE_DIR;
            out->size = info.size;
            out->mtime = -1;
            return 1;
        }
    }
    sd_readdir_t a = {d, out, 0};
    vtk_host_call(sd_readdir_on_host, &a);
    if (a.r) sim_charge_cycles(20000); /* FAT directory entry parsing (LFN) */
    return a.r;
}

void hal_fs_closedir(hal_dir_t *d) {
    if (!d) return;
    if (d->vol == VOL_USER) {
        lock_user();
        lfs_dir_close(&U.lfs, &d->ld);
        unlock_user();
    } else if (d->hd) {
        vtk_host_call(sd_closedir_on_host, d);
    }
    sim_host_free(d);
}

int hal_fs_volume_info(const char *volume, hal_fs_volume_info_t *out) {
    if (!strcmp(volume, "/user")) {
        if (!U.mounted) return -1;
        lock_user();
        lfs_ssize_t used = lfs_fs_size(&U.lfs);
        unlock_user();
        out->total_bytes = (uint64_t)U.cfg.block_count * U.cfg.block_size;
        out->free_bytes = used < 0 ? 0 : out->total_bytes - (uint64_t)used * U.cfg.block_size;
        return 0;
    }
    if (!strcmp(volume, "/content") || !strcmp(volume, "/cache")) {
        if (!sd_ok()) return -1;
        out->total_bytes = (uint64_t)(g_profile.sd_capacity_gib * 1024.0 * 1024.0 * 1024.0);
        out->free_bytes = SD.free_left >= 0 ? (uint64_t)SD.free_left : out->total_bytes / 2;
        sd_cost(512, false); /* the FAT's free count */
        return 0;
    }
    return -1;
}

/* --------------------------------------------------------------- init */

static void mkdirs(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    mkdir(tmp, 0755);
}

static void copy_tree(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) {
        sim_warn("--content %s: not a directory", src);
        return;
    }
    mkdirs(dst);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || !strcmp(e->d_name, ".DS_Store")) continue; /* the card has hidden folders of its own */
        char a[1024], b[1024];
        snprintf(a, sizeof a, "%s/%s", src, e->d_name);
        snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
        struct stat st;
        if (stat(a, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            copy_tree(a, b);
            continue;
        }
        FILE *in = fopen(a, "rb"), *out = fopen(b, "wb");
        char *buf = sim_host_malloc(65536); /* host-side copy: keep it off the simulated task's stack */
        if (in && out && buf) {
            size_t n;
            while ((n = fread(buf, 1, 65536, in)) > 0) fwrite(buf, 1, n, out);
        }
        sim_host_free(buf);
        if (in) fclose(in);
        if (out) fclose(out);
    }
    closedir(d);
}

/* The card and the flash as they are at power-on, from host files: host work, not the boot task's stack. */
static void power_on_contents(void *arg) {
    (void)arg;
    /* SD card */
    if (g_sim.sd_dir) snprintf(SD.root, sizeof SD.root, "%s", g_sim.sd_dir);
    else snprintf(SD.root, sizeof SD.root, "%s/sd", g_sim.out_dir ? g_sim.out_dir : ".");
    char p[600];
    snprintf(p, sizeof p, "%s/content", SD.root);
    mkdirs(p);
    snprintf(p, sizeof p, "%s/cache", SD.root);
    mkdirs(p);
    if (g_sim.seed_content) { /* files that are already on the card when the device boots */
        snprintf(p, sizeof p, "%s/content", SD.root);
        copy_tree(g_sim.seed_content, p);
    }

    /* internal flash */
    FL.sector = g_profile.flash_sector ? g_profile.flash_sector : 4096;
    FL.size = g_profile.flash_user_bytes / FL.sector * FL.sector;
    FL.img = sim_host_malloc(FL.size);
    FL.erase_count = sim_host_calloc(FL.size / FL.sector, sizeof(uint32_t));
    if (!FL.img || !FL.erase_count) abort();
    memset(FL.img, 0xff, FL.size);
    bool loaded = false;
    if (g_sim.flash_image) {
        FILE *f = fopen(g_sim.flash_image, "rb");
        if (f) {
            loaded = fread(FL.img, 1, FL.size, f) == FL.size;
            fclose(f);
            if (!loaded) sim_warn("flash image %s has the wrong size; starting erased", g_sim.flash_image);
        }
    }
    FL.loaded = loaded;
}

void sim_storage_init(void) {
    SD.free_left = -1;
    vtk_host_call(power_on_contents, NULL);
    bool loaded = FL.loaded;

    const char *prev = osal_mem_tag("littlefs");
    U.lock = osal_mutex_create();
    U.cfg.context = NULL;
    U.cfg.read = bd_read;
    U.cfg.prog = bd_prog;
    U.cfg.erase = bd_erase;
    U.cfg.sync = bd_sync;
    U.cfg.read_size = LFS_READ;
    U.cfg.prog_size = LFS_PROG;
    U.cfg.block_size = FL.sector;
    U.cfg.block_count = (lfs_size_t)(FL.size / FL.sector);
    U.cfg.block_cycles = 512;
    U.cfg.cache_size = LFS_CACHE;
    U.cfg.lookahead_size = LFS_LOOKAHEAD;
    /* Port decision: CONFIG_LITTLEFS_MALLOC_STRATEGY_INTERNAL. With buffers in PSRAM, ESP-IDF
     * programs flash 32 bytes at a time (one cache-off window each) and bounces reads through
     * a temporary internal buffer of up to 16 KiB per call. */
    U.cfg.read_buffer = osal_malloc(LFS_CACHE, OSAL_MEM_INTERNAL);
    U.cfg.prog_buffer = osal_malloc(LFS_CACHE, OSAL_MEM_INTERNAL);
    U.cfg.lookahead_buffer = osal_malloc(LFS_LOOKAHEAD, OSAL_MEM_INTERNAL);
    osal_mem_tag(prev);
    if (!U.lock || !U.cfg.read_buffer || !U.cfg.prog_buffer || !U.cfg.lookahead_buffer) osal_panic("littlefs: no memory");

    uint64_t t0 = vtk_now();
    bool blank = true;
    for (size_t i = 0; i < 2 * (size_t)FL.sector && blank; i++) blank = FL.img[i] == 0xff;
    int r = blank ? LFS_ERR_CORRUPT : lfs_mount(&U.lfs, &U.cfg);
    if (r < 0) {
        if (loaded) sim_warn("/user: mount failed (%d) on the saved image: formatting (data loss!)", r);
        else sim_note("/user: blank flash, formatting LittleFS (%u KiB)", (unsigned)(FL.size / 1024));
        r = lfs_format(&U.lfs, &U.cfg);
        if (r == 0) r = lfs_mount(&U.lfs, &U.cfg);
        if (r < 0) osal_panic("/user: cannot format/mount LittleFS (%d)", r);
        sim_metric_add("flash.formats", 1);
    }
    U.mounted = true;
    sim_metric_sample("flash.mount_ms", (vtk_now() - t0) / 1e6);
}

void sim_storage_shutdown(void) {
    if (!FL.cut) save_image();
}

void sim_storage_report(void) {
    sim_metric_set("flash.reads", (double)FL.reads);
    sim_metric_set("flash.programs", (double)FL.progs);
    sim_metric_set("flash.program_bytes", (double)FL.prog_bytes);
    sim_metric_set("flash.erases", (double)FL.erases);
    sim_metric_set("flash.max_stall_ms", FL.max_stall_ns / 1e6);
    uint32_t maxe = 0;
    for (size_t i = 0; FL.erase_count && i < FL.size / FL.sector; i++)
        if (FL.erase_count[i] > maxe) maxe = FL.erase_count[i];
    sim_metric_set("flash.max_sector_erases", maxe);
    sim_metric_set("sd.ops", (double)SD.ops);
    sim_metric_set("sd.read_bytes", (double)SD.bytes_read);
    sim_metric_set("sd.write_bytes", (double)SD.bytes_written);
    sim_metric_set("sd.busy_spikes", (double)SD.spikes);
}
