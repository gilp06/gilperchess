#pragma once

#include "types.h"


static inline void add_move(move_t *moves, size_t* count, bindex_t from, bindex_t to, movetype_t mt) {
    moves[(*count)++] = pack_move(from, to, mt);
}

static inline void add_promotions(move_t *moves, size_t* count, bindex_t from, bindex_t to) {
    moves[(*count)++] = pack_promo(from, to, PROMO_BISHOP);  
    moves[(*count)++] = pack_promo(from, to, PROMO_KNIGHT);  
    moves[(*count)++] = pack_promo(from, to, PROMO_ROOK);  
    moves[(*count)++] = pack_promo(from, to, PROMO_QUEEN);  
}



static inline void insert_mask_moves(move_t *moves, size_t *count, bindex_t from, bb_t mask,
                       bb_t potential_captures, bb_t our_pieces) {
    bb_t possible_moves = (mask & ~our_pieces);
    while (possible_moves != 0) {
        bindex_t end_pos = __builtin_ctzll(possible_moves);
        possible_moves = (possible_moves & (possible_moves - 1));
        add_move(moves, count, from, end_pos, NORMAL);
    }
}


void generate_pawn_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_knight_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_bishop_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_rook_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_queen_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_king_moves(board_t *board, side_t side, move_t *moves, size_t *count);
void generate_pseudolegal_moves(board_t *board, side_t side, move_t* moves, size_t *count);

void insert_mask_moves(move_t *moves, size_t* count, bindex_t from, bb_t mask, bb_t potential_captures, bb_t our_pieces);
bb_t generate_attack_mask(board_t* board, side_t side);
