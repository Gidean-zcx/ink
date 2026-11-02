/*
 * Simulated heap pools (internal SRAM, DMA-capable internal, external PSRAM),
 * each a real TLSF arena of the size given by the device profile, so the
 * system sees the same capacity limits and fragmentation as on the target.
 */
#ifndef INK_SIM_POOLS_H
#define INK_SIM_POOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/osal.h"

typedef enum { POOL_INTERNAL = 0, POOL_DMA = 1, POOL_EXTERNAL = 2, POOL_COUNT } pool_id_t;

void pools_init(size_t internal_bytes, size_t dma_bytes, size_t external_bytes);
/* `always_internal_max`: default-policy blocks up to this size go internal first (ESP-IDF SPIRAM_MALLOC_ALWAYSINTERNAL). */
void pools_set_policy(size_t always_internal_max);
void *pools_alloc(size_t size, uint32_t caps, const char *owner);
void *pools_realloc(void *p, size_t size, uint32_t caps, const char *owner);
void pools_free(void *p);
size_t pools_block_size(void *p);
void pools_stats(pool_id_t id, osal_mem_stats_t *out);
const char *pools_name(pool_id_t id);
/** Sample current stats into min/largest trackers (called periodically by the runner). */
void pools_sample(void);
/** Largest free block low-water since boot. */
size_t pools_min_largest(pool_id_t id);
/** Report allocation failures to the warning sink. */
typedef void (*pools_warn_fn)(const char *msg);
void pools_set_warn(pools_warn_fn fn);
bool pools_owns(const void *p);
int pools_pool_of(const void *p); /* pool_id_t or -1 */
/* Attribution of live/peak bytes per owner tag (task name or osal_mem_tag()). */
int pools_tag_count(void);
void pools_tag_info(int i, const char **tag, size_t cur[POOL_COUNT], size_t peak[POOL_COUNT], size_t *live);

#endif
