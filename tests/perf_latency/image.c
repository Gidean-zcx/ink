/* Observe a cold thumbnail without holding or delaying any task. */
#define ink_image_thumbnail original_thumbnail
#include "../../system/services/media/image.c"
#undef ink_image_thumbnail
#include "ink/probe.h"
static bool decoding;
bool perf_decode_active(void) { return __atomic_load_n(&decoding, __ATOMIC_ACQUIRE); }
int ink_image_thumbnail(ink_thumb_cache_t *cache, uint64_t object, uint64_t version,
                        const void *data, size_t size, ink_image_t *out) {
    if (!data || !size) return original_thumbnail(cache, object, version, data, size, out);
    uint64_t start = osal_now_us();
    __atomic_store_n(&decoding, true, __ATOMIC_RELEASE);
    int r = original_thumbnail(cache, object, version, data, size, out);
    ink_probe_value("perf.thumbnail_decode_ms", (osal_now_us() - start) / 1000.0);
    __atomic_store_n(&decoding, false, __ATOMIC_RELEASE);
    return r;
}
