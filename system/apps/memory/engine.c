/* Copyright (c) 2018 Tania Rascia. MIT; C adaptation, see SOURCE.md. */
#include "engine.h"
void ink_memory_start(ink_memory_t *s, uint32_t seed) {
    *s = (ink_memory_t){.first = -1, .second = -1};
    for (int i = 0; i < INK_MEMORY_CARDS; i++) s->cards[i] = (uint8_t)(i / 2);
    for (int i = INK_MEMORY_CARDS - 1; i > 0; i--) {
        seed = seed * 1664525u + 1013904223u;
        int j = (int)(seed % (unsigned)(i + 1));
        uint8_t tmp = s->cards[i]; s->cards[i] = s->cards[j]; s->cards[j] = tmp;
    }
}
int ink_memory_flip(ink_memory_t *s, int card) {
    if (card < 0 || card >= INK_MEMORY_CARDS || s->second >= 0 || s->open[card]) return -1;
    s->open[card] = 1;
    if (s->first < 0) s->first = card;
    else {
        s->second = card; s->moves++;
        if (s->cards[s->first] == s->cards[card]) {
            s->open[s->first] = s->open[card] = 2; s->pairs++; s->first = s->second = -1;
        }
    }
    return 0;
}
void ink_memory_hide(ink_memory_t *s) {
    if (s->second < 0) return;
    s->open[s->first] = s->open[s->second] = 0; s->first = s->second = -1;
}
