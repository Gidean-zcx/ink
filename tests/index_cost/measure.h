#ifndef INK_INDEX_COST_MEASURE_H
#define INK_INDEX_COST_MEASURE_H
#include <stddef.h>
#include <stdint.h>
void ink_cost_begin(const char *path, int kind, size_t len, uint64_t queued, uint64_t begin);
void ink_cost_end(int result);
void ink_cost_wait(const char *name, uint64_t begin, uint64_t end, int result);
void ink_cost_flash_counts(uint64_t out[3]);
#endif
