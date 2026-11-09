/* Explicit old payloads keep migration tests independent of Git history. */
#include "../file_index_3/boundary.c"

bool ink_test_compact_seed(unsigned schema) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL), *done = ink_db_open(&INDEX_DONE_DEF, NULL);
    uint8_t raw[216], old[216]; ink_rec_t rec; ink_db_cursor_t cur = {0};
    int r = db && done ? 0 : -1, n = 0;
    while (!r && (n = ink_db_next(db, &cur, &rec, raw, sizeof raw)) == 1) {
        const char *path = (const char *)raw + (rec.schema == 3 ? 32 : 16);
        memset(old, 0, sizeof old); memcpy(old, raw, 16);
        memcpy(old + 16, path, strlen(path) + 1);
        if (schema == 2) memcpy(old + 200, raw + (rec.schema == 3 ? 16 : 200), 16);
        ink_db_write_t put = {.id = rec.id, .data = old, .len = sizeof old, .schema = (uint8_t)schema};
        r = ink_db_put(db, &put, NULL);
    }
    if (!r && n < 0) r = n;
    cur = (ink_db_cursor_t){0};
    while (!r && (n = ink_db_next(done, &cur, &rec, raw, sizeof raw)) == 1) {
        ink_db_write_t put = {.id = rec.id, .data = raw, .len = sizeof raw, .schema = (uint8_t)schema};
        r = ink_db_put(done, &put, NULL);
    }
    if (!r && n < 0) r = n;
    if (!r) r = ink_db_wait(db, ink_db_last_ver(db), INDEX_WAIT_US);
    if (!r) r = ink_db_wait(done, ink_db_last_ver(done), INDEX_WAIT_US);
    ink_db_close(done); ink_db_close(db);
    return !r;
}

bool ink_test_compact_records(unsigned schema, unsigned rewritten) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    uint8_t raw[216]; ink_rec_t rec; ink_db_cursor_t cur = {0}; int n = 0; unsigned count = 0;
    bool ok = db != NULL;
    while (db && (n = ink_db_next(db, &cur, &rec, raw, sizeof raw)) == 1) {
        unsigned expected = rec.id == rewritten ? 3 : schema;
        const char *path = (const char *)raw + (expected == 3 ? 32 : 16);
        size_t len = expected == 3 ? 32 + strnlen(path, sizeof raw - 32) + 1 : 216;
        ok = ok && rec.schema == expected && rec.len == len;
        if (expected == 3 && rec.schema == expected) {
            int64_t size, mtime;
            memcpy(&size, raw + 16, 8); memcpy(&mtime, raw + 24, 8);
            ok = ok && size == hal_fs_size(path) && mtime == hal_fs_mtime(path);
        }
        count++;
    }
    ink_probe_value("test.compact.records", count);
    ink_db_close(db);
    return ok && n == 0 && count == 3;
}

bool ink_test_compact_completion(unsigned schema) {
    ink_db_t *db = ink_db_open(&INDEX_DONE_DEF, NULL);
    index_rec_t saved, expected; ink_rec_t rec;
    uint32_t id = db ? index_saved(db, FS.index_card, &saved, &rec) : 0;
    index_signature(&expected, FS.index_card);
    bool ok = id && rec.schema == schema && rec.len == sizeof saved && !memcmp(&saved, &expected, sizeof saved);
    ink_db_close(db);
    return ok;
}

bool ink_test_compact_rewrite(unsigned id) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    uint8_t raw[216]; ink_rec_t rec; index_rec_t e = {0};
    int r = db ? ink_db_get(db, id, &rec, raw, sizeof raw) : -1;
    if (!r) {
        memcpy(e.card, raw, 16);
        snprintf(e.path, sizeof e.path, "%.183s", (char *)raw + (rec.schema == 3 ? 32 : 16));
        e.size = hal_fs_size(e.path); e.mtime = hal_fs_mtime(e.path);
        r = index_save(db, id, &e, NULL);
    }
    if (!r) r = ink_db_wait(db, ink_db_last_ver(db), INDEX_WAIT_US);
    ink_db_close(db);
    return !r;
}

bool ink_test_compact_long(bool create) {
    static const ink_db_def_t def = {.name = "SYNTH.compact", .scope = INK_DB_DEVICE, .schema = 3};
    ink_db_t *db = ink_db_open(&def, NULL);
    index_rec_t e = {.size = 12345, .mtime = 67890};
    memcpy(e.card, "SYNTH_CARD_00001", 16);
    memcpy(e.path, "/content/", 9);
    for (int i = 0; i < 47; i++) memcpy(e.path + 9 + i * 3, "测", 3);
    memcpy(e.path + 150, "a.txt", 6);
    int r = db ? 0 : -1;
    if (!r && create) r = index_save(db, 1, &e, NULL);
    if (!r && create) r = ink_db_wait(db, ink_db_last_ver(db), INDEX_WAIT_US);
    uint8_t raw[216]; ink_rec_t rec; int64_t size = 0, mtime = 0;
    if (!r) r = ink_db_get(db, 1, &rec, raw, sizeof raw);
    if (!r) { memcpy(&size, raw + 16, 8); memcpy(&mtime, raw + 24, 8); }
    bool ok = !r && rec.schema == 3 && rec.len == 188 && !memcmp(raw, e.card, 16) &&
        size == e.size && mtime == e.mtime && !memcmp(raw + 32, e.path, 156);
    index_walk_t *w = osal_calloc(1, sizeof *w, OSAL_MEM_EXTERNAL);
    if (!r) r = w ? index_walk_load(w, db, e.card) : -1;
    ok = ok && !r && index_slot(w, db, e.path)->id == 1;
    ink_probe_value("test.compact.long_path_bytes", strlen(e.path));
    osal_free(w); ink_db_close(db);
    return ok;
}
