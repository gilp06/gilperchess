#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "utils.h"

void init_board_from_start(board_t *board);
void init_board_from_fen(board_t *board, const char *str);

void set_piece(board_t *board, piece_t piece, bindex_t pos);

bool perform_move(board_t *board, move_t move, dstate_t *undo);
void undo_move(board_t *board, dstate_t *undo);
void perform_null_move(board_t *board, dstate_t *undo);
void undo_null_move(board_t *board, dstate_t *undo);
bool in_check(board_t *board, side_t side);

uint8_t king_square(const board_t *board, side_t side);

inline side_t stm(const board_t *board) { return board->side_to_move; }

inline side_t otm(const board_t *board) { return board->side_to_move ^ 0b1; }

bool in_check(board_t *board, side_t side);
bool is_draw(board_t *board);

// side being attacked
static inline bool is_attacked(board_t *board, side_t side, bindex_t sq) {
    bb_t blockers = board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK];

    bb_t op_king = board->pieces_occ[PIECETYPE_KING] & board->sides_occ[!side];
    bb_t op_pawns = board->pieces_occ[PIECETYPE_PAWN] & board->sides_occ[!side];
    bb_t op_knights =
        board->pieces_occ[PIECETYPE_KNIGHT] & board->sides_occ[!side];
    bb_t op_bq = (board->pieces_occ[PIECETYPE_QUEEN] |
                  board->pieces_occ[PIECETYPE_BISHOP]) &
                 board->sides_occ[!side];
    bb_t op_rq = (board->pieces_occ[PIECETYPE_QUEEN] |
                  board->pieces_occ[PIECETYPE_ROOK]) &
                 board->sides_occ[!side];

    return (op_pawns & PAWN_ATTACKS[!side][sq]) |
           (op_knights & KNIGHT_MOVES[sq]) |
           (op_bq & fetch_bishop_moves(sq, blockers)) |
           (op_rq & fetch_rook_moves(sq, blockers)) |
           (op_king & KING_MOVES[sq]);
}

static inline bb_t attackers_to(board_t *board, bindex_t to, bb_t occupied) {
    bb_t white_occ = board->sides_occ[SIDE_WHITE];
    bb_t black_occ = board->sides_occ[SIDE_BLACK];

    bb_t pawns = board->pieces_occ[PIECETYPE_PAWN];
    bb_t knights = board->pieces_occ[PIECETYPE_KNIGHT];
    bb_t bishops = board->pieces_occ[PIECETYPE_BISHOP];
    bb_t rooks = board->pieces_occ[PIECETYPE_ROOK];
    bb_t queens = board->pieces_occ[PIECETYPE_QUEEN];
    bb_t kings = board->pieces_occ[PIECETYPE_KING];

    bb_t attackers = 0ULL;

    attackers |= (PAWN_ATTACKS[SIDE_WHITE][to] & pawns & black_occ);
    attackers |= (PAWN_ATTACKS[SIDE_BLACK][to] & pawns & white_occ);

    attackers |= KNIGHT_MOVES[to] & knights;
    attackers |= fetch_bishop_moves(to, occupied) & (bishops | queens);
    attackers |= fetch_rook_moves(to, occupied) & (rooks | queens);
    attackers |= KING_MOVES[to] & kings;
    return attackers;
}

static inline bool is_capture(board_t *board, move_t move) {
    return (board->sides_occ[SIDE_WHITE] | board->sides_occ[SIDE_BLACK]) &
               (1ULL << move_to(move)) ||
           move_type(move) == EN_PASSANT;
}

// for nnue
static inline int calculate_chess768_index(side_t perspective, bindex_t square,
                                           piecetype_t piecetype, side_t side) {
    if (perspective == SIDE_BLACK) {
        side = !side;
        square = square ^ 56;
    }
    return side * 64 * 6 + piecetype * 64 + square;
}

static inline void nnue_add_piece(board_t *board, bindex_t sq, piece_t piece) {
    assert(piece != PIECE_NONE);
    piecetype_t pt = piece_type(piece);
    side_t ps = piece_side(piece);
    int windex = calculate_chess768_index(SIDE_WHITE, sq, pt, ps);
    accum_add_feat(&NNUE, windex, &board->white_accum);
    int bindex = calculate_chess768_index(SIDE_BLACK, sq, pt, ps);
    accum_add_feat(&NNUE, bindex, &board->black_accum);
}

static inline void nnue_remove_piece(board_t *board, bindex_t sq,
                                     piece_t piece) {

    assert(piece != PIECE_NONE);
    piecetype_t pt = piece_type(piece);
    side_t ps = piece_side(piece);
    int windex = calculate_chess768_index(SIDE_WHITE, sq, pt, ps);
    accum_remove_feat(&NNUE, windex, &board->white_accum);
    int bindex = calculate_chess768_index(SIDE_BLACK, sq, pt, ps);
    accum_remove_feat(&NNUE, bindex, &board->black_accum);
}



