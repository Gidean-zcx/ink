#ifndef INK_GOMOKU_ENGINE_H
#define INK_GOMOKU_ENGINE_H
#include <stdint.h>
/* Byte-only member DB payload; turn is count parity. No pointers or host padding. */
typedef struct { uint8_t size, mode, level, count, winner, history[225], cells[225]; } ink_gomoku_t;
#ifdef __cplusplus
extern "C" {
#endif
int ink_gomoku_play(ink_gomoku_t *g, int operation, int row, int col);
int ink_gomoku_choose(const ink_gomoku_t *g);
#ifdef __cplusplus
}
#endif
#endif
