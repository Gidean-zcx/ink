/*
 * Structured data store (foundation §5.5, system services design §3): small
 * collections of records, kept per member, durable, readable in pages. An open
 * collection is all in PSRAM, so reads never touch the flash and never wait for
 * a refresh; writes go to an append log through the storage service, and the log
 * is folded into a snapshot now and then. A power cut at any moment leaves a
 * consistent collection: what was durable stays.
 *
 * Any task; nothing here waits for the flash except opening a collection for the
 * first time and ink_db_wait.
 */
#ifndef INK_SYS_DATA_H
#define INK_SYS_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INK_DB_MEMBER = 0, INK_DB_DEVICE } ink_db_scope_t;

enum {
    INK_DB_OK = 0,
    INK_DB_ERR_ARG = -1,
    INK_DB_ERR_FULL = -2,      /* over the collection's limit, or too much waits to be written: refused, nothing changed */
    INK_DB_ERR_NOMEM = -3,
    INK_DB_ERR_TOO_LARGE = -4, /* a record over INK_DB_RECORD_MAX */
    INK_DB_ERR_NOT_FOUND = -5,
};

#define INK_DB_RECORD_MAX 8192
#define INK_DB_NAME_MAX 40

/* A collection's description (static). Names start with their owner: "notify.inbox", "reader.words". */
typedef struct {
    const char *name;
    const char *title;      /* for people and the model */
    ink_db_scope_t scope;
    uint8_t schema;         /* version of what new records hold */
    bool json;              /* records are JSON text */
    bool ai_read, ai_write; /* reachable through data.list, data.get / data.put, data.delete (JSON collections only) */
    uint32_t max_records;   /* 0 = no limit */
    size_t max_bytes;       /* of record contents; 0 = 256 KiB */
    bool drop_oldest;       /* at a limit the oldest records go (by sort), instead of refusing the write */
} ink_db_def_t;

typedef struct ink_db ink_db_t;

typedef struct {
    uint32_t id;
    uint32_t ver;      /* the collection's change counter at its last change */
    int64_t sort;
    int64_t updated_s; /* UTC seconds, 0 if the clock was unknown */
    uint16_t len;      /* of the content, also when the copy was cut */
    uint8_t src;       /* ink_source_t: who wrote it */
    uint8_t schema;
    bool deleted;      /* only from ink_db_changes */
} ink_rec_t;

enum { INK_DB_LAZY = 1 }; /* may wait up to 60 s for a batch (positions): lost if the power fails before */

typedef struct {
    uint32_t id;         /* 0 = a new id */
    int64_t sort;        /* the order of pages, usually a time */
    const void *data;
    size_t len;
    uint8_t schema;      /* 0 = the collection's */
    uint8_t src;         /* ink_source_t */
    uint32_t flags;      /* INK_DB_LAZY */
} ink_db_write_t;

/* A position in the sort order: by key, so inserts and deletes between pages cannot skip or repeat. */
typedef struct {
    int64_t sort;
    uint32_t id;
    bool started;
    bool desc; /* newest first */
} ink_db_cursor_t;

/** After the storage service, the resource manager and the shared lock and timer. */
int ink_data_init(void);
/** List it for data.collections and the AI (the definition must outlive the program). Up to 32. */
int ink_db_register(const ink_db_def_t *def);
/** A registered collection by name, or NULL. */
const ink_db_def_t *ink_db_find_def(const char *name);
const ink_db_def_t *ink_db_def_at(int i);

/** Open (reference counted). member NULL = the current member; device collections ignore it. The first
 *  open reads the files and may wait for a refresh. NULL: no memory, or a format newer than this firmware. */
ink_db_t *ink_db_open(const ink_db_def_t *def, const char *member);
void ink_db_close(ink_db_t *db);

/** Accepted, in memory, on its way to the flash: `ver` of this change is ink_db_last_ver() right after,
 *  durable once ink_db_durable_ver() reaches it. */
int ink_db_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *out_id);
/** An id for a record about to be written, for records that carry their own id inside. */
uint32_t ink_db_new_id(ink_db_t *db);
int ink_db_del(ink_db_t *db, uint32_t id, uint8_t src);
/** One record: the first cap bytes of its content into buf (buf may be NULL). 0 or INK_DB_ERR_NOT_FOUND. */
int ink_db_get(ink_db_t *db, uint32_t id, ink_rec_t *out, void *buf, size_t cap);
/** The next record after the cursor: 1 (filled in), 0 at the end, < 0 on an error. */
int ink_db_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap);
int ink_db_count(ink_db_t *db);

/* Sync: every change after `since_ver`, deleted records included (their tombstones), in change order: 1, 0, < 0. */
int ink_db_changes(ink_db_t *db, uint32_t *since_ver, ink_rec_t *out, void *buf, size_t cap);
/** The hub has everything up to `ver`: older tombstones may go. */
void ink_db_ack(ink_db_t *db, uint32_t ver);

uint32_t ink_db_last_ver(ink_db_t *db);
uint32_t ink_db_durable_ver(ink_db_t *db);
/** The newest change the hub has confirmed (ink_db_ack). */
uint32_t ink_db_acked_ver(ink_db_t *db);
/** Hand what waits to the storage service now. */
void ink_db_flush(ink_db_t *db);
/** Wait until `ver` is durable (not in the UI task): 0, or -1 after the timeout. */
int ink_db_wait(ink_db_t *db, uint32_t ver, int64_t timeout_us);
/** The last write failed (a write retried later, what is in memory stays): non-zero until one works. */
int ink_db_error(ink_db_t *db);
/** Everything that waits, of every collection: locking, power off, low battery. Any task. */
void ink_db_flush_all(void);
/** A member is gone: what is held of theirs goes, unwritten or not. */
void ink_db_forget_member(const char *member);
/** Nothing waits to be written, anywhere (the storage service's queue is checked separately). */
bool ink_db_idle(void);
/** The state storage.user_free, measured now (blocks, walks the file system: at boot; it follows by itself after each folding). */
void ink_data_refresh_state(void);

#ifdef __cplusplus
}
#endif
#endif
