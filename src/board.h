#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "types.h"
#include "utils.h"

void init_board_from_start(board_t* board);
void init_board_from_fen(board_t* board, const char* str);

void set_piece(board_t* board, piece_t piece, bindex_t pos);

bool perform_move(board_t* board, move_t move, dstate_t* undo);
void undo_move(board_t* board, dstate_t* undo);
void perform_null_move(board_t* board, dstate_t* undo);
void undo_null_move(board_t* board, dstate_t* undo);
bool in_check(board_t* board, side_t side);

uint8_t king_square(const board_t* board, side_t side);

inline side_t stm(const board_t* board)
{
    return board->side_to_move;
}

inline side_t otm(const board_t* board)
{
    return board->side_to_move ^ 0b1;
}


bool in_check(board_t *board, side_t side);
bool is_draw(board_t* board);

// side being attacked
static inline bool is_attacked(board_t* board, side_t side, bindex_t sq)
{
    bb_t blockers = board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK];

    bb_t op_king = board->pieces_occ[PIECETYPE_KING] & board->sides_occ[!side];
    bb_t op_pawns = board->pieces_occ[PIECETYPE_PAWN] & board->sides_occ[!side];
    bb_t op_knights = board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[!side];
    bb_t op_bq = (board->pieces_occ[PIECETYPE_QUEEN] | board->pieces_occ[PIECETYPE_BISHOP]) & board->sides_occ[!side];
    bb_t op_rq = (board->pieces_occ[PIECETYPE_QUEEN] | board->pieces_occ[PIECETYPE_ROOK]) & board->sides_occ[!side];

    return (op_pawns & PAWN_ATTACKS[!side][sq])
        | (op_knights & KNIGHT_MOVES[sq])
        | (op_bq & fetch_bishop_moves(sq, blockers))
        | (op_rq & fetch_rook_moves(sq, blockers))
        | (op_king & KING_MOVES[sq]);
}


