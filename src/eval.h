#pragma once

#include "types.h"

#define VAL_PAWN   100
#define VAL_KNIGHT 320
#define VAL_BISHOP 330
#define VAL_ROOK   500
#define VAL_QUEEN  900
#define VAL_KING   0 

extern const int16_t piece_values[16];
int16_t evaluate(board_t* board);
