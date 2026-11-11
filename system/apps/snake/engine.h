/* Rules ported from patorjk/JavaScript-Snake (MIT); see SOURCE.md. */
#ifndef INK_SNAKE_ENGINE_H
#define INK_SNAKE_ENGINE_H
#include <stdbool.h>
#include <stdint.h>
enum { INK_SNAKE_SIDE = 12, INK_SNAKE_CELLS = INK_SNAKE_SIDE * INK_SNAKE_SIDE };
typedef struct {
    uint8_t body[INK_SNAKE_CELLS];
    int length, food, dir, next, score;
    bool paused, over;
} ink_snake_t;
void ink_snake_start(ink_snake_t *s, uint32_t seed);
int ink_snake_turn(ink_snake_t *s, int dir);
void ink_snake_step(ink_snake_t *s, uint32_t seed);
#endif
