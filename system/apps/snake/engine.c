/* Copyright Patrick Gillespie. MIT; C adaptation, see SOURCE.md. */
#include "engine.h"
#include <string.h>
static void food(ink_snake_t *s, uint32_t seed) {
    int empty = INK_SNAKE_CELLS - s->length;
    if (!empty) { s->food = -1; s->over = true; return; }
    int choice = (int)(seed % (unsigned)empty);
    for (int i = 0; i < INK_SNAKE_CELLS; i++) {
        bool used = false;
        for (int j = 0; j < s->length; j++) if (s->body[j] == i) used = true;
        if (!used && choice-- == 0) { s->food = i; return; }
    }
}
void ink_snake_start(ink_snake_t *s, uint32_t seed) {
    *s = (ink_snake_t){.body = {78, 77, 76}, .length = 3, .dir = 1, .next = 1}; food(s, seed);
}
int ink_snake_turn(ink_snake_t *s, int dir) {
    if (!s->length || s->over || s->paused || dir < 0 || dir > 3 || s->next != s->dir || (dir + 2) % 4 == s->dir) return -1;
    s->next = dir; return 0;
}
void ink_snake_step(ink_snake_t *s, uint32_t seed) {
    if (!s->length || s->paused || s->over) return;
    static const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    int x = s->body[0] % INK_SNAKE_SIDE + dx[s->next], y = s->body[0] / INK_SNAKE_SIDE + dy[s->next];
    int head = y * INK_SNAKE_SIDE + x;
    bool eat = head == s->food;
    if (x < 0 || x >= INK_SNAKE_SIDE || y < 0 || y >= INK_SNAKE_SIDE) { s->over = true; return; }
    for (int i = 0; i < s->length - !eat; i++) if (s->body[i] == head) { s->over = true; return; }
    memmove(s->body + 1, s->body, (size_t)(s->length - !eat)); s->body[0] = (uint8_t)head; s->dir = s->next;
    if (eat) { s->length++; s->score++; food(s, seed); }
}
