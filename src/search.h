#pragma once

#include "types.h"
#include "board.h"
#include "hashtable.h"


typedef struct s_global_state {
    ttable_t transposition_table;
    move_t best_move;
    bool stop_search;
} global_state_t;


void perform_search(global_state_t* gs, board_t* starting_board, int depth);
int16_t alphabeta(global_state_t* gs, board_t *board, bool root, int16_t depth, int16_t alpha, int16_t beta, int16_t ply);
// move_t search_root(board_t *board, int depth);
// int16_t search(board_t *board, int depth);
