/*
 * File service (see ink/sys/file.h). Everything on the card goes through one
 * resolver: a virtual path, checked and mapped, or a refusal. Only the card:
 * nothing here can name /user.
 *
 * Jobs (copy, delete) walk a tree with an explicit stack, a piece per step on
 * the background executor, so the hold on it stays short and they can be
 * cancelled. A copied file is written as <name>.part and renamed when complete.
 *
 * Lock: the shared service lock, for the table of open files and the job's
 * status. Card calls are made outside it.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/i18n.h"
#include "ink/hal_storage.h"
#include "ink/hal_device.h"
#include "ink/hal_usb.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/task.h"
#include "ink/sys/sha256.h"
#include "ink/sys/json.h"
#include "file_internal.h"

#define MAX_OPEN 8
#define MAX_RELEASE 4
#define PHYS_MAX 200
#define JOB_DEPTH 8
#define JOB_BUF 16384
#define STEP_BYTES (64u * 1024u)
#define RELEASE_WAIT_US 3000000ull
#define PART_SUFFIX ".part"
#define TMP_SUFFIX ".tmp"
#define INDEX_CARD "/content/.file-index-card"
#define INDEX_DIRTY "/content/.file-index-dirty"

struct ink_fs_file {
    hal_file_t *f;
    bool used;
    bool wrote;
    bool writable;
    uint32_t revision;
    file_context_t context;
    char member[INK_MEMBER_ID_MAX];
    char path[INK_FS_PATH_MAX]; /* virtual, for the change event */
    const char *owner;          /* the task that opened it, for "in use by" */
};

struct ink_fs_dir {
    char member[INK_MEMBER_ID_MAX];
    file_context_t context;
    hal_dir_t *d;
    bool at_root; /* "/" lists the virtual roots */
    int next_root;
    hal_dirent_t e; /* large for the callers' stacks: the object is in PSRAM */
};

typedef struct {
    int id;
    int op; /* 0 copy, 1 delete */
    char from[PHYS_MAX], to[PHYS_MAX];
    char vfrom[INK_FS_PATH_MAX], vto[INK_FS_PATH_MAX];
    /* the walk */
    hal_dir_t *dirs[JOB_DEPTH];
    int depth;
    int phase; /* 0 measure, 1 work */
    hal_file_t *in, *out;
    char part[PHYS_MAX];
    uint8_t *buf;
    bool cancel;
    int task;
    char who[INK_MEMBER_ID_MAX];
    ink_fs_job_status_t st;
    void (*on_end)(void *ctx, const ink_fs_job_status_t *st);
    void *on_end_ctx;
} fjob_t;

static struct {
    osal_mutex_t *edits;
    uint32_t revision;
    struct ink_fs_file files[MAX_OPEN];
    int nopen;
    fjob_t job;
    bool job_active;
    int next_job;
    bool usb_lent;
    bool ready; /* Early reset recovery runs alone, before the service lock exists. */
    struct {
        ink_fs_release_fn fn;
        void *ctx;
    } rel[MAX_RELEASE];
    int nrel;
    int h_free, h_card, h_mode, h_usb;
    int h_index;
    bool index_seen, index_dirty;
    uint8_t index_card[16];
    ink_thumb_cache_t *thumbs;
    uint32_t thumb_epoch;
    ink_exec_job_t usb_job, refresh_job, index_job;
    ink_svc_deadline_t refresh_at, index_at;
} FS INK_PSRAM_BSS;

static int index_edit(const char *from, const char *to);
static void index_background(void *arg);
static bool storage_index_ready(void);
static void index_invalidate(void);
static int index_prepare(const char *path);
static void index_refresh_cb(void *arg) { ink_exec_post(INK_EXEC_BG, &FS.index_job, index_background, arg); }
static void thumb_clear(void *ctx, const char *id, const ink_args_t *args) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    ink_thumb_cache_destroy(FS.thumbs); FS.thumbs = NULL;
    /* Switching members only clears images; roster changes may resume indexing. */
    if (!ctx && !storage_index_ready()) ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
    osal_mutex_unlock(FS.edits);
}
static void thumb_member(void *ctx, int handle, const char *id) { (void)ctx; (void)handle; thumb_clear(&FS, id, NULL); }
static const ink_db_def_t INDEX_DEF = {.name = "file.index", .title = "文件编号", .scope = INK_DB_DEVICE, .schema = 3,
    .max_records = INK_FS_INDEX_MAX, .max_bytes = INK_FS_INDEX_MAX * 216u};
static const ink_db_def_t INDEX_DONE_DEF = {.name = "file.index_ready", .title = "索引完成记录", .scope = INK_DB_DEVICE, .schema = 2,
    .max_records = INK_FS_INDEX_MAX, .max_bytes = INK_FS_INDEX_MAX * 216u};

const char *ink_fs_err_name(int e) {
    switch (e) {
    case INK_FS_OK: return "ok";
    case INK_FS_ERR_PATH: return "not a valid path";
    case INK_FS_ERR_DENIED: return "not allowed for this member";
    case INK_FS_ERR_NOT_FOUND: return "not found";
    case INK_FS_ERR_EXISTS: return "already exists";
    case INK_FS_ERR_NO_CARD: return "no card";
    case INK_FS_ERR_USB: return "the card is lent to a computer";
    case INK_FS_ERR_NO_SPACE: return "the card is full";
    case INK_FS_ERR_BUSY: return "busy";
    case INK_FS_ERR_IS_DIR: return "a directory";
    case INK_FS_ERR_NO_HOST: return "no computer connected";
    case INK_FS_ERR_INDEX_FULL: return "file index is full";
    case INK_FS_ERR_CARD_CHANGED: return "the storage card changed";
    default: return "read or write error";
    }
}

/* ------------------------------------------------------------ paths */

/* One segment of a path: UTF-8, not hidden, nothing FAT cannot hold. */
static bool segment_ok(const char *s, size_t n) {
    if (n == 0 || n >= INK_FS_NAME_MAX || s[0] == '.' || s[n - 1] == ' ' || s[n - 1] == '.') return false;
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7F || strchr("\\:*?\"<>|", c)) return false;
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 && c >= 0xC2 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 && c <= 0xF4 ? 4 : 0;
        if (!len || i + len > n) return false;
        for (size_t k = 1; k < len; k++)
            if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
        if (len == 3 && ((c == 0xE0 && (unsigned char)s[i + 1] < 0xA0) || (c == 0xED && (unsigned char)s[i + 1] > 0x9F))) return false;
        if (len == 4 && ((c == 0xF0 && (unsigned char)s[i + 1] < 0x90) || (c == 0xF4 && (unsigned char)s[i + 1] > 0x8F))) return false;
        i += len;
    }
    return true;
}

/* The suffixes a file has while it is not finished. */
static bool hidden_name(const char *name) {
    size_t n = strlen(name);
    if (name[0] == '.') return true;
    return (n > 5 && !strcmp(name + n - 5, PART_SUFFIX)) || (n > 4 && !strcmp(name + n - 4, TMP_SUFFIX));
}

/* Capture once, before a card call or waiting for another edit. */
static file_context_t current_context(void) {
    file_context_t c = {0};
    ink_svc_lock();
    const ink_member_t *m = ink_member_current();
    snprintf(c.member, sizeof c.member, "%s", m->id);
    c.adult = m->role == INK_ROLE_ADULT;
    
    ink_svc_unlock();
    return c;
}

static file_context_t member_context(const char *member) {
    file_context_t c = {0};
    ink_svc_lock();
    if (member && ink_member_exists(member)) {
        const ink_member_t *m;
        for (int i = 0; (m = ink_member_at(i)) != NULL; i++) {
            if (strcmp(m->id, member)) continue;
            snprintf(c.member, sizeof c.member, "%s", m->id);
            c.adult = m->role == INK_ROLE_ADULT;
            
            break;
        }
    }
    ink_svc_unlock();
    return c;
}

bool fs_context_for_call(file_context_t *context, ink_call_t *call) {
    *context = current_context();
    const char *member = ink_cap_call_member(call);
    if (!strcmp(context->member, member)) return true;
    call->rc.status = INK_RC_STALE;
    snprintf(call->rc.detail, sizeof call->rc.detail, "someone else is using the device now");
    return false;
}

/* Every path of an operation uses the same authority and namespace. */
static int resolve_for(const file_context_t *c, const char *path, bool write, char *phys, size_t cap) {
    if (!ink_member_exists(c->member)) return INK_FS_ERR_DENIED;
    size_t n = path ? strlen(path) : 0;
    if (n < 2 || n >= INK_FS_PATH_MAX || path[0] != '/' || path[n - 1] == '/') return INK_FS_ERR_PATH;
    for (const char *p = path + 1;;) { /* every segment */
        const char *e = strchr(p, '/');
        size_t seg = e ? (size_t)(e - p) : strlen(p);
        if (!segment_ok(p, seg)) return INK_FS_ERR_PATH;
        if (!e) break;
        p = e + 1;
    }
    int r;
    if (!strncmp(path, "/me", 3) && (path[3] == '/' || !path[3])) {
        r = snprintf(phys, cap, "/content/.m/%s%s", c->member, path + 3);
    } else if (!strncmp(path, "/family", 7) && (path[7] == '/' || !path[7])) {
        if (write && !c->adult) return INK_FS_ERR_DENIED;
        r = snprintf(phys, cap, "/content%s", path + 7);
    } else if (!strncmp(path, "/members/", 9)) {
        if (!c->adult) return INK_FS_ERR_DENIED;
        const char *rest = strchr(path + 9, '/');
        size_t idn = rest ? (size_t)(rest - (path + 9)) : strlen(path + 9);
        char id[INK_MEMBER_ID_MAX];
        if (idn >= sizeof id) return INK_FS_ERR_PATH;
        memcpy(id, path + 9, idn);
        id[idn] = 0;
        if (!ink_layout_member_ok(id)) return INK_FS_ERR_PATH;
        if (!ink_member_exists(id)) return INK_FS_ERR_DENIED;
        r = snprintf(phys, cap, "/content/.m/%s%s", id, rest ? rest : "");
    } else if (!strcmp(path, "/members")) {
        return c->adult ? INK_FS_ERR_IS_DIR : INK_FS_ERR_DENIED; /* the list of members is not a directory */
    } else if (!strncmp(path, "/apps", 5) && (!path[5] || path[5] == '/')) {
        if (write && !c->adult) return INK_FS_ERR_DENIED;
        r = snprintf(phys, cap, "/content%s", path);
    } else if (!strncmp(path, "/app/", 5)) {
        const char *rest = strchr(path + 5, '/');
        if (!rest) return INK_FS_ERR_PATH;
        r = snprintf(phys, cap, "/content/.m/%s/apps/%.*s%s", c->member, (int)(rest - (path + 5)), path + 5, rest);
    } else if (!strncmp(path, "/cache/", 7)) {
        r = snprintf(phys, cap, "%s", path);
    } else return INK_FS_ERR_PATH;
    if (r <= 0 || (size_t)r >= cap) return INK_FS_ERR_PATH;
    if (FS.usb_lent) return INK_FS_ERR_USB;
    if (!hal_fs_card_present()) {
        ink_state_set_bool(FS.h_card, false);
        ink_state_set_str(FS.h_index, "unavailable");
        return INK_FS_ERR_NO_CARD;
    }
    return INK_FS_OK;
}

int fs_resolve(const char *path, bool write, char *phys, size_t cap) {
    file_context_t c = current_context();
    return resolve_for(&c, path, write, phys, cap);
}

/* The lease covers resolution and every HAL call, including short operations. */
static int resolve_held(const file_context_t *c, const char *path, bool write, char *phys, size_t cap) {
    int r = ink_fs_card_hold();
    if (r) return r;
    r = resolve_for(c, path, write, phys, cap);
    if (r) ink_fs_card_release();
    return r;
}

static int card_done(int r) {
    ink_fs_card_release();
    return r;
}

/* A place that is there for everyone and that nobody can take away: under /family lie the members' own folders, which
 * nobody sees from there and which nothing that walks it may touch. */
static bool virtual_root(const char *path) {
    if (!strcmp(path, "/me") || !strcmp(path, "/family")) return true;
    return !strncmp(path, "/members/", 9) && !strchr(path + 9, '/');
}

/* The directories of a physical path, from the card's root down. */
static void make_parents(const char *phys) {
    char dir[PHYS_MAX];
    size_t n = strlen(phys);
    if (n >= sizeof dir) return;
    memcpy(dir, phys, n + 1);
    for (char *p = dir + 1; *p; p++)
        if (*p == '/' && p > dir + 8) { /* past /content or /cache */
            *p = 0;
            hal_fs_mkdir(dir);
            *p = '/';
        }
}

/* What a failed card call means now. */
static int card_error(void) {
    if (FS.usb_lent) return INK_FS_ERR_USB;
    if (!hal_fs_card_present()) {
        ink_state_set_bool(FS.h_card, false);
        return INK_FS_ERR_NO_CARD;
    }
    return INK_FS_ERR_IO;
}

static void changed_for(const char *who, const char *path, const char *kind) {
    /* Known paths keep their records; file.usage follows FS.revision without a walk. */
    if (!strcmp(kind, "copied") || !strcmp(kind, "rescan") || !strcmp(kind, "restored")) index_invalidate();
    char *block = osal_malloc(sizeof(ink_args_t), OSAL_MEM_EXTERNAL); /* the arguments are large for most stacks */
    if (!block) return;
    ink_args_t *a = (ink_args_t *)block;
    ink_args_init(a);
    ink_args_str(a, "path", path);
    ink_args_str(a, "kind", kind);
    ink_args_str(a, "member", who);
    ink_cap_emit_for("file.changed", a, who);
    osal_free(block);
    ink_svc_at(&FS.refresh_at, osal_now_us() + 60000000ull, fs_refresh_cb, NULL); /* what is free changed */
}

static void changed(const file_context_t *c, const char *path, const char *kind) { changed_for(c->member, path, kind); }

/* ------------------------------------------------------------ files */

static int fs_open_unlocked(const file_context_t *c, const char *path, int mode, ink_fs_file_t **out) {
    char phys[PHYS_MAX];
    bool write = mode & (INK_FS_WRITE | INK_FS_APPEND | INK_FS_CREATE);
    int r = resolve_held(c, path, write, phys, sizeof phys);
    if (r) return r;
    ink_fs_file_t *slot = NULL;
    ink_svc_lock();
    for (int i = 0; i < MAX_OPEN && !slot; i++)
        if (!FS.files[i].used) slot = &FS.files[i];
    if (slot) {
        slot->used = true; /* before the card: lending it waits for this */
    }
    ink_svc_unlock();
    if (!slot) return card_done(INK_FS_ERR_BUSY);
    int64_t size = hal_fs_size(phys);
    int err = INK_FS_OK;
    if ((mode & INK_FS_EXCL) && size >= 0) err = INK_FS_ERR_EXISTS;
    else if (!(mode & INK_FS_CREATE) && size < 0) err = hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error();
    else if (size < 0) err = index_prepare(phys);
    hal_file_t *f = NULL;
    if (!err) {
        int flags = 0;
        if (mode & INK_FS_READ) flags |= HAL_O_READ;
        if (mode & (INK_FS_WRITE | INK_FS_APPEND)) flags |= HAL_O_WRITE;
        if (mode & INK_FS_APPEND) flags |= HAL_O_APPEND;
        if (mode & INK_FS_CREATE) flags |= HAL_O_CREATE;
        if (!flags) flags = HAL_O_READ;
        if (mode & INK_FS_CREATE) make_parents(phys);
        f = hal_fs_open(phys, flags);
        if (!f) err = card_error();
    }
    ink_svc_lock();
    if (err) {
        slot->used = false;
        FS.nopen--;
    } else {
        slot->f = f;
        slot->wrote = size < 0;
        slot->writable = (mode & (INK_FS_WRITE | INK_FS_APPEND)) != 0;
        slot->revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
        slot->context = *c;
        snprintf(slot->member, sizeof slot->member, "%s", c->member);
        if (!strncmp(phys, "/content/.m/", 12)) {
            const char *end = strchr(phys + 12, '/');
            snprintf(slot->member, sizeof slot->member, "%.*s", (int)(end ? end - (phys + 12) : (ptrdiff_t)strlen(phys + 12)), phys + 12);
        }
        snprintf(slot->path, sizeof slot->path, "%s", path);
        slot->owner = osal_task_name(osal_task_self());
    }
    ink_svc_unlock();
    if (err) return err;
    if (size < 0 && !strncmp(phys, "/content/", 9)) index_invalidate(); /* a new path: the walk gives it an id */
    *out = slot;
    return INK_FS_OK;
}

int64_t ink_fs_read(ink_fs_file_t *f, void *buf, size_t n) {
    if (!f || !f->f) return INK_FS_ERR_IO;
    int64_t r = hal_fs_read(f->f, buf, n);
    return r < 0 ? card_error() : r;
}

static int64_t fs_write_unlocked(ink_fs_file_t *f, const void *buf, size_t n) {
    if (!f || !f->f) return INK_FS_ERR_IO;
    int64_t r = hal_fs_write(f->f, buf, n);
    if (r < 0) {
        int e = card_error();
        return e == INK_FS_ERR_IO ? INK_FS_ERR_NO_SPACE : e; /* the card answers and refuses: full */
    }
    f->wrote = true;
    f->revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    return r;
}

int ink_fs_seek(ink_fs_file_t *f, int64_t off, int whence) { return f && f->f && hal_fs_seek(f->f, off, whence) == 0 ? 0 : INK_FS_ERR_IO; }
int64_t ink_fs_tell(ink_fs_file_t *f) { return f && f->f ? hal_fs_tell(f->f) : INK_FS_ERR_IO; }
int ink_fs_sync(ink_fs_file_t *f) { return f && f->f && hal_fs_sync(f->f) == 0 ? 0 : INK_FS_ERR_IO; }

int ink_fs_close(ink_fs_file_t *f) {
    if (!f || !f->used) return INK_FS_ERR_IO;
    int r = hal_fs_close(f->f);
    bool wrote = f->wrote;
    file_context_t c = f->context;
    char path[INK_FS_PATH_MAX];
    snprintf(path, sizeof path, "%s", f->path);
    ink_svc_lock();
    f->f = NULL;
    f->used = false;
    FS.nopen--;
    ink_svc_unlock();
    if (wrote) changed(&c, path, "written");
    return r == 0 ? INK_FS_OK : INK_FS_ERR_IO;
}

/* ------------------------------------------------------------ directories */

int fs_stat_for(const file_context_t *c, const char *path, ink_fs_stat_t *out) {
    char phys[PHYS_MAX];
    int r = resolve_held(c, path, false, phys, sizeof phys);
    if (r == INK_FS_ERR_IS_DIR) { /* "/members" */
        memset(out, 0, sizeof *out);
        snprintf(out->name, sizeof out->name, "members");
        out->is_dir = true;
        return INK_FS_OK;
    }
    if (r) return r;
    const char *base = strrchr(path, '/');
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "%s", base + 1);
    int64_t size = hal_fs_size(phys);
    if (size >= 0) {
        hal_dir_t *d = hal_fs_opendir(phys); /* the HAL's size says nothing about directories */
        out->is_dir = d != NULL;
        if (d) hal_fs_closedir(d);
        out->size = d ? 0 : size;
        return card_done(INK_FS_OK);
    }
    /* /me may not exist yet: it is empty, not missing */
    if (!strcmp(path, "/me") || !strncmp(path, "/members/", 9) || !strcmp(path, "/family")) {
        const char *rest = !strncmp(path, "/members/", 9) ? strchr(path + 9, '/') : NULL;
        if (!rest) {
            out->is_dir = true;
            return card_done(INK_FS_OK);
        }
    }
    return card_done(hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error());
}

int ink_fs_stat(const char *path, ink_fs_stat_t *out) {
    file_context_t c = current_context();
    return fs_stat_for(&c, path, out);
}

int fs_dir_open_for(const file_context_t *c, const char *path, ink_fs_dir_t **out) {
    ink_fs_dir_t *dir = osal_calloc(1, sizeof *dir, OSAL_MEM_EXTERNAL);
    if (!dir) return INK_FS_ERR_IO;
    dir->context = *c;
    if (path && !strcmp(path, "/")) {
        dir->at_root = true;
        *out = dir;
        return INK_FS_OK;
    }
    char phys[PHYS_MAX];
    int r = resolve_held(c, path, false, phys, sizeof phys);
    if (r == INK_FS_ERR_IS_DIR) r = INK_FS_ERR_DENIED; /* who the members are: member.list */
    if (r) {
        osal_free(dir);
        return r;
    }
    if (strncmp(path, "/family", 7)) {
        snprintf(dir->member, sizeof dir->member, "%s", c->member);
        if (!strncmp(path, "/members/", 9)) {
            const char *end = strchr(path + 9, '/');
            snprintf(dir->member, sizeof dir->member, "%.*s", (int)(end ? end - (path + 9) : (ptrdiff_t)strlen(path + 9)), path + 9);
        }
        
    }
    dir->d = hal_fs_opendir(phys);
    if (!dir->d && !strcmp(path, "/me")) { /* first use: an empty place of their own */
        hal_fs_mkdir("/content/.m");
        hal_fs_mkdir(phys);
        dir->d = hal_fs_opendir(phys);
    }
    if (!dir->d) {
        int e = hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error();
        osal_free(dir);
        return card_done(e);
    }
    *out = dir;
    return INK_FS_OK;
}

int ink_fs_dir_open(const char *path, ink_fs_dir_t **out) {
    file_context_t c = current_context();
    return fs_dir_open_for(&c, path, out);
}

int ink_fs_dir_next(ink_fs_dir_t *d, ink_fs_stat_t *out) {
    if (!d) return INK_FS_ERR_IO;
    if (!ink_member_exists(d->context.member)) return INK_FS_ERR_DENIED;
    if (d->member[0] && (!ink_member_exists(d->member))) return INK_FS_ERR_DENIED;
    if (d->at_root) {
        static const char *const ROOTS[] = {"me", "family", "members"};
        int n = d->context.adult ? 3 : 2;
        if (d->next_root >= n) return 0;
        memset(out, 0, sizeof *out);
        snprintf(out->name, sizeof out->name, "%s", ROOTS[d->next_root++]);
        out->is_dir = true;
        return 1;
    }
    for (;;) {
        int r = hal_fs_readdir(d->d, &d->e);
        if (r <= 0) return r < 0 ? card_error() : 0;
        if (hidden_name(d->e.name) || strlen(d->e.name) >= INK_FS_NAME_MAX) continue;
        memset(out, 0, sizeof *out);
        snprintf(out->name, sizeof out->name, "%s", d->e.name);
        out->is_dir = d->e.is_dir;
        out->size = d->e.is_dir ? 0 : d->e.size;
        return 1;
    }
}

void ink_fs_dir_close(ink_fs_dir_t *d) {
    if (!d) return;
    if (d->d) {
        hal_fs_closedir(d->d);
        ink_fs_card_release();
    }
    osal_free(d);
}

static int fs_mkdir_unlocked(const file_context_t *c, const char *path) {
    char phys[PHYS_MAX];
    int r = resolve_held(c, path, true, phys, sizeof phys);
    if (r) return r;
    make_parents(phys);
    if (hal_fs_mkdir(phys) != 0) return card_done(card_error());
    changed(c, path, "created");
    return card_done(INK_FS_OK);
}

static int fs_remove_unlocked(const file_context_t *c, const char *path) {
    char phys[PHYS_MAX];
    int r = resolve_held(c, path, true, phys, sizeof phys);
    if (r) return r;
    if (hal_fs_size(phys) < 0) return card_done(hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error());
    if (hal_fs_remove(phys) != 0) return card_done(card_error());
    r = index_edit(phys, NULL);
    if (r) return card_done(r);
    changed(c, path, "removed");
    return card_done(INK_FS_OK);
}

static int fs_move_unlocked(const file_context_t *c, const char *from, const char *to) {
    char a[PHYS_MAX], b[PHYS_MAX];
    if (virtual_root(from)) return INK_FS_ERR_DENIED;
    int r = resolve_held(c, from, true, a, sizeof a);
    if (r) return r;
    r = resolve_for(c, to, true, b, sizeof b);
    if (r) return card_done(r);
    if (hal_fs_size(a) < 0) return card_done(hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error());
    if (hal_fs_size(b) >= 0) return card_done(INK_FS_ERR_EXISTS);
    size_t an = strlen(a);
    if (!strncmp(a, b, an) && b[an] == '/') return card_done(INK_FS_ERR_PATH); /* into itself */
    r = index_prepare(b);
    if (r) return card_done(r);
    make_parents(b);
    if (hal_fs_rename(a, b) != 0) return card_done(card_error());
    r = index_edit(a, b);
    if (r) return card_done(r);
    changed(c, from, "moved");
    changed(c, to, "moved");
    return card_done(INK_FS_OK);
}

/* Everything under phys, however deep: one entry at a time, nothing held open, no recursion. 0 only when nothing is left. */
static int wipe_tree(const char *phys) {
    char *path = osal_malloc(PHYS_MAX, OSAL_MEM_EXTERNAL);
    hal_dirent_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
    int r = path && e ? 0 : INK_FS_ERR_IO;
    size_t root = strlen(phys);
    if (!r) snprintf(path, PHYS_MAX, "%s", phys);
    while (!r) {
        hal_dir_t *d = hal_fs_opendir(path);
        if (!d && strlen(path) == root && hal_fs_size(path) < 0) {
            /* A size error cannot prove absence. Establish an empty root, then inspect it. */
            make_parents(path);
            if (hal_fs_mkdir(path) == 0) d = hal_fs_opendir(path);
        }
        if (!d) {
            r = INK_FS_ERR_IO;
            break;
        }
        bool found = false;
        int more;
        while (!found && (more = hal_fs_readdir(d, e)) == 1) found = strcmp(e->name, ".") && strcmp(e->name, "..");
        hal_fs_closedir(d);
        size_t n = strlen(path);
        if (found) {
            if (snprintf(path + n, PHYS_MAX - n, "/%s", e->name) >= (int)(PHYS_MAX - n)) r = INK_FS_ERR_IO;
            else if (!e->is_dir) {
                if (hal_fs_remove(path) != 0) r = INK_FS_ERR_IO;
                else r = index_edit(path, NULL);
                path[n] = 0;
            }
        } else if (more < 0) r = INK_FS_ERR_IO;
        else if (n == root) break;
        else if (hal_fs_remove(path) != 0) r = INK_FS_ERR_IO;
        else *strrchr(path, '/') = 0;
    }
    osal_free(path);
    osal_free(e);
    return r;
}

int ink_fs_wipe_reset(bool media) {
    if (FS.ready) osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = ink_fs_card_hold();
    if (!r) {
        __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
        r = wipe_tree("/content/.m");
        if (!r) r = wipe_tree("/content/.update");
        if (!r) r = wipe_tree("/cache");
        if (!r && media) r = wipe_tree("/content/books");
        if (!r && media) r = wipe_tree("/content/audio");
        r = card_done(r);
    }
    if (FS.ready) osal_mutex_unlock(FS.edits);
    return r;
}
int ink_fs_wipe_personal(void) { return ink_fs_wipe_reset(false); }

static int fs_write_atomic_unlocked(const file_context_t *c, const char *path, const void *data, size_t n) {
    char phys[PHYS_MAX], tmp[PHYS_MAX + 8];
    int r = resolve_held(c, path, true, phys, sizeof phys);
    if (r) return r;
    bool added = hal_fs_size(phys) < 0 && !strncmp(phys, "/content/", 9);
    if (added && (r = index_prepare(phys))) return card_done(r);
    snprintf(tmp, sizeof tmp, "%s" TMP_SUFFIX, phys);
    make_parents(phys);
    hal_file_t *f = hal_fs_open(tmp, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return card_done(card_error());
    ssize_t w = hal_fs_write(f, data, n);
    int closed = hal_fs_close(f);
    if (w != (ssize_t)n || closed != 0) {
        hal_fs_remove(tmp);
        int e = card_error();
        return card_done(e == INK_FS_ERR_IO ? INK_FS_ERR_NO_SPACE : e);
    }
    if (hal_fs_rename(tmp, phys) != 0) {
        hal_fs_remove(tmp);
        return card_done(card_error());
    }
    if (added) index_invalidate();
    changed(c, path, "written");
    return card_done(INK_FS_OK);
}

int ink_fs_free(uint64_t *free_bytes, uint64_t *total_bytes) {
    int r = ink_fs_card_hold();
    if (r) return r;
    hal_fs_volume_info_t vi;
    if (hal_fs_volume_info("/content", &vi) != 0) return card_done(card_error());
    if (free_bytes) *free_bytes = vi.free_bytes;
    if (total_bytes) *total_bytes = vi.total_bytes;
    return card_done(INK_FS_OK);
}

void ink_fs_refresh_state(void) {
    ink_state_set_str(FS.h_mode, FS.usb_lent ? "usb" : "normal");
    ink_state_set_bool(FS.h_usb, hal_usb_host_connected());
    bool card = hal_fs_card_present();
    ink_state_set_bool(FS.h_card, card);
    if (!card || FS.usb_lent) ink_state_set_str(FS.h_index, "unavailable");
    uint64_t free_bytes = 0;
    if (card && ink_fs_free(&free_bytes, NULL) == INK_FS_OK) ink_state_set_int(FS.h_free, (int64_t)free_bytes);
    if (!FS.index_seen || !storage_index_ready()) ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
}

static void refresh_job(void *arg) {
    (void)arg;
    ink_fs_refresh_state();
}

void fs_refresh_cb(void *arg) { /* timer task: what it does blocks on the card */
    (void)arg;
    ink_exec_post(INK_EXEC_BG, &FS.refresh_job, refresh_job, NULL);
}
static void on_card(void *ctx) {
    (void)ctx; __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    index_invalidate();
    fs_refresh_cb(NULL);
}
void ink_fs_changed_outside(void) { on_card(NULL); }

/* ------------------------------------------------------------ jobs */

static void job_failed(fjob_t *j, int err) {
    ink_svc_lock();
    j->st.err = err;
    j->st.state = INK_FS_JOB_FAILED;
    ink_svc_unlock();
}

/* Close what the walk holds. */
static void job_release(fjob_t *j) {
    while (j->depth > 0) hal_fs_closedir(j->dirs[--j->depth]);
    if (j->in) hal_fs_close(j->in);
    j->in = NULL;
    if (j->out) {
        hal_fs_close(j->out);
        hal_fs_remove(j->part); /* never half a file under its name */
    }
    j->out = NULL;
    osal_free(j->buf);
    j->buf = NULL;
}

static bool job_push(fjob_t *j, const char *dir) {
    if (j->depth >= JOB_DEPTH) return false;
    hal_dir_t *d = hal_fs_opendir(dir);
    if (!d) return false;
    j->dirs[j->depth++] = d;
    return true;
}

/* The path of the directory on top of the walk: paths are kept in from[] and to[] as the walk goes down. */
static void path_up(char *p) {
    char *s = strrchr(p, '/');
    if (s) *s = 0;
}

static bool path_down(char *p, size_t cap, const char *name) {
    size_t n = strlen(p), m = strlen(name);
    if (n + 1 + m + 1 > cap) return false;
    p[n] = '/';
    memcpy(p + n + 1, name, m + 1);
    return true;
}

/* Measure a tree, one entry per call: how much there is, whether it fits. True when it is done. */
static bool measure_step(fjob_t *j) {
    static hal_dirent_t e INK_PSRAM_BSS; /* exec.bg only */
    int r = hal_fs_readdir(j->dirs[j->depth - 1], &e);
    if (r < 0) {
        job_failed(j, card_error());
        return true;
    }
    if (r == 0) {
        hal_fs_closedir(j->dirs[--j->depth]);
        if (j->depth == 0) return true; /* from[] is the top again */
        path_up(j->from);
        return false;
    }
    if (hidden_name(e.name)) return false;
    if (e.is_dir) {
        if (!path_down(j->from, sizeof j->from, e.name) || !job_push(j, j->from)) job_failed(j, INK_FS_ERR_PATH);
        return j->st.state == INK_FS_JOB_FAILED;
    }
    j->st.bytes_total += (uint64_t)e.size;
    j->st.files_total++;
    return false;
}

/* The next piece of the file being copied. True when it is finished (j->in closed). */
static bool copy_piece(fjob_t *j) {
    size_t budget = STEP_BYTES;
    while (budget) {
        ssize_t n = hal_fs_read(j->in, j->buf, budget < JOB_BUF ? budget : JOB_BUF);
        if (n < 0) {
            job_failed(j, card_error());
            return true;
        }
        if (n == 0) {
            hal_fs_close(j->in);
            j->in = NULL;
            int c = hal_fs_close(j->out);
            j->out = NULL;
            char final_name[PHYS_MAX];
            snprintf(final_name, sizeof final_name, "%.*s", (int)(strlen(j->part) - strlen(PART_SUFFIX)), j->part);
            if (c != 0 || hal_fs_rename(j->part, final_name) != 0) {
                hal_fs_remove(j->part);
                job_failed(j, card_error());
                return true;
            }
            j->st.files_done++;
            return true;
        }
        ssize_t w = hal_fs_write(j->out, j->buf, (size_t)n);
        if (w != n) {
            int e = card_error();
            job_failed(j, e == INK_FS_ERR_IO ? INK_FS_ERR_NO_SPACE : e);
            return true;
        }
        j->st.bytes_done += (uint64_t)n;
        budget -= (size_t)n;
    }
    return false;
}

static bool start_file(fjob_t *j, const char *from, const char *to) {
    if (snprintf(j->part, sizeof j->part, "%s" PART_SUFFIX, to) >= (int)sizeof j->part) {
        job_failed(j, INK_FS_ERR_PATH);
        return false;
    }
    j->in = hal_fs_open(from, HAL_O_READ);
    j->out = j->in ? hal_fs_open(j->part, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC) : NULL;
    if (!j->in || !j->out) {
        job_failed(j, card_error());
        return false;
    }
    return true;
}

/* One entry of the walk that copies. True when the walk is over. */
static bool copy_step(fjob_t *j) {
    if (j->in) {
        copy_piece(j);
        if (j->st.state == INK_FS_JOB_FAILED) return true;
        return !j->in && j->depth == 0; /* a single file: over; in a tree the walk goes on */
    }
    if (!j->depth) return true;
    static hal_dirent_t e INK_PSRAM_BSS; /* exec.bg only */
    int r = hal_fs_readdir(j->dirs[j->depth - 1], &e);
    if (r < 0) {
        job_failed(j, card_error());
        return true;
    }
    if (r == 0) {
        hal_fs_closedir(j->dirs[--j->depth]);
        if (j->depth == 0) return true;
        path_up(j->from);
        path_up(j->to);
        return false;
    }
    if (hidden_name(e.name)) return false;
    size_t fn = strlen(j->from), tn = strlen(j->to);
    if (!path_down(j->from, sizeof j->from, e.name) || !path_down(j->to, sizeof j->to, e.name)) {
        job_failed(j, INK_FS_ERR_PATH);
        return true;
    }
    if (e.is_dir) {
        if (hal_fs_mkdir(j->to) != 0 || !job_push(j, j->from)) {
            job_failed(j, card_error());
            return true;
        }
        return false;
    }
    bool ok = start_file(j, j->from, j->to);
    j->from[fn] = 0;
    j->to[tn] = 0;
    if (!ok) return true;
    return false;
}

/* One step of the walk that deletes: what is in a directory goes first. It reads the directory afresh each
 * time: entries are not read while others are removed. True when the walk is over. */
static bool delete_step(fjob_t *j) {
    static hal_dirent_t e INK_PSRAM_BSS; /* exec.bg only */
    hal_dir_t *d = hal_fs_opendir(j->from);
    if (!d) { /* a file */
        if (hal_fs_remove(j->from) != 0) job_failed(j, card_error());
        else if (index_edit(j->from, NULL)) job_failed(j, INK_FS_ERR_IO);
        else j->st.files_done++;
        return true;
    }
    int removed = 0;
    int r;
    bool descend = false;
    while (removed < 8 && (r = hal_fs_readdir(d, &e)) > 0) {
        if (!strcmp(e.name, ".") || !strcmp(e.name, "..")) continue;
        char child[PHYS_MAX];
        if (snprintf(child, sizeof child, "%s/%s", j->from, e.name) >= (int)sizeof child) continue;
        if (e.is_dir) {
            snprintf(j->from, sizeof j->from, "%s", child);
            j->depth++;
            descend = true;
            break;
        }
        if (hal_fs_remove(child) != 0) {
            hal_fs_closedir(d);
            job_failed(j, card_error());
            return true;
        }
        if (index_edit(child, NULL)) { job_failed(j, INK_FS_ERR_IO); hal_fs_closedir(d); return true; }
        j->st.files_done++;
        removed++;
    }
    hal_fs_closedir(d);
    if (descend || removed) return false;
    /* empty: the directory itself, and up */
    if (hal_fs_remove(j->from) != 0) {
        job_failed(j, card_error());
        return true;
    }
    if (j->depth == 0) return true;
    j->depth--;
    path_up(j->from);
    return false;
}

static int job_percent(const fjob_t *j) { /* under nobody's lock: a look */
    if (j->phase == 0) return -1; /* still measuring */
    if (j->op == 1) return j->st.files_total ? (int)((uint64_t)j->st.files_done * 100 / j->st.files_total) : 0;
    return j->st.bytes_total ? (int)(j->st.bytes_done * 100 / j->st.bytes_total) : 0;
}

static ink_job_step_t job_step(void *ctx) {
    fjob_t *j = ctx;
    ink_svc_lock();
    bool cancel = j->cancel;
    ink_svc_unlock();
    if (cancel) {
        ink_svc_lock();
        j->st.state = INK_FS_JOB_CANCELLED;
        ink_svc_unlock();
        return INK_JOB_DONE;
    }
    ink_task_progress(j->task, job_percent(j), NULL);
    bool over;
    if (j->op == 1) over = delete_step(j);
    else if (j->phase == 0) {
        over = measure_step(j);
        if (over && j->st.state == INK_FS_JOB_RUNNING) {
            uint64_t free_bytes = 0;
            int r = ink_fs_free(&free_bytes, NULL);
            /* the walk restores from[] to the top on its way out: the work starts from there */
            if (r) job_failed(j, r);
            else if (j->st.bytes_total > free_bytes) job_failed(j, INK_FS_ERR_NO_SPACE);
            else if (hal_fs_mkdir(j->to) != 0 || !job_push(j, j->from)) job_failed(j, card_error());
            else {
                j->phase = 1;
                over = false;
            }
        }
    } else over = copy_step(j);
    if (j->st.state != INK_FS_JOB_RUNNING) return INK_JOB_FAILED;
    if (!over) return INK_JOB_MORE;
    ink_svc_lock();
    j->st.state = INK_FS_JOB_DONE;
    ink_svc_unlock();
    return INK_JOB_DONE;
}

static void job_finish(void *ctx, ink_job_step_t how, bool cancelled) {
    fjob_t *j = ctx;
    (void)how;
    job_release(j);
    ink_svc_lock();
    if (cancelled && j->st.state == INK_FS_JOB_RUNNING) j->st.state = INK_FS_JOB_CANCELLED;
    bool ok = j->st.state == INK_FS_JOB_DONE;
    int err = j->st.err;
    int id = j->id;
    ink_fs_job_status_t end = j->st;
    void (*on_end)(void *, const ink_fs_job_status_t *) = j->on_end;
    ink_svc_unlock();
    char say[64], object[48];
    snprintf(say, sizeof say, "%s", ok ? (j->op == 0 ? TR("拷贝好了") : TR("删好了")) : err == INK_FS_ERR_NO_SPACE ? TR("存储卡满了") : err ? TR("没做成") : TR("停下了"));
    if (snprintf(object, sizeof object, "file:%s", j->vto) >= (int)sizeof object) object[0] = 0;
    ink_task_end(j->task, ok ? INK_TASK_DONE : end.state == INK_FS_JOB_CANCELLED ? INK_TASK_CANCELLED : INK_TASK_FAILED, say, ok && j->op == 0 ? object : NULL);
    void *on_end_ctx = j->on_end_ctx;
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_int(a, "id", id);
        ink_args_bool(a, "ok", ok);
        if (!ok) ink_args_str(a, "why", err ? ink_fs_err_name(err) : "cancelled");
        ink_args_str(a, "member", j->who);
        ink_cap_emit_for("file.job_done", a, j->who);
        osal_free(a);
    }
    changed_for(j->who, j->op == 0 ? j->vto : j->vfrom, j->op == 0 ? "copied" : "removed");
    ink_probe_count(ok ? "file.jobs_done" : "file.jobs_failed");
    ink_svc_lock();
    FS.job_active = false;
    ink_svc_unlock();
    if (on_end) on_end(on_end_ctx, &end);
}

static void job_cancel_cb(void *ctx) { ink_fs_job_cancel((int)(intptr_t)ctx); }

static int job_begin_unlocked(const file_context_t *c, int op, const char *from, const char *to) {
    char a[PHYS_MAX], b[PHYS_MAX] = "";
    if (op == 1 && virtual_root(from)) return INK_FS_ERR_DENIED;
    int r = resolve_held(c, from, op == 1, a, sizeof a);
    if (r) return r;
    if (op == 0) r = resolve_for(c, to, true, b, sizeof b);
    if (r) return card_done(r);
    if (hal_fs_size(a) < 0) return card_done(hal_fs_card_present() ? INK_FS_ERR_NOT_FOUND : card_error());
    if (op == 0) {
        if (hal_fs_size(b) >= 0) return card_done(INK_FS_ERR_EXISTS);
        size_t an = strlen(a);
        if (!strncmp(a, b, an) && b[an] == '/') return card_done(INK_FS_ERR_PATH); /* into itself */
        r = index_prepare(b);
        if (r) return card_done(r);
    }
    ink_svc_lock();
    if (FS.job_active) {
        ink_svc_unlock();
        return card_done(INK_FS_ERR_BUSY);
    }
    FS.job_active = true;
    fjob_t *j = &FS.job;
    int id = ++FS.next_job;
    memset(j, 0, sizeof *j);
    snprintf(j->who, sizeof j->who, "%s", c->member);
    
    j->id = id;
    j->op = op;
    j->st.state = INK_FS_JOB_RUNNING;
    ink_svc_unlock();
    ink_fs_card_release(); /* job_active now holds the card. */
    snprintf(j->from, sizeof j->from, "%s", a);
    snprintf(j->to, sizeof j->to, "%s", b);
    snprintf(j->vfrom, sizeof j->vfrom, "%s", from);
    if (to) snprintf(j->vto, sizeof j->vto, "%s", to);
    if (op == 0) {
        make_parents(b);
        j->buf = osal_malloc(JOB_BUF, OSAL_MEM_EXTERNAL);
        hal_dir_t *d = j->buf ? hal_fs_opendir(a) : NULL;
        if (j->buf && d) { /* a tree: measure first */
            j->dirs[j->depth++] = d;
        } else if (j->buf) { /* a single file: already measured */
            int64_t size = hal_fs_size(a);
            uint64_t free_bytes = 0;
            int fr = ink_fs_free(&free_bytes, NULL);
            int e = fr ? fr : (uint64_t)size > free_bytes ? INK_FS_ERR_NO_SPACE : 0;
            if (!e && !start_file(j, a, b)) e = j->st.err;
            if (e) {
                job_release(j);
                ink_svc_lock();
                FS.job_active = false;
                ink_svc_unlock();
                return e;
            }
            j->st.bytes_total = (uint64_t)size;
            j->st.files_total = 1;
            j->phase = 1;
        } else r = INK_FS_ERR_IO;
    }
    char title[64];
    const char *base = strrchr(from, '/');
    snprintf(title, sizeof title, "%s %.48s", op == 0 ? TR("拷贝") : TR("删除"), base ? base + 1 : from);
    j->task = ink_task_begin_for(op == 0 ? "file.copy" : "file.delete", title, j->who, job_cancel_cb, (void *)(intptr_t)id);
    ink_job_t job = {.name = "file", .step = job_step, .finish = job_finish, .ctx = j, .step_budget_ms = 500};
    if (r || ink_job_submit(&job) < 0) {
        job_release(j);
        ink_svc_lock();
        FS.job_active = false;
        ink_svc_unlock();
        ink_task_end(j->task, INK_TASK_FAILED, TR("没做成"), NULL);
        return r ? r : INK_FS_ERR_BUSY;
    }
    return id;
}

int fs_job_begin_for(const file_context_t *c, int op, const char *from, const char *to) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    int r = job_begin_unlocked(c, op, from, to);
    osal_mutex_unlock(FS.edits);
    return r;
}

static int job_begin(int op, const char *from, const char *to) {
    file_context_t c = current_context();
    return fs_job_begin_for(&c, op, from, to);
}

int ink_fs_copy_start(const char *from, const char *to) { return job_begin(0, from, to); }
int ink_fs_delete_start(const char *path) { return job_begin(1, path, NULL); }

int ink_fs_forget_member(const char *id) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    ink_svc_lock();
    bool busy = FS.job_active;
    for (int i = 0; i < MAX_OPEN; i++) busy |= FS.files[i].used && (!strcmp(FS.files[i].member, id) || !strncmp(FS.files[i].path, "/cache/", 7));
    ink_svc_unlock();
    if (busy) { osal_mutex_unlock(FS.edits); return INK_FS_ERR_BUSY; }
    char phys[PHYS_MAX];
    if (snprintf(phys, sizeof phys, "/content/.m/%s", id) >= (int)sizeof phys) {
        osal_mutex_unlock(FS.edits);
        return INK_FS_ERR_PATH;
    }
    int r = ink_fs_card_hold();
    if (r) { osal_mutex_unlock(FS.edits); return r; }
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    r = wipe_tree(phys);
    if (!r) r = wipe_tree("/cache");
    if (!r && hal_fs_size(phys) >= 0 && hal_fs_remove(phys) != 0) r = INK_FS_ERR_IO;
    r = card_done(r);
    osal_mutex_unlock(FS.edits);
    return r;
}

int ink_fs_job_status(int id, ink_fs_job_status_t *out) {
    ink_svc_lock();
    bool hit = FS.job.id == id && id > 0 && ink_member_exists(FS.job.who);
    if (hit) *out = FS.job.st;
    ink_svc_unlock();
    return hit ? INK_FS_OK : INK_FS_ERR_NOT_FOUND;
}

void ink_fs_job_on_end(int id, void (*fn)(void *ctx, const ink_fs_job_status_t *st), void *ctx) {
    ink_svc_lock();
    if (FS.job.id == id) {
        FS.job.on_end = fn;
        FS.job.on_end_ctx = ctx;
    }
    ink_svc_unlock();
}

void ink_fs_job_cancel(int id) {
    ink_svc_lock();
    if (FS.job.id == id && FS.job_active) FS.job.cancel = true;
    ink_svc_unlock();
}

/* ------------------------------------------------------------ the USB disk */

int ink_fs_on_release(ink_fs_release_fn fn, void *ctx) {
    ink_svc_lock();
    int r = -1;
    if (fn && FS.nrel < MAX_RELEASE) {
        FS.rel[FS.nrel].fn = fn;
        FS.rel[FS.nrel++].ctx = ctx;
        r = 0;
    }
    ink_svc_unlock();
    return r;
}

static void usb_tell(void) {
    char *block = osal_malloc(sizeof(ink_args_t), OSAL_MEM_EXTERNAL);
    if (!block) return;
    ink_args_t *a = (ink_args_t *)block;
    ink_args_init(a);
    ink_args_bool(a, "connected", hal_usb_host_connected());
    ink_args_bool(a, "lent", FS.usb_lent);
    ink_cap_emit("usb.changed", a);
    osal_free(block);
}

int ink_fs_card_hold(void) {
    if (!FS.ready) return hal_fs_card_present() ? INK_FS_OK : INK_FS_ERR_NO_CARD;
    int r = INK_FS_OK;
    ink_svc_lock();
    if (FS.usb_lent) r = INK_FS_ERR_USB;
    else if (!hal_fs_card_present()) r = INK_FS_ERR_NO_CARD;
    else FS.nopen++;
    ink_svc_unlock();
    if (r) ink_state_set_str(FS.h_index, "unavailable");
    return r;
}

void ink_fs_card_release(void) {
    if (!FS.ready) return;
    ink_svc_lock();
    FS.nopen--;
    ink_svc_unlock();
}

int ink_usb_disk_start(void) {
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    if (FS.usb_lent) return INK_FS_OK;
    if (!hal_usb_host_connected()) return INK_FS_ERR_NO_HOST;
    if (!hal_fs_card_present()) return INK_FS_ERR_NO_CARD;
    /* holders close their files; a job is not interrupted behind its owner's back */
    ink_svc_lock();
    int nrel = FS.nrel;
    ink_svc_unlock();
    for (int i = 0; i < nrel; i++) FS.rel[i].fn(FS.rel[i].ctx);
    uint64_t end = osal_now_us() + RELEASE_WAIT_US;
    for (;;) {
        ink_svc_lock();
        bool busy = FS.nopen > 0 || FS.job_active;
        if (!busy) FS.usb_lent = true; /* from here every access is refused: nothing can open in between */
        ink_svc_unlock();
        if (!busy) break;
        if (osal_now_us() >= end) {
            ink_probe_count("usb.refused_files_in_use");
            return INK_FS_ERR_BUSY;
        }
        osal_task_delay_us(50000);
    }
    int r = index_prepare("/content/");
    if (r || hal_usb_disk_start() != 0) {
        ink_svc_lock();
        FS.usb_lent = false;
        ink_svc_unlock();
        return r ? r : hal_fs_card_present() ? INK_FS_ERR_NO_HOST : INK_FS_ERR_NO_CARD;
    }
    ink_power_hold(INK_NEED_NO_LOCK);
    ink_power_hold(INK_NEED_AWAKE); /* USB has to answer */
    ink_state_set_str(FS.h_mode, "usb");
    ink_probe_count("usb.disk_started");
    usb_tell();
    return INK_FS_OK;
}

int ink_usb_disk_stop(void) {
    file_context_t c = current_context();
    ink_svc_lock();
    bool lent = FS.usb_lent;
    ink_svc_unlock();
    if (!lent) return INK_FS_OK;
    int r = hal_usb_disk_stop();
    ink_svc_lock();
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    FS.usb_lent = false;
    ink_svc_unlock();
    ink_power_release(INK_NEED_AWAKE);
    ink_power_release(INK_NEED_NO_LOCK);
    ink_fs_refresh_state();
    ink_probe_count(r == 0 ? "usb.disk_stopped" : "usb.disk_stopped_bad");
    usb_tell();
    if (r != 0) return INK_FS_ERR_NO_CARD; /* never formatted here: a computer can repair it */
    changed(&c, "/", "rescan"); /* the computer may have changed anything */
    return INK_FS_OK;
}

bool ink_usb_disk_active(void) {
    ink_svc_lock();
    bool lent = FS.usb_lent;
    ink_svc_unlock();
    return lent;
}

static void usb_job(void *arg) { /* exec.bg */
    (void)arg;
    if (ink_usb_disk_active() && hal_usb_host_done()) ink_usb_disk_stop();
    else {
        ink_state_set_bool(FS.h_usb, hal_usb_host_connected());
        usb_tell();
    }
}

static void on_usb(void *ctx) { /* the platform's event task */
    (void)ctx;
    ink_exec_post(INK_EXEC_BG, &FS.usb_job, usb_job, NULL);
}

/* ------------------------------------------------------------ start */

static const ink_state_def_t ST_FREE = {.id = "storage.free", .type = INK_ST_INT, .title = "存储卡剩余空间（字节）", .min_interval_ms = 60000};
static const ink_state_def_t ST_CARD = {.id = "storage.card", .type = INK_ST_BOOL, .title = "存储卡在且能用"};
static const ink_state_def_t ST_MODE = {.id = "storage.mode", .type = INK_ST_STR, .title = "存储卡在哪（normal 系统用，usb 借给电脑）"};
static const ink_state_def_t ST_USB = {.id = "usb.connected", .type = INK_ST_BOOL, .title = "连着电脑"};
static const ink_state_def_t ST_INDEX = {.id = "file.index_state", .type = INK_ST_STR, .title = "文件索引状态（ready 可用，unavailable 不可用，changed 已换卡）"};

int ink_file_service_init(void) {
    FS.revision = 1;
    FS.edits = osal_mutex_create();
    if (!FS.edits) return -1;
    FS.ready = true;
    FS.h_free = ink_state_define(&ST_FREE);
    FS.h_card = ink_state_define(&ST_CARD);
    FS.h_mode = ink_state_define(&ST_MODE);
    FS.h_usb = ink_state_define(&ST_USB);
    FS.h_index = ink_state_define(&ST_INDEX);
    if (ink_db_register(&INDEX_DEF)) return -1;
    if (ink_db_register(&INDEX_DONE_DEF)) return -1;
    ink_db_close(ink_db_open(&INDEX_DEF, NULL));
    ink_db_close(ink_db_open(&INDEX_DONE_DEF, NULL));
    if (ink_cap_subscribe("member.changed", thumb_clear, NULL, INK_EXEC_BG) ||
        ink_state_watch("member.current", thumb_member, NULL, INK_EXEC_BG)) return -1;
    ink_state_set_str(FS.h_index, "unavailable");
    ink_state_set_str(FS.h_mode, "normal");
    ink_state_set_bool(FS.h_card, hal_fs_card_present());
    ink_state_set_bool(FS.h_usb, hal_usb_host_connected());
    hal_usb_set_callback(on_usb, NULL);
    hal_fs_set_card_callback(on_card, NULL);
    return ink_file_caps_init();
}

uint32_t ink_fs_revision(void) {
    if (osal_mutex_lock(FS.edits, 0) != OSAL_OK) return 0;
    ink_svc_lock();
    bool busy = FS.job_active || FS.usb_lent;
    for (int i = 0; i < MAX_OPEN; i++) busy |= FS.files[i].used && FS.files[i].writable;
    uint32_t revision = busy ? 0 : __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    ink_svc_unlock();
    osal_mutex_unlock(FS.edits);
    return revision;
}

uint32_t ink_fs_file_revision(ink_fs_file_t *f) { return f ? f->revision : 0; }

static bool inverse_ready(uint32_t revision) {
    if (!revision || revision != __atomic_load_n(&FS.revision, __ATOMIC_RELAXED) || FS.job_active || ink_usb_disk_active()) return false;
    for (int i = 0; i < MAX_OPEN; i++)
        if (FS.files[i].used && FS.files[i].writable) return false;
    return true;
}

int fs_open_for(const file_context_t *c, const char *path, int mode, ink_fs_file_t **out) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (mode & (INK_FS_WRITE | INK_FS_APPEND | INK_FS_CREATE)) __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    int r = fs_open_unlocked(c, path, mode, out);
    osal_mutex_unlock(FS.edits);
    return r;
}

int ink_fs_open(const char *path, int mode, ink_fs_file_t **out) {
    file_context_t c = current_context();
    return fs_open_for(&c, path, mode, out);
}

int ink_fs_open_for(const char *member, const char *path, int mode, ink_fs_file_t **out) {
    file_context_t c = member_context(member);
    return fs_open_for(&c, path, mode, out);
}

int64_t ink_fs_write(ink_fs_file_t *f, const void *buf, size_t n) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    int64_t r = fs_write_unlocked(f, buf, n);
    osal_mutex_unlock(FS.edits);
    return r;
}

int fs_mkdir_for(const file_context_t *c, const char *path) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    int r = fs_mkdir_unlocked(c, path);
    osal_mutex_unlock(FS.edits);
    return r;
}

int ink_fs_mkdir_for(const char *member, const char *path) {
    file_context_t c = member_context(member);
    return fs_mkdir_for(&c, path);
}

int ink_fs_mkdir(const char *path) {
    file_context_t c = current_context();
    return fs_mkdir_for(&c, path);
}

int fs_remove_for(const file_context_t *c, const char *path, uint32_t revision) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = INK_FS_ERR_BUSY;
    if (!revision || inverse_ready(revision)) { __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED); r = fs_remove_unlocked(c, path); }
    osal_mutex_unlock(FS.edits);
    return r;
}
int ink_fs_remove_if_revision(const char *path, uint32_t revision) {
    file_context_t c = current_context();
    return fs_remove_for(&c, path, revision);
}
int ink_fs_remove(const char *path) { return ink_fs_remove_if_revision(path, 0); }

int ink_fs_remove_for(const char *member, const char *path) {
    file_context_t c = member_context(member);
    return fs_remove_for(&c, path, 0);
}

int fs_move_for(const file_context_t *c, const char *from, const char *to, uint32_t revision, uint32_t *written) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = INK_FS_ERR_BUSY;
    if (!revision || inverse_ready(revision)) {
        __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
        r = fs_move_unlocked(c, from, to);
        if (!r && written) *written = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    }
    osal_mutex_unlock(FS.edits);
    return r;
}
int ink_fs_move_if_revision(const char *from, const char *to, uint32_t revision, uint32_t *written) {
    file_context_t c = current_context();
    return fs_move_for(&c, from, to, revision, written);
}
int ink_fs_move(const char *from, const char *to) { return ink_fs_move_if_revision(from, to, 0, NULL); }

int fs_write_atomic_for(const file_context_t *c, const char *path, const void *data, size_t n) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
    int r = fs_write_atomic_unlocked(c, path, data, n);
    osal_mutex_unlock(FS.edits);
    return r;
}
int ink_fs_write_atomic(const char *path, const void *data, size_t n) {
    file_context_t c = current_context();
    return fs_write_atomic_for(&c, path, data, n);
}

static int restore_file(const char *member, unsigned stage, const char *path, const uint8_t expected_sha256[32], uint32_t revision) {
    if (!member || !path || (!expected_sha256 && !revision) || stage >= 128 || strncmp(path, "/me/", 4)) return INK_FS_ERR_PATH;
    file_context_t c = member_context(member);
    if (!ink_member_io_begin(member)) return INK_FS_ERR_DENIED;
    int r = ink_fs_card_hold();
    if (r) { ink_member_io_end(member); return r; }
    uint8_t *buf = revision ? NULL : osal_malloc(JOB_BUF, OSAL_MEM_EXTERNAL);
    if (!revision && !buf) { ink_fs_card_release(); ink_member_io_end(member); return INK_FS_ERR_IO; }
    if (osal_mutex_lock(FS.edits, revision ? 0 : OSAL_WAIT_FOREVER) != OSAL_OK) {
        osal_free(buf); ink_fs_card_release(); ink_member_io_end(member);
        return INK_FS_ERR_BUSY;
    }
    char target[PHYS_MAX], source[PHYS_MAX], root[INK_LAYOUT_PATH_MAX];
    r = resolve_for(&c, path, true, target, sizeof target);
    if (!r) r = index_prepare(target);
    if (!r && FS.job_active) r = INK_FS_ERR_BUSY;
    for (int i = 0; !r && i < MAX_OPEN; i++)
        if (FS.files[i].used && FS.files[i].writable) r = INK_FS_ERR_BUSY;
    uint8_t hash[32] = {0};
    if (!r && revision && !inverse_ready(revision)) r = INK_FS_ERR_EXISTS;
    if (!revision) {
        if (!r && hal_fs_size(target) >= 0) {
            hal_file_t *f = hal_fs_open(target, HAL_O_READ);
            if (!f) r = INK_FS_ERR_IO;
            else {
                ink_sha256_t sha; ink_sha256_init(&sha);
                ssize_t n;
                while ((n = hal_fs_read(f, buf, JOB_BUF)) > 0) ink_sha256_update(&sha, buf, (size_t)n);
                int closed = hal_fs_close(f);
                if (n < 0 || closed != 0) r = INK_FS_ERR_IO;
                else ink_sha256_final(&sha, hash);
            }
        }
        if (!r && memcmp(hash, expected_sha256, 32)) r = INK_FS_ERR_EXISTS;
    }
    if (!r) {
        if (ink_layout_member_files(root, sizeof root, member) < 0) r = INK_FS_ERR_PATH;
        else {
            snprintf(source, sizeof source, "%s/.backup/down%u", root, stage);
            if (hal_fs_rename(source, target) != 0) r = INK_FS_ERR_IO;
            else { __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED); changed(&c, path, "restored"); }
        }
    }
    osal_mutex_unlock(FS.edits);
    osal_free(buf); ink_fs_card_release(); ink_member_io_end(member);
    return r;
}

int ink_fs_restore(const char *member, unsigned stage, const char *path, const uint8_t expected_sha256[32]) {
    return restore_file(member, stage, path, expected_sha256, 0);
}

int ink_fs_restore_if_revision(const char *member, unsigned stage, const char *path, uint32_t revision) {
    if (!revision) return INK_FS_ERR_EXISTS;
    return restore_file(member, stage, path, NULL, revision);
}

/* In memory and in completion records; index payloads use the codec below. */
typedef struct { uint8_t card[16]; char path[PHYS_MAX - 16]; int64_t size, mtime; } index_rec_t;
static size_t index_path_bytes(const char *path, size_t cap) {
    size_t n = strnlen(path, cap);
    return n > 9 && n < cap && n < sizeof(((index_rec_t *)0)->path) && !strncmp(path, "/content/", 9) ? n + 1 : 0;
}
static bool index_decode(const ink_rec_t *rec, const uint8_t *raw, index_rec_t *e) {
    if (rec->schema < 1 || rec->schema > INDEX_DEF.schema ||
        (rec->schema == 3 ? rec->len <= 32 || rec->len > sizeof *e : rec->len != sizeof *e)) return false;
    const char *path = (const char *)raw + (rec->schema == 3 ? 32 : 16);
    size_t cap = rec->schema == 3 ? (size_t)rec->len - 32 : rec->schema == 2 ? sizeof e->path : PHYS_MAX;
    size_t n = index_path_bytes(path, cap);
    if (!n || (rec->schema == 3 && n != cap)) return false;
    memset(e, 0, sizeof *e);
    memcpy(e->card, raw, 16); memcpy(e->path, path, n);
    if (rec->schema >= 2) {
        size_t at = rec->schema == 3 ? 16 : 16 + sizeof e->path;
        memcpy(&e->size, raw + at, 8); memcpy(&e->mtime, raw + at + 8, 8);
    }
    return true;
}
static int index_get(ink_db_t *db, uint32_t id, ink_rec_t *rec, index_rec_t *e) {
    uint8_t raw[sizeof *e];
    int r = ink_db_get(db, id, rec, raw, sizeof raw);
    return r ? r : index_decode(rec, raw, e) ? 0 : INK_FS_ERR_IO;
}
static int index_next(ink_db_t *db, ink_db_cursor_t *cur, ink_rec_t *rec, index_rec_t *e) {
    uint8_t raw[sizeof *e];
    int r = ink_db_next(db, cur, rec, raw, sizeof raw);
    return r == 1 && !index_decode(rec, raw, e) ? INK_FS_ERR_IO : r;
}
static int index_put(ink_db_t *db, uint32_t id, const index_rec_t *e, uint32_t *written, uint32_t flags) {
    uint8_t raw[sizeof *e];
    size_t n = index_path_bytes(e->path, sizeof e->path);
    if (!n) return INK_DB_ERR_ARG;
    memcpy(raw, e->card, 16); memcpy(raw + 16, &e->size, 8); memcpy(raw + 24, &e->mtime, 8);
    memcpy(raw + 32, e->path, n);
    ink_db_write_t w = {.id = id, .data = raw, .len = 32 + n, .schema = INDEX_DEF.schema, .flags = flags};
    return ink_db_put(db, &w, written);
}
static struct {
    bool family, saving, full, pruned;
    ink_db_cursor_t prune;
    char indexed[INK_MEMBERS_MAX][INK_MEMBER_ID_MAX], member[INK_MEMBER_ID_MAX];
    uint32_t revision;
    uint64_t bytes[2][4];
} storage INK_PSRAM_BSS;
static bool storage_index_scanned(void) {
    if (__atomic_load_n(&FS.index_dirty, __ATOMIC_RELAXED) || !storage.family) return false;
    for (int i = 0; ink_member_at(i); i++)
        if (strcmp(storage.indexed[i], ink_member_at(i)->id)) return false;
    return true;
}
static bool storage_index_ready(void) { return !storage.saving && storage_index_scanned(); }

static void index_invalidate(void) {
    __atomic_store_n(&FS.index_dirty, true, __ATOMIC_RELAXED);
    ink_state_set_str(FS.h_index, hal_fs_card_present() && !FS.usb_lent ? "organizing" : "unavailable");
    ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
}
/* Mark FAT before the edit; never wait for internal flash while holding edits. */
static int index_prepare(const char *path) {
    if (!FS.ready || strncmp(path, "/content/", 9)) return 0;
    if (hal_fs_size(INDEX_DIRTY) < 0) {
        hal_file_t *f = hal_fs_open(INDEX_DIRTY, HAL_O_WRITE | HAL_O_CREATE);
        if (!f) return card_error();
        int synced = hal_fs_sync(f), closed = hal_fs_close(f);
        if (synced || closed) return INK_FS_ERR_IO;
        ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
    }
    return 0;
}
static int index_open(uint8_t card[16], ink_db_t **db) {
    hal_file_t *f = hal_fs_open(INDEX_CARD, HAL_O_READ);
    int64_t size = f ? 16 : -1;
    if (!f) {
        hal_dir_t *d = hal_fs_opendir("/content");
        if (!d) return card_error();
        hal_dirent_t e; int more;
        while ((more = hal_fs_readdir(d, &e)) == 1)
            if (!strcmp(e.name, ".file-index-card")) break;
        hal_fs_closedir(d);
        if (more) return card_error();
    }
    if (!f) f = hal_fs_open(INDEX_CARD ".tmp", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    if (!f) return card_error();
    if (size < 0) hal_fs_new_card_id(card);
    ssize_t n = size < 0 ? hal_fs_write(f, card, 16) : hal_fs_read(f, card, 16);
    uint8_t extra;
    if (size >= 0 && n == 16 && hal_fs_read(f, &extra, 1) != 0) n = -1;
    int r = size < 0 ? hal_fs_sync(f) : 0;
    int closed = hal_fs_close(f);
    if (n != 16 || r || closed || (size >= 0 && size != 16)) return INK_FS_ERR_IO;
    if (size < 0 && hal_fs_rename(INDEX_CARD ".tmp", INDEX_CARD)) return card_error();
    if ((!FS.index_seen || memcmp(card, FS.index_card, 16)) && hal_fs_size(INDEX_DIRTY) >= 0)
        __atomic_store_n(&FS.index_dirty, true, __ATOMIC_RELAXED);
    ink_state_set_str(FS.h_index, FS.index_seen && memcmp(card, FS.index_card, 16) ? "changed" : storage_index_ready() ? "ready" : "organizing");
    if (memcmp(card, FS.index_card, 16)) { ink_thumb_cache_destroy(FS.thumbs); FS.thumbs = NULL; memset(&storage, 0, sizeof storage); }
    memcpy(FS.index_card, card, 16); FS.index_seen = true;
    if (!db) return 0;
    *db = ink_db_open(&INDEX_DEF, NULL);
    return *db ? 0 : INK_FS_ERR_IO;
}

static int index_save(ink_db_t *db, uint32_t id, const index_rec_t *e, uint32_t *written) {
    int r = e ? index_put(db, id, e, written, 0) : ink_db_del(db, id, 0);
    return r ? (r == INK_DB_ERR_FULL ? INK_FS_ERR_INDEX_FULL : INK_FS_ERR_IO) : 0;
}

static bool index_under(const char *path, const char *root) {
    size_t n = strlen(root);
    return !strncmp(path, root, n) && (!path[n] || path[n] == '/');
}

static int index_edit(const char *from, const char *to) {
    if (!FS.ready || hal_fs_size(INDEX_CARD) < 0) return 0;
    uint8_t card[16]; ink_db_t *db = NULL;
    int r = index_open(card, &db);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; int n = 0;
    while (!r && (n = index_next(db, &cur, &rec, &e)) == 1) {
        if (memcmp(card, e.card, 16) || !index_under(e.path, from)) continue;
        if (to) {
            char path[sizeof e.path];
            if (snprintf(path, sizeof path, "%s%s", to, e.path + strlen(from)) >= (int)sizeof path) { r = INK_FS_ERR_PATH; break; }
            memcpy(e.path, path, strlen(path) + 1);
        }
        if (!to) ink_thumb_cache_remove(FS.thumbs, rec.id);
        r = index_save(db, rec.id, to ? &e : NULL, NULL);
    }
    ink_db_close(db);
    return r ? r : n < 0 ? INK_FS_ERR_IO : 0;
}

static ink_fs_type_t index_type(const char *path) {
    const char *ext = strrchr(path, '.'); char lower[12];
    if (!ext || strlen(ext) >= sizeof lower) return INK_FS_OTHER;
    size_t i; for (i = 0; ext[i]; i++) lower[i] = ext[i] >= 'A' && ext[i] <= 'Z' ? (char)(ext[i] + 'a' - 'A') : ext[i];
    lower[i] = 0;
    static const char *const kinds[] = {"", ".png.jpg.jpeg.bmp.gif.webp.", ".mp3.wav.ogg.opus.flac.m4a.", ".txt.md.csv.json.html.htm.epub.pdf.mobi.azw.azw3."};
    for (int k = INK_FS_IMAGE; k <= INK_FS_TEXT; k++) {
        const char *p = strstr(kinds[k], lower);
        if (p && p[strlen(lower)] == '.') return (ink_fs_type_t)k;
    }
    return INK_FS_OTHER;
}

#define INDEX_BATCH 16
typedef struct { uint32_t hash, id; } index_slot_t;
typedef struct {
    file_context_t context;
    ink_fs_dir_t *dirs[JOB_DEPTH];
    int depth;
    char path[INK_FS_PATH_MAX], root[INK_FS_PATH_MAX];
    uint32_t revision;
    uint8_t card[16];
    index_slot_t known[INK_FS_INDEX_MAX * 2];
    uint32_t missing[INK_FS_INDEX_MAX];
    unsigned nmissing;
} index_walk_t;
static index_walk_t *index_walk;
static bool index_manual;
static void index_walk_close(index_walk_t *w) {
    while (w->depth) ink_fs_dir_close(w->dirs[--w->depth]);
}
static int index_walk_open(index_walk_t *w, const file_context_t *c, const char *root) {
    memset(w, 0, sizeof *w); w->context = *c;
    w->revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    snprintf(w->path, sizeof w->path, "%s", root);
    snprintf(w->root, sizeof w->root, "%s", root);
    int r = fs_dir_open_for(c, root, &w->dirs[0]);
    if (!r) w->depth = 1;
    return r == INK_FS_ERR_NOT_FOUND && virtual_root(root) ? 0 : r;
}
static index_slot_t *index_slot(index_walk_t *w, ink_db_t *db, const char *path) {
    uint32_t hash = 2166136261u;
    for (const char *p = path; *p; p++) hash = (hash ^ (uint8_t)*p) * 16777619u;
    unsigned at = hash % (INK_FS_INDEX_MAX * 2);
    for (;;) {
        index_slot_t *slot = &w->known[at];
        if (!slot->id) { slot->hash = hash; return slot; }
        if (slot->hash == hash) {
            index_rec_t old; ink_rec_t rec;
            if (!index_get(db, slot->id, &rec, &old) && !strcmp(old.path, path)) return slot;
        }
        at = (at + 1) % (INK_FS_INDEX_MAX * 2);
    }
}
/* Read once outside edits; this scratch set lasts only for the current walk. */
static int index_walk_load(index_walk_t *w, ink_db_t *db, const uint8_t card[16]) {
    memcpy(w->card, card, 16);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; int n;
    while ((n = index_next(db, &cur, &rec, &e)) == 1) {
        if (memcmp(card, e.card, 16)) continue;
        index_slot(w, db, e.path)->id = rec.id;
        /* Check absence only for metadata matches of a new path, not every known record. */
        if (rec.schema >= 2)
            w->missing[w->nmissing++] = rec.id;
    }
    return n < 0 ? INK_FS_ERR_IO : 0;
}
static uint32_t index_renamed(index_walk_t *w, ink_db_t *db, const index_rec_t *e) {
    /* Metadata is from the last scan: an edited then renamed file may receive a new id after power loss. */
    uint32_t id = 0;
    for (unsigned i = 0; i < w->nmissing; i++) {
        index_rec_t old; ink_rec_t rec;
        if (!w->missing[i] || index_get(db, w->missing[i], &rec, &old)) continue;
        if (old.size != e->size || old.mtime != e->mtime || hal_fs_size(old.path) >= 0) continue;
        if (id) return 0; /* Ambiguous metadata must not steal another file's id. */
        id = w->missing[i];
    }
    return id;
}
static int index_walk_step(index_walk_t *w, ink_db_t *db, const uint8_t card[16]) {
    int r = 0; ink_fs_stat_t st;
    for (int steps = 0; !r && w->depth && steps < INDEX_BATCH; steps++) {
        int n = ink_fs_dir_next(w->dirs[w->depth - 1], &st);
        if (n < 0) { r = n; break; }
        if (!n) { ink_fs_dir_close(w->dirs[--w->depth]); if (w->depth) *strrchr(w->path, '/') = 0; continue; }
        size_t len = strlen(w->path);
        if (snprintf(w->path + len, sizeof w->path - len, "/%s", st.name) >= (int)(sizeof w->path - len)) { r = INK_FS_ERR_PATH; break; }
        if (!st.is_dir) {
            index_rec_t e = {0}; memcpy(e.card, card, 16);
            r = resolve_for(&w->context, w->path, false, e.path, sizeof e.path);
            e.size = st.size; e.mtime = w->dirs[w->depth - 1]->e.mtime;
            if (!r && e.mtime < 0) r = INK_FS_ERR_IO;
            index_slot_t *slot = r ? NULL : index_slot(w, db, e.path);
            if (slot && !slot->id) slot->id = index_renamed(w, db, &e);
            index_rec_t old; ink_rec_t rec;
            if (slot && (!slot->id || index_get(db, slot->id, &rec, &old) ||
                rec.schema != INDEX_DEF.schema || memcmp(&old, &e, sizeof old))) {
                int added = index_put(db, slot->id, &e, &slot->id, INK_DB_LAZY);
                if (added == INK_DB_ERR_FULL && !index_manual) storage.full = true;
                else if (added) r = added == INK_DB_ERR_FULL ? INK_FS_ERR_INDEX_FULL : INK_FS_ERR_IO;
            }
            w->path[len] = 0;
        } else if (w->depth == JOB_DEPTH) r = INK_FS_ERR_PATH;
        else { r = fs_dir_open_for(&w->context, w->path, &w->dirs[w->depth]); if (!r) w->depth++; }
    }
    storage.revision = 0;
    return r;
}
static void index_done(const file_context_t *c, const char *root, bool durable) {
    bool was_ready = storage_index_ready(), was_scanned = storage_index_scanned();
    if (!strcmp(root, "/family")) storage.family = true;
    for (int i = 0; ink_member_at(i); i++) {
        char member_root[64]; snprintf(member_root, sizeof member_root, "/members/%s", ink_member_at(i)->id);
        if (!strcmp(root, member_root) || (!strcmp(root, "/me") && !strcmp(c->member, ink_member_at(i)->id)))
            snprintf(storage.indexed[i], sizeof storage.indexed[i], "%s", ink_member_at(i)->id);
    }
    /* A root scan never finishes the card: only the background prune ends with root "". */
    if (*root && storage_index_scanned() && (storage.saving || !was_scanned)) durable = false;
    storage.saving = !durable;
    if (storage_index_ready()) {
        ink_svc_cancel(&FS.index_at);
        if (!was_ready) for (int i = 0; ink_member_at(i); i++) changed_for(ink_member_at(i)->id, "/", "indexed");
        ink_state_set_str(FS.h_index, "ready");
    } else if (*root && storage_index_scanned()) ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
}
#define INDEX_WAIT_US 10000000ll /* a compaction rewrites the whole index (1024 records) in one write: seconds */
/* The synchronous first-page API uses the same batches, releasing edits for durability. */
static int index_scan(const file_context_t *c, ink_db_t *db, const uint8_t card[16], const char *root) {
    index_manual = true;
    if (index_walk) index_walk->revision = 0;
    index_walk_t *w = osal_calloc(1, sizeof *w, OSAL_MEM_EXTERNAL);
    if (!w) { index_manual = false; return INK_FS_ERR_IO; }
    uint32_t version = ink_db_last_ver(db); bool was_ready = storage_index_ready();
    int r = index_walk_open(w, c, root);
    osal_mutex_unlock(FS.edits);
    if (!r) r = index_walk_load(w, db, card);
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    do {
        uint64_t begin = osal_now_us();
        if (!r) r = index_walk_step(w, db, card);
        ink_probe_value("file.index.hold_us", (double)(osal_now_us() - begin));
        osal_mutex_unlock(FS.edits);
        if (ink_db_wait(db, ink_db_last_ver(db), INDEX_WAIT_US) && !r) r = INK_FS_ERR_IO; /* a full index stays INDEX_FULL */
        osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
        uint8_t now[16]; ink_db_t *check = NULL;
        if (!r) r = index_open(now, &check);
        if (!r && memcmp(card, now, 16)) r = INK_FS_ERR_CARD_CHANGED;
        ink_db_close(check);
    } while (!r && w->depth);
    if (!r) index_done(c, root, true);
    if (!r && was_ready && version != ink_db_last_ver(db)) changed(c, root, "indexed");
    index_walk_close(w); osal_free(w);
    index_manual = false;
    return r;
}
/* Completion is independent of the index payload encoding. */
static void index_signature(index_rec_t *e, const uint8_t card[16]) {
    memset(e, 0, sizeof *e); memcpy(e->card, card, 16);
    size_t used = 0;
    for (int i = 0; ink_member_at(i); i++)
        used += (size_t)snprintf(e->path + used, sizeof e->path - used, "/%s", ink_member_at(i)->id);
}
static uint32_t index_saved(ink_db_t *db, const uint8_t card[16], index_rec_t *e, ink_rec_t *rec) {
    ink_db_cursor_t cur = {0};
    while (db && ink_db_next(db, &cur, rec, e, sizeof *e) == 1)
        if (!memcmp(card, e->card, 16)) return rec->id;
    return 0;
}
/* All roots are finished: renamed records already point at their new paths. */
static int index_prune(ink_db_t *db, const uint8_t card[16]) {
    index_rec_t e; ink_rec_t rec; int n = 1;
    uint32_t stale[INDEX_BATCH]; unsigned count = 0;
    for (int i = 0; i < INDEX_BATCH; i++) {
        n = index_next(db, &storage.prune, &rec, &e);
        if (n < 0) return INK_FS_ERR_IO;
        if (!n) break;
        int exists = memcmp(card, e.card, 16) ? 1 : hal_fs_exists(e.path);
        /* Only a confirmed absence deletes; an I/O error rechecks the whole card next pass. */
        if (exists < 0) { memset(&storage.prune, 0, sizeof storage.prune); return INK_FS_ERR_IO; }
        if (exists) continue;
        stale[count++] = rec.id;
    }
    int r = count ? index_prepare("/content/") : 0;
    if (r) return r;
    /* No card I/O between deletes: the existing log can collect this batch. */
    for (unsigned i = 0; i < count; i++) {
        if (__atomic_load_n(&FS.index_dirty, __ATOMIC_RELAXED) || !hal_fs_card_present()) return INK_FS_ERR_BUSY;
        r = index_save(db, stale[i], NULL, NULL);
        if (r) return r;
        ink_thumb_cache_remove(FS.thumbs, stale[i]);
        storage.pruned = true; storage.revision = 0;
    }
    return n;
}
static void index_background(void *arg) {
    (void)arg;
    ink_probe_count("file.index.background_runs");
    if (ink_storage_pending() || ink_jobs_held() || ink_exec_waiting(INK_EXEC_BG)) { ink_svc_at(&FS.index_at, osal_now_us() + 500000ull, index_refresh_cb, NULL); return; }
    ink_db_t *loaded = ink_db_open(&INDEX_DEF, NULL), *done = ink_db_open(&INDEX_DONE_DEF, NULL);
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (index_manual) { osal_mutex_unlock(FS.edits); ink_db_close(done); ink_db_close(loaded); ink_svc_at(&FS.index_at, osal_now_us() + 500000ull, index_refresh_cb, NULL); return; }
    uint64_t begin = osal_now_us();
    int r = ink_fs_card_hold(); bool held = !r;
    uint8_t card[16]; ink_db_t *db = NULL;
    if (!r) r = index_open(card, &db);
    if (index_walk && (r || !index_walk->revision || memcmp(index_walk->card, card, 16))) { index_walk_close(index_walk); osal_free(index_walk); index_walk = NULL; }
    index_rec_t expected, saved; ink_rec_t rec;
    uint32_t done_id = r ? 0 : index_saved(done, card, &saved, &rec);
    if (!r) index_signature(&expected, card);
    if (!r && done && __atomic_exchange_n(&FS.index_dirty, false, __ATOMIC_RELAXED)) {
        memset(&storage, 0, sizeof storage);
        if (index_walk) { index_walk_close(index_walk); osal_free(index_walk); index_walk = NULL; }
        if (done_id && ink_db_del(done, done_id, 0)) r = INK_FS_ERR_IO;
        done_id = 0;
    }
    bool complete = done_id && rec.schema >= 1 && rec.schema <= INDEX_DONE_DEF.schema && !memcmp(&saved, &expected, sizeof saved);
    if (!r && inverse_ready(__atomic_load_n(&FS.revision, __ATOMIC_RELAXED)) && !storage_index_scanned()) {
        if (!done) r = INK_FS_ERR_IO;
        else if (!index_walk && complete) {
            storage.family = true;
            for (int i = 0; ink_member_at(i); i++) snprintf(storage.indexed[i], sizeof storage.indexed[i], "%s", ink_member_at(i)->id);
            ink_state_set_str(FS.h_index, "ready");
        } else {
            if (!index_walk) {
                file_context_t c = current_context(); c.adult = true;
                char root[64] = "/family";
                if (storage.family) for (int i = 0; ink_member_at(i); i++) {
                    if (!strcmp(storage.indexed[i], ink_member_at(i)->id)) continue;
                    snprintf(root, sizeof root, "/members/%s", ink_member_at(i)->id); break;
                }
                index_walk = osal_calloc(1, sizeof *index_walk, OSAL_MEM_EXTERNAL);
                r = index_walk ? index_walk_open(index_walk, &c, root) : INK_FS_ERR_IO;
                ink_probe_value("file.index.hold_us", (double)(osal_now_us() - begin));
                osal_mutex_unlock(FS.edits);
                if (!r) r = index_walk_load(index_walk, db, card);
                osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
                begin = osal_now_us();
            }
            if (!r && (index_manual || !index_walk->revision)) r = INK_FS_ERR_BUSY;
            if (!r) r = index_walk_step(index_walk, db, card);
        }
    }
    ink_probe_value("file.index.hold_us", (double)(osal_now_us() - begin));
    if (!r && index_walk && !index_walk->depth && index_walk->revision == __atomic_load_n(&FS.revision, __ATOMIC_RELAXED))
        index_done(&index_walk->context, index_walk->root, false);
    bool scanned = !r && storage_index_scanned();
    bool finished = scanned;
    if (scanned && !complete) {
        storage.saving = true;
        int n = inverse_ready(__atomic_load_n(&FS.revision, __ATOMIC_RELAXED)) ? index_prune(db, card) : 1;
        if (n < 0) r = n;
        if (!n && storage.full) {
            if (storage.pruned) index_invalidate(); /* Retry paths deferred while the index was full. */
            else r = INK_FS_ERR_INDEX_FULL;
        }
        finished = !r && !n;
    }
    osal_mutex_unlock(FS.edits);
    if (scanned) ink_db_flush_background(db);
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (index_walk && (r || !index_walk->depth)) {
        index_walk_close(index_walk); osal_free(index_walk); index_walk = NULL;
    }
    if (!r && finished && storage_index_scanned() && ink_db_durable_ver(db) == ink_db_last_ver(db)) index_done(NULL, "", true);
    if (!r && storage_index_ready() && done && !complete) {
        ink_db_write_t put = {.id = done_id, .data = &expected, .len = sizeof expected, .schema = INDEX_DONE_DEF.schema};
        if (ink_db_put(done, &put, NULL)) r = INK_FS_ERR_IO;
    }
    bool durable = !r && done && ink_db_durable_ver(db) == ink_db_last_ver(db) && ink_db_durable_ver(done) == ink_db_last_ver(done);
    if (durable && storage_index_ready() && inverse_ready(__atomic_load_n(&FS.revision, __ATOMIC_RELAXED)) &&
        hal_fs_size(INDEX_DIRTY) >= 0 && hal_fs_remove(INDEX_DIRTY)) r = INK_FS_ERR_IO;
    bool more = !r && held && !FS.job_active && (!storage_index_ready() || !durable);
    osal_mutex_unlock(FS.edits);
    ink_db_close(done); ink_db_close(db); ink_db_close(loaded);
    if (held) ink_fs_card_release();
    if (!r && more) ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);
}

static int index_query(const file_context_t *c, const char *root, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *out) {
    int r = ink_fs_card_hold();
    if (r) return r;
    uint8_t card[16]; ink_db_t *db = NULL; char phys[PHYS_MAX] = "";
    r = FS.job_active && FS.job.st.state == INK_FS_JOB_RUNNING ? INK_FS_ERR_BUSY : root ? resolve_for(c, root, false, phys, sizeof phys) : 0;
    if (!r && root && strncmp(phys, "/content", 8)) r = INK_FS_ERR_DENIED;
    if (!r) r = index_open(card, &db);
    if (!r && root && !*cursor) r = index_scan(c, db, card, root);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {.id = *cursor, .started = *cursor != 0};
    int more = 0;
    while (!r && (more = root ? index_next(db, &cur, &rec, &e) : index_get(db, *cursor, &rec, &e) == 0 ? 1 : 0) == 1) {
        if (memcmp(e.card, card, 16)) { if (!root) r = INK_FS_ERR_CARD_CHANGED; else continue; }
        if (r) break;
        if (root && !strcmp(phys, "/content") && !strncmp(e.path, "/content/.", 10)) continue;
        if (root && !index_under(e.path, phys)) continue;
        char own[PHYS_MAX]; snprintf(own, sizeof own, "/content/.m/%s", c->member);
        const char *suffix = e.path + 8, *prefix = "/family";
        if (index_under(e.path, own)) { suffix = e.path + strlen(own); prefix = "/me"; }
        else if (!strncmp(e.path, "/content/.m/", 12)) { suffix = e.path + 12; prefix = "/members/"; }
        if (snprintf(out->path, sizeof out->path, "%s%s", prefix, suffix) >= (int)sizeof out->path) { r = INK_FS_ERR_PATH; break; }
        ink_fs_stat_t st;
        int access = fs_stat_for(c, out->path, &st);
        if (access) { if (!root) { r = access; break; } continue; }
        out->type = index_type(e.path);
        if (root && type != INK_FS_ANY && type != out->type) continue;
        out->id = rec.id; out->size = st.size; *cursor = rec.id;
        break;
    }
    ink_db_close(db); ink_fs_card_release();
    return r ? r : more;
}
int fs_index_for(const file_context_t *c, const char *root, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *out) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = index_query(c, root, type, cursor, out);
    osal_mutex_unlock(FS.edits); return r;
}

int fs_thumb_for(const file_context_t *c, uint32_t id, ink_image_t *out) {
    *out = (ink_image_t){0};
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    ink_fs_entry_t e; int r = index_query(c, NULL, INK_FS_ANY, &id, &e);
    r = r == 1 ? 0 : r ? r : INK_FS_ERR_NOT_FOUND;
    int held = ink_fs_card_hold(); char phys[PHYS_MAX];
    if (!r) r = held ? held : resolve_for(c, e.path, false, phys, sizeof phys);
    uint32_t epoch = ink_member_epoch();
    if (r || FS.thumb_epoch != epoch) { ink_thumb_cache_destroy(FS.thumbs); FS.thumbs = NULL; }
    FS.thumb_epoch = epoch;
    int64_t stamp = r ? -1 : hal_fs_mtime(phys);
    if (!r && stamp < 0) r = INK_FS_ERR_IO;
    if (!r && !FS.thumbs) FS.thumbs = ink_thumb_cache_create();
    if (!r && !FS.thumbs) r = INK_FS_ERR_IO;
    ink_image_t im = {0};
    uint64_t version = (uint64_t)stamp ^ ((uint64_t)(r ? 0 : e.size) * UINT64_C(0x9e3779b97f4a7c15));
    if (!r && ink_image_thumbnail(FS.thumbs, id, version, NULL, 0, &im)) {
        void *data = e.size > 0 && e.size <= INK_IMAGE_MAX_INPUT ? osal_malloc((size_t)e.size, OSAL_MEM_EXTERNAL) : NULL;
        hal_file_t *f = data ? hal_fs_open(phys, HAL_O_READ) : NULL;
        int64_t n = f ? hal_fs_read(f, data, (size_t)e.size) : -1;
        int closed = f ? hal_fs_close(f) : -1;
        if (n != e.size || closed) r = INK_FS_ERR_IO;
        else {
            /* MEDIA owns the cache while decoding; file operations can continue. */
            ink_thumb_cache_t *cache = FS.thumbs;
            FS.thumbs = NULL;
            uint32_t revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
            osal_mutex_unlock(FS.edits);
            int decoded = ink_image_thumbnail(cache, id, version, data, (size_t)e.size, &im);
            osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
            if (epoch != ink_member_epoch() || revision != __atomic_load_n(&FS.revision, __ATOMIC_RELAXED)) {
                ink_thumb_cache_destroy(cache);
                r = INK_FS_ERR_BUSY;
            } else {
                FS.thumbs = cache;
                if (decoded) r = INK_FS_ERR_IO;
            }
        }
        osal_free(data);
    }
    if (!r) {
        *out = im; out->pixels = osal_malloc((size_t)im.width * im.height, OSAL_MEM_EXTERNAL);
        if (out->pixels) memcpy(out->pixels, im.pixels, (size_t)im.width * im.height);
        else { *out = (ink_image_t){0}; r = INK_FS_ERR_IO; }
    }
    if (!held) ink_fs_card_release();
    osal_mutex_unlock(FS.edits); return r;
}

int ink_fs_index(const char *path, ink_fs_type_t type, uint32_t *cursor, ink_fs_entry_t *out) {
    file_context_t c = current_context();
    return fs_index_for(&c, path, type, cursor, out);
}
int ink_fs_lookup(uint32_t id, ink_fs_entry_t *out) {
    file_context_t c = current_context();
    int r = fs_index_for(&c, NULL, INK_FS_ANY, &id, out);
    return r == 1 ? 0 : r ? r : INK_FS_ERR_NOT_FOUND;
}

/* Cache one member's view; the index workflow owns directory scans. */
static void storage_error(ink_call_t *c, int r) {
    c->rc.status = INK_RC_FAILED;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", ink_fs_err_name(r));
}
static void storage_usage(ink_call_t *call) {
    file_context_t c;
    if (!fs_context_for_call(&c, call)) return;
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = ink_fs_card_hold();
    bool held = !r;
    uint8_t card[16]; ink_db_t *db = NULL;
    uint64_t bytes[2][4] = {{0}};
    uint32_t revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    if (!r && !inverse_ready(revision)) r = INK_FS_ERR_BUSY;
    if (!r) r = index_open(card, &db);
    bool ready = !r && storage_index_ready();
    bool cached = ready && storage.revision == revision && !strcmp(storage.member, c.member);
    if (cached) memcpy(bytes, storage.bytes, sizeof bytes);
    uint32_t version = db ? ink_db_last_ver(db) : 0;
    osal_mutex_unlock(FS.edits);
    char own[64]; snprintf(own, sizeof own, "/content/.m/%s", c.member);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; int more = 0;
    while (!r && ready && !cached && (more = index_next(db, &cur, &rec, &e)) == 1) {
        if (memcmp(e.card, card, 16)) continue;
        int64_t size = hal_fs_size(e.path);
        if (size < 0) continue;
        ink_fs_type_t type = index_type(e.path);
        int k = type == INK_FS_TEXT ? 0 : type == INK_FS_AUDIO ? 1 : type == INK_FS_IMAGE ? 2 : 3;
        bytes[1][k] += (uint64_t)size;
        if (index_under(e.path, own)) bytes[0][k] += (uint64_t)size;
    }
    if (more < 0) r = INK_FS_ERR_IO;
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    ready = ready && inverse_ready(revision) && version == ink_db_last_ver(db) && storage_index_ready();
    if (!r && ready && !cached) {
        memcpy(storage.bytes, bytes, sizeof bytes); storage.revision = revision;
        snprintf(storage.member, sizeof storage.member, "%s", c.member);
    }
    ink_db_close(db);
    if (held) ink_fs_card_release();
    osal_mutex_unlock(FS.edits);
    if (r) { storage_error(call, r); return; }
    if (!ready) { ink_cap_result(call, "{\"status\":\"organizing\"}"); return; }
    char out[512]; ink_jw_t w; ink_jw_init(&w, out, sizeof out);
    static const char *const keys[] = {"books", "audio", "photos", "other"};
    ink_jw_raw(&w, "{\"status\":\"ready\"");
    for (int scope = 0; scope < 2; scope++) {
        ink_jw_raw(&w, scope ? ",\"device\":{" : ",\"member\":{");
        for (int k = 0; k < 4; k++) {
            if (k) ink_jw_raw(&w, ",");
            ink_jw_str(&w, keys[k]); ink_jw_raw(&w, ":"); ink_jw_int(&w, (int64_t)bytes[scope][k]);
        }
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "}"); ink_cap_result(call, out);
}

/* Fixed roots, hidden service data excluded; suffixes are never directory rules. */
static int storage_sweep(char *path, unsigned depth, bool cache, bool remove, uint64_t bytes[3]) {
    if (depth == JOB_DEPTH) return INK_FS_ERR_PATH;
    hal_dir_t *dir = hal_fs_opendir(path);
    if (!dir) return hal_fs_size(path) < 0 ? 0 : INK_FS_ERR_IO;
    hal_dirent_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
    int r = e ? 0 : INK_FS_ERR_IO, more = 0; size_t len = strlen(path);
    while (!r && (more = hal_fs_readdir(dir, e)) == 1) {
        if (e->name[0] == '.' && strcmp(e->name, ".m")) continue;
        if (!path_down(path, PHYS_MAX, e->name)) { r = INK_FS_ERR_PATH; break; }
        const char *ext = strrchr(e->name, '.');
        int k = cache ? 1 : ext && !strcmp(ext, TMP_SUFFIX) ? 0 : ext && !strcmp(ext, PART_SUFFIX) ? 2 : -1;
        if (e->is_dir) r = storage_sweep(path, depth + 1, cache, remove, bytes);
        else if (k >= 0) {
            if (remove && hal_fs_remove(path)) r = INK_FS_ERR_IO;
            else bytes[k] += (uint64_t)e->size;
        }
        path[len] = 0;
    }
    if (more < 0) r = INK_FS_ERR_IO;
    osal_free(e); hal_fs_closedir(dir); return r;
}
static void storage_cleanup(ink_call_t *call) {
    file_context_t c;
    if (!fs_context_for_call(&c, call)) return;
    bool preview = !strcmp(call->cap->id, "file.cleanup_preview");
    bool cache_only = !strcmp(call->cap->id, "file.cache_clear");
    int64_t expected = 0; ink_args_get_int(&call->args, "revision", &expected);
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    int r = ink_fs_card_hold(); bool held = !r;
    uint32_t revision = __atomic_load_n(&FS.revision, __ATOMIC_RELAXED);
    if (!r && !inverse_ready(preview ? revision : (uint32_t)expected)) r = INK_FS_ERR_BUSY;
    bool attempted = !preview && !r;
    uint64_t bytes[3] = {0}; char path[PHYS_MAX] = "/cache";
    if (!r) r = storage_sweep(path, 0, true, !preview, bytes);
    if (!r && !cache_only) {
        snprintf(path, sizeof path, "/content");
        r = storage_sweep(path, 0, false, !preview, bytes);
    }
    if (attempted) {
        __atomic_add_fetch(&FS.revision, 1, __ATOMIC_RELAXED);
        ink_thumb_cache_destroy(FS.thumbs); FS.thumbs = NULL;
    }
    if (held) ink_fs_card_release();
    osal_mutex_unlock(FS.edits);
    if (!preview && !r) { ink_fs_refresh_state(); changed(&c, "/", "cleaned"); }
    if (r) { storage_error(call, r); return; }
    char out[240];
    snprintf(out, sizeof out, "{\"temporary\":%llu,\"cache\":%llu,\"partial\":%llu,\"bytes\":%llu,\"revision\":%u}",
        (unsigned long long)bytes[0], (unsigned long long)bytes[1], (unsigned long long)bytes[2],
        (unsigned long long)(bytes[0] + bytes[1] + bytes[2]), (unsigned)revision);
    ink_cap_result(call, out);
}
static const ink_cap_t STORAGE_CAPS[] = {
    {.id = "file.usage", .locked_owner_only = true, .app = "files", .title = "读取当前成员和整台设备的书、音频、照片和其他文件字节数", .effect = INK_FX_READ,
     .output = "{\"type\":\"object\"}", .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .run = storage_usage},
    {.id = "file.cleanup_preview", .locked_owner_only = true, .app = "files", .title = "列出临时文件、缓存和未传完文件可清理的字节数", .effect = INK_FX_READ,
     .output = "{\"type\":\"object\"}", .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .run = storage_cleanup},
    {.id = "file.cache_clear", .locked_owner_only = true, .app = "files", .title = "清理可重新生成的缓存", .effect = INK_FX_LOCAL_WRITE,
     .input = "{\"type\":\"object\",\"properties\":{\"revision\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"revision\"]}",
     .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .run = storage_cleanup},
    {.id = "file.cleanup", .locked_owner_only = true, .app = "files", .title = "清理临时文件", .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true,
     .ask = TR_NOOP("清理临时文件和未传完的文件，删了找不回来；缓存可以重新生成"),
     .input = "{\"type\":\"object\",\"properties\":{\"revision\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"revision\"]}",
     .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .run = storage_cleanup}};
int ink_storage_usage_caps_init(void) { return ink_cap_register(STORAGE_CAPS, sizeof STORAGE_CAPS / sizeof *STORAGE_CAPS); }
