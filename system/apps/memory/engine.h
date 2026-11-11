/* Rules ported from taniarascia/memory (MIT); see SOURCE.md. */
#ifndef INK_MEMORY_ENGINE_H
#define INK_MEMORY_ENGINE_H
#include <stdbool.h>
#include <stdint.h>
enum { INK_MEMORY_CARDS = 16 };
typedef struct {
    uint8_t cards[INK_MEMORY_CARDS], open[INK_MEMORY_CARDS];
    int first, second, pairs, moves;
} ink_memory_t;
void ink_memory_start(ink_memory_t *s, uint32_t seed);
int ink_memory_flip(ink_memory_t *s, int card);
void ink_memory_hide(ink_memory_t *s);
#endif
