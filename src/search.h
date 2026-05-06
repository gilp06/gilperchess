#pragma once

#include "types.h"
#include "board.h"

void perform_search(board_t* starting_board, int depth);
move_t search_root(board_t *board, int depth);
int16_t search(board_t *board, int depth);
