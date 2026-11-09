/* Expose reference counts only in this executable, using the real store. */
#include "../../system/services/data/datastore.c"

int ink_fw_gap_refs(const char *member) {
    int refs = -1;
    lock();
    for (int i = 0; i < MAX_COLLECTIONS; i++) {
        ink_db_t *c = &DB.c[i];
        if (c->used && c->loaded && !strcmp(c->def->name, "gap.entries") && !strcmp(c->member, member)) refs = c->refs;
    }
    unlock();
    return refs;
}
