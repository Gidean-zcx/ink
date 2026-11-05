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

enum { ITEM_DB = 1, ITEM_STATE, ITEM_FILE };
enum { PH_NONE, PH_UPLOAD, PH_SAVED, PH_FETCH, PH_PREVIEW, PH_RESTORE, PH_DONE, PH_FAILED };
static const char *const PH_NAMES[] = {"none", "uploading", "saved", "fetching", "preview", "restoring", "done", "failed"};

typedef ink_backup_item_t item_t;
typedef ink_backup_manifest_t manifest_t;

typedef struct {
    uint32_t magic, format, phase, generation, next, offset;
    char member[INK_MEMBER_ID_MAX];
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
    int reply_done, reply_status;
    char reply[1024];
    uint16_t upload, xfer;
    uint32_t ack, rx_len, want_off;
    bool receiving;
    char rx_member[INK_MEMBER_ID_MAX];
    uint32_t rx_generation;
    struct { char member[INK_MEMBER_ID_MAX]; uint32_t generation; int id; } tasks[INK_MEMBERS_MAX];
    int no_task;
    struct { char member[INK_MEMBER_ID_MAX]; bool pending; } known[INK_MEMBERS_MAX];
    bool started;
} B INK_PSRAM_BSS;

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
    B.j.magic = STATE_MAGIC; B.j.format = 1;
    B.j.crc = ink_crc32(0, &B.j, offsetof(journal_t, crc));
    int rc = state_path(path, B.j.member) ? write_state_file(path, &B.j, sizeof B.j) : -1;
    if (!rc) remember(B.j.member, B.j.phase == PH_UPLOAD || B.j.phase == PH_FETCH || B.j.phase == PH_RESTORE);
    return rc;
}

static bool load_journal(const char *member) {
    char path[INK_LAYOUT_PATH_MAX];
    journal_t j = {0};
    if (!state_path(path, member)) return false;
    int n = ink_storage_read(path, &j, sizeof j);
    if (n < 0) { memset(&B.j, 0, sizeof B.j); return ink_storage_size(path) < 0; }
    if (n != sizeof j || j.magic != STATE_MAGIC || j.format != 1 || j.phase > PH_FAILED ||
        j.crc != ink_crc32(0, &j, offsetof(journal_t, crc)) || memchr(j.member, 0, sizeof j.member) == NULL ||
        strcmp(j.member, member) || j.next > ITEMS + 1 ||
        !memchr(j.manifest_hash, 0, sizeof j.manifest_hash) || !memchr(j.preview, 0, sizeof j.preview) ||
        !memchr(j.reason, 0, sizeof j.reason)) return false;
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

static int hash_file(const char *path, uint8_t hash[32], uint32_t *size) {
    int64_t len = hal_fs_size(path);
    if (len < 0 || len > UINT32_MAX) return -1;
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return -1;
    ink_sha256_t sha;
    ink_sha256_init(&sha);
    int rc = 0;
    for (uint32_t off = 0; off < (uint32_t)len;) {
        size_t n = (uint32_t)len - off;
        if (n > CHUNK) n = CHUNK;
        if (hal_fs_read(f, B.buf, n) != (ssize_t)n) { rc = -1; break; }
        ink_sha256_update(&sha, B.buf, n);
        off += (uint32_t)n;
    }
    if (hal_fs_size(path) != len || hal_fs_close(f) != 0) rc = -1;
    if (!rc) { ink_sha256_final(&sha, hash); if (size) *size = (uint32_t)len; }
    return rc;
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

static int copy_personal(const char *name) {
    item_t *it = new_item(ITEM_FILE, name);
    char from[256], to[160];
    if (!it || !physical(it, from, sizeof from) || !stage_path(to, sizeof to, "up", B.m->count)) return -1;
    int64_t len = hal_fs_size(from);
    if (len < 0 || len > UINT32_MAX) return -1;
    hal_file_t *in = hal_fs_open(from, HAL_O_READ), *out = hal_fs_open(to, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    int rc = in && out ? 0 : -1;
    ink_sha256_t sha; ink_sha256_init(&sha);
    for (uint32_t off = 0; !rc && off < (uint32_t)len;) {
        size_t n = (uint32_t)len - off; if (n > CHUNK) n = CHUNK;
        if (hal_fs_read(in, B.buf, n) != (ssize_t)n || hal_fs_write(out, B.buf, n) != (ssize_t)n) rc = -1;
        else { ink_sha256_update(&sha, B.buf, n); off += (uint32_t)n; }
    }
    if (in && hal_fs_close(in) != 0) rc = -1;
    if (out) { if (hal_fs_sync(out) != 0) rc = -1; if (hal_fs_close(out) != 0) rc = -1; }
    if (hal_fs_size(from) != len) rc = -1;
    if (!rc) {
        it->size = (uint32_t)len;
        ink_sha256_final(&sha, it->hash);
        /* Catch a writer changing a same-size file during the copy. */
        uint8_t h[32];
        if (hash_file(from, h, NULL) != 0 || memcmp(h, it->hash, 32)) return -1;
        B.m->count++;
    }
    return rc;
}

static int walk_personal(const char *relative, unsigned depth) {
    if (depth > 8) return -1;
    char root[INK_LAYOUT_PATH_MAX], path[256];
    if (ink_layout_member_files(root, sizeof root, B.j.member) < 0) return -1;
    snprintf(path, sizeof path, "%s%s%s", root, relative[0] ? "/" : "", relative);
    hal_dir_t *dir = hal_fs_opendir(path);
    if (!dir) return hal_fs_size(path) < 0 && !relative[0] ? 0 : -1;
    hal_dirent_t *ent = osal_malloc(sizeof *ent, OSAL_MEM_EXTERNAL);
    if (!ent) { hal_fs_closedir(dir); return -1; }
    int rc = 0, got;
    while ((got = hal_fs_readdir(dir, ent)) > 0) {
        size_t suffix = strlen(ent->name);
        if (ent->name[0] == '.' || (suffix > 5 && !strcmp(ent->name + suffix - 5, ".part")) ||
            (suffix > 4 && !strcmp(ent->name + suffix - 4, ".tmp"))) continue;
        char name[128];
        int n = snprintf(name, sizeof name, "%s%s%s", relative, relative[0] ? "/" : "", ent->name);
        if (n < 0 || (size_t)n >= sizeof name || (ent->is_dir ? walk_personal(name, depth + 1) : copy_personal(name)) != 0) { rc = -1; break; }
    }
    if (got < 0) rc = -1;
    osal_free(ent); hal_fs_closedir(dir);
    return rc;
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

static int make_manifest(void) {
    memset(B.m, 0, sizeof *B.m);
    B.m->magic = MAN_MAGIC; B.m->format = 1;
    snprintf(B.m->member, sizeof B.m->member, "%s", B.j.member);
    B.m->created_s = osal_wall_us() / 1000000;
    if (!stage_dirs()) return -1;
    for (int i = 0;; i++) {
        const ink_db_def_t *d = ink_db_def_at(i);
        if (!d) break;
        if (d->scope != INK_DB_MEMBER) continue;
        ink_db_t *db = ink_db_open_member(d, B.j.member, B.j.generation);
        if (!db) return -1;
        void *data = NULL; size_t len = 0;
        int rc = ink_db_wait(db, ink_db_last_ver(db), 5000000);
        if (!rc) rc = ink_db_export(db, &data, &len);
        if (!rc) {
            uint32_t revision; memcpy(&revision, (uint8_t *)data + 12, sizeof revision);
            uint32_t records = 0;
            for (size_t off = 32; off + 36 <= len;) {
                const uint8_t *p = (const uint8_t *)data + off;
                uint16_t bytes; memcpy(&bytes, p + 28, sizeof bytes);
                records += p[2] == 1; off += 36u + bytes;
            }
            rc = add_memory(ITEM_DB, d->name, data, len, revision, records);
        }
        osal_free(data); ink_db_close(db);
        if (rc) return -1;
    }
    names_t *names = osal_malloc(sizeof *names, OSAL_MEM_EXTERNAL);
    if (!names) return -1;
    memset(names, 0, sizeof *names);
    snprintf(names->root, sizeof names->root, "/user/m/%s", B.j.member);
    ink_storage_run(list_state, names, true);
    int rc = names->rc;
    for (int i = 0; !rc && i < names->count; i++) {
        char path[INK_LAYOUT_PATH_MAX]; void *data = NULL; size_t len = 0;
        if (ink_layout_member(path, sizeof path, B.j.member, names->name[i]) < 0 || ink_storage_load(path, &data, &len) != 0) rc = -1;
        else rc = add_memory(ITEM_STATE, names->name[i], data, len, 0, 0);
        ink_storage_free(data);
    }
    osal_free(names);
    if (!rc) rc = walk_personal("", 0);
    if (!rc) {
        char path[160]; uint8_t hash[32];
        stage_path(path, sizeof path, "manifest", 0);
        size_t len = manifest_size();
        ink_sha256(B.m, len, hash); hex(hash, 32, B.j.manifest_hash);
        B.j.manifest_size = (uint32_t)len;
        rc = card_write(path, B.m, len);
    }
    return rc;
}

static void reply(void *ctx, int status, const ink_json_t *j, int result) {
    (void)ctx;
    ink_svc_lock();
    B.reply_status = status;
    B.reply[0] = 0;
    if (!status && j && result >= 0) {
        size_t n = (size_t)(j->tok[result].end - j->tok[result].start);
        if (n >= sizeof B.reply) B.reply_status = INK_HUB_ERR_REMOTE;
        else { memcpy(B.reply, j->text + j->tok[result].start, n); B.reply[n] = 0; }
        int64_t xfer;
        if (B.receiving && ink_json_int(j, ink_json_find(j, result, "xfer"), &xfer) && xfer > 0 && xfer <= UINT16_MAX) B.xfer = (uint16_t)xfer;
    }
    __atomic_store_n(&B.reply_done, 1, __ATOMIC_RELEASE);
    ink_svc_unlock();
}

static int rpc(const char *method, const char *params, ink_json_t *j) {
    if (!actor_valid()) return -1;
    __atomic_store_n(&B.reply_done, 0, __ATOMIC_RELEASE);
    if (ink_hub_call(method, params, 3000000, reply, NULL) <= 0) return -1;
    while (!__atomic_load_n(&B.reply_done, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
    if (!actor_valid()) return -1;
    if (B.reply_status == INK_HUB_ERR_REMOTE) return -2;
    if (B.reply_status != 0 || ink_json_parse(j, B.reply, strlen(B.reply)) != 0) return -1;
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
    ink_svc_lock();
    if (B.upload && upload == B.upload && (uint32_t)off >= B.ack) B.ack = (uint32_t)off;
    ink_svc_unlock();
}

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t n) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8 * i)); }
static void blob_frame(void *ctx, const uint8_t *d, size_t len) {
    (void)ctx;
    if (len < 16 || d[0] != 6 || d[1]) return;
    ink_svc_lock();
    uint16_t xfer = (uint16_t)(d[10] | d[11] << 8);
    uint32_t off = u32(d + 12);
    if (B.receiving && B.xfer && xfer == B.xfer && B.buf && off == B.want_off + B.rx_len &&
        len - 16 <= CHUNK - B.rx_len && B.rx_generation == ink_member_generation(B.rx_member) &&
        !strcmp(B.rx_member, ink_member_current()->id)) {
        memcpy(B.buf + B.rx_len, d + 16, len - 16);
        B.rx_len += (uint32_t)(len - 16);
    }
    ink_svc_unlock();
}

/* One upload segment (up to four frames) per job step. The hub's durable offset is authoritative after every reconnect. */
static int upload_block(const char *path, const uint8_t hash[32], uint32_t size, ink_json_t *j) {
    char sha[65], params[400]; hex(hash, 32, sha);
    snprintf(params, sizeof params, "{\"name\":\"backup\",\"size\":%u,\"sha256\":\"%s\",\"member\":\"%s\",\"kind\":\"backup\"}", size, sha, B.j.member);
    int called = rpc("blob/put.begin", params, j);
    if (called) return called;
    uint32_t upload, offset;
    if (!number(j, "upload", &upload) || !upload || upload > UINT16_MAX || !number(j, "offset", &offset) || offset > size) return -1;
    ink_svc_lock();
    B.upload = (uint16_t)upload; B.ack = offset;
    ink_svc_unlock();
    if (offset < size) {
        uint32_t segment = size - offset; if (segment > 65536u) segment = 65536u;
        hal_file_t *f = hal_fs_open(path, HAL_O_READ);
        bool ok = f && hal_fs_seek(f, offset, SEEK_SET) == 0;
        int failure = -2;
        for (uint32_t sent = 0; ok && sent < segment;) {
            size_t n = segment - sent; if (n > CHUNK) n = CHUNK;
            if (hal_fs_read(f, B.buf + 16, n) != (ssize_t)n) { ok = false; break; }
            memset(B.buf, 0, 16); B.buf[0] = 7;
            B.buf[10] = (uint8_t)upload; B.buf[11] = (uint8_t)(upload >> 8); put32(B.buf + 12, offset + sent);
            if (ink_hub_send_frame_for(B.buf, n + 16, INK_HUB_TX_BACKGROUND, B.j.member, B.j.generation) != 0) { ok = false; failure = -1; break; }
            sent += (uint32_t)n;
        }
        if (f && hal_fs_close(f) != 0) ok = false;
        if (!ok) return failure;
        uint64_t end = osal_now_us() + 3000000;
        uint32_t got;
        do {
            ink_svc_lock(); got = B.ack; ink_svc_unlock();
            if (got == offset + segment) break;
            if (!actor_valid() || !ink_hub_online() || osal_now_us() >= end || got > offset + segment) return -1;
            osal_task_delay_us(1000);
        } while (true);
        B.j.offset = got;
        ink_probe_value("backup.upload_offset", got);
        if (got < size) return 1;
    }
    snprintf(params, sizeof params, "{\"upload\":%u}", upload);
    called = rpc("blob/put.end", params, j);
    if (called) return called;
    char id[80], expected[80]; snprintf(expected, sizeof expected, "sha256:%s", sha);
    if (ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id) < 0 || strcmp(id, expected)) return -2;
    return 0;
}

/* A completed block is synced on the card before the next request. File size survives a restart. */
static int download_block(const char *path, const char *sha, uint32_t size, ink_json_t *j) {
    int64_t have = hal_fs_size(path);
    if (have < 0) have = 0;
    if (have > size) { hal_fs_remove(path); have = 0; }
    if ((uint32_t)have < size) {
        uint32_t n = size - (uint32_t)have; if (n > CHUNK) n = CHUNK;
        char params[320];
        ink_svc_lock();
        B.receiving = true; B.xfer = 0; B.rx_len = 0; B.want_off = (uint32_t)have;
        snprintf(B.rx_member, sizeof B.rx_member, "%s", B.j.member); B.rx_generation = B.j.generation;
        ink_svc_unlock();
        snprintf(params, sizeof params, "{\"id\":\"sha256:%s\",\"offset\":%u,\"len\":%u,\"member\":\"%s\"}", sha, (uint32_t)have, n, B.j.member);
        int rc = rpc("blob/fetch", params, j);
        uint32_t total = 0;
        char got_sha[65];
        if (!rc && (!number(j, "size", &total) || total != size ||
            ink_json_str(j, ink_json_find(j, 0, "sha256"), got_sha, sizeof got_sha) < 0 || strcmp(got_sha, sha))) rc = -2;
        uint64_t end = osal_now_us() + 3000000;
        uint32_t got = 0;
        while (!rc) {
            ink_svc_lock(); got = B.rx_len; ink_svc_unlock();
            if (got == n) break;
            if (!actor_valid() || !ink_hub_online() || osal_now_us() >= end) { rc = -1; break; }
            osal_task_delay_us(1000);
        }
        ink_svc_lock(); B.receiving = false; ink_svc_unlock();
        if (rc) return rc;
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | (have ? HAL_O_APPEND : HAL_O_TRUNC));
        bool ok = f && hal_fs_write(f, B.buf, n) == (ssize_t)n && hal_fs_sync(f) == 0;
        if (f && hal_fs_close(f) != 0) ok = false;
        if (!ok) return -2;
        B.j.offset = (uint32_t)have + n;
        ink_probe_value("backup.download_offset", B.j.offset);
        if (B.j.offset < size) return 1;
    } else if (!size && have == 0 && hal_fs_size(path) < 0 && card_write(path, "", 0)) return -2;
    uint8_t hash[32]; char actual[65];
    if (hash_file(path, hash, NULL)) return -2;
    hex(hash, 32, actual);
    if (strcmp(actual, sha)) { hal_fs_remove(path); return -2; }
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
    if (B.m->magic != MAN_MAGIC || B.m->format != 1 || B.m->count > ITEMS || manifest_size() != B.j.manifest_size ||
        !memchr(B.m->member, 0, sizeof B.m->member) || strcmp(B.m->member, B.j.member)) return -1;
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
        int rc = ink_db_wait(db, ink_db_last_ver(db), 5000000);
        if (!rc) rc = ink_db_export(db, &data, &len);
        if (!rc) { ink_sha256(data, len, hash); if (revision) *revision = u32((uint8_t *)data + 12); }
        osal_free(data); ink_db_close(db);
        return rc;
    }
    char path[256]; if (!physical(it, path, sizeof path)) return -1;
    if (it->kind == ITEM_FILE) {
        if (hal_fs_size(path) < 0) return 0;
        return hash_file(path, hash, NULL);
    }
    void *data = NULL; size_t len = 0;
    int rc = ink_storage_load(path, &data, &len);
    if (rc == INK_STORAGE_NO_FILE) return 0;
    if (rc) return -1;
    ink_sha256(data, len, hash);
    ink_storage_free(data);
    return 0;
}

static int prepare_preview(void) {
    B.j.conflicts = 0;
    for (unsigned i = 0; i < B.m->count; i++) {
        item_t *it = &B.m->item[i];
        if (current_hash(it, it->before, NULL)) return -1;
        if (!zero_hash(it->before) && memcmp(it->before, it->hash, 32)) B.j.conflicts++;
    }
    char path[160]; stage_path(path, sizeof path, "preview", 0);
    if (card_write(path, B.m, manifest_size())) return -1;
    ink_sha256(B.m, manifest_size(), B.j.preview_hash);
    uint8_t nonce[16]; hal_random_fill(nonce, sizeof nonce); hex(nonce, sizeof nonce, B.j.preview);
    B.j.phase = PH_PREVIEW; B.j.next = B.j.offset = 0;
    return save_journal();
}

static int apply_item(unsigned index) {
    item_t *it = &B.m->item[index];
    if (!supported(it)) return 0;
    uint8_t now[32]; uint32_t version;
    if (current_hash(it, now, &version)) return -1;
    if (!memcmp(now, it->hash, 32)) return 0; /* A cut after installation but before the journal commit. */
    if (memcmp(now, it->before, 32)) return -3;
    char staged[160], target[256]; stage_path(staged, sizeof staged, "down", index);
    if (!physical(it, target, sizeof target)) return -2;
    if (it->kind == ITEM_FILE) {
        /* mkdir through the file service validates paths and keeps the same member. */
        char virt[INK_FS_PATH_MAX]; snprintf(virt, sizeof virt, "/me/%.123s", it->name);
        char *slash = strrchr(virt, '/');
        if (slash && slash != virt + 3) { *slash = 0; if (ink_fs_mkdir_member_generation(B.j.member, B.j.generation, virt) != 0) return -2; }
        snprintf(virt, sizeof virt, "/me/%.123s", it->name);
        int rc = ink_fs_restore(B.j.member, B.j.generation, index, virt, it->before);
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
static bool active(void) { return B.j.phase == PH_UPLOAD || B.j.phase == PH_FETCH || B.j.phase == PH_RESTORE; }
static void schedule(void) { if (pending_for(NULL)) ink_svc_at(&B.retry, osal_now_us() + RETRY_US, wake, NULL); }

static void mark_failed(const char *reason) {
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
        return rc < 0 ? rc : 1;
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
        char path[160]; stage_path(path, sizeof path, "remote", 0);
        int rc = download_block(path, B.j.manifest_hash, B.j.manifest_size, j);
        if (rc) return rc;
        if (load_manifest("remote", true) || prepare_preview()) return -2;
        event("backup.changed");
        return 0;
    }
    if (load_manifest("preview", false)) return -2;
    if (B.j.next > B.m->count) return -2;
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
        uint8_t now[32];
        if (current_hash(it, now, NULL)) return -1;
        if (memcmp(now, it->hash, 32)) {
            if (memcmp(now, it->before, 32)) return -3;
            char path[160], sha[65]; stage_path(path, sizeof path, "down", B.j.next); hex(it->hash, 32, sha);
            rc = download_block(path, sha, it->size, j);
            if (!rc) rc = apply_item(B.j.next);
        }
    }
    if (!rc) {
        B.j.next++; B.j.offset = 0;
        if (save_journal()) return -1;
    }
    return rc < 0 ? rc : 1;
}

static void step(void *arg) {
    (void)arg;
    const char *member = ink_member_current()->id;
    if (!pending_for(member)) { schedule(); return; }
    if (!load_journal(member)) { remember(member, false); schedule(); return; }
    if (!actor_valid() || ink_jobs_held()) { schedule(); return; }
    if (B.j.phase == PH_SAVED) {
        if (ink_fs_card_hold() == INK_FS_OK) {
            if (resources() && load_manifest("manifest", true) == 0) { acknowledge_manifest(); remember(member, false); }
            free_resources(); ink_fs_card_release();
        }
        schedule(); return;
    }
    if (!active()) { remember(member, false); schedule(); return; }
    if (!*task_id()) *task_id() = ink_task_begin_for(B.j.phase == PH_UPLOAD ? "backup" : "restore", "继续备份或恢复资料", B.j.member, B.j.generation, NULL, NULL);
    if (!ink_member_io_begin(B.j.member, B.j.generation)) return;
    if (ink_fs_card_hold() != INK_FS_OK) { schedule(); ink_member_io_end(B.j.member); return; }
    int rc = -1;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (resources() && j) {
        rc = B.j.phase == PH_UPLOAD ? upload_step(j) : restore_step(j);
        if ((*task_id()) && B.m->count) ink_task_progress((*task_id()), (int)(B.j.next * 100 / (B.m->count + 1)), B.j.phase == PH_UPLOAD ? "sending" : "writing");
    }
    if (rc == 1 && active()) save_journal();
    if (rc == -2) mark_failed("备份资料校验或写入失败，原件仍保留");
    else if (rc == -3) mark_failed("预览后资料有改动，请重新预览");
    if (rc < 0 && active()) ink_probe_count("backup.retries");
    osal_free(j); free_resources(); ink_fs_card_release();
    ink_member_io_end(B.j.member);
    schedule();
}

static bool for_call(ink_call_t *c) {
    uint32_t generation;
    const char *member = ink_cap_call_member(c, &generation);
    if (!generation || generation != ink_member_generation(member) || strcmp(member, ink_member_current()->id) || !ink_member_space_open()) {
        c->rc.status = INK_RC_STALE; return false;
    }
    if (!load_journal(member)) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "备份记录暂时读不出来，请稍后再试"); return false; }
    if (!B.j.member[0]) { snprintf(B.j.member, sizeof B.j.member, "%s", member); B.j.generation = generation; }
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
    ink_jw_raw(&w, ",\"next\":"); ink_jw_int(&w, B.j.next);
    ink_jw_raw(&w, ",\"offset\":"); ink_jw_int(&w, B.j.offset);
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
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        char params[160];
        snprintf(params, sizeof params, "{\"id\":\"sha256:%s\",\"member\":\"%s\"}", B.j.manifest_hash, B.j.member);
        if (j && rpc("blob/status", params, j) == 0 && ink_json_eq(j, ink_json_find(j, 0, "durable"), "true")) {
            B.j.durable = 1; save_journal();
        }
        osal_free(j);
    }
    result(c, false);
}

static void cap_manifest(ink_call_t *c) {
    if (!for_call(c)) return;
    if (B.j.phase == PH_NONE) { result(c, false); return; }
    if (ink_fs_card_hold() != INK_FS_OK) { c->rc.status = INK_RC_FAILED; return; }
    bool preview = B.j.phase == PH_PREVIEW || B.j.phase == PH_RESTORE || B.j.phase == PH_DONE || (B.j.phase == PH_FAILED && B.j.preview[0]);
    if (!resources() || load_manifest(preview ? "preview" : "manifest", !preview)) c->rc.status = INK_RC_FAILED;
    else result(c, true);
    free_resources(); ink_fs_card_release();
}

static void cap_backup(ink_call_t *c) {
    if (!for_call(c)) return;
    if (active()) { c->rc.status = INK_RC_QUEUED; result(c, false); schedule(); return; }
    if (!ink_member_io_begin(B.j.member, B.j.generation)) { c->rc.status = INK_RC_STALE; return; }
    if (ink_fs_card_hold() != INK_FS_OK) {
        c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "请插入存储卡后备份"); ink_member_io_end(B.j.member); return;
    }
    B.j.phase = PH_NONE; B.j.next = B.j.offset = B.j.received = B.j.durable = B.j.conflicts = 0;
    B.j.reason[0] = B.j.preview[0] = 0;
    /* Invalidate an earlier preview before replacing any staging file. */
    int rc = save_journal();
    if (!rc && resources()) rc = make_manifest(); else rc = -1;
    if (!rc) {
        B.j.phase = PH_UPLOAD;
        rc = save_journal();
    }
    if (rc) { c->rc.status = INK_RC_FAILED; mark_failed("备份清单没有写好，原件仍保留"); }
    else {
        (*task_id()) = ink_task_begin_for("backup", "备份资料", B.j.member, B.j.generation, NULL, NULL);
        c->rc.status = INK_RC_QUEUED; result(c, true); schedule();
    }
    free_resources(); ink_fs_card_release(); ink_member_io_end(B.j.member);
}

static void cap_preview(ink_call_t *c) {
    if (!for_call(c)) return;
    if (B.j.phase == PH_PREVIEW) { cap_manifest(c); return; }
    if (active()) { c->rc.status = INK_RC_QUEUED; result(c, false); schedule(); return; }
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char params[96]; snprintf(params, sizeof params, "{\"member\":\"%s\"}", B.j.member);
    uint32_t len = 0; char sha[65] = "";
    bool ok = j && rpc("backup/latest", params, j) == 0 && number(j, "size", &len) && len >= offsetof(manifest_t, item) && len <= sizeof(manifest_t) &&
        ink_json_str(j, ink_json_find(j, 0, "sha256"), sha, sizeof sha) > 0 && hash_ok(sha);
    osal_free(j);
    if (!ok) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "暂时取不到这位成员的备份，请联网后再试"); return; }
    if (ink_fs_card_hold() != INK_FS_OK) { c->rc.status = INK_RC_FAILED; return; }
    if (!stage_dirs()) { ink_fs_card_release(); c->rc.status = INK_RC_FAILED; return; }
    char path[160]; stage_path(path, sizeof path, "remote", 0); hal_fs_remove(path);
    for (unsigned i = 0; i < ITEMS; i++) { stage_path(path, sizeof path, "down", i); hal_fs_remove(path); }
    ink_fs_card_release();
    snprintf(B.j.manifest_hash, sizeof B.j.manifest_hash, "%s", sha); B.j.manifest_size = len;
    B.j.phase = PH_FETCH; B.j.next = B.j.offset = B.j.conflicts = B.j.received = B.j.durable = 0;
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
    for (unsigned i = 0; ok && i < B.m->count; i++) {
        uint8_t now[32];
        if (current_hash(&B.m->item[i], now, NULL) || memcmp(now, B.m->item[i].before, 32)) ok = false;
    }
    if (!ok) {
        c->rc.status = INK_RC_STALE; B.j.phase = PH_FAILED;
        snprintf(B.j.reason, sizeof B.j.reason, "预览后资料有改动，请重新预览"); save_journal();
    } else {
        B.j.phase = PH_RESTORE; B.j.next = B.j.offset = 0;
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
