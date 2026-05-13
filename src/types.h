#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "nnue.h"

typedef uint64_t bb_t;
typedef uint16_t move_t;
typedef uint8_t bindex_t;

typedef enum e_side { SIDE_WHITE = 0, SIDE_BLACK, SIDE_NONE } side_t;

typedef enum e_piecetype {
    PIECETYPE_PAWN = 0,
    PIECETYPE_KNIGHT,
    PIECETYPE_BISHOP,
    PIECETYPE_ROOK,
    PIECETYPE_QUEEN,
    PIECETYPE_KING,
    PIECETYPE_NONE
} piecetype_t;

typedef enum e_piece {
    PIECE_NONE = 0,

    PIECE_WHITE_MASK = 0b00000000,
    PIECE_BLACK_MASK = 0b00000001,
    PIECE_COLOR_MASK = 0b00000001,

    PIECE_PAWN_MASK = 0b00000010,
    PIECE_KNIGHT_MASK = 0b00000100,
    PIECE_BISHOP_MASK = 0b00000110,
    PIECE_ROOK_MASK = 0b00001000,
    PIECE_QUEEN_MASK = 0b00001010,
    PIECE_KING_MASK = 0b00001100,
    PIECE_TYPE_MASK = 0b00001110,

    // white pieces
    PIECE_WPAWN = PIECE_WHITE_MASK | PIECE_PAWN_MASK,
    PIECE_WKNIGHT = PIECE_WHITE_MASK | PIECE_KNIGHT_MASK,
    PIECE_WBISHOP = PIECE_WHITE_MASK | PIECE_BISHOP_MASK,
    PIECE_WROOK = PIECE_WHITE_MASK | PIECE_ROOK_MASK,
    PIECE_WQUEEN = PIECE_WHITE_MASK | PIECE_QUEEN_MASK,
    PIECE_WKING = PIECE_WHITE_MASK | PIECE_KING_MASK,

    // black pieces
    PIECE_BPAWN = PIECE_BLACK_MASK | PIECE_PAWN_MASK,
    PIECE_BKNIGHT = PIECE_BLACK_MASK | PIECE_KNIGHT_MASK,
    PIECE_BBISHOP = PIECE_BLACK_MASK | PIECE_BISHOP_MASK,
    PIECE_BROOK = PIECE_BLACK_MASK | PIECE_ROOK_MASK,
    PIECE_BQUEEN = PIECE_BLACK_MASK | PIECE_QUEEN_MASK,
    PIECE_BKING = PIECE_BLACK_MASK | PIECE_KING_MASK

} piece_t;

typedef enum e_promo {
    PROMO_KNIGHT = 0,
    PROMO_BISHOP = 1 << 12,
    PROMO_ROOK = 2 << 12,
    PROMO_QUEEN = 3 << 12,
} promo_t;

typedef enum e_movetype {
    NORMAL = 0,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
} movetype_t;

static inline piecetype_t promo_piecetype(promo_t promo) {
    return ((promo >> 12)) + 1;
}

static inline move_t pack_move(bindex_t from, bindex_t to, movetype_t type) {
    return type | (to << 6) | (from);
}

static inline move_t pack_promo(bindex_t from, bindex_t to,
                                promo_t promo_piece) {
    return PROMOTION | promo_piece | (to << 6) | from;
}

static inline bindex_t move_from(move_t move) { return move & 0x3F; }

static inline bindex_t move_to(move_t move) { return (move >> 6) & 0x3F; }

static inline movetype_t move_type(move_t move) { return (move & (3 << 14)); }

static inline piecetype_t move_promo(move_t move) {
    return promo_piecetype(move & (3 << 12));
}

static inline piecetype_t piece_type(piece_t piece) {
    return (piece >> 1) - 1;
}

static inline side_t piece_side(piece_t piece) { return (piece & 1); }

static inline piece_t make_piece(piecetype_t type, side_t side) {
    return (type + 1) << 1 | side;
}

// piecetype_t piece_type(piece_t piece);
// side_t piece_side(piece_t piece);
// piece_t make_piece(piecetype_t type, side_t side);

typedef struct s_state {
    uint64_t key;
    bindex_t ep_square;
    uint8_t castling_rights;
    uint8_t ep_was_possible;
    int halfmove_clock, fullmove_clock;
} state_t;

typedef struct s_board {
    int move_number; // this is the number of plys currently stored in history
    side_t side_to_move;
    bb_t pieces_occ[6]; // indexed by piecetype_t
    bb_t sides_occ[2];  // indexed by side_t
    piece_t pieces_at[64];
    uint64_t key_hist[8192]; // this is what ethereal uses, I think it should be sufficient
    state_t st;


    // NNUE stuff
    accumulator_t white_accum;
    accumulator_t black_accum;
} board_t;

typedef struct s_dstate {
    piece_t captured;
    move_t move;
    state_t prev_state;
} dstate_t;

typedef enum e_square {    
    A1 = 0,
    B1,
    C1,
    D1,
    E1,
    F1,
    G1,
    H1,
    A2,
    B2,
    C2,
    D2,
    E2,
    F2,
    G2,
    H2,
    A3,
    B3,
    C3,
    D3,
    E3,
    F3,
    G3,
    H3,
    A4,
    B4,
    C4,
    D4,
    E4,
    F4,
    G4,
    H4,
    A5,
    B5,
    C5,
    D5,
    E5,
    F5,
    G5,
    H5,
    A6,
    B6,
    C6,
    D6,
    E6,
    F6,
    G6,
    H6,
    A7,
    B7,
    C7,
    D7,
    E7,
    F7,
    G7,
    H7,
    A8,
    B8,
    C8,
    D8,
    E8,
    F8,
    G8,
    H8
} square_t;

typedef enum e_brank {
    RANK_1 = 0,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8
} brank_t;
typedef enum e_bfile {
    FILE_A = 0,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
} bfile_t;

typedef enum e_castling_side { KINGSIDE = 0, QUEENSIDE } castling_side_t;
