#include "engine.h"
#include "vendor/crossmux/GomokuAI.h"
#include <cstring>
static_assert(sizeof(ink_gomoku_t) == 455, "member record must be byte-only");
static bool restore(const ink_gomoku_t *g, GomokuBoard &board) {
    if ((g->size != 9 && g->size != 15) || g->count > g->size * g->size || g->mode > 1 || g->level > 2) return false;
    board.clear(g->size);
    for (int i = 0; i < g->count; i++)
        if (!board.placeStone(g->history[i] / g->size, g->history[i] % g->size)) return false;
    return true;
}
int ink_gomoku_play(ink_gomoku_t *g, int operation, int row, int col) {
    GomokuBoard board;
    if (!restore(g, board)) return -1;
    if (operation == 1 && (row < 0 || col < 0 || row >= g->size || col >= g->size || !board.placeStone(row, col))) return -1;
    if (operation == 2) {
        if (!board.undo()) return -1;
        if (g->mode && board.nextTurn() == GomokuBoard::Stone::White) board.undo();
    }
    g->count = static_cast<uint8_t>(board.moveCount); g->winner = static_cast<uint8_t>(board.winner);
    std::memcpy(g->cells, board.cells, sizeof g->cells); std::memcpy(g->history, board.moveHistory, sizeof g->history);
    return 0;
}
int ink_gomoku_choose(const ink_gomoku_t *g) {
    GomokuBoard board;
    if (!restore(g, board) || board.isOver() || board.nextTurn() != GomokuBoard::Stone::White) return -1;
    return GomokuAI::chooseMove(board, board.nextTurn(), static_cast<GomokuAiLevel>(g->level));
}
