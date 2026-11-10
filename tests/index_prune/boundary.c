#include "../file_index_3/boundary.c"
#include "image_fixtures.h"

void ink_test_prune_measure(void) { max_hold = 0; }
void ink_test_prune_phase(const char *name) { ink_probe_value(name, (double)max_hold); }

unsigned ink_test_prune_largest(void) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    ink_db_cursor_t cur = {0}; ink_rec_t rec; unsigned largest = 0;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1)
        if (rec.id > largest) largest = rec.id;
    ink_db_close(db);
    return largest;
}

bool ink_test_prune_record(unsigned id) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    ink_rec_t rec;
    bool found = db && ink_db_get(db, id, &rec, NULL, 0) == 0;
    ink_db_close(db);
    return found;
}

bool ink_test_prune_partial(void) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    bool partial = storage.family && !storage_index_scanned() && index_walk && index_walk->depth;
    osal_mutex_unlock(FS.edits);
    return partial;
}

bool ink_test_prune_thumb(unsigned id, bool seed) {
    ink_image_t image;
    ink_thumb_cache_t *cache = seed ? ink_thumb_cache_create() : NULL;
    bool found = seed && !ink_image_thumbnail(cache, id, 1, black_png, sizeof black_png, &image);
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (seed) {
        ink_thumb_cache_destroy(FS.thumbs); FS.thumbs = cache;
    } else found = !ink_image_thumbnail(FS.thumbs, id, 1, NULL, 0, &image);
    osal_mutex_unlock(FS.edits);
    return found;
}
