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
    accum_add_feat(NNUE, windex, &board->white_accum);
    int bindex = calculate_chess768_index(SIDE_BLACK, sq, pt, ps);
    accum_add_feat(NNUE, bindex, &board->black_accum);
}

static inline void nnue_remove_piece(board_t *board, bindex_t sq,
                                     piece_t piece) {

    assert(piece != PIECE_NONE);
    piecetype_t pt = piece_type(piece);
    side_t ps = piece_side(piece);
    int windex = calculate_chess768_index(SIDE_WHITE, sq, pt, ps);
    accum_remove_feat(NNUE, windex, &board->white_accum);
    int bindex = calculate_chess768_index(SIDE_BLACK, sq, pt, ps);
    accum_remove_feat(NNUE, bindex, &board->black_accum);
}

// update side's pinners and !side's blockers
static inline void update_pinners_and_blockers(board_t* board, side_t side) {

    bb_t us = board->sides_occ[side];
    bb_t them = board->sides_occ[!side];

    bindex_t ksq = __builtin_ctzll(board->pieces_occ[PIECETYPE_KING] & them);

    bb_t pinners = 0ULL;
    bb_t blockers = 0ULL;
    

    bb_t rq = (board->pieces_occ[PIECETYPE_ROOK] | board->pieces_occ[PIECETYPE_QUEEN]) & us;
    bb_t bq = (board->pieces_occ[PIECETYPE_BISHOP] | board->pieces_occ[PIECETYPE_QUEEN]) & us;

    bb_t rq_candidates = ROOK_SLIDER_MASK[ksq] & rq;

    while(rq_candidates) {
        bindex_t sq = __builtin_ctzll(rq_candidates);
        rq_candidates = (rq_candidates & (rq_candidates-1));

        bb_t between = BETWEEN_MASK[ksq][sq];

        bb_t our_blockers = between & us;
        bb_t enemy_blockers = between & them;

        // check if there is exactly one piece between our piece and their king and that it is their piece
        if (!our_blockers && enemy_blockers && !(enemy_blockers & (enemy_blockers-1))) {
            pinners |= (1ULL << sq);
            blockers |= enemy_blockers;
        }
    }


    bb_t bq_candidates = BISHOP_SLIDER_MASK[ksq] & bq;
    
    while(bq_candidates) {
        bindex_t sq = __builtin_ctzll(bq_candidates);
        bq_candidates = (bq_candidates & (bq_candidates-1));

        bb_t between = BETWEEN_MASK[ksq][sq];

        bb_t our_blockers = between & us;
        bb_t enemy_blockers = between & them;

        // check if there is exactly one piece between our piece and their king and that it is their piece
        if (!our_blockers && enemy_blockers && !(enemy_blockers & (enemy_blockers-1))) {
            pinners |= (1ULL << sq);
            blockers |= enemy_blockers;
        }
    }

    board->st.blockers[!side] = blockers;
    board->st.pinners[side] = pinners;
}


// check if board has 
static inline bool has_pieces(board_t* board, side_t stm) {
    return (board->pieces_occ[PIECETYPE_ROOK] | board->pieces_occ[PIECETYPE_BISHOP] | board->pieces_occ[PIECETYPE_KNIGHT] | board->pieces_occ[PIECETYPE_QUEEN]) & board->sides_occ[stm];
}
