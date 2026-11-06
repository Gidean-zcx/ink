/* Durable member backups. Immutable blobs use the existing hub connection; only exec.bg touches files. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/backup.h"
#include "ink/sys/backup_format.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/sha256.h"
#include "ink/sys/storage.h"
#include "ink/sys/res.h"
#include "ink/sys/svc.h"
#include "ink/sys/task.h"

#define ITEMS 128
#define CHUNK 16384u
#define SMALL_MAX (1024u * 1024u)
#define MAN_MAGIC 0x314d4249u
#define STATE_MAGIC 0x31534249u
#define RESULT_CAP 32768u
#define RETRY_US 1000000u
#define WAIT_IO 2

enum { ITEM_DB = 1, ITEM_STATE, ITEM_FILE };
enum { PH_NONE, PH_CAPTURE, PH_UPLOAD, PH_SAVED, PH_FETCH, PH_PREPARE, PH_PREVIEW, PH_CHECK, PH_RESTORE, PH_DONE, PH_FAILED };
static const char *const PH_NAMES[] = {"none", "collecting", "uploading", "saved", "fetching", "preparing", "preview", "checking", "restoring", "done", "failed"};

typedef ink_backup_item_t item_t;
typedef ink_backup_manifest_t manifest_t;

typedef struct {
    uint32_t magic, format, phase, generation, next, offset, subphase;
    char member[INK_MEMBER_ID_MAX];
    uint8_t identity[16];
    char manifest_hash[65], preview[33];
    uint8_t preview_hash[32];
    uint32_t manifest_size, received, durable, conflicts;
    int64_t saved_s;
    char reason[96];
    uint32_t crc;
} journal_t;

typedef struct { int result, done; } stored_t;
static struct {
    journal_t j;
    manifest_t *m;
    uint8_t *buf; /* One bounded transfer block, allocated only while work runs. */
    ink_exec_job_t job;
    ink_svc_deadline_t retry;
    struct { char member[INK_MEMBER_ID_MAX]; uint32_t generation; int id; } tasks[INK_MEMBERS_MAX];
    int no_task;
    struct { char member[INK_MEMBER_ID_MAX]; bool pending; } known[INK_MEMBERS_MAX];
    bool started;
} B INK_PSRAM_BSS;

/* Network callbacks use a separate continuation, never transient step buffers. */
static struct {
    bool bound, pending, done, receiving;
    char member[INK_MEMBER_ID_MAX], reply[1024];
    uint8_t identity[16], *rx;
    uint32_t generation, phase, next, stage, offset, ack, expected, rx_len;
    uint16_t upload, xfer;
    uintptr_t cookie;
    int status;
    uint64_t until;
} N INK_PSRAM_BSS;
static uintptr_t next_cookie;
static ink_svc_deadline_t net_at INK_PSRAM_BSS;
static void wake(void *arg);

static void net_reset(void) {
    ink_svc_cancel(&net_at);
    ink_svc_lock();
    uint8_t *rx = N.rx;
    memset(&N, 0, sizeof N);
    ink_svc_unlock();
    osal_free(rx);
}

static void net_bind(void) {
    if (N.bound && !strcmp(N.member, B.j.member) && N.generation == B.j.generation &&
        !memcmp(N.identity, B.j.identity, 16) && N.phase == B.j.phase && N.next == B.j.next) return;
    net_reset();
    ink_svc_lock();
    snprintf(N.member, sizeof N.member, "%s", B.j.member);
    memcpy(N.identity, B.j.identity, 16);
    N.generation = B.j.generation; N.phase = B.j.phase; N.next = B.j.next; N.bound = true;
    ink_svc_unlock();
}

static void net_wait(void) {
    N.until = osal_now_us() + 3000000;
    ink_svc_at(&net_at, N.until, wake, NULL);
}

static int *task_id(void) {
    int free_slot = -1;
    for (int i = 0; i < INK_MEMBERS_MAX; i++) {
        if (!strcmp(B.tasks[i].member, B.j.member)) {
            if (B.tasks[i].generation != B.j.generation) { B.tasks[i].generation = B.j.generation; B.tasks[i].id = 0; }
            return &B.tasks[i].id;
        }
        if (free_slot < 0 && (!B.tasks[i].member[0] || !ink_member_generation(B.tasks[i].member))) free_slot = i;
    }
    if (free_slot >= 0) {
        snprintf(B.tasks[free_slot].member, sizeof B.tasks[free_slot].member, "%s", B.j.member);
        B.tasks[free_slot].generation = B.j.generation; B.tasks[free_slot].id = 0;
        return &B.tasks[free_slot].id;
    }
    return &B.no_task;
}

static void remember(const char *member, bool pending) {
    int slot = -1;
    for (int i = 0; i < INK_MEMBERS_MAX; i++) {
        if (!strcmp(B.known[i].member, member)) { B.known[i].pending = pending; return; }
        if (slot < 0 && (!B.known[i].member[0] || !ink_member_generation(B.known[i].member))) slot = i;
    }
    if (slot >= 0) { snprintf(B.known[slot].member, sizeof B.known[slot].member, "%s", member); B.known[slot].pending = pending; }
}
static bool pending_for(const char *member) {
    for (int i = 0; i < INK_MEMBERS_MAX; i++) if (B.known[i].pending && (!member || !strcmp(B.known[i].member, member))) return true;
    return false;
}

static bool actor_valid(void) {
    return B.j.generation && B.j.generation == ink_member_generation(B.j.member) &&
           !strcmp(B.j.member, ink_member_current()->id) && ink_member_space_open();
}

static void hex(const uint8_t *p, size_t n, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { out[i * 2] = digits[p[i] >> 4]; out[i * 2 + 1] = digits[p[i] & 15]; }
    out[n * 2] = 0;
}

static bool hash_ok(const char *s) {
    if (strlen(s) != 64) return false;
    for (int i = 0; i < 64; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}

static bool zero_hash(const uint8_t *h) {
    uint8_t b = 0;
    for (int i = 0; i < 32; i++) b |= h[i];
    return !b;
}

static bool state_path(char *out, const char *member) {
    return ink_layout_member(out, INK_LAYOUT_PATH_MAX, member, "backup.journal") > 0;
}

static void stored(void *ctx, int result) {
    stored_t *s = ctx;
    s->result = result;
    __atomic_store_n(&s->done, 1, __ATOMIC_RELEASE);
}

static int write_state_file(const char *path, const void *p, size_t n) {
    stored_t s = {0};
    if (ink_storage_write_borrow(path, p, n, stored, &s) != 0) return -1;
    while (!__atomic_load_n(&s.done, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
    return s.result;
}

static int save_journal(void) {
    char path[INK_LAYOUT_PATH_MAX];
    B.j.magic = STATE_MAGIC; B.j.format = 2;
    B.j.crc = ink_crc32(0, &B.j, offsetof(journal_t, crc));
    int rc = state_path(path, B.j.member) ? write_state_file(path, &B.j, sizeof B.j) : -1;
    if (!rc) remember(B.j.member, B.j.phase == PH_CAPTURE || B.j.phase == PH_UPLOAD || B.j.phase == PH_FETCH || B.j.phase == PH_PREPARE || B.j.phase == PH_CHECK || B.j.phase == PH_RESTORE);
    return rc;
}

static bool load_journal(const char *member) {
    char path[INK_LAYOUT_PATH_MAX];
    journal_t j = {0};
    if (!state_path(path, member)) return false;
    int n = ink_storage_read(path, &j, sizeof j);
    if (n < 0) { memset(&B.j, 0, sizeof B.j); return ink_storage_size(path) < 0; }
    if (n >= 8 && j.magic == STATE_MAGIC && j.format == 1) {
        memset(&B.j, 0, sizeof B.j);
        snprintf(B.j.member, sizeof B.j.member, "%s", member);
        B.j.generation = ink_member_generation(member); B.j.phase = PH_FAILED;
        snprintf(B.j.reason, sizeof B.j.reason, "旧备份没有成员身份记录，请重新备份");
        remember(member, false);
        return ink_member_durable_id(member, B.j.generation, B.j.identity) == 0;
    }
    if (n != sizeof j || j.magic != STATE_MAGIC || j.format != 2 || j.phase > PH_FAILED ||
        j.crc != ink_crc32(0, &j, offsetof(journal_t, crc)) || memchr(j.member, 0, sizeof j.member) == NULL ||
        strcmp(j.member, member) || j.next > ITEMS + 1 || j.subphase > 2 ||
        !memchr(j.manifest_hash, 0, sizeof j.manifest_hash) || !memchr(j.preview, 0, sizeof j.preview) ||
        !memchr(j.reason, 0, sizeof j.reason)) return false;
    uint8_t identity[16];
    if (ink_member_durable_id(member, ink_member_generation(member), identity)) return false;
    if (memcmp(j.identity, identity, 16)) {
        memset(&B.j, 0, sizeof B.j);
        remember(member, false);
        return true;
    }
    B.j = j;
    B.j.generation = ink_member_generation(member);
    return true;
}

static bool stage_path(char *out, size_t cap, const char *which, unsigned i) {
    int n = snprintf(out, cap, "/content/.m/%s/.backup/%s%u", B.j.member, which, i);
    return n > 0 && (size_t)n < cap;
}

static bool stage_dirs(void) {
    char path[128];
    if (hal_fs_mkdir("/content/.m") != 0) return false;
    if (ink_layout_member_files(path, sizeof path, B.j.member) < 0 || hal_fs_mkdir(path) != 0) return false;
    snprintf(path, sizeof path, "/content/.m/%s/.backup", B.j.member);
    return hal_fs_mkdir(path) == 0;
}

static int card_write(const char *path, const void *data, size_t len) {
    hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return -1;
    int rc = hal_fs_write(f, data, len) == (ssize_t)len && hal_fs_sync(f) == 0 ? 0 : -1;
    if (hal_fs_close(f) != 0) rc = -1;
    return rc;
}

static int card_load(const char *path, void *buf, size_t len) {
    if (hal_fs_size(path) != (int64_t)len) return -1;
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return -1;
    int rc = hal_fs_read(f, buf, len) == (ssize_t)len ? 0 : -1;
    if (hal_fs_close(f) != 0) rc = -1;
    return rc;
}

/* exec.bg owns this bounded hash continuation. No file handle survives a step. */
static struct {
    char path[256];
    uint32_t size, offset, revision;
    ink_sha256_t sha;
    uint8_t hash[32];
    bool valid, ready;
} H INK_PSRAM_BSS;

static int hash_file(const char *path, uint8_t hash[32], uint32_t *size) {
    uint32_t revision = ink_fs_revision();
    if (!revision) { H.valid = false; return 1; }
    int64_t len = hal_fs_size(path);
    if (revision != ink_fs_revision()) { H.valid = false; return 1; }
    if (len < 0 || len > UINT32_MAX) return -1;
    if (!H.valid || strcmp(H.path, path) || H.size != (uint32_t)len || H.revision != revision) {
        memset(&H, 0, sizeof H);
        snprintf(H.path, sizeof H.path, "%s", path);
        H.size = (uint32_t)len; H.revision = revision; H.valid = true;
        ink_sha256_init(&H.sha);
    }
    if (!H.ready) {
        hal_file_t *f = hal_fs_open(path, HAL_O_READ);
        size_t n = H.size - H.offset; if (n > CHUNK) n = CHUNK;
        bool ok = f && hal_fs_seek(f, H.offset, SEEK_SET) == 0 && hal_fs_read(f, B.buf, n) == (ssize_t)n;
        if (f && hal_fs_close(f) != 0) ok = false;
        if (!ok) { H.valid = false; return -1; }
        if (hal_fs_size(path) != len || revision != ink_fs_revision()) { H.valid = false; return 1; }
        ink_sha256_update(&H.sha, B.buf, n); H.offset += (uint32_t)n;
        if (H.offset != H.size) return 1;
        ink_sha256_final(&H.sha, H.hash); H.ready = true;
    }
    memcpy(hash, H.hash, 32);
    if (size) *size = H.size;
    return 0;
}

static bool physical(const item_t *it, char *out, size_t cap) {
    if (it->kind == ITEM_FILE) {
        char root[INK_LAYOUT_PATH_MAX];
        if (ink_layout_member_files(root, sizeof root, B.j.member) < 0) return false;
        int n = snprintf(out, cap, "%s/%s", root, it->name);
        return n > 0 && (size_t)n < cap;
    }
    return ink_layout_member(out, cap, B.j.member, it->name) > 0;
}

static bool raw_db_file(const char *name) {
    for (int i = 0;; i++) {
        const ink_db_def_t *d = ink_db_def_at(i);
        if (!d) return false;
        size_t n = strlen(d->name);
        if (!strncmp(name, d->name, n) && name[n] == '.' &&
            (!strcmp(name + n, ".snap") || strstr(name + n, ".log"))) return true;
    }
}

static item_t *new_item(uint32_t kind, const char *name) {
    if (B.m->count == ITEMS || strlen(name) >= sizeof B.m->item[0].name) return NULL;
    item_t *it = &B.m->item[B.m->count];
    memset(it, 0, sizeof *it);
    snprintf(it->name, sizeof it->name, "%s", name);
    it->kind = kind;
    return it;
}

static int add_memory(uint32_t kind, const char *name, const void *data, size_t len, uint32_t rev, uint32_t records) {
    if (len > SMALL_MAX) return -1;
    item_t *it = new_item(kind, name);
    char path[160];
    if (!it || !stage_path(path, sizeof path, "up", B.m->count)) return -1;
    it->size = (uint32_t)len; it->revision = rev; it->records = records;
    ink_sha256(data, len, it->hash);
    if (card_write(path, data, len) != 0) return -1;
    B.m->count++;
    return 0;
}


typedef struct { char name[ITEMS][64]; int count, rc; char root[INK_LAYOUT_PATH_MAX]; } names_t;
static void list_state(void *arg) {
    names_t *names = arg;
    hal_dir_t *d = hal_fs_opendir(names->root);
    if (!d) { names->rc = -1; return; }
    hal_dirent_t ent;
    int got;
    while ((got = hal_fs_readdir(d, &ent)) > 0) {
        if (!strcmp(ent.name, ".") || !strcmp(ent.name, "..") || !strncmp(ent.name, "backup.", 7) || raw_db_file(ent.name)) continue;
        if (ent.is_dir || !ink_layout_name_ok(ent.name) || strlen(ent.name) >= 64 || names->count == ITEMS) { names->rc = -1; break; }
        memcpy(names->name[names->count++], ent.name, strlen(ent.name) + 1);
    }
    if (got < 0) names->rc = -1;
    hal_fs_closedir(d);
}

static size_t manifest_size(void) { return offsetof(manifest_t, item) + B.m->count * sizeof(item_t); }

static uint32_t u32(const uint8_t *p);

typedef struct {
    char member[INK_MEMBER_ID_MAX];
    uint32_t generation, mode, db, state, depth, offset, length, revision;
    manifest_t manifest;
    names_t names;
    hal_dir_t *dirs[9];
    hal_dirent_t entry;
    char relative[128], file[128], path[256], staged[160];
    ink_sha256_t sha;
} capture_t;
static capture_t *C;

static void free_capture(void) {
    if (!C) return;
    while (C->depth) hal_fs_closedir(C->dirs[--C->depth]);
    osal_free(C); C = NULL;
    ink_fs_card_release();
}

/* One collection, state file, directory entry, or 16 KiB of personal content per invocation. */
static int capture_step(void) {
    if (!C) {
        if (ink_fs_card_hold() != INK_FS_OK) return -1;
        C = osal_calloc(1, sizeof *C, OSAL_MEM_EXTERNAL);
        if (!C) { ink_fs_card_release(); return -1; }
        snprintf(C->member, sizeof C->member, "%s", B.j.member); C->generation = B.j.generation;
        C->manifest.magic = MAN_MAGIC; C->manifest.format = 2;
        snprintf(C->manifest.member, sizeof C->manifest.member, "%s", B.j.member);
        memcpy(C->manifest.identity, B.j.identity, 16);
        C->manifest.created_s = osal_wall_us() / 1000000;
        if (!stage_dirs()) return -2;
    }
    memcpy(B.m, &C->manifest, sizeof *B.m);
    int rc = 0;
    if (C->mode == 0) {
        const ink_db_def_t *d = ink_db_def_at((int)C->db++);
        if (!d) C->mode++;
        else if (d->scope == INK_DB_MEMBER) {
            ink_db_t *db = ink_db_open_member(d, B.j.member, B.j.generation);
            if (!db) return -1;
            void *data = NULL; size_t len = 0;
            /* Flush, then yield until durable: do not block the shared executor waiting for a writer. */
            ink_db_flush(db);
            if (ink_db_durable_ver(db) != ink_db_last_ver(db)) { C->db--; ink_db_close(db); return 1; }
            rc = ink_db_export(db, &data, &len);
            if (!rc) {
                uint32_t records = 0;
                for (size_t off = 32; off + 36 <= len;) {
                    const uint8_t *p = (const uint8_t *)data + off;
                    uint16_t bytes; memcpy(&bytes, p + 28, sizeof bytes);
                    records += p[2] == 1; off += 36u + bytes;
                }
                rc = add_memory(ITEM_DB, d->name, data, len, u32((uint8_t *)data + 12), records);
            }
            osal_free(data); ink_db_close(db);
        }
    } else if (C->mode == 1) {
        snprintf(C->names.root, sizeof C->names.root, "/user/m/%s", B.j.member);
        ink_storage_run(list_state, &C->names, true);
        rc = C->names.rc; C->mode++;
    } else if (C->mode == 2) {
        if (C->state < (unsigned)C->names.count) {
            char path[INK_LAYOUT_PATH_MAX]; void *data = NULL; size_t len = 0;
            const char *name = C->names.name[C->state++];
            if (ink_layout_member(path, sizeof path, B.j.member, name) < 0 || ink_storage_load(path, &data, &len)) rc = -1;
            else rc = add_memory(ITEM_STATE, name, data, len, 0, 0);
            ink_storage_free(data);
        } else {
            if (ink_layout_member_files(C->path, sizeof C->path, B.j.member) < 0) return -2;
            C->dirs[0] = hal_fs_opendir(C->path);
            if (!C->dirs[0] && hal_fs_size(C->path) >= 0) return -2;
            C->depth = C->dirs[0] ? 1 : 0; C->mode++;
        }
    } else if (C->mode == 3 && C->file[0]) {
        if (!C->revision) {
            uint32_t revision = ink_fs_revision();
            if (!revision) return 1;
            int64_t len = hal_fs_size(C->path);
            if (revision != ink_fs_revision()) return 1;
            if (len < 0 || len > UINT32_MAX) return -2;
            C->length = (uint32_t)len; C->offset = 0; C->revision = revision; ink_sha256_init(&C->sha);
        }
        size_t n = C->length - C->offset; if (n > CHUNK) n = CHUNK;
        hal_file_t *in = hal_fs_open(C->path, HAL_O_READ);
        hal_file_t *out = hal_fs_open(C->staged, HAL_O_WRITE | HAL_O_CREATE | (C->offset ? HAL_O_APPEND : HAL_O_TRUNC));
        bool ok = in && out && hal_fs_seek(in, C->offset, SEEK_SET) == 0 &&
            hal_fs_read(in, B.buf, n) == (ssize_t)n && hal_fs_write(out, B.buf, n) == (ssize_t)n;
        if (in && hal_fs_close(in) != 0) ok = false;
        if (out) { if (hal_fs_sync(out)) ok = false; if (hal_fs_close(out)) ok = false; }
        if (!ok || hal_fs_size(C->path) != C->length || ink_fs_revision() != C->revision) return -2;
        ink_sha256_update(&C->sha, B.buf, n); C->offset += (uint32_t)n;
        ink_probe_value("backup.collect_offset", C->offset);
        ink_probe_count("backup.collect_blocks");
        if (C->offset == C->length) {
            item_t *it = new_item(ITEM_FILE, C->file);
            if (!it) return -2;
            it->size = C->length; ink_sha256_final(&C->sha, it->hash); B.m->count++;
            C->file[0] = 0;
        }
    } else if (C->mode == 3 && C->depth) {
        int got = hal_fs_readdir(C->dirs[C->depth - 1], &C->entry);
        if (got < 0) return -2;
        if (!got) {
            hal_fs_closedir(C->dirs[--C->depth]);
            char *slash = strrchr(C->relative, '/');
            if (slash) *slash = 0; else C->relative[0] = 0;
        } else {
            size_t suffix = strlen(C->entry.name);
            if (C->entry.name[0] == '.' || (suffix > 5 && !strcmp(C->entry.name + suffix - 5, ".part")) ||
                (suffix > 4 && !strcmp(C->entry.name + suffix - 4, ".tmp"))) return 1;
            int n = snprintf(C->file, sizeof C->file, "%s%s%s", C->relative, C->relative[0] ? "/" : "", C->entry.name);
            if (n < 0 || (size_t)n >= sizeof C->file) return -2;
            char root[INK_LAYOUT_PATH_MAX];
            if (ink_layout_member_files(root, sizeof root, B.j.member) < 0) return -2;
            n = snprintf(C->path, sizeof C->path, "%s/%s", root, C->file);
            if (n < 0 || (size_t)n >= sizeof C->path) return -2;
            if (C->entry.is_dir) {
                if (C->depth == 9) return -2;
                hal_dir_t *d = hal_fs_opendir(C->path); if (!d) return -2;
                C->dirs[C->depth++] = d;
                snprintf(C->relative, sizeof C->relative, "%s", C->file); C->file[0] = 0;
            } else {
                if (!stage_path(C->staged, sizeof C->staged, "up", B.m->count)) return -2;
                C->revision = 0;
            }
        }
    } else {
        char path[160]; uint8_t hash[32];
        stage_path(path, sizeof path, "manifest", 0);
        size_t len = manifest_size(); ink_sha256(B.m, len, hash); hex(hash, 32, B.j.manifest_hash);
        B.j.manifest_size = (uint32_t)len;
        if (card_write(path, B.m, len)) return -2;
        B.j.phase = PH_UPLOAD; B.j.next = B.j.offset = 0;
        return save_journal() ? -1 : 0;
    }
    if (rc) return -2;
    memcpy(&C->manifest, B.m, sizeof *B.m);
    return 1;
}

static void reply(void *ctx, int status, const ink_json_t *j, int result) {
    ink_svc_lock();
    if (!N.pending || N.cookie != (uintptr_t)ctx) { ink_svc_unlock(); return; }
    N.status = status;
    N.reply[0] = 0;
    if (!status && j && result >= 0) {
        size_t n = (size_t)(j->tok[result].end - j->tok[result].start);
        if (n >= sizeof N.reply) N.status = INK_HUB_ERR_REMOTE;
        else { memcpy(N.reply, j->text + j->tok[result].start, n); N.reply[n] = 0; }
        int64_t xfer;
        if (N.receiving && ink_json_int(j, ink_json_find(j, result, "xfer"), &xfer) && xfer > 0 && xfer <= UINT16_MAX) N.xfer = (uint16_t)xfer;
    }
    N.done = true;
    ink_svc_unlock();
    wake(NULL);
}

static int rpc(const char *method, const char *params, ink_json_t *j) {
    if (!actor_valid()) return -1;
    char authenticated[640], identity[33];
    hex(B.j.identity, 16, identity);
    ink_svc_lock();
    bool pending = N.pending, done = N.done;
    int status = N.status;
    if (pending && done) { N.pending = false; N.cookie = 0; }
    ink_svc_unlock();
    if (!pending) {
        size_t len = strlen(params);
        if (!len || params[len - 1] != '}' || len + 50 >= sizeof authenticated) return -1;
        snprintf(authenticated, sizeof authenticated, "%.*s,\"identity\":\"%s\"}", (int)(len - 1), params, identity);
        uintptr_t cookie = ++next_cookie;
        if (!cookie) cookie = ++next_cookie;
        ink_svc_lock();
        N.pending = true; N.done = false; N.cookie = cookie;
        ink_svc_unlock();
        if (ink_hub_call(method, authenticated, 3000000, reply, (void *)cookie) <= 0) return -1;
        return WAIT_IO;
    }
    if (!done) return WAIT_IO;
    if (!actor_valid()) return -1;
    if (status == INK_HUB_ERR_REMOTE) return -2;
    if (status != 0 || ink_json_parse(j, N.reply, strlen(N.reply)) != 0) return -1;
    char received[33];
    if (ink_json_str(j, ink_json_find(j, 0, "identity"), received, sizeof received) != 32 || strcmp(received, identity)) return -2;
    return 0;
}

static bool number(const ink_json_t *j, const char *key, uint32_t *out) {
    int64_t n;
    if (!ink_json_int(j, ink_json_find(j, 0, key), &n) || n < 0 || n > UINT32_MAX) return false;
    *out = (uint32_t)n;
    return true;
}

static void ack(void *ctx, const char *method, const ink_json_t *j, int params) {
    (void)ctx; (void)method;
    int64_t upload, off;
    if (!ink_json_int(j, ink_json_find(j, params, "upload"), &upload) ||
        !ink_json_int(j, ink_json_find(j, params, "offset"), &off) || off < 0 || off > UINT32_MAX) return;
    char received[33];
    if (ink_json_str(j, ink_json_find(j, params, "identity"), received, sizeof received) != 32) return;
    ink_svc_lock();
    char identity[33]; hex(N.identity, 16, identity);
    bool valid = N.bound && N.upload && upload == N.upload && !strcmp(received, identity) &&
        N.generation == ink_member_generation(N.member) && !strcmp(N.member, ink_member_current()->id);
    if (valid && (uint32_t)off >= N.ack) N.ack = (uint32_t)off;
    ink_svc_unlock();
    if (valid) wake(NULL);
}

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t n) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8 * i)); }
static void blob_frame(void *ctx, const uint8_t *d, size_t len) {
    (void)ctx;
    if (len < 16 || d[0] != 6 || d[1]) return;
    ink_svc_lock();
    uint16_t xfer = (uint16_t)(d[10] | d[11] << 8);
    uint32_t off = u32(d + 12);
    bool valid = N.receiving && N.xfer && xfer == N.xfer && N.rx && off == N.offset + N.rx_len &&
        len - 16 <= N.expected - N.rx_len && N.generation == ink_member_generation(N.member) &&
        !strcmp(N.member, ink_member_current()->id);
    if (valid) {
        memcpy(N.rx + N.rx_len, d + 16, len - 16);
        N.rx_len += (uint32_t)(len - 16);
    }
    ink_svc_unlock();
    if (valid) wake(NULL);
}

/* Send one frame per continuation; the protocol acknowledges at 64 KiB boundaries. */
static int upload_block(const char *path, const uint8_t hash[32], uint32_t size, ink_json_t *j) {
    char sha[65], params[400]; hex(hash, 32, sha);
    if (!N.stage) {
        snprintf(params, sizeof params, "{\"name\":\"backup\",\"size\":%u,\"sha256\":\"%s\",\"member\":\"%s\",\"kind\":\"backup\"}", size, sha, B.j.member);
        int called = rpc("blob/put.begin", params, j);
        if (called) return called;
        uint32_t upload, offset;
        if (!number(j, "upload", &upload) || !upload || upload > UINT16_MAX || !number(j, "offset", &offset) || offset > size) return -1;
        ink_svc_lock();
        N.upload = (uint16_t)upload; N.offset = N.ack = offset;
        ink_svc_unlock();
        uint32_t segment = 65536u - offset % 65536u;
        if (segment > size - offset) segment = size - offset;
        N.expected = offset + segment;
        N.stage = offset < size ? 1 : 3;
    }
    if (N.stage == 1) {
        uint32_t n = N.expected - N.offset; if (n > CHUNK) n = CHUNK;
        hal_file_t *f = hal_fs_open(path, HAL_O_READ);
        bool ok = f && hal_fs_seek(f, N.offset, SEEK_SET) == 0 && hal_fs_read(f, B.buf + 16, n) == (ssize_t)n;
        if (f && hal_fs_close(f) != 0) ok = false;
        if (!ok) return -2;
        memset(B.buf, 0, 16); B.buf[0] = 7;
        B.buf[10] = (uint8_t)N.upload; B.buf[11] = (uint8_t)(N.upload >> 8); put32(B.buf + 12, N.offset);
        if (ink_hub_send_frame_for(B.buf, n + 16, INK_HUB_TX_BACKGROUND, B.j.member, B.j.generation, ink_member_epoch()) != 0) return -1;
        N.offset += n;
        if (N.offset < N.expected) return 1;
        N.stage = 2; net_wait();
        return WAIT_IO;
    }
    if (N.stage == 2) {
        ink_svc_lock(); uint32_t got = N.ack; ink_svc_unlock();
        if (got != N.expected) {
            if (!ink_hub_online() || osal_now_us() >= N.until || got > N.expected) return -1;
            return WAIT_IO;
        }
        ink_svc_cancel(&net_at);
        N.offset = B.j.offset = got;
        ink_probe_value("backup.upload_offset", got);
        N.stage = got < size ? 1 : 3;
        uint32_t segment = size - got; if (segment > 65536u) segment = 65536u;
        N.expected = got + segment;
        return 1;
    }
    snprintf(params, sizeof params, "{\"upload\":%u,\"member\":\"%s\"}", N.upload, B.j.member);
    int called = rpc("blob/put.end", params, j);
    if (called) return called;
    char id[80], expected[80]; snprintf(expected, sizeof expected, "sha256:%s", sha);
    if (ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id) < 0 || strcmp(id, expected)) return -2;
    net_reset();
    return 0;
}

/* Only a completed block is synced and journaled. The receive buffer survives yielding. */
static int download_block(const char *path, const char *sha, uint32_t size, ink_json_t *j) {
    int64_t have = hal_fs_size(path);
    if (have < 0) have = 0;
    if (have > size) { hal_fs_remove(path); have = 0; }
    if ((uint32_t)have < size) {
        uint32_t n = size - (uint32_t)have; if (n > CHUNK) n = CHUNK;
        if (!N.stage) {
            if (!N.receiving) {
                uint8_t *rx = osal_malloc(CHUNK, OSAL_MEM_EXTERNAL);
                if (!rx) return -1;
                ink_svc_lock();
                N.rx = rx; N.receiving = true; N.xfer = 0; N.rx_len = 0;
                N.offset = (uint32_t)have; N.expected = n;
                ink_svc_unlock();
            }
            char params[320];
            snprintf(params, sizeof params, "{\"id\":\"sha256:%s\",\"offset\":%u,\"len\":%u,\"member\":\"%s\"}", sha, (uint32_t)have, n, B.j.member);
            int rc = rpc("blob/fetch", params, j);
            if (rc) return rc;
            uint32_t total = 0; char got_sha[65];
            if (!number(j, "size", &total) || total != size ||
                ink_json_str(j, ink_json_find(j, 0, "sha256"), got_sha, sizeof got_sha) < 0 || strcmp(got_sha, sha)) return -2;
            N.stage = 1;
            net_wait();
        }
        ink_svc_lock(); uint32_t got = N.rx_len; ink_svc_unlock();
        if (got != n) {
            if (!ink_hub_online() || osal_now_us() >= N.until) return -1;
            return WAIT_IO;
        }
        ink_svc_lock(); N.receiving = false; ink_svc_unlock();
        ink_svc_cancel(&net_at);
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | (have ? HAL_O_APPEND : HAL_O_TRUNC));
        bool ok = f && hal_fs_write(f, N.rx, n) == (ssize_t)n && hal_fs_sync(f) == 0;
        if (f && hal_fs_close(f) != 0) ok = false;
        if (!ok) return -2;
        osal_free(N.rx); N.rx = NULL; N.stage = 0;
        B.j.offset = (uint32_t)have + n;
        ink_probe_value("backup.download_offset", B.j.offset);
        if (B.j.offset < size) return 1;
    } else if (!size && have == 0 && hal_fs_size(path) < 0 && card_write(path, "", 0)) return -2;
    uint8_t hash[32]; char actual[65];
    int hashed = hash_file(path, hash, NULL);
    if (hashed) return hashed > 0 ? 1 : -2;
    hex(hash, 32, actual);
    if (strcmp(actual, sha)) { hal_fs_remove(path); return -2; }
    net_reset();
    return 0;
}

static bool supported(const item_t *it) {
    if (it->kind != ITEM_DB) return true;
    const ink_db_def_t *d = ink_db_find_def(it->name);
    return d && d->scope == INK_DB_MEMBER && strcmp(it->name, "activity.log");
}

static bool safe_name(const char *name, bool nested) {
    size_t n = strlen(name), segment = 0;
    if (!n || n > (nested ? 123u : 63u) || name[0] == '.') return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c < 32 || c == 127 || strchr("\\:*?\"<>|", c)) return false;
        if (c == '/') {
            if (!nested || !segment || name[i + 1] == '.' || !name[i + 1]) return false;
            segment = 0;
        } else if (++segment >= 64) return false;
    }
    return true;
}

static int load_manifest(const char *which, bool remote) {
    if (B.j.manifest_size < offsetof(manifest_t, item) || B.j.manifest_size > sizeof *B.m) return -1;
    char path[160]; stage_path(path, sizeof path, which, 0);
    memset(B.m, 0, sizeof *B.m);
    if (card_load(path, B.m, B.j.manifest_size) != 0) return -1;
    {
        uint8_t h[32]; char sha[65]; ink_sha256(B.m, B.j.manifest_size, h); hex(h, 32, sha);
        if (remote ? strcmp(sha, B.j.manifest_hash) != 0 : memcmp(h, B.j.preview_hash, 32) != 0) return -1;
    }
    if (B.m->magic != MAN_MAGIC || B.m->format != 2 || B.m->count > ITEMS || manifest_size() != B.j.manifest_size ||
        !memchr(B.m->member, 0, sizeof B.m->member) || strcmp(B.m->member, B.j.member) || memcmp(B.m->identity, B.j.identity, 16)) return -1;
    for (unsigned i = 0; i < B.m->count; i++) {
        item_t *it = &B.m->item[i];
        if (!memchr(it->name, 0, sizeof it->name) || !safe_name(it->name, it->kind == ITEM_FILE) ||
            it->kind < ITEM_DB || it->kind > ITEM_FILE || (it->kind != ITEM_FILE && it->size > SMALL_MAX) || zero_hash(it->hash)) return -1;
        if (it->kind == ITEM_DB) {
            const ink_db_def_t *d = ink_db_find_def(it->name);
            if (d && d->scope != INK_DB_MEMBER) return -1;
        } else if (it->kind == ITEM_STATE && (!strncmp(it->name, "backup.", 7) || raw_db_file(it->name))) return -1;
        for (unsigned k = 0; k < i; k++)
            if (B.m->item[k].kind == it->kind && !strcmp(B.m->item[k].name, it->name)) return -1;
        if (remote) memset(it->before, 0, sizeof it->before);
    }
    return 0;
}

static int current_hash(const item_t *it, uint8_t hash[32], uint32_t *revision) {
    memset(hash, 0, 32);
    if (revision) *revision = 0;
    if (!supported(it)) return 0;
    if (it->kind == ITEM_DB) {
        const ink_db_def_t *d = ink_db_find_def(it->name);
        ink_db_t *db = ink_db_open_member(d, B.j.member, B.j.generation);
        if (!db) return -1;
        void *data = NULL; size_t len = 0;
        ink_db_flush(db);
        if (ink_db_durable_ver(db) != ink_db_last_ver(db)) { ink_db_close(db); return 1; }
        int rc = ink_db_export(db, &data, &len);
        if (!rc) { ink_sha256(data, len, hash); if (revision) *revision = u32((uint8_t *)data + 12); }
        osal_free(data); ink_db_close(db);
        return rc;
    }
    char path[256]; if (!physical(it, path, sizeof path)) return -1;
    if (it->kind == ITEM_FILE) {
        uint32_t stable = ink_fs_revision();
        if (!stable) return 1;
        int64_t len = hal_fs_size(path);
        if (stable != ink_fs_revision()) return 1;
        if (len < 0) { if (revision) *revision = stable; return 0; }
        int rc = hash_file(path, hash, NULL);
        if (!rc && revision) *revision = H.revision;
        return rc;
    }
    void *data = NULL; size_t len = 0;
    int rc = ink_storage_load(path, &data, &len);
    if (rc == INK_STORAGE_NO_FILE) return 0;
    if (rc) return -1;
    ink_sha256(data, len, hash);
    ink_storage_free(data);
    return 0;
}

static void event(const char *id);

static int prepare_preview(void) {
    if (B.j.next < B.m->count) {
        item_t *it = &B.m->item[B.j.next];
        int rc = current_hash(it, it->before, NULL);
        if (rc) return rc > 0 ? 1 : -2;
        if (!zero_hash(it->before) && memcmp(it->before, it->hash, 32)) B.j.conflicts++;
        B.j.next++;
    } else {
        uint8_t nonce[16]; hal_random_fill(nonce, sizeof nonce); hex(nonce, sizeof nonce, B.j.preview);
        B.j.phase = PH_PREVIEW; B.j.next = B.j.offset = 0;
    }
    char path[160]; stage_path(path, sizeof path, "preview", 0);
    if (card_write(path, B.m, manifest_size())) return -2;
    ink_sha256(B.m, manifest_size(), B.j.preview_hash);
    if (save_journal()) return -1;
    if (B.j.phase == PH_PREVIEW) event("backup.changed");
    return 1;
}

static int apply_item(unsigned index) {
    item_t *it = &B.m->item[index];
    if (!supported(it)) return 0;
    uint8_t now[32]; uint32_t version;
    int checked = current_hash(it, now, &version);
    if (checked) return checked;
    if (!memcmp(now, it->hash, 32)) return 0; /* A cut after installation but before the journal commit. */
    if (memcmp(now, it->before, 32)) return -3;
    char staged[160], target[256]; stage_path(staged, sizeof staged, "down", index);
    if (!physical(it, target, sizeof target)) return -2;
    if (it->kind == ITEM_FILE) {
        char virt[INK_FS_PATH_MAX]; snprintf(virt, sizeof virt, "/me/%.123s", it->name);
        int rc = ink_fs_restore_if_revision(B.j.member, B.j.generation, index, virt, version);
        return rc == 0 ? 0 : rc == INK_FS_ERR_EXISTS ? -3 : rc == INK_FS_ERR_BUSY ? -1 : -2;
    }
    void *data = osal_malloc(it->size ? it->size : 1, OSAL_MEM_EXTERNAL);
    if (!data) return -1;
    int rc = card_load(staged, data, it->size);
    if (!rc && it->kind == ITEM_DB) {
        ink_db_t *db = ink_db_open_member(ink_db_find_def(it->name), B.j.member, B.j.generation);
        rc = db ? ink_db_import(db, data, it->size, version) : -1;
        ink_db_close(db);
    } else if (!rc) {
        stored_t done = {0};
        rc = ink_storage_restore(target, data, it->size, it->before, stored, &done);
        if (!rc) {
            while (!__atomic_load_n(&done.done, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
            rc = done.result;
        }
    }
    osal_free(data);
    return rc == -3 ? -3 : rc ? -2 : 0;
}

static bool resources(void) {
    if (!B.m) B.m = osal_malloc(sizeof *B.m, OSAL_MEM_EXTERNAL);
    if (!B.buf) B.buf = osal_malloc(CHUNK + 16, OSAL_MEM_EXTERNAL);
    return B.m && B.buf;
}
static void free_resources(void) {
    osal_free(B.m); B.m = NULL;
    osal_free(B.buf); B.buf = NULL;
}

static void event(const char *id) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!a) return;
    ink_args_init(a);
    ink_args_str(a, "member", B.j.member);
    ink_args_str(a, "phase", PH_NAMES[B.j.phase]);
    ink_cap_emit_for(id, a, B.j.member, B.j.generation);
    osal_free(a);
}

static void step(void *arg);
static void wake(void *arg) { (void)arg; ink_exec_post(INK_EXEC_BG, &B.job, step, NULL); }
static bool active(void) { return B.j.phase == PH_CAPTURE || B.j.phase == PH_UPLOAD || B.j.phase == PH_FETCH || B.j.phase == PH_PREPARE || B.j.phase == PH_CHECK || B.j.phase == PH_RESTORE; }
static void schedule(void) { if (pending_for(NULL)) ink_svc_at(&B.retry, osal_now_us() + RETRY_US, wake, NULL); }

static void mark_failed(const char *reason) {
    free_capture();
    B.j.phase = PH_FAILED;
    snprintf(B.j.reason, sizeof B.j.reason, "%s", reason);
    save_journal();
    if ((*task_id())) { ink_task_end((*task_id()), INK_TASK_FAILED, reason, NULL); (*task_id()) = 0; }
    event("backup.failed");
}

static void acknowledge_manifest(void) {
    for (unsigned k = 0; k < B.m->count; k++) if (B.m->item[k].kind == ITEM_DB && supported(&B.m->item[k])) {
        ink_db_t *db = ink_db_open_member(ink_db_find_def(B.m->item[k].name), B.j.member, B.j.generation);
        if (db && B.m->item[k].revision <= ink_db_last_ver(db)) ink_db_ack(db, B.m->item[k].revision);
        ink_db_close(db);
    }
}

static int upload_step(ink_json_t *j) {
    if (load_manifest("manifest", true)) return -2;
    unsigned i = B.j.next;
    if (i > B.m->count + 1) return -2;
    if (i <= B.m->count) {
        char path[160]; uint8_t h[32]; uint32_t len;
        if (i < B.m->count) {
            stage_path(path, sizeof path, "up", i);
            memcpy(h, B.m->item[i].hash, 32); len = B.m->item[i].size;
        } else {
            stage_path(path, sizeof path, "manifest", 0);
            ink_sha256(B.m, manifest_size(), h); len = B.j.manifest_size;
        }
        /* The hub verifies the whole hash at put.end; each step reads only its own segment. */
        if (hal_fs_size(path) != len) return -2;
        int rc = upload_block(path, h, len, j);
        if (!rc) { B.j.next++; B.j.offset = 0; if (save_journal()) return -1; }
        return rc ? rc : 1;
    }
    char params[320];
    snprintf(params, sizeof params, "{\"member\":\"%s\",\"blob\":\"sha256:%s\",\"size\":%u,\"sha256\":\"%s\"}",
             B.j.member, B.j.manifest_hash, B.j.manifest_size, B.j.manifest_hash);
    int called = rpc("backup/commit", params, j);
    if (called) return called;
    char got[65];
    if (!ink_json_eq(j, ink_json_find(j, 0, "received"), "true") ||
        ink_json_str(j, ink_json_find(j, 0, "snapshot"), got, sizeof got) < 0 || strcmp(got, B.j.manifest_hash)) return -2;
    B.j.received = 1; B.j.durable = ink_json_eq(j, ink_json_find(j, 0, "durable"), "true");
    B.j.phase = PH_SAVED; B.j.saved_s = osal_wall_us() / 1000000;
    if (save_journal()) { B.j.phase = PH_UPLOAD; return -1; }
    acknowledge_manifest();
    if ((*task_id())) { ink_task_end((*task_id()), INK_TASK_DONE, "已备份", NULL); (*task_id()) = 0; }
    event("backup.changed"); ink_probe_count("backup.saved");
    return 0;
}

static int restore_step(ink_json_t *j) {
    if (B.j.phase == PH_FETCH) {
        if (!B.j.manifest_hash[0]) {
            char params[96], sha[65]; uint32_t len;
            snprintf(params, sizeof params, "{\"member\":\"%s\"}", B.j.member);
            int rc = rpc("backup/latest", params, j);
            if (rc) return rc;
            if (!number(j, "size", &len) || len < offsetof(manifest_t, item) || len > sizeof(manifest_t) ||
                ink_json_str(j, ink_json_find(j, 0, "sha256"), sha, sizeof sha) < 0 || !hash_ok(sha)) return -2;
            snprintf(B.j.manifest_hash, sizeof B.j.manifest_hash, "%s", sha); B.j.manifest_size = len;
            return save_journal() ? -1 : 1;
        }
        char path[160]; stage_path(path, sizeof path, "remote", 0);
        int rc = download_block(path, B.j.manifest_hash, B.j.manifest_size, j);
        if (rc) return rc;
        if (load_manifest("remote", true)) return -2;
        stage_path(path, sizeof path, "preview", 0);
        if (card_write(path, B.m, manifest_size())) return -2;
        ink_sha256(B.m, manifest_size(), B.j.preview_hash);
        B.j.phase = PH_PREPARE; B.j.next = B.j.offset = B.j.conflicts = 0; H.valid = false;
        return save_journal() ? -1 : 1;
    }
    if (load_manifest("preview", false)) {
        /* Before approval, a cut between the card write and journal commit can restart preparation. */
        if (B.j.phase != PH_PREPARE || load_manifest("remote", true)) return -2;
        char path[160]; stage_path(path, sizeof path, "preview", 0);
        if (card_write(path, B.m, manifest_size())) return -2;
        ink_sha256(B.m, manifest_size(), B.j.preview_hash);
        B.j.next = B.j.conflicts = 0; H.valid = false;
        return save_journal() ? -1 : 1;
    }
    if (B.j.next > B.m->count) return -2;
    if (B.j.phase == PH_PREPARE) return prepare_preview();
    if (B.j.phase == PH_CHECK) {
        if (B.j.next == B.m->count) {
            B.j.phase = PH_RESTORE; B.j.next = 0; H.valid = false;
        } else {
            item_t *it = &B.m->item[B.j.next]; uint8_t now[32];
            int rc = current_hash(it, now, NULL);
            if (rc) return rc > 0 ? 1 : -1;
            if (memcmp(now, it->before, 32)) return -3;
            B.j.next++;
        }
        return save_journal() ? -1 : 1;
    }
    if (B.j.next == B.m->count) {
        B.j.phase = PH_DONE; B.j.offset = 0;
        if (save_journal()) { B.j.phase = PH_RESTORE; return -1; }
        if ((*task_id())) { ink_task_end((*task_id()), INK_TASK_DONE, "已恢复，请重新开机", NULL); (*task_id()) = 0; }
        event("backup.changed"); ink_probe_count("backup.restored");
        return 0;
    }
    item_t *it = &B.m->item[B.j.next];
    int rc = 0;
    if (supported(it)) {
        if (!B.j.subphase) {
            uint8_t now[32];
            rc = current_hash(it, now, NULL);
            if (rc) return rc > 0 ? 1 : -1;
            if (!memcmp(now, it->hash, 32)) goto installed;
            if (memcmp(now, it->before, 32)) return -3;
            B.j.subphase = 1; H.valid = false;
        }
        if (B.j.subphase == 1) {
            char path[160], sha[65]; stage_path(path, sizeof path, "down", B.j.next); hex(it->hash, 32, sha);
            rc = download_block(path, sha, it->size, j);
            if (rc) return rc;
            if (it->kind == ITEM_FILE) {
                char virt[INK_FS_PATH_MAX]; snprintf(virt, sizeof virt, "/me/%.123s", it->name);
                char *slash = strrchr(virt, '/');
                if (slash && slash != virt + 3) { *slash = 0; if (ink_fs_mkdir_member_generation(B.j.member, B.j.generation, virt)) return -2; }
            }
            B.j.subphase = 2; H.valid = false;
        }
        rc = apply_item(B.j.next);
    }
installed:
    if (!rc) {
        B.j.next++; B.j.offset = B.j.subphase = 0; H.valid = false;
        if (save_journal()) return -1;
    }
    return rc < 0 ? rc : 1;
}

static void step(void *arg) {
    (void)arg;
    const char *member = ink_member_current()->id;
    if (C && (strcmp(C->member, member) || C->generation != ink_member_generation(member) || ink_jobs_held() || !ink_member_space_open())) free_capture();
    if (!pending_for(member)) { net_reset(); schedule(); return; }
    if (!load_journal(member)) { net_reset(); remember(member, false); schedule(); return; }
    if (!actor_valid() || ink_jobs_held()) { net_reset(); schedule(); return; }
    net_bind();
    if (B.j.phase == PH_SAVED) {
        int polled = 0;
        if (!B.j.durable && ink_hub_online()) {
            ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
            char params[160];
            snprintf(params, sizeof params, "{\"id\":\"sha256:%s\",\"member\":\"%s\"}", B.j.manifest_hash, B.j.member);
            polled = j ? rpc("blob/status", params, j) : -1;
            if (!polled && ink_json_eq(j, ink_json_find(j, 0, "durable"), "true")) { B.j.durable = 1; save_journal(); }
            osal_free(j);
        }
        if (polled == WAIT_IO) return;
        net_reset();
        if (ink_fs_card_hold() == INK_FS_OK) {
            if (resources() && load_manifest("manifest", true) == 0) { acknowledge_manifest(); remember(member, false); }
            free_resources(); ink_fs_card_release();
        }
        schedule(); return;
    }
    if (!active()) { remember(member, false); schedule(); return; }
    if (!*task_id()) *task_id() = ink_task_begin_for(B.j.phase == PH_CAPTURE || B.j.phase == PH_UPLOAD ? "backup" : "restore", "继续备份或恢复资料", B.j.member, B.j.generation, NULL, NULL);
    if (!ink_member_io_begin(B.j.member, B.j.generation)) return;
    if (ink_fs_card_hold() != INK_FS_OK) { schedule(); ink_member_io_end(B.j.member); return; }
    int rc = -1;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (resources() && j) {
        rc = B.j.phase == PH_CAPTURE ? capture_step() : B.j.phase == PH_UPLOAD ? upload_step(j) : restore_step(j);
        if ((*task_id()) && B.m->count) ink_task_progress((*task_id()), (int)(B.j.next * 100 / (B.m->count + 1)), B.j.phase == PH_UPLOAD ? "sending" : "writing");
    }
    if (rc < 0) net_reset();
    if (rc == 1 && active() && B.j.phase != PH_CAPTURE) save_journal();
    if (rc == -2) mark_failed("备份资料校验或写入失败，原件仍保留");
    else if (rc == -3) mark_failed("预览后资料有改动，请重新预览");
    if (rc < 0 && active()) ink_probe_count("backup.retries");
    if (B.j.phase != PH_CAPTURE) free_capture();
    osal_free(j); free_resources(); ink_fs_card_release();
    ink_member_io_end(B.j.member);
    if (rc >= 0 && rc != WAIT_IO && active()) wake(NULL);
    else if (rc != WAIT_IO) schedule();
}

static bool for_call(ink_call_t *c) {
    uint32_t generation;
    const char *member = ink_cap_call_member(c, &generation);
    if (C && (strcmp(C->member, member) || C->generation != generation)) free_capture();
    if (!generation || generation != ink_member_generation(member) || strcmp(member, ink_member_current()->id) || !ink_member_space_open()) {
        c->rc.status = INK_RC_STALE; return false;
    }
    if (!load_journal(member)) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "备份记录暂时读不出来，请稍后再试"); return false; }
    if (!B.j.member[0]) {
        snprintf(B.j.member, sizeof B.j.member, "%s", member); B.j.generation = generation;
        if (ink_member_durable_id(member, generation, B.j.identity)) { c->rc.status = INK_RC_FAILED; return false; }
    }
    return true;
}

static void result(ink_call_t *c, bool include_items) {
    char *out = osal_malloc(RESULT_CAP, OSAL_MEM_EXTERNAL);
    if (!out) { c->rc.status = INK_RC_FAILED; return; }
    ink_jw_t w; ink_jw_init(&w, out, RESULT_CAP);
    ink_jw_raw(&w, "{\"member\":"); ink_jw_str(&w, B.j.member);
    ink_jw_raw(&w, ",\"phase\":"); ink_jw_str(&w, PH_NAMES[B.j.phase]);
    ink_jw_raw(&w, ",\"snapshot\":"); ink_jw_str(&w, B.j.manifest_hash);
    ink_jw_raw(&w, ",\"preview\":"); ink_jw_str(&w, B.j.phase == PH_PREVIEW ? B.j.preview : "");
    ink_jw_raw(&w, ",\"received\":"); ink_jw_raw(&w, B.j.received ? "true" : "false");
    ink_jw_raw(&w, ",\"durable\":"); ink_jw_raw(&w, B.j.durable ? "true" : "false");
    bool collecting = C && B.j.phase == PH_CAPTURE && !strcmp(C->member, B.j.member);
    ink_jw_raw(&w, ",\"next\":"); ink_jw_int(&w, collecting ? C->manifest.count : B.j.next);
    ink_jw_raw(&w, ",\"offset\":"); ink_jw_int(&w, collecting ? C->offset : B.j.offset);
    ink_jw_raw(&w, ",\"conflicts\":"); ink_jw_int(&w, B.j.conflicts);
    ink_jw_raw(&w, ",\"saved_s\":"); ink_jw_int(&w, B.j.saved_s);
    ink_jw_raw(&w, ",\"reason\":"); ink_jw_str(&w, B.j.reason);
    ink_jw_raw(&w, ",\"restart_required\":"); ink_jw_raw(&w, B.j.phase == PH_DONE ? "true" : "false");
    if (include_items && B.m) {
        ink_jw_raw(&w, ",\"created_s\":"); ink_jw_int(&w, B.m->created_s);
        ink_jw_raw(&w, ",\"items\":[");
        for (unsigned i = 0; i < B.m->count; i++) {
            item_t *it = &B.m->item[i];
            ink_jw_raw(&w, i ? ",{\"name\":" : "{\"name\":"); ink_jw_str(&w, it->name);
            ink_jw_raw(&w, ",\"kind\":"); ink_jw_str(&w, it->kind == ITEM_DB ? "collection" : it->kind == ITEM_STATE ? "state" : "file");
            ink_jw_raw(&w, ",\"size\":"); ink_jw_int(&w, it->size);
            ink_jw_raw(&w, ",\"revision\":"); ink_jw_int(&w, it->revision);
            ink_jw_raw(&w, ",\"records\":"); ink_jw_int(&w, it->records);
            ink_jw_raw(&w, ",\"result\":");
            ink_jw_str(&w, !strcmp(it->name, "activity.log") && it->kind == ITEM_DB ? "kept_local_audit" : !supported(it) ? "collection_unavailable" : B.j.phase == PH_DONE || ((B.j.phase == PH_RESTORE || (B.j.phase == PH_FAILED && B.j.preview[0])) && i < B.j.next) ? "restored" :
                       B.j.phase == PH_FAILED && B.j.preview[0] ? "not_restored" :
                       B.j.phase == PH_PREVIEW && !zero_hash(it->before) && memcmp(it->before, it->hash, 32) ? "replace" : "included");
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]");
    }
    ink_jw_raw(&w, ",\"not_restored\":[\"设备设置、家人身份和密码：请重新设置\",\"网络和家庭凭据：请重新加入\",\"系统、字体和缓存：由系统提供\",\"家庭共享内容：按需重新下载\",\"已发送的短信、已执行的操作和过期提醒：不重放；保留本机 AI 操作记录\"],\"extra_local_files\":\"保留备份清单以外的本地文件\"}");
    if (!w.overflow) ink_cap_result(c, out); else c->rc.status = INK_RC_FAILED;
    osal_free(out);
}

static void cap_status(ink_call_t *c) {
    if (!for_call(c)) return;
    if (B.j.phase == PH_SAVED && !B.j.durable && ink_hub_online()) {
        remember(B.j.member, true);
        wake(NULL);
    }
    result(c, false);
}

static void cap_manifest(ink_call_t *c) {
    if (!for_call(c)) return;
    if (B.j.phase == PH_NONE || B.j.phase == PH_CAPTURE || !B.j.manifest_hash[0]) { result(c, false); return; }
    if (ink_fs_card_hold() != INK_FS_OK) { c->rc.status = INK_RC_FAILED; return; }
    bool preview = B.j.phase == PH_PREPARE || B.j.phase == PH_PREVIEW || B.j.phase == PH_CHECK || B.j.phase == PH_RESTORE || B.j.phase == PH_DONE || (B.j.phase == PH_FAILED && B.j.preview[0]);
    if (!resources() || load_manifest(preview ? "preview" : "manifest", !preview)) c->rc.status = INK_RC_FAILED;
    else result(c, true);
    free_resources(); ink_fs_card_release();
}

static void cap_backup(ink_call_t *c) {
    if (!for_call(c)) return;
    if (active()) { c->rc.status = INK_RC_QUEUED; result(c, false); schedule(); return; }
    if (ink_fs_card_hold() != INK_FS_OK) {
        c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "请插入存储卡后备份"); return;
    }
    ink_fs_card_release(); free_capture(); net_reset(); H.valid = false;
    B.j.phase = PH_CAPTURE; B.j.next = B.j.offset = B.j.subphase = B.j.received = B.j.durable = B.j.conflicts = 0;
    B.j.reason[0] = B.j.preview[0] = B.j.manifest_hash[0] = 0; B.j.manifest_size = 0;
    /* Persist intent before doing any capture or replacing staging content. */
    if (save_journal()) { c->rc.status = INK_RC_FAILED; return; }
    (*task_id()) = ink_task_begin_for("backup", "备份资料", B.j.member, B.j.generation, NULL, NULL);
    c->rc.status = INK_RC_QUEUED; result(c, false); schedule();
}

static void cap_preview(ink_call_t *c) {
    if (!for_call(c)) return;
    if (B.j.phase == PH_PREVIEW) { cap_manifest(c); return; }
    if (active()) { c->rc.status = INK_RC_QUEUED; result(c, false); schedule(); return; }
    if (ink_fs_card_hold() != INK_FS_OK) { c->rc.status = INK_RC_FAILED; return; }
    if (!stage_dirs()) { ink_fs_card_release(); c->rc.status = INK_RC_FAILED; return; }
    char path[160]; stage_path(path, sizeof path, "remote", 0); hal_fs_remove(path);
    for (unsigned i = 0; i < ITEMS; i++) { stage_path(path, sizeof path, "down", i); hal_fs_remove(path); }
    ink_fs_card_release();
    net_reset(); H.valid = false;
    B.j.manifest_hash[0] = 0; B.j.manifest_size = 0;
    B.j.phase = PH_FETCH; B.j.next = B.j.offset = B.j.subphase = B.j.conflicts = B.j.received = B.j.durable = 0;
    B.j.preview[0] = B.j.reason[0] = 0;
    if (save_journal()) c->rc.status = INK_RC_FAILED;
    else { c->rc.status = INK_RC_QUEUED; result(c, false); schedule(); }
}

static void cap_apply(ink_call_t *c) {
    if (!for_call(c)) return;
    char preview[33]; ink_args_get_str(&c->args, "preview", preview, sizeof preview);
    if (B.j.phase != PH_PREVIEW || strcmp(preview, B.j.preview)) { c->rc.status = INK_RC_STALE; return; }
    if (ink_fs_card_hold() != INK_FS_OK) { c->rc.status = INK_RC_FAILED; return; }
    bool ok = resources() && load_manifest("preview", false) == 0;
    if (!ok) {
        c->rc.status = INK_RC_STALE; B.j.phase = PH_FAILED;
        snprintf(B.j.reason, sizeof B.j.reason, "预览后资料有改动，请重新预览"); save_journal();
    } else {
        B.j.phase = PH_CHECK; B.j.next = B.j.offset = B.j.subphase = 0; H.valid = false;
        if (save_journal()) c->rc.status = INK_RC_FAILED;
        else {
            (*task_id()) = ink_task_begin_for("restore", "恢复资料", B.j.member, B.j.generation, NULL, NULL);
            c->rc.status = INK_RC_QUEUED; result(c, true); schedule();
        }
    }
    free_resources(); ink_fs_card_release();
}

static const ink_cap_t CAPS[] = {
    {.id="backup.status", .title="查看这位成员的备份、上传回执和恢复进度", .effect=INK_FX_READ, .child_ok=true, .exec=INK_EXEC_BG, .run=cap_status},
    {.id="backup.manifest", .title="查看备份清单、覆盖项目和未恢复项目", .effect=INK_FX_READ, .child_ok=true, .exec=INK_EXEC_BG, .run=cap_manifest},
    {.id="backup.now", .title="备份这位成员的资料", .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_BG, .offline=INK_OFFLINE_QUEUE, .run=cap_backup},
    {.id="restore.preview", .title="预览恢复这位成员的备份和将被替换的资料", .effect=INK_FX_READ, .child_ok=true, .exec=INK_EXEC_BG, .run=cap_preview},
    {.id="restore.apply", .title="按预览覆盖这位成员的资料", .effect=INK_FX_LOCAL_DESTRUCTIVE, .child_ok=true,
     .ask_by_hand=true, .parent_pin=true, .ask="恢复预览中的备份，将替换这位成员的现有资料。请先核对备份清单。",
     .input="{\"type\":\"object\",\"properties\":{\"preview\":{\"type\":\"string\",\"maxLength\":32}},\"required\":[\"preview\"]}",
     .exec=INK_EXEC_BG, .run=cap_apply},
    {.id="backup.changed", .title="这位成员的备份或恢复进度有变化", .form=INK_FORM_EVENT, .effect=INK_FX_READ, .child_ok=true},
    {.id="backup.failed", .title="这位成员的备份或恢复没有完成", .form=INK_FORM_EVENT, .effect=INK_FX_READ, .child_ok=true},
};

int ink_backup_init(void) {
    ink_hub_on_blob(blob_frame, NULL);
    if (ink_hub_subscribe("blob/put.ack", ack, NULL) != 0) return -1;
    return ink_cap_register(CAPS, sizeof CAPS / sizeof CAPS[0]);
}
void ink_backup_start(void) {
    if (B.started) return;
    B.started = true;
    for (int i = 0;; i++) {
        const ink_member_t *m = ink_member_at(i);
        if (!m) break;
        char path[INK_LAYOUT_PATH_MAX];
        if (state_path(path, m->id) && ink_storage_size(path) > 0) remember(m->id, true);
    }
    if (pending_for(NULL)) wake(NULL);
}
