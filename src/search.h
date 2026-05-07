#pragma once

#include "types.h"
#include "board.h"
#include "hashtable.h"


typedef struct s_global_state {
    ttable_t transposition_table;
    bool stop_search;
} global_state_t;


void perform_search(global_state_t* gs, board_t* starting_board, int depth);
// move_t search_root(board_t *board, int depth);
// int16_t search(board_t *board, int depth);
