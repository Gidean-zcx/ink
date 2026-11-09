#include "file_index_boundary.c"
#include "../../system/services/media/image.c"

uint64_t ink_test_thumb_version(uint32_t id) {
    uint64_t version = 0;
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (FS.thumbs) for (unsigned i = 0; i < FS.thumbs->count; i++)
        if (FS.thumbs->entries[i].object == id) version = FS.thumbs->entries[i].version;
    osal_mutex_unlock(FS.edits);
    return version;
}

size_t ink_test_thumb_bytes(void) {
    size_t bytes = 0;
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    if (FS.thumbs) for (unsigned i = 0; i < FS.thumbs->count; i++)
        bytes += (size_t)FS.thumbs->entries[i].image.width * FS.thumbs->entries[i].image.height;
    osal_mutex_unlock(FS.edits);
    return bytes;
}
