#pragma once

#include "types.h"
#include "move_gen.h"
#include "eval.h"
#include "board.h"

typedef enum e_selectphase { TT_MOVE, GEN_MOVES, MOVES, DONE } selectphase_t;

typedef struct s_moveselect {
    move_t moves[256];
    int16_t move_scores[256];
    size_t count;
    move_t tt_move;
    selectphase_t phase;
    bool nonquiet_only;
} moveselect_t;

void init_select(board_t *board, moveselect_t *move_select, move_t tt_move,
                 bool nonquiet_only);
move_t select_move(board_t *board, moveselect_t *move_select);

bool see(board_t *board, move_t move, int16_t threshold);
