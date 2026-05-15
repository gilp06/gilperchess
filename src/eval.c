#include "eval.h"
#include "board.h"
#include "nnue.h"

const int16_t piece_values[16] = {
    [PIECE_NONE] = 0,

    // white pieces
    [PIECE_WPAWN] = VAL_PAWN,
    [PIECE_WKNIGHT] = VAL_KNIGHT,
    [PIECE_WBISHOP] = VAL_BISHOP,
    [PIECE_WROOK] = VAL_ROOK,
    [PIECE_WQUEEN] = VAL_QUEEN,
    [PIECE_WKING] = VAL_KING,

    // black pieces
    [PIECE_BPAWN] = VAL_PAWN,
    [PIECE_BKNIGHT] = VAL_KNIGHT,
    [PIECE_BBISHOP] = VAL_BISHOP,
    [PIECE_BROOK] = VAL_ROOK,
    [PIECE_BQUEEN] = VAL_QUEEN,
    [PIECE_BKING] = VAL_KING,
};

int16_t evaluate(board_t *board) {    
    // TODO: probably make this a thing we track when we make moves
    size_t piece_count = __builtin_popcountll(board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK]);
    if (board->side_to_move == SIDE_WHITE)
        return evaluate_nnue(&NNUE, &board->white_accum, &board->black_accum, piece_count);
    return evaluate_nnue(&NNUE, &board->black_accum, &board->white_accum, piece_count);
}
